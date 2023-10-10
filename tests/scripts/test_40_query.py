import pytest
import logging

from dali_interface.source.frame import DaliFrame, DaliStatus

logger = logging.getLogger(__name__)
timeout_time_sec = 2


def test_queries(dali_serial) -> None:
    dali_serial.start_receive()
    test_cmd_1 = 0xFF01
    test_frame_1 = DaliFrame(length=16, data=test_cmd_1)
    dali_serial.query_reply(test_frame_1)
    assert dali_serial.rx_frame.status.status == DaliStatus.TIMEOUT
    test_cmd_2 = 0xFF02
    test_frame_2 = DaliFrame(length=16, data=test_cmd_2)
    dali_serial.transmit(test_frame_2)
    dali_serial.get_next()
    assert dali_serial.rx_frame.status.status == DaliStatus.LOOPBACK
    assert dali_serial.rx_frame.length == 16
    assert dali_serial.rx_frame.data == test_cmd_2
