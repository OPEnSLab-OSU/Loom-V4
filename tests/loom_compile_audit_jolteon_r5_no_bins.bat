@echo off
echo Starting saved Jolteon SARA-R5 compile audit without retained build folders or firmware binaries.
set "LOOM_COMPILE_MODE=AUDIT"
set "EXAMPLES_ROOT=%~dp0..\examples\Lab Examples\Jolteon"
if not exist "%~dp0loom_compile_engine.bat" (
    echo ERROR: loom_compile_engine.bat was not found in %~dp0
    pause
    exit /b 1
)
call "%~dp0loom_compile_engine.bat"
exit /b %ERRORLEVEL%
