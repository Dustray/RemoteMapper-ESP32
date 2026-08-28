import serial
import time
import sys

def stream():
    ser = serial.Serial('COM10', 115200, timeout=0.1)
    print("Listening to RemoteMapper ESP32-S3 (COM10)...")
    ser.write(b'status\n')
    t0 = time.time()
    try:
        while time.time() - t0 < 20:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                print(line)
            time.sleep(0.01)
    finally:
        ser.close()

if __name__ == "__main__":
    stream()
