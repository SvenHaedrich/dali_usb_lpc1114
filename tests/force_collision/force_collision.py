import serial
import time

serial_0_portname = "/dev/ttyUSB0"
serial_1_portname = "/dev/ttyUSB1"

print("open serial ports")
serial_0_port = serial.Serial(port=serial_0_portname, baudrate=115200, timeout=0.2)
serial_1_port = serial.Serial(port=serial_1_portname, baudrate=115200, timeout=0.2)
mu_sec = 0.000001


print("start test sequence")
for i in range(1000):
    print(f"iteration: {i}")
    serial_0_port.write("Y00\r".encode("utf-8"))
    time.sleep(i * 10 * mu_sec)
    serial_1_port.write("YFF\r".encode("utf-8"))
    time.sleep(0.2)
    print(
        f"received from serial port 0: {serial_0_port.readline().decode('utf-8')}",
        end="",
    )
    print(
        f"received from serial port 1: {serial_1_port.readline().decode('utf-8')}",
        end="",
    )
