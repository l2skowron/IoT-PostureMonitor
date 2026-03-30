#ifndef SSD1351_H
#define SSD1351_H

#include "driver/spi_master.h"
#include "driver/gpio.h"

// Definicje kolorów (565 RGB)
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define WHITE   0xFFFF

void ssd1351_write_text(spi_device_handle_t spi, int dc_pin, uint8_t x, uint8_t y, const char *text, uint16_t color);
void ssd1351_init(spi_device_handle_t spi, int dc_pin, int rst_pin);
void ssd1351_fill_screen(spi_device_handle_t spi, int dc_pin, uint16_t color);
void ssd1351_draw_rect(spi_device_handle_t spi, int dc_pin, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t color);
#endif