@echo off
chcp 65001 >nul
setlocal

echo ================================================
echo   快递驿站智能管理系统 - 一键编译
echo ================================================
echo.

cd /d "%~dp0"

set SRC=
for %%f in (*.c) do call set SRC=%%SRC%% %%f

if "%SRC%"=="" (
    echo [错误] 当前目录下没有找到 .c 源文件。
    pause
    exit /b 1
)

echo 编译命令：gcc -std=c99 -O2 -Wall -Wextra -o main.exe%SRC%
echo.

gcc -std=c99 -O2 -Wall -Wextra -o main.exe%SRC%

if errorlevel 1 (
    echo.
    echo [失败] 编译出错。若提示找不到 gcc，请先安装 MinGW-w64
    echo        或改用 Dev-C++ / VS Code 的编译任务。
    pause
    exit /b 1
)

echo.
echo [成功] 已生成 main.exe，双击或运行它即可启动系统。
echo.
pause
