import pytest
import serial
import logging

logger = logging.getLogger(__name__)


@pytest.fixture(scope="session")
def serial_conn(port="/dev/ttyUSB0", baudrate=115200):
    logger.debug(f"open serial port {port}")
    serial_port = serial.Serial(port=port, baudrate=baudrate, timeout=2)
    return serial_port
