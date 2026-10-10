@echo off
rem ============================================================
rem  Stop a running main.exe before linking.
rem  Windows locks a running .exe, which causes:
rem      LINK : fatal error LNK1168: cannot open main.exe for writing
rem  This script always exits with code 0, so the build task
rem  never fails just because no instance was running.
rem ============================================================
taskkill /F /IM main.exe >nul 2>&1
exit /b 0
