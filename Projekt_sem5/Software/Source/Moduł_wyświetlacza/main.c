#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "ssd1351.h"

// Pinout
#define PIN_DC          9
#define PIN_RST         14
#define PIN_CS          10
#define PIN_MOSI        11
#define PIN_CLK         12
#define BUTTON_PIN      21
#define LED_RED_PIN     39
#define LED_GREEN_PIN   40 

#define DATA_TIMEOUT_MS 3000 
#define WEIGHT_TOLERANCE 3000  
#define FSR_TOLERANCE    900   

uint8_t seat_mac[] = {0x1C, 0xDB, 0xD4, 0x45, 0x65, 0x1C}; 

typedef struct __attribute__((packed)) {
    uint16_t fsr[3];    
    int32_t raw_a;      
    int32_t raw_b;      
} seat_data_t;

typedef struct {
    seat_data_t ideal;      
    bool is_calibrated;
} calibration_t;

static seat_data_t current_data;
static bool new_data_flag = false;
static bool initial_sync_done = false;
static uint32_t last_recv_time = 0; 
static portMUX_TYPE dataMux = portMUX_INITIALIZER_UNLOCKED;
calibration_t calib = {0};
spi_device_handle_t spi_handle;

// --- FUNKCJE EKRANU I LED ---

void update_leds(bool ok) {
    gpio_set_level(LED_GREEN_PIN, ok ? 1 : 0);
    gpio_set_level(LED_RED_PIN, ok ? 0 : 1);
}

void leds_off() {
    gpio_set_level(LED_GREEN_PIN, 0);
    gpio_set_level(LED_RED_PIN, 0);
}

// --- POWIĘKSZONY INTERFEJS ---
void draw_static_interface() {
    uint16_t gray = 0x52AA;
    ssd1351_fill_screen(spi_handle, PIN_DC, 0x0000);
    
    // Powiększony obrys fotela
    ssd1351_draw_rect(spi_handle, PIN_DC, 35, 5, 3, 95, gray);   // Lewe oparcie
    ssd1351_draw_rect(spi_handle, PIN_DC, 90, 5, 3, 95, gray);   // Prawe oparcie
    ssd1351_draw_rect(spi_handle, PIN_DC, 35, 5, 58, 3, gray);   // Góra oparcia
    
    // Napisy na obrzeżach
    ssd1351_write_text(spi_handle, PIN_DC, 2, 22, "L.G", 0xFFFF);
    ssd1351_write_text(spi_handle, PIN_DC, 100, 22, "P.G", 0xFFFF);
    ssd1351_write_text(spi_handle, PIN_DC, 48, 45, "LEDZW", 0xCE79);
    ssd1351_write_text(spi_handle, PIN_DC, 5, 105, "TYL", 0xFFFF);
    ssd1351_write_text(spi_handle, PIN_DC, 5, 118, "PRZOD", 0xFFFF);
}

void update_indicators(bool s[]) {
    // Wskaźniki wewnątrz powiększonego obrysu
    ssd1351_draw_rect(spi_handle, PIN_DC, 42, 15, 15, 15, s[0] ? 0x07E0 : 0xF800); // L.G
    ssd1351_draw_rect(spi_handle, PIN_DC, 71, 15, 15, 15, s[1] ? 0x07E0 : 0xF800); // P.G
    ssd1351_draw_rect(spi_handle, PIN_DC, 52, 60, 24, 15, s[2] ? 0x07E0 : 0xF800); // Lędźwie
    ssd1351_draw_rect(spi_handle, PIN_DC, 40, 102, 48, 12, s[3] ? 0x07E0 : 0xF800); // Tył
    ssd1351_draw_rect(spi_handle, PIN_DC, 40, 115, 48, 12, s[4] ? 0x07E0 : 0xF800); // Przód
}

// --- PROCEDURA KALIBRACJI ---
void run_calibration_procedure() {
    leds_off();
    ssd1351_draw_rect(spi_handle,PIN_DC,0,0,128,128,0xFFFF);
    vTaskDelay(pdMS_TO_TICKS(400));
    ssd1351_draw_rect(spi_handle,PIN_DC,0,0,128,128,0x0000);
    ssd1351_write_text(spi_handle, PIN_DC, 25, 60, "KALIBRACJA...", 0xFFFF);
    
    // Wolne mruganie czerwoną diodą (3 razy)
    for(int i=0; i<3; i++) {
        gpio_set_level(LED_RED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(600)); 
        gpio_set_level(LED_RED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(600));
    }

    // Moment zapisu
    taskENTER_CRITICAL(&dataMux);
    calib.ideal = current_data;
    calib.is_calibrated = true;
    taskEXIT_CRITICAL(&dataMux);

    nvs_handle_t hw;
    if (nvs_open("storage", NVS_READWRITE, &hw) == ESP_OK) {
        nvs_set_blob(hw, "cal", &calib, sizeof(calibration_t));
        nvs_commit(hw); nvs_close(hw);
    }

    gpio_set_level(LED_GREEN_PIN, 1);
    ssd1351_draw_rect(spi_handle,PIN_DC,0,0,128,128,0x07E0);
    vTaskDelay(pdMS_TO_TICKS(400));
    ssd1351_draw_rect(spi_handle,PIN_DC,0,0,128,128,0x0000);
    draw_static_interface();
}

