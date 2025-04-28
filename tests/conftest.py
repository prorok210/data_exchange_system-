import pytest
import serial
import time
from config import get_device_port, list_available_ports


def pytest_addoption(parser):
    parser.addoption("--run-integration", action="store_true", default=False,
                     help="Запустить интеграционные тесты с несколькими устройствами")
    parser.addoption("--list-ports", action="store_true", default=False,
                     help="Вывести список доступных COM-портов")
    parser.addoption("--esp32-port", action="store", default=None,
                     help="Указать порт для первого устройства ESP32")
    parser.addoption("--esp32-2-port", action="store", default=None,
                     help="Указать порт для второго устройства ESP32")
    parser.addoption("--arduino-port", action="store", default=None,
                     help="Указать порт для Arduino")


def pytest_configure(config):
    """Конфигурация перед запуском тестов"""
    if config.getoption("--list-ports"):
        list_available_ports()
        exit(0)


class DeviceConnection:
    """Класс для управления подключением к устройству"""
    def __init__(self, port, baudrate=115200, timeout=1):
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.serial = None

    def connect(self):
        try:
            self.serial = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=self.timeout
            )
            return True
        except Exception as e:
            print(f"Ошибка подключения к {self.port}: {e}")
            return False

    def disconnect(self):
        if self.serial and self.serial.is_open:
            self.serial.close()

    def send(self, data):
        if not self.serial or not self.serial.is_open:
            raise Exception("Соединение не установлено")

        if isinstance(data, str):
            data = data.encode('utf-8')

        return self.serial.write(data)

    def receive(self, size=1024, timeout=None):
        if not self.serial or not self.serial.is_open:
            raise Exception("Соединение не установлено")

        old_timeout = None
        if timeout is not None:
            old_timeout = self.serial.timeout
            self.serial.timeout = timeout

        try:
            return self.serial.read(size)
        finally:
            if old_timeout is not None:
                self.serial.timeout = old_timeout

    def expect(self, expected_text, timeout=5):
        if isinstance(expected_text, str):
            expected_text = expected_text.encode('utf-8')

        start_time = time.time()
        buffer = b""

        while (time.time() - start_time) < timeout:
            chunk = self.serial.read(100)
            if chunk:
                print(f"Получено: {chunk}")
                buffer += chunk
                if expected_text in buffer:
                    return True
            time.sleep(0.1)

        return False


def get_port_from_options(config, option_name, env_device_name):
    port = config.getoption(option_name)
    if port:
        return port

    return get_device_port(env_device_name)


@pytest.fixture(scope="function")
def esp32_device(request):
    port = get_port_from_options(request.config, "--esp32-port", "ESP32_1")

    if not port:
        pytest.skip("Не указан порт для ESP32")

    device = DeviceConnection(port)

    if not device.connect():
        pytest.skip(f"Не удалось подключиться к устройству ESP32 на порту {port}")

    yield device
    device.disconnect()


@pytest.fixture(scope="function")
def second_esp32_device(request):
    port = get_port_from_options(request.config, "--esp32-2-port", "ESP32_2")

    if not port:
        pytest.skip("Не указан порт для второго ESP32")

    device = DeviceConnection(port)

    if not device.connect():
        pytest.skip(f"Не удалось подключиться ко второму устройству ESP32 на порту {port}")

    yield device
    device.disconnect()


@pytest.fixture(scope="function")
def arduino_device(request):
    port = get_port_from_options(request.config, "--arduino-port", "ARDUINO")

    if not port:
        pytest.skip("Не указан порт для Arduino")

    device = DeviceConnection(port)

    if not device.connect():
        pytest.skip(f"Не удалось подключиться к Arduino на порту {port}")

    yield device
    device.disconnect()


def format_uart_message(data):
    START_MARKER = bytes([0xAA, 0x55])
    END_MARKER = bytes([0x55, 0xAA])

    if isinstance(data, str):
        data = data.encode('utf-8')

    return START_MARKER + data + END_MARKER