#include <string.h>
#include <inttypes.h> 
#include <stdbool.h>
#include "drone_message.h"
#include "esp_log.h"

static const char* TAG = "DRONE_MSG";

#define DEBUG_LEVEL 0

// Функция для печати размера структуры и смещений полей (отладка)
void print_struct_info() {
    ESP_LOGI(TAG, "=== ИНФОРМАЦИЯ О СТРУКТУРЕ drone_message_t ===");
    ESP_LOGI(TAG, "Общий размер: %d байт", sizeof(drone_message_t));
    ESP_LOGI(TAG, "Размер без контрольной суммы: %d байт", DRONE_MSG_PAYLOAD_SIZE);
    
    // Проверяем соответствие ожидаемому размеру
    if (sizeof(drone_message_t) != EXPECTED_DRONE_MSG_SIZE) {
        ESP_LOGW(TAG, "Размер структуры не соответствует ожидаемому: %d != %d", 
                 sizeof(drone_message_t), EXPECTED_DRONE_MSG_SIZE);
    } else {
        ESP_LOGI(TAG, "Размер структуры соответствует ожидаемому");
    }
    
    // Напечатаем размер каждого типа для отладки
    ESP_LOGI(TAG, "sizeof(uint8_t)=%d, sizeof(uint16_t)=%d, sizeof(uint32_t)=%d, sizeof(float)=%d", 
             sizeof(uint8_t), sizeof(uint16_t), sizeof(uint32_t), sizeof(float));
    
    // Напечатаем смещения ключевых полей
    ESP_LOGI(TAG, "Смещение поля version: %ld", 
             (long)(&((drone_message_t*)0)->version) - (long)0);
    ESP_LOGI(TAG, "Смещение поля checksum: %ld", 
             (long)(&((drone_message_t*)0)->checksum) - (long)0);
    ESP_LOGI(TAG, "=======================================");
}

// Вычисление простой контрольной суммы
uint16_t drone_msg_calculate_checksum(const uint8_t *buffer, int length) {
    uint16_t checksum = 0;
    for (int i = 0; i < length; i++) {
        checksum += buffer[i];
    }
    return checksum;
}

// Кодирование структуры в бинарный формат
int drone_msg_encode(const drone_message_t *msg, uint8_t *buffer, int buf_size) {
    if (!msg || !buffer || buf_size < DRONE_MSG_PACKET_SIZE) {
        ESP_LOGE(TAG, "Недостаточный размер буфера для кодирования: %d < %d", 
                 buf_size, DRONE_MSG_PACKET_SIZE);
        return -1;
    }
    
    drone_message_t temp_msg = *msg;
    temp_msg.version = DRONE_MSG_VERSION;
    temp_msg.reserved = 0; 
    
    memcpy(buffer, &temp_msg, DRONE_MSG_PAYLOAD_SIZE);
    
    uint16_t checksum = drone_msg_calculate_checksum(buffer, DRONE_MSG_PAYLOAD_SIZE);
    
    buffer[DRONE_MSG_PAYLOAD_SIZE] = (checksum >> 8) & 0xFF;
    buffer[DRONE_MSG_PAYLOAD_SIZE + 1] = checksum & 0xFF;
    
    if (DEBUG_LEVEL > 0) {
        ESP_LOGI(TAG, "Сообщение закодировано: размер=%d (payload=%d + checksum=%d), тип=%d, id=%d", 
                DRONE_MSG_PACKET_SIZE, DRONE_MSG_PAYLOAD_SIZE, sizeof(uint16_t), 
                msg->msg_type, msg->msg_id);
    }
    
    return DRONE_MSG_PACKET_SIZE;
}

