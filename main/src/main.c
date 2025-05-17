#include "espnow_handler.h"
#include "uart_handler.h"
#include "mavlink_handler.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h> 
#include "esp_timer.h"
#include <math.h>

#define TAG "MAIN"
#define ESPNOW_MAX_DATA_LEN 250

// MAC адрес узла назначения
static const uint8_t PEER_MAC[6] = {0x40, 0x91, 0x51, 0x52, 0xad, 0x24};

// Идентификаторы системы MAVLink
#define MAVLINK_SYSTEM_ID      1
#define MAVLINK_COMPONENT_ID   MAV_COMP_ID_AUTOPILOT1

// Буфер для сообщений MAVLink
static uint8_t mavlink_buffer[MAVLINK_BUFFER_SIZE];

void print_mac() {
    uint8_t mac[6] = {0};
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    ESP_LOGI(TAG, "STA MAC: %02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// Обработчик сообщений MAVLink
void mavlink_message_handler(mavlink_message_t *msg) {
    ESP_LOGI(TAG, "Получено сообщение MAVLink ID: %d", msg->msgid);
    
    // Используем новую функцию для анализа и безопасного вывода данных
    mavlink_parse_message(msg);
    
    // Пересылаем сообщение через ESP-NOW
    uint16_t len = mavlink_pack_message(msg, mavlink_buffer);
    if (len > 0) {
        espnow_send(PEER_MAC, mavlink_buffer, len);
        ESP_LOGI(TAG, "Сообщение MAVLink переслано через ESP-NOW, %d байт", len);
    }
}

// Задача для отправки данных UART в ESP-NOW
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

// Задача для периодической отправки heartbeat
void heartbeat_task(void *arg) {
    while (1) {
        mavlink_message_t msg;
        
        // Отправляем heartbeat
        mavlink_msg_heartbeat_pack(
            MAVLINK_SYSTEM_ID,
            MAVLINK_COMPONENT_ID,
            &msg,
            MAV_TYPE_QUADROTOR,
            MAV_AUTOPILOT_GENERIC,
            MAV_MODE_FLAG_SAFETY_ARMED | MAV_MODE_FLAG_MANUAL_INPUT_ENABLED,
            0,
            MAV_STATE_ACTIVE
        );
        
        uint16_t len = mavlink_pack_message(&msg, mavlink_buffer);
        
        // Отправляем по UART
        uart_send_data(mavlink_buffer, len);
        
        // Отправляем через ESP-NOW
        espnow_send(PEER_MAC, mavlink_buffer, len);
        
        ESP_LOGI(TAG, "Heartbeat отправлен");
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Пример задачи, которая отправляет тестовые данные телеметрии
void telemetry_test_task(void *arg) {
    uint32_t counter = 0;
    
    while(1) {
        mavlink_message_t msg;
        uint16_t len;
        
        // Генерируем случайные данные для демонстрации
        float roll = sinf((float)counter / 50.0f) * 0.5f;  // ±0.5 рад
        float pitch = cosf((float)counter / 30.0f) * 0.3f; // ±0.3 рад
        float yaw = (float)counter / 100.0f;
        
        int32_t lat = 550000000 + counter % 10000;  // ~55.00 градусов
        int32_t lon = 370000000 + counter % 20000;  // ~37.00 градусов
        int32_t alt = 100000 + (counter % 1000) * 10;  // ~100-110м в мм
        
        uint16_t voltage = 12000 + (counter % 1000); // ~12В в мВ
        int16_t current = 5000 + (counter % 2000);   // ~5А в 10*мА
        uint8_t battery = 100 - (counter % 30);      // 70-100%

        // Отправляем пакет с ориентацией
        msg = mavlink_prepare_attitude(roll, pitch, yaw, 0.01f, 0.01f, 0.02f);
        len = mavlink_pack_message(&msg, mavlink_buffer);
        uart_send_data(mavlink_buffer, len);
        espnow_send(PEER_MAC, mavlink_buffer, len);
        ESP_LOGI(TAG, "Отправлен пакет ATTITUDE");
        
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Отправляем пакет с позицией
        msg = mavlink_prepare_position(lat, lon, alt, alt - 100000,
                                     (int16_t)(sinf((float)counter / 20.0f) * 500), 
                                     (int16_t)(cosf((float)counter / 20.0f) * 500),
                                     0, (uint16_t)(yaw * 180.0f / M_PI * 100.0f));
        len = mavlink_pack_message(&msg, mavlink_buffer);
        uart_send_data(mavlink_buffer, len);
        espnow_send(PEER_MAC, mavlink_buffer, len);
        ESP_LOGI(TAG, "Отправлен пакет GLOBAL_POSITION_INT");
        
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Отправляем пакет с информацией о батарее
        msg = mavlink_prepare_battery_status(battery, voltage, current);
        len = mavlink_pack_message(&msg, mavlink_buffer);
        uart_send_data(mavlink_buffer, len);
        espnow_send(PEER_MAC, mavlink_buffer, len);
        ESP_LOGI(TAG, "Отправлен пакет BATTERY_STATUS");
        
        counter++;
        vTaskDelay(pdMS_TO_TICKS(800)); // Пауза между наборами сообщений
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

    ESP_LOGI(TAG, "Initializing MAVLink...");
    mavlink_init(MAVLINK_SYSTEM_ID, MAVLINK_COMPONENT_ID);
    mavlink_register_message_handler(mavlink_message_handler);

    ESP_LOGI(TAG, "Initializing UART...");
    uart_init(115200);

    ESP_LOGI(TAG, "Creating tasks...");
    xTaskCreate(uart_to_espnow_task, "uart_to_espnow", 4096, NULL, 5, NULL);
    xTaskCreate(heartbeat_task, "heartbeat", 4096, NULL, 3, NULL);
    xTaskCreate(telemetry_test_task, "telemetry_test", 4096, NULL, 4, NULL);

    ESP_LOGI(TAG, "ESP32 initialization complete");
}