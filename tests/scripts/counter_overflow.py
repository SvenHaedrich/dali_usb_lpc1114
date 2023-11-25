import time
import logging

from dali_interface.serial import DaliSerial
from dali_interface.dali_interface import DaliStatus, DaliFrame

logging.basicConfig(level=logging.DEBUG)

if_a = DaliSerial("/dev/ttyUSB0")
if_b = DaliSerial("/dev/ttyUSB1")


def dali_check_queue_empty(adapter: DaliSerial) -> bool:
    result = adapter.get(1)
    if result.status != DaliStatus.TIMEOUT:
        print("Expected queue to be empty. Found:", result)
        return False
    print("Queue is empty")
    return True


def dali_compare_frame(A: DaliFrame, B: DaliFrame) -> bool:
    if A.length != B.length:
        print("length deviation: {A.length} is not equal to {B.length}")
        return False
    if A.data != B.data:
        print("data deviation: {A.data} is not equal to {B.data}")
        return False
    return True


def dali_exchange(
    from_adapter: DaliSerial, to_adapter: DaliSerial, frame: DaliFrame
) -> bool:
    from_adapter.transmit(frame)
    from_result = from_adapter.get(5)
    if not dali_compare_frame(frame, from_result):
        return False
    to_result = to_adapter.get(5)
    if not dali_compare_frame(frame, to_result):
        return False
    return True


def dali_interaction() -> bool:
    if not dali_check_queue_empty(if_a):
        return False
    if not dali_check_queue_empty(if_b):
        return False
    forward_frame = DaliFrame(length=16, data=0xFF00)
    if not dali_exchange(if_a, if_b, forward_frame):
        return False
    if not dali_exchange(if_b, if_a, forward_frame):
        return False
    backward_frame = DaliFrame(length=8, data=0xAA, priority=0)
    if not dali_exchange(if_a, if_b, backward_frame):
        return False
    if not dali_exchange(if_b, if_a, backward_frame):
        return False
    print("successful interaction")
    return True


print("CHECK FOR COUNTER OVERFLOWS")
print("To execute please connect the following:")
print("* two DALI / USB adapter, enumerating as ttyUSB0 and ttyUSB1")
print("* a DALI power supply")

sleep_for_seconds = 1
go_on = True
while go_on:
    go_on = dali_interaction()
    print(f"sleep for {sleep_for_seconds} seconds")
    continue_time = time.time() + sleep_for_seconds
    continue_string = time.strftime("%H:%M", time.localtime(continue_time))
    print(f"continue at {continue_string} local time")
    time.sleep(sleep_for_seconds)
    sleep_for_seconds += sleep_for_seconds
    if sleep_for_seconds > 8500:
        print("* Test run successful")
        go_on = False
