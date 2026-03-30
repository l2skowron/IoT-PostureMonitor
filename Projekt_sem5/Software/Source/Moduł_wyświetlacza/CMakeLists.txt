idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_wifi adafruit_ssd1351 driver esp_adc nvs_flash esp_event
)
