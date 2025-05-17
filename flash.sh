#!/bin/bash
if ! docker image inspect esp8266_flasher >/dev/null 2>&1; then
    docker build -t esp8266_flasher .
fi

docker run --rm \
  --device=/dev/ttyUSB0 \
  -e ESPPORT=/dev/ttyUSB0 \
  -v "$(pwd)":/project \
  -w /project \
  esp8266_flasher \
  make flash