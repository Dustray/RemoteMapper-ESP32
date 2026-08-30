# -*- coding: utf-8 -*-
"""
RemoteMapper PC Companion Daemon
================================
Ultra-lightweight Windows background service for RemoteMapper ESP32.
Listens for F13-F24 (and other virtual hotkeys) using native Win32 RegisterHotKey (0.0% CPU usage).
Executes custom local scripts (.bat, .ps1, .py, apps, URLs) asynchronously.
"""

import sys
import os
import json
import time
import ctypes
import subprocess
import threading
from ctypes import wintypes

# Ensure Windows environment
if sys.platform != 'win32':
    print("[ERROR] RemoteMapper PC Companion only supports Windows.")
    sys.exit(1)

# Win32 Constants
WM_HOTKEY = 0x0312
MOD_NONE = 0x0000
MOD_ALT = 0x0001
MOD_CONTROL = 0x0002
MOD_SHIFT = 0x0004
MOD_WIN = 0x0008
MOD_NOREPEAT = 0x4000

# Virtual Key Mapping for F13 - F24 and standard keys
VK_MAP = {
    # High F-keys (F13 - F24: standard no-conflict keys)
    "F13": 0x7C, "F14": 0x7D, "F15": 0x7E, "F16": 0x7F,
    "F17": 0x80, "F18": 0x81, "F19": 0x82, "F20": 0x83,
    "F21": 0x84, "F22": 0x85, "F23": 0x86, "F24": 0x87,
    # Standard F-keys
    "F1": 0x70, "F2": 0x71, "F3": 0x72, "F4": 0x73,
    "F5": 0x74, "F6": 0x75, "F7": 0x76, "F8": 0x77,
    "F9": 0x78, "F10": 0x79, "F11": 0x7A, "F12": 0x7B,
    # Special keys
    "PAUSE": 0x13, "SCROLLLOCK": 0x91, "PRINTSCREEN": 0x2C,
    "APPS": 0x5D, "SLEEP": 0x5F
}

def get_app_dir():
    """Get absolute directory path whether running from Python script or PyInstaller EXE."""
    if getattr(sys, 'frozen', False):
        return os.path.dirname(os.path.abspath(sys.executable))
    return os.path.dirname(os.path.abspath(__file__))

SCRIPT_DIR = get_app_dir()
CONFIG_FILE = os.path.join(SCRIPT_DIR, "config.json")

user32 = ctypes.windll.user32
kernel32 = ctypes.windll.kernel32

