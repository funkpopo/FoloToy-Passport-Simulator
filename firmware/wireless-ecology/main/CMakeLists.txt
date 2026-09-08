idf_component_register(SRCS "ecology.c" INCLUDE_DIRS "."
    REQUIRES bsp esp_wifi esp_event esp_netif nvs_flash esp_timer esp_lcd
    EMBED_FILES "name_font.bin")
