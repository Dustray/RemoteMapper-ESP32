import serial
import serial.tools.list_ports
import time

def find_esp32_port():
    for p in serial.tools.list_ports.comports():
        if "303A" in p.hwid.upper():
            return p.device
    return None

def monitor():
    print("Waiting for ESP32-S3 port to appear...")
    port = None
    for _ in range(30):
        port = find_esp32_port()
        if port:
            break
        time.sleep(0.5)

    if not port:
        print("No ESP32 port found.")
        return

    print(f"Found ESP32 on {port}, connecting at 115200 baud...")
    try:
        ser = serial.Serial(port, 115200, timeout=0.2)
        print("Connected! Listening for logs (Press Ctrl+C to exit):")
        print("=" * 60)
        t_end = time.time() + 15
        while time.time() < t_end:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                print(line)
        ser.close()
    except Exception as e:
        print(f"Serial error: {e}")

if __name__ == "__main__":
    monitor()
