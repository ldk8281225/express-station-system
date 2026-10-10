@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo.
echo  ==================================================
echo           快递驿站智能管理系统  v2.0  启动器
echo  ==================================================
echo.

if exist "main.exe" goto RUN

echo  [提示] 未找到 main.exe，正在尝试自动编译...
echo.

set "CC="
if exist "C:\Program Files (x86)\Dev-Cpp\MinGW64\bin\gcc.exe" goto HAVE_CC
where gcc >nul 2>&1
if errorlevel 1 goto NOCC
set "CC=gcc"
goto BUILD

:HAVE_CC
set "CC=C:\Program Files (x86)\Dev-Cpp\MinGW64\bin\gcc.exe"

:BUILD
set "SRC="
for %%f in (*.c) do set "SRC=!SRC! %%f"
echo  编译器: !CC!
"%CC%" -std=c99 -O2 -Wall -Wextra -o main.exe !SRC!
if not exist "main.exe" goto BUILDFAIL
echo.
echo  [完成] 编译成功。
echo.
goto RUN

:NOCC
echo  [错误] 没有找到可用的 C 编译器。
echo         请先用 Dev-C++ 或 VS Code 编译出 main.exe，再运行本文件。
echo.
pause
exit /b 1

:BUILDFAIL
echo  [错误] 编译失败，请查看上面的报错信息。
echo.
pause
exit /b 1

:RUN
echo  正在启动程序（会自动从 packages.dat 恢复上次的数据）...
echo  退出时请选菜单 [0] 保存并退出。
echo  --------------------------------------------------
main.exe
chcp 936 >nul 2>&1
echo  --------------------------------------------------
echo.
echo  程序已退出，数据保存在 packages.dat 中。
echo  再次双击本文件即可重新启动。
echo.
pause
