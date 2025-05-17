#include <string.h>
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "mavlink_handler.h"

#define ESPNOW_MAX_DATA_LEN 256
static const char* TAG = "ESPNOW";

static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    const uint8_t *mac_addr = recv_info->src_addr;
    if (len > ESPNOW_MAX_DATA_LEN) {
        ESP_LOGW(TAG, "Принято слишком много данных (%d байт), обрезаем до %d!", len, ESPNOW_MAX_DATA_LEN);
        len = ESPNOW_MAX_DATA_LEN;
    }
    ESP_LOGI(TAG, "Получено %d байт от " MACSTR, len, MAC2STR(mac_addr));
    
    for (int i = 0; i < len; i++) {
        mavlink_parse_byte(data[i]);
    }
}

static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
    ESP_LOGI(TAG, "Отправка " MACSTR ": %s",
        MAC2STR(mac_addr),
        status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL"
    );
}

void espnow_init(void) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
}

void espnow_add_peer(const uint8_t *peer_mac) {
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, peer_mac, 6);
    peer.channel = 0;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
}

void espnow_send(const uint8_t *peer_mac, const uint8_t *data, size_t len) {
    if (len > ESPNOW_MAX_DATA_LEN) {
        ESP_LOGW(TAG, "Передача превышает лимит ESP-NOW (%d > %d байт), данные будут усечены!", len, ESPNOW_MAX_DATA_LEN);
        len = ESPNOW_MAX_DATA_LEN;
    }
    esp_err_t err = esp_now_send(peer_mac, data, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_now_send error: 0x%x", err);
    }
}