param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("Strip", "Restore")]
    [string]$Action
)

$ErrorActionPreference = "Stop"

$TestsDir = Split-Path -Parent $PSCommandPath
$LoomDir = [IO.Path]::GetFullPath((Join-Path $TestsDir ".."))
$SourceDir = Join-Path $LoomDir "src"
$GuardPath = Join-Path $SourceDir "Loom_WarningGuards.h"
$StateDir = Join-Path $TestsDir ".warning_scope_state"
$PatchPath = Join-Path $StateDir "restore.patch"
$MetadataPath = Join-Path $StateDir "metadata.txt"
$StrippedTree = Join-Path $StateDir "a"
$InstrumentedTree = Join-Path $StateDir "b"

# Match complete instrumentation lines only. Comments and normal Loom code are
# deliberately outside the removal pattern.
$MarkerPattern = '(?m)^[\t ]*(?:#\s*include\s*["<]Loom_WarningGuards\.h[">]|LOOM_EXTERNAL_INCLUDE_(?:BEGIN|END))[\t ]*(?:\r\n|\n|\r|$)'
$ByteEncoding = [Text.Encoding]::GetEncoding(28591)

function Get-RelativePath {
    param(
        [Parameter(Mandatory = $true)][string]$BasePath,
        [Parameter(Mandatory = $true)][string]$TargetPath
    )

    $baseFull = [IO.Path]::GetFullPath($BasePath).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    $targetFull = [IO.Path]::GetFullPath($TargetPath)
    $baseUri = New-Object Uri($baseFull)
    $targetUri = New-Object Uri($targetFull)
    return [Uri]::UnescapeDataString($baseUri.MakeRelativeUri($targetUri).ToString()).Replace('/', '\')
}

function Invoke-GitApply {
    param(
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    Push-Location $LoomDir
    try {
        # The restore patch must preserve each source file's existing bytes.
        # Disable repository/user CRLF conversion for these internal patch
        # operations so Git does not warn about or rewrite mixed line endings.
        $gitOutput = & git -c core.autocrlf=false -c core.safecrlf=false @Arguments 2>&1
        $gitResult = $LASTEXITCODE
        $gitOutput | ForEach-Object { Write-Host $_ }
        return $gitResult
    }
    finally {
        Pop-Location
    }
}

function Restore-BackupTree {
    if (-not (Test-Path -LiteralPath $InstrumentedTree -PathType Container)) {
        return
    }

    Get-ChildItem -LiteralPath $InstrumentedTree -Recurse -File | ForEach-Object {
        $relativePath = Get-RelativePath -BasePath $InstrumentedTree -TargetPath $_.FullName
        $destination = Join-Path $LoomDir $relativePath
        $destinationDirectory = Split-Path -Parent $destination
        if (-not (Test-Path -LiteralPath $destinationDirectory -PathType Container)) {
            New-Item -ItemType Directory -Path $destinationDirectory -Force | Out-Null
        }
        Copy-Item -LiteralPath $_.FullName -Destination $destination -Force
    }
}

function Get-InstrumentedFiles {
    $extensions = @(".c", ".cc", ".cpp", ".h", ".hpp", ".tpp")
    return @(Get-ChildItem -LiteralPath $SourceDir -Recurse -File | Where-Object {
        $_.FullName -ne $GuardPath -and $extensions -contains $_.Extension.ToLowerInvariant()
    } | Where-Object {
        $rawText = $ByteEncoding.GetString([IO.File]::ReadAllBytes($_.FullName))
        [regex]::IsMatch($rawText, $MarkerPattern)
    })
}

function Strip-WarningScope {
    if (Test-Path -LiteralPath $StateDir) {
        throw "Saved warning-scope state already exists at '$StateDir'. Restore it before stripping again."
    }
    if (-not (Test-Path -LiteralPath $GuardPath -PathType Leaf)) {
        throw "The warning guard header is missing; the source tree does not appear instrumented."
    }

    $files = Get-InstrumentedFiles
    if ($files.Count -eq 0) {
        throw "No warning-scope marker lines were found in src."
    }

    New-Item -ItemType Directory -Path $StrippedTree -Force | Out-Null
    New-Item -ItemType Directory -Path $InstrumentedTree -Force | Out-Null

    $removedMarkers = 0
    $relativeFiles = New-Object Collections.Generic.List[string]

    try {
        foreach ($file in $files) {
            $relativePath = Get-RelativePath -BasePath $LoomDir -TargetPath $file.FullName
            $relativeFiles.Add($relativePath)

            $instrumentedCopy = Join-Path $InstrumentedTree $relativePath
            $strippedCopy = Join-Path $StrippedTree $relativePath
            New-Item -ItemType Directory -Path (Split-Path -Parent $instrumentedCopy) -Force | Out-Null
            New-Item -ItemType Directory -Path (Split-Path -Parent $strippedCopy) -Force | Out-Null

            $sourceBytes = [IO.File]::ReadAllBytes($file.FullName)
            [IO.File]::WriteAllBytes($instrumentedCopy, $sourceBytes)

            $sourceText = $ByteEncoding.GetString($sourceBytes)
            $matches = [regex]::Matches($sourceText, $MarkerPattern)
            $removedMarkers += $matches.Count
            $strippedText = [regex]::Replace($sourceText, $MarkerPattern, "")
            [IO.File]::WriteAllBytes($strippedCopy, $ByteEncoding.GetBytes($strippedText))
        }

        $guardRelativePath = Get-RelativePath -BasePath $LoomDir -TargetPath $GuardPath
        $guardBackup = Join-Path $InstrumentedTree $guardRelativePath
        New-Item -ItemType Directory -Path (Split-Path -Parent $guardBackup) -Force | Out-Null
        Copy-Item -LiteralPath $GuardPath -Destination $guardBackup -Force

        Push-Location $StateDir
        try {
            & git -c core.autocrlf=false -c core.safecrlf=false diff --no-index --binary --output=restore.patch -- a b
            $diffResult = $LASTEXITCODE
        }
        finally {
            Pop-Location
        }
        if ($diffResult -ne 1 -or -not (Test-Path -LiteralPath $PatchPath -PathType Leaf)) {
            throw "Git could not create the restore patch (exit code $diffResult)."
        }
    }
    catch {
        if (Test-Path -LiteralPath $StateDir) {
            Remove-Item -LiteralPath $StateDir -Recurse -Force
        }
        throw
    }

    try {
        Get-ChildItem -LiteralPath $StrippedTree -Recurse -File | ForEach-Object {
            $relativePath = Get-RelativePath -BasePath $StrippedTree -TargetPath $_.FullName
            $destination = Join-Path $LoomDir $relativePath
            Copy-Item -LiteralPath $_.FullName -Destination $destination -Force
        }
        Remove-Item -LiteralPath $GuardPath -Force

        $checkResult = Invoke-GitApply -Arguments @("apply", "-p2", "--check", "--", $PatchPath)
        if ($checkResult -ne 0) {
            throw "The generated restore patch did not validate against the stripped source tree."
        }

        $remainingFiles = Get-InstrumentedFiles
        if ($remainingFiles.Count -ne 0 -or (Test-Path -LiteralPath $GuardPath)) {
            throw "Instrumentation markers or the guard header remain after stripping."
        }

        $patchHash = (Get-FileHash -LiteralPath $PatchPath -Algorithm SHA256).Hash
        $metadata = @(
            "Created: $([DateTime]::Now.ToString('o'))"
            "Loom root: $LoomDir"
            "Instrumented files: $($files.Count)"
            "Removed marker lines: $removedMarkers"
            "Patch SHA256: $patchHash"
            ""
            "Files:"
        ) + ($relativeFiles | Sort-Object)
        [IO.File]::WriteAllLines($MetadataPath, $metadata, [Text.UTF8Encoding]::new($false))

        Remove-Item -LiteralPath $StrippedTree -Recurse -Force
        Remove-Item -LiteralPath $InstrumentedTree -Recurse -Force

        Write-Host "Removed $removedMarkers marker lines from $($files.Count) source files."
        Write-Host "Saved exact restore patch: $PatchPath"
    }
    catch {
        Restore-BackupTree
        if (Test-Path -LiteralPath $StateDir) {
            Remove-Item -LiteralPath $StateDir -Recurse -Force
        }
        throw
    }
}

function Restore-WarningScope {
    if (-not (Test-Path -LiteralPath $PatchPath -PathType Leaf)) {
        throw "No saved restore patch exists at '$PatchPath'. Run warning_scope_strip.cmd first."
    }
    if (Test-Path -LiteralPath $GuardPath -PathType Leaf) {
        throw "The warning guard already exists. Refusing to apply the instrumentation twice."
    }
    if ((Get-InstrumentedFiles).Count -ne 0) {
        throw "Warning-scope markers already exist in src. Refusing to apply the saved patch twice."
    }

    $checkResult = Invoke-GitApply -Arguments @("apply", "-p2", "--check", "--", $PatchPath)
    if ($checkResult -ne 0) {
        throw "The restore patch no longer applies cleanly. Source files were left unchanged and the patch was retained."
    }

    $applyResult = Invoke-GitApply -Arguments @("apply", "-p2", "--whitespace=nowarn", "--", $PatchPath)
    if ($applyResult -ne 0) {
        throw "Git failed while applying the restore patch. The patch was retained."
    }

    if (-not (Test-Path -LiteralPath $GuardPath -PathType Leaf)) {
        throw "Restore completed without recreating Loom_WarningGuards.h. The state directory was retained."
    }
    $restoredFiles = Get-InstrumentedFiles
    if ($restoredFiles.Count -eq 0) {
        throw "Restore completed without recreating source marker lines. The state directory was retained."
    }

    Remove-Item -LiteralPath $StateDir -Recurse -Force
    Write-Host "Restored warning-scope instrumentation to $($restoredFiles.Count) source files."
    Write-Host "The consumed restore state was removed; the strip/restore cycle can be run again."
}

try {
    if ($Action -eq "Strip") {
        Strip-WarningScope
    }
    else {
        Restore-WarningScope
    }
    exit 0
}
catch {
    Write-Host "ERROR: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
