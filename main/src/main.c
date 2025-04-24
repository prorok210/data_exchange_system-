#include <string.h>
#include <inttypes.h>
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_mesh_internal.h"
#include "nvs_flash.h"
#include "mesh_light.h"
#include "common.h"
#include "event_handler.h"
#include "msg_handler.h"
#include "uart_handler.h"

#define RX_SIZE          (1500)
#define TX_SIZE          (1460)


const char *MESH_TAG = "mesh_main";
const uint8_t MESH_ID[6] = { 0x77, 0x77, 0x77, 0x77, 0x77, 0x77};
uint8_t tx_buf[TX_SIZE] = { 0, };
uint8_t rx_buf[RX_SIZE] = { 0, };
bool is_running = true;
bool is_mesh_connected = false;
mesh_addr_t mesh_parent_addr;
int mesh_layer = -1;
esp_netif_t *netif_sta = NULL;

mesh_light_ctl_t light_on = {
    .cmd = MESH_CONTROL_CMD,
    .on = 1,
    .token_id = MESH_TOKEN_ID,
    .token_value = MESH_TOKEN_VALUE,
};

mesh_light_ctl_t light_off = {
    .cmd = MESH_CONTROL_CMD,
    .on = 0,
    .token_id = MESH_TOKEN_ID,
    .token_value = MESH_TOKEN_VALUE,
};


// Запускаем задачи передачи и приема данных
esp_err_t esp_mesh_comm_p2p_start(void)
{
    static bool is_comm_p2p_started = false;
    if (!is_comm_p2p_started) {
        is_comm_p2p_started = true;
        xTaskCreate(esp_mesh_p2p_tx_main, "MPTX", 3072, NULL, 5, NULL);
        xTaskCreate(esp_mesh_p2p_rx_main, "MPRX", 3072, NULL, 5, NULL);
    }
    return ESP_OK;
}


void app_main(void)
{
#ifdef TEST_BUILD
    ESP_LOGI(MESH_TAG, "Успешная загрузка в режиме тестирования!");
    return;
#endif
    /*  uart initialization */
    ESP_ERROR_CHECK(uart_init(UART_BAUD_RATE));


    ESP_ERROR_CHECK(mesh_light_init());
    ESP_ERROR_CHECK(nvs_flash_init());
    /*  tcpip initialization */
    ESP_ERROR_CHECK(esp_netif_init());
    /*  event initialization */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    /*  create network interfaces for mesh (only station instance saved for further manipulation, soft AP instance ignored */
    ESP_ERROR_CHECK(esp_netif_create_default_wifi_mesh_netifs(&netif_sta, NULL));

    uint8_t eth_mac[6];
    esp_read_mac(eth_mac, ESP_MAC_WIFI_STA);
    ESP_LOGI(MESH_TAG, "Device MAC: " MACSTR, MAC2STR(eth_mac));

    /*  wifi initialization */
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&config));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_start());

    /*  mesh initialization */
    ESP_ERROR_CHECK(esp_mesh_init());
    ESP_ERROR_CHECK(esp_mesh_set_self_organized(true, false)); 
    ESP_ERROR_CHECK(esp_event_handler_register(MESH_EVENT, ESP_EVENT_ANY_ID, &mesh_event_handler, NULL));
    
#ifdef CONFIG_MESH_DEVICE_TYPE_ROOT
    ESP_LOGI(MESH_TAG, "Configuring device as ROOT node");
    ESP_ERROR_CHECK(esp_mesh_set_type(MESH_ROOT));
    ESP_ERROR_CHECK(esp_mesh_fix_root(true));
#else
    ESP_LOGI(MESH_TAG, "Configuring device as INTERNAL node");
    ESP_ERROR_CHECK(esp_mesh_set_type(MESH_NODE));
