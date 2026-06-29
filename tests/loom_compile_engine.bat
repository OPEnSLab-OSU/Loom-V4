@echo off
echo.
echo ================================================================================
echo Loom compile test engine starting.
echo Version: 2026-06-27-v18-warning-scope
echo Script: %~f0
echo Tests folder: %~dp0
echo ================================================================================
setlocal EnableExtensions EnableDelayedExpansion

set "LOOM_TESTS_VERSION=2026-06-27-v18-warning-scope"
if not defined LOOM_COMPILE_MODE set "LOOM_COMPILE_MODE=SMOKE"
if not defined AUTO_INSTALL_TOOLS set "AUTO_INSTALL_TOOLS=1"
if not defined FQBN set "FQBN=loom4:samd:adafruit_feather_m0:usbstack=arduino,debug=off"
if not defined WARNINGS set "WARNINGS=all"
if not defined STRICT_WARNINGS set "STRICT_WARNINGS=0"
if not defined CONSOLE_OUTPUT set "CONSOLE_OUTPUT=important"
if not defined CONSOLE_WARNINGS set "CONSOLE_WARNINGS=suppress"
if not defined NO_PAUSE set "NO_PAUSE=0"

call :SetAnsi
call :SanitizedTimestamp TS
set "RANDTAG=%RANDOM%%RANDOM%"

set "TESTS_DIR=%~dp0"
if "%TESTS_DIR:~-1%"=="\" set "TESTS_DIR=%TESTS_DIR:~0,-1%"
for %%A in ("%TESTS_DIR%\..") do set "LOOM_DIR=%%~fA"
for %%A in ("%LOOM_DIR%\..\..") do set "PACKAGE_ROOT=%%~fA"
set "PACKAGE_LIBS=%PACKAGE_ROOT%\libraries"
if not defined EXAMPLES_ROOT set "EXAMPLES_ROOT=%LOOM_DIR%\examples"
if not defined ARDUINO_DATA set "ARDUINO_DATA=%LOCALAPPDATA%\Arduino15"
if not defined SKETCHBOOK_DIR set "SKETCHBOOK_DIR=%USERPROFILE%\Documents\Arduino"
set "SKETCHBOOK_LIBS=%SKETCHBOOK_DIR%\libraries"

if /i "%LOOM_COMPILE_MODE%"=="CHECK" (
    call :Preflight
    set "RC=!ERRORLEVEL!"
    call :MaybePause
    exit /b !RC!
)

call :Preflight
if errorlevel 1 (
    echo.
    call :Red "Preflight failed. Compile run was not started."
    call :MaybePause
    exit /b 1
)

set "SAVE_REPORT=0"
set "KEEP_BUILD=0"
set "SAVE_ARTIFACTS=0"
set "MODE_LABEL=temporary Loom compile smoke test"
if /i "%LOOM_COMPILE_MODE%"=="AUDIT" (
    set "SAVE_REPORT=1"
    set "KEEP_BUILD=0"
    set "SAVE_ARTIFACTS=0"
    set "MODE_LABEL=saved Loom compile audit"
)
if /i "%LOOM_COMPILE_MODE%"=="FULL" (
    set "SAVE_REPORT=1"
    set "KEEP_BUILD=1"
    set "SAVE_ARTIFACTS=1"
    set "MODE_LABEL=full Loom compile audit with build artifacts"
)

if "%SAVE_REPORT%"=="1" (
    set "OUT_ROOT=%TESTS_DIR%\loom_compile_audit_%TS%"
) else (
    set "OUT_ROOT=%TEMP%\loom_compile_smoke_%TS%_%RANDTAG%"
)
set "LOG_DIR=%OUT_ROOT%\logs"
set "BUILD_ROOT_AUTO=0"
if not defined BUILD_ROOT (
    set "BUILD_ROOT=%TEMP%\loom_build_%TS%_%RANDTAG%"
    set "BUILD_ROOT_AUTO=1"
)
set "REPORT_CSV=%OUT_ROOT%\compile_report.csv"
set "SUMMARY_TXT=%OUT_ROOT%\summary.txt"
set "COLOR_SUMMARY=%OUT_ROOT%\summary_color.ansi.log"
set "WARNINGS_ALL=%OUT_ROOT%\warnings_all.txt"
set "WARNINGS_UNIQUE=%OUT_ROOT%\warnings_unique.txt"
set "WARNINGS_LOOM=%OUT_ROOT%\warnings_loom.txt"
set "WARNINGS_LOOM_UNIQUE=%OUT_ROOT%\warnings_loom_unique.txt"
set "WARNINGS_EXTERNAL=%OUT_ROOT%\warnings_external.txt"
set "WARNINGS_EXTERNAL_UNIQUE=%OUT_ROOT%\warnings_external_unique.txt"
set "ERRORS_ALL=%OUT_ROOT%\errors_all.txt"
set "COMPLETE_DIR=%OUT_ROOT%\complete_builds"
mkdir "%LOG_DIR%" >nul 2>nul
mkdir "%BUILD_ROOT%" >nul 2>nul
if "%SAVE_ARTIFACTS%"=="1" mkdir "%COMPLETE_DIR%" >nul 2>nul

