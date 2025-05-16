#pragma once

#include <stdint.h>
#include <stddef.h>

#define ESPNOW_MAX_DATA_LEN 250

void espnow_init(void);
void espnow_add_peer(const uint8_t *peer_mac);
void espnow_send(const uint8_t *peer_mac, const uint8_t *data, size_t len);