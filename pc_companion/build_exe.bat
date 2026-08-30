@echo off
cd /d "%~dp0"
echo 正在安装/检查 PyInstaller...
python -m pip install pyinstaller
echo 正在打包为无黑框独立 exe...
pyinstaller --noconsole --onefile --name RemoteMapperHost remotemapper_host.py
echo.
echo [OK] 打包完成！独立免安装程序位于: dist\RemoteMapperHost.exe
pause
