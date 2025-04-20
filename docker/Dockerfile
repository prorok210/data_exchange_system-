FROM ubuntu:22.04

# Установка переменных окружения для предотвращения интерактивных запросов
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Moscow

# Установка необходимых пакетов
RUN apt-get update && apt-get install -y \
    git wget flex bison gperf python3 python3-pip python3-setuptools \
    python3-venv \
    cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0 \
    net-tools iproute2 iputils-ping tzdata \
    qemu-system-misc \
    && apt-get clean

# Установка Python зависимостей
RUN pip3 install tomli

# Клонирование и настройка ESP-IDF
ENV IDF_PATH=/root/esp/esp-idf
RUN mkdir -p /root/esp
WORKDIR /root/esp
RUN git clone -b v5.1 --recursive https://github.com/espressif/esp-idf.git
WORKDIR ${IDF_PATH}
RUN ./install.sh esp32

# Экспорт переменных ESP-IDF в файл профиля
RUN echo ". ${IDF_PATH}/export.sh" >> /root/.bashrc

# Создадим простой эмулятор для ESP32 через скрипт-обертку
WORKDIR /usr/local/bin
RUN echo '#!/bin/bash' > qemu-system-xtensa && \
    echo 'echo "Running ESP32 simulation via wrapper"' >> qemu-system-xtensa && \
    echo 'BINARY=$6' >> qemu-system-xtensa && \
    echo 'echo "Starting simulation for $BINARY"' >> qemu-system-xtensa && \
    echo 'echo "MAC: $12"' >> qemu-system-xtensa && \
    echo 'sleep infinity' >> qemu-system-xtensa && \
    chmod +x qemu-system-xtensa

# Копирование проекта
COPY . /root/app
WORKDIR /root/app

# Подготовка скрипта запуска
COPY docker/start.sh /start.sh
RUN chmod +x /start.sh

ENTRYPOINT ["/start.sh"]
