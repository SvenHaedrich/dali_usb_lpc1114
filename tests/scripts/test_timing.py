import pytest
import logging
import time

from dali_py.source.DALI import DALIError, Raw_Frame

logger = logging.getLogger(__name__)

def set_up_and_send_sequence(serial, bit_timings):
    short_time = 0.05
    # start sequence
    cmd = "Q\r"
    serial.write(cmd.encode("utf-8"))
    time.sleep(short_time)
    # set periods
    for period in bit_timings:
        cmd = F"N {period:x}\r"
        serial.write(cmd.encode("utf-8"))
        time.sleep(short_time)
    # here we go
    cmd = "X\r"
    serial.write(cmd.encode("utf-8"))

def read_result_and_assert(serial, length, data):
    reply = serial.readline()
    logger.debug(F"read line: {reply}")
    result = Raw_Frame()
    result.from_line(reply)
    assert result.type == Raw_Frame.COMMAND
    assert result.length == length
    assert result.data == data

def test_legal_bittiming(serial_conn):
    std_halfbit_period = 417
    bits = [std_halfbit_period] * 17
    set_up_and_send_sequence(serial_conn, bits);
    read_result_and_assert(serial_conn, 8, 0xff)

@pytest.mark.parametrize("index,value",[
    (1,0x00), 
    (3,0x80), 
    (5,0xC0), 
    (7,0xE0), 
    (9,0xF0), 
    (11,0xF8), 
    (13,0xFC), 
    (15,0xFE)
    ])
def test_more_legal_timings(serial_conn, index, value):
    std_halfbit_period = 417
    std_fullbit_period = 833
    bits = [std_halfbit_period] * 17
    bits[index] = std_fullbit_period
    set_up_and_send_sequence(serial_conn, bits);
    read_result_and_assert(serial_conn, 8, value)

@pytest.mark.parametrize("length_us,expected_code",[
    (200,DALIError.RECEIVE_START_TIMING),
    (250,DALIError.RECEIVE_START_TIMING),
    (300,DALIError.RECEIVE_START_TIMING),
    (350,DALIError.OK),
    (400,DALIError.OK),
    (450,DALIError.OK),
    (500,DALIError.OK),
    (750,DALIError.RECEIVE_START_TIMING),
    (1000,DALIError.RECEIVE_START_TIMING),
    (1500,DALIError.RECEIVE_START_TIMING),
    (2000,DALIError.RECEIVE_START_TIMING),
    (2500,DALIError.RECEIVE_START_TIMING),
    (4000,DALIError.RECEIVE_START_TIMING),
    (10000,DALIError.RECEIVE_START_TIMING),
    (20000,DALIError.RECEIVE_START_TIMING),
    (50000,DALIError.RECEIVE_START_TIMING),
    (100000,DALIError.RECEIVE_START_TIMING),
    (500000,DALIError.RECEIVE_START_TIMING),
])
def test_startbit_lengths(serial_conn, length_us, expected_code):
    short_time = 0.05
    # start sequence
    cmd = "Q\r"
    serial_conn.write(cmd.encode("utf-8"))
    time.sleep(short_time)
    # set period
    cmd = F"N {length_us:x}\r"
    serial_conn.write(cmd.encode("utf-8"))
    time.sleep(short_time)
    # here we go
    cmd = "X\r"
    serial_conn.write(cmd.encode("utf-8"))
    time.sleep(short_time)
    # read result
    reply = serial_conn.readline()
    logger.debug(F"read line: {reply}")
    result = Raw_Frame()
    result.from_line(reply)
    if expected_code == DALIError.OK:
        assert result.type == Raw_Frame.COMMAND
    else:
        assert result.type == Raw_Frame.ERROR
        assert result.length == expected_code
        assert (result.data & 0xFF) == 0
        time_us = result.data >> 8
        assert abs(time_us - length_us)  < 10.0

@pytest.mark.parametrize("length_us",[600000,800000,1000000])
def test_system_failures(serial_conn, length_us):
    short_time = 0.05
    # start sequence
    cmd = "Q\r"
    serial_conn.write(cmd.encode("utf-8"))
    time.sleep(short_time)
    # set period
    cmd = F"N {length_us:x}\r"
    serial_conn.write(cmd.encode("utf-8"))
    time.sleep(short_time)
    # here we go
    cmd = "X\r"
    serial_conn.write(cmd.encode("utf-8"))
    time.sleep(short_time)
    # read failure message
    reply = serial_conn.readline()
    logger.debug(F"read line: {reply}")
    failure = Raw_Frame()
    failure.from_line(reply)
    assert failure.type == Raw_Frame.ERROR
    assert failure.length == DALIError.SYSTEM_FAILURE
    assert (failure.data & 0xFF) == 0
    time_us = failure.data >> 8
    assert time_us == 0
    # read recover message
    time.sleep ((length_us-600000)/1E6)
    reply = serial_conn.readline()
    logger.debug(F"read line: {reply}")
    recover = Raw_Frame()
    recover.from_line(reply)
    assert recover.type == Raw_Frame.ERROR
    assert recover.length == DALIError.SYSTEM_RECOVER
    assert (recover.data & 0xFF) == 0
    time_us = recover.data >> 8
    assert abs(time_us - length_us) < 10.0
    assert abs((recover.timestamp - failure.timestamp) - (length_us/1E6)) < 0.002
