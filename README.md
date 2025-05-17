# Система обмена данными на базе ESP32 с поддержкой MAVLink и ESP-NOW

## Описание проекта
Этот проект реализует систему обмена данными между устройствами ESP32 с использованием протокола MAVLink и технологии ESP-NOW. Система позволяет организовывать прямое соединение между устройствами без необходимости Wi-Fi роутера, а также обеспечивает связь с полетными контроллерами через UART интерфейс с использованием стандарта MAVLink.

## Возможности
- Передача телеметрии и команд управления по протоколу MAVLink
- Организация беспроводной сети ESP-NOW для прямой связи между устройствами
- Прием/передача MAVLink-сообщений через UART интерфейс
- Маршрутизация сообщений между UART и ESP-NOW
- Периодическая отправка служебных сообщений (heartbeat)

## Требования для сборки
- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/) v5.1 или выше (для локальной сборки)
- Библиотека MAVLink v2
- CMake 3.16 или выше
- Python 3.7 или выше

## Сборка и запуск

### Локальная сборка
1. Настройте окружение ESP-IDF:
   ```bash
   . $IDF_PATH/export.sh
   ```

2. Загрузите библиотеку MAVLink в компоненты проекта:
   ```bash
   mkdir -p components/mavlink/include
   git clone --recursive https://github.com/mavlink/c_library_v2.git components/mavlink/include/mavlink
   ```

3. Соберите проект:
   ```bash
   idf.py build
   ```

4. Прошивка на устройство:
   ```bash
   idf.py -p [PORT] flash
   ```
   
5. Просмотр логов 
    ```bash
   idf.py -p [PORT] monitor
   ```

### Тестовая прошивка с использованием Docker и QEMU (позволяет проверить только возможность прошить устройство, т.к нет поддержки wi-fi)
1. Соберите Docker-образ:
   ```bash
   docker build -t test_build .
   ```

2. Запустите контейнер:
   ```bash
   docker run --rm test_build
   ```

## Настройка ESP-NOW и MAVLink
Для настройки соединения ESP-NOW используются следующие параметры:
- MAC-адрес узла назначения определяется в `main.c` через константу `PEER_MAC`
- Идентификаторы системы MAVLink настраиваются через константы `MAVLINK_SYSTEM_ID` и `MAVLINK_COMPONENT_ID`

Для изменения параметров UART используйте файл `include/uart_handler.h`:
- `UART_PORT`: номер UART (UART_NUM_0, UART_NUM_1, UART_NUM_2)
- `UART_BAUD_RATE`: скорость передачи (по умолчанию 115200)
- `UART_TX_PIN`, `UART_RX_PIN`: номера GPIO для TX и RX

## Поддерживаемые сообщения MAVLink

Система поддерживает следующие типы сообщений MAVLink:
- `HEARTBEAT` - для поддержания соединения
- `ATTITUDE` - информация об ориентации в пространстве
- `GLOBAL_POSITION_INT` - геопространственные данные
- `BATTERY_STATUS` - информация о батарее
- `SYS_STATUS` - статус системы

Дополнительные сообщения MAVLink могут быть добавлены с использованием примеров в файле `mavlink_handler.c`.
