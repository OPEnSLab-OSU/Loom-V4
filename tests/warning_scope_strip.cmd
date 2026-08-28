@echo off
setlocal

echo Stripping temporary Loom warning-scope instrumentation...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0warning_scope_architecture.ps1" -Action Strip
set "RESULT=%ERRORLEVEL%"

echo.
if not "%RESULT%"=="0" (
    echo Strip failed. Source files were left instrumented or rolled back.
) else (
    echo Strip complete. Use warning_scope_restore.cmd to reinsert the instrumentation.
)
echo.
pause
exit /b %RESULT%
