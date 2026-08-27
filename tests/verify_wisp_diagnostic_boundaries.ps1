$ErrorActionPreference = 'Stop'

$loomRoot = Split-Path -Parent $PSScriptRoot
$sketches = @(
    'examples\Lab Examples\Wisp\Wisp_Batch_Logging\Wisp_Batch_Logging.ino',
    'examples\Lab Examples\Wisp\Wisp_Mux_BatchLogging\Wisp_Mux_BatchLogging.ino',
    'examples\Lab Examples\Wisp\WispV2_Deploy_2026\WispV2_Deploy_2026.ino',
    'examples\Lab Examples\Wisp\examples\Wisp\Wisp_Batch_Logging\Wisp_Batch_Logging.ino',
    'examples\Lab Examples\Wisp\examples\Wisp\Wisp_Mux_BatchLogging\Wisp_Mux_BatchLogging.ino',
    'examples\Lab Examples\Wisp\examples\Wisp\WispV2_Deploy_2026\WispV2_Deploy_2026.ino'
)

$failed = $false

function Report-Failure([string] $path, [int] $lineNumber, [string] $message) {
    $location = if ($lineNumber -gt 0) { "${path}:${lineNumber}" } else { $path }
    Write-Error "${location}: ${message}" -ErrorAction Continue
    $script:failed = $true
}

foreach ($relativePath in $sketches) {
    $path = Join-Path $loomRoot $relativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        Report-Failure $relativePath 0 'missing sketch'
        continue
    }

    $lines = [System.IO.File]::ReadAllLines($path)
    $insideBlock = $false
    $beginCount = 0
    $endCount = 0
    $taggedCallCount = 0
    $hasWatchdog = $false

    for ($index = 0; $index -lt $lines.Length; $index++) {
        $line = $lines[$index]
        $lineNumber = $index + 1

        if ($line.Contains('// BEGIN LOOM_BETA_DIAGNOSTICS')) {
            if ($insideBlock) {
                Report-Failure $relativePath $lineNumber 'nested diagnostic block'
            }
            $insideBlock = $true
            $beginCount++
            continue
        }

        if ($line.Contains('// END LOOM_BETA_DIAGNOSTICS')) {
            if (-not $insideBlock) {
                Report-Failure $relativePath $lineNumber 'diagnostic block ends without a begin marker'
            }
            $insideBlock = $false
            $endCount++
            continue
        }

        if ($line.Contains('Adafruit_SleepyDog.h') -or
            $line.Contains('enableActiveWatchdog()')) {
            $hasWatchdog = $true
        }

        if ($line.Contains('ENABLE_SD_LOGGING')) {
            Report-Failure $relativePath $lineNumber 'ordinary per-log SD debug output is enabled'
        }

        if (-not $insideBlock -and $line.Contains('memoryDiagnostics')) {
            Report-Failure $relativePath $lineNumber 'raw memory-diagnostic implementation escaped its marked block'
        }

        if (-not $insideBlock -and $line.Contains('LOOM_WISP_BETA_DIAGNOSTICS')) {
            Report-Failure $relativePath $lineNumber 'diagnostic compile switch escaped its marked block'
        }

        if (-not $insideBlock -and $line.Contains('WISP_DIAGNOSTIC_')) {
            if (-not $line.Contains('// LOOM_BETA_DIAGNOSTIC')) {
                Report-Failure $relativePath $lineNumber 'diagnostic call is not tagged for canonical removal'
            }
            else {
                $taggedCallCount++
            }
        }

        if (-not $insideBlock -and
            $line.Contains('LOOM_BETA_DIAGNOSTIC') -and
            -not $line.Contains('WISP_DIAGNOSTIC_')) {
            Report-Failure $relativePath $lineNumber 'removal tag is attached to a non-diagnostic line'
        }
    }

    if ($insideBlock) {
        Report-Failure $relativePath $lines.Length 'diagnostic block is not closed'
    }
    if ($beginCount -ne 2 -or $endCount -ne 2) {
        Report-Failure $relativePath 0 "expected two diagnostic blocks; found ${beginCount} begin and ${endCount} end markers"
    }
    if ($taggedCallCount -eq 0) {
        Report-Failure $relativePath 0 'no tagged diagnostic calls found'
    }
    if (-not $hasWatchdog) {
        Report-Failure $relativePath 0 'production watchdog coverage is missing'
    }

    if (-not $failed) {
        Write-Host "PASS $relativePath ($taggedCallCount tagged calls)"
    }
}

if ($failed) {
    exit 1
}

Write-Host 'All WISP beta diagnostics are compile-gated, bounded, and removable.'
