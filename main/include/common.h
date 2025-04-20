#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdbool.h>
#include "esp_wifi.h"
#include "esp_mesh.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "mesh_light.h"

#define RX_SIZE            (1500)
#define TX_SIZE            (1460)

extern const char *MESH_TAG;
extern const uint8_t MESH_ID[6];
extern uint8_t tx_buf[TX_SIZE];
extern uint8_t rx_buf[RX_SIZE];
extern bool is_running;
extern bool is_mesh_connected;
extern mesh_addr_t mesh_parent_addr;
extern int mesh_layer;
extern esp_netif_t *netif_sta;


extern mesh_light_ctl_t light_on;
extern mesh_light_ctl_t light_off;


esp_err_t esp_mesh_comm_p2p_start(void);

#endif /* _COMMON_H_ */