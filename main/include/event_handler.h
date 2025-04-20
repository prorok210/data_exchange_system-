#ifndef _EVENT_HANDLER_H_
#define _EVENT_HANDLER_H_

#include "esp_event.h"

void mesh_event_handler(void *arg, esp_event_base_t event_base,
    int32_t event_id, void *event_data);
void ip_event_handler(void *arg, esp_event_base_t event_base,
    int32_t event_id, void *event_data);

#endif /* _EVENT_HANDLER_H_ */