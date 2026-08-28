import serial
import time
import sys

def main():
    try:
        ser = serial.Serial('COM10', 115200, timeout=0.1)
    except Exception as e:
        print(f"Failed to open COM10: {e}")
        return

    print("==================================================")
    print(" RemoteMapper ESP32-S3 Live Monitor Started")
    print("==================================================")
    
    last_status_time = 0
    try:
        while True:
            # Poll status every 5 seconds if idle
            now = time.time()
            if now - last_status_time > 5:
                ser.write(b'status\n')
                last_status_time = now

            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                # Print any log or status
                print(line)
            time.sleep(0.02)
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()

if __name__ == "__main__":
    main()
