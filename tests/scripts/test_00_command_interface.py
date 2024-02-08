import pytest
import logging
import time
from dali_interface.dali_interface import DaliStatus
from dali_interface.serial import DaliSerial

logger = logging.getLogger(__name__)
timeout_time_sec = 2
time_for_command_processing = 0.0005


def test_version():
    serial = DaliSerial("/dev/ttyUSB0", start_receive=False)
    command = "?"
    serial.port.write(command.encode("utf-8"))
    timeout = time.time() + timeout_time_sec
    while time.time() < timeout:
        if serial.port.inWaiting() >= 0:
            line = serial.port.readline()
            logger.debug(f"read line: {line}")
            if line.find(b"Version") == 0:
                line.decode("utf-8")
                try:
                    major = line[8] - ord("0")
                    minor = line[10] - ord("0")
                    bugfix = line[12] - ord("0")
                    logger.debug(f"found Version information {major}.{minor}.{bugfix}")
                    break
                except ValueError:
                    continue
    assert major == 3
    assert minor == 3
    assert bugfix >= 0
    serial.close()


@pytest.mark.parametrize(
    "command,expected_result,detailed_code",
    [
        ("Sx\r", DaliStatus.INTERFACE, 0xA3),
        ("Y200\r", DaliStatus.INTERFACE, 0xA3),
        ("S0 10 1000\r", DaliStatus.INTERFACE, 0xA3),
        ("S1 10 A3CD\r", DaliStatus.LOOPBACK, 0x10),
        ("S7 10 1000\r", DaliStatus.INTERFACE, 0xA3),
        ("S8 10 1000\r", DaliStatus.INTERFACE, 0xA3),
        ("S9 10 1000\r", DaliStatus.INTERFACE, 0xA3),
        ("S2 10 11111\r", DaliStatus.INTERFACE, 0xA3),
    ],
)
def test_bad_parameter(dali_serial, command, expected_result, detailed_code):
    dali_serial.port.write(command.encode("utf-8"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == expected_result
    assert result.length == detailed_code


def test_input_queue(dali_serial):
    cmd_one = "S1 10 FF01\r"
    cmd_two = "S1 10 FF02\r"
    dali_serial.port.write(cmd_one.encode("utf-8"))
    time.sleep(time_for_command_processing)
    dali_serial.port.write(cmd_two.encode("utf-8"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.LOOPBACK
    assert result.length == 0x10
    assert result.data == 0xFF01
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.LOOPBACK
    assert result.length == 0x10
    assert result.data == 0xFF02


def test_queue_overflow(dali_serial):
    time.sleep(timeout_time_sec)
    test_cmd = "S1 10 FF0A\r"
    queue_size = 5
    overfill = 5
    for i in range(queue_size + overfill):
        dali_serial.port.write(test_cmd.encode("utf-8"))
        time.sleep(time_for_command_processing)
    for i in range(queue_size + overfill):
        result = dali_serial.get(timeout_time_sec)
        if result.status == DaliStatus.LOOPBACK:
            continue
        break
    assert result.status == DaliStatus.INTERFACE
    # read until no message is left
    for i in range(queue_size + overfill):
        result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.TIMEOUT
