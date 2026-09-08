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
#include "badge_font.h"
#include "ecology_model.h"

#define RGB(r,g,b) (((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
#define INK RGB(19,36,44)
#define PAPER RGB(237,231,207)
#define REFRESH_MS 5000
typedef struct { esp_err_t error; uint16_t count, total; eco_tree trees[ECO_CAP]; } scan_result;
typedef struct { int x,y; uint16_t color; char text[32]; } label;
static QueueHandle_t keys, requests, results;
static SemaphoreHandle_t dma_done;
static uint16_t pixels[120*160];
static uint8_t *stripe;
static label labels[18];
static unsigned label_count;
static eco_scene live, sample;
static bool demo, scanning;
static unsigned habitat, selected_channel=1, updates;
static char notice[32] = "STARTING RADIO";
static int64_t next_scan, next_demo, notice_until;
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
static void number(int x,int y,unsigned value,uint16_t color) {
    static const uint16_t digits[]={0x7b6f,0x2492,0x73e7,0x73cf,0x5bc9,0x79cf,0x79ef,0x7249,0x7bef,0x7bcf};
    char str[4]; snprintf(str,sizeof(str),"%u",value);
    for(unsigned a=0;str[a];a++) for(int row=0;row<5;row++) for(int col=0;col<3;col++)
        if(digits[str[a]-'0'] & (1u<<(14-row*3-col))) rect(x+a*4+col,y+row,1,1,color);
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
    rect(0,0,120,160,PAPER); rect(0,0,120,27,INK);
    text(12,8,PAPER,"WIRELESS ECOLOGY");
    text(12,30,RGB(152,206,187),demo?"DEMO / SYNTHETIC":"LIVE / 2.4 GHZ");
    char line[96];
    snprintf(line,sizeof(line),"%u AP   AUTO 5S   #%u",s->total,updates);
    text(8,57,INK,line);
    /* Equal baseline and width: channel determines X, RSSI determines height.
     * One tree per channel is the strongest smoothed AP; counts show multiplicity. */
    rect(20,40,98,69,RGB(204,220,205));
    int sx=eco_channel_x(selected_channel);
    rect(sx-3,40,7,69,RGB(245,224,177));
    const int ticks[]={-30,-60,-90};
    for(unsigned j=0;j<3;j++) {
        int y=108-eco_height(ticks[j]);
        rect(20,y,98,1,RGB(173,190,174));
        snprintf(line,sizeof(line),"%d",ticks[j]); text(0,y*2-6,INK,line);
    }
    text(0,76,INK,"dBm");
    for(unsigned ch=1;ch<=14;ch++) {
        int x=eco_channel_x(ch);
        eco_channel c=eco_channel_summary(s,ch);
        unsigned band=ch<=4?0:ch<=9?1:2;
        rect(x-3,109,7,3,terrain[band]);
        number(x-(ch>=10?3:1),115,ch,INK);
        number(x-(c.count>=10?3:1),123,c.count,INK);
        if(!c.count) continue;
        int h=eco_height(c.rssi), top=108-h;
        draw_tree(x,108,h,band,c.stale);
        if(!c.stale) rect(x+((frame/3+ch)%2?2:-2),top-3,1,1,RGB(112,76,27));
    }
    text(0,226,INK,"CH"); text(0,244,INK,"AP");
    eco_channel c=eco_channel_summary(s,selected_channel);
    if(c.count) snprintf(line,sizeof(line),"CH %02u  %u AP  PEAK %d dBm",selected_channel,c.count,c.rssi);
    else snprintf(line,sizeof(line),"CH %02u  0 AP  NO SIGNAL",selected_channel);
    text(8,261,INK,line);
    text(8,280,INK,"UP/DN CH  HOLD DN LIVE/DEMO");
    if(now_ms()<notice_until) snprintf(line,sizeof(line),"%s",notice);
    else if(demo) snprintf(line,sizeof(line),"DEMO %u/3  NEXT %lldS",habitat+1,(long long)((next_demo-now_ms()+999)/1000));
    else if(scanning) snprintf(line,sizeof(line),"SCANNING / PREVIOUS VIEW");
    else snprintf(line,sizeof(line),"NEXT %lldS  OK: SCAN",(long long)((next_scan-now_ms()+999)/1000));
    text(8,299,INK,line);
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
    updates++; next_demo=now_ms()+REFRESH_MS;
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
    if(xQueueSend(requests,&request,0)==pdTRUE) { scanning=true; next_scan=now_ms()+REFRESH_MS; }
}
static void button(bsp_btn_t key,bsp_btn_ev_t event,void *arg) {
    (void)arg;
    if(event!=BSP_BTN_CLICK && event!=BSP_BTN_LONG) return;
    int value=(int)key+(event==BSP_BTN_LONG?10:0); xQueueSend(keys,&value,0);
}
static void handle_key(int key) {
    if(key==BSP_BTN_UP) selected_channel=selected_channel==1?14:selected_channel-1;
    else if(key==BSP_BTN_DOWN) selected_channel=selected_channel%14+1;
    else if(key==10+BSP_BTN_DOWN) {
        demo=!demo;
        if(demo) make_demo();
        else { say("LIVE / AUTO REFRESH"); if(now_ms()>=next_scan) request_scan(); }
    } else if(key==BSP_BTN_OK) {
        if(demo) { habitat=(habitat+1)%3; make_demo(); }
        else if(now_ms()>=next_scan) request_scan();
        else say("AUTO REFRESH IS RUNNING");
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
    assert(xTaskCreate(radio_worker,"eco_radio",6144,NULL,4,NULL)==pdPASS);
    request_scan(); unsigned frame=0;
    while(true) {
        scan_result result;
        if(xQueueReceive(results,&result,0)==pdTRUE) {
            scanning=false; updates++;
            ESP_LOGI("ecology","Scan complete #%u: %s, %u AP",updates,esp_err_to_name(result.error),result.total);
            if(result.error==ESP_OK) { eco_update(&live,result.trees,result.count,result.total); if(!demo) say(result.total?"LANDSCAPE UPDATED":"NO APS / HOLD DOWN: DEMO"); }
            else { ESP_LOGW("ecology","Scan failed: %s",esp_err_to_name(result.error)); if(!demo) say("SCAN FAILED / AUTO RETRY"); }
        }
        int key; while(xQueueReceive(keys,&key,0)==pdTRUE) handle_key(key);
        if(!demo && now_ms()>=next_scan) request_scan();
        if(demo && now_ms()>=next_demo) { habitat=(habitat+1)%3; make_demo(); }
        draw(frame++); vTaskDelay(pdMS_TO_TICKS(150));
    }
}
