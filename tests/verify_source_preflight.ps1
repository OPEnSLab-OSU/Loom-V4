param(
    [string] $DeploymentFolder,
    [string] $PackageLibraries,
    [string] $FormatterPath
)

# This entry point only reads sources and installed dependencies. It never builds firmware.
$ErrorActionPreference = 'Stop'
$loomRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$enginePath = Join-Path $PSHOME 'pwsh.exe'
if (-not (Test-Path -LiteralPath $enginePath -PathType Leaf)) {
    throw 'Run the source preflight with PowerShell 7 (pwsh); dependency checks require its .NET APIs.'
}

$deploymentArguments = @()
if ($DeploymentFolder) {
    $deploymentArguments = @('-DeploymentFolder', $DeploymentFolder)
}
$dependencyArguments = @()
if ($PackageLibraries) {
    $dependencyArguments = @('-PackageLibraries', $PackageLibraries)
}

$checks = @(
    @{ Name = 'Warning scopes'; Script = 'verify_warning_scope.ps1'; Arguments = @() },
    @{ Name = 'Quiet/debug parity'; Script = 'verify_wisp_example_mirrors.ps1'; Arguments = $deploymentArguments },
    @{ Name = 'Diagnostic boundaries'; Script = 'verify_wisp_diagnostic_boundaries.ps1'; Arguments = $deploymentArguments },
    @{ Name = 'Dependencies and official core'; Script = 'verify_patched_dependencies.ps1'; Arguments = $dependencyArguments }
)

foreach ($check in $checks) {
    Write-Host "Checking $($check.Name)..."
    # A child process keeps an individual check's exit code from ending this script silently.
    $checkArguments = @('-NoProfile', '-NonInteractive', '-File', (Join-Path $PSScriptRoot $check.Script))
    $checkArguments += $check.Arguments
    & $enginePath @checkArguments
    if ($LASTEXITCODE -ne 0) {
        throw "$($check.Name) failed (exit $LASTEXITCODE)."
    }
}

if ($FormatterPath) {
    if (-not (Test-Path -LiteralPath $FormatterPath -PathType Leaf)) {
        throw "Formatter not found: $FormatterPath"
    }
    Write-Host 'Checking source formatting...'
    $sourceFiles = Get-ChildItem -LiteralPath (Join-Path $loomRoot 'src') -Recurse -File |
        Where-Object { $_.Extension -in @('.cpp', '.h') }
    foreach ($sourceFile in $sourceFiles) {
        # clang-format reads its style beside the source. Dry run never rewrites the file.
        & $FormatterPath '--dry-run' '--Werror' '--style=file' $sourceFile.FullName
        if ($LASTEXITCODE -ne 0) {
            throw "Formatting failed: $($sourceFile.FullName)"
        }
    }
    Write-Host "PASS formatting: $($sourceFiles.Count) source files"
}

Write-Host 'PASS source preflight; no compiler was invoked.'
