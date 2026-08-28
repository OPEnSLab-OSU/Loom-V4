param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("count", "append", "print")]
    [string]$Action,

    [Parameter(Mandatory = $true)]
    [ValidateSet("all", "loom", "external")]
    [string]$Scope,

    [Parameter(Mandatory = $true)]
    [string]$LogPath,

    [string]$OutputPath = "",
    [string]$LoomDir = "",
    [string]$BuildDir = "",
    [string]$SketchDir = ""
)

# The batch harness always captures full raw compiler logs. This helper only
# classifies concrete "warning:" lines so the console can stay Loom-focused
# while audit reports still preserve Arduino core and third-party diagnostics.
function Normalize-PathFragment {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ""
    }

    return $Path.Replace("/", "\").TrimEnd("\").ToLowerInvariant()
}

# A warning is Loom-owned when the actual warning line points at Loom source,
# a Loom example, the active sketch folder, or Arduino's generated sketch code.
# Include traces through Loom files are intentionally not enough; otherwise one
# noisy vendor header can make every downstream compile look like a Loom issue.
function Test-LoomWarningLine {
    param([string]$Line)

    $normalizedLine = $Line.Replace("/", "\").ToLowerInvariant()
    $loomRoot = Normalize-PathFragment $LoomDir
    $buildRoot = Normalize-PathFragment $BuildDir
    $sketchRoot = Normalize-PathFragment $SketchDir

    if ($loomRoot -ne "") {
        if ($normalizedLine.Contains("$loomRoot\src\")) {
            return $true
        }
        if ($normalizedLine.Contains("$loomRoot\examples\")) {
            return $true
        }
    }

    if ($sketchRoot -ne "" -and $normalizedLine.Contains("$sketchRoot\")) {
        return $true
    }

    if ($buildRoot -ne "" -and $normalizedLine.Contains("$buildRoot\sketch\")) {
        return $true
    }

    return $false
}

if (-not (Test-Path -LiteralPath $LogPath)) {
    if ($Action -eq "count") {
        Write-Output 0
    }
    exit 0
}

$warningLines = Get-Content -LiteralPath $LogPath | Where-Object {
    $_ -match "(?i)warning:"
}

switch ($Scope) {
    "loom" {
        $selectedLines = $warningLines | Where-Object { Test-LoomWarningLine $_ }
    }
    "external" {
        $selectedLines = $warningLines | Where-Object { -not (Test-LoomWarningLine $_) }
    }
    default {
        $selectedLines = $warningLines
    }
}

switch ($Action) {
    "count" {
        Write-Output (($selectedLines | Measure-Object).Count)
    }
    "append" {
        if ($OutputPath -ne "" -and $selectedLines) {
            Add-Content -LiteralPath $OutputPath -Value $selectedLines
        }
    }
    "print" {
        if ($selectedLines) {
            $selectedLines | Write-Output
        }
    }
}
