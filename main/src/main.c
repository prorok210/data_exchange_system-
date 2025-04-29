#include "common.h"

#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "tcpip_adapter.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "uart_handler.h"
#include "drone_message.h"
#include "mesh_light.h"

const uint8_t MESH_ID[6] = { 0x77, 0x77, 0x77, 0x77, 0x77, 0x77};

#define MESH_AP_PREFIX "MESH_"
#define MESH_AP_PASSWORD "MeshNetPassword"
#define ALT_PASSWORD "admin1337"
#define ESP32_MESH_SSID "ESP32_MESH_NETWORK"

#define UDP_PORT 5555
#define ESP32_PORT 5555
#define MAX_RETRY 10

const char *TAG = "esp8266_main";
const char *MESH_TAG = "wifi_mesh";
uint8_t tx_buf[TX_SIZE] = { 0, };
uint8_t rx_buf[RX_SIZE] = { 0, };
bool is_wifi_connected = false;
bool is_mesh_connected = false;
int mesh_layer = 2;

static EventGroupHandle_t wifi_event_group;
const static int CONNECTED_BIT = BIT0;
static int retry_count = 0;
static bool tried_alt_password = false;

static int sock = -1;
struct sockaddr_in dest_addr;
uint8_t esp32_ip[4] = {0};

mesh_light_ctl_t light_on = {
    .token_id = MESH_TOKEN_ID,
    .token_value = MESH_TOKEN_VALUE,
    .cmd = MESH_CONTROL_CMD,
    .on = 1,
};

mesh_light_ctl_t light_off = {
    .token_id = MESH_TOKEN_ID,
    .token_value = MESH_TOKEN_VALUE,
    .cmd = MESH_CONTROL_CMD,
    .on = 0,
};

void mesh_connected_indicator(int layer) {
    ESP_LOGI(MESH_TAG, "WiFi mesh connected, layer: %d", layer);
}

void mesh_disconnected_indicator(void) {
    ESP_LOGI(MESH_TAG, "WiFi mesh disconnected");
}

static void udp_client_init(void)
{
    dest_addr.sin_addr.s_addr = inet_addr("192.168.4.1");
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(ESP32_PORT);

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return;
    }

    struct sockaddr_in listen_addr;
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(UDP_PORT);

    int err = bind(sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(sock);
        sock = -1;
        return;
    }

    fcntl(sock, F_SETFL, O_NONBLOCK);

    ESP_LOGI(TAG, "UDP socket initialized");

    uint8_t reg_msg[] = {MESH_TOKEN_ID, MESH_TOKEN_VALUE, 0xFF, 0x01};
    sendto(sock, reg_msg, sizeof(reg_msg), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
}

static void connect_to_ap(uint8_t *bssid, int channel, int authmode, bool use_alt_password)
{
    wifi_config_t wifi_config = {0};

    strcpy((char *)wifi_config.sta.ssid, ESP32_MESH_SSID);

    if (use_alt_password) {
        strcpy((char *)wifi_config.sta.password, ALT_PASSWORD);
        ESP_LOGI(TAG, "Using alternate password: %s", ALT_PASSWORD);
    } else {
        strcpy((char *)wifi_config.sta.password, MESH_AP_PASSWORD);
    }

    memcpy(wifi_config.sta.bssid, bssid, 6);
    wifi_config.sta.bssid_set = true;
    wifi_config.sta.channel = channel;
    wifi_config.sta.threshold.authmode = authmode;
    wifi_config.sta.pmf_cfg.capable = false;
    wifi_config.sta.pmf_cfg.required = false;
    wifi_config.sta.scan_method = WIFI_FAST_SCAN;

    ESP_LOGI(TAG, "Connecting to AP on channel %d with auth %d", channel, authmode);

    esp_wifi_disconnect();
    vTaskDelay(200 / portTICK_PERIOD_MS);

    esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B);
    int8_t power = 82;
    esp_wifi_set_max_tx_power(power);

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());
}

