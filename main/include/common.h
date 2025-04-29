#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdbool.h>
#include "esp_wifi.h"
#include "esp_log.h"

#define RX_SIZE            (512)
#define TX_SIZE            (512)

extern const char *TAG;
extern const char *MESH_TAG;
extern uint8_t tx_buf[TX_SIZE];
extern uint8_t rx_buf[RX_SIZE];
extern bool is_wifi_connected;
extern bool is_mesh_connected;
extern int mesh_layer;

// UART configurations
#define UART_PORT           UART_NUM_0
#define UART_BAUD_RATE      115200
#define UART_BUF_SIZE       512

#endif /* _COMMON_H_ */