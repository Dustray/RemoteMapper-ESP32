@echo off
echo ========================================================
echo  Starting Serial Monitor (115200 baud) - Ctrl+C to Exit
echo ========================================================
python -m platformio device monitor -b 115200
