#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_mesh.h"
#include "mesh_light.h" 
#include "common.h" 
#include "msg_handler.h"

static void log_mac(const uint8_t *mac_addr) {
    ESP_LOGI(MESH_TAG, "MAC: %02x:%02x:%02x:%02x:%02x:%02x", 
        mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
}

// Отправка данных по сети
void esp_mesh_p2p_tx_main(void *arg)
{
    int i;
    esp_err_t err;
    int send_count = 0;
    mesh_addr_t route_table[CONFIG_MESH_ROUTE_TABLE_SIZE];
    int route_table_size = 0;
    mesh_data_t data;
    data.data = tx_buf;
    data.size = sizeof(tx_buf);
    data.proto = MESH_PROTO_BIN;
    data.tos = MESH_TOS_P2P;
    is_running = true;

    while (is_running) {
        esp_mesh_get_routing_table((mesh_addr_t *) &route_table,
                                   CONFIG_MESH_ROUTE_TABLE_SIZE * 6, &route_table_size);
        if (send_count && !(send_count % 100)) {
            ESP_LOGI(MESH_TAG, "size:%d/%d,send_count:%d", route_table_size,
                     esp_mesh_get_routing_table_size(), send_count);
        }
        send_count++;
        tx_buf[25] = (send_count >> 24) & 0xff;
        tx_buf[24] = (send_count >> 16) & 0xff;
        tx_buf[23] = (send_count >> 8) & 0xff;
        tx_buf[22] = (send_count >> 0) & 0xff;
        if (send_count % 2) {
            memcpy(tx_buf, (uint8_t *)&light_on, sizeof(light_on));
        } else {
            memcpy(tx_buf, (uint8_t *)&light_off, sizeof(light_off));
        }

        // отправляем данные каждому узлу в таблице маршрутизации
        for (i = 0; i < route_table_size; i++) {
            err = esp_mesh_send(&route_table[i], &data, MESH_DATA_P2P, NULL, 0);
            if (err) {
                ESP_LOGE(MESH_TAG, "[ROOT-2-UNICAST:%d][L:%d]parent: err:0x%x, proto:%d, tos:%d, heap:%" PRId32,
                         send_count, mesh_layer, err, data.proto, data.tos, esp_get_minimum_free_heap_size());
                ESP_LOGI(MESH_TAG, "Parent MAC:");
                log_mac(mesh_parent_addr.addr);
                ESP_LOGI(MESH_TAG, "Destination MAC:");
                log_mac(route_table[i].addr);
            } else if (!(send_count % 100)) {
                ESP_LOGW(MESH_TAG, "[ROOT-2-UNICAST:%d][L:%d][rtableSize:%d] err:0x%x, proto:%d, tos:%d, heap:%" PRId32,
                         send_count, mesh_layer,
                         esp_mesh_get_routing_table_size(),
                         err, data.proto, data.tos, esp_get_minimum_free_heap_size());
                ESP_LOGI(MESH_TAG, "Parent MAC:");
                log_mac(mesh_parent_addr.addr);
                ESP_LOGI(MESH_TAG, "Destination MAC:");
                log_mac(route_table[i].addr);
            }
        }
        if (route_table_size < 10) {
            vTaskDelay(1 * 1000 / portTICK_PERIOD_MS);
        }
    }
    vTaskDelete(NULL);
}


// Прием и обработка входящих сообщений
void esp_mesh_p2p_rx_main(void *arg)
{
    int recv_count = 0;
    esp_err_t err;
    mesh_addr_t from;
    int send_count = 0;
    mesh_data_t data;
    int flag = 0;
    data.data = rx_buf;
    data.size = RX_SIZE;
    is_running = true;

    while (is_running) {
        data.size = RX_SIZE;

        // получение сообщения
        err = esp_mesh_recv(&from, &data, portMAX_DELAY, &flag, NULL, 0);
        if (err != ESP_OK || !data.size) {
            ESP_LOGE(MESH_TAG, "err:0x%x, size:%d", err, data.size);
            continue;
        }
        if (data.size >= sizeof(send_count)) {
            send_count = (data.data[25] << 24) | (data.data[24] << 16)
                         | (data.data[23] << 8) | data.data[22];
        }
        recv_count++;
        
        // обработка сообщения
        mesh_light_process(&from, data.data, data.size);
        if (!(recv_count % 1)) {
            ESP_LOGW(MESH_TAG, "[#RX:%d/%d][L:%d] size:%d, flag:%d, err:0x%x, proto:%d, tos:%d, heap:%" PRId32,
                     recv_count, send_count, mesh_layer,
                     data.size, flag, err, data.proto,
                     data.tos, esp_get_minimum_free_heap_size());
            ESP_LOGI(MESH_TAG, "Parent MAC:");
            log_mac(mesh_parent_addr.addr);
            ESP_LOGI(MESH_TAG, "Source MAC:");
            log_mac(from.addr);
        }
    }
    vTaskDelete(NULL);
}