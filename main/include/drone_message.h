#ifndef DRONE_MESSAGE_H
#define DRONE_MESSAGE_H

#include <stdint.h>

// Тип сообщения
typedef enum {
    MSG_TYPE_TELEMETRY = 0x01,     // Телеметрия
    MSG_TYPE_COMMAND = 0x02,       // Команда управления
    MSG_TYPE_ACK = 0x03,           // Подтверждение
    MSG_TYPE_ALERT = 0x04          // Аварийное сообщение
} drone_msg_type_t;

// Флаги статуса дрона
typedef enum {
    DRONE_FLAG_ARMED = (1 << 0),           // Дрон активирован
    DRONE_FLAG_FLYING = (1 << 1),          // Дрон в полете
    DRONE_FLAG_GPS_FIX = (1 << 2),         // GPS фиксация получена
    DRONE_FLAG_LOW_BATTERY = (1 << 3),     // Низкий заряд батареи
    DRONE_FLAG_RTH_ACTIVE = (1 << 4),      // Активен режим "Возврат домой"
    DRONE_FLAG_FAILSAFE = (1 << 5),        // Активен режим "Аварийная защита"
    DRONE_FLAG_CALIBRATING = (1 << 6),     // Идет калибровка датчиков
    DRONE_FLAG_ERROR = (1 << 7)            // Наличие ошибки
} drone_status_flags_t;

// Версия формата сообщения для контроля совместимости
#define DRONE_MSG_VERSION 1

// Основная структура сообщения дрона
typedef struct {
    uint8_t msg_type;                  // Тип сообщения
    uint8_t msg_id;                    // ID сообщения для подтверждений
    uint32_t timestamp;                // Временная метка (мс с момента включения)
    
    // Геолокация
    float latitude;                    // Широта (градусы)
    float longitude;                   // Долгота (градусы)
    float altitude;                    // Высота (м)
    float relative_altitude;           // Относительная высота от точки взлета (м)
    
    // Ориентация
    float roll;                        // Крен (градусы)
    float pitch;                       // Тангаж (градусы)
    float yaw;                         // Рысканье (градусы)
    
    // Скорость
    float vx;                          // Скорость по X (м/с)
    float vy;                          // Скорость по Y (м/с)
    float vz;                          // Скорость по Z (м/с)
    
    // Состояние батареи
    uint8_t battery_percentage;        // Процент заряда батареи
    float battery_voltage;             // Напряжение батареи (В)
    float battery_current;             // Ток потребления (А)
    
    // Информация о полете
    uint16_t flight_time;              // Время полета (с)
    uint16_t status_flags;             // Флаги состояния дрона
    
    // Системная информация
    uint8_t cpu_load;                  // Загрузка процессора (%)
    uint8_t rssi;                      // Уровень сигнала (%)
    
    // Дополнительная информация
    uint8_t satellites;                // Количество спутников GPS
    uint8_t fix_type;                  // Тип фиксации GPS (0-нет, 1-2D, 2-3D)
    
    // Маркер версии формата
    uint8_t version;                   // Версия формата сообщения
    uint8_t reserved;                  // Зарезервировано для выравнивания

    uint16_t checksum;                 // Контрольная сумма
} __attribute__((packed)) drone_message_t;  // Для правильной упаковки

// Фактический размер структуры
#define EXPECTED_DRONE_MSG_SIZE 67

// Размер структуры без контрольной суммы
#define DRONE_MSG_PAYLOAD_SIZE (sizeof(drone_message_t) - sizeof(uint16_t))

// Размер пакета с данными
#define DRONE_MSG_PACKET_SIZE (DRONE_MSG_PAYLOAD_SIZE + sizeof(uint16_t))

/**
 * Кодирует структуру сообщения в бинарный формат
 */
int drone_msg_encode(const drone_message_t *msg, uint8_t *buffer, int buf_size);

/**
 * Декодирует бинарные данные в структуру сообщения
 */
int drone_msg_decode(const uint8_t *buffer, int buf_size, drone_message_t *msg);

/**
 * Вычисляет контрольную сумму сообщения
 */
uint16_t drone_msg_calculate_checksum(const uint8_t *buffer, int length);

/**
 * Инициализирует структуру сообщения дрона значениями по умолчанию
 */
void drone_msg_init(drone_message_t *msg);

/**
 * Выводит информацию о сообщении в логи
 */
void drone_msg_log(const drone_message_t *msg);

#endif /* DRONE_MESSAGE_H */
