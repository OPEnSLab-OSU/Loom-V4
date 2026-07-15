@echo off
setlocal

echo Restoring temporary Loom warning-scope instrumentation...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0warning_scope_architecture.ps1" -Action Restore
set "RESULT=%ERRORLEVEL%"

echo.
if not "%RESULT%"=="0" (
    echo Restore failed. The saved restore patch was retained for recovery.
) else (
    echo Restore complete. The consumed restore state was removed.
)
echo.
pause
exit /b %RESULT%
