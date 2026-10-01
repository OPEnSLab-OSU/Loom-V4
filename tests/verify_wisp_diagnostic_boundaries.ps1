param([string] $DeploymentFolder)

$ErrorActionPreference = 'Stop'
$loomRoot = Split-Path -Parent $PSScriptRoot
$wispRoot = Join-Path $loomRoot 'examples\Lab Examples\Wisp'
foreach ($header in @('src\Logger.h', 'src\Diagnostics\Loom_MemoryDiagnostics.h')) {
    if (-not (Test-Path -LiteralPath (Join-Path $loomRoot $header) -PathType Leaf)) {
        throw "Missing canonical debug header: $header"
    }
}
$sketches = @(Get-ChildItem -LiteralPath $wispRoot -Filter '*.ino' -Recurse |
    Where-Object { $_.FullName -notmatch '[\\/]\.loom-build[\\/]' })
if ($DeploymentFolder) {
    $sketches += Get-ChildItem -LiteralPath $DeploymentFolder -Filter '*.ino' -Recurse |
        Where-Object { $_.FullName -notmatch '[\\/]\.loom-build[\\/]' }
}
if ($sketches.Count -lt 6) { throw 'Expected all three clean/debug Wisp pairs.' }

foreach ($sketch in $sketches) {
    if ($sketch.BaseName -cne $sketch.Directory.Name) {
        throw "Arduino main sketch/folder name mismatch: $($sketch.FullName)"
    }
    $text = [IO.File]::ReadAllText($sketch.FullName)
    if ($text -notmatch '(?m)^\s*#include\s*[<"]Logger\.h[>"]') {
        throw "Sketch must explicitly include its logger header: $($sketch.FullName)"
    }
    foreach ($required in @('hypnos.setWakeWatchdogTimeout(ACTIVE_WATCHDOG_MS)',
                            'const bool networkWindow = batchSD.shouldPublish()',
                            'Watchdog.disable();', 'sd->retryBatch()', 'hypnos.logToSD()')) {
        if (-not $text.Contains($required)) {
            throw "Missing production safeguard '$required': $($sketch.FullName)"
        }
    }
    if (-not $sketch.BaseName.EndsWith('_debug')) {
        if ($text -match 'LOOM_BETA_DIAGNOSTIC|WISP_DIAGNOSTIC_|Loom_MemoryDiagnostics|LOOM_WISP_TRACE|WISP_TRACE_|executionTrace|ENABLE_SD_LOGGING|manager\.display_data\(') {
            throw "Debug instrumentation escaped into the quiet sketch: $($sketch.FullName)"
        }
        if (-not $text.Contains('Logger::getInstance()->setDebugOutput(false)')) {
            throw "Quiet sketch does not suppress routine logger output: $($sketch.FullName)"
        }
        Write-Host "PASS quiet boundary: $($sketch.Name)"
        continue
    }

    if ($text -notmatch '(?m)^\s*#include\s*[<"]Diagnostics/Loom_MemoryDiagnostics\.h[>"]') {
        throw "Debug sketch must explicitly include the canonical diagnostic header: $($sketch.FullName)"
    }
    $inside = $false
    $begins = 0
    $ends = 0
    $calls = 0
    foreach ($line in ($text -split '\r?\n')) {
        if ($line.Contains('// BEGIN LOOM_BETA_DIAGNOSTICS')) {
            if ($inside) { throw "Nested diagnostic block: $($sketch.FullName)" }
            $inside = $true
            $begins++
        } elseif ($line.Contains('// END LOOM_BETA_DIAGNOSTICS')) {
            if (-not $inside) { throw "Unmatched diagnostic end: $($sketch.FullName)" }
            $inside = $false
            $ends++
        } elseif (-not $inside) {
            if ($line -match 'memoryDiagnostics|LOOM_WISP_BETA_DIAGNOSTICS') {
                throw "Diagnostic implementation outside its block: $($sketch.FullName)"
            }
            if ($line.Contains('WISP_DIAGNOSTIC_')) {
                if (-not $line.Contains('// LOOM_BETA_DIAGNOSTIC')) {
                    throw "Untagged diagnostic call: $($sketch.FullName)"
                }
                $calls++
            } elseif ($line.Contains('LOOM_BETA_DIAGNOSTIC')) {
                throw "Diagnostic tag attached to operational code: $($sketch.FullName)"
            }
        }
    }
    if ($inside -or $begins -ne 2 -or $ends -ne 2 -or $calls -eq 0) {
        throw "Incomplete diagnostic blocks/calls: $($sketch.FullName)"
    }
    if (-not $text.Contains('#if LOOM_DEBUG_DIAGNOSTICS') -or
        ([regex]::Matches($text, '(?m)^\s*ENABLE_SD_LOGGING;\s*$').Count -ne 1) -or
        $text.Contains('DISABLE_RTC_LOG_TIMESTAMPS')) {
        throw "Debug build lost its gate or timestamped SD logging: $($sketch.FullName)"
    }
    Write-Host "PASS debug boundary: $($sketch.Name) ($calls checkpoints/traces)"
}
Write-Host 'All Wisp sketches preserve production safeguards and separate debug output.'
