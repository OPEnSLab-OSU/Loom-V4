param(
    [Parameter(Mandatory = $true)]
    [string]$TestsDir,

    [Parameter(Mandatory = $true)]
    [string]$ManifestPath,

    [switch]$AllowIncomplete
)

$ErrorActionPreference = 'Stop'

try {
    $testsPath = (Resolve-Path -LiteralPath $TestsDir).Path
    $audits = @(
        Get-ChildItem -LiteralPath $testsPath -Directory -Filter 'loom_compile_audit_*' |
            Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'compile_report.csv') } |
            Sort-Object LastWriteTime -Descending
    )

    if ($audits.Count -eq 0) {
        Write-Host 'ERROR: No compile audit containing compile_report.csv was found.' -ForegroundColor Red
        exit 1
    }

    $sourceAudit = $audits[0]
    $reportPath = Join-Path $sourceAudit.FullName 'compile_report.csv'
    $summaryPath = Join-Path $sourceAudit.FullName 'summary.txt'
    $auditFinished = Test-Path -LiteralPath $summaryPath -PathType Leaf
    if ($auditFinished) {
        $auditFinished = Select-String -LiteralPath $summaryPath -Pattern '^Total sketches:' -Quiet
    }
    if (-not $auditFinished -and -not $AllowIncomplete) {
        Write-Host 'ERROR: The newest compile audit is still running or did not reach its final summary.' -ForegroundColor Red
        Write-Host 'Wait for it to finish (or stop it cleanly with Ctrl-C) before retrying failures.' -ForegroundColor Yellow
        exit 1
    }
    if (-not $auditFinished) {
        Write-Host 'WARNING: Generating a partial manifest from an audit that has not reached its final summary.' -ForegroundColor Yellow
    }

    $failedRows = @(Import-Csv -LiteralPath $reportPath | Where-Object { $_.result -eq 'FAIL' })

    Write-Host ("Source audit: {0}" -f $sourceAudit.FullName)
    Write-Host ("Failed rows: {0}" -f $failedRows.Count)

    $eligible = New-Object 'System.Collections.Generic.List[string]'
    $seen = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
    $missingLogs = 0
    $missingSketches = 0

    foreach ($row in $failedRows) {
        $logPath = [string]$row.log_path
        $sketchPath = [string]$row.sketch_path

        # Deleting a failed sketch's log is an intentional opt-out from retry.
        if ([string]::IsNullOrWhiteSpace($logPath) -or -not (Test-Path -LiteralPath $logPath -PathType Leaf)) {
            $missingLogs++
            Write-Host ("Excluded (log deleted): {0}" -f $row.sketch_name) -ForegroundColor DarkYellow
            continue
        }

        if ([string]::IsNullOrWhiteSpace($sketchPath) -or -not (Test-Path -LiteralPath $sketchPath -PathType Container)) {
            $missingSketches++
            Write-Host ("Excluded (sketch deleted): {0}" -f $row.sketch_name) -ForegroundColor DarkYellow
            continue
        }

        $sketchName = Split-Path -Leaf $sketchPath
        $mainFile = Join-Path $sketchPath ($sketchName + '.ino')
        if (-not (Test-Path -LiteralPath $mainFile -PathType Leaf)) {
            $missingSketches++
            Write-Host ("Excluded (no matching main file): {0}" -f $row.sketch_name) -ForegroundColor DarkYellow
            continue
        }

        $resolvedSketch = (Resolve-Path -LiteralPath $sketchPath).Path
        if ($seen.Add($resolvedSketch)) {
            $eligible.Add($resolvedSketch)
        }
    }

    Write-Host ("Retry eligible: {0}" -f $eligible.Count) -ForegroundColor Cyan
    Write-Host ("Excluded because log was deleted: {0}" -f $missingLogs)
    Write-Host ("Excluded because sketch was deleted or invalid: {0}" -f $missingSketches)

    if ($eligible.Count -eq 0) {
        exit 2
    }

    $manifestDirectory = Split-Path -Parent $ManifestPath
    if (-not [string]::IsNullOrWhiteSpace($manifestDirectory)) {
        New-Item -ItemType Directory -Path $manifestDirectory -Force | Out-Null
    }
    Set-Content -LiteralPath $ManifestPath -Value $eligible -Encoding ASCII
    exit 0
}
catch {
    Write-Host ("ERROR: {0}" -f $_.Exception.Message) -ForegroundColor Red
    exit 1
}
