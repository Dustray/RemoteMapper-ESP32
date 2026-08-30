@echo off
cd /d "%~dp0"
cls
echo ========================================================
echo   RemoteMapper PC Companion Service
echo ========================================================
echo.
python remotemapper_host.py
if errorlevel 1 (
    echo.
    echo [ERROR] Python script exited with error.
    pause
)
