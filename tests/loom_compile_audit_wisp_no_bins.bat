@echo off
setlocal
set "LOOM_COMPILE_MODE=AUDIT"
set "EXAMPLES_ROOT=%~dp0..\examples\Lab Examples\Wisp"
call "%~dp0loom_compile_engine.bat"
exit /b %ERRORLEVEL%
