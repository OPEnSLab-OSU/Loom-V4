param(
    [Parameter(Mandatory)] [string] $SketchList,
    [string] $ArduinoCli = "$env:LOCALAPPDATA\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe",
    [string] $Fqbn = 'loom4:samd:adafruit_feather_m0:usbstack=arduino,debug=off',
    [string] $ExtraCppFlags = '',
    [int] $Jobs = 4,
    [string] $RunDirectory,
    [string] $PrecompiledLoom,
    [switch] $ReuseVerifiedLibraryObjects,
    [string] $PythonPath = 'C:\Users\brews\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$loomRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$packageLibraries = Split-Path -Parent $loomRoot
$sketchbookLibraries = Join-Path $env:USERPROFILE 'Documents/Arduino/libraries'
$temporaryRoot = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd('\')
if (-not $RunDirectory) {
    $RunDirectory = Join-Path $temporaryRoot ('loom-staged-audit-' + [guid]::NewGuid().ToString('N'))
}
$RunDirectory = [System.IO.Path]::GetFullPath($RunDirectory).TrimEnd('\')
# This script deletes only its temporary staging children, never sketches or dependencies.
if ((Split-Path -Parent $RunDirectory) -ne $temporaryRoot -or
    (Split-Path -Leaf $RunDirectory) -notmatch '^loom-staged-audit-[a-f0-9]{32}$') {
    throw 'RunDirectory must be a direct temporary child named loom-staged-audit-<32 hex digits>.'
}
$stage = Join-Path $RunDirectory 'LoomAudit'
$build = Join-Path $RunDirectory 'build'
$stamp = (Get-Date -Format 'yyyy-MM-dd_HH-mm-ss') + '_' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$report = Join-Path $PSScriptRoot ('sketch_compile_' + $stamp)
$logs = Join-Path $report 'logs'
$firmware = Join-Path $report 'firmware'
New-Item -ItemType Directory -Force -Path $stage, $build, $logs, $firmware | Out-Null
$sketches = @(Get-Content -LiteralPath $SketchList | Where-Object { $_.Trim() } | ForEach-Object { (Resolve-Path -LiteralPath $_).Path })
if (-not $sketches.Count -or @($sketches | Select-Object -Unique).Count -ne $sketches.Count) {
    throw 'SketchList must contain unique, existing sketch directories.'
}
Copy-Item -LiteralPath $SketchList -Destination (Join-Path $report 'requested_sketches.txt')
$archiveManifest = $null
if ($PrecompiledLoom) {
    if ($ReuseVerifiedLibraryObjects) { throw 'Choose object reuse or the verified archive, not both.' }
    $manifestPath = Join-Path $PrecompiledLoom 'audit_archive.json'
    $archiveManifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if ($archiveManifest.accepted_build.fqbn -ne $Fqbn -or $ExtraCppFlags -notin @('', '-DLOOM_WISP_BETA_DIAGNOSTICS=0')) {
        throw 'Use a cold build for a changed board/profile or library-affecting compiler flags.'
    }
    if ((Get-FileHash -LiteralPath $archiveManifest.archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $archiveManifest.archive_sha256) {
        throw 'Accepted archive changed.'
    }
    foreach ($inputFile in $archiveManifest.accepted_inputs.PSObject.Properties) {
        if ((Get-FileHash -LiteralPath $inputFile.Name -Algorithm SHA256).Hash.ToLowerInvariant() -ne $inputFile.Value) { throw "Accepted input changed: $($inputFile.Name)" }
    }
    foreach ($header in $archiveManifest.headers) {
        foreach ($path in @($header.original, $header.copy)) {
            if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $header.sha256) { throw "Accepted header changed: $path" }
        }
    }
    Copy-Item -LiteralPath $manifestPath -Destination (Join-Path $report 'audit_archive.json')
}
$configuration = [ordered]@{
    saved = (Get-Date).ToString('o'); board = $Fqbn; cpp_flags = $ExtraCppFlags; jobs = $Jobs
    cli = $ArduinoCli; cli_version = (& $ArduinoCli version | Out-String).Trim()
    run_directory = $RunDirectory; report = $report
    method = 'Unchanged sketch bytes staged under LoomAudit. Main .ino filename only is renamed. Arduino CLI compiles and links each sketch, using the normal Arduino CLI cache and exporting separate firmware snapshots. Original-folder audit remains available in loom_compile_engine.bat.'
    precompiled_library = $PrecompiledLoom
    verified_library_reuse = [bool]$ReuseVerifiedLibraryObjects
    warning_note = 'Cached libraries do not repeat warnings. Raw per-sketch logs and firmware snapshots are retained; count unique warning locations across the run, not warnings per sketch as independent clean-build evidence.'
}
$configuration | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $report 'configuration.json')
$sourceHashes = @(Get-ChildItem -LiteralPath (Join-Path $loomRoot 'src') -Recurse -File | ForEach-Object {
    [ordered]@{ path = $_.FullName; sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
})
$sourceHashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $report 'source_hashes.json')
$verifiedCache = Join-Path $RunDirectory 'verified-libraries'
$hookScript = Join-Path $PSScriptRoot 'verified_library_cache.py'
if ($ReuseVerifiedLibraryObjects -and -not (Test-Path -LiteralPath $PythonPath -PathType Leaf)) {
    throw 'PythonPath must point to an installed Python interpreter for verified object reuse.'
}
$rows = @()
$inputs = @()
$index = 0
foreach ($sketch in $sketches) {
    ++$index
    $name = Split-Path -Leaf $sketch
    $main = Join-Path $sketch ($name + '.ino')
    $prefix = '{0:d3}_{1}' -f $index, ($name -replace '[^a-zA-Z0-9_.-]', '_')
    $log = Join-Path $logs ($prefix + '.log')
    $exitCode = 1
    $elapsed = [System.Diagnostics.Stopwatch]::StartNew()
    Write-Output "[$index/$($sketches.Count)] $name - compiling"
    if (-not (Test-Path -LiteralPath $main -PathType Leaf)) {
        'Missing main sketch: ' + $main | Set-Content -LiteralPath $log
    } else {
        # The resolved paths below are constructed from our validated temporary root.
        foreach ($target in @($stage)) {
            $resolvedTarget = [System.IO.Path]::GetFullPath($target)
            if (-not $resolvedTarget.StartsWith($RunDirectory + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
                throw 'Temporary cleanup escaped this audit directory.'
            }
            if (Test-Path -LiteralPath $resolvedTarget) {
                Get-ChildItem -LiteralPath $resolvedTarget -Force | Remove-Item -Recurse -Force
            }
        }
        $compileFiles = @(Get-ChildItem -LiteralPath $sketch -File | Where-Object { $_.Extension -in @('.ino', '.cpp', '.c', '.h', '.hpp', '.S') })
        # Parent-relative includes depend on original folder layout; reject rather than guess.
        if ($compileFiles | Select-String -Pattern '^\s*#\s*include\s*["<]\.\.[/\\]') {
            throw "Use the original-folder audit for parent-relative includes: $sketch"
        }
        foreach ($file in $compileFiles) {
            $destination = if ($file.FullName -eq $main) { 'LoomAudit.ino' } else { $file.Name }
            Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $stage $destination)
            $inputs += [ordered]@{ sketch = $sketch; path = $file.FullName; sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash; staged_name = $destination }
        }
        if (Test-Path -LiteralPath (Join-Path $sketch 'src')) {
            Copy-Item -LiteralPath (Join-Path $sketch 'src') -Destination $stage -Recurse
        }
        $compileArguments = @('compile', '--fqbn', $Fqbn, '--warnings', 'all', '--jobs', $Jobs, '--no-color', '--libraries', $packageLibraries, '--output-dir', $build)
        if (Test-Path -LiteralPath $sketchbookLibraries) { $compileArguments += @('--libraries', $sketchbookLibraries) }
        if ($ReuseVerifiedLibraryObjects) {
            $hook = 'recipe.hooks.libraries.prebuild.1.pattern="' + $PythonPath + '" "' + $hookScript + '" restore --build "{build.path}" --cache "' + $verifiedCache + '"'
            $compileArguments += @('--build-property', $hook)
        }
        if ($PrecompiledLoom) {
            $compileArguments += @('--library', $PrecompiledLoom)
            $includeFlags = @($archiveManifest.include_folders | ForEach-Object { '-I"' + $_ + '"' }) -join ' '
            $compileArguments += @('--build-property', ('compiler.cpp.extra_flags=' + $includeFlags + ' ' + $ExtraCppFlags))
            # Arduino links the archive after directly compiled SDK objects. Ordinary
            # archive extraction avoids defining those SDK functions twice.
        } elseif ($ExtraCppFlags) { $compileArguments += @('--build-property', ('compiler.cpp.extra_flags=' + $ExtraCppFlags)) }
        $compileArguments += $stage
        & $ArduinoCli @compileArguments *> $log
        $exitCode = $LASTEXITCODE
        if ($ReuseVerifiedLibraryObjects -and $exitCode -eq 0) {
            # The constant staging path has one Arduino cache key. Locate it by its options,
            # not by guessing a hash. Cache saving never changes the installed libraries.
            $cliCache = Join-Path $env:LOCALAPPDATA 'arduino/sketches'
            $matchingBuild = @(Get-ChildItem -LiteralPath $cliCache -Directory | Where-Object {
                $optionsPath = Join-Path $_.FullName 'build.options.json'
                if (Test-Path -LiteralPath $optionsPath) {
                    (Get-Content -LiteralPath $optionsPath -Raw | ConvertFrom-Json).sketchLocation -eq $stage
                }
            })
            if ($matchingBuild.Count -ne 1) { throw 'Could not identify this staged sketch cache uniquely.' }
            & $PythonPath $hookScript save --build $matchingBuild[0].FullName --cache $verifiedCache >> $log
            if ($LASTEXITCODE -ne 0) { throw 'Could not save verified object cache.' }
        }
        if ($exitCode -eq 0) {
            foreach ($extension in @('bin', 'elf', 'hex', 'map', 'uf2')) {
                $product = Join-Path $build ('LoomAudit.ino.' + $extension)
                if (Test-Path -LiteralPath $product) { Copy-Item -LiteralPath $product -Destination (Join-Path $firmware ($prefix + '.' + $extension)) }
            }
        }
    }
    $elapsed.Stop()
    $loomWarnings = Join-Path $logs ($prefix + '.loom-warnings.txt')
    $externalWarnings = Join-Path $logs ($prefix + '.external-warnings.txt')
    & (Join-Path $PSScriptRoot 'loom_warning_filter.ps1') -Action append -Scope loom -LogPath $log -OutputPath $loomWarnings -LoomDir $loomRoot -AdditionalLoomDirs @($PrecompiledLoom) -BuildDir $build -SketchDir $stage
    & (Join-Path $PSScriptRoot 'loom_warning_filter.ps1') -Action append -Scope external -LogPath $log -OutputPath $externalWarnings -LoomDir $loomRoot -AdditionalLoomDirs @($PrecompiledLoom) -BuildDir $build -SketchDir $stage
    $warningCount = @(Select-String -LiteralPath $log -Pattern 'warning:').Count
    $loomWarningCount = if (Test-Path -LiteralPath $loomWarnings) { @(Get-Content -LiteralPath $loomWarnings).Count } else { 0 }
    $result = if ($exitCode -eq 0) { 'PASS' } else { 'FAIL' }
    $rows += [pscustomobject]@{ index = $index; result = $result; exit_code = $exitCode; warnings = $warningCount; loom_warnings = $loomWarningCount; seconds = [math]::Round($elapsed.Elapsed.TotalSeconds, 1); sketch = $sketch; log = $log }
    $rows | Export-Csv -LiteralPath (Join-Path $report 'compile_report.csv') -NoTypeInformation
    $inputs | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $report 'sketch_hashes.json')
    Write-Output "$result $name ($([math]::Round($elapsed.Elapsed.TotalSeconds)) s, $loomWarningCount Loom warnings, $warningCount total)"
    if ($exitCode -ne 0) { Get-Content -LiteralPath $log | Select-String 'error:|undefined reference|Compilation error|Missing main sketch' }
}
if (@(Get-ChildItem -LiteralPath (Join-Path $loomRoot 'src') -Recurse -File).Count -ne $sourceHashes.Count) {
    throw 'Source file inventory changed during audit; rerun the audit.'
}
foreach ($source in $sourceHashes) {
    if ((Get-FileHash -LiteralPath $source.path -Algorithm SHA256).Hash -ne $source.sha256) {
        throw "Source changed during audit; rerun affected builds: $($source.path)"
    }
}
$failures = @($rows | Where-Object { $_.exit_code -ne 0 })
Write-Output "Completed $($rows.Count) sketches: $($rows.Count - $failures.Count) passed, $($failures.Count) failed. Report: $report"
Write-Output "RunDirectory: $RunDirectory"
if ($failures.Count) { throw 'Sketch compilation failures; see saved report and raw logs.' }