#endif

    /*  set mesh topology */
    ESP_ERROR_CHECK(esp_mesh_set_topology(CONFIG_MESH_TOPOLOGY));
    /*  set mesh max layer according to the topology */
    ESP_ERROR_CHECK(esp_mesh_set_max_layer(CONFIG_MESH_MAX_LAYER));
    ESP_ERROR_CHECK(esp_mesh_set_root_healing_delay(60));
    ESP_ERROR_CHECK(esp_mesh_set_vote_percentage(1));
    ESP_ERROR_CHECK(esp_mesh_set_xon_qsize(128));
#ifdef CONFIG_MESH_ENABLE_PS
    /* Enable mesh PS function */
    ESP_ERROR_CHECK(esp_mesh_enable_ps());
    /* better to increase the associate expired time, if a small duty cycle is set. */
    ESP_ERROR_CHECK(esp_mesh_set_ap_assoc_expire(60));
    /* better to increase the announce interval to avoid too much management traffic, if a small duty cycle is set. */
    ESP_ERROR_CHECK(esp_mesh_set_announce_interval(600, 3300));
#else
    /* Disable mesh PS function */
    ESP_ERROR_CHECK(esp_mesh_disable_ps());
    ESP_ERROR_CHECK(esp_mesh_set_ap_assoc_expire(10));
#endif
    mesh_cfg_t cfg = MESH_INIT_CONFIG_DEFAULT();
    /* mesh ID */
    memcpy((uint8_t *) &cfg.mesh_id, MESH_ID, 6);
    /* router */
    cfg.channel = CONFIG_MESH_CHANNEL;
    
    cfg.router.ssid_len = strlen(CONFIG_MESH_ROUTER_SSID);
    memcpy((uint8_t *) &cfg.router.ssid, CONFIG_MESH_ROUTER_SSID, cfg.router.ssid_len);
    memcpy((uint8_t *) &cfg.router.password, CONFIG_MESH_ROUTER_PASSWD, 
           strlen(CONFIG_MESH_ROUTER_PASSWD));
    cfg.router.allow_router_switch = true;
    
    /* mesh softAP */
    ESP_ERROR_CHECK(esp_mesh_set_ap_authmode(CONFIG_MESH_AP_AUTHMODE));
    cfg.mesh_ap.max_connection = CONFIG_MESH_AP_CONNECTIONS;
    cfg.mesh_ap.nonmesh_max_connection = CONFIG_MESH_NON_MESH_AP_CONNECTIONS;

    memcpy((uint8_t *) &cfg.mesh_ap.password, CONFIG_MESH_AP_PASSWD,
           strlen(CONFIG_MESH_AP_PASSWD));
    
    ESP_ERROR_CHECK(esp_mesh_set_config(&cfg));
    /* mesh start */
    ESP_ERROR_CHECK(esp_mesh_start());
#ifdef CONFIG_MESH_ENABLE_PS
    /* set the device active duty cycle. (default:10, MESH_PS_DEVICE_DUTY_REQUEST) */
    ESP_ERROR_CHECK(esp_mesh_set_active_duty_cycle(CONFIG_MESH_PS_DEV_DUTY, CONFIG_MESH_PS_DEV_DUTY_TYPE));
    /* set the network active duty cycle. (default:10, -1, MESH_PS_NETWORK_DUTY_APPLIED_ENTIRE) */
    ESP_ERROR_CHECK(esp_mesh_set_network_duty_cycle(CONFIG_MESH_PS_NWK_DUTY, CONFIG_MESH_PS_NWK_DUTY_DURATION, CONFIG_MESH_PS_NWK_DUTY_RULE));
#endif
    ESP_LOGI(MESH_TAG, "mesh starts successfully, heap:%" PRId32 ", %s<%d>%s, ps:%d",  esp_get_minimum_free_heap_size(),
             esp_mesh_is_root_fixed() ? "root fixed" : "root not fixed",
             esp_mesh_get_topology(), esp_mesh_get_topology() ? "(chain)":"(tree)", esp_mesh_is_ps_enabled());

    ESP_LOGI(MESH_TAG, "Starting mesh with router SSID: %s, channel: %d", 
             CONFIG_MESH_ROUTER_SSID, CONFIG_MESH_CHANNEL);
}