class RemoteMapperDaemon:
    def __init__(self, config_path=CONFIG_FILE):
        self.config_path = config_path
        self.config = {}
        self.hotkey_map = {} # id -> (key_name, config_entry)
        self.registered_ids = []
        self.last_config_mtime = 0
        self.running = True

    def load_config(self):
        """Load JSON configuration mapping hotkeys to local script actions."""
        if not os.path.exists(self.config_path):
            self.create_default_config()

        try:
            with open(self.config_path, "r", encoding="utf-8") as f:
                self.config = json.load(f)
            self.last_config_mtime = os.path.getmtime(self.config_path)
            print(f"[CONFIG] Loaded {len(self.config)} hotkey action(s) from {os.path.basename(self.config_path)}")
            return True
        except Exception as e:
            print(f"[ERROR] Failed to load config.json: {e}")
            return False

    def create_default_config(self):
        """Create sample config.json if missing."""
        default_cfg = {
            "F13": {
                "name": "打开计算器",
                "command": "calc.exe",
                "enabled": True
            },
            "F14": {
                "name": "打开浏览器网址",
                "command": "explorer https://gemini.google.com",
                "enabled": True
            },
            "F15": {
                "name": "打开记事本",
                "command": "notepad.exe",
                "enabled": True
            },
            "F16": {
                "name": "运行自定义 Python 任务",
                "command": "python scripts/demo_task.py",
                "enabled": True
            },
            "F17": {
                "name": "运行自定义批处理",
                "command": "scripts/demo_action.bat",
                "enabled": True
            }
        }
        with open(self.config_path, "w", encoding="utf-8") as f:
            json.dump(default_cfg, f, ensure_ascii=False, indent=4)
        print(f"[INIT] Created default template: {self.config_path}")

    def register_all_hotkeys(self):
        """Register all enabled hotkeys with Windows OS."""
        self.unregister_all_hotkeys()
        self.hotkey_map.clear()
        hotkey_id = 100

        for key_str, entry in self.config.items():
            if not entry.get("enabled", True):
                continue

            parts = [p.strip().upper() for p in key_str.split("+")]
            main_key = parts[-1]
            vk = VK_MAP.get(main_key)

            if not vk:
                print(f"[WARN] Unknown key: '{main_key}' in '{key_str}' (skipped)")
                continue

            mod = MOD_NOREPEAT
            for p in parts[:-1]:
                if p in ("CTRL", "CONTROL"): mod |= MOD_CONTROL
                elif p in ("ALT", "MENU"): mod |= MOD_ALT
                elif p in ("SHIFT",): mod |= MOD_SHIFT
                elif p in ("WIN", "WINDOWS", "SUPER"): mod |= MOD_WIN

            res = user32.RegisterHotKey(None, hotkey_id, mod, vk)
            if res:
                self.hotkey_map[hotkey_id] = (key_str, entry)
                self.registered_ids.append(hotkey_id)
                action_name = entry.get("name", "未命名动作")
                print(f"  [+] 成功监听: {key_str: <10} -> {action_name}")
                hotkey_id += 1
            else:
                err = kernel32.GetLastError()
                print(f"  [-] 注册失败: {key_str} (可能已被其他程序占用, Win32 Error: {err})")

    def unregister_all_hotkeys(self):
        """Unregister previously registered hotkeys."""
        for hid in self.registered_ids:
            user32.UnregisterHotKey(None, hid)
        self.registered_ids.clear()

    def execute_action(self, key_str, entry):
        """Execute the configured command asynchronously with robust relative path and Python environment resolution."""
        name = entry.get("name", "未命名动作")
        cmd = entry.get("command", "").strip()

        if not cmd:
            print(f"[TRIGGER] {key_str} 按下 -> 无配置命令")
            return

        # 1. Expand {DIR} or {ROOT} placeholders
        cmd = cmd.replace("{DIR}", SCRIPT_DIR).replace("{ROOT}", SCRIPT_DIR)

        # 2. Smart Python path resolution (works even if Python is not in system PATH on end-user PC)
        if cmd.startswith("python ") or cmd.startswith("pythonw "):
            py_bin = sys.executable
            if cmd.startswith("pythonw "):
                py_bin = py_bin.replace("python.exe", "pythonw.exe")
            cmd = f'"{py_bin}" ' + cmd.split(" ", 1)[1]

        print(f"\n[TRIGGER] >>> 遥控器按下 {key_str} [{name}] -> 执行命令: {cmd}")

        def _run():
            try:
                # Force working directory to SCRIPT_DIR so all relative paths (scripts/...) work anywhere
                subprocess.Popen(cmd, shell=True, cwd=SCRIPT_DIR)
            except Exception as ex:
                print(f"[ERROR] 执行失败 [{cmd}]: {ex}")

        threading.Thread(target=_run, daemon=True).start()

    def check_config_reload(self):
        """Periodically check if config.json was edited by user and hot-reload."""
        try:
            if os.path.exists(self.config_path):
                mtime = os.path.getmtime(self.config_path)
                if mtime > self.last_config_mtime:
                    print("\n[HOT-RELOAD] 检测到 config.json 已修改，正在重新载入配置...")
                    if self.load_config():
                        self.register_all_hotkeys()
        except Exception:
            pass

    def run(self):
        """Main message loop with Windows event pump (0% CPU)."""
        print("=" * 60)
        print("  RemoteMapper PC Companion - 极简超轻量本地脚本扩展守护进程")
        print(f"  工作目录: {SCRIPT_DIR}")
        print("=" * 60)

        self.load_config()
        self.register_all_hotkeys()

        print("\n[READY] 正在后台监听遥控器快捷键 (0.0% CPU 极低开销)...")
        print("[TIP] 随时编辑 config.json 即可实时热更新，按 Ctrl+C 退出程序。\n")

        msg = wintypes.MSG()
        last_check_time = time.time()

        while self.running:
            # Check for config reload every 1.5s
            now = time.time()
            if now - last_check_time > 1.5:
                self.check_config_reload()
                last_check_time = now

            # Process any available messages without busy looping
            while user32.PeekMessageW(ctypes.byref(msg), None, 0, 0, 1): # PM_REMOVE = 1
                if msg.message == WM_HOTKEY:
                    hid = msg.wParam
                    if hid in self.hotkey_map:
                        key_str, entry = self.hotkey_map[hid]
                        self.execute_action(key_str, entry)
                user32.TranslateMessage(ctypes.byref(msg))
                user32.DispatchMessageW(ctypes.byref(msg))

            # Sleep 30ms to prevent CPU spin
            time.sleep(0.03)

        self.unregister_all_hotkeys()

if __name__ == "__main__":
    daemon = RemoteMapperDaemon()
    try:
        daemon.run()
    except KeyboardInterrupt:
        print("\n[EXIT] RemoteMapper PC Companion 已退出。")
        daemon.unregister_all_hotkeys()
