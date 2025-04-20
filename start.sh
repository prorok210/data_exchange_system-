#!/bin/bash
set -e

# Загружаем переменные окружения ESP-IDF
. $IDF_PATH/export.sh

idf.py fullclean

# Сборка проекта
export TEST_BUILD=1
idf.py build

PROJECT_NAME="data_exchange_system"

# Создаем пустой образ флеш-памяти размером 2MB (поддерживаемый размер QEMU и на нашей плате)
dd if=/dev/zero of=build/flash_image.bin bs=1M count=2

# Записываем бинарные файлы по соответствующим смещениям
dd if=build/bootloader/bootloader.bin of=build/flash_image.bin bs=1 seek=$((0x1000)) conv=notrunc
dd if=build/partition_table/partition-table.bin of=build/flash_image.bin bs=1 seek=$((0x8000)) conv=notrunc
dd if=build/${PROJECT_NAME}.bin of=build/flash_image.bin bs=1 seek=$((0x10000)) conv=notrunc

# Запуск QEMU с корректными параметрами
LOG_FILE="logs/esp32_$(date +%Y%m%d_%H%M%S).log"
qemu-system-xtensa -nographic -machine esp32 \
                   -drive file=build/flash_image.bin,if=mtd,format=raw \
                   -serial mon:stdio 2>&1 | tee "$LOG_FILE"
