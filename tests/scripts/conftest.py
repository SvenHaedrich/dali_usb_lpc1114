import pytest
import logging
from connection.serial import DaliSerial

logger = logging.getLogger(__name__)


@pytest.fixture(scope="session")
def dali_serial(portname="/dev/ttyUSB0"):
    logger.debug(f"open serial port {portname}")
    dali_serial = DaliSerial(portname=portname)
    yield dali_serial
    logger.debug(f"close serial port {portname}")
    dali_serial.close()
