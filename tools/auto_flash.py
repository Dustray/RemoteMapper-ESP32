import serial
import serial.tools.list_ports
import subprocess
import time
import sys

def get_esp_ports():
    ports = []
    for p in serial.tools.list_ports.comports():
        if "303A" in p.hwid.upper():
            ports.append(p.device)
    return ports

def auto_upload():
    ports = get_esp_ports()
    if not ports:
        print("No ESP32-S3 port found!")
        sys.exit(1)

    port = ports[0]
    print(f"Detected ESP32 port: {port}")

    # Trigger 1200bps DTR reset to enter bootloader
    print("Triggering 1200bps CDC bootloader entry...")
    try:
        s = serial.Serial(port, 1200)
        s.dtr = False
        s.rts = True
        time.sleep(0.1)
        s.close()
    except Exception as e:
        pass

    # Wait up to 3 seconds for bootloader port to appear
    boot_port = None
    for _ in range(15):
        time.sleep(0.2)
        cur = get_esp_ports()
        if cur:
            boot_port = cur[0]
            break

    if not boot_port:
        boot_port = port

    print(f"Targeting bootloader port: {boot_port}")

    esptool_path = r"C:\Users\cuicuiV5\.platformio\packages\tool-esptoolpy\esptool.py"
    bootloader = r".pio\build\esp32s3_n16r8\bootloader.bin"
    partitions = r".pio\build\esp32s3_n16r8\partitions.bin"
    boot_app0 = r"C:\Users\cuicuiV5\.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin"
    firmware = r".pio\build\esp32s3_n16r8\firmware.bin"

    # Crucial: app0 offset is 0x10000 for standard Arduino default_16MB.csv
    cmd = [
        sys.executable, esptool_path,
        "--chip", "esp32s3",
        "--port", boot_port,
        "--baud", "460800",
        "--before", "default_reset",
        "--after", "hard_reset",
        "write_flash", "-z",
        "--flash_mode", "dio",
        "--flash_freq", "80m",
        "--flash_size", "16MB",
        "0x0", bootloader,
        "0x8000", partitions,
        "0xe000", boot_app0,
        "0x10000", firmware
    ]

    print(f"Flashing firmware to {boot_port} (App offset: 0x10000, Mode: dio)...")
    res = subprocess.run(cmd)
    if res.returncode == 0:
        print("\n==========================================")
        print(" FLASH SUCCESSFUL! (App written to 0x10000)")
        print("==========================================\n")
    else:
        sys.exit(res.returncode)

if __name__ == "__main__":
    auto_upload()
