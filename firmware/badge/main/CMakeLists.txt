set(WEB_ROOT "${CMAKE_CURRENT_LIST_DIR}/../web")
idf_component_register(SRCS "badge.c" INCLUDE_DIRS "."
    REQUIRES bsp esp_wifi esp_event esp_netif esp_http_server nvs_flash spiffs esp_timer esp_lcd esp_driver_gpio
    EMBED_FILES "${WEB_ROOT}/badge.html" "${WEB_ROOT}/badge.css" "${WEB_ROOT}/badge.js" "${WEB_ROOT}/badge-render.js" "sample.rgb")
