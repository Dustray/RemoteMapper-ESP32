# -*- coding: utf-8 -*-
import os
import sys
import winreg
import subprocess

REG_PATH = r"Software\Microsoft\Windows\CurrentVersion\Run"
APP_NAME = "RemoteMapperCompanion"

def get_pythonw_path():
    python_exe = sys.executable
    pythonw = os.path.join(os.path.dirname(python_exe), "pythonw.exe")
    if os.path.exists(pythonw):
        return pythonw
    return python_exe

def enable_autostart():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    host_script = os.path.join(script_dir, "remotemapper_host.py")
    pythonw = get_pythonw_path()

    cmd = f'"{pythonw}" "{host_script}"'

    # Write to HKCU Run
    key = winreg.OpenKey(winreg.HKEY_CURRENT_USER, REG_PATH, 0, winreg.KEY_SET_VALUE)
    winreg.SetValueEx(key, APP_NAME, 0, winreg.REG_SZ, cmd)
    winreg.CloseKey(key)

    print("=" * 60)
    print("[OK] 成功设置开机自启动！")
    print(f"  自启目标: {cmd}")
    print("  启动模式: 后台静默无弹窗运行 (pythonw.exe)")
    print("=" * 60)

    # Launch immediately in background
    subprocess.Popen([pythonw, host_script], cwd=script_dir)
    print("[OK] 已在后台为您即刻启动静默守护进程！")

def disable_autostart():
    try:
        key = winreg.OpenKey(winreg.HKEY_CURRENT_USER, REG_PATH, 0, winreg.KEY_SET_VALUE)
        winreg.DeleteValue(key, APP_NAME)
        winreg.CloseKey(key)
        print("=" * 60)
        print("[OK] 已成功从开机自启动中移除！")
        print("=" * 60)
    except FileNotFoundError:
        print("[INFO] 当前未开启开机自启。")
    except Exception as e:
        print(f"[ERROR] 移除失败: {e}")

if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1].lower() == "disable":
        disable_autostart()
    else:
        enable_autostart()
