#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_mesh.h"
#include "mesh_light.h" 
#include "common.h" 
#include "msg_handler.h"
#include "uart_handler.h"
#include "drone_message.h"  

static void log_mac(const uint8_t *mac_addr) {
    ESP_LOGI(MESH_TAG, "MAC: %02x:%02x:%02x:%02x:%02x:%02x", 
        mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
}

static void process_network_data_to_fc(uint8_t *data, int size) {
    ESP_LOGI(MESH_TAG, "Обработка данных из сети для полетного контроллера, размер: %d", size);
    
    ESP_LOGI(MESH_TAG, "Первые байты сообщения:");
    for (int i = 0; i < size && i < 10; i++) {
        ESP_LOGI(MESH_TAG, "[%d] = 0x%02X (%c)", i, data[i], (data[i] >= 32 && data[i] <= 126) ? data[i] : '.');
    }
    
    drone_message_t drone_msg;
    if (drone_msg_decode(data, size, &drone_msg) == 0) {
        ESP_LOGI(MESH_TAG, "Получено валидное сообщение от дрона:");
        drone_msg_log(&drone_msg);
    } else {
        ESP_LOGW(MESH_TAG, "Не удалось распознать формат сообщения дрона");
    }
    
    uart_send_data(data, size);
}

static void process_fc_data_to_network(uint8_t *data, int size) {
    ESP_LOGI(MESH_TAG, "Обработка данных от полетного контроллера для сети, размер: %d", size);
    
    ESP_LOGI(MESH_TAG, "Первые байты сообщения:");
    for (int i = 0; i < size && i < 10; i++) {
        ESP_LOGI(MESH_TAG, "[%d] = 0x%02X (%c)", i, data[i], (data[i] >= 32 && data[i] <= 126) ? data[i] : '.');
    }
    
    drone_message_t drone_msg;
    if (drone_msg_decode(data, size, &drone_msg) == 0) {
        ESP_LOGI(MESH_TAG, "Отправка сообщения дрона в сеть:");
        drone_msg_log(&drone_msg);
    } else {
        ESP_LOGW(MESH_TAG, "Не удалось распознать формат сообщения дрона от контроллера");
    }
}

// Отправка данных по сети
void esp_mesh_p2p_tx_main(void *arg)
{
    int i;
    esp_err_t err;
    mesh_addr_t route_table[CONFIG_MESH_ROUTE_TABLE_SIZE];
    int route_table_size = 0;
    mesh_data_t data;
    data.data = tx_buf;
    data.size = sizeof(tx_buf);
    data.proto = MESH_PROTO_BIN;
    data.tos = MESH_TOS_P2P;
    is_running = true;
    uint8_t uart_buffer[256];
    int uart_data_len = 0;

    while (is_running) {
        // Проверяем наличие данных от UART (полетного контроллера)
        uart_data_len = uart_receive_data(uart_buffer, sizeof(uart_buffer));
        if (uart_data_len > 0) {
            // Обрабатываем данные от полетного контроллера
            process_fc_data_to_network(uart_buffer, uart_data_len);
            
            // Копируем данные в буфер отправки
            if (uart_data_len <= sizeof(tx_buf)) {
                memcpy(tx_buf, uart_buffer, uart_data_len);
                data.size = uart_data_len;
                
                // Получаем таблицу маршрутизации
                esp_mesh_get_routing_table((mesh_addr_t *) &route_table,
                                          CONFIG_MESH_ROUTE_TABLE_SIZE * 6, &route_table_size);
                
                // Отправляем данные каждому узлу в таблице маршрутизации
                for (i = 0; i < route_table_size; i++) {
                    err = esp_mesh_send(&route_table[i], &data, MESH_DATA_P2P, NULL, 0);
                    if (err) {
                        ESP_LOGE(MESH_TAG, "[FC-2-NET][L:%d] Ошибка отправки: 0x%x, размер: %d", 
                                mesh_layer, err, data.size);
                        log_mac(route_table[i].addr);
                    } else {
                        ESP_LOGD(MESH_TAG, "[FC-2-NET][L:%d] Успешная отправка в сеть, размер: %d", 
                                mesh_layer, data.size);
                    }
                }
            } else {
                ESP_LOGE(MESH_TAG, "Данные от UART слишком велики для буфера: %d > %d", 
                        uart_data_len, sizeof(tx_buf));
            }
        }
        
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

// Прием и обработка входящих сообщений
void esp_mesh_p2p_rx_main(void *arg)
{
    int recv_count = 0;
    esp_err_t err;
    mesh_addr_t from;
    mesh_data_t data;
    int flag = 0;
    data.data = rx_buf;
    data.size = RX_SIZE;
    is_running = true;

    while (is_running) {
        data.size = RX_SIZE;
        
        // Получение сообщения из сети
        err = esp_mesh_recv(&from, &data, portMAX_DELAY, &flag, NULL, 0);
        if (err != ESP_OK || !data.size) {
            ESP_LOGE(MESH_TAG, "Ошибка приема: 0x%x, размер: %d", err, data.size);
            continue;
        }
        
        recv_count++;
        
        // Логгирование
        if (!(recv_count % 10)) {
            ESP_LOGD(MESH_TAG, "[NET-2-FC][L:%d] Получено сообщение, размер: %d", 
                    mesh_layer, data.size);
            log_mac(from.addr);
        }
        
        // Обработка полученных данных и отправка на полетный контроллер
        process_network_data_to_fc(data.data, data.size);
    }
    vTaskDelete(NULL);
}