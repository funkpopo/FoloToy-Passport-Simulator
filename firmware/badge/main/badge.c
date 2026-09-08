#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_spiffs.h"
#include "esp_random.h"
#include "esp_mac.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "badge_protocol.h"
#include "badge_json.h"
#include "badge_font.h"

static const char *TAG = "badge";
static QueueHandle_t keys;
static SemaphoreHandle_t storage_lock, transfer_done;
static nvs_handle_t settings;
static char password[17], ssid[32];
static uint32_t generation;
static unsigned current_page;
static bool config_screen = true;
static uint8_t *stripe;
static const char *slots[] = {"/badge/slot0.bin", "/badge/slot1.bin"};
extern const uint8_t sample_start[] asm("_binary_sample_rgb_start");
extern const uint8_t sample_end[] asm("_binary_sample_rgb_end");
extern const uint8_t html_start[] asm("_binary_badge_html_start");
extern const uint8_t html_end[] asm("_binary_badge_html_end");
extern const uint8_t css_start[] asm("_binary_badge_css_start");
extern const uint8_t css_end[] asm("_binary_badge_css_end");
extern const uint8_t js_start[] asm("_binary_badge_js_start");
extern const uint8_t js_end[] asm("_binary_badge_js_end");
extern const uint8_t render_start[] asm("_binary_badge_render_js_start");
extern const uint8_t render_end[] asm("_binary_badge_render_js_end");

