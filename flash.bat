@echo off
echo ========================================================
echo  Flashing RemoteMapper-ESP32 Firmware to ESP32-S3
echo ========================================================
python -m platformio run -e esp32s3_n16r8 -t upload
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Flash Failed!
    exit /b %ERRORLEVEL%
)
echo [SUCCESS] Flash completed successfully!
