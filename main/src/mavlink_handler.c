#include <string.h>
#include <math.h>
#include <inttypes.h>
#include "mavlink_handler.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "MAVLINK";

static uint8_t system_id;
static uint8_t component_id;

static mavlink_message_t received_msg;
static mavlink_status_t status;

static void (*message_handler)(mavlink_message_t *msg) = NULL;

void mavlink_init(uint8_t sys_id, uint8_t comp_id) {
    system_id = sys_id;
    component_id = comp_id;
    ESP_LOGI(TAG, "MAVLink initialized. System ID: %d, Component ID: %d", system_id, component_id);
}

uint16_t mavlink_pack_message(mavlink_message_t *msg, uint8_t *buffer) {
    if (!msg || !buffer) {
        ESP_LOGE(TAG, "Invalid parameters for packing MAVLink message");
        return 0;
    }
    
    return mavlink_msg_to_send_buffer(buffer, msg);
}

void mavlink_parse_byte(uint8_t byte) {
    if (mavlink_parse_char(MAVLINK_COMM_0, byte, &received_msg, &status)) {
        ESP_LOGD(TAG, "Received MAVLink message with ID: %d, sequence: %d", 
                received_msg.msgid, received_msg.seq);
                
        if (message_handler) {
            message_handler(&received_msg);
        }
    }
}

void mavlink_register_message_handler(void (*callback)(mavlink_message_t *msg)) {
    message_handler = callback;
    ESP_LOGI(TAG, "MAVLink message handler registered");
}

void mavlink_send_heartbeat(void) {
    mavlink_message_t msg;
    
    uint8_t type = MAV_TYPE_QUADROTOR;
    uint8_t autopilot = MAV_AUTOPILOT_GENERIC;
    uint8_t base_mode = MAV_MODE_FLAG_SAFETY_ARMED | MAV_MODE_FLAG_MANUAL_INPUT_ENABLED;
    uint32_t custom_mode = 0;
    uint8_t system_status = MAV_STATE_ACTIVE;

    mavlink_msg_heartbeat_pack(
        system_id,
        component_id,
        &msg,
        type,
        autopilot,
        base_mode,
        custom_mode,
        system_status
    );
    
    ESP_LOGD(TAG, "Sending MAVLink heartbeat");
}

mavlink_message_t mavlink_prepare_position(int32_t lat, int32_t lon, int32_t alt, 
                               int32_t relative_alt, int16_t vx, int16_t vy, int16_t vz,
                               uint16_t heading) {
    mavlink_message_t msg;
    uint32_t time_boot_ms = (uint32_t)(esp_timer_get_time() / 1000);
    
    mavlink_msg_global_position_int_pack(
        system_id,
        component_id,
        &msg,
        time_boot_ms,
        lat,
        lon,
        alt,
        relative_alt,
        vx,
        vy,
        vz,
        heading
    );
    
    return msg;
}

mavlink_message_t mavlink_prepare_attitude(float roll, float pitch, float yaw,
                             float rollspeed, float pitchspeed, float yawspeed) {
    mavlink_message_t msg;
    uint32_t time_boot_ms = (uint32_t)(esp_timer_get_time() / 1000);
    
    mavlink_msg_attitude_pack(
        system_id,
        component_id,
        &msg,
        time_boot_ms,
        roll,
        pitch,
        yaw,
        rollspeed,
        pitchspeed,
        yawspeed
    );
    
    return msg;
}

mavlink_message_t mavlink_prepare_battery_status(uint8_t battery_remaining,
                                   uint16_t voltage_battery, int16_t current_battery) {
    mavlink_message_t msg;
    
    uint16_t voltages[10] = {0};
    voltages[0] = voltage_battery;
    uint16_t voltages_ext[4] = {0};
    uint8_t id = 0;
    uint8_t battery_function = MAV_BATTERY_FUNCTION_UNKNOWN;
    uint8_t type = MAV_BATTERY_TYPE_UNKNOWN;
    int16_t temperature = INT16_MAX;  // Неизвестно
    int32_t current_consumed = -1;    // Неизвестно
    int32_t energy_consumed = -1;     // Неизвестно
    int32_t time_remaining = -1;      // Неизвестно
    uint8_t charge_state = MAV_BATTERY_CHARGE_STATE_UNDEFINED;
    uint8_t mode = 0;                 // Неизвестно
    uint32_t fault_bitmask = 0;       // Неизвестно
    
    mavlink_msg_battery_status_pack(
        system_id,
        component_id,
        &msg,
        id,
        battery_function,
        type,
        temperature,
        voltages,               
        current_battery,
        current_consumed,
        energy_consumed,
        battery_remaining,
        time_remaining,
        charge_state,
        voltages_ext,        
        mode,
        fault_bitmask
    );
    
    return msg;
}

mavlink_message_t mavlink_prepare_sys_status(uint32_t onboard_control_sensors_present,
                               uint32_t onboard_control_sensors_enabled,
                               uint32_t onboard_control_sensors_health,
                               uint16_t load, uint16_t voltage_battery,
                               int16_t current_battery, int8_t battery_remaining,
                               uint16_t drop_rate_comm, uint16_t errors_comm) {
    mavlink_message_t msg;
    
    mavlink_msg_sys_status_pack(
        system_id,
        component_id,
        &msg,
        onboard_control_sensors_present,
        onboard_control_sensors_enabled,
        onboard_control_sensors_health,
        load,
        voltage_battery,
        current_battery,
        battery_remaining,
        drop_rate_comm,
        errors_comm,
        0, 0, 0, 0, 0, 0, 0
    );
    
    return msg;
}
