@echo off
setlocal
where node >nul 2>nul
if errorlevel 1 (
  echo Node.js is missing. Install it from https://nodejs.org/en/download
  echo Then reopen this window and run this launcher again.
  if "%~1"=="" pause
  exit /b 1
)
node "%~dp0loom-build.cjs" %*
set "loomBuildExit=%errorlevel%"
if "%~1"=="" pause
exit /b %loomBuildExit%
