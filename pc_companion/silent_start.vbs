Set WshShell = CreateObject("WScript.Shell")
WshShell.Run "pythonw.exe "" & Replace(WScript.ScriptFullName, "silent_start.vbs", "remotemapper_host.py") & "", 0, False
