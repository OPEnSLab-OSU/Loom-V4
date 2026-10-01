param(
    [ValidateSet('off', 'calls', 'heap')] [string] $Mode = 'off',
    [string] $Sketch = '',
    [string] $ArduinoCli = "$env:LOCALAPPDATA\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe",
    [string] $Fqbn = 'loom4:samd:adafruit_feather_m0:usbstack=arduino,debug=off'
)

# Compile only; never upload or alter the installed platform. A new temporary build path
# prevents objects from a different trace toggle being reused with incompatible class layouts.
$ErrorActionPreference = 'Stop'
$loomRoot = Split-Path -Parent $PSScriptRoot
if (-not $Sketch) {
    $Sketch = Join-Path $loomRoot 'examples/Lab Examples/Wisp/WispV2_Deploy_2026_debug'
}
$source = (Resolve-Path -LiteralPath $Sketch).Path
$main = Join-Path $source ((Split-Path -Leaf $source) + '.ino')
if (-not (Test-Path -LiteralPath $main -PathType Leaf)) { throw 'Missing sketch main file.' }
if (-not (Test-Path -LiteralPath $ArduinoCli -PathType Leaf)) { throw 'Arduino CLI was not found.' }

$buildRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('loom-trace-' + [guid]::NewGuid().ToString('N'))
$stage = Join-Path $buildRoot 'LoomTraceBuild'
$build = Join-Path $buildRoot 'build'
New-Item -ItemType Directory -Path $stage, $build | Out-Null
foreach ($file in Get-ChildItem -LiteralPath $source -File) {
    if ($file.Extension -in @('.ino', '.cpp', '.c', '.h', '.hpp', '.S')) {
        $name = if ($file.FullName -eq $main) { 'LoomTraceBuild.ino' } else { $file.Name }
        Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $stage $name)
    }
}
$enabled = if ($Mode -eq 'off') { 0 } else { 1 }
$heap = if ($Mode -eq 'heap') { 1 } else { 0 }
$cppFlags = "-DLOOM_ENABLE_TRACE=$enabled -DLOOM_WISP_TRACE=$enabled -DLOOM_WISP_TRACE_HEAP=$heap"
$properties = @('compile', '--fqbn', $Fqbn, '--warnings', 'all', '--jobs', '4',
    '--build-path', $build, '--build-property', "compiler.cpp.extra_flags=$cppFlags",
    '--libraries', (Split-Path -Parent $loomRoot),
    '--libraries', (Join-Path $env:USERPROFILE 'Documents/Arduino/libraries'))
if ($heap) {
    $symbols = @('malloc', 'calloc', 'realloc', 'free', '_malloc_r', '_calloc_r', '_realloc_r', '_free_r')
    $linkFlags = '-Wl,' + (($symbols | ForEach-Object { '--wrap=' + $_ }) -join ',')
    $properties += @('--build-property', "compiler.c.elf.extra_flags=$linkFlags")
}
# Preserve the exact flags with the output, so the matching ELF can symbolize caller addresses.
[ordered]@{ mode = $Mode; sketch = $source; board = $Fqbn; arguments = $properties } |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $buildRoot 'trace-build.json')
$properties += $stage
Write-Output "Building optional trace mode: $Mode"
& $ArduinoCli @properties 2>&1 | Tee-Object -FilePath (Join-Path $buildRoot 'compile.log') |
    Where-Object { $_ -match 'error:|undefined reference|Sketch uses|Global variables use|Compilation error|Error during build' }
$compileExit = $LASTEXITCODE
Write-Output "Trace build, log, and matching ELF: $buildRoot"
if ($compileExit -ne 0) { throw "Trace build failed (exit $compileExit)." }
