#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

static const char *TAG = "NADAJNIK_C3";

#define HX711_DOUT      4
#define HX711_SCK       5

typedef struct __attribute__((packed)) {
    uint16_t fsr[3];    
    int32_t raw_a;      
    int32_t raw_b;      
} seat_data_t;

seat_data_t data_pkt = {0};
uint8_t broadcast_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};


void sort_values(int32_t a[], int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (a[j] > a[j + 1]) {
                int32_t temp = a[j];
                a[j] = a[j + 1];
                a[j + 1] = temp;
            }
        }
    }
}

int32_t hx711_single_read(uint8_t pulses) {
    uint32_t timeout = 0;
    while (gpio_get_level(HX711_DOUT) && timeout++ < 100) {
        esp_rom_delay_us(1000);
    }
    if (timeout >= 100) return -999;

    int32_t data = 0;
    portMUX_TYPE myMux = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&myMux);

    for (int i = 0; i < 24; i++) {
        gpio_set_level(HX711_SCK, 1);
        esp_rom_delay_us(1);
        data <<= 1;
        gpio_set_level(HX711_SCK, 0);
        esp_rom_delay_us(1);
        if (gpio_get_level(HX711_DOUT)) data++;
    }

    for (int i = 0; i < (pulses - 24); i++) {
        gpio_set_level(HX711_SCK, 1);
        esp_rom_delay_us(1);
        gpio_set_level(HX711_SCK, 0);
        esp_rom_delay_us(1);
    }
    portEXIT_CRITICAL(&myMux);

    if (data & 0x800000) data |= 0xFF000000;
    return data;
}


int32_t hx711_read_filtered(uint8_t next_pulses) {
    int32_t samples[3];
    int count = 0;

    for (int i = 0; i < 3; i++) {
        int32_t val = hx711_single_read(next_pulses);

        if (val != -999) {
            samples[count++] = val;
        }
        vTaskDelay(pdMS_TO_TICKS(15)); 
    }

    if (count == 0) return -999;
    if (count == 1) return samples[0];
    
    sort_values(samples, count);
    return samples[count / 2]; 
}

void app_main(void) {
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    esp_now_init();
    esp_now_peer_info_t peer = {.channel = 1, .encrypt = false};
    memcpy(peer.peer_addr, broadcast_mac, 6);
    esp_now_add_peer(&peer);

    adc1_config_width(ADC_WIDTH_BIT_12);

\
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_12);
    adc1_config_channel_atten(ADC1_CHANNEL_1, ADC_ATTEN_DB_12);
    adc1_config_channel_atten(ADC1_CHANNEL_3, ADC_ATTEN_DB_12);
    gpio_set_direction(HX711_SCK, GPIO_MODE_OUTPUT);
    gpio_set_direction(HX711_DOUT, GPIO_MODE_INPUT);
    gpio_set_level(HX711_SCK, 0);

    hx711_single_read(25);

    while (1) {
        data_pkt.fsr[0] = adc1_get_raw(ADC1_CHANNEL_0);
        data_pkt.fsr[1] = adc1_get_raw(ADC1_CHANNEL_1);
        data_pkt.fsr[2] = adc1_get_raw(ADC1_CHANNEL_3);

        int32_t a_val = hx711_read_filtered(26);
        if (a_val != -999) data_pkt.raw_a = a_val;

        vTaskDelay(pdMS_TO_TICKS(50));

        int32_t b_val = hx711_read_filtered(25);
        if (b_val != -999) data_pkt.raw_b = b_val;


        esp_now_send(broadcast_mac, (uint8_t *)&data_pkt, sizeof(seat_data_t));
        
        ESP_LOGI(TAG, "FILTR: A:%ld B:%ld FSR:%d,%d,%d", 
                 data_pkt.raw_a, data_pkt.raw_b, 
                 data_pkt.fsr[0], data_pkt.fsr[1], data_pkt.fsr[2]);

        vTaskDelay(pdMS_TO_TICKS(400));
    }
}