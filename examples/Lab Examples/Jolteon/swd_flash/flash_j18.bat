@echo off
cd /d "%~dp0"
openocd -f flash_j18.cfg
pause
