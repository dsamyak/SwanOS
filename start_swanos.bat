@echo off
REM ============================================================
REM SwanOS — One-Click Launcher (C++ Qt6 Edition)
REM Just double-click this file!
REM ============================================================
title SwanOS
color 0B

echo.
echo   ============================================
echo        SwanOS - Starting Up...
echo   ============================================
echo.

set VBOX="C:\Program Files\Oracle\VirtualBox\VBoxManage.exe"
set VM=SwanOS
set PIPE=\\.\pipe\swanos

set GUIEXE="%~dp0swanos_gui\build\Release\swanos_gui.exe"
set GUIEXE_DBG="%~dp0swanos_gui\build\Debug\swanos_gui.exe"

REM Determine which GUI to launch
if exist %GUIEXE% (
    set LAUNCH=%GUIEXE%
) else if exist %GUIEXE_DBG% (
    set LAUNCH=%GUIEXE_DBG%
) else (
    echo   C++ GUI not built. Using Python fallback...
    set LAUNCH=python "%~dp0swanos_gui.py"
)

if not exist %VBOX% (
    echo   VirtualBox not found. Starting demo...
    %LAUNCH% --demo
    exit /b 0
)

REM --- Auto-configure serial port ---
echo   [1/3] Configuring VM serial port...
%VBOX% modifyvm "%VM%" --uart1 0x3F8 4 >nul 2>&1
%VBOX% modifyvm "%VM%" --uart-mode1 server "%PIPE%" >nul 2>&1
echo         OK

REM --- Start VM headless (runs in background) ---
echo   [2/3] Starting VM...
%VBOX% startvm "%VM%" --type gui >nul 2>&1
echo         OK

REM --- Wait for pipe to be created ---
echo   [3/3] Waiting for boot...
timeout /t 5 /nobreak >nul

echo.
echo   Launching GUI...
echo.

%LAUNCH% --pipe "%PIPE%"

echo.
set /p X=Power off VM? (Y/N):
if /i "%X%"=="Y" %VBOX% controlvm "%VM%" poweroff >nul 2>&1
