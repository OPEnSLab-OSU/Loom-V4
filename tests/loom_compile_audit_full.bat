@echo off
echo Starting loom_compile_audit_full.bat
set "LOOM_COMPILE_MODE=FULL"
if not exist "%~dp0loom_compile_engine.bat" (
  echo ERROR: Missing %~dp0loom_compile_engine.bat
  pause
  exit /b 1
)
call "%~dp0loom_compile_engine.bat"
exit /b %ERRORLEVEL%
