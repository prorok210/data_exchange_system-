#ifndef _MSG_HANDLER_H_
#define _MSG_HANDLER_H_

#include "esp_err.h"

void esp_mesh_p2p_tx_main(void *arg);

void esp_mesh_p2p_rx_main(void *arg);

#endif /* _MSG_HANDLER_H_ */