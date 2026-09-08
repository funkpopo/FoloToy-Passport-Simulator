idf_component_register(SRCS "src/bsp_display.c" "src/bsp_button.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_lcd button esp_adc)
