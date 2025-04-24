#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "common.h"
#include "uart_handler.h"
#include "mesh_light.h"

#define UART_TAG "UART_HANDLER"

// Маркеры начала и конца пакета
#define START_MARKER_1 0xAA
#define START_MARKER_2 0x55
#define END_MARKER_1   0x55
#define END_MARKER_2   0xAA

// Состояния конечного автомата для приема данных
typedef enum {
    WAIT_START_1,    // Ожидаем первого маркера начала
    WAIT_START_2,    // Ожидаем второго маркера начала
    RECEIVING_DATA,  // Прием данных
    WAIT_END_1,      // Ожидаем первого маркера конца
    WAIT_END_2       // Ожидаем второго маркера конца
} receive_state_t;

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

// Отправить данные по UART
int uart_send_data(const uint8_t *data, size_t len) {
    if (!data || len == 0) {
        ESP_LOGE(UART_TAG, "Неверные параметры для отправки данных по UART");
        return -1;
    }
    
    // Отправляем маркер начала
    uint8_t start_marker_1 = START_MARKER_1;
    uint8_t start_marker_2 = START_MARKER_2;
    uart_write_bytes(UART_PORT, (const char*)&start_marker_1, 1);
    uart_write_bytes(UART_PORT, (const char*)&start_marker_2, 1);
    
    // Отправляем данные
    int sent = uart_write_bytes(UART_PORT, (const char*)data, len);
    
    if (sent < 0) {
        ESP_LOGE(UART_TAG, "Ошибка при отправке данных по UART");
        return -1;
    }
    
    // Отправляем маркер конца
    uint8_t end_marker_1 = END_MARKER_1;
    uint8_t end_marker_2 = END_MARKER_2;
    uart_write_bytes(UART_PORT, (const char*)&end_marker_1, 1);
    uart_write_bytes(UART_PORT, (const char*)&end_marker_2, 1);
    
    ESP_LOGD(UART_TAG, "Отправлено %d байт по UART", sent);
    return sent;
}

int uart_receive_data(uint8_t *output_buffer, size_t max_len) {
    if (!output_buffer || max_len == 0) {
        ESP_LOGE(UART_TAG, "Неверные параметры для приема данных по UART");
        return -1;
    }
    
    uint8_t temp_buffer[UART_BUF_SIZE];
    int len = uart_read_bytes(UART_PORT, temp_buffer, sizeof(temp_buffer), 0);
    
    if (len <= 0) {
        return 0;
    }
    
    ESP_LOGD(UART_TAG, "Прочитано %d байт из UART", len);
    
    for (int i = 0; i < len; i++) {
        uint8_t byte = temp_buffer[i];
        
        switch (receive_state) {
            case WAIT_START_1:
                if (byte == START_MARKER_1) {
                    receive_state = WAIT_START_2;
                    ESP_LOGD(UART_TAG, "Получен первый маркер начала");
                }
                break;
                
            case WAIT_START_2:
                if (byte == START_MARKER_2) {
                    receive_state = RECEIVING_DATA;
                    data_index = 0;
                    ESP_LOGD(UART_TAG, "Получен второй маркер начала, начинаем прием данных");
                } else {
                    receive_state = WAIT_START_1;
                    ESP_LOGW(UART_TAG, "Ожидался второй маркер начала, получено: 0x%02x", byte);
                }
                break;
                
            case RECEIVING_DATA:
                if (byte == END_MARKER_1) {
                    receive_state = WAIT_END_1;
                    ESP_LOGD(UART_TAG, "Получен первый маркер конца");
                } else {
                    if (data_index < UART_BUF_SIZE) {
                        data_buffer[data_index++] = byte;
                    } else {
                        receive_state = WAIT_START_1;
                        ESP_LOGW(UART_TAG, "Переполнение буфера данных");
                    }
                }
                break;
                
            case WAIT_END_1:
                if (byte == END_MARKER_2) {
                    ESP_LOGD(UART_TAG, "Получен второй маркер конца, пакет завершен (%d байт)", data_index);
                    
                    if (data_index <= max_len) {
                        memcpy(output_buffer, data_buffer, data_index);
                        receive_state = WAIT_START_1;
                        return data_index;
                    } else {
                        ESP_LOGW(UART_TAG, "Выходной буфер слишком мал: %d > %d", data_index, max_len);
                        receive_state = WAIT_START_1;
                        return -1;
                    }
                } else {
                    if (data_index < UART_BUF_SIZE - 1) {
                        data_buffer[data_index++] = END_MARKER_1;
                        data_buffer[data_index++] = byte;
                        receive_state = RECEIVING_DATA;
                    } else {
                        receive_state = WAIT_START_1;
                        ESP_LOGW(UART_TAG, "Переполнение буфера при добавлении маркера");
                    }
                }
                break;
                
            case WAIT_END_2:
                receive_state = WAIT_START_1;
                ESP_LOGW(UART_TAG, "Неожиданное состояние WAIT_END_2");
                break;
        }
    }
    
    return 0;
}