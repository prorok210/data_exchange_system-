#ifndef MAVLINK_HANDLER_H
#define MAVLINK_HANDLER_H

#include <stdint.h>
#include <stddef.h>
#include "common/mavlink.h"

#define MAVLINK_BUFFER_SIZE 280

/**
 * Инициализация системы MAVLink
 * @param system_id ID этой системы
 * @param component_id ID компонента системы
 */
void mavlink_init(uint8_t system_id, uint8_t component_id);

/**
 * Упаковывает сообщение MAVLink в буфер
 * @param msg Сообщение MAVLink
 * @param buffer Буфер для упакованного сообщения
 * @return Длина упакованного сообщения в байтах
 */
uint16_t mavlink_pack_message(mavlink_message_t *msg, uint8_t *buffer);

/**
 * Обрабатывает полученный байт данных MAVLink
 * @param byte Полученный байт
 */
void mavlink_parse_byte(uint8_t byte);

/**
 * Регистрирует обработчик полученных сообщений
 * @param callback Функция обработчика
 */
void mavlink_register_message_handler(void (*callback)(mavlink_message_t *msg));

/**
 * Отправляет heartbeat-сообщение MAVLink
 */
void mavlink_send_heartbeat(void);

/**
 * Создает сообщение с информацией о позиции (GLOBAL_POSITION_INT)
 * @param lat Широта (градусы * 10^7)
 * @param lon Долгота (градусы * 10^7) 
 * @param alt Высота (мм)
 * @param relative_alt Относительная высота (мм)
 * @param vx Скорость по X (см/с)
 * @param vy Скорость по Y (см/с)
 * @param vz Скорость по Z (см/с)
 * @param heading Курс (градусы * 100, от 0 до 35999)
 * @return Сообщение MAVLink
 */
mavlink_message_t mavlink_prepare_position(int32_t lat, int32_t lon, int32_t alt, 
                               int32_t relative_alt, int16_t vx, int16_t vy, int16_t vz,
                               uint16_t heading);

/**
 * Создает сообщение с информацией об ориентации (ATTITUDE)
 * @param roll Крен (рад)
 * @param pitch Тангаж (рад)
 * @param yaw Рысканье (рад)
 * @param rollspeed Скорость крена (рад/с)
 * @param pitchspeed Скорость тангажа (рад/с)
 * @param yawspeed Скорость рысканья (рад/с)
 * @return Сообщение MAVLink
 */
mavlink_message_t mavlink_prepare_attitude(float roll, float pitch, float yaw,
                             float rollspeed, float pitchspeed, float yawspeed);

/**
 * Создает сообщение о состоянии системы питания (BATTERY_STATUS)
 * @param battery_remaining Оставшийся заряд (0-100%)
 * @param voltage_battery Напряжение батареи (мВ) 
 * @param current_battery Ток батареи (10 * мА)
 * @return Сообщение MAVLink
 */
mavlink_message_t mavlink_prepare_battery_status(uint8_t battery_remaining,
                                   uint16_t voltage_battery, int16_t current_battery);

/**
 * Создает сообщение с данными системы (SYS_STATUS)
 * @param onboard_control_sensors_present Датчики, присутствующие в системе
 * @param onboard_control_sensors_enabled Активированные датчики
 * @param onboard_control_sensors_health Здоровье датчиков
 * @param load Загрузка процессора (0.0-1.0) * 1000
 * @param voltage_battery Напряжение батареи (мВ)
 * @param current_battery Ток батареи (10 * мА)
 * @param battery_remaining Оставшийся заряд (0-100%)
 * @param drop_rate_comm Потери пакетов (%)
 * @param errors_comm Ошибки связи
 * @return Сообщение MAVLink
 */
mavlink_message_t mavlink_prepare_sys_status(uint32_t onboard_control_sensors_present,
                               uint32_t onboard_control_sensors_enabled,
                               uint32_t onboard_control_sensors_health,
                               uint16_t load, uint16_t voltage_battery,
                               int16_t current_battery, int8_t battery_remaining,
                               uint16_t drop_rate_comm, uint16_t errors_comm);

/**
 * Анализирует сообщение MAVLink и выводит его содержимое в лог
 * @param msg Сообщение MAVLink для анализа
 */
void mavlink_parse_message(const mavlink_message_t *msg);

/**
 * Вспомогательная функция для безопасного вывода позиционных данных
 * @param lat Широта
 * @param lon Долгота
 * @param alt Абсолютная высота
 * @param relative_alt Относительная высота
 */
void mavlink_log_position_data(int32_t lat, int32_t lon, int32_t alt, int32_t relative_alt);

/**
 * Вспомогательная функция для безопасного вывода данных об ориентации
 * @param roll_deg Крен в градусах
 * @param pitch_deg Тангаж в градусах
 * @param yaw_deg Рысканье в градусах
 */
void mavlink_log_attitude_data(float roll_deg, float pitch_deg, float yaw_deg);

#endif /* MAVLINK_HANDLER_H */
