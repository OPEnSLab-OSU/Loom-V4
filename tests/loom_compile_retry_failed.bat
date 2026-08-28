@echo off
setlocal EnableExtensions

echo Finding retry-eligible failures in the newest Loom compile audit...
set "RETRY_MANIFEST=%TEMP%\loom_retry_failed_%RANDOM%%RANDOM%.txt"
set "RETRY_TESTS_DIR=%~dp0"
if "%RETRY_TESTS_DIR:~-1%"=="\" set "RETRY_TESTS_DIR=%RETRY_TESTS_DIR:~0,-1%"

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0loom_retry_failed.ps1" -TestsDir "%RETRY_TESTS_DIR%" -ManifestPath "%RETRY_MANIFEST%"
set "SELECT_RC=%ERRORLEVEL%"

if "%SELECT_RC%"=="2" (
    if exist "%RETRY_MANIFEST%" del /q "%RETRY_MANIFEST%" >nul 2>nul
    echo.
    echo No failed sketches are eligible for retry.
    echo Press any key to close.
    pause >nul
    exit /b 0
)

if not "%SELECT_RC%"=="0" (
    if exist "%RETRY_MANIFEST%" del /q "%RETRY_MANIFEST%" >nul 2>nul
    echo.
    echo ERROR: Could not prepare the failed-sketch retry list.
    echo Press any key to close.
    pause >nul
    exit /b %SELECT_RC%
)

set "LOOM_COMPILE_MODE=AUDIT"
set "SKETCH_LIST_FILE=%RETRY_MANIFEST%"

if not exist "%~dp0loom_compile_engine.bat" (
    echo ERROR: loom_compile_engine.bat was not found in %~dp0
    del /q "%RETRY_MANIFEST%" >nul 2>nul
    pause
    exit /b 1
)

call "%~dp0loom_compile_engine.bat"
set "COMPILE_RC=%ERRORLEVEL%"
del /q "%RETRY_MANIFEST%" >nul 2>nul
exit /b %COMPILE_RC%