static bool transfer_complete(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *event, void *ctx) {
    (void)io; (void)event; (void)ctx;
    BaseType_t wake = pdFALSE;
    xSemaphoreGiveFromISR(transfer_done, &wake);
    return wake == pdTRUE;
}
static void draw_stripe(unsigned y) {
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(bsp_display_panel(), 0, y, 240, y + 10, stripe));
    /* The DMA buffer must remain unchanged until the SPI transfer finishes. */
    xSemaphoreTake(transfer_done, portMAX_DELAY);
}
static void text_stripe(unsigned stripe_y, unsigned x, unsigned y, const char *text, unsigned scale) {
    for (unsigned i = 0; text[i]; i++) {
        unsigned char ch = (unsigned char)text[i]; if (ch < 32 || ch > 126) ch = '?';
        for (unsigned row = 0; row < 14 * scale; row++) {
            unsigned py = y + row; if (py < stripe_y || py >= stripe_y + 10) continue;
            for (unsigned col = 0; col < 8 * scale; col++) {
                unsigned px = x + i * 8 * scale + col;
                if (px < 240 && (badge_font[ch - 32][row / scale] & (1u << (col / scale)))) {
                    unsigned offset = ((py - stripe_y) * 240 + px) * 2;
                    stripe[offset] = 0xef; stripe[offset + 1] = 0x7c;
                }
            }
        }
    }
}
static void show_connection(void) {
    for (unsigned y = 0; y < 320; y += 10) {
        for (unsigned i = 0; i < 4800; i += 2) { stripe[i] = 0x11; stripe[i + 1] = 0xe6; }
        text_stripe(y, 16, 20, "MY PASSPORT", 2);
        text_stripe(y, 16, 68, "1. JOIN WI-FI", 1);
        text_stripe(y, 16, 91, ssid, 1);
        text_stripe(y, 16, 124, "WI-FI / EDIT PASSWORD", 1);
        text_stripe(y, 16, 147, password, 1);
        text_stripe(y, 16, 184, "2. OPEN IN PHONE BROWSER", 1);
        text_stripe(y, 16, 207, "http://192.168.4.1", 1);
        text_stripe(y, 16, 248, "3. EDIT AND SAVE", 1);
        text_stripe(y, 16, 289, "HOLD OK: SHOW / HIDE", 1);
        draw_stripe(y);
    }
}
static void display_page(void) {
    xSemaphoreTake(storage_lock, portMAX_DELAY);
    FILE *file = generation ? fopen(slots[generation & 1], "rb") : NULL;
    if (config_screen) { if (file) fclose(file); xSemaphoreGive(storage_lock); show_connection(); return; }
    if (!file) {
        /* Offline examples use the real SPI drawing path, never a browser overlay. */
        assert(sample_end - sample_start == BADGE_PIXEL_BYTES);
        for (unsigned y = 0; y < 320; y += 10) {
            memcpy(stripe, sample_start + current_page * BADGE_FRAME_BYTES + y * 480, 4800);
            draw_stripe(y);
        }
        xSemaphoreGive(storage_lock);
        return;
    }
    fseek(file, (long)(current_page * BADGE_FRAME_BYTES), SEEK_SET);
    for (unsigned y = 0; y < 320; y += 10) {
        if (fread(stripe, 1, 4800, file) != 4800) { config_screen = true; break; }
        draw_stripe(y);
    }
    fclose(file); xSemaphoreGive(storage_lock);
    if (config_screen) show_connection();
}
static void on_button(bsp_btn_t btn, bsp_btn_ev_t event, void *user) {
    (void)user;
    int key = event == BSP_BTN_LONG && btn == BSP_BTN_OK ? 3 : event == BSP_BTN_CLICK ? (int)btn : -1;
    if (key >= 0) xQueueSend(keys, &key, 0);
}
static void display_task(void *arg) {
    (void)arg;
    display_page();
    int key;
    while (true) {
        if (xQueueReceive(keys, &key, portMAX_DELAY)) {
            if (key == 3) config_screen = !config_screen;
            else if (key == 4) config_screen = false;
            else { config_screen = false; current_page = badge_next_page(current_page, (unsigned)key); }
            display_page();
        }
    }
}
static esp_err_t json_error(httpd_req_t *req, const char *status, const char *message) {
    httpd_resp_set_status(req, status); httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    char body[192]; snprintf(body, sizeof(body), "{\"error\":\"%s\"}", message);
    return httpd_resp_sendstr(req, body);
}
static bool authorized(httpd_req_t *req) {
    char key[24] = {0};
    if (httpd_req_get_hdr_value_str(req, "X-Badge-Key", key, sizeof(key)) != ESP_OK) return false;
    unsigned diff = strlen(key) ^ strlen(password);
    for (unsigned i = 0; i < strlen(password); i++) diff |= (unsigned char)key[i] ^ (unsigned char)password[i];
    return diff == 0;
}
static esp_err_t assets(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(req, "Content-Security-Policy", "default-src 'self'; img-src 'self' data:; object-src 'none'; base-uri 'none'; frame-ancestors 'none'");
    const uint8_t *start = html_start, *end = html_end;
    const char *type = "text/html; charset=utf-8";
    if (!strcmp(req->uri, "/badge.css")) { start = css_start; end = css_end; type = "text/css"; }
    else if (!strcmp(req->uri, "/badge.js")) { start = js_start; end = js_end; type = "application/javascript"; }
    else if (!strcmp(req->uri, "/badge-render.js")) { start = render_start; end = render_end; type = "application/javascript"; }
    else if (strcmp(req->uri, "/") && strcmp(req->uri, "/badge.html")) return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
    httpd_resp_set_type(req, type);
    return httpd_resp_send(req, (const char *)start, end - start);
}
static esp_err_t get_state(httpd_req_t *req) {
    if (!authorized(req)) return json_error(req, "401 Unauthorized", "Enter the password on the badge screen");
    httpd_resp_set_type(req, "application/json; charset=utf-8"); httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    xSemaphoreTake(storage_lock, portMAX_DELAY);
    char header[96]; snprintf(header, sizeof(header), "{\"mode\":\"firmware\",\"revision\":\"%lu\",\"profile\":", (unsigned long)generation);
    esp_err_t result = httpd_resp_sendstr_chunk(req, header);
    FILE *file = generation ? fopen(slots[generation & 1], "rb") : NULL;
    if (file) {
        fseek(file, BADGE_PIXEL_BYTES, SEEK_SET);
        char buffer[1024]; size_t n;
        while (result == ESP_OK && (n = fread(buffer, 1, sizeof(buffer), file))) result = httpd_resp_send_chunk(req, buffer, n);
        fclose(file);
    } else if (result == ESP_OK) result = httpd_resp_sendstr_chunk(req, "{}");
    xSemaphoreGive(storage_lock);
    if (result == ESP_OK) result = httpd_resp_sendstr_chunk(req, "}");
    if (result == ESP_OK) result = httpd_resp_send_chunk(req, NULL, 0);
    return result;
}
static esp_err_t put_bundle(httpd_req_t *req) {
    if (!authorized(req)) return json_error(req, "401 Unauthorized", "Incorrect configuration password");
    if (!badge_valid_size(req->content_len)) return json_error(req, "413 Content Too Large", "Invalid bundle size");
    char revision[24], expected[24], content_type[40];
    snprintf(expected, sizeof(expected), "%lu", (unsigned long)generation);
    if (httpd_req_get_hdr_value_str(req, "If-Match", revision, sizeof(revision)) != ESP_OK || strcmp(expected, revision)) return json_error(req, "409 Conflict", "Badge changed. Reload before saving");
    if (httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type)) != ESP_OK || strcmp(content_type, "application/octet-stream")) return json_error(req, "415 Unsupported Media Type", "Expected binary bundle");
    /* Write the inactive slot. NVS generation is committed only after close succeeds.
     * A reset during upload leaves the previous slot selected. */
    uint32_t next = generation + 1; if (!next) return json_error(req, "500 Internal Server Error", "Revision exhausted");
    FILE *file = fopen(slots[next & 1], "wb");
    if (!file) return json_error(req, "500 Internal Server Error", "Cannot open badge storage");
    char buffer[2048]; unsigned received = 0; bool valid = true; badge_json_t parser = {0};
    while (received < req->content_len) {
        unsigned remaining = req->content_len - received;
        int n = httpd_req_recv(req, buffer, remaining < sizeof(buffer) ? remaining : sizeof(buffer));
        if (n <= 0) { valid = false; break; }
        for (int i = 0; i < n; i++) if (received + i >= BADGE_PIXEL_BYTES && !badge_json_feed(&parser, (unsigned char)buffer[i])) valid = false;
        if (!valid || fwrite(buffer, 1, n, file) != (size_t)n) { valid = false; break; }
        received += n;
    }
    if (fflush(file) != 0) valid = false;
    if (fclose(file) != 0) valid = false;
    if (!valid || !badge_json_done(&parser)) return json_error(req, "400 Bad Request", "Incomplete or invalid badge data. Previous badge kept");
    xSemaphoreTake(storage_lock, portMAX_DELAY);
    esp_err_t result = nvs_set_u32(settings, "generation", next);
    if (result == ESP_OK) result = nvs_commit(settings);
    if (result == ESP_OK) generation = next;
    xSemaphoreGive(storage_lock);
    if (result != ESP_OK) return json_error(req, "500 Internal Server Error", "Cannot commit badge data");
    int refresh = 4; xQueueSend(keys, &refresh, 0);
    snprintf(buffer, sizeof(buffer), "{\"revision\":\"%lu\"}", (unsigned long)generation);
    httpd_resp_set_type(req, "application/json"); return httpd_resp_sendstr(req, buffer);
}
void app_main(void) {
    /* Never erase the global NVS partition automatically: other apps may use it. */
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(nvs_open("id_badge", NVS_READWRITE, &settings));
    size_t size = sizeof(password);
    if (nvs_get_str(settings, "password", password, &size) != ESP_OK) {
        snprintf(password, sizeof(password), "%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random());
        ESP_ERROR_CHECK(nvs_set_str(settings, "password", password)); ESP_ERROR_CHECK(nvs_commit(settings));
    }
    esp_vfs_spiffs_conf_t fs = { .base_path = "/badge", .partition_label = "badge", .max_files = 4, .format_if_mount_failed = false };
    esp_err_t mounted = esp_vfs_spiffs_register(&fs);
    if (mounted != ESP_OK) {
        uint8_t provisioned = 0; nvs_get_u8(settings, "provisioned", &provisioned);
        if (provisioned) { ESP_LOGE(TAG, "Storage mount failed; refusing to format existing badge data"); return; }
        ESP_ERROR_CHECK(esp_spiffs_format("badge")); ESP_ERROR_CHECK(esp_vfs_spiffs_register(&fs));
    }
    ESP_ERROR_CHECK(nvs_set_u8(settings, "provisioned", 1)); ESP_ERROR_CHECK(nvs_commit(settings));
    nvs_get_u32(settings, "generation", &generation);
    struct stat info;
    if (generation && (stat(slots[generation & 1], &info) || !badge_valid_size(info.st_size))) generation = 0;
    config_screen = false;
    storage_lock = xSemaphoreCreateMutex(); transfer_done = xSemaphoreCreateBinary(); keys = xQueueCreate(8, sizeof(int));
    stripe = heap_caps_malloc(4800, MALLOC_CAP_DMA);
    assert(storage_lock && transfer_done && keys && stripe);
    ESP_ERROR_CHECK(esp_netif_init()); ESP_ERROR_CHECK(esp_event_loop_create_default());
    assert(esp_netif_create_default_wifi_ap());
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT(); ESP_ERROR_CHECK(esp_wifi_init(&init));
    uint8_t mac[6]; ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP));
    snprintf(ssid, sizeof(ssid), "FoloToy-Badge-%02X%02X", mac[4], mac[5]);
    wifi_config_t wifi = {0};
    memcpy(wifi.ap.ssid, ssid, strlen(ssid)); wifi.ap.ssid_len = strlen(ssid);
    memcpy(wifi.ap.password, password, strlen(password)); wifi.ap.channel = 1;
    wifi.ap.max_connection = 2; wifi.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP)); ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi)); ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(bsp_display_init());
    esp_lcd_panel_io_callbacks_t callbacks = {.on_color_trans_done = transfer_complete};
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(bsp_display_io(), &callbacks, NULL));
    bsp_display_backlight(80);
    assert(xTaskCreate(display_task, "badge_display", 4096, NULL, 4, NULL) == pdPASS);
    ESP_ERROR_CHECK(bsp_button_init(on_button, NULL));
    httpd_config_t config = HTTPD_DEFAULT_CONFIG(); config.max_uri_handlers = 8; config.stack_size = 6144;
    config.recv_wait_timeout = 15; config.send_wait_timeout = 15; config.lru_purge_enable = true;
    httpd_handle_t server; ESP_ERROR_CHECK(httpd_start(&server, &config));
    const char *paths[] = {"/", "/badge.html", "/badge.css", "/badge.js", "/badge-render.js"};
    for (unsigned i = 0; i < 5; i++) { httpd_uri_t uri = {.uri = paths[i], .method = HTTP_GET, .handler = assets}; ESP_ERROR_CHECK(httpd_register_uri_handler(server, &uri)); }
    httpd_uri_t state = {.uri = "/api/badge/state", .method = HTTP_GET, .handler = get_state};
    httpd_uri_t bundle = {.uri = "/api/badge/bundle", .method = HTTP_PUT, .handler = put_bundle};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &state)); ESP_ERROR_CHECK(httpd_register_uri_handler(server, &bundle));
    ESP_LOGI(TAG, "Identity badge ready; hold OK for phone configuration");
}
