#include "espnow_handler.h"
#include <string.h>
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "drone_message.h"
#include "tcpip_adapter.h"

static const char* TAG = "ESPNOW";

static void espnow_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
    if (data_len > ESPNOW_MAX_DATA_LEN) {
        ESP_LOGW(TAG, "Принято слишком много данных (%d байт), обрезаем до %d!", data_len, ESPNOW_MAX_DATA_LEN);
        data_len = ESPNOW_MAX_DATA_LEN;
    }
    ESP_LOGI(TAG, "Получено %d байт от " MACSTR, data_len, MAC2STR(mac_addr));
    drone_message_t drone_msg;
    if (drone_msg_decode(data, data_len, &drone_msg) == 0) {
        drone_msg_log(&drone_msg);
    }
}

static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
    ESP_LOGI(TAG, "Отправка " MACSTR ": %s", MAC2STR(mac_addr), (status == ESP_NOW_SEND_SUCCESS) ? "OK" : "FAIL");
}

void espnow_init(void) {
    // Инициализируем TCP/IP адаптер перед WiFi (требуется для ESP8266)
    tcpip_adapter_init();

    // Инициализируем WiFi с проверкой на уже инициализированное состояние
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_wifi_init(&cfg);
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "WiFi уже инициализирован, продолжаем...");
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Ошибка при инициализации WiFi: 0x%x", ret);
        return;
    }

    // Устанавливаем режим и запускаем WiFi с игнорированием ошибки ESP_ERR_INVALID_STATE
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_NOT_INIT && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Ошибка при установке режима WiFi: 0x%x", ret);
        return;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Ошибка при запуске WiFi: 0x%x", ret);
        return;
    }

    // Инициализируем ESP-NOW
    ret = esp_now_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Ошибка при инициализации ESP-NOW: 0x%x", ret);
        return;
    }

    // Регистрируем callback функции
    esp_now_register_recv_cb(espnow_recv_cb);
    esp_now_register_send_cb(espnow_send_cb);

    ESP_LOGI(TAG, "ESP-NOW инициализирован успешно");
}

void espnow_add_peer(const uint8_t *peer_mac) {
    if (!peer_mac) {
        ESP_LOGE(TAG, "Неверный MAC адрес (NULL)");
        return;
    }

    ESP_LOGI(TAG, "Добавление пира: " MACSTR, MAC2STR(peer_mac));

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, peer_mac, 6);
    peer.channel = 0;
    peer.encrypt = false;

    esp_now_del_peer(peer_mac);

    esp_err_t ret = esp_now_add_peer(&peer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Ошибка при добавлении пира: 0x%x", ret);
    } else {
        ESP_LOGI(TAG, "Пир успешно добавлен");
    }
}

void espnow_send(const uint8_t *peer_mac, const uint8_t *data, size_t len) {
    if (!peer_mac || !data) {
        ESP_LOGE(TAG, "Неверные параметры для отправки данных");
        return;
    }

    if (len > ESPNOW_MAX_DATA_LEN) {
        ESP_LOGW(TAG, "Передача превышает лимит ESP-NOW (%d > %d байт), данные будут усечены!", (int)len, ESPNOW_MAX_DATA_LEN);
        len = ESPNOW_MAX_DATA_LEN;
    }

    esp_err_t err = esp_now_send(peer_mac, data, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_now_send error: 0x%x", err);
    }
}