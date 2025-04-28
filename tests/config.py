import os
import sys
import glob
import serial.tools.list_ports

DEFAULT_CONFIGS = {
    "windows": {
        "ESP32_1": "COM3",
        "ESP32_2": "COM4",
        "ARDUINO": "COM5"
    },
    "linux": {
        "ESP32_1": "/dev/ttyUSB0",
        "ESP32_2": "/dev/ttyUSB1",
        "ARDUINO": "/dev/ttyACM0"
    },
    "darwin": {  # macOS
        "ESP32_1": "/dev/tty.SLAB_USBtoUART",
        "ESP32_2": "/dev/tty.SLAB_USBtoUART2",
        "ARDUINO": "/dev/tty.usbmodem*"
    }
}


def get_platform():
    if sys.platform.startswith('win'):
        return "windows"
    elif sys.platform.startswith('linux'):
        return "linux"
    elif sys.platform.startswith('darwin'):
        return "darwin"
    else:
        return "unknown"


def list_available_ports():
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("Не найдено доступных портов!")
        return []

    print("Доступные COM-порты:")
    result = []
    for port, desc, hwid in sorted(ports):
        print(f"{port}: {desc} [{hwid}]")
        result.append(port)
    return result


def get_device_port(device_name):
    """
    Args:
        device_name: Имя устройства (ESP32_1, ESP32_2, ARDUINO)
    Returns:
        Строка с именем порта
    """
    env_var = f"{device_name}_PORT"
    if env_var in os.environ:
        return os.environ[env_var]

    platform = get_platform()
    if platform in DEFAULT_CONFIGS and device_name in DEFAULT_CONFIGS[platform]:
        port_pattern = DEFAULT_CONFIGS[platform][device_name]

        if '*' in port_pattern:
            matching_ports = glob.glob(port_pattern)
            if matching_ports:
                return matching_ports[0]

        return port_pattern

    return None


available_ports = list_available_ports()
