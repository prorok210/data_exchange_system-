FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && apt install -y \
    git make gcc g++ cmake python3 python3-pip \
    libncurses-dev flex bison gperf \
    wget unzip xz-utils curl file

RUN ln -s /usr/bin/python3 /usr/bin/python

RUN git clone --recursive https://github.com/espressif/ESP8266_RTOS_SDK.git /opt/ESP8266_RTOS_SDK

RUN /opt/ESP8266_RTOS_SDK/install.sh && \
    pip3 install -r /opt/ESP8266_RTOS_SDK/requirements.txt

ENV IDF_PATH=/opt/ESP8266_RTOS_SDK
ENV PATH="/root/.espressif/tools/xtensa-lx106-elf/esp-2020r3-*/bin:$PATH"

RUN pip3 install --upgrade pip && \
    pip3 install -r $IDF_PATH/requirements.txt

COPY . /project
WORKDIR /project

RUN bash -c ". /opt/ESP8266_RTOS_SDK/export.sh && make"

ENTRYPOINT ["/bin/bash", "-c", ". /opt/ESP8266_RTOS_SDK/export.sh && exec \"$@\"", "--"]
CMD ["make"]
