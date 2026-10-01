param(
    [ValidateSet('sketch', 'off', 'calls', 'heap')] [string] $Mode = 'off',
    [string] $Sketch = '',
    [string] $ArduinoCli = "$env:LOCALAPPDATA\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe",
    [string] $Fqbn = 'loom4:samd:adafruit_feather_m0:usbstack=arduino,debug=off',
    [switch] $Upload,
    [string] $Port = ''
)
# Compatibility entry point. Use build_loom_trace.ps1 for any Loom sketch.
& (Join-Path $PSScriptRoot 'build_loom_trace.ps1') @PSBoundParameters
