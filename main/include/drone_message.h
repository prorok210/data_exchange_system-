#ifndef DRONE_MESSAGE_H
#define DRONE_MESSAGE_H

#include <stdint.h>
#include "mavlink/common/mavlink.h"

// Функции для работы с сообщениями
int drone_msg_encode(const mavlink_message_t *msg, uint8_t *buffer, int buf_size);
int drone_msg_decode(const uint8_t *buffer, int buf_size, mavlink_message_t *msg, mavlink_status_t *status);
void drone_msg_init(mavlink_message_t *msg, mavlink_status_t *status);
void drone_msg_log(const mavlink_message_t *msg);

#endif /* DRONE_MESSAGE_H */