// Декодирование данных из бинарного формата в структуру
int drone_msg_decode(const uint8_t *buffer, int buf_size, drone_message_t *msg) {
    static bool info_printed = false;
    
    if (!info_printed) {
        print_struct_info();
        info_printed = true;
    }
    
    ESP_LOGI(TAG, "Декодирование: получено %d байт, требуется %d байт (структура %d байт)", 
             buf_size, DRONE_MSG_PACKET_SIZE, sizeof(drone_message_t));

    const int min_payload_size = 10;
    
    if (!buffer || !msg) {
        ESP_LOGE(TAG, "Неверные параметры: buffer=%p, msg=%p", buffer, msg);
        return -1;
    }
    
    memset(msg, 0, sizeof(drone_message_t));
    
    if (buf_size < DRONE_MSG_PACKET_SIZE) {
        ESP_LOGW(TAG, "Размер буфера меньше ожидаемого: %d < %d, декодирование может быть неполным", 
                 buf_size, DRONE_MSG_PACKET_SIZE);
                 
        if (buf_size < min_payload_size + sizeof(uint16_t)) {
            ESP_LOGE(TAG, "Буфер слишком мал для минимального декодирования: %d < %d", 
                     buf_size, min_payload_size + sizeof(uint16_t));
            return -1;
        }
    }
    
    int payload_size = buf_size - sizeof(uint16_t);
    if (payload_size > DRONE_MSG_PAYLOAD_SIZE) {
        payload_size = DRONE_MSG_PAYLOAD_SIZE;
    }
    
    memcpy(msg, buffer, payload_size);
    
    if (buf_size >= min_payload_size + sizeof(uint16_t)) {
        int checksum_pos = (buf_size < DRONE_MSG_PACKET_SIZE) ? 
            buf_size - sizeof(uint16_t) : DRONE_MSG_PAYLOAD_SIZE;
            
        uint16_t calculated_checksum = drone_msg_calculate_checksum(buffer, checksum_pos);
        uint16_t received_checksum = (buffer[checksum_pos] << 8) | buffer[checksum_pos + 1];
        
        msg->checksum = received_checksum;
        
        if (calculated_checksum != received_checksum) {
            ESP_LOGE(TAG, "Ошибка контрольной суммы: получено 0x%04X, вычислено 0x%04X", 
                     received_checksum, calculated_checksum);
            
            ESP_LOGW(TAG, "Дамп буфера (первые %d байт из %d):", 
                    (buf_size > 16) ? 16 : buf_size, buf_size);
            for (int i = 0; i < buf_size && i < 16; i++) {
                ESP_LOGW(TAG, "  [%02d]: 0x%02X", i, buffer[i]);
            }
        }
    } else {
        ESP_LOGW(TAG, "Буфер не содержит контрольную сумму или слишком мал");
    }
    
    if (msg->version == 0) {
        msg->version = DRONE_MSG_VERSION;
    }
    
    ESP_LOGI(TAG, "Сообщение декодировано (возможно частично): тип=%d, id=%d",
             msg->msg_type, msg->msg_id);
    
    return 0;
}

// Инициализация структуры сообщения значениями по умолчанию
void drone_msg_init(drone_message_t *msg) {
    if (!msg) return;
    
    memset(msg, 0, sizeof(drone_message_t));
    
    msg->msg_type = MSG_TYPE_TELEMETRY;
    msg->battery_percentage = 100;
    msg->battery_voltage = 12.6f;
    msg->rssi = 99;
    msg->version = DRONE_MSG_VERSION;
    msg->reserved = 0;
}

