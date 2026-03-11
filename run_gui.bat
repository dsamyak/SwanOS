@echo off
REM ============================================================
REM SwanOS — GUI Launcher (C++ Qt6 Edition)
REM ============================================================

echo.
echo   SwanOS GUI Launcher
echo   ====================
echo.

set GUIEXE="%~dp0swanos_gui\build\Release\swanos_gui.exe"
set GUIEXE_DBG="%~dp0swanos_gui\build\Debug\swanos_gui.exe"

REM Check for compiled binary
if exist %GUIEXE% (
    set LAUNCH=%GUIEXE%
) else if exist %GUIEXE_DBG% (
    set LAUNCH=%GUIEXE_DBG%
) else (
    echo   C++ GUI not found. Falling back to Python GUI...
    echo   To build the C++ GUI:
    echo     cd swanos_gui
    echo     mkdir build ^& cd build
    echo     cmake .. -G "Visual Studio 17 2022"
    echo     cmake --build . --config Release
    echo.
    where python >nul 2>&1
    if %ERRORLEVEL% NEQ 0 (
        echo   ERROR: Neither C++ GUI nor Python found.
        pause
        exit /b 1
    )
    set LAUNCH=python "%~dp0swanos_gui.py"
)

REM === Demo Mode ===
if "%1"=="--demo" (
    echo   Starting SwanOS GUI in demo mode...
    echo.
    %LAUNCH% --demo
    exit /b 0
)

REM === VirtualBox Mode ===
if "%1"=="--vbox" (
    echo   Connecting to VirtualBox VM via named pipe...
    echo   Make sure VirtualBox serial port is set to:
    echo     Port Mode: Host Pipe
    echo     Path: \\.\pipe\swanos
    echo     [x] Create Pipe (checked)
    echo.
    echo   Start the VM FIRST, then press any key...
    pause >nul
    %LAUNCH% --pipe \\.\pipe\swanos
    exit /b 0
)

REM === COM Port Mode ===
if "%1"=="--com" (
    echo   Connecting to COM port %2...
    %LAUNCH% --pipe %2
    exit /b 0
)

REM === Default: Demo ===
echo   Usage:
echo     run_gui.bat --demo       Demo mode (no VM)
echo     run_gui.bat --vbox       VirtualBox mode
echo     run_gui.bat --com COM3   COM port mode
echo.
echo   Starting demo mode...
echo.
%LAUNCH% --demo
