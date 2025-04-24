#ifndef UART_HANDLER_H
#define UART_HANDLER_H

#include <stdint.h>
#include <stddef.h>
#include "driver/uart.h"
#include "driver/gpio.h"

#define UART_PORT           UART_NUM_0  // Можно использовать UART_NUM_1 или UART_NUM_2
#define UART_BAUD_RATE      115200
#define UART_BUF_SIZE       1024
#define UART_TX_PIN         GPIO_NUM_1  // Настройте под свою плату
#define UART_RX_PIN         GPIO_NUM_3  // Настройте под свою плату

/**
 * Функция инициализации
 */
int uart_init(int baud_rate);

/**
 * Отправка данных
 */
int uart_send_data(const uint8_t *data, size_t len);

/**
 * Получение данных
 */
int uart_receive_data(uint8_t *data, size_t max_len);

#endif /* UART_HANDLER_H */