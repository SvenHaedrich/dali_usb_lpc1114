import pytest
import logging
import time
from connection.status import DaliStatus

logger = logging.getLogger(__name__)
timeout_time_sec = 2
time_for_command_processing = 0.0005


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
    assert major == 3
    assert minor == 0
    assert bugfix >= 0


@pytest.mark.parametrize(
    "command,expected_result,detailed_code",
    [
        ("Sx\r", DaliStatus.INTERFACE, 0xA3),
        ("Y200\r", DaliStatus.INTERFACE, 0xA3),
        ("S0 10 1000\r", DaliStatus.INTERFACE, 0xA3),
        ("S1 10 A3CD\r", DaliStatus.LOOPBACK, 0x10),
        ("S6 10 1000\r", DaliStatus.INTERFACE, 0xA3),
        ("S7 10 1000\r", DaliStatus.INTERFACE, 0xA3),
        ("S8 10 1000\r", DaliStatus.INTERFACE, 0xA3),
        ("S9 10 1000\r", DaliStatus.INTERFACE, 0xA3),
    ],
)
def test_bad_parameter(dali_serial, command, expected_result, detailed_code):
    dali_serial.start_receive()
    dali_serial.port.write(command.encode("utf-8"))
    dali_serial.get_next(timeout_time_sec)
    assert dali_serial.rx_frame.status.status == expected_result
    assert dali_serial.rx_frame.length == detailed_code


def test_input_queue(dali_serial):
    cmd_one = "S1 10 FF01\r"
    cmd_two = "S1 10 FF02\r"
    dali_serial.start_receive()
    dali_serial.port.write(cmd_one.encode("utf-8"))
    dali_serial.port.write(cmd_two.encode("utf-8"))
    dali_serial.get_next(timeout_time_sec)
    assert dali_serial.rx_frame.status.status == DaliStatus.LOOPBACK
    assert dali_serial.rx_frame.length == 0x10
    assert dali_serial.rx_frame.data == 0xFF01
    dali_serial.get_next(timeout_time_sec)
    assert dali_serial.rx_frame.status.status == DaliStatus.LOOPBACK
    assert dali_serial.rx_frame.length == 0x10
    assert dali_serial.rx_frame.data == 0xFF02


def test_queue_overflow(dali_serial):
    test_cmd = "S1 10 FF0A\r"
    dali_serial.start_receive()
    queue_size = 5
    for i in range(queue_size + 2):
        dali_serial.port.write(test_cmd.encode("utf-8"))
        time.sleep(time_for_command_processing)
    for i in range(queue_size + 2):
        dali_serial.get_next(timeout_time_sec)
        if dali_serial.rx_frame.status.status == DaliStatus.LOOPBACK:
            continue
        break
    assert dali_serial.rx_frame.status.status == DaliStatus.INTERFACE
    # read until no message is left
    for i in range(queue_size + 2):
        dali_serial.get_next(timeout_time_sec)
    assert dali_serial.rx_frame.status.status == DaliStatus.TIMEOUT
