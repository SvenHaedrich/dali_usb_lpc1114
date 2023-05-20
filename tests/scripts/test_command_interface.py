import pytest
import logging
import time

logger = logging.getLogger(__name__)
timeout_time_sec = 2


def test_version(dali_serial):
    command = "?"
    dali_serial.port.write(command.encode("utf-8"))
    timeout = time.time() + timeout_time_sec
    while time.time() < timeout:
        if dali_serial.port.inWaiting() >= 0:
            line = dali_serial.port.readline()
            logger.debug(f"read line: {line}")
            if line.find(b"Version") == 0:
                line.decode("utf-8")
                try:
                    major = line[8] - ord("0")
                    minor = line[10] - ord("0")
                    bugfix = line[12] - ord("0")
                    logger.debug(f"found Version information {major}.{minor}.{bugfix}")
                except ValueError:
                    continue
    assert major == 2
    assert minor == 0
    assert bugfix >= 0


def test_bad_parameter(dali_serial):
    test = "Sx\r"
    logger.debug(f"test command: {test}")
    dali_serial.port.write(test.encode("utf-8"))
    timeout = time.time() + timeout_time_sec
    while time.time() < timeout:
        if dali_serial.port.inWaiting() >= 0:
            reply = dali_serial.port.readline()
            logger.debug(f"read line: {reply}")
            start = reply.find(ord("{")) + 1
            end = reply.find(ord("}"))
            assert start > 0
            assert end > 0
            payload = reply[start:end]
            type_code = chr(payload[8])
            status = int(payload[9:11], 16)
            data = int(payload[12:20], 16)
            assert type_code == ":"
            assert status == 0xa3
            assert data == 0
        break
    if time.time() >= timeout:
        assert False
