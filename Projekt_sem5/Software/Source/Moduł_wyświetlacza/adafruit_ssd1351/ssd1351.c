#include "ssd1351.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


static const uint8_t font5x7[][5] = {
    [' ' - 32] = {0x00, 0x00, 0x00, 0x00, 0x00},
    ['A' - 32] = {0x7C, 0x12, 0x11, 0x12, 0x7C},
    ['B' - 32] = {0x7F, 0x49, 0x49, 0x49, 0x36},
    ['C' - 32] = {0x3E, 0x41, 0x41, 0x41, 0x22},
    ['D' - 32] = {0x7F, 0x41, 0x41, 0x22, 0x1C},
    ['E' - 32] = {0x7F, 0x49, 0x49, 0x49, 0x41},
    ['F' - 32] = {0x7F, 0x09, 0x09, 0x09, 0x01},
    ['G' - 32] = {0x3E, 0x41, 0x49, 0x49, 0x7A},
    ['H' - 32] = {0x7F, 0x08, 0x08, 0x08, 0x7F},
    ['I' - 32] = {0x00, 0x41, 0x7F, 0x41, 0x00},
    ['J' - 32] = {0x20, 0x40, 0x41, 0x3F, 0x01},
    ['K' - 32] = {0x7F, 0x08, 0x14, 0x22, 0x41},
    ['L' - 32] = {0x7F, 0x40, 0x40, 0x40, 0x40},
    ['M' - 32] = {0x7F, 0x02, 0x0C, 0x02, 0x7F},
    ['N' - 32] = {0x7F, 0x04, 0x08, 0x10, 0x7F},
    ['O' - 32] = {0x3E, 0x41, 0x41, 0x41, 0x3E},
    ['P' - 32] = {0x7F, 0x09, 0x09, 0x09, 0x06},
    ['Q' - 32] = {0x3E, 0x41, 0x51, 0x21, 0x5E},
    ['R' - 32] = {0x7F, 0x09, 0x19, 0x29, 0x46},
    ['S' - 32] = {0x46, 0x49, 0x49, 0x49, 0x31},
    ['T' - 32] = {0x01, 0x01, 0x7F, 0x01, 0x01},
    ['U' - 32] = {0x3F, 0x40, 0x40, 0x40, 0x3F},
    ['V' - 32] = {0x1F, 0x20, 0x40, 0x20, 0x1F},
    ['W' - 32] = {0x3F, 0x40, 0x38, 0x40, 0x3F},
    ['X' - 32] = {0x63, 0x14, 0x08, 0x14, 0x63},
    ['Y' - 32] = {0x07, 0x08, 0x70, 0x08, 0x07},
    ['Z' - 32] = {0x61, 0x51, 0x49, 0x45, 0x43}
};
// Funkcja pomocnicza do wysyłania komend
void ssd1351_write_cmd(spi_device_handle_t spi, int dc_pin, uint8_t cmd) {
    gpio_set_level(dc_pin, 0); // DC na 0 = Komenda
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    spi_device_polling_transmit(spi, &t);
}

