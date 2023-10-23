import pytest
import logging
import time

from dali_interface.dali_interface import DaliStatus

logger = logging.getLogger(__name__)
timeout_time_sec = 10
time_for_command_processing = 0.0005
time_loopback_tolerance_us = 5


def set_up_and_send_sequence(serial, bit_timings):
    # set up sequence
    first_cmd = True
    for period in bit_timings:
        if first_cmd:
            cmd = f"W{period:x}\r"
            first_cmd = False
        else:
            cmd = f"N{period:x}\r"
        serial.port.write(cmd.encode("utf-8"))
        time.sleep(time_for_command_processing)
    # here we go
    serial.port.write("X\r".encode("utf-8"))


def read_result_and_assert(serial, length, data):
    result = serial.get(timeout_time_sec)
    assert result.status == DaliStatus.LOOPBACK
    assert result.length == length
    assert result.data == data


def test_legal_bittiming(dali_serial):
    std_halfbit_period = 417
    bits = [std_halfbit_period] * 17
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
    result = dali_serial.get(timeout_time_sec)
    assert result.status == expected_code
    if expected_code == DaliStatus.LOOPBACK:
        assert (result.data & 0xFF) == 0
    else:
        assert (result.data & 0xFF) == 0
        time_us = result.data >> 8
        assert abs(time_us - length_us) < time_loopback_tolerance_us
    time.sleep(0.100)


@pytest.mark.parametrize("length_us", [600000, 800000, 1000000])
def test_system_failures(dali_serial, length_us):
    # set-up sequence
    cmd = f"W{length_us:x}\r"
    dali_serial.port.write(cmd.encode("utf-8"))
    time.sleep(time_for_command_processing)
    # here we go
    cmd = "X\r"
    dali_serial.port.write(cmd.encode("utf-8"))
    # read failure message
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.FAILURE
    time_us = result.data >> 8
    failure = result.timestamp
    assert time_us == 0
    # read recover message
    sleep_sec = 0.1
    if sleep_sec > 0:
        logger.debug(f"sleep {sleep_sec} sec")
        time.sleep(sleep_sec)
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.RECOVER
    recover = result.timestamp
    time_us = result.data >> 8
    assert abs(time_us - length_us) < time_loopback_tolerance_us
    assert abs((recover - failure) - (length_us / 1e6)) < 0.002
    dali_serial.get(timeout_time_sec)


def test_backframe_timing(dali_serial):
    forward_frame = "S1 10 FF00\r"
    backward_frame = "YFF\r"
    dali_serial.port.write(forward_frame.encode("utf-8"))
    time.sleep(time_for_command_processing)
    dali_serial.port.write(backward_frame.encode("utf-8"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.LOOPBACK
    timestamp_1 = result.timestamp
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.LOOPBACK
    timestamp_2 = result.timestamp
    delta = timestamp_2 - timestamp_1
    fullbit_time = 833 / 1000000
    expected_delta = 17 * fullbit_time + (12.4 / 1000)
    logger.debug(f"delta is {delta} expected is {expected_delta}")
    assert delta < expected_delta


def test_kill_sequence(dali_serial):
    # set up the fatal sequence
    length_norm_us = 420
    length_abnorm_us = 1000
    dali_serial.port.write(f"W{length_norm_us:x}\r".encode("utf-8"))
    time.sleep(time_for_command_processing)
    dali_serial.port.write(f"N{length_norm_us:x}\r".encode("utf-8"))
    time.sleep(time_for_command_processing)
    dali_serial.port.write(f"N{length_abnorm_us:x}\r".encode("utf-8"))
    time.sleep(time_for_command_processing)
    dali_serial.port.write(f"N{length_norm_us:x}\r".encode("utf-8"))
    time.sleep(time_for_command_processing)
    dali_serial.port.write("X\r".encode("utf-8"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.TIMING
    time.sleep(0.05)
    # check if interface is still alive
    dali_serial.port.write("YFF\r".encode("utf-8"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.LOOPBACK
    assert result.data == 0xFF


def test_3_11_receiver_bit_timing(dali_serial):
    length_norm_single_us = 416
    length_norm_double_us = 833
    length_table = [334, 375, 416, 458, 500]
    for m in range(5):
        low_time = length_table[m]
        for n in range(5):
            high_time = length_table[n]
            dali_serial.port.write(f"W{length_norm_single_us:x}\r".encode("utf-8"))
            time.sleep(time_for_command_processing)
            dali_serial.port.write(f"N{length_norm_single_us:x}\r".encode("utf-8"))
            time.sleep(time_for_command_processing)
            for _ in range(3):
                dali_serial.port.write(f"N{low_time:x}\r".encode("utf-8"))
                time.sleep(time_for_command_processing)
                dali_serial.port.write(f"N{high_time:x}\r".encode("utf-8"))
                time.sleep(time_for_command_processing)
            dali_serial.port.write(f"N{low_time:x}\r".encode("utf-8"))
            time.sleep(time_for_command_processing)
            dali_serial.port.write(f"N{2*high_time:x}\r".encode("utf-8"))
            time.sleep(time_for_command_processing)
            for _ in range(3):
                dali_serial.port.write(f"N{low_time:x}\r".encode("utf-8"))
                time.sleep(time_for_command_processing)
                dali_serial.port.write(f"N{high_time:x}\r".encode("utf-8"))
                time.sleep(time_for_command_processing)
            dali_serial.port.write(f"N{low_time:x}\r".encode("utf-8"))
            time.sleep(time_for_command_processing)
            dali_serial.port.write("X\r".encode("utf-8"))
            result = dali_serial.get(timeout_time_sec)
            logger.debug(result.message)
            assert result.status == DaliStatus.LOOPBACK
            assert result.data == 0xF0
            assert result.length == 8
