@echo off
where node >nul 2>nul
if errorlevel 1 (
  echo Install Node.js 18 or newer from https://nodejs.org then run this again.
  pause
  exit /b 1
)
node "%~dp0launch.mjs" %*
if errorlevel 1 pause