// Funkcja pomocnicza do wysyłania danych
void ssd1351_write_data(spi_device_handle_t spi, int dc_pin, uint8_t data) {
    gpio_set_level(dc_pin, 1); // DC na 1 = Dane
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &data,
    };
    spi_device_polling_transmit(spi, &t);
}
void ssd1351_write_text(spi_device_handle_t spi, int dc_pin, uint8_t x, uint8_t y, const char *text, uint16_t color) {
    while (*text) {
        char c = *text;
        // Konwersja małych liter na duże (bo tylko takie mamy w tablicy)
        if (c >= 'a' && c <= 'z') c -= 32;
        
        if (c >= ' ' && c <= 'Z') {
            uint8_t index = c - 32;
            for (int col = 0; col < 5; col++) {
                uint8_t line = font5x7[index][col];
                for (int row = 0; row < 7; row++) {
                    if (line & (1 << row)) {
                        // Rysujemy piksel 1x1
                        ssd1351_draw_rect(spi, dc_pin, x + col, y + row, 1, 1, color);
                    }
                }
            }
        }
        x += 6; // Przesunięcie o szerokość znaku (5px) + odstęp (1px)
        text++;
        if (x > 122) break; // Koniec linii
    }
}
void ssd1351_init(spi_device_handle_t spi, int dc_pin, int rst_pin) {
    // Konfiguracja pinów DC i RST
    gpio_reset_pin(dc_pin);
    gpio_set_direction(dc_pin, GPIO_MODE_OUTPUT);
    gpio_reset_pin(rst_pin);
    gpio_set_direction(rst_pin, GPIO_MODE_OUTPUT);

    // Hard Reset
    gpio_set_level(rst_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(rst_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(rst_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Sekwencja startowa Adafruit
    ssd1351_write_cmd(spi, dc_pin, 0xFD); // Command Lock
    ssd1351_write_data(spi, dc_pin, 0x12);
    ssd1351_write_cmd(spi, dc_pin, 0xFD); // Command Lock
    ssd1351_write_data(spi, dc_pin, 0xB1);

    ssd1351_write_cmd(spi, dc_pin, 0xAE); // Display OFF

    ssd1351_write_cmd(spi, dc_pin, 0xB3); // Clock & Frequency
    ssd1351_write_data(spi, dc_pin, 0xF1);

    ssd1351_write_cmd(spi, dc_pin, 0xCA); // Mux Ratio
    ssd1351_write_data(spi, dc_pin, 127);

    ssd1351_write_cmd(spi, dc_pin, 0xA0); // Set Re-map
    ssd1351_write_data(spi, dc_pin, 0x66); // 16-bit color

    ssd1351_write_cmd(spi, dc_pin, 0x15); // Set Column
    ssd1351_write_data(spi, dc_pin, 0x00);
    ssd1351_write_data(spi, dc_pin, 0x7F);

    ssd1351_write_cmd(spi, dc_pin, 0x75); // Set Row
    ssd1351_write_data(spi, dc_pin, 0x00);
    ssd1351_write_data(spi, dc_pin, 0x7F);

    ssd1351_write_cmd(spi, dc_pin, 0xA1); // Start Line
    ssd1351_write_data(spi, dc_pin, 132);

    ssd1351_write_cmd(spi, dc_pin, 0xA2); // Display Offset
    ssd1351_write_data(spi, dc_pin, 0);

    ssd1351_write_cmd(spi, dc_pin, 0xB5); // Set GPIO
    ssd1351_write_data(spi, dc_pin, 0x00);

    ssd1351_write_cmd(spi, dc_pin, 0xAB); // Function Selection
    ssd1351_write_data(spi, dc_pin, 0x01);

    ssd1351_write_cmd(spi, dc_pin, 0xB1); // Pre-charge
    ssd1351_write_data(spi, dc_pin, 0x32);

    ssd1351_write_cmd(spi, dc_pin, 0xBE); // VCOMH
    ssd1351_write_data(spi, dc_pin, 0x05);

    ssd1351_write_cmd(spi, dc_pin, 0xA6); // Normal Display

    ssd1351_write_cmd(spi, dc_pin, 0xC1); // Contrast ABC
    ssd1351_write_data(spi, dc_pin, 0xC8);
    ssd1351_write_data(spi, dc_pin, 0x80);
    ssd1351_write_data(spi, dc_pin, 0xC8);

    ssd1351_write_cmd(spi, dc_pin, 0xC7); // Master Contrast
    ssd1351_write_data(spi, dc_pin, 0x0F);

    ssd1351_write_cmd(spi, dc_pin, 0xAF); // Display ON
}

void ssd1351_fill_screen(spi_device_handle_t spi, int dc_pin, uint16_t color) {
    // Ustawienie okna na cały ekran
    ssd1351_write_cmd(spi, dc_pin, 0x5C); // RAM Write
    
    // OLED SSD1351 przyjmuje kolory jako dwa bajty (MSB, LSB)
    uint8_t color_high = (color >> 8) & 0xFF;
    uint8_t color_low = color & 0xFF;

    for (int i = 0; i < 128 * 128; i++) {
        ssd1351_write_data(spi, dc_pin, color_high);
        ssd1351_write_data(spi, dc_pin, color_low);
    }
}
void ssd1351_draw_rect(spi_device_handle_t spi, int dc_pin, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t color) {
    ssd1351_write_cmd(spi, dc_pin, 0x15); // Set Column Address
    ssd1351_write_data(spi, dc_pin, x);
    ssd1351_write_data(spi, dc_pin, x + w - 1);

    ssd1351_write_cmd(spi, dc_pin, 0x75); // Set Row Address
    ssd1351_write_data(spi, dc_pin, y);
    ssd1351_write_data(spi, dc_pin, y + h - 1);

    ssd1351_write_cmd(spi, dc_pin, 0x5C); // Write RAM
    
    uint8_t color_h = color >> 8;
    uint8_t color_l = color & 0xFF;

    for (int i = 0; i < w * h; i++) {
        ssd1351_write_data(spi, dc_pin, color_h);
        ssd1351_write_data(spi, dc_pin, color_l);
    }
}