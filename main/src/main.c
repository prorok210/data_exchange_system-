#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "espnow_handler.h"
#include "uart_handler.h"
#include "drone_message.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#define TAG "MAIN"

// MAC ESP32 (root)
static const uint8_t PEER_MAC[6] = {0xF4, 0x65, 0x0B, 0x46, 0xD9, 0xE0};

void print_mac() {
    uint8_t mac[6] = {0};
    esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);
    ESP_LOGI(TAG, "STA MAC: %02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void uart_to_espnow_task(void *arg) {
    uint8_t uart_buffer[ESPNOW_MAX_DATA_LEN];
    while (1) {
        int uart_len = uart_receive_data(uart_buffer, ESPNOW_MAX_DATA_LEN);
        if (uart_len > 0) {
            espnow_send(PEER_MAC, uart_buffer, uart_len);
            ESP_LOGI(TAG, "Отправлено по ESP-NOW: %d байт", uart_len);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void) {
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "--- ESP8266 APP STARTING ---");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    uart_driver_delete(UART_PORT);
    vTaskDelay(200 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Initializing UART...");
    uart_init(115200);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Initializing WiFi + ESPNOW...");
    espnow_init();
    vTaskDelay(100 / portTICK_PERIOD_MS);

    print_mac();

    ESP_LOGI(TAG, "Adding peer...");
    espnow_add_peer(PEER_MAC);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Creating UART->ESPNOW task...");
    xTaskCreate(uart_to_espnow_task, "uart_to_espnow_task", 2048, NULL, 5, NULL);

    ESP_LOGI(TAG, "ESP8266 initialization complete");
}