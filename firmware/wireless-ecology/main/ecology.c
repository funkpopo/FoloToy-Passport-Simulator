#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "badge_font.h"
#include "ecology_model.h"

#define RGB(r,g,b) (((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
#define INK RGB(19,36,44)
#define PAPER RGB(237,231,207)
#define SLOTS 6
typedef struct { esp_err_t error; uint16_t count, total; eco_tree trees[ECO_CAP]; } scan_result;
typedef struct { eco_scene scene; uint8_t name, demo; } stamp;
typedef struct { uint32_t version, count, next; stamp items[SLOTS]; } album_data;
typedef struct { int x,y; uint16_t color; char text[32]; } label;
static QueueHandle_t keys, requests, results;
static SemaphoreHandle_t dma_done;
static uint16_t pixels[120*160];
static uint8_t *stripe;
static label labels[18];
static unsigned label_count;
static eco_scene live, sample;
static album_data album = {.version=1};
static nvs_handle_t nvs;
static bool storage_ready, demo, in_album, naming, scanning;
static unsigned habitat, album_index, name_index;
static stamp pending;
static char notice[32] = "STARTING RADIO";
static int64_t next_scan, notice_until;
static const char *names[] = {"MOSS GARDEN", "AMBER GROVE", "TIDAL FOREST", "QUIET VALLEY", "POCKET JUNGLE", "MOON MEADOW", "HIDDEN SPRING", "HOME WOODS"};
static const uint16_t terrain[] = {RGB(65,133,103),RGB(179,125,64),RGB(93,123,161)};
static int64_t now_ms(void) { return esp_timer_get_time()/1000; }
static void say(const char *s) { snprintf(notice,sizeof(notice),"%s",s); notice_until=now_ms()+3500; }
static void rect(int x,int y,int w,int h,uint16_t color) {
    for(int j=y;j<y+h;j++) for(int i=x;i<x+w;i++)
        if(i>=0 && i<120 && j>=0 && j<160) pixels[j*120+i]=color;
}
static void text(int x,int y,uint16_t color,const char *s) {
    if(label_count>=18) return;
    label *l=&labels[label_count++]; l->x=x; l->y=y; l->color=color;
    snprintf(l->text,sizeof(l->text),"%s",s);
}
static bool transfer_done(esp_lcd_panel_io_handle_t io,esp_lcd_panel_io_event_data_t *e,void *ctx) {
    (void)io; (void)e; (void)ctx; BaseType_t wake=pdFALSE;
    xSemaphoreGiveFromISR(dma_done,&wake); return wake==pdTRUE;
}
static void flush(void) {
    for(int y=0;y<320;y+=10) {
        for(int j=0;j<10;j++) for(int x=0;x<240;x++) {
            uint16_t c=pixels[((y+j)/2)*120+x/2];
            int p=(j*240+x)*2; stripe[p]=c>>8; stripe[p+1]=c;
        }
        for(unsigned k=0;k<label_count;k++) {
            label *l=&labels[k];
            for(int j=0;j<14;j++) {
                int py=l->y+j; if(py<y || py>=y+10) continue;
                for(unsigned a=0;l->text[a];a++) {
                    unsigned char ch=l->text[a]; if(ch<32 || ch>126) ch='?';
                    for(int b=0;b<8;b++) {
                        int x=l->x+(int)a*8+b;
                        if(x>=0 && x<240 && (badge_font[ch-32][j] & (1u<<b))) {
                            int p=((py-y)*240+x)*2; stripe[p]=l->color>>8; stripe[p+1]=l->color;
                        }
                    }
                }
            }
        }
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(bsp_display_panel(),0,y,240,y+10,stripe));
        xSemaphoreTake(dma_done,portMAX_DELAY);
    }
}
static void draw(unsigned frame) {
    label_count=0;
    const stamp *saved=in_album && album.count ? &album.items[album_index] : NULL;
    const eco_scene *s=naming ? &pending.scene : saved ? &saved->scene : demo ? &sample : &live;
    bool synthetic=naming ? pending.demo : saved ? saved->demo : demo;
    rect(0,0,120,160,PAPER); rect(0,0,120,27,INK);
    text(12,8,PAPER,"WIRELESS ECOLOGY");
    char line[32];
    snprintf(line,sizeof(line),"%s  %s",in_album?"ALBUM":"FIELD",synthetic?"DEMO / SYNTHETIC":"LIVE / 2.4 GHZ");
    text(12,30,RGB(152,206,187),line);
    rect(0,28,120,88,RGB(161,199,192));
    rect(90,34,12,12,RGB(247,218,151)); rect(87,37,18,6,RGB(247,218,151));
    for(int x=0;x<120;x+=4) {
        int h=4+((x*17+13)%19); rect(x,85-h,4,h+24,RGB(119,165,150));
    }
    rect(0,101,120,15,RGB(42,77,69));
    /* Draw back-to-front by stable depth. Identity fixes the tree's position. */
    unsigned drawn=0;
    for(int depth=0;depth<3;depth++) for(unsigned i=0;i<ECO_CAP;i++) {
        const eco_tree *t=&s->trees[i]; if(!t->id || (int)((t->id>>8)%3)!=depth) continue;
        drawn++;
        int x=8+(t->id%104), base=87+depth*10, h=eco_height(t->rssi);
        unsigned band=t->channel<=4?0:t->channel<=9?1:2;
        uint16_t leaf=t->misses ? RGB(135,146,126) : terrain[band];
        rect(x-7,base,15,3,leaf); rect(x-1,base-h/2,3,h/2,RGB(90,64,45));
        if(t->id&1) {
            for(int row=0;row<h-7;row+=3) {
                int w=3+row/2; rect(x-w/2,base-h+row,w,3,leaf);
            }
        } else {
            rect(x-6,base-h+4,13,h/2,leaf); rect(x-3,base-h,7,h/2+7,leaf);
            rect(x-4,base-h+5,3,4,RGB(189,208,135));
        }
        int bx=x+((frame+(t->id%7))%9)-4, by=base-h-7;
        rect(bx,by,2,2,RGB(255,242,186));
        rect(bx+((frame/2)%2?2:-2),by-1,2,1,PAPER);
    }
    if(!drawn) { text(24,135,INK,in_album?"NO STAMPS YET":"WAITING FOR SEEDS"); }
    rect(4,119,3,3,terrain[0]); text(18,236,INK,"1-4");
    rect(43,119,3,3,terrain[1]); text(96,236,INK,"5-9");
    rect(82,119,3,3,terrain[2]); text(174,236,INK,"10-14");
    if(naming) {
        rect(6,51,108,47,INK); text(20,110,PAPER,"NAME YOUR STAMP");
        text(20,135,RGB(247,218,151),names[name_index]);
        text(20,160,PAPER,"UP/DOWN PICK  OK SAVE");
        text(12,258,INK,"HOLD OK: CANCEL");
    } else if(in_album) {
        snprintf(line,sizeof(line),"STAMP %u/%lu %s",album.count?album_index+1:0,(unsigned long)album.count,saved?names[saved->name]:"");
        text(8,258,INK,line);
    } else {
        snprintf(line,sizeof(line),"%u AP  %u TREES  +%u -%u",s->total,drawn,s->added,s->removed);
        text(8,258,INK,line);
    }
    if(!naming) {
        text(8,278,INK,in_album?"UP EXIT  DOWN NEXT  OK NAME":"UP ALBUM DOWN SCAN OK STAMP");
        text(8,298,INK,now_ms()<notice_until?notice:in_album?"SNAPSHOT / FLASH SAVED":scanning&&!demo?"SCANNING...":"HOLD DOWN: LIVE/DEMO");
    }
    flush();
}
static void make_demo(void) {
    static const int8_t levels[3][9]={{-35,-50,-73,-85,-64,-92,0,0,0},{-70,-42,-51,-58,0,0,-36,-67,0},{-88,0,-41,0,-58,0,-72,-47,-62}};
    eco_tree a[9]; unsigned count=0;
    for(unsigned i=0;i<9;i++) if(levels[habitat][i]) {
        uint8_t mac[6]={2,0,0,0,0,(uint8_t)(i+1)};
        a[count++]=(eco_tree){eco_id(mac),levels[habitat][i],(uint8_t)(1+(i*5)%13),0};
    }
    eco_update(&sample,a,count,count);
    snprintf(notice,sizeof(notice),"DEMO HABITAT %u / 3",habitat+1); notice_until=now_ms()+3500;
}
static void radio_worker(void *arg) {
    (void)arg;
    esp_err_t err=esp_netif_init();
    if(err==ESP_OK) err=esp_event_loop_create_default();
    if(err==ESP_OK && !esp_netif_create_default_wifi_sta()) err=ESP_ERR_NO_MEM;
    wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();
    if(err==ESP_OK) err=esp_wifi_init(&cfg);
    if(err==ESP_OK) err=esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if(err==ESP_OK) err=esp_wifi_set_mode(WIFI_MODE_STA);
    if(err==ESP_OK) err=esp_wifi_start();
    int request;
    while(true) {
        xQueueReceive(requests,&request,portMAX_DELAY);
        scan_result result={.error=err};
        if(err==ESP_OK) {
            /* Passive discovery only: no connection, credentials or Internet. */
            wifi_scan_config_t config={.show_hidden=true,.scan_type=WIFI_SCAN_TYPE_PASSIVE,.scan_time.passive=180};
            result.error=esp_wifi_scan_start(&config,true);
            wifi_ap_record_t records[ECO_CAP]; result.count=ECO_CAP;
            if(result.error==ESP_OK) result.error=esp_wifi_scan_get_ap_num(&result.total);
            if(result.error==ESP_OK) result.error=esp_wifi_scan_get_ap_records(&result.count,records);
            if(result.error==ESP_OK) for(unsigned i=0;i<result.count;i++)
                result.trees[i]=(eco_tree){eco_id(records[i].bssid),records[i].rssi,records[i].primary,0};
            else { result.count=0; esp_wifi_clear_ap_list(); }
        }
        xQueueOverwrite(results,&result);
    }
}
static void request_scan(void) {
    if(scanning) return;
    int request=1;
    if(xQueueSend(requests,&request,0)==pdTRUE) { scanning=true; next_scan=now_ms()+30000; }
}
static void button(bsp_btn_t key,bsp_btn_ev_t event,void *arg) {
    (void)arg;
    if(event!=BSP_BTN_CLICK && event!=BSP_BTN_LONG) return;
    int value=(int)key+(event==BSP_BTN_LONG?10:0); xQueueSend(keys,&value,0);
}
static void save_stamp(void) {
    if(!storage_ready) { say("STORAGE UNAVAILABLE"); naming=false; return; }
    album_data updated=album;
    pending.name=name_index;
    if(in_album && album.count) updated.items[album_index]=pending;
    else {
        updated.items[updated.next]=pending; album_index=updated.next;
        updated.next=(updated.next+1)%SLOTS;
        if(updated.count<SLOTS) updated.count++;
    }
    esp_err_t err=nvs_set_blob(nvs,"album",&updated,sizeof(updated));
    if(err==ESP_OK) err=nvs_commit(nvs);
    naming=false;
    if(err==ESP_OK) { album=updated; say("STAMP SAVED!"); }
    else say("SAVE FAILED - RETRY");
}
static void handle_key(int key) {
    if(naming) {
        if(key==BSP_BTN_UP) name_index=(name_index+7)%8;
        else if(key==BSP_BTN_DOWN) name_index=(name_index+1)%8;
        else if(key==BSP_BTN_OK) save_stamp();
        else if(key==10+BSP_BTN_OK) naming=false;
        return;
    }
    if(key==BSP_BTN_UP) { in_album=!in_album; if(in_album && album.count) album_index=(album.next+SLOTS-1)%SLOTS; }
    else if(key==BSP_BTN_DOWN) {
        if(in_album) { if(album.count) album_index=(album_index+1)%album.count; }
        else if(demo) { habitat=(habitat+1)%3; make_demo(); }
        else if(now_ms()+25000<next_scan) say("SCAN COOLDOWN: 5 SECONDS");
        else request_scan();
    } else if(key==10+BSP_BTN_DOWN && !in_album) {
        demo=!demo; if(demo) make_demo(); else { say("LIVE RADIO / 2.4 GHZ"); if(now_ms()>=next_scan) request_scan(); }
    } else if(key==BSP_BTN_OK) {
        if(in_album) { if(!album.count) { say("RETURN TO FIELD TO STAMP"); return; } pending=album.items[album_index]; }
        else { pending=(stamp){.scene=demo?sample:live,.demo=demo}; }
        name_index=pending.name; naming=true;
    }
}
void app_main(void) {
    esp_err_t err=nvs_flash_init();
    /* Do not erase existing user or factory NVS when initialization fails. */
    if(err==ESP_OK && nvs_open("wireless_eco",NVS_READWRITE,&nvs)==ESP_OK) {
        storage_ready=true; size_t size=sizeof(album); album_data loaded;
        if(nvs_get_blob(nvs,"album",&loaded,&size)==ESP_OK && size==sizeof(loaded) && loaded.version==1 && loaded.count<=SLOTS && loaded.next<SLOTS) {
            bool valid=true;
            for(unsigned i=0;i<loaded.count;i++) if(loaded.items[i].name>=8 || loaded.items[i].demo>1) valid=false;
            if(valid) album=loaded;
        }
    }
    keys=xQueueCreate(12,sizeof(int)); requests=xQueueCreate(1,sizeof(int)); results=xQueueCreate(1,sizeof(scan_result));
    dma_done=xSemaphoreCreateBinary(); stripe=heap_caps_malloc(4800,MALLOC_CAP_DMA);
    assert(keys && requests && results && dma_done && stripe);
    ESP_ERROR_CHECK(bsp_display_init());
    esp_lcd_panel_io_callbacks_t callbacks={.on_color_trans_done=transfer_done};
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(bsp_display_io(),&callbacks,NULL));
    ESP_ERROR_CHECK(bsp_button_init(button,NULL)); bsp_display_backlight(65);
    assert(xTaskCreate(radio_worker,"eco_radio",6144,NULL,4,NULL)==pdPASS);
    request_scan(); unsigned frame=0;
    while(true) {
        scan_result result;
        if(xQueueReceive(results,&result,0)==pdTRUE) {
            scanning=false;
            if(result.error==ESP_OK) { eco_update(&live,result.trees,result.count,result.total); if(!demo) say(result.total?"LANDSCAPE UPDATED":"NO APS / HOLD DOWN: DEMO"); }
            else { ESP_LOGW("ecology","Scan failed: %s",esp_err_to_name(result.error)); if(!demo) say("SCAN FAILED / DOWN RETRY"); }
        }
        int key; while(xQueueReceive(keys,&key,0)==pdTRUE) handle_key(key);
        if(!demo && !in_album && !naming && now_ms()>=next_scan) request_scan();
        draw(frame++); vTaskDelay(pdMS_TO_TICKS(150));
    }
}