// Функция для вывода информации о сообщении в лог
void drone_msg_log(const drone_message_t *msg) {
    if (!msg) return;
    
    const char* msg_type_str = "НЕИЗВЕСТНО";
    switch(msg->msg_type) {
        case MSG_TYPE_TELEMETRY: msg_type_str = "ТЕЛЕМЕТРИЯ"; break;
        case MSG_TYPE_COMMAND:   msg_type_str = "КОМАНДА"; break;
        case MSG_TYPE_ACK:       msg_type_str = "ПОДТВЕРЖДЕНИЕ"; break;
        case MSG_TYPE_ALERT:     msg_type_str = "ТРЕВОГА"; break;
    }
    
    ESP_LOGI(TAG, "=========== СООБЩЕНИЕ ДРОНА ===========");
    ESP_LOGI(TAG, "Тип: %s (0x%02X), ID: %d", msg_type_str, msg->msg_type, msg->msg_id);
    ESP_LOGI(TAG, "Время: %" PRIu32 " мс", msg->timestamp);
    ESP_LOGI(TAG, "Версия формата: %d", msg->version);
    
    bool valid_coords = (msg->latitude >= -90.0f && msg->latitude <= 90.0f &&
                         msg->longitude >= -180.0f && msg->longitude <= 180.0f &&
                         msg->altitude >= -1000.0f && msg->altitude <= 10000.0f);
                         
    ESP_LOGI(TAG, "----- ПОЗИЦИЯ -----");
    if (valid_coords) {
        ESP_LOGI(TAG, "Координаты: %.6f, %.6f (высота: %.2f м, отн. высота: %.2f м)", 
                msg->latitude, msg->longitude, msg->altitude, msg->relative_altitude);
    } else {
        ESP_LOGW(TAG, "Некорректные координаты: lat=%.6f, lon=%.6f, alt=%.2f (возможно, данные повреждены)",
                msg->latitude, msg->longitude, msg->altitude);
    }
    
    bool valid_attitude = (msg->roll >= -180.0f && msg->roll <= 180.0f &&
                           msg->pitch >= -90.0f && msg->pitch <= 90.0f &&
                           msg->yaw >= -360.0f && msg->yaw <= 360.0f);
                           
    if (valid_attitude) {
        ESP_LOGI(TAG, "Ориентация: крен=%.2f°, тангаж=%.2f°, курс=%.2f°", 
                msg->roll, msg->pitch, msg->yaw);
    } else {
        ESP_LOGW(TAG, "Некорректная ориентация: roll=%.2f, pitch=%.2f, yaw=%.2f (данные могут быть повреждены)",
                msg->roll, msg->pitch, msg->yaw);
    }
    
    bool valid_speed = (msg->vx >= -100.0f && msg->vx <= 100.0f &&
                        msg->vy >= -100.0f && msg->vy <= 100.0f &&
                        msg->vz >= -50.0f && msg->vz <= 50.0f);
                        
    if (valid_speed) {
        ESP_LOGI(TAG, "Скорость: X=%.2f, Y=%.2f, Z=%.2f м/с", 
                msg->vx, msg->vy, msg->vz);
    } else {
        ESP_LOGW(TAG, "Некорректная скорость: vx=%.2f, vy=%.2f, vz=%.2f (данные могут быть повреждены)",
                msg->vx, msg->vy, msg->vz);
    }
    
    ESP_LOGI(TAG, "----- СОСТОЯНИЕ -----");
    if (msg->battery_percentage <= 100) {
        ESP_LOGI(TAG, "Батарея: %d%%, %.2fV, %.2fA", 
                msg->battery_percentage, msg->battery_voltage, msg->battery_current);
    } else {
        ESP_LOGW(TAG, "Некорректный заряд батареи: %d%% (ожидается 0-100)", 
                msg->battery_percentage);
    }
    
    ESP_LOGI(TAG, "Время полета: %" PRIu16 " с", msg->flight_time);
    ESP_LOGI(TAG, "Статус: 0x%04X", msg->status_flags);
    if (msg->status_flags & DRONE_FLAG_ARMED) ESP_LOGI(TAG, " - Активирован");
    if (msg->status_flags & DRONE_FLAG_FLYING) ESP_LOGI(TAG, " - В полете");
    if (msg->status_flags & DRONE_FLAG_GPS_FIX) ESP_LOGI(TAG, " - GPS фиксация");
    if (msg->status_flags & DRONE_FLAG_LOW_BATTERY) ESP_LOGI(TAG, " - Низкий заряд!");
    if (msg->status_flags & DRONE_FLAG_RTH_ACTIVE) ESP_LOGI(TAG, " - Возврат домой");
    if (msg->status_flags & DRONE_FLAG_FAILSAFE) ESP_LOGI(TAG, " - Аварийная защита!");
    if (msg->status_flags & DRONE_FLAG_CALIBRATING) ESP_LOGI(TAG, " - Калибровка");
    if (msg->status_flags & DRONE_FLAG_ERROR) ESP_LOGI(TAG, " - Ошибка!");
    
    ESP_LOGI(TAG, "----- СИСТЕМА -----");
    ESP_LOGI(TAG, "Загрузка процессора: %d%%", msg->cpu_load);
    ESP_LOGI(TAG, "Уровень сигнала: %d%%", msg->rssi);
    ESP_LOGI(TAG, "GPS: %d спутников, тип фиксации: %d", msg->satellites, msg->fix_type);
    ESP_LOGI(TAG, "====================================");
    
    ESP_LOGI(TAG, "Размер структуры: %d байт (ожидаемый: %d + 2)",
             sizeof(drone_message_t), EXPECTED_DRONE_MSG_SIZE);
    ESP_LOGI(TAG, "Размер полезных данных: %d байт", DRONE_MSG_PAYLOAD_SIZE);
}