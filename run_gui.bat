@echo off
REM ============================================================
REM SwanOS — GUI Launcher
REM ============================================================

echo.
echo   SwanOS GUI Launcher
echo   ====================
echo.

where python >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo   ERROR: Python not found. Install Python 3.8+
    pause
    exit /b 1
)

REM === Demo Mode ===
if "%1"=="--demo" (
    echo   Starting SwanOS GUI in demo mode...
    echo.
    python swanos_gui.py --demo
    exit /b 0
)

REM === VirtualBox Mode ===
REM Step 1: Start your VirtualBox VM manually
REM Step 2: Run this script with the pipe path:
REM   run_gui.bat --vbox
if "%1"=="--vbox" (
    echo   Connecting to VirtualBox VM via named pipe...
    echo   Make sure VirtualBox serial port is set to:
    echo     Port Mode: Host Pipe
    echo     Path: \\.\pipe\swanos
    echo     [x] Create Pipe (checked)
    echo.
    echo   Start the VM FIRST, then press any key...
    pause >nul
    python swanos_gui.py --pipe \\.\pipe\swanos
    exit /b 0
)

REM === COM Port Mode ===
if "%1"=="--com" (
    echo   Connecting to COM port %2...
    python swanos_gui.py --pipe %2
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
python swanos_gui.py --demo
