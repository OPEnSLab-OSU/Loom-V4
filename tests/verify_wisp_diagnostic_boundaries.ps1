param([string[]] $DeploymentFolder)

$ErrorActionPreference = 'Stop'
$loomRoot = Split-Path -Parent $PSScriptRoot
$wispRoot = Join-Path $loomRoot 'examples\Lab Examples\Wisp'
foreach ($header in @('src\Logger.h', 'src\Diagnostics\Loom_MemoryDiagnostics.h', 'src\Diagnostics\Loom_DebugSketch.h')) {
    if (-not (Test-Path -LiteralPath (Join-Path $loomRoot $header) -PathType Leaf)) {
        throw "Missing canonical debug header: $header"
    }
}
$sketches = @(Get-ChildItem -LiteralPath $wispRoot -Filter '*.ino' -Recurse |
    Where-Object { $_.FullName -notmatch '[\\/]\.loom-build[\\/]' })
foreach ($savedFolder in $DeploymentFolder) {
    $sketches += Get-ChildItem -LiteralPath $savedFolder -Filter '*.ino' -Recurse |
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
    $diagnosticHeader = $text.IndexOf('#include <Diagnostics/')
    if ($diagnosticHeader -ge 0 -and
        ($text.IndexOf('#include <Loom_Manager.h>') -lt 0 -or
         $text.IndexOf('#include <Loom_Manager.h>') -gt $diagnosticHeader)) {
        throw "Arduino must discover Loom before its nested diagnostic headers: $($sketch.FullName)"
    }
    if ($sketch.BaseName.EndsWith('_debug_minimal')) {
        if ($text -match 'WISP_DIAGNOSTIC_|Loom_MemoryDiagnostics|Loom_DebugSketch|LOOM_DEBUG_CHECKPOINT|executionTrace|manager\.display_data\(|LOOM_TRACE_RECORDER|LOOM_TRACE_BEGIN|LOOM_TRACE_SAVE_ON_RETURN|LOOM_TRACE_FLUSH|LOOM_TRACE_CHECKPOINT') {
            throw "Manual/full diagnostics escaped into the automatic minimal sketch: $($sketch.FullName)"
        }
        if (-not $text.Contains('Logger::getInstance()->setDebugOutput(LOOM_DEBUG_TEXT != 0)') -or
            $text -notmatch '(?m)^#define LOOM_DEBUG_TEXT 0\s*$' -or
            $text -notmatch '(?m)^#define LOOM_DEBUG_SD_LOG 0\s*$' -or
            $text -notmatch '#if LOOM_DEBUG_SD_LOG\s+ENABLE_SD_LOGGING;\s+#endif' -or
            ([regex]::Matches($text, 'LOOM_TRACE_ATTACH\(manager, hypnos\)').Count -ne 1) -or
            $text -notmatch '(?m)^\s*#include\s*[<"]Diagnostics/Loom_TraceSketch\.h[>"]' -or
            $text -notmatch '(?m)^\s*#define\s+LOOM_TRACE\s+[01]\s*$' -or
            $text -notmatch '(?m)^\s*#define\s+LOOM_TRACE_HEAP\s+[01]\s*$') {
            throw "Minimal sketch lost its flags, quiet text output, or one-call attachment: $($sketch.FullName)"
        }
        Write-Host "PASS automatic minimal boundary: $($sketch.Name)"
        continue
    }
    if (-not $sketch.BaseName.EndsWith('_debug')) {
        if ($text -match 'LOOM_BETA_DIAGNOSTIC|WISP_DIAGNOSTIC_|Loom_MemoryDiagnostics|Loom_DebugSketch|LOOM_DEBUG_CHECKPOINT|LOOM_WISP_TRACE|WISP_TRACE_|executionTrace|ENABLE_SD_LOGGING|manager\.display_data\(') {
            throw "Debug instrumentation escaped into the quiet sketch: $($sketch.FullName)"
        }
        if (-not $text.Contains('Logger::getInstance()->setDebugOutput(false)')) {
            throw "Quiet sketch does not suppress routine logger output: $($sketch.FullName)"
        }
        Write-Host "PASS quiet boundary: $($sketch.Name)"
        continue
    }

    if ($text -notmatch '(?m)^\s*#include\s*[<"]Diagnostics/Loom_DebugSketch\.h[>"]' -or
        $text -match 'WISP_DIAGNOSTIC_|WISP_SERIAL_MEMORY') {
        throw "Debug sketch must use shared Loom helpers without local Wisp wrappers: $($sketch.FullName)"
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
            $code = ($line -split '//', 2)[0]
            if ($code -cmatch 'memoryDiagnostics\.(checkpoint|beginCycle)|Loom_MemoryDiagnostics memoryDiagnostics|LOOM_WISP_BETA_DIAGNOSTICS') {
                throw "Diagnostic implementation outside its block: $($sketch.FullName)"
            }
            if ($code -match 'LOOM_DEBUG_(CHECKPOINT|BEGIN_CYCLE)\(|mux\.set(Debug|ScanDebug)\(true\)|hypnos\.getSDManager\(\)->setWriteDebug\(true\)') {
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
    if (-not $text.Contains('#if LOOM_DEBUG_MEMORY') -or
        ([regex]::Matches($text, '(?m)^\s*ENABLE_SD_LOGGING;\s*$').Count -ne 1) -or
        $text.Contains('DISABLE_RTC_LOG_TIMESTAMPS')) {
        throw "Debug build lost its gate or timestamped SD logging: $($sketch.FullName)"
    }
    if (-not $text.Contains('Logger::getInstance()->setDebugOutput(LOOM_DEBUG_TEXT != 0)') -or
        $text -notmatch '#if LOOM_DEBUG_SD_LOG\s+ENABLE_SD_LOGGING;\s+#endif' -or
        $text -notmatch 'LOOM_DEBUG_CHECKPOINT\(memoryDiagnostics, "[^"\r\n]+", manager\.getDocument\(\), batchSD\.getCurrentBatch\(\)\)') {
        throw "Debug sketch lost its explicit text controls or shared checkpoint: $($sketch.FullName)"
    }
    if (([regex]::Matches($text, 'LOOM_TRACE_ATTACH\(manager, hypnos\)').Count -ne 1) -or
        $text -match 'LOOM_TRACE_RECORDER|LOOM_TRACE_BEGIN|LOOM_TRACE_SAVE_ON_RETURN|LOOM_TRACE_FLUSH|executionTrace') {
        throw "Full debug sketch lost automatic attachment or retained manual capture plumbing: $($sketch.FullName)"
    }
    Write-Host "PASS debug boundary: $($sketch.Name) ($calls checkpoints/traces)"
}
Write-Host 'All Wisp sketches preserve production safeguards and explicit trace/text controls.'
