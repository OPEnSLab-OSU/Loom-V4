$ErrorActionPreference = 'Stop'
$sourceRoot = Join-Path (Split-Path -Parent $PSScriptRoot) 'src'
$guardPath = Join-Path $sourceRoot 'Loom_WarningGuards.h'
if (-not (Test-Path -LiteralPath $guardPath -PathType Leaf)) {
    throw 'The compiler warning-scope header is missing. Restore the audit instrumentation first.'
}

# Standard/runtime headers retain full diagnostics; only core/vendor includes are scoped.
$standardHeaders = @('algorithm', 'array', 'cmath', 'cstdarg', 'cstdint', 'cstdio', 'cstring',
    'functional', 'initializer_list', 'limits', 'map', 'memory', 'tuple', 'utility', 'vector',
    'assert.h', 'ctype.h', 'errno.h', 'float.h', 'inttypes.h', 'limits.h', 'malloc.h', 'math.h',
    'stddef.h', 'stdint.h', 'stdio.h', 'stdlib.h', 'string.h', 'time.h')
$extensions = @('.c', '.cc', '.cpp', '.h', '.hpp', '.tpp')
$scopeCount = 0
$fileCount = 0

foreach ($file in (Get-ChildItem -LiteralPath $sourceRoot -File -Recurse)) {
    if ($file.FullName -eq $guardPath -or $file.Extension -notin $extensions) { continue }
    $inside = $false
    $guardIncluded = $false
    $includeCount = 0
    $hasScope = $false
    $lineNumber = 0
    foreach ($line in [IO.File]::ReadAllLines($file.FullName)) {
        $lineNumber++
        $location = "$($file.FullName):$lineNumber"
        if ($line -match '^\s*LOOM_EXTERNAL_INCLUDE_BEGIN\s*$') {
            if ($inside -or -not $guardIncluded) { throw "Invalid warning-scope start: $location" }
            $inside = $true
            $includeCount = 0
            $hasScope = $true
            $scopeCount++
            continue
        }
        if ($line -match '^\s*LOOM_EXTERNAL_INCLUDE_END\s*$') {
            if (-not $inside -or $includeCount -eq 0) { throw "Invalid warning-scope end: $location" }
            $inside = $false
            continue
        }
        if ($line -match '^\s*#\s*include\s*[<"]([^>"]+)[>"]') {
            $header = $Matches[1]
            $isOwned = (Test-Path -LiteralPath (Join-Path $file.DirectoryName $header) -PathType Leaf) -or
                (Test-Path -LiteralPath (Join-Path $sourceRoot $header) -PathType Leaf)
            $isExternal = -not $isOwned -and $header -notin $standardHeaders
            if ($isExternal -ne $inside) { throw "Incorrect include warning scope for '$header': $location" }
            if ($header -eq 'Loom_WarningGuards.h') { $guardIncluded = $true }
            if ($inside) { $includeCount++ }
        } elseif ($inside -and $line -notmatch '^\s*(//|/\*|\*|$)') {
            throw "Warning scope contains code or a conditional instead of external includes: $location"
        }
    }
    if ($inside) { throw "Unclosed warning scope: $($file.FullName)" }
    if ($hasScope) { $fileCount++ }
}
Write-Host "PASS warning scopes: $scopeCount balanced external include groups in $fileCount files."
Write-Host 'Loom-owned code and standard/runtime headers retain their diagnostics. No compiler was run.'
