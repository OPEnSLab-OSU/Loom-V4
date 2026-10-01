param([string] $DeploymentFolder)

# Arduino discovers a library from a top-level header before nested includes resolve.
# Check this early; no compiler, dependency modification or sketch rewrite is needed.
$ErrorActionPreference = 'Stop'
$loomRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$sourceRoot = Join-Path $loomRoot 'src'
$headers = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
foreach ($file in Get-ChildItem -LiteralPath $sourceRoot -Recurse -File -Filter '*.h') {
    $relative = [System.IO.Path]::GetRelativePath($sourceRoot, $file.FullName).Replace('\', '/')
    $headers.Add($relative) | Out-Null
}
$sketches = @(Get-ChildItem -LiteralPath (Join-Path $loomRoot 'examples') -Recurse -File -Filter '*.ino')
$sketches += @(Get-ChildItem -LiteralPath $PSScriptRoot -File -Filter '*.ino')
# Saved compiler evidence contains historical sketch copies. Check today's examples/tests, not
# those frozen copies: a future header change must not make an old audit fail source preflight.
foreach ($testFolder in Get-ChildItem -LiteralPath $PSScriptRoot -Directory) {
    if ($testFolder.Name -match '^(loom_compile_audit_|sketch_compile_|__pycache__$)') { continue }
    $sketches += @(Get-ChildItem -LiteralPath $testFolder.FullName -Recurse -File -Filter '*.ino')
}
if ($DeploymentFolder) {
    $sketches += @(Get-ChildItem -LiteralPath (Resolve-Path -LiteralPath $DeploymentFolder).Path -Recurse -File -Filter '*.ino')
}
$failures = @()
$checked = 0
foreach ($sketch in $sketches) {
    foreach ($line in Get-Content -LiteralPath $sketch.FullName) {
        if ($line -match '^\s*#\s*include\s*[<"]([^>"]+)[>"]') {
            $header = $Matches[1].Replace('\', '/')
            if ($headers.Contains($header)) {
                ++$checked
                if ($header.Contains('/')) {
                    $failures += "$($sketch.FullName): include Loom_Manager.h before $header."
                }
                break # Only the first recognized Loom include controls discovery.
            }
        }
    }
}
if ($failures.Count) { throw ($failures -join "`n") }
Write-Output "PASS library discovery include order: $checked sketches"
