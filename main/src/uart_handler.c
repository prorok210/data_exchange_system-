#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "uart_handler.h"

#define UART_TAG "UART_HANDLER"



// Буферы для приема данных
static uint8_t data_buffer[UART_BUF_SIZE];
static int data_index = 0;
static receive_state_t receive_state = WAIT_START_1;

int uart_init(int baud_rate) {
    uart_config_t uart_config = {
        .baud_rate = baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    
    ESP_LOGI(UART_TAG, "Инициализация UART для полетного контроллера, baud_rate=%d", baud_rate);
    
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, UART_BUF_SIZE * 2, UART_BUF_SIZE * 2, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    return 0;
}

int uart_send_data(const uint8_t *data, size_t len) {
    if (!data || len == 0) {
        ESP_LOGE(UART_TAG, "Неверные параметры для отправки данных по UART");
        return -1;
    }
    

    int sent = uart_write_bytes(UART_PORT, (const char*)data, len);
    
    if (sent < 0) {
        ESP_LOGE(UART_TAG, "Ошибка при отправке данных по UART");
        return -1;
    }
    
    ESP_LOGD(UART_TAG, "Отправлено %d байт по UART", sent);
    return sent;
}

int uart_receive_data(uint8_t *output_buffer, size_t max_len) {
    if (!output_buffer || max_len == 0) {
        ESP_LOGE(UART_TAG, "Неверные параметры для приема данных по UART");
        return -1;
    }
    
    int len = uart_read_bytes(UART_PORT, output_buffer, max_len, 0);
    
    if (len <= 0) {
        return 0;
    }
    
    ESP_LOGD(UART_TAG, "Прочитано %d байт из UART", len);
    return len;
}