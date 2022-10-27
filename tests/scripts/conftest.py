import pytest
import serial
import logging

logger = logging.getLogger(__name__)

@pytest.fixture(scope="session")
def serial_conn(port="/dev/ttyUSB3", baudrate=115200):
    logger.debug(F"open serial port {port}")
    serial_port = serial.Serial(port=port,baudrate=baudrate, timeout=0.5)
    return serial_port
