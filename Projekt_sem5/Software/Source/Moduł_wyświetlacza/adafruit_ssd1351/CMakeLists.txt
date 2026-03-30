idf_component_register(
    SRCS "ssd1351.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_driver_spi
)