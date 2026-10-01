param([string] $DeploymentFolder)

$ErrorActionPreference = 'Stop'
$wispRoot = Join-Path (Split-Path -Parent $PSScriptRoot) 'examples\Lab Examples\Wisp'
$names = @('Wisp_Batch_Logging', 'Wisp_Mux_BatchLogging', 'WispV2_Deploy_2026')

function Get-ComparableCode([string] $text) {
    $lines = foreach ($line in ($text -split '\r?\n')) {
        $trimmed = $line.Trim()
        if ($trimmed -and -not $trimmed.StartsWith('//')) { $trimmed }
    }
    return (($lines -join [Environment]::NewLine) -replace '(Watchdog\.reset\(\);\s*){2,}', ('Watchdog.reset();' + [Environment]::NewLine)).Trim()
}

function Test-SketchPair([string] $cleanPath, [string] $debugPath) {
    $clean = [IO.File]::ReadAllText($cleanPath) -replace '\r\n', [string][char]10
    $debug = [IO.File]::ReadAllText($debugPath) -replace '\r\n', [string][char]10
    $projected = $debug -replace '(?s)// BEGIN LOOM_BETA_DIAGNOSTICS\n.*?// END LOOM_BETA_DIAGNOSTICS\n', ''
    $projected = $projected -replace '(?m)^.*// LOOM_BETA_DIAGNOSTIC.*\n', ''
    $projected = $projected -replace '(?s)// BEGIN LOOM_TRACE_DIAGNOSTICS\n.*?// END LOOM_TRACE_DIAGNOSTICS\n', ''
    $projected = $projected -replace '(?m)^.*// LOOM_TRACE_DIAGNOSTIC.*\n', ''
    $projected = $projected.Replace('ENABLE_SD_LOGGING;', 'Logger::getInstance()->setDebugOutput(false);')
    $projected = $projected.Replace('manager.beginSerial();', 'manager.beginSerial(false);')
    $projected = $projected -replace '(?m)^\s*manager\.display_data\(\);\n', ''
    if ((Get-ComparableCode $clean) -cne (Get-ComparableCode $projected)) {
        throw "Operational code/configuration drift between $cleanPath and $debugPath"
    }
    Write-Host "PASS clean/debug parity: $(Split-Path -Leaf $cleanPath)"
}

if (Test-Path -LiteralPath (Join-Path $wispRoot 'examples')) {
    throw 'The obsolete nested Wisp examples folder has returned.'
}
foreach ($name in $names) {
    Test-SketchPair (Join-Path $wispRoot "$name\$name.ino") (Join-Path $wispRoot "$($name)_debug\$($name)_debug.ino")
}
if ($DeploymentFolder) {
    Test-SketchPair (Join-Path $DeploymentFolder 'WispV2_Deploy_2026.ino') (Join-Path $DeploymentFolder 'WispV2_Deploy_2026_debug\WispV2_Deploy_2026_debug.ino')
}
Write-Host 'All Wisp pairs preserve the same operational code and configuration.'