// --- CALLBACK I INIT ---

void on_data_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len == sizeof(seat_data_t)) {
        taskENTER_CRITICAL_ISR(&dataMux);
        memcpy(&current_data, data, sizeof(seat_data_t));
        last_recv_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        new_data_flag = true; 
        initial_sync_done = true;
        taskEXIT_CRITICAL_ISR(&dataMux);
    }
}

void app_main(void) {
    nvs_flash_init();
    nvs_handle_t h; 
    if(nvs_open("storage", NVS_READWRITE, &h) == ESP_OK) {
        size_t sz = sizeof(calibration_t); 
        nvs_get_blob(h, "cal", &calib, &sz); 
        nvs_close(h);
    }

    esp_netif_init(); esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg); esp_wifi_set_mode(WIFI_MODE_STA); esp_wifi_start();
    
    esp_now_init();
    esp_now_register_recv_cb(on_data_recv);
    esp_now_peer_info_t p = {.channel=1, .encrypt=false}; 
    memcpy(p.peer_addr, seat_mac, 6);
    esp_now_add_peer(&p);

    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_PIN, GPIO_PULLUP_ONLY);
    gpio_set_direction(LED_RED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_GREEN_PIN, GPIO_MODE_OUTPUT);

    spi_bus_config_t bus_cfg = {.mosi_io_num=PIN_MOSI, .sclk_io_num=PIN_CLK, .miso_io_num=-1};
    spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    spi_device_interface_config_t dev_cfg = {.clock_speed_hz=10*1000*1000, .spics_io_num=PIN_CS, .queue_size=10};
    spi_bus_add_device(SPI2_HOST, &dev_cfg, &spi_handle);

    // Inicjalizacja OLED (Reset sprzętowy)
    gpio_set_level(PIN_RST, 0); vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(PIN_RST, 1); vTaskDelay(pdMS_TO_TICKS(50));
    ssd1351_init(spi_handle, PIN_DC, PIN_RST);

    // SZUKANIE FOTELA
    ssd1351_draw_rect(spi_handle, PIN_DC,0,0,128,128, 0xFFFF);
    vTaskDelay(pdMS_TO_TICKS(600));
    ssd1351_draw_rect(spi_handle,PIN_DC,0,0,128,128,0x0000);
    ssd1351_write_text(spi_handle, PIN_DC, 10, 60, "SZUKAM FOTELA...", 0xFFFF);

    while(!initial_sync_done) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // ZNALEZIONO - Zielony błysk
    ssd1351_draw_rect(spi_handle, PIN_DC,0,0,128,128, 0x07E0);
    vTaskDelay(pdMS_TO_TICKS(600));
    ssd1351_draw_rect(spi_handle,PIN_DC,0,0,128,128,0x0000);
    draw_static_interface();

    while(1) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // UTRATA POŁĄCZENIA
        if(now - last_recv_time > DATA_TIMEOUT_MS) {
            leds_off();
            ssd1351_draw_rect(spi_handle, PIN_DC,0,0,128,128,0xF800);
            ssd1351_write_text(spi_handle, PIN_DC, 15, 60, "UTRATA SYGNALU", 0xFFFF);
            
            while(xTaskGetTickCount() * portTICK_PERIOD_MS - last_recv_time > DATA_TIMEOUT_MS) {
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            
            // POWRÓT POŁĄCZENIA
            ssd1351_draw_rect(spi_handle, PIN_DC,0,0,128,128,0x07E0);
            vTaskDelay(pdMS_TO_TICKS(100));
            ssd1351_draw_rect(spi_handle, PIN_DC,0,0,128,128,0x0000);
            draw_static_interface();
        } 
        
        // NOWE DANE
        if(new_data_flag) {
            bool s[5], all=true;
            s[0] = (abs((int)current_data.fsr[0] - (int)calib.ideal.fsr[0]) < FSR_TOLERANCE);
            s[1] = (abs((int)current_data.fsr[1] - (int)calib.ideal.fsr[1]) < FSR_TOLERANCE);
            s[2] = (abs((int)current_data.fsr[2] - (int)calib.ideal.fsr[2]) < FSR_TOLERANCE);
            s[3] = (abs(current_data.raw_a - calib.ideal.raw_a) < WEIGHT_TOLERANCE);
            s[4] = (abs(current_data.raw_b - calib.ideal.raw_b) < WEIGHT_TOLERANCE);
            for(int i=0; i<5; i++) if(!s[i]) all=false;
            
            update_indicators(s); 
            update_leds(all);
            new_data_flag = false;
        }

        // PRZYCISK
        if(gpio_get_level(BUTTON_PIN) == 0) {
            uint32_t st = xTaskGetTickCount();
            while(gpio_get_level(BUTTON_PIN) == 0) vTaskDelay(pdMS_TO_TICKS(20));
            uint32_t dur = (xTaskGetTickCount() - st) * portTICK_PERIOD_MS;

            if(dur < 1500) { 
                run_calibration_procedure(); 
            } else {
                draw_static_interface();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}