param(
    [string] $ArduinoJsonSource,
    [string] $VsWhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
)

$ErrorActionPreference = 'Stop'
$loomRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ArduinoJsonSource)) {
    $ArduinoJsonSource = Join-Path (Split-Path -Parent $loomRoot) 'ArduinoJson/src'
}
if (-not (Test-Path -LiteralPath (Join-Path $ArduinoJsonSource 'ArduinoJson.h'))) {
    throw "ArduinoJson headers not found: $ArduinoJsonSource"
}
$vsRoot = & $VsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ([string]::IsNullOrWhiteSpace($vsRoot)) {
    throw 'MSVC C++ Build Tools are required for this Windows runner.'
}
$vcVars = Join-Path $vsRoot 'VC/Auxiliary/Build/vcvars64.bat'
$buildDir = Join-Path ([System.IO.Path]::GetTempPath()) ('loom-data-safety-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $buildDir | Out-Null
$testSource = Join-Path $PSScriptRoot 'data_safety_regression.cpp'
$loomIncludes = Join-Path $loomRoot 'src'
$testExe = Join-Path $buildDir 'data_safety.exe'
$testObj = Join-Path $buildDir 'data_safety.obj'

# Compile only the production allocation-free helpers and ArduinoJson, then inject file faults.
# All compiler products go in a new temporary directory; no board or dependency is modified.
$command = "call `"$vcVars`" >nul && cl /nologo /EHsc /std:c++14 /W4 /I`"$loomIncludes`" /I`"$ArduinoJsonSource`" `"$testSource`" /Fe:`"$testExe`" /Fo:`"$testObj`" && `"$testExe`""
& $env:ComSpec /d /s /c $command
$testExit = $LASTEXITCODE
Write-Output "Test build directory: $buildDir"
if ($testExit -ne 0) {
    throw "Data safety regression failed with exit code $testExit"
}
