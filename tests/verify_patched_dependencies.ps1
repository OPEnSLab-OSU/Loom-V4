param(
    [string]$PackageLibraries
)

$ErrorActionPreference = 'Stop'
$loomRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path

if ([string]::IsNullOrWhiteSpace($PackageLibraries)) {
    $PackageLibraries = Split-Path -Parent $loomRoot
}
$PackageLibraries = [System.IO.Path]::GetFullPath($PackageLibraries)
$platformRoot = Split-Path -Parent $PackageLibraries

$dependencies = @(
    @{
        Name = 'OPEnS_RTC'
        MarkerFile = 'src/OPEnS_RTC.h'
        Marker = 'LOOM_OPENS_RTC_PATCH_LEVEL 1'
    },
    @{
        Name = 'SparkFun_AS726X'
        MarkerFile = 'src/AS726X.h'
        Marker = 'LOOM_AS726X_PATCH_LEVEL 1'
    },
    @{
        Name = 'SparkFun_Spectral_Triad_AS7265X'
        MarkerFile = 'src/SparkFun_AS7265X.h'
        Marker = 'LOOM_AS7265X_PATCH_LEVEL 1'
    }
)

$failed = $false

foreach ($dependency in $dependencies) {
    $dependencyFailed = $false
    $authoritative = Join-Path (Join-Path $loomRoot 'dependencies') $dependency.Name
    $installed = Join-Path $PackageLibraries $dependency.Name

    if (-not (Test-Path -LiteralPath $authoritative -PathType Container)) {
        Write-Error "Missing authoritative dependency: $authoritative" -ErrorAction Continue
        $failed = $true
        continue
    }
    if (-not (Test-Path -LiteralPath $installed -PathType Container)) {
        Write-Error "Missing package-level dependency: $installed" -ErrorAction Continue
        $failed = $true
        continue
    }

    $authoritativeFiles = @(
        Get-Item -LiteralPath (Join-Path $authoritative 'library.properties')
        Get-ChildItem -LiteralPath (Join-Path $authoritative 'src') -Recurse -File
    )

    foreach ($sourceFile in $authoritativeFiles) {
        $relative = [System.IO.Path]::GetRelativePath($authoritative, $sourceFile.FullName)
        $installedFile = Join-Path $installed $relative

        if (-not (Test-Path -LiteralPath $installedFile -PathType Leaf)) {
            Write-Error "Missing package build input: $installedFile" -ErrorAction Continue
            $dependencyFailed = $true
            continue
        }

        $sourceHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $sourceFile.FullName).Hash
        $installedHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $installedFile).Hash
        if ($sourceHash -ne $installedHash) {
            Write-Error "Patched dependency drift: $($dependency.Name)/$relative" `
                -ErrorAction Continue
            $dependencyFailed = $true
        }
    }

    $markerPath = Join-Path $installed $dependency.MarkerFile
    if ((Get-Content -LiteralPath $markerPath -Raw) -notmatch [regex]::Escape($dependency.Marker)) {
        Write-Error "Missing Loom patch marker in $markerPath" -ErrorAction Continue
        $dependencyFailed = $true
    }

    if ($dependencyFailed) {
        $failed = $true
    } else {
        Write-Output "PASS $($dependency.Name)"
    }
}

$officialCoreFiles = @(
    @{
        Relative = 'cores/arduino/SERCOM.cpp'
        Sha256 = 'E0ABC9FAF850762A966D0C6A64A048CEC8B8A0C0E37290454FE97A840510ADB2'
    },
    @{
        Relative = 'libraries/Wire/Wire.cpp'
        Sha256 = '4975914E951A003DE39EB3A6DA72E721B48F1156E890A0A3A2EF8A81C871592C'
    }
)

foreach ($coreFile in $officialCoreFiles) {
    $installed = Join-Path $platformRoot $coreFile.Relative
    $noteSnapshot = Join-Path (Join-Path $loomRoot 'dependencies/Loom_SAMD21_Core_Patches') $coreFile.Relative

    if (-not (Test-Path -LiteralPath $installed -PathType Leaf)) {
        Write-Error "Missing official package core file: $installed" -ErrorAction Continue
        $failed = $true
        continue
    }

    $installedHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $installed).Hash
    if ($installedHash -ne $coreFile.Sha256) {
        Write-Error "Official Loom 4.9 core drift: $($coreFile.Relative)" -ErrorAction Continue
        $failed = $true
    } else {
        Write-Output "PASS official $($coreFile.Relative)"
    }

    if (-not (Test-Path -LiteralPath $noteSnapshot -PathType Leaf)) {
        Write-Warning "Missing inactive core investigation note: $noteSnapshot"
    }
}

if ($failed) {
    Write-Error 'Dependency verification failed. Install the vendored third-party libraries and restore the official Loom 4.9 board core before packaging.'
    exit 1
}

Write-Output 'All Loom-patched third-party libraries match their reviewed copies, and the active SAMD21 core matches the official Loom 4.9 release.'
Write-Output 'Experimental SERCOM/Wire snapshots under Loom/dependencies are notes only and are not active package inputs.'
