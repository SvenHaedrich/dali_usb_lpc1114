import pytest
import logging
import time

logger = logging.getLogger(__name__)
timeout_time_sec = 2


def test_version(serial_conn):
    command = "?"
    serial_conn.write(command.encode("utf-8"))
    timeout = time.time() + timeout_time_sec
    while time.time() < timeout:
        if serial_conn.inWaiting() >= 0:
            line = serial_conn.readline()
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
    assert major == 1
    assert minor == 5
    assert bugfix >= 4


def test_buffer_overflow(serial_conn):
    test = "1" * 30
    test = test + "!"
    logger.debug(f"test command: {test}")
    serial_conn.write(test.encode("utf-8"))
    timeout = time.time() + timeout_time_sec
    while time.time() < timeout:
        if serial_conn.inWaiting() >= 0:
            reply = serial_conn.readline()
            logger.debug(f"read line: {reply}")
            start = reply.find(ord("{")) + 1
            end = reply.find(ord("}"))
            assert start > 0
            assert end > 0
            payload = reply[start:end]
            type = chr(payload[8])
            status = int(payload[9:11], 16)
            data = int(payload[12:20], 16)
            assert type == "*"
            assert status == 0
            assert data == 0
        break
    if time.time() >= timeout:
        assert False
