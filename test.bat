@echo off
echo ========================================================
echo  Running RemoteMapper-ESP32 Algorithm & Unit Tests
echo ========================================================
python test\native\test_suite.py
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Unit Tests Failed!
    exit /b %ERRORLEVEL%
)
echo [SUCCESS] All Unit Tests Passed!
