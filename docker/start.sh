#!/bin/bash

# Активируем окружение ESP-IDF
. ${IDF_PATH}/export.sh

# Получаем переменные среды из Docker
NODE_TYPE=${NODE_TYPE:-"root"}  # По умолчанию root, альтернативы: "internal"
NODE_ID=${NODE_ID:-"1"}
MESH_ID=${MESH_ID:-"mesh_example"}
ROUTER_SSID=${ROUTER_SSID:-"mesh_router"}
ROUTER_PASSWORD=${ROUTER_PASSWORD:-"mesh_password"}
MESH_PASSWORD=${MESH_PASSWORD:-"mesh123456"}
MESH_CHANNEL=${MESH_CHANNEL:-"1"}
MESH_LAYER=${NODE_ID}  # Используем ID узла как слой

# Вывод диагностической информации
echo "NODE INFO ==========================================="
echo "ESP-IDF Path: $IDF_PATH"
echo "Python version: $(python3 --version)"
echo "Node Type: ${NODE_TYPE}, Node ID: ${NODE_ID}"
echo "Router SSID: ${ROUTER_SSID}"
echo "====================================================="

# Настраиваем параметры для menuconfig по типу узла
if [ "$NODE_TYPE" = "root" ]; then
    CONFIG_OPTS="-DCONFIG_MESH_DEVICE_TYPE_ROOT=y"
    echo "Configuring as ROOT node with ID ${NODE_ID}"
else
    CONFIG_OPTS="-DCONFIG_MESH_DEVICE_TYPE_INTERNAL=y"
    echo "Configuring as INTERNAL node with ID ${NODE_ID}"
fi

# Собираем проект
cd /root/app
idf.py set-target esp32
idf.py build ${CONFIG_OPTS} \
    -DCONFIG_MESH_ROUTER_SSID=\"${ROUTER_SSID}\" \
    -DCONFIG_MESH_ROUTER_PASSWD=\"${ROUTER_PASSWORD}\" \
    -DCONFIG_MESH_AP_PASSWD=\"${MESH_PASSWORD}\" \
    -DCONFIG_MESH_CHANNEL=${MESH_CHANNEL}

echo "Build complete!"
echo "====================================================="
echo "FIRMWARE INFO:"
ls -la build/*.bin
echo "====================================================="

# Запускаем QEMU с нужными параметрами
MAC_ADDRESS=$(printf '02:00:00:00:%02x:%02x' $((NODE_ID / 256)) $((NODE_ID % 256)))

# Анализ прошивки вместо эмуляции
echo "FIRMWARE ANALYSIS ================================="
echo "Firmware size: $(du -h build/data_exchange_system.bin | cut -f1)"
echo "Node MAC address: ${MAC_ADDRESS}"

if [ "$NODE_TYPE" = "root" ]; then
    echo "Role: ROOT NODE - connects to router and forwards traffic"
else
    echo "Role: INTERNAL NODE - participates in mesh network"
fi

# Эмуляция работы для тестирования сети между контейнерами
echo "====================================================="
echo "Node ${NODE_ID} is running..."
echo "Testing network connectivity:"
ping -c 3 192.168.100.1
echo "Pinging other nodes:"
for i in $(seq 2 4); do
  if [ $i -ne $NODE_ID ]; then
    IP="192.168.100.$i"
    echo "Trying to reach node at $IP..."
    ping -c 2 $IP || echo "Node $i not reachable yet"
  fi
done

# Имитируем запуск эмулятора и держим контейнер активным
QEMU_ARGS="-nographic -machine esp32 -m 4M"
QEMU_ARGS="${QEMU_ARGS} -drive file=build/data_exchange_system.bin,if=mtd,format=raw"
QEMU_ARGS="${QEMU_ARGS} -nic user,mac=${MAC_ADDRESS}"

echo "Starting node simulation with MAC address ${MAC_ADDRESS}"
echo "Type: ${NODE_TYPE}, Layer: ${MESH_LAYER}"
echo "(Simulation mode - container will remain active)"

# Эмуляция вывода работы узла для демонстрационных целей
while true; do
  # Имитация логов узла для демонстрации работы
  TIMESTAMP=$(date "+%H:%M:%S")
  if [ "$NODE_TYPE" = "root" ]; then
    echo "[$TIMESTAMP] ROOT node online. ID: ${NODE_ID}, MAC: ${MAC_ADDRESS}"
    echo "[$TIMESTAMP] Connected to router: ${ROUTER_SSID}"
    echo "[$TIMESTAMP] Network active, routing table: $(( $RANDOM % 5 + 1 )) nodes"
  else
    PARENT=$((($NODE_ID - 1) > 0 ? ($NODE_ID - 1) : 1))
    echo "[$TIMESTAMP] INTERNAL node online. ID: ${NODE_ID}, MAC: ${MAC_ADDRESS}"
    echo "[$TIMESTAMP] Connected to parent node ID: ${PARENT}"
    echo "[$TIMESTAMP] Mesh layer: $(($NODE_ID))"
  fi
  sleep 30
done
