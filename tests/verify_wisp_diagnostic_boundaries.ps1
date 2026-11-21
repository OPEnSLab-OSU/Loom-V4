$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$labRoot = Join-Path $root 'examples/Lab Examples/Wisp'
$profileRoot = Join-Path $root 'tools/trace/sketches/Wisp'
$families = @('WispV2_Deploy_2026', 'Wisp_Batch_Logging', 'Wisp_Mux_BatchLogging')

foreach ($family in $families) {
    $ordinary = Get-Content -LiteralPath (Join-Path $labRoot "$family/$family.ino") -Raw
    if ($ordinary -match 'Loom_(?:DebugSketch|TraceSketch|MemoryDiagnostics)|\bLOOM_TRACE[_A-Z]*\b|\.set(?:Scan)?Debug\(true\)|\.debugScan\(') {
        throw "Diagnostic code escaped into the regular $family sketch."
    }

    foreach ($suffix in @('_debug', '_debug_minimal')) {
        $name = "$family$suffix"
        $profile = Get-Content -LiteralPath (Join-Path $profileRoot "$name/$name.ino") -Raw
        $attach = [regex]::Matches($profile, '\bLOOM_TRACE_ATTACH\(manager,\s*hypnos\)')
        $initialize = [regex]::Match($profile, '(?m)^\s*manager\.initialize\(\);')
        if ($attach.Count -ne 1 -or !$initialize.Success -or $initialize.Index -lt $attach[0].Index) {
            throw "$name must attach one recorder before Manager initialization."
        }
        foreach ($step in @('batchSD.shouldPublish()', 'hypnos.logToSD()', 'sd->retryBatch()', 'Watchdog.disable()')) {
            if (!$profile.Contains($step)) { throw "$name is missing the field safeguard $step" }
        }
        if ($suffix -eq '_debug_minimal') {
            if ($profile -match 'Loom_DebugSketch|Loom_MemoryDiagnostics' -or
                $profile -notmatch '(?m)^#define LOOM_DEBUG_TEXT 0\s*$' -or
                $profile -notmatch '(?m)^#define LOOM_DEBUG_SD_LOG 0\s*$') {
                throw "$name must provide a quiet trace without the full diagnostic reports."
            }
        } elseif ($family -ne 'Wisp_Batch_Logging' -and
                  ($profile -notmatch '\bmux\.setDebug\(true\)' -or
                   $profile -notmatch '\bmux\.setScanDebug\(true\)')) {
            throw "$name lost the optional detailed mux diagnostics."
        }
    }
}
Write-Output 'PASS Wisp diagnostics: three ordinary sketches and six opt-in profiles'
