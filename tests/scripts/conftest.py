import logging

import pytest
from dali_interface.serial import DaliSerial

logger = logging.getLogger(__name__)


# module scope, not session: test_90_robustness.py reads the port raw, and a
# DaliSerial left open from an earlier file would be a second reader on it
@pytest.fixture(scope="module")
def dali_serial(portname="/dev/ttyUSB0"):
    logger.debug(f"open serial port {portname}")
    dali_serial = DaliSerial(portname=portname)
    yield dali_serial
    logger.debug(f"close serial port {portname}")
    dali_serial.close()
