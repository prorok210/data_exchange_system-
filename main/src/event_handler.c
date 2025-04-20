#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include "esp_event.h"
#include "esp_mesh.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "mesh_light.h"
#include "event_handler.h"
#include "common.h"

extern esp_netif_t *netif_sta;
extern const char *MESH_TAG;
extern mesh_addr_t mesh_parent_addr;
extern int mesh_layer;
extern bool is_mesh_connected;

static void log_mac(const uint8_t *mac_addr) {
    ESP_LOGI(MESH_TAG, "MAC: %02x:%02x:%02x:%02x:%02x:%02x", 
        mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
}

void mesh_event_handler(void *arg, esp_event_base_t event_base,
    int32_t event_id, void *event_data) {
    mesh_addr_t id = {0,};
    static uint16_t last_layer = 0;

    switch (event_id) {
        case MESH_EVENT_STARTED: {
            esp_mesh_get_id(&id);
            ESP_LOGI(MESH_TAG, "<MESH_EVENT_MESH_STARTED>ID:");
            log_mac(id.addr);
            is_mesh_connected = false;
            mesh_layer = esp_mesh_get_layer();
        }
        break;
        case MESH_EVENT_STOPPED: {
            ESP_LOGI(MESH_TAG, "<MESH_EVENT_STOPPED>");
            is_mesh_connected = false;
            mesh_layer = esp_mesh_get_layer();
        }
        break;
        case MESH_EVENT_CHILD_CONNECTED: {
            mesh_event_child_connected_t *child_connected = (mesh_event_child_connected_t *)event_data;
            ESP_LOGI(MESH_TAG, "<MESH_EVENT_CHILD_CONNECTED>aid:%d", child_connected->aid);
            log_mac(child_connected->mac);
        }
        break;
        case MESH_EVENT_CHILD_DISCONNECTED: {
            mesh_event_child_disconnected_t *child_disconnected = (mesh_event_child_disconnected_t *)event_data;
            ESP_LOGI(MESH_TAG, "<MESH_EVENT_CHILD_DISCONNECTED>aid:%d", child_disconnected->aid);
            log_mac(child_disconnected->mac);
        }
        break;
        case MESH_EVENT_ROUTING_TABLE_ADD: {
            mesh_event_routing_table_change_t *routing_table = (mesh_event_routing_table_change_t *)event_data;
            ESP_LOGW(MESH_TAG, "<MESH_EVENT_ROUTING_TABLE_ADD>add %d, new:%d, layer:%d",
                routing_table->rt_size_change,
                routing_table->rt_size_new, mesh_layer);
        }
        break;
        case MESH_EVENT_ROUTING_TABLE_REMOVE: {
            mesh_event_routing_table_change_t *routing_table = (mesh_event_routing_table_change_t *)event_data;
            ESP_LOGW(MESH_TAG, "<MESH_EVENT_ROUTING_TABLE_REMOVE>remove %d, new:%d, layer:%d",
                routing_table->rt_size_change,
                routing_table->rt_size_new, mesh_layer);
        }
        break;
        case MESH_EVENT_NO_PARENT_FOUND: {
            mesh_event_no_parent_found_t *no_parent = (mesh_event_no_parent_found_t *)event_data;
            ESP_LOGI(MESH_TAG, "<MESH_EVENT_NO_PARENT_FOUND>scan times:%d",
                no_parent->scan_times);
        }
        break;
        case MESH_EVENT_PARENT_CONNECTED: {
            mesh_event_connected_t *connected = (mesh_event_connected_t *)event_data;
            esp_mesh_get_id(&id);
            mesh_layer = connected->self_layer;
            memcpy(&mesh_parent_addr.addr, connected->connected.bssid, 6);
            ESP_LOGI(MESH_TAG, "<MESH_EVENT_PARENT_CONNECTED>layer:%d-->%d, parent:", 
                last_layer, mesh_layer);
            log_mac(mesh_parent_addr.addr);
            ESP_LOGI(MESH_TAG, "%s, ID:", esp_mesh_is_root() ? "<ROOT>" : 
                (mesh_layer == 2) ? "<layer2>" : "");
            log_mac(id.addr);
            ESP_LOGI(MESH_TAG, "duty:%d", connected->duty);
            
            last_layer = mesh_layer;
            mesh_connected_indicator(mesh_layer);
            is_mesh_connected = true;
            if (esp_mesh_is_root()) {
                esp_netif_dhcpc_stop(netif_sta);
                esp_netif_dhcpc_start(netif_sta);
            }
            esp_mesh_comm_p2p_start();
        }
        break;
        case MESH_EVENT_PARENT_DISCONNECTED: {
            mesh_event_disconnected_t *disconnected = (mesh_event_disconnected_t *)event_data;
            ESP_LOGI(MESH_TAG,
                "<MESH_EVENT_PARENT_DISCONNECTED>reason:%d",
                disconnected->reason);
            is_mesh_connected = false;
            mesh_disconnected_indicator();
            mesh_layer = esp_mesh_get_layer();
        }
        break;
        case MESH_EVENT_LAYER_CHANGE: {
            mesh_event_layer_change_t *layer_change = (mesh_event_layer_change_t *)event_data;
            mesh_layer = layer_change->new_layer;
            ESP_LOGI(MESH_TAG, "<MESH_EVENT_LAYER_CHANGE>layer:%d-->%d%s",
                last_layer, mesh_layer,
                esp_mesh_is_root() ? "<ROOT>" :
                (mesh_layer == 2) ? "<layer2>" : "");
            last_layer = mesh_layer;
            mesh_connected_indicator(mesh_layer);
        }
        break;
        case MESH_EVENT_ROOT_ADDRESS: {
            mesh_event_root_address_t *root_addr = (mesh_event_root_address_t *)event_data;
            ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROOT_ADDRESS>root address:");
            log_mac(root_addr->addr);
        }
        break;
        case MESH_EVENT_VOTE_STARTED: {
            mesh_event_vote_started_t *vote_started = (mesh_event_vote_started_t *)event_data;
            ESP_LOGI(MESH_TAG, "<MESH_EVENT_VOTE_STARTED>attempts:%d, reason:%d, rc_addr:",
                vote_started->attempts, vote_started->reason);
            log_mac(vote_started->rc_addr.addr);
        }
        break;
        case MESH_EVENT_VOTE_STOPPED: {
            ESP_LOGI(MESH_TAG, "<MESH_EVENT_VOTE_STOPPED>");
            break;
        }
        case MESH_EVENT_ROOT_SWITCH_REQ: {
            mesh_event_root_switch_req_t *switch_req = (mesh_event_root_switch_req_t *)event_data;
            ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROOT_SWITCH_REQ>reason:%d, rc_addr:",
                switch_req->reason);
            log_mac(switch_req->rc_addr.addr);
        }
        break;
        case MESH_EVENT_ROOT_SWITCH_ACK: {
            /* new root */
            mesh_layer = esp_mesh_get_layer();
            esp_mesh_get_parent_bssid(&mesh_parent_addr);
            ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROOT_SWITCH_ACK>layer:%d, parent:", mesh_layer);
            log_mac(mesh_parent_addr.addr);
        }
        break;
        case MESH_EVENT_TODS_STATE: {
            mesh_event_toDS_state_t *toDs_state = (mesh_event_toDS_state_t *)event_data;
            ESP_LOGI(MESH_TAG, "<MESH_EVENT_TODS_REACHABLE>state:%d", *toDs_state);
        }
        break;
        case MESH_EVENT_ROOT_FIXED: {
            mesh_event_root_fixed_t *root_fixed = (mesh_event_root_fixed_t *)event_data;
            ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROOT_FIXED>%s",
                root_fixed->is_fixed ? "fixed" : "not fixed");
        }
        break;
        case MESH_EVENT_ROOT_ASKED_YIELD: {
            mesh_event_root_conflict_t *root_conflict = (mesh_event_root_conflict_t *)event_data;
            ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROOT_ASKED_YIELD>");
            log_mac(root_conflict->addr);
            ESP_LOGI(MESH_TAG, "rssi:%d, capacity:%d", 
                root_conflict->rssi, root_conflict->capacity);
        }
        break;
        case MESH_EVENT_CHANNEL_SWITCH: {
            mesh_event_channel_switch_t *channel_switch = (mesh_event_channel_switch_t *)event_data;
            ESP_LOGI(MESH_TAG, "<MESH_EVENT_CHANNEL_SWITCH>new channel:%d", channel_switch->channel);
        }
        break;
        case MESH_EVENT_SCAN_DONE: {
            mesh_event_scan_done_t *scan_done = (mesh_event_scan_done_t *)event_data;
            ESP_LOGI(MESH_TAG, "<MESH_EVENT_SCAN_DONE>number:%d",
                scan_done->number);
        }
        break;
        case MESH_EVENT_NETWORK_STATE: {
            mesh_event_network_state_t *network_state = (mesh_event_network_state_t *)event_data;
            ESP_LOGI(MESH_TAG, "<MESH_EVENT_NETWORK_STATE>is_rootless:%d",
                network_state->is_rootless);
        }
        break;
        case MESH_EVENT_STOP_RECONNECTION: {
            ESP_LOGI(MESH_TAG, "<MESH_EVENT_STOP_RECONNECTION>");
        }
        break;
        case MESH_EVENT_FIND_NETWORK: {
            mesh_event_find_network_t *find_network = (mesh_event_find_network_t *)event_data;
            ESP_LOGI(MESH_TAG, "<MESH_EVENT_FIND_NETWORK>new channel:%d, router BSSID:", 
                find_network->channel);
            log_mac(find_network->router_bssid);
        }
        break;
        case MESH_EVENT_ROUTER_SWITCH: {
            mesh_event_router_switch_t *router_switch = (mesh_event_router_switch_t *)event_data;
            ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROUTER_SWITCH>new router:%s, channel:%d", 
                router_switch->ssid, router_switch->channel);
            log_mac(router_switch->bssid);
        }
        break;
        case MESH_EVENT_PS_PARENT_DUTY: {
            mesh_event_ps_duty_t *ps_duty = (mesh_event_ps_duty_t *)event_data;
            ESP_LOGI(MESH_TAG, "<MESH_EVENT_PS_PARENT_DUTY>duty:%d", ps_duty->duty);
        }
        break;
        case MESH_EVENT_PS_CHILD_DUTY: {
            mesh_event_ps_duty_t *ps_duty = (mesh_event_ps_duty_t *)event_data;
            ESP_LOGI(MESH_TAG, "<MESH_EVENT_PS_CHILD_DUTY>cidx:%d, duty:%d", 
                ps_duty->child_connected.aid-1, ps_duty->duty);
            log_mac(ps_duty->child_connected.mac);
        }
        break;
        default:
        ESP_LOGI(MESH_TAG, "unknown id:%" PRId32 "", event_id);
        break;
    }
}

void ip_event_handler(void *arg, esp_event_base_t event_base,
  int32_t event_id, void *event_data) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    ESP_LOGI(MESH_TAG, "<IP_EVENT_STA_GOT_IP>IP:" IPSTR, IP2STR(&event->ip_info.ip));
}
