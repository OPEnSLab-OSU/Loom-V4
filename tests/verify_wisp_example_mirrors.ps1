$ErrorActionPreference = 'Stop'

$loomRoot = Split-Path -Parent $PSScriptRoot
$pairs = @(
    @{
        Label = 'Wisp_Batch_Logging'
        Canonical = 'examples\Lab Examples\Wisp\Wisp_Batch_Logging\Wisp_Batch_Logging.ino'
        Mirror = 'examples\Lab Examples\Wisp\examples\Wisp\Wisp_Batch_Logging\Wisp_Batch_Logging.ino'
    },
    @{
        Label = 'Wisp_Mux_BatchLogging'
        Canonical = 'examples\Lab Examples\Wisp\Wisp_Mux_BatchLogging\Wisp_Mux_BatchLogging.ino'
        Mirror = 'examples\Lab Examples\Wisp\examples\Wisp\Wisp_Mux_BatchLogging\Wisp_Mux_BatchLogging.ino'
    },
    @{
        Label = 'WispV2_Deploy_2026'
        Canonical = 'examples\Lab Examples\Wisp\WispV2_Deploy_2026\WispV2_Deploy_2026.ino'
        Mirror = 'examples\Lab Examples\Wisp\examples\Wisp\WispV2_Deploy_2026\WispV2_Deploy_2026.ino'
    },
    @{
        Label = 'Loom_MemoryDiagnostics'
        Canonical = 'src\Diagnostics\Loom_MemoryDiagnostics.h'
        Mirror = 'examples\Lab Examples\Wisp\src\Diagnostics\Loom_MemoryDiagnostics.h'
    }
)

function Read-NormalizedText([string] $path) {
    $text = [System.IO.File]::ReadAllText($path)
    return $text.Replace("`r`n", "`n").Replace("`r", "`n")
}

$failed = $false
foreach ($pair in $pairs) {
    $canonicalPath = Join-Path $loomRoot $pair.Canonical
    $mirrorPath = Join-Path $loomRoot $pair.Mirror

    if (-not (Test-Path -LiteralPath $canonicalPath -PathType Leaf)) {
        Write-Error "Missing canonical file: $canonicalPath" -ErrorAction Continue
        $failed = $true
        continue
    }
    if (-not (Test-Path -LiteralPath $mirrorPath -PathType Leaf)) {
        Write-Error "Missing mirror file: $mirrorPath" -ErrorAction Continue
        $failed = $true
        continue
    }

    if ((Read-NormalizedText $canonicalPath) -cne (Read-NormalizedText $mirrorPath)) {
        Write-Error "$($pair.Label) mirror drifted from its canonical source.`n  Canonical: $canonicalPath`n  Mirror:    $mirrorPath" -ErrorAction Continue
        $failed = $true
        continue
    }

    Write-Host "PASS $($pair.Label)"
}

if ($failed) {
    exit 1
}

Write-Host 'All WISP mirrors match their canonical sources.'