static void scan_for_mesh_ap(void)
{
    ESP_LOGI(TAG, "Scanning for ESP32 mesh AP...");

    // Проверяем, не находится ли WiFi в процессе подключения
    wifi_mode_t current_mode;
    if (esp_wifi_get_mode(&current_mode) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get WiFi mode");
        return;
    }

    // Проверка состояния подключения
    if (is_wifi_connected) {
        ESP_LOGW(TAG, "WiFi already connected, skipping scan");
        return;
    }

    // Безопасный запуск сканирования
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 500,
        .scan_time.active.max = 1000
    };

    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start scan: %d", err);
        return;
    }

    uint16_t ap_num = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_num));

    if (ap_num == 0) {
        ESP_LOGI(TAG, "No APs found");
        return;
    }

    wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * ap_num);
    if (!ap_records) {
        ESP_LOGE(TAG, "Out of memory");
        return;
    }

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, ap_records));

    ESP_LOGI(TAG, "==== FOUND %d ACCESS POINTS ====", ap_num);
    for (int i = 0; i < ap_num; i++) {
        ESP_LOGI(TAG, "[%d] SSID: %s, MAC: "MACSTR", Channel: %d, RSSI: %d, Auth: %d",
                i, ap_records[i].ssid,
                MAC2STR(ap_records[i].bssid),
                ap_records[i].primary,
                ap_records[i].rssi,
                ap_records[i].authmode);
    }
    ESP_LOGI(TAG, "==== SCANNING FOR ESP32 MAC: f4:65:0b:46:d9:e1 ====");

    // Искать AP с MAC-адресом ESP32 SoftAP
    uint8_t esp32_mac[6] = {0xf4, 0x65, 0x0b, 0x46, 0xd9, 0xe1};
    bool found = false;
    char found_ssid[33] = {0};
    int found_index = -1; // Индекс найденной точки доступа

    for (int i = 0; i < ap_num; i++) {
        ESP_LOGI(TAG, "Found AP: %s, MAC: "MACSTR", RSSI: %d",
                 ap_records[i].ssid,
                 MAC2STR(ap_records[i].bssid),
                 ap_records[i].rssi);

        if (memcmp(ap_records[i].bssid, esp32_mac, 6) == 0) {
            strcpy(found_ssid, (char *)ap_records[i].ssid);
            ESP_LOGI(TAG, "Found ESP32 mesh AP: %s", found_ssid);
            found = true;
            found_index = i; // Сохраняем индекс найденной AP
            break;
        }
    }

    if (found && found_index >= 0) {
        ESP_LOGI(TAG, "Auth mode: %d, Channel: %d, Signal strength: %d dBm",
                 ap_records[found_index].authmode, ap_records[found_index].primary, ap_records[found_index].rssi);

        // Принудительное подключение к пустому SSID
        wifi_config_t wifi_config = {0};

        // Используем пустой SSID вместо того, чтобы заменять его на "ESP32_MESH_NETWORK"
        // При пустом SSID оставляем его пустым!
        if (strlen(found_ssid) == 0) {
            ESP_LOGI(TAG, "Empty SSID detected, keeping it empty for direct connection");
            // Не устанавливаем SSID - оставляем его пустым
        } else {
            strcpy((char *)wifi_config.sta.ssid, found_ssid);
        }

        // Попробуйте оба пароля
        strcpy((char *)wifi_config.sta.password, "admin1337");

        // Копируем BSSID точно как найденный
        memcpy(wifi_config.sta.bssid, esp32_mac, 6);
        wifi_config.sta.bssid_set = true;

        // Используем точно такой же канал
        wifi_config.sta.channel = ap_records[found_index].primary;

        // Установка специфических флагов для подключения к пустому SSID
        wifi_config.sta.threshold.authmode = ap_records[found_index].authmode;
        wifi_config.sta.pmf_cfg.capable = false;
        wifi_config.sta.pmf_cfg.required = false;

        // Отключаем автоматическое подключение к другим сетям
        esp_wifi_set_auto_connect(false);

        // Коротко отключаемся перед новым подключением
        esp_wifi_disconnect();
        vTaskDelay(300 / portTICK_PERIOD_MS);

        // Применяем конфигурацию и подключаемся
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));

        // Устанавливаем только протокол B для надежности
        esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B);

        // Повышаем мощность передачи
        int8_t power = 84;
        esp_wifi_set_max_tx_power(power);

        ESP_LOGI(TAG, "Connecting with empty SSID and BSSID:"MACSTR", password:%s",
                 MAC2STR(wifi_config.sta.bssid), wifi_config.sta.password);
                 
        ESP_ERROR_CHECK(esp_wifi_connect());

        // Дополнительные настройки
        wifi_config.sta.scan_method = WIFI_FAST_SCAN;
        wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
        wifi_config.sta.threshold.authmode = ap_records[found_index].authmode;

        ESP_ERROR_CHECK(esp_wifi_disconnect());
        vTaskDelay(200 / portTICK_PERIOD_MS);
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_connect());

        ESP_LOGI(TAG, "Connecting to ESP32 mesh AP: %s", found_ssid);
    } else {
        ESP_LOGI(TAG, "No ESP32 mesh AP found, will retry later");
    }

    free(ap_records);
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            ESP_LOGI(TAG, "WiFi STA started, starting scan for ESP32 mesh AP");
            scan_for_mesh_ap();
            break;

        case SYSTEM_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "===== CONNECTED to ESP32 AP! =====");
            ESP_LOGI(TAG, "SSID: %s, BSSID: "MACSTR", Channel: %d",
                     event->event_info.connected.ssid,
                     MAC2STR(event->event_info.connected.bssid),
                     event->event_info.connected.channel);
            break;

        case SYSTEM_EVENT_SCAN_DONE:
            ESP_LOGI(TAG, "Scan completed");
            // Удалена повторная попытка сканирования
            break;

        case SYSTEM_EVENT_STA_GOT_IP:
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            is_wifi_connected = true;
            is_mesh_connected = true; // Активируем "mesh" соединение
            mesh_connected_indicator(mesh_layer);

            // Запоминаем IP-адрес шлюза (это должен быть ESP32)
            tcpip_adapter_ip_info_t ip_info;
            tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);

            ESP_LOGI(TAG, "===== FULLY CONNECTED to ESP32 =====");
            ESP_LOGI(TAG, "IP: " IPSTR ", GW: " IPSTR ", MASK: " IPSTR,
                   IP2STR(&ip_info.ip), IP2STR(&ip_info.gw), IP2STR(&ip_info.netmask));

            // Устанавливаем адрес ESP32 для UDP
            dest_addr.sin_addr.s_addr = ip_info.gw.addr;

            // Инициализация UDP после получения IP
            udp_client_init();
            break;

        case SYSTEM_EVENT_STA_DISCONNECTED: {
            is_wifi_connected = false;
            is_mesh_connected = false;
            mesh_disconnected_indicator();

            uint8_t reason = event->event_info.disconnected.reason;
            const char* reason_str = "UNKNOWN"; // Объявляем переменную здесь

            // Расшифровка кодов отключения
            switch(reason) {
                case 1: reason_str = "UNSPECIFIED"; break;
                case 2: reason_str = "AUTH_EXPIRE"; break;
                case 4: reason_str = "ASSOC_EXPIRE"; break;
                case 6: reason_str = "NOT_AUTHED"; break;
                case 7: reason_str = "NOT_ASSOCED"; break;
                case 8: reason_str = "ASSOC_LEAVE"; break;
                case 14: reason_str = "4WAY_HANDSHAKE_TIMEOUT"; break;
                case 15: reason_str = "GROUP_KEY_UPDATE_TIMEOUT"; break;
                case 201: reason_str = "NO_AP_FOUND - АР не найдена при подключении"; break;
                case 202: reason_str = "AUTH_FAIL"; break;
                case 203: reason_str = "ASSOC_FAIL"; break;
                case 204: reason_str = "HANDSHAKE_TIMEOUT"; break;
                case 205: reason_str = "CONNECTION_FAIL"; break;
            }

            ESP_LOGW(TAG, "==== DISCONNECTED from AP ====");
            ESP_LOGW(TAG, "Reason: %d (%s)", reason, reason_str);

            if (reason == 202 || reason == 14 || reason == 15) {
                ESP_LOGW(TAG, "Authentication failure - check password!");
            }

            if (retry_count < MAX_RETRY) {
                esp_wifi_connect();
                retry_count++;
                ESP_LOGI(TAG, "Retrying connection attempt %d of %d", retry_count, MAX_RETRY);
            } else {
                ESP_LOGI(TAG, "Connection failed after %d attempts, rescanning...", MAX_RETRY);
                retry_count = 0;
                vTaskDelay(5000 / portTICK_PERIOD_MS);
                scan_for_mesh_ap();
            }
            break;
        }

        default:
            ESP_LOGI(TAG, "Unhandled WiFi event: %d", event->event_id);
            break;
    }
    return ESP_OK;
}

