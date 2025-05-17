#!/bin/bash

/opt/ESP8266_RTOS_SDK/install.sh

. /opt/ESP8266_RTOS_SDK/export.sh

make menuconfig
make
