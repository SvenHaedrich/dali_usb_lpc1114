import pytest
import logging
import time

from connection.status import DaliStatus

logger = logging.getLogger(__name__)
timeout_time_sec = 2


def set_up_and_send_sequence(serial, bit_timings):
    short_time = 0.01
    # set up sequence
    first_cmd = True
    for period in bit_timings:
        if first_cmd:
            cmd = f"W{period:x}\r"
            first_cmd = False
        else:
            cmd = f"N{period:x}\r"
        serial.port.write(cmd.encode("utf-8"))
        # time.sleep(short_time)
    # here we go
    serial.port.write("X\r".encode("utf-8"))


def read_result_and_assert(serial, length, data):
    serial.get_next(timeout_time_sec)
    assert serial.rx_frame.status.status == DaliStatus.LOOPBACK
    assert serial.rx_frame.length == length
    assert serial.rx_frame.data == data


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
        (350, DaliStatus.LOOPBACK),
        (400, DaliStatus.LOOPBACK),
        (450, DaliStatus.LOOPBACK),
        (500, DaliStatus.LOOPBACK),
        (750, DaliStatus.TIMING),
        (1000, DaliStatus.TIMING),
        (1500, DaliStatus.TIMING),
        (2000, DaliStatus.TIMING),
        (2500, DaliStatus.TIMING),
        (4000, DaliStatus.TIMING),
        (5000, DaliStatus.TIMING),
        (6000, DaliStatus.TIMING),
        (75000, DaliStatus.TIMING),
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
    cmd = f"W{length_us:x}\r"
    dali_serial.port.write(cmd.encode("utf-8"))
    time.sleep(short_time)
    # here we go
    cmd = "X\r"
    dali_serial.port.write(cmd.encode("utf-8"))
    # read result
    dali_serial.start_receive()
    dali_serial.get_next(timeout_time_sec)
    assert dali_serial.rx_frame.status.status == expected_code
    if expected_code == DaliStatus.LOOPBACK:
        assert (dali_serial.rx_frame.data & 0xFF) == 0
    else:
        assert (dali_serial.rx_frame.data & 0xFF) == 0
        time_us = dali_serial.rx_frame.data >> 8
        assert abs(time_us - length_us) < 12.0


@pytest.mark.parametrize("length_us", [600000, 800000, 1000000])
def test_system_failures(dali_serial, length_us):
    short_time = 0.05
    # set-up sequence
    cmd = f"W{length_us:x}\r"
    dali_serial.port.write(cmd.encode("utf-8"))
    time.sleep(short_time)
    # here we go
    cmd = "X\r"
    dali_serial.port.write(cmd.encode("utf-8"))
    # read failure message
    dali_serial.start_receive()
    dali_serial.get_next(timeout_time_sec)
    assert dali_serial.rx_frame.status.status == DaliStatus.FAILURE
    time_us = dali_serial.rx_frame.data >> 8
    failure = dali_serial.rx_frame.timestamp
    assert time_us == 0
    # read recover message
    sleep_sec = 0.1
    if sleep_sec > 0:
        logger.debug(f"sleep {sleep_sec} sec")
        time.sleep(sleep_sec)
    dali_serial.get_next(timeout_time_sec)
    assert dali_serial.rx_frame.status.status == DaliStatus.OK
    recover = dali_serial.rx_frame.timestamp
    time_us = dali_serial.rx_frame.data >> 8
    assert abs(time_us - length_us) < 12.0
    assert abs((recover - failure) - (length_us / 1e6)) < 0.002
    dali_serial.get_next(timeout_time_sec)


def test_backframe_timing(dali_serial):
    forward_frame = "S1 10 FF00\r"
    backward_frame = "YFF\r"
    dali_serial.start_receive()
    dali_serial.port.write(forward_frame.encode("utf-8"))
    dali_serial.port.write(backward_frame.encode("utf-8"))
    dali_serial.get_next(timeout_time_sec)
    assert dali_serial.rx_frame.status.status == DaliStatus.LOOPBACK
    timestamp_1 = dali_serial.rx_frame.timestamp
    dali_serial.get_next(timeout_time_sec)
    assert dali_serial.rx_frame.status.status == DaliStatus.LOOPBACK
    timestamp_2 = dali_serial.rx_frame.timestamp
    delta = timestamp_2 - timestamp_1
    fullbit_time = 833 / 1000000
    expected_delta = 17 * fullbit_time + (12.4 / 1000)
    logger.debug(f"delta is {delta} expected is {expected_delta}")
    assert delta < expected_delta


def test_kill_sequence(dali_serial):
    dali_serial.start_receive()
    assert dali_serial.rx_frame is None
    # set up the fatal sequence
    length_norm_us = 420
    length_abnorm_us = 1000
    dali_serial.port.write(f"W{length_norm_us:x}\r".encode("utf-8"))
    dali_serial.port.write(f"N{length_norm_us:x}\r".encode("utf-8"))
    dali_serial.port.write(f"N{length_abnorm_us:x}\r".encode("utf-8"))
    dali_serial.port.write(f"N{length_norm_us:x}\r".encode("utf-8"))
    dali_serial.port.write(f"X\r".encode("utf-8"))
    dali_serial.get_next(timeout_time_sec)
    assert dali_serial.rx_frame.status.status == DaliStatus.TIMING
    # check if interface is still alive
    dali_serial.port.write(f"YFF\r".encode("utf-8"))
    dali_serial.get_next(timeout_time_sec)
    assert dali_serial.rx_frame.status.status == DaliStatus.LOOPBACK
    assert dali, serial.rx_frame.data == 0xFF
