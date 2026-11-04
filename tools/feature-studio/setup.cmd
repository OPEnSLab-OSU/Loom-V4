@echo off
node "%~dp0setup-loom.mjs" %*
if errorlevel 1 pause
