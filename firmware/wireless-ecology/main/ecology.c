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
#include "bsp_display.h"
#include "bsp_button.h"
#include "ecology_labels.h"
#include "ecology_model.h"
#include "ecology_name.h"
#include "badge_font.h"

#define RGB(r,g,b) (((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
#define INK RGB(19,36,44)
#define PAPER RGB(237,231,207)
#define REFRESH_MS 5000
typedef struct { esp_err_t error; uint16_t count, total; eco_tree trees[ECO_CAP]; } scan_result;
typedef struct { int x,y; uint16_t color; unsigned id; } label;
static QueueHandle_t keys, requests, results;
static SemaphoreHandle_t dma_done;
static uint16_t pixels[120*160];
static uint8_t *stripe;
static label labels[18];
static unsigned label_count;
static eco_scene live, sample;
static bool demo, scanning, have_scan, scan_error;
static unsigned habitat, selected_channel=1, updates;
static int64_t next_scan, next_demo;
extern const uint8_t name_font_start[] asm("_binary_name_font_bin_start");
static char displayed_name[33];
static int64_t name_since;
static bool show_name;
static const uint16_t terrain[] = {RGB(65,133,103),RGB(179,125,64),RGB(93,123,161)};
static int64_t now_ms(void) { return esp_timer_get_time()/1000; }
static void rect(int x,int y,int w,int h,uint16_t color) {
    for(int j=y;j<y+h;j++) for(int i=x;i<x+w;i++)
        if(i>=0 && i<120 && j>=0 && j<160) pixels[j*120+i]=color;
}
static void caption(int x,int y,uint16_t color,unsigned id) {
    if(label_count>=18) return;
    labels[label_count++]=(label){x,y,color,id};
}
static bool transfer_done(esp_lcd_panel_io_handle_t io,esp_lcd_panel_io_event_data_t *e,void *ctx) {
    (void)io; (void)e; (void)ctx; BaseType_t wake=pdFALSE;
    xSemaphoreGiveFromISR(dma_done,&wake); return wake==pdTRUE;
}
static void name_stripe(int stripe_y) {
    if(!show_name) return;
    uint32_t chars[32]; unsigned count=0; int width=0;
    const unsigned char *p=(const unsigned char *)displayed_name;
    while(*p && count<32) {
        uint32_t cp=eco_next_char(&p);
        if(eco_glyph_index(cp)<0 && (cp<32 || cp>126)) cp='?';
        chars[count++]=cp; width+=cp<128?8:16;
    }
    int offset=0;
    if(width>208) {
        int travel=width-208;
        int phase=(int)((now_ms()-name_since)/60)%(travel+40);
        offset=phase<20?0:phase<20+travel?phase-20:travel;
    }
    int x=16-offset;
    for(unsigned i=0;i<count;i++) {
        uint32_t cp=chars[i]; int glyph=eco_glyph_index(cp), w=cp<128?8:16;
        for(int row=0;row<20;row++) {
            int y=263+row; if(y<stripe_y || y>=stripe_y+10) continue;
            for(int col=0;col<w;col++) {
                bool lit=glyph>=0?(name_font_start[glyph*40+row*2+col/8] & (1u<<(7-col%8))):row>=3 && row<17 && (badge_font[cp-32][row-3] & (1u<<col));
                if(lit && x+col>=16 && x+col<224) {
                    int at=((y-stripe_y)*240+x+col)*2; stripe[at]=INK>>8; stripe[at+1]=INK&255;
                }
            }
        }
        x+=w;
    }
}
static void flush(void) {
    for(int y=0;y<320;y+=10) {
        for(int j=0;j<10;j++) for(int x=0;x<240;x++) {
            uint16_t c=pixels[((y+j)/2)*120+x/2];
            int p=(j*240+x)*2; stripe[p]=c>>8; stripe[p+1]=c;
        }
        for(unsigned k=0;k<label_count;k++) {
            label *l=&labels[k];
            const eco_label_bitmap *bitmap=&eco_labels[l->id];
            for(int j=0;j<22;j++) {
                int py=l->y+j; if(py<y || py>=y+10) continue;
                for(int col=0;col<bitmap->width;col++) {
                    int bit=j*bitmap->width+col, x=l->x+col;
                    if(x>=0 && x<240 && (bitmap->bits[bit/8] & (1u<<(7-bit%8)))) {
                        int p=((py-y)*240+x)*2; stripe[p]=l->color>>8; stripe[p+1]=l->color;
                    }
                }
            }
        }
        name_stripe(y);
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(bsp_display_panel(),0,y,240,y+10,stripe));
        xSemaphoreTake(dma_done,portMAX_DELAY);
    }
}
static void draw_tree(int x,int base,int height,unsigned band,bool stale) {
    static const uint16_t shadows[]={RGB(37,88,64),RGB(120,75,36),RGB(49,76,113)};
    static const uint16_t highlights[]={RGB(137,183,105),RGB(220,174,86),RGB(151,186,196)};
    uint16_t leaf=stale?RGB(133,140,129):terrain[band];
    uint16_t shade=stale?RGB(99,109,98):shadows[band];
    uint16_t light=stale?RGB(168,173,151):highlights[band];
    uint16_t bark=RGB(104,72,43);
    int top=base-height;
    int trunk=height/5; if(trunk<2) trunk=2;
    int crown=height-trunk;
    int tiers=height>=36?4:height>=17?3:2;
    /* Stepped, separate boughs give a pine silhouette even within a 7-pixel
     * channel. The tip still lies exactly on the RSSI height, roots on baseline. */
    rect(x,top+1,1,height-1,bark);
    for(int tier=0;tier<tiers;tier++) {
        int start=top+tier*crown/tiers;
        int end=top+(tier+1)*crown/tiers;
        int length=end-start;
        int radius=tier==0 && tiers>2?2:3;
        for(int row=0;row<length;row++) {
            int width=length>1?row*radius/(length-1):1;
            rect(x-width,start+row,width*2+1,1,leaf);
            if(width) {
                rect(x+width,start+row,1,1,shade);
                if(row%3==1) rect(x-width,start+row,1,1,light);
            }
        }
        /* Dark undersides separate the overlapping branch tiers. */
        if(length>2) rect(x-radius+1,end-1,radius*2-1,1,shade);
    }
    rect(x,base-trunk,1,trunk,bark);
    if(trunk>=4) rect(x+1,base-trunk+1,1,trunk-2,RGB(157,113,63));
    rect(x-1,base-2,3,1,bark);
    rect(x-2,base-1,5,1,bark);
}
static void draw(unsigned frame) {
    label_count=0;
    const eco_scene *s=demo?&sample:&live;
    rect(0,0,120,160,PAPER); rect(0,0,120,28,INK);
    caption(16,3,PAPER,UI_TITLE);
    caption(16,29,RGB(152,206,187),demo?UI_DEMO:UI_LIVE);
    rect(0,28,120,84,RGB(204,220,205));
    // A quiet landscape, with the same signal-height and channel-order mapping.
    rect(98,33,9,9,RGB(245,219,154));
    rect(96,35,13,5,RGB(245,219,154));
    rect(0,108,120,4,RGB(112,148,100));
    if(!eco_channel_summary(s,selected_channel).count) {
        for(unsigned ch=1;ch<=14;ch++) if(eco_channel_summary(s,ch).count) { selected_channel=ch; break; }
    }
    for(unsigned ch=1;ch<=14;ch++) {
        int x=10+(int)(ch-1)*8;
        eco_channel c=eco_channel_summary(s,ch);
        if(!c.count) continue;
        unsigned band=ch<=4?0:ch<=9?1:2;
        // Expand tree height into the 16 logical rows reclaimed from the hint.
        int h=eco_height(c.rssi)*76/60, top=108-h;
        draw_tree(x,108,h,band,c.stale);
        if(ch==selected_channel) { rect(x-2,113,5,1,INK); rect(x-1,114,3,1,INK); rect(x,115,1,1,INK); }
        if(!c.stale) rect(x+((frame/3+ch)%2?2:-2),top-3,1,1,RGB(112,76,27));
    }
    eco_channel c=eco_channel_summary(s,selected_channel);
    unsigned strength=!c.count?(!demo && scan_error?UI_RETRY:!have_scan && !demo?UI_WAITING:UI_EMPTY):c.stale?UI_FADING:c.rssi>=-55?UI_STRONG:c.rssi>=-75?UI_MEDIUM:UI_WEAK;
    caption(16,236,INK,strength);
    const char *name=c.peak?c.peak->ssid:"";
    if(strncmp(displayed_name,name,33)) { memcpy(displayed_name,name,strlen(name)+1); name_since=now_ms(); }
    show_name=*name!='\0';
    if(c.peak && !show_name) caption(16,261,INK,UI_HIDDEN);
    caption(16,291,INK,UI_HELP);
    flush();
}
static void make_demo(void) {
    static const int8_t levels[3][9]={{-35,-50,-73,-85,-64,-92,0,0,0},{-70,-42,-51,-58,0,0,-36,-67,0},{-88,0,-41,0,-58,0,-72,-47,-62}};
    eco_tree a[9]; unsigned count=0;
    for(unsigned i=0;i<9;i++) if(levels[habitat][i]) {
        uint8_t mac[6]={2,0,0,0,0,(uint8_t)(i+1)};
        static const char *names[]={"森林小屋","Cafe-WiFi","Library","Garden-Guest","Studio","Pocket-Net","Sunny-Room","Reading-Corner","Terrace"};
        a[count]=(eco_tree){.id=eco_id(mac),.rssi=levels[habitat][i],.channel=(uint8_t)(1+(i*5)%13)};
        snprintf(a[count].ssid,sizeof(a[count].ssid),"%s",names[i]); count++;
    }
    eco_update(&sample,a,count,count);
    updates++; next_demo=now_ms()+REFRESH_MS;
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
            if(result.error==ESP_OK) for(unsigned i=0;i<result.count;i++) {
                result.trees[i]=(eco_tree){.id=eco_id(records[i].bssid),.rssi=records[i].rssi,.channel=records[i].primary};
                memcpy(result.trees[i].ssid,records[i].ssid,32); result.trees[i].ssid[32]='\0';
            }
            else { result.count=0; esp_wifi_clear_ap_list(); }
        }
        xQueueOverwrite(results,&result);
    }
}
static void request_scan(void) {
    if(scanning) return;
    int request=1;
    if(xQueueSend(requests,&request,0)==pdTRUE) { scanning=true; next_scan=now_ms()+REFRESH_MS; }
}
static void button(bsp_btn_t key,bsp_btn_ev_t event,void *arg) {
    (void)arg;
    if(event!=BSP_BTN_CLICK && event!=BSP_BTN_LONG) return;
    int value=(int)key+(event==BSP_BTN_LONG?10:0); xQueueSend(keys,&value,0);
}
static void handle_key(int key) {
    if(key==BSP_BTN_UP || key==BSP_BTN_DOWN) {
        const eco_scene *s=demo?&sample:&live;
        for(unsigned i=0;i<14;i++) {
            selected_channel=key==BSP_BTN_UP?(selected_channel==1?14:selected_channel-1):selected_channel%14+1;
            if(eco_channel_summary(s,selected_channel).count) break;
        }
    } else if(key==10+BSP_BTN_DOWN) {
        demo=!demo;
        if(demo) make_demo();
        else if(now_ms()>=next_scan) request_scan();
    } else if(key==BSP_BTN_OK) {
        if(demo) { habitat=(habitat+1)%3; make_demo(); }
        else if(now_ms()>=next_scan) request_scan();
    }
}
void app_main(void) {
    /* Wi-Fi requires NVS initialization; observation never writes album data. */
    esp_err_t err=nvs_flash_init();
    if(err!=ESP_OK) ESP_LOGW("ecology","NVS init: %s",esp_err_to_name(err));
    keys=xQueueCreate(12,sizeof(int)); requests=xQueueCreate(1,sizeof(int)); results=xQueueCreate(1,sizeof(scan_result));
    dma_done=xSemaphoreCreateBinary(); stripe=heap_caps_malloc(4800,MALLOC_CAP_DMA);
    assert(keys && requests && results && dma_done && stripe);
    ESP_ERROR_CHECK(bsp_display_init());
    esp_lcd_panel_io_callbacks_t callbacks={.on_color_trans_done=transfer_done};
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(bsp_display_io(),&callbacks,NULL));
    ESP_ERROR_CHECK(bsp_button_init(button,NULL)); bsp_display_backlight(65);
    assert(xTaskCreate(radio_worker,"eco_radio",8192,NULL,4,NULL)==pdPASS);
    request_scan(); unsigned frame=0;
    while(true) {
        scan_result result;
        if(xQueueReceive(results,&result,0)==pdTRUE) {
            scanning=false; updates++; scan_error=result.error!=ESP_OK;
            ESP_LOGI("ecology","Scan complete #%u: %s, %u AP",updates,esp_err_to_name(result.error),result.total);
            if(result.error==ESP_OK) { eco_update(&live,result.trees,result.count,result.total); have_scan=true; }
            else ESP_LOGW("ecology","Scan failed: %s",esp_err_to_name(result.error));
        }
        int key; while(xQueueReceive(keys,&key,0)==pdTRUE) handle_key(key);
        if(!demo && now_ms()>=next_scan) request_scan();
        if(demo && now_ms()>=next_demo) { habitat=(habitat+1)%3; make_demo(); }
        draw(frame++); vTaskDelay(pdMS_TO_TICKS(150));
    }
}
