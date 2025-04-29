#ifndef __MESH_LIGHT_H__
#define __MESH_LIGHT_H__

#include "esp_err.h"
#include "esp_system.h"

#ifdef __cplusplus
extern "C" {
#endif

// Определения для совместимости с ESP32
#define MESH_TOKEN_ID           0x4D   /* 'M' */
#define MESH_TOKEN_VALUE        0x65   /* 'e' */

#define MESH_LIGHT_RED          1
#define MESH_LIGHT_GREEN        2
#define MESH_LIGHT_BLUE         3
#define MESH_LIGHT_YELLOW       4
#define MESH_LIGHT_PINK         5
#define MESH_LIGHT_INIT         6
#define MESH_LIGHT_WARNING      7

#define MESH_CONTROL_CMD        0x01
#define MESH_CONTROL_CMD_ON     0x01
#define MESH_CONTROL_CMD_OFF    0x00

#define MESH_LEVEL_ROOT    1
#define MESH_LEVEL_CHILD   2

// Добавляем CONFIG_MESH_AP_PASSWD для совместимости
#define CONFIG_MESH_AP_PASSWD "password123"

typedef struct mesh_light_ctl {
    uint8_t token_id;
    uint8_t token_value;
    uint8_t cmd;
    uint8_t on;
} __attribute__((packed)) mesh_light_ctl_t;

// Функции индикации состояния
void mesh_connected_indicator(int layer);
void mesh_disconnected_indicator(void);

#ifdef __cplusplus
}
#endif

#endif