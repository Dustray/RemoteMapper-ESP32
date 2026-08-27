@echo off
echo ========================================================
echo  Building RemoteMapper-ESP32 Firmware (ESP32-S3 N16R8)
echo ========================================================
python -m platformio run -e esp32s3_n16r8
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build Failed!
    exit /b %ERRORLEVEL%
)
echo [SUCCESS] Build completed successfully!
