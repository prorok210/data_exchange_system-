FROM ubuntu:22.04

# Установка переменных окружения для предотвращения интерактивных запросов
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Moscow

# Установка необходимых пакетов
RUN apt-get update && apt-get install -y \
    git wget flex bison gperf python3 python3-pip python3-setuptools python3-venv \
    cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0 net-tools \
    iproute2 iputils-ping tzdata qemu-system-misc glib-2.0 \
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

# Пакеты для qemu
RUN apt-get install -y  build-essential pkg-config libglib2.0-dev libpixman-1-dev \
    libslirp-dev libgcrypt20-dev libnfs-dev libssh-dev

# Установка qemu
RUN git clone --quiet https://github.com/espressif/qemu.git \
		&& cd qemu \
		&& mkdir -p build \
		&& cd build \
		&& ../configure --target-list=xtensa-softmmu --without-default-features --enable-slirp  --enable-gcrypt \
		&& make -j $(nproc) vga=no \
		&& make install


WORKDIR /root/app

COPY . /root/app

COPY start.sh /start.sh
RUN chmod +x /start.sh

# Экспорт переменных ESP-IDF в файл профиля
RUN echo ". ${IDF_PATH}/export.sh" >> /root/.bashrc

ENTRYPOINT ["/start.sh"]
