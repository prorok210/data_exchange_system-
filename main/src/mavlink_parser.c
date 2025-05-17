#include <string.h>
#include <inttypes.h>
#include <math.h>
#include "mavlink_handler.h"
#include "esp_log.h"

static const char *TAG = "MAVLINK_PARSER";

void mavlink_log_position_data(int32_t lat, int32_t lon, int32_t alt, int32_t relative_alt) {
    ESP_LOGI(TAG, "Parsed position data: lat=%" PRId32 ", lon=%" PRId32 ", alt=%" PRId32 " mm, rel_alt=%" PRId32 " mm",
             lat, lon, alt, relative_alt);
}

void mavlink_log_attitude_data(float roll_deg, float pitch_deg, float yaw_deg) {
    ESP_LOGI(TAG, "Parsed attitude data: roll=%.2f°, pitch=%.2f°, yaw=%.2f°",
             roll_deg, pitch_deg, yaw_deg);
}

void mavlink_parse_message(const mavlink_message_t *msg) {
    if (!msg) {
        ESP_LOGE(TAG, "Null message pointer");
        return;
    }
    
    switch (msg->msgid) {
        case MAVLINK_MSG_ID_HEARTBEAT: {
            mavlink_heartbeat_t heartbeat;
            mavlink_msg_heartbeat_decode(msg, &heartbeat);
            ESP_LOGI(TAG, "Heartbeat received: type=%u, autopilot=%u, base_mode=%u",
                     heartbeat.type, heartbeat.autopilot, heartbeat.base_mode);
            break;
        }
        
        case MAVLINK_MSG_ID_ATTITUDE: {
            mavlink_attitude_t attitude;
            mavlink_msg_attitude_decode(msg, &attitude);
            float roll_deg = attitude.roll * 180.0f / M_PI;
            float pitch_deg = attitude.pitch * 180.0f / M_PI;
            float yaw_deg = attitude.yaw * 180.0f / M_PI;
            
            mavlink_log_attitude_data(roll_deg, pitch_deg, yaw_deg);
            break;
        }
        
        case MAVLINK_MSG_ID_GLOBAL_POSITION_INT: {
            mavlink_global_position_int_t pos;
            mavlink_msg_global_position_int_decode(msg, &pos);
            
            mavlink_log_position_data(pos.lat, pos.lon, pos.alt, pos.relative_alt);
            
            // Выводим преобразованные значения отдельно
            ESP_LOGI(TAG, "Converted position: lat=%.7f°, lon=%.7f°",
                     pos.lat / 1E7, pos.lon / 1E7);
            break;
        }
        
        default:
            ESP_LOGI(TAG, "Received MAVLink message ID: %u, sequence: %u",
                     msg->msgid, msg->seq);
            break;
    }
}