void wifi_init(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void mesh_tx_task(void *arg)
{
    ESP_LOGI(TAG, "TX task started");

    drone_message_t drone_msg;
    drone_msg_init(&drone_msg);

    while (1) {
        int uart_len = uart_receive_data(rx_buf, RX_SIZE);

        if (uart_len > 0) {
            ESP_LOGI(TAG, "Received %d bytes from UART", uart_len);

            if (drone_msg_decode(rx_buf, uart_len, &drone_msg) == 0) {
                drone_msg_log(&drone_msg);

                if (is_mesh_connected && sock != -1) {
                    int err = sendto(sock, rx_buf, uart_len, 0,
                                    (struct sockaddr *)&dest_addr, sizeof(dest_addr));
                    if (err < 0) {
                        ESP_LOGE(TAG, "Error sending data: errno %d", errno);
                    } else {
                        ESP_LOGI(TAG, "Sent %d bytes to ESP32 via UDP", uart_len);
                    }
                }
            }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void mesh_rx_task(void *arg)
{
    ESP_LOGI(TAG, "RX task started");

    drone_message_t drone_msg;
    drone_msg_init(&drone_msg);

    struct sockaddr_in source_addr;
    socklen_t socklen = sizeof(source_addr);
    int counter = 0;

    while (1) {
        if (sock != -1 && is_mesh_connected) {
            int len = recvfrom(sock, tx_buf, sizeof(tx_buf)-1, 0,
                             (struct sockaddr *)&source_addr, &socklen);

            if (len > 0) {
                ESP_LOGI(TAG, "Received %d bytes from ESP32", len);
                uart_send_data(tx_buf, len);

                if (drone_msg_decode(tx_buf, len, &drone_msg) == 0) {
                    drone_msg_log(&drone_msg);
                }
            }
        }

        if (counter++ % 100 == 0 && is_mesh_connected && sock != -1) {
            uint8_t status_msg[] = {MESH_TOKEN_ID, MESH_TOKEN_VALUE, 0xFE, counter & 0xFF};
            int err = sendto(sock, status_msg, sizeof(status_msg), 0,
                           (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            if (err > 0) {
                ESP_LOGI(TAG, "Sent status message to ESP32");
            }
        }

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "--- ESP8266 APP STARTING ---");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    uart_driver_delete(UART_PORT);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Initializing UART...");
    uart_init(115200);

    ESP_LOGI(TAG, "Initializing WiFi...");
    wifi_init();

    xTaskCreate(mesh_tx_task, "mesh_tx", 2048, NULL, 5, NULL);
    xTaskCreate(mesh_rx_task, "mesh_rx", 2048, NULL, 5, NULL);

    ESP_LOGI(TAG, "ESP8266 initialization complete");
}