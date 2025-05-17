#include <string.h>
#include <inttypes.h> 
#include <stdbool.h>
#include "drone_message.h"
#include "esp_log.h"
#include "mavlink/common/mavlink.h"

static const char* TAG = "DRONE_MSG";

// Кодирование сообщения MAVLink в бинарный формат
int drone_msg_encode(const mavlink_message_t *msg, uint8_t *buffer, int buf_size) {
    if (!msg || !buffer || buf_size < MAVLINK_MAX_PACKET_LEN) {
        ESP_LOGE(TAG, "Недостаточный размер буфера для кодирования");
        return -1;
    }
    
    uint16_t len = mavlink_msg_to_send_buffer(buffer, msg);
    return len;
}

// Декодирование бинарных данных в сообщение MAVLink
int drone_msg_decode(const uint8_t *buffer, int buf_size, mavlink_message_t *msg, mavlink_status_t *status) {
    if (!buffer || !msg || !status || buf_size <= 0) {
        ESP_LOGE(TAG, "Неверные параметры для декодирования");
        return -1;
    }
    
    for (int i = 0; i < buf_size; i++) {
        if (mavlink_parse_char(MAVLINK_COMM_0, buffer[i], msg, status)) {
            // Полное сообщение успешно декодировано
            return i + 1; // Возвращаем количество обработанных байт
        }
    }
    
    return 0; // Сообщение не полностью получено
}

// Инициализация сообщения MAVLink
void drone_msg_init(mavlink_message_t *msg, mavlink_status_t *status) {
    if (!msg || !status) return;
    
    memset(msg, 0, sizeof(mavlink_message_t));
    memset(status, 0, sizeof(mavlink_status_t));
}

// Вывод информации о сообщении в лог
void drone_msg_log(const mavlink_message_t *msg) {
    if (!msg) return;
    
    ESP_LOGI(TAG, "MAVLINK СООБЩЕНИЕ");
    ESP_LOGI(TAG, "Система ID: %d, Компонент ID: %d", msg->sysid, msg->compid);
    ESP_LOGI(TAG, "ID сообщения: %d, Длина: %d", msg->msgid, msg->len);
    
    // Обработка разных типов сообщений
    switch(msg->msgid) {
        case MAVLINK_MSG_ID_HEARTBEAT: {
            mavlink_heartbeat_t heartbeat;
            mavlink_msg_heartbeat_decode(msg, &heartbeat);
            
            ESP_LOGI(TAG, "--- HEARTBEAT ---");
            ESP_LOGI(TAG, "Тип: %d, Автопилот: %d, Режим: %d", 
                    heartbeat.type, heartbeat.autopilot, heartbeat.base_mode);
            ESP_LOGI(TAG, "Статус: %d, Версия: %d", 
                    heartbeat.system_status, heartbeat.mavlink_version);
            break;
        }
        
        case MAVLINK_MSG_ID_GLOBAL_POSITION_INT: {
            mavlink_global_position_int_t gpos;
            mavlink_msg_global_position_int_decode(msg, &gpos);
            
            ESP_LOGI(TAG, "--- GLOBAL POSITION ---");
            ESP_LOGI(TAG, "Координаты: %d, %d (высота: %d мм, отн. высота: %d мм)", 
                    gpos.lat, gpos.lon, gpos.alt, gpos.relative_alt);
            ESP_LOGI(TAG, "Скорость: X=%d, Y=%d, Z=%d см/с", 
                    gpos.vx, gpos.vy, gpos.vz);
            ESP_LOGI(TAG, "Курс: %d (1/100 градусов)", gpos.hdg);
            break;
        }
        
        case MAVLINK_MSG_ID_ATTITUDE: {
            mavlink_attitude_t att;
            mavlink_msg_attitude_decode(msg, &att);
            
            ESP_LOGI(TAG, "--- ATTITUDE ---");
            ESP_LOGI(TAG, "Ориентация: крен=%.4f, тангаж=%.4f, курс=%.4f рад", 
                    att.roll, att.pitch, att.yaw);
            ESP_LOGI(TAG, "Скорость вращения: roll=%.4f, pitch=%.4f, yaw=%.4f рад/с", 
                    att.rollspeed, att.pitchspeed, att.yawspeed);
            break;
        }
        
        case MAVLINK_MSG_ID_SYS_STATUS: {
            mavlink_sys_status_t sys;
            mavlink_msg_sys_status_decode(msg, &sys);
            
            ESP_LOGI(TAG, "--- SYSTEM STATUS ---");
            ESP_LOGI(TAG, "Напряжение: %d mV, Ток: %d mA", sys.voltage_battery, sys.current_battery);
            ESP_LOGI(TAG, "Заряд батареи: %d%%, Загрузка: %d%%", 
                    sys.battery_remaining, sys.load);
            break;
        }
        
        default:
            ESP_LOGI(TAG, "Неизвестный тип сообщения: %d", msg->msgid);
            break;
    }
}