import pytest
import logging
import time

from connection.serial import DaliSerial
from connection.status import DaliStatus

logger = logging.getLogger(__name__)


def set_up_and_send_sequence(serial, bit_timings):
    short_time = 0.05
    # set up sequence
    first_cmd = True
    for period in bit_timings:
        if first_cmd:
            cmd = f"D{period:x}\r"
            first_cmd = False
        else:
            cmd = f"N{period:x}\r"
        serial.port.write(cmd.encode("utf-8"))
        time.sleep(short_time)
    # here we go
    serial.port.write("X\r".encode("utf-8"))
    time.sleep(short_time)


def read_result_and_assert(serial, length, data):
    result = serial.get_next(2)
    assert result.status == DaliStatus.FRAME
    assert serial.length == length
    assert serial.data == data


def test_legal_bittiming(dali_serial):
    std_halfbit_period = 417
    bits = [std_halfbit_period] * 17
    dali_serial.start_receive()
    set_up_and_send_sequence(dali_serial, bits)
    read_result_and_assert(dali_serial, 8, 0xFF)


@pytest.mark.parametrize(
    "index,value",
    [
        (1, 0x00),
        (3, 0x80),
        (5, 0xC0),
        (7, 0xE0),
        (9, 0xF0),
        (11, 0xF8),
        (13, 0xFC),
        (15, 0xFE),
    ],
)
def test_more_legal_timings(dali_serial, index, value):
    std_halfbit_period = 417
    std_fullbit_period = 833
    bits = [std_halfbit_period] * 17
    bits[index] = std_fullbit_period
    dali_serial.start_receive()
    set_up_and_send_sequence(dali_serial, bits)
    read_result_and_assert(dali_serial, 8, value)


@pytest.mark.parametrize(
    "length_us,expected_code",
    [
        (200, DaliStatus.TIMING),
        (250, DaliStatus.TIMING),
        (300, DaliStatus.TIMING),
        (350, DaliStatus.FRAME),
        (400, DaliStatus.FRAME),
        (450, DaliStatus.FRAME),
        (500, DaliStatus.FRAME),
        (750, DaliStatus.TIMING),
        (1000, DaliStatus.TIMING),
        (1500, DaliStatus.TIMING),
        (2000, DaliStatus.TIMING),
        (2500, DaliStatus.TIMING),
        (4000, DaliStatus.TIMING),
        (10000, DaliStatus.TIMING),
        (20000, DaliStatus.TIMING),
        (50000, DaliStatus.TIMING),
        (100000, DaliStatus.TIMING),
        (500000, DaliStatus.TIMING),
    ],
)
def test_startbit_lengths(dali_serial, length_us, expected_code):
    short_time = 0.05
    # define sequence
    cmd = f"D{length_us:x}\r"
    dali_serial.port.write(cmd.encode("utf-8"))
    time.sleep(short_time)
    # here we go
    cmd = "X\r"
    dali_serial.port.write(cmd.encode("utf-8"))
    time.sleep(short_time)
    # read result
    dali_serial.start_receive()
    result = dali_serial.get_next(2)
    assert result.status == expected_code
    if expected_code == DaliStatus.FRAME:
        assert (dali_serial.data & 0xFF) == 0
    else:
        assert (dali_serial.data & 0xFF) == 0
        time_us = dali_serial.data >> 8
        assert abs(time_us - length_us) < 10.0


@pytest.mark.parametrize("length_us", [600000, 800000, 1000000])
def test_system_failures(dali_serial, length_us):
    short_time = 0.05
    # set-up sequence
    cmd = f"D{length_us:x}\r"
    dali_serial.port.write(cmd.encode("utf-8"))
    time.sleep(short_time)
    # here we go
    cmd = "X\r"
    dali_serial.port.write(cmd.encode("utf-8"))
    time.sleep(0.40)
    # read failure message
    dali_serial.start_receive()
    result = dali_serial.get_next(5)
    assert result.status == DaliStatus.FAILURE
    time_us = dali_serial.data >> 8
    failure = dali_serial.timestamp
    assert time_us == 0
    # read recover message
    sleep_sec = 0.1
    if sleep_sec > 0:
        logger.debug(f"sleep {sleep_sec} sec")
        time.sleep(sleep_sec)
    result = dali_serial.get_next(5)
    assert result.status == DaliStatus.OK
    recover = dali_serial.timestamp
    time_us = dali_serial.data >> 8
    assert abs(time_us - length_us) < 20.0
    assert abs((recover - failure) - (length_us / 1e6)) < 0.002
