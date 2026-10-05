param(
    [ValidateSet('sketch', 'off', 'calls', 'heap')] [string] $Mode = 'off',
    [ValidateSet('sketch', 'off')] [string] $Diagnostics = 'sketch',
    [ValidateSet('sketch', 'all', 'selected')] [string] $MuxDrivers = 'sketch',
    [string] $MuxAddresses = '',
    [string] $Sketch = '',
    [string] $ArduinoCli = "$env:LOCALAPPDATA\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe",
    [string] $Fqbn = 'loom4:samd:adafruit_feather_m0:usbstack=arduino,debug=off',
    [switch] $Upload,
    [string] $Port = ''
)

# Compile by default; upload only when explicitly requested with a port. Never alter the
# installed platform. A new temporary build path
# prevents objects from a different trace toggle being reused with incompatible class layouts.
$ErrorActionPreference = 'Stop'
if ($Upload -and [string]::IsNullOrWhiteSpace($Port)) {
    throw 'Specify -Port with -Upload (for example, -Port COM5). No firmware was uploaded.'
}
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
$cppFlags = "-DLOOM_TRACE=$enabled -DLOOM_TRACE_HEAP=$heap -DLOOM_TRACE_LINKER_HEAP_HOOKS=$heap"
if ($Mode -eq 'sketch') { $cppFlags = '' }
if ($Diagnostics -eq 'off') {
    $cppFlags += ' -DLOOM_DEBUG_TEXT=0 -DLOOM_DEBUG_SD_LOG=0 -DLOOM_DEBUG_DIAGNOSTICS=0 -DLOOM_DEBUG_MEMORY=0 -DLOOM_DEBUG_MUX_SCAN=0 -DLOOM_DEBUG_SD_WRITES=0 -DLOOM_DEBUG_PRINT_SAMPLES=0 -DLOOM_COMPILE_MUX_DEBUG=0 -DLOOM_COMPILE_SD_WRITE_DEBUG=0'
}
if ($MuxDrivers -eq 'all') { $cppFlags += ' -DLOOM_MUX_FORCE_ALL_DRIVERS=1' }
if ($MuxDrivers -eq 'selected') {
    $supported = @(0x10, 0x11, 0x15, 0x1C, 0x1D, 0x29, 0x36, 0x44, 0x45,
                   0x48, 0x49, 0x69, 0x6B, 0x70, 0x74, 0x75, 0x76, 0x77)
    $selectedAddresses = @($MuxAddresses -split ',' | ForEach-Object {
        $address = $_.Trim()
        if ($address -notmatch '^0[xX][0-9a-fA-F]{1,2}$') {
            throw 'Specify -MuxAddresses as comma-separated hexadecimal sensor addresses, for example 0x74,0x6B,0x44.'
        }
        $value = [Convert]::ToByte($address.Substring(2), 16)
        if ($value -notin $supported) { throw "Unsupported mux sensor address: $address" }
        '0x{0:X2}' -f $value
    })
    if (($selectedAddresses | Select-Object -Unique).Count -ne $selectedAddresses.Count) {
        throw 'Mux sensor addresses must not be duplicated.'
    }
    $cppFlags += ' -DLOOM_MUX_FORCE_ALL_DRIVERS=0 -DLOOM_MUX_COMPILED_ADDRESSES=' + ($selectedAddresses -join ',')
} elseif ($MuxAddresses) {
    throw '-MuxAddresses requires -MuxDrivers selected.'
}
$properties = @('compile', '--fqbn', $Fqbn, '--warnings', 'all', '--jobs', '4',
    '--build-path', $build,
    '--libraries', (Split-Path -Parent $loomRoot),
    '--libraries', (Join-Path $env:USERPROFILE 'Documents/Arduino/libraries'))
if (-not [string]::IsNullOrWhiteSpace($cppFlags)) {
    $properties += @('--build-property', "compiler.cpp.extra_flags=$cppFlags")
}
if ($heap) {
    $symbols = @('malloc', 'calloc', 'realloc', 'free', '_malloc_r', '_calloc_r', '_realloc_r', '_free_r')
    $linkFlags = '-Wl,' + (($symbols | ForEach-Object { '--wrap=' + $_ }) -join ',')
    $properties += @('--build-property', "compiler.c.elf.extra_flags=$linkFlags")
}
# Sketch-local source files may live in Arduino's recursively compiled src folder.
if (Test-Path -LiteralPath (Join-Path $source 'src') -PathType Container) {
    Copy-Item -LiteralPath (Join-Path $source 'src') -Destination (Join-Path $stage 'src') -Recurse
}
# Preserve the exact flags with the output, so the matching ELF can symbolize caller addresses.
[ordered]@{ mode = $Mode; diagnostics = $Diagnostics; muxDrivers = $MuxDrivers;
            muxAddresses = $MuxAddresses; sketch = $source; board = $Fqbn; arguments = $properties } |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $buildRoot 'trace-build.json')
$properties += $stage
Write-Output "Building optional trace mode: $Mode"
& $ArduinoCli @properties 2>&1 | Tee-Object -FilePath (Join-Path $buildRoot 'compile.log') |
    Where-Object { $_ -match 'error:|undefined reference|Sketch uses|Global variables use|Compilation error|Error during build' }
$compileExit = $LASTEXITCODE
Write-Output "Trace build, log, and matching ELF: $buildRoot"
if ($compileExit -ne 0) { throw "Trace build failed (exit $compileExit)." }
if ($Upload) {
    # Upload the exact binary just verified. An ordinary IDE upload would rebuild with the
    # sketch's own flags and omit the heap linker hooks.
    Write-Output "Uploading verified trace mode $Mode to $Port"
    & $ArduinoCli upload --fqbn $Fqbn --port $Port --input-dir $build 2>&1 |
        Tee-Object -FilePath (Join-Path $buildRoot 'upload.log')
    if ($LASTEXITCODE -ne 0) { throw "Firmware upload failed (exit $LASTEXITCODE)." }
} else {
    Write-Output 'Compile only: firmware has not been uploaded. Use -Upload -Port COMx to upload this mode.'
}
