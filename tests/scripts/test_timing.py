import pytest
import logging
import time

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

def read_result_and_assert(serial, type, length, data):
    reply = serial.readline()
    logger.debug(F"read line: {reply}")
    start = reply.find(ord('{'))+1
    end = reply.find(ord('}'))
    assert start >= 0
    assert end >= 0
    payload = reply[start:end]
    payload_type = chr(payload[8])
    payload_length = int(payload[9:11], 16)
    payload_data = int(payload[12:20], 16)
    assert payload_type == type
    assert payload_length == length
    assert payload_data == data

def test_legal_bittiming(serial_conn):
    std_halfbit_period = 417
    bits = [std_halfbit_period] * 17
    set_up_and_send_sequence(serial_conn, bits);
    read_result_and_assert(serial_conn, "-", 8, 0xff)

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
    read_result_and_assert(serial_conn, "-", 8, value)
