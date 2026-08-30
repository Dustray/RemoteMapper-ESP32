; ==============================================================================
; RemoteMapper AutoHotkey Companion (极致超轻量 2MB 方案)
; ==============================================================================
#NoEnv
#SingleInstance Force
SetWorkingDir %A_ScriptDir%

; F13 键 -> 打开计算器
F13::
    Run, calc.exe
    return

; F14 键 -> 打开常用网址
F14::
    Run, https://gemini.google.com
    return

; F15 键 -> 打开记事本
F15::
    Run, notepad.exe
    return

; F16 键 -> 运行 Python 脚本
F16::
    Run, python scripts/demo_task.py
    return

; F17 键 -> 运行批处理脚本
F17::
    Run, scripts/demo_action.bat
    return

; F19 ~ F24 预留扩展槽位
F19::
    ; 在此添加您的自定义命令
    return

F20::
    ; 在此添加您的自定义命令
    return

F21::
    return

F22::
    return

F23::
    return

F24::
    return