if "%SAVE_REPORT%"=="1" (
    >"%REPORT_CSV%" echo index,result,exit_code,warnings,loom_warnings,external_warnings,errors,time_seconds,sketch_name,sketch_path,log_path,build_path
    >"%SUMMARY_TXT%" echo Loom compile audit summary
    >>"%SUMMARY_TXT%" echo Version: %LOOM_TESTS_VERSION%
    >>"%SUMMARY_TXT%" echo Started: %TS%
    >>"%SUMMARY_TXT%" echo FQBN: %FQBN%
    >>"%SUMMARY_TXT%" echo Warnings: %WARNINGS%
    >>"%SUMMARY_TXT%" echo Console output: %CONSOLE_OUTPUT%
    >>"%SUMMARY_TXT%" echo Console warnings: %CONSOLE_WARNINGS%
    >>"%SUMMARY_TXT%" echo Build root: %BUILD_ROOT%
    >"%COLOR_SUMMARY%" echo %ESC%[36mLoom compile audit color summary%ESC%[0m
    >>"%COLOR_SUMMARY%" echo %ESC%[36mVersion: %LOOM_TESTS_VERSION%%ESC%[0m
    >"%WARNINGS_ALL%" echo Loom compile warnings
    >"%WARNINGS_UNIQUE%" echo Loom unique warning lines
    >"%WARNINGS_LOOM%" echo Loom-owned compile warnings
    >"%WARNINGS_LOOM_UNIQUE%" echo Loom-owned unique warning lines
    >"%WARNINGS_EXTERNAL%" echo External/core compile warnings
    >"%WARNINGS_EXTERNAL_UNIQUE%" echo External/core unique warning lines
    >"%ERRORS_ALL%" echo Loom compile errors
)

echo.
echo ================ compile run ================
echo Running %MODE_LABEL% from Loom\tests.
echo Version: %LOOM_TESTS_VERSION%
echo Console output: %CONSOLE_OUTPUT%
echo Console warnings: %CONSOLE_WARNINGS%  ^(default hides all warning detail from console but still counts/logs it^)
echo Result states: PASS, PASS+WARN, FAIL, or STOPPED.
echo Arduino CLI: %ARDUINO_CLI%
echo Examples: %EXAMPLES_ROOT%
echo FQBN: %FQBN%
echo Warnings passed to compiler: %WARNINGS%
echo Warning detail in console: %CONSOLE_WARNINGS% ^(counts still shown; audit modes save raw logs plus warnings_all/loom/external.txt^)
echo Package libraries: %PACKAGE_LIBS%
echo Build root: %BUILD_ROOT%
if exist "%SKETCHBOOK_LIBS%" echo Sketchbook libraries: %SKETCHBOOK_LIBS%
echo.
echo Interrupt behavior: Ctrl-C stops the current compile, then this script asks whether to stop all remaining sketches.
if "%SAVE_REPORT%"=="1" (
    echo Report folder: %OUT_ROOT%
    echo Raw per-sketch logs: %LOG_DIR%
    echo Warning lines saved to: %WARNINGS_ALL%
    echo Loom-owned warning lines saved to: %WARNINGS_LOOM%
    echo External/core warning lines saved to: %WARNINGS_EXTERNAL%
    echo Unique warning lines saved to: %WARNINGS_UNIQUE%
    echo Raw full per-sketch compiler logs are saved in: %LOG_DIR%
) else (
    echo No permanent report files are saved by this mode.
)
echo.

set "RAW_DIRS=%OUT_ROOT%\sketch_dirs_raw.txt"
set "SKETCH_DIRS=%OUT_ROOT%\sketch_dirs.txt"
if exist "%RAW_DIRS%" del /q "%RAW_DIRS%" >nul 2>nul
for /r "%EXAMPLES_ROOT%" %%F in (*.ino) do (
    echo %%~dpF>>"%RAW_DIRS%"
)
if not exist "%RAW_DIRS%" (
    call :Red "ERROR: No .ino sketches were found under: %EXAMPLES_ROOT%"
    call :Cleanup
    call :MaybePause
    exit /b 1
)
sort "%RAW_DIRS%" /unique > "%SKETCH_DIRS%"
for /f %%C in ('find /c /v "" ^< "%SKETCH_DIRS%"') do set "TOTAL=%%C"
if "%TOTAL%"=="0" (
    call :Red "ERROR: No sketch folders were found under: %EXAMPLES_ROOT%"
    call :Cleanup
    call :MaybePause
    exit /b 1
)

echo Found %TOTAL% sketch folders.
echo.

set /a PASS_COUNT=0
set /a WARN_COUNT=0
set /a FAIL_COUNT=0
set /a STOP_COUNT=0
set /a INDEX=0
set "FINAL_RC=0"
set "RUN_STOPPED=0"

for /f "usebackq delims=" %%D in ("%SKETCH_DIRS%") do (
    set "SKETCH_DIR=%%D"
    if "!SKETCH_DIR:~-1!"=="\" set "SKETCH_DIR=!SKETCH_DIR:~0,-1!"
    for %%A in ("!SKETCH_DIR!") do set "SKETCH_NAME=%%~nxA"
    set /a INDEX+=1
    call :SafeName "!SKETCH_NAME!" SAFE_NAME
    set "PAD=000!INDEX!"
    set "PAD=!PAD:~-3!"
    set "LOG_FILE=%LOG_DIR%\!PAD!_!SAFE_NAME!.log"
    set "BUILD_DIR=%BUILD_ROOT%\!PAD!_!SAFE_NAME!"
    mkdir "!BUILD_DIR!" >nul 2>nul

    echo [!INDEX!/%TOTAL%] !SKETCH_NAME! - compiling...
    echo Sketch: !SKETCH_DIR!
    call :NowCs START_CS
    pushd "!SKETCH_DIR!" >nul
    if exist "%SKETCHBOOK_LIBS%" (
        "%ARDUINO_CLI%" compile --fqbn "%FQBN%" --warnings "%WARNINGS%" --jobs 0 --libraries "%PACKAGE_LIBS%" --libraries "%SKETCHBOOK_LIBS%" --build-path "!BUILD_DIR!" . > "!LOG_FILE!" 2>&1
    ) else (
        "%ARDUINO_CLI%" compile --fqbn "%FQBN%" --warnings "%WARNINGS%" --jobs 0 --libraries "%PACKAGE_LIBS%" --build-path "!BUILD_DIR!" . > "!LOG_FILE!" 2>&1
    )
    set "EXITCODE=!ERRORLEVEL!"
    popd >nul
    call :NowCs END_CS
    call :ElapsedSeconds !START_CS! !END_CS! ELAPSED
    set "INTERRUPTED=0"
    if "!EXITCODE!"=="-1073741510" set "INTERRUPTED=1"
    if "!EXITCODE!"=="3221225786" set "INTERRUPTED=1"

    call :CountWarnings "!LOG_FILE!" WARNINGS_FOUND
    call :CountLoomWarnings "!LOG_FILE!" "!BUILD_DIR!" "!SKETCH_DIR!" LOOM_WARNINGS_FOUND
    set /a EXTERNAL_WARNINGS_FOUND=WARNINGS_FOUND-LOOM_WARNINGS_FOUND
    call :CountErrors "!LOG_FILE!" ERRORS_FOUND

    set "RESULT=PASS"
    if "!INTERRUPTED!"=="1" (
        set "RESULT=STOPPED"
    ) else (
        if not "!EXITCODE!"=="0" set "RESULT=FAIL"
        if "!EXITCODE!"=="0" if !WARNINGS_FOUND! GTR 0 set "RESULT=PASS+WARN"
    )

    if "!RESULT!"=="PASS" set /a PASS_COUNT+=1
    if "!RESULT!"=="PASS+WARN" set /a WARN_COUNT+=1
    if "!RESULT!"=="FAIL" (
        set /a FAIL_COUNT+=1
        set "FINAL_RC=1"
    )
    if "!RESULT!"=="STOPPED" (
        set /a STOP_COUNT+=1
        set "FINAL_RC=130"
    )
    if "%STRICT_WARNINGS%"=="1" if "!RESULT!"=="PASS+WARN" set "FINAL_RC=1"

    call :PrintStatus "!RESULT!" "!SKETCH_NAME!" "!EXITCODE!" "!ELAPSED!" "!WARNINGS_FOUND!" "!ERRORS_FOUND!" "!LOOM_WARNINGS_FOUND!" "!EXTERNAL_WARNINGS_FOUND!"
    if not "!RESULT!"=="STOPPED" call :PrintLogToConsole "!LOG_FILE!" "!BUILD_DIR!" "!SKETCH_DIR!" "!WARNINGS_FOUND!" "!LOOM_WARNINGS_FOUND!" "!EXTERNAL_WARNINGS_FOUND!"
    echo.

    if "%SAVE_REPORT%"=="1" (
        >>"%REPORT_CSV%" echo !INDEX!,!RESULT!,!EXITCODE!,!WARNINGS_FOUND!,!LOOM_WARNINGS_FOUND!,!EXTERNAL_WARNINGS_FOUND!,!ERRORS_FOUND!,!ELAPSED!,"!SKETCH_NAME!","!SKETCH_DIR!","!LOG_FILE!","!BUILD_DIR!"
        >>"%SUMMARY_TXT%" echo [!RESULT!] !SKETCH_NAME! exit=!EXITCODE! time=!ELAPSED!s warnings=!WARNINGS_FOUND! loom_warnings=!LOOM_WARNINGS_FOUND! external_warnings=!EXTERNAL_WARNINGS_FOUND! errors=!ERRORS_FOUND!
        call :WriteColorSummary "!RESULT!" "!SKETCH_NAME!" "!EXITCODE!" "!ELAPSED!" "!WARNINGS_FOUND!" "!ERRORS_FOUND!" "!LOOM_WARNINGS_FOUND!" "!EXTERNAL_WARNINGS_FOUND!"
        if !WARNINGS_FOUND! GTR 0 (
            >>"%WARNINGS_ALL%" echo.
            >>"%WARNINGS_ALL%" echo ===== !INDEX! !SKETCH_NAME! =====
            call :AppendWarnings "!LOG_FILE!" "%WARNINGS_ALL%" all "!BUILD_DIR!" "!SKETCH_DIR!"
        )
        if !LOOM_WARNINGS_FOUND! GTR 0 (
            >>"%WARNINGS_LOOM%" echo.
            >>"%WARNINGS_LOOM%" echo ===== !INDEX! !SKETCH_NAME! =====
            call :AppendWarnings "!LOG_FILE!" "%WARNINGS_LOOM%" loom "!BUILD_DIR!" "!SKETCH_DIR!"
        )
        if !EXTERNAL_WARNINGS_FOUND! GTR 0 (
            >>"%WARNINGS_EXTERNAL%" echo.
            >>"%WARNINGS_EXTERNAL%" echo ===== !INDEX! !SKETCH_NAME! =====
            call :AppendWarnings "!LOG_FILE!" "%WARNINGS_EXTERNAL%" external "!BUILD_DIR!" "!SKETCH_DIR!"
        )
        if !ERRORS_FOUND! GTR 0 (
            >>"%ERRORS_ALL%" echo.
            >>"%ERRORS_ALL%" echo ===== !INDEX! !SKETCH_NAME! =====
            findstr /i /c:"error:" /c:"fatal error:" /c:"undefined reference" /c:"collect2.exe" /c:"ld.exe" /c:"Compilation error:" /c:"Can't open sketch" /c:"no valid sketch" /c:"main file missing" /c:"Error during build" "!LOG_FILE!" >>"%ERRORS_ALL%" 2>nul
        )
    )

    if "%SAVE_ARTIFACTS%"=="1" if "!EXITCODE!"=="0" (
        for /r "!BUILD_DIR!" %%A in (*.bin *.hex *.elf *.map *.uf2 *.eep) do (
            copy /y "%%~fA" "%COMPLETE_DIR%\!PAD!_!SAFE_NAME!_%%~nxA" >nul 2>nul
        )
    )

    if "%KEEP_BUILD%"=="0" rmdir /s /q "!BUILD_DIR!" >nul 2>nul

    if "!INTERRUPTED!"=="1" (
        call :AskStopAfterInterrupt "!SKETCH_NAME!"
        if "!STOP_AFTER_INTERRUPT!"=="1" (
            set "RUN_STOPPED=1"
            goto :CompileLoopComplete
        )
    )
)

:CompileLoopComplete
echo.
echo ================ summary ================
echo Total sketches: %TOTAL%
echo Processed sketches: %INDEX%
call :Green "PASS: %PASS_COUNT%"
call :Yellow "PASS+WARN: %WARN_COUNT%"
call :Red "FAIL: %FAIL_COUNT%"
if %STOP_COUNT% GTR 0 call :Yellow "STOPPED: %STOP_COUNT%"
if "%RUN_STOPPED%"=="1" call :Yellow "Run stopped by user before all sketches were compiled."
echo Strict warnings: %STRICT_WARNINGS%
if "%SAVE_REPORT%"=="1" (
    call :WriteUniqueWarnings
    >>"%SUMMARY_TXT%" echo.
    >>"%SUMMARY_TXT%" echo Total sketches: %TOTAL%
    >>"%SUMMARY_TXT%" echo Processed sketches: %INDEX%
    >>"%SUMMARY_TXT%" echo PASS: %PASS_COUNT%
    >>"%SUMMARY_TXT%" echo PASS+WARN: %WARN_COUNT%
    >>"%SUMMARY_TXT%" echo FAIL: %FAIL_COUNT%
    >>"%SUMMARY_TXT%" echo STOPPED: %STOP_COUNT%
    if "%RUN_STOPPED%"=="1" >>"%SUMMARY_TXT%" echo Run stopped by user before all sketches were compiled.
    if defined UNIQUE_WARNING_COUNT >>"%SUMMARY_TXT%" echo Unique warning lines: !UNIQUE_WARNING_COUNT!
    if defined UNIQUE_LOOM_WARNING_COUNT >>"%SUMMARY_TXT%" echo Unique Loom-owned warning lines: !UNIQUE_LOOM_WARNING_COUNT!
    if defined UNIQUE_EXTERNAL_WARNING_COUNT >>"%SUMMARY_TXT%" echo Unique external/core warning lines: !UNIQUE_EXTERNAL_WARNING_COUNT!
    echo Report folder: %OUT_ROOT%
    echo CSV: %REPORT_CSV%
    echo Summary: %SUMMARY_TXT%
    echo Color summary: %COLOR_SUMMARY%
    echo Warnings: %WARNINGS_ALL%
    echo Unique warnings: %WARNINGS_UNIQUE%
    echo Loom-owned warnings: %WARNINGS_LOOM%
    echo Unique Loom-owned warnings: %WARNINGS_LOOM_UNIQUE%
    echo External/core warnings: %WARNINGS_EXTERNAL%
    echo Unique external/core warnings: %WARNINGS_EXTERNAL_UNIQUE%
    if defined UNIQUE_WARNING_COUNT echo Unique warning lines: !UNIQUE_WARNING_COUNT!
    if defined UNIQUE_LOOM_WARNING_COUNT echo Unique Loom-owned warning lines: !UNIQUE_LOOM_WARNING_COUNT!
    if defined UNIQUE_EXTERNAL_WARNING_COUNT echo Unique external/core warning lines: !UNIQUE_EXTERNAL_WARNING_COUNT!
    echo Errors: %ERRORS_ALL%
) else (
    call :Cleanup
)

if "%KEEP_BUILD%"=="0" if "%BUILD_ROOT_AUTO%"=="1" if exist "%BUILD_ROOT%" rmdir /s /q "%BUILD_ROOT%" >nul 2>nul

call :MaybePause
exit /b %FINAL_RC%

:Preflight
echo.
echo ================ tool and package preflight ================
echo Version: %LOOM_TESTS_VERSION%
echo Mode: %LOOM_COMPILE_MODE%
echo Auto install tools: %AUTO_INSTALL_TOOLS% ^(set AUTO_INSTALL_TOOLS=0 to disable^)

call :FindArduinoCli
if not defined ARDUINO_CLI (
    call :Yellow "Arduino CLI not found."
    if "%AUTO_INSTALL_TOOLS%"=="1" (
        where winget >nul 2>nul
        if errorlevel 1 (
            call :Red "ERROR: winget was not found. Install Arduino CLI or Arduino IDE, then run again."
            exit /b 1
        )
        echo Trying: winget install --id ArduinoSA.CLI --exact
        winget install --id ArduinoSA.CLI --exact --accept-package-agreements --accept-source-agreements
        call :FindArduinoCli
    )
)
if not defined ARDUINO_CLI (
    call :Red "ERROR: Arduino CLI still not found."
    exit /b 1
)
if not exist "%ARDUINO_CLI%" (
    call :Red "ERROR: Arduino CLI path does not exist: %ARDUINO_CLI%"
    exit /b 1
)
echo Arduino CLI: %ARDUINO_CLI%
echo Running: arduino-cli version
"%ARDUINO_CLI%" version
if errorlevel 1 call :Yellow "WARNING: arduino-cli version returned non-zero. Compile may still work if IDE-bundled CLI is odd."

echo Arduino data folder: %ARDUINO_DATA%
if not exist "%ARDUINO_DATA%" call :Yellow "WARNING: Arduino data folder does not exist yet: %ARDUINO_DATA%"

if not exist "%LOOM_DIR%" (
    call :Red "ERROR: Loom library folder was not found: %LOOM_DIR%"
    exit /b 1
)
echo OK: Loom library folder: %LOOM_DIR%

if not exist "%EXAMPLES_ROOT%" (
    call :Red "ERROR: Examples folder was not found: %EXAMPLES_ROOT%"
    exit /b 1
)
echo OK: Examples folder: %EXAMPLES_ROOT%

if not exist "%PACKAGE_ROOT%" (
    call :Red "ERROR: Package root was not found: %PACKAGE_ROOT%"
    exit /b 1
)
echo OK: Package root: %PACKAGE_ROOT%

if not exist "%PACKAGE_LIBS%" (
    call :Red "ERROR: Package libraries folder was not found: %PACKAGE_LIBS%"
    exit /b 1
)
echo OK: Package libraries: %PACKAGE_LIBS%

if exist "%SKETCHBOOK_LIBS%" (
    echo OK: Sketchbook libraries: %SKETCHBOOK_LIBS%
) else (
    echo NOTE: Sketchbook libraries folder not found: %SKETCHBOOK_LIBS%
)

set "BOARDS_FILE=%PACKAGE_ROOT%\boards.txt"
if not exist "%BOARDS_FILE%" (
    call :Yellow "WARNING: boards.txt not found at %BOARDS_FILE%"
) else (
    for /f "tokens=3 delims=:" %%B in ("%FQBN%") do set "BOARD_ID=%%B"
    findstr /b /c:"!BOARD_ID!.name=" "%BOARDS_FILE%" >nul 2>nul
    if errorlevel 1 (
        call :Yellow "WARNING: boards.txt did not contain board id !BOARD_ID! from FQBN %FQBN%"
    ) else (
        echo OK: boards.txt contains board from FQBN: %FQBN%
    )
)

echo Arduino installed cores:
"%ARDUINO_CLI%" core list
if errorlevel 1 call :Yellow "WARNING: arduino-cli core list returned non-zero, continuing because the package exists on disk."

echo Preflight complete.
exit /b 0

:FindArduinoCli
if defined ARDUINO_CLI if exist "%ARDUINO_CLI%" exit /b 0
set "ARDUINO_CLI="
set "IDE_CLI=%LOCALAPPDATA%\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
if exist "%IDE_CLI%" (
    set "ARDUINO_CLI=%IDE_CLI%"
    exit /b 0
)
for /f "delims=" %%P in ('where arduino-cli.exe 2^>nul') do (
    if not defined ARDUINO_CLI set "ARDUINO_CLI=%%P"
)
exit /b 0

:SanitizedTimestamp
set "_ts=%DATE%_%TIME%"
set "_ts=%_ts:/=-%"
set "_ts=%_ts:\=-%"
set "_ts=%_ts::=-%"
set "_ts=%_ts:.=-%"
set "_ts=%_ts:,=-%"
set "_ts=%_ts: =0%"
set "%~1=%_ts%"
exit /b 0

:NowCs
set "_t=%time: =0%"
set /a _cs=(1%_t:~0,2%-100)*360000 + (1%_t:~3,2%-100)*6000 + (1%_t:~6,2%-100)*100 + (1%_t:~9,2%-100)
set "%~1=%_cs%"
exit /b 0

:ElapsedSeconds
set /a _elapsed=%~2-%~1
if %_elapsed% LSS 0 set /a _elapsed+=8640000
set /a _whole=_elapsed/100
set "%~3=%_whole%"
exit /b 0

:SafeName
set "_s=%~1"
set "_s=%_s: =_%"
set "_s=%_s:(=_%"
set "_s=%_s:)=_%"
set "_s=%_s:[=_%"
set "_s=%_s:]=_%"
set "%~2=%_s%"
exit /b 0

:CountWarnings
set "%~2=0"
for /f %%C in ('findstr /i /c:"warning:" "%~1" 2^>nul ^| find /c /v ""') do set "%~2=%%C"
exit /b 0

:CountLoomWarnings
set "%~4=0"
for /f %%C in ('powershell -NoProfile -ExecutionPolicy Bypass -File "%TESTS_DIR%\loom_warning_filter.ps1" -Action count -Scope loom -LogPath "%~1" -LoomDir "%LOOM_DIR%" -BuildDir "%~2" -SketchDir "%~3" 2^>nul') do set "%~4=%%C"
exit /b 0

:CountErrors
set "%~2=0"
for /f %%C in ('findstr /i /c:"error:" /c:"fatal error:" /c:"undefined reference" /c:"collect2.exe" /c:"ld.exe" /c:"Compilation error:" /c:"Can't open sketch" /c:"no valid sketch" /c:"main file missing" /c:"Error during build" "%~1" 2^>nul ^| find /c /v ""') do set "%~2=%%C"
exit /b 0

:AppendWarnings
powershell -NoProfile -ExecutionPolicy Bypass -File "%TESTS_DIR%\loom_warning_filter.ps1" -Action append -Scope "%~3" -LogPath "%~1" -OutputPath "%~2" -LoomDir "%LOOM_DIR%" -BuildDir "%~4" -SketchDir "%~5" >nul 2>nul
exit /b 0

:WriteUniqueWarnings
set "UNIQUE_WARNING_COUNT=0"
set "UNIQUE_LOOM_WARNING_COUNT=0"
set "UNIQUE_EXTERNAL_WARNING_COUNT=0"
if not exist "%WARNINGS_ALL%" exit /b 0
>"%WARNINGS_UNIQUE%" echo Loom unique warning lines
findstr /i /c:"warning:" "%WARNINGS_ALL%" 2>nul | sort /unique >> "%WARNINGS_UNIQUE%"
for /f %%C in ('findstr /i /c:"warning:" "%WARNINGS_UNIQUE%" 2^>nul ^| find /c /v ""') do set "UNIQUE_WARNING_COUNT=%%C"
if exist "%WARNINGS_LOOM%" (
    >"%WARNINGS_LOOM_UNIQUE%" echo Loom-owned unique warning lines
    findstr /i /c:"warning:" "%WARNINGS_LOOM%" 2>nul | sort /unique >> "%WARNINGS_LOOM_UNIQUE%"
    for /f %%C in ('findstr /i /c:"warning:" "%WARNINGS_LOOM_UNIQUE%" 2^>nul ^| find /c /v ""') do set "UNIQUE_LOOM_WARNING_COUNT=%%C"
)
if exist "%WARNINGS_EXTERNAL%" (
    >"%WARNINGS_EXTERNAL_UNIQUE%" echo External/core unique warning lines
    findstr /i /c:"warning:" "%WARNINGS_EXTERNAL%" 2>nul | sort /unique >> "%WARNINGS_EXTERNAL_UNIQUE%"
    for /f %%C in ('findstr /i /c:"warning:" "%WARNINGS_EXTERNAL_UNIQUE%" 2^>nul ^| find /c /v ""') do set "UNIQUE_EXTERNAL_WARNING_COUNT=%%C"
)
exit /b 0

:PrintStatus
set "_result=%~1"
set "_line=[%~1] %~2  exit=%~3  time=%~4s  warnings=%~5  loom=%~7  external=%~8  errors=%~6"
if "%~1"=="PASS" call :Green "%_line%" & exit /b 0
if "%~1"=="PASS+WARN" call :Yellow "%_line%" & exit /b 0
if "%~1"=="FAIL" call :Red "%_line%" & exit /b 0
if "%~1"=="STOPPED" call :Yellow "%_line%" & exit /b 0
echo %_line%
exit /b 0

:WriteColorSummary
set "_result=%~1"
set "_line=[%~1] %~2  exit=%~3  time=%~4s  warnings=%~5  loom=%~7  external=%~8  errors=%~6"
if "%~1"=="PASS" >>"%COLOR_SUMMARY%" echo %ESC%[32m%_line%%ESC%[0m& exit /b 0
if "%~1"=="PASS+WARN" >>"%COLOR_SUMMARY%" echo %ESC%[33m%_line%%ESC%[0m& exit /b 0
if "%~1"=="FAIL" >>"%COLOR_SUMMARY%" echo %ESC%[31m%_line%%ESC%[0m& exit /b 0
if "%~1"=="STOPPED" >>"%COLOR_SUMMARY%" echo %ESC%[33m%_line%%ESC%[0m& exit /b 0
>>"%COLOR_SUMMARY%" echo %_line%
exit /b 0

:AskStopAfterInterrupt
set "STOP_AFTER_INTERRUPT=0"
echo.
call :Yellow "Ctrl-C detected while compiling %~1."
choice /c YN /n /m "Stop all remaining compilations? [Y/N] "
if errorlevel 2 (
    set "STOP_AFTER_INTERRUPT=0"
) else (
    set "STOP_AFTER_INTERRUPT=1"
)
exit /b 0

:PrintLogToConsole
set "_log=%~1"
set "_build=%~2"
set "_sketch=%~3"
if not exist "%_log%" (
    call :Red "Log file was not created: %_log%"
    exit /b 0
)
for %%Z in ("%_log%") do if %%~zZ EQU 0 (
    call :Yellow "Arduino CLI produced no stdout/stderr for this sketch."
    exit /b 0
)
if /i "%CONSOLE_OUTPUT%"=="none" exit /b 0

rem Warning output is noisy because GCC warnings include multi-line include
rem traces, source excerpts, caret lines, and note lines. External/core warning
rem lines are never printed unless CONSOLE_WARNINGS=all; they are still saved in
rem the raw per-sketch log and warnings_external.txt.
if /i "%CONSOLE_WARNINGS%"=="suppress" (
    call :PrintImportantNoWarnings "%_log%"
    exit /b 0
)

if /i "%CONSOLE_WARNINGS%"=="show" (
    call :PrintImportantNoWarnings "%_log%"
    call :PrintLoomWarnings "%_log%" "%_build%" "%_sketch%"
    exit /b 0
)

if /i "%CONSOLE_WARNINGS%"=="all" (
    if /i "%CONSOLE_OUTPUT%"=="important" (
        call :PrintImportantWithWarnings "%_log%"
        exit /b 0
    )
    type "%_log%"
    exit /b 0
)

call :PrintImportantNoWarnings "%_log%"
exit /b 0

:PrintImportantNoWarnings
set "_log=%~1"
findstr /i /c:"error:" /c:"fatal error:" /c:"undefined reference" /c:"collect2.exe" /c:"ld.exe" /c:"Compilation error:" /c:"Can't open sketch" /c:"no valid sketch" /c:"main file missing" /c:"Error during build" /c:"Sketch uses" /c:"Global variables" /c:"Maximum is" /c:"program storage" /c:"dynamic memory" "%_log%" 2>nul
exit /b 0

:PrintImportantWithWarnings
set "_log=%~1"
findstr /i /c:"error:" /c:"fatal error:" /c:"undefined reference" /c:"collect2.exe" /c:"ld.exe" /c:"Compilation error:" /c:"Can't open sketch" /c:"no valid sketch" /c:"main file missing" /c:"Error during build" /c:"Sketch uses" /c:"Global variables" /c:"Maximum is" /c:"program storage" /c:"dynamic memory" /c:"warning:" "%_log%" 2>nul
exit /b 0

:PrintLoomWarnings
set "_log=%~1"
set "_build=%~2"
set "_sketch=%~3"
powershell -NoProfile -ExecutionPolicy Bypass -File "%TESTS_DIR%\loom_warning_filter.ps1" -Action print -Scope loom -LogPath "%_log%" -LoomDir "%LOOM_DIR%" -BuildDir "%_build%" -SketchDir "%_sketch%" 2>nul
exit /b 0

:SetAnsi
for /F "tokens=1 delims=#" %%A in ('"prompt #$E# & echo on & for %%B in (1) do rem"') do set "ESC=%%A"
exit /b 0

:Green
echo %ESC%[32m%~1%ESC%[0m
exit /b 0
:Yellow
echo %ESC%[33m%~1%ESC%[0m
exit /b 0
:Red
echo %ESC%[31m%~1%ESC%[0m
exit /b 0

:Cleanup
if "%SAVE_REPORT%"=="0" if exist "%OUT_ROOT%" rmdir /s /q "%OUT_ROOT%" >nul 2>nul
exit /b 0

:MaybePause
if /i "%NO_PAUSE%"=="1" exit /b 0
echo.
echo Press any key to close.
pause >nul
exit /b 0
