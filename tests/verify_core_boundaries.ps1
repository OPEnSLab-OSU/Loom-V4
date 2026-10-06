param(
    [string] $VsWhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe',
    [string[]] $TestName = @()
)

$ErrorActionPreference = 'Stop'
$vsRoot = & $VsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ([string]::IsNullOrWhiteSpace($vsRoot)) {
    throw 'MSVC C++ Build Tools are required for this Windows runner.'
}
$vcVars = Join-Path $vsRoot 'VC/Auxiliary/Build/vcvars64.bat'
$buildDir = Join-Path ([System.IO.Path]::GetTempPath()) ('loom-core-boundaries-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $buildDir | Out-Null
$loomIncludes = Join-Path (Split-Path -Parent $PSScriptRoot) 'src'
$jsonIncludes = Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'ArduinoJson/src'
$sourceDir = Join-Path $PSScriptRoot 'Core_Boundaries'
$tests = @(Get-ChildItem -LiteralPath $sourceDir -Filter 'test_*.cpp' | Sort-Object Name)
if ($tests.Count -eq 0) { throw 'No boundary tests found.' }
if ($TestName.Count) {
    $unknown = @($TestName | Where-Object { $_ -notin $tests.BaseName })
    if ($unknown.Count) { throw "Unknown boundary programs: $($unknown -join ', ')" }
    $tests = @($tests | Where-Object { $_.BaseName -in $TestName })
}
$failed = @()
foreach ($test in $tests) {
    $testExe = Join-Path $buildDir ($test.BaseName + '.exe')
    $testObj = Join-Path $buildDir ($test.BaseName + '.obj')
    $log = Join-Path $buildDir ($test.BaseName + '.log')
    $fakeIncludes = ''
    if ($test.BaseName -in @('test_ads1115', 'test_analog', 'test_reed_anemometer', 'test_sen66_startup', 'test_df_gas_retained')) {
        $fakeIncludes = '/I"' + (Join-Path $sourceDir 'sensor_fakes') + '" /I"' + $jsonIncludes + '"'
    } elseif ($test.BaseName -eq 'test_mongo_batch') {
        $fakeIncludes = '/I"' + (Join-Path $sourceDir 'mongo_fakes') + '" /I"' + (Join-Path $sourceDir 'mqtt_fakes') + '" /I"' + $jsonIncludes + '"'
    } elseif ($test.BaseName -in @('test_mqtt_component', 'test_thingspeak', 'test_remote_manager')) {
        $fakeIncludes = '/I"' + (Join-Path $sourceDir 'mqtt_fakes') + '" /I"' + $jsonIncludes + '"'
    } elseif ($test.BaseName -in @('test_lora_packet_header', 'test_buffer_pool', 'test_gnss_metadata', 'test_heartbeat_payload')) {
        $fakeIncludes = '/I"' + $jsonIncludes + '"'
    } elseif ($test.BaseName -in @('test_manager_lifecycle', 'test_function_start', 'test_function_start_off')) {
        $fakeIncludes = '/I"' + (Join-Path $sourceDir 'manager_fakes') + '" /I"' + $jsonIncludes + '"'
    } elseif ($test.BaseName -eq 'test_watchdog_pause') {
        $fakeIncludes = '/I"' + (Join-Path $sourceDir 'fakes') + '"'
    } elseif ($test.BaseName -eq 'test_as5311') {
        $fakeIncludes = '/I"' + (Join-Path $sourceDir 'as5311_fakes') + '"'
    } elseif ($test.BaseName -eq 'test_debug_sketch') {
        $fakeIncludes = '/I"' + (Join-Path $sourceDir 'debug_sketch_fakes') + '"'
    } elseif ($test.BaseName -eq 'test_trace_auto') {
        $fakeIncludes = '/I"' + (Join-Path $sourceDir 'trace_auto_fakes') + '" /I"' + (Join-Path $sourceDir 'manager_fakes') + '" /I"' + $jsonIncludes + '"'
    } elseif ($test.BaseName -eq 'test_trace') {
        $fakeIncludes = '/I"' + (Join-Path $sourceDir 'trace_fakes') + '"'
    }
    # Assertions remain enabled. Each program exercises production helpers or readers.
    # Keep logs/products for diagnosis; never delete a caller-supplied directory.
    $runCommands = "`"$testExe`""
    if ($test.BaseName -eq 'test_trace_auto') {
        foreach ($scenario in @('immediate', 'failed', 'append-failed', 'manual')) {
            $runCommands += " && `"$testExe`" $scenario"
        }
    }
    $command = "call `"$vcVars`" >nul && cl /nologo /EHsc /std:c++14 /Zc:__cplusplus /D_CRT_SECURE_NO_WARNINGS /W4 /UNDEBUG $fakeIncludes /I`"$loomIncludes`" `"$($test.FullName)`" /Fe:`"$testExe`" /Fo:`"$testObj`" && $runCommands"
    if ($test.BaseName -eq 'test_debug_sketch') {
        foreach ($flags in @('/DLOOM_TRACE=1 /DLOOM_DEBUG_MEMORY=0', '/DLOOM_TRACE=0 /DLOOM_DEBUG_MEMORY=1', '/DLOOM_TRACE=1 /DLOOM_DEBUG_MEMORY=1')) {
            $command += " && cl /nologo /EHsc /std:c++14 /W4 /UNDEBUG $flags $fakeIncludes /I`"$loomIncludes`" `"$($test.FullName)`" /Fe:`"$testExe`" /Fo:`"$testObj`" && `"$testExe`""
        }
    }
    & $env:ComSpec /d /s /c $command 2>&1 | Tee-Object -FilePath $log
    if ($LASTEXITCODE -ne 0) {
        $failed += $test.BaseName
        Write-Output "FAIL: $($test.BaseName)"
    } else {
        Write-Output "PASS: $($test.BaseName)"
    }
}
Write-Output "Test build directory: $buildDir"
if ($failed.Count) { throw "Boundary regressions failed: $($failed -join ', ')" }
Write-Output "All $($tests.Count) boundary programs passed."
