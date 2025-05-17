#include "espnow_handler.h"
#include "uart_handler.h"
#include "drone_message.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "esp_timer.h"

#define TAG "MAIN"
#define ESPNOW_MAX_DATA_LEN 250

// MAC ESP8266
static const uint8_t PEER_MAC[6] = {0x40, 0x91, 0x51, 0x52, 0xad, 0x24};
// MAC ESP32: f4:65:0b:46:d9:e0

void print_mac() {
    uint8_t mac[6] = {0};
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    ESP_LOGI(TAG, "STA MAC: %02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void uart_to_espnow_task(void *arg) {
    uint8_t uart_buffer[ESPNOW_MAX_DATA_LEN];
    while (1) {
        int uart_len = uart_receive_data(uart_buffer, ESPNOW_MAX_DATA_LEN);
        if (uart_len > 0) {
            espnow_send(PEER_MAC, uart_buffer, uart_len);
            ESP_LOGI(TAG, "Sent via ESP-NOW: %d bytes", uart_len);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void test_send_task(void *arg) {
    int counter = 0;
    static uint8_t buffer[32];

    while(1) {
        snprintf((char*)buffer, sizeof(buffer), "Test message #%d", counter++);

        espnow_send(PEER_MAC, buffer, strlen((char*)buffer));
        ESP_LOGI(TAG, "Sent: %s", buffer);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void) {
    vTaskDelay(500 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "--- ESP32 APP STARTING ---");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Initializing WiFi + ESPNOW...");
    espnow_init();

    ESP_LOGI(TAG, "Adding peer...");
    espnow_add_peer(PEER_MAC);

    print_mac();

    ESP_LOGI(TAG, "Initializing UART...");
    uart_init(115200);

    ESP_LOGI(TAG, "Creating UART->ESPNOW task...");
    xTaskCreate(uart_to_espnow_task, "uart_to_espnow_task", 2048, NULL, 5, NULL);

    ESP_LOGI(TAG, "ESP32 initialization complete");
}