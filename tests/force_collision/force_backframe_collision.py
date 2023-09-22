#!/usr/bin/env python3
import serial
import time


def send_spaced_backframes(spacing_us):
    mu_sec = 0.000001
    print(f"send backframes with {spacing_us} us spacing")
    serial_0_port.write("Y00\r".encode("utf-8"))
    time.sleep(spacing_us * mu_sec)
    serial_1_port.write("YFF\r".encode("utf-8"))
    time.sleep(0.2)


print("FORCE_BACKFRAME COLLISION")
print("To execute please connect the following:")
print("* two DALI / USB adapter, enumerating as ttyUSB0 and ttyUSB1")
print("* a DALI power supply")
print("* optionally an oscilloscope")
print("Success and/or error detection is not part of this script.")
print("The test is passed, when all frames are created and opeation")
print("of the adapter doesn't stall.")


serial_0_portname = "/dev/ttyUSB0"
serial_1_portname = "/dev/ttyUSB1"

print("open serial ports")
serial_0_port = serial.Serial(port=serial_0_portname, baudrate=500000, timeout=0.2)
serial_1_port = serial.Serial(port=serial_1_portname, baudrate=500000, timeout=0.2)


print("start test sequence")
for i in range(5000):
    send_spaced_backframes(i * 2)
    print(
        f"received from serial port 0: {serial_0_port.readline().decode('utf-8')}",
        end="",
    )
    print(
        f"received from serial port 1: {serial_1_port.readline().decode('utf-8')}",
        end="",
    )
