<#
.SYNOPSIS
    Compiles both boards, exports the factory + OTA images each into
    firmware/<board>/, and bumps manifest.json + ota-manifest.json to the
    same new version once the build succeeded.

.DESCRIPTION
    ESPHome/PlatformIO already merge the bootloader, partition table, OTA
    marker, and application into one flashable-at-offset-0 image named
    firmware.factory.bin (used by the browser flasher's manifest.json), and
    separately produce firmware.ota.bin - the plain app-partition image used
    by the on-device OTA updater's ota-manifest.json (see
    docs/OTA_UPDATE_PLAN.md). This script just compiles and copies both -
    no manual offset bookkeeping needed.

    esphome.build_path is not honored by "esphome compile" in this ESPHome
    version, so the build always lands in the default, hidden
    .esphome/build/<device_name>/ cache directory regardless of YAML config.

    Before compiling, it lists the version currently in each board's
    manifest.json and asks whether to bump it as a Patch, Minor, or Major
    release. Both manifest.json and ota-manifest.json are only updated at
    the very end, after both boards compiled successfully, and always to
    the same version - a failed build never leaves a bumped version behind
    with no matching binary, and the browser flasher / OTA updater never
    disagree about the current version.

    Both manifests fetch their binary straight from GitHub
    (raw.githubusercontent.com), not from wherever the docs site is hosted.
    So the only thing that has to happen after this script is
    "git add firmware", commit, and push - no separate upload anywhere.

.EXAMPLE
    .\build.ps1
#>

$ErrorActionPreference = "Stop"

Set-Location -Path $PSScriptRoot

$boards = @("thermostat_240", "thermostat_480")
# device_name substitution in each board YAML drops the underscore
# (thermostat_240 -> thermostat240).
$deviceNames = @{
    thermostat_240 = "thermostat240"
    thermostat_480 = "thermostat480"
}

# ------------------------------------------------------------------
# Read the current version out of every manifest.json before touching
# anything, and ask how to bump it. Neither manifest is written yet - only
# after a successful build, at the very end.
# ------------------------------------------------------------------
$manifestPaths = @{}
$otaManifestPaths = @{}
$currentVersions = @{}

Write-Host "==> Current firmware versions:"
foreach ($board in $boards) {
    $manifestPath = Join-Path "firmware" $board "manifest.json"
    $otaManifestPath = Join-Path "firmware" $board "ota-manifest.json"
    $manifestPaths[$board] = $manifestPath
    $otaManifestPaths[$board] = $otaManifestPath
    if (-not (Test-Path $manifestPath)) {
        throw "Missing $manifestPath"
    }
    if (-not (Test-Path $otaManifestPath)) {
        throw "Missing $otaManifestPath"
    }
    $version = (Get-Content -Path $manifestPath -Raw | ConvertFrom-Json).version
    $currentVersions[$board] = $version
    Write-Host "    $board : $version"
}

$distinctVersions = $currentVersions.Values | Select-Object -Unique
if ($distinctVersions.Count -gt 1) {
    Write-Warning "Board manifests have different versions ($($distinctVersions -join ', ')) - bumping from the highest one."
}
$baseVersion = $distinctVersions | Sort-Object { [version]$_ } | Select-Object -Last 1

Write-Host ""
Write-Host "Bump version how?"
Write-Host "  [1] Patch"
Write-Host "  [2] Minor"
Write-Host "  [3] Major"
$choice = Read-Host "Choice (1-3)"

$parts = $baseVersion.Split(".")
[int]$major = $parts[0]
[int]$minor = $parts[1]
[int]$patch = $parts[2]

switch ($choice) {
    "1" { $patch++ }
    "2" { $minor++; $patch = 0 }
    "3" { $major++; $minor = 0; $patch = 0 }
    default { throw "Invalid choice '$choice' - expected 1, 2, or 3." }
}
$newVersion = "$major.$minor.$patch"
Write-Host "==> New version will be $newVersion - applied to both manifests only after a successful build."
Write-Host ""

# ------------------------------------------------------------------
# Compile + export. Manifests are untouched through this whole loop. MD5s
# are computed now (from the freshly built firmware.ota.bin) but only
# written into ota-manifest.json in the final step below.
# ------------------------------------------------------------------
$otaMd5s = @{}

foreach ($board in $boards) {
    $deviceName = $deviceNames[$board]

    Write-Host "==> Compiling $board.yaml"
    esphome compile "$board.yaml"
    if ($LASTEXITCODE -ne 0) {
        throw "esphome compile $board.yaml failed with exit code $LASTEXITCODE"
    }

    $pioenvDir = Join-Path ".esphome" "build\$deviceName\.pioenvs\$deviceName"
    $destDir = Join-Path "firmware" $board
    New-Item -ItemType Directory -Force -Path $destDir | Out-Null

    $factorySrc = Join-Path $pioenvDir "firmware.factory.bin"
    if (-not (Test-Path $factorySrc)) {
        Write-Error "Missing $factorySrc"
        Write-Error "PlatformIO may have named this file differently for your ESPHome/IDF version."
        Write-Error "List $pioenvDir\ and update this script plus manifest.json to match."
        exit 1
    }
    $otaSrc = Join-Path $pioenvDir "firmware.ota.bin"
    if (-not (Test-Path $otaSrc)) {
        Write-Error "Missing $otaSrc"
        Write-Error "PlatformIO may have named this file differently for your ESPHome/IDF version."
        Write-Error "List $pioenvDir\ and update this script plus ota-manifest.json to match."
        exit 1
    }

    Write-Host "==> Exporting firmware.factory.bin + firmware.ota.bin to $destDir\"
    Copy-Item -Path $factorySrc -Destination (Join-Path $destDir "firmware.factory.bin") -Force
    Copy-Item -Path $otaSrc -Destination (Join-Path $destDir "firmware.ota.bin") -Force

    $otaMd5s[$board] = (Get-FileHash -Path $otaSrc -Algorithm MD5).Hash.ToLower()
}

# ------------------------------------------------------------------
# Both boards built successfully - now, and only now, bump both manifests
# to the same version (and refresh ota-manifest.json's md5).
# ------------------------------------------------------------------
Write-Host "==> Updating manifests to $newVersion"
foreach ($board in $boards) {
    $manifestPath = $manifestPaths[$board]
    $raw = Get-Content -Path $manifestPath -Raw
    $updated = $raw -replace '"version":\s*"[0-9]+\.[0-9]+\.[0-9]+"', ('"version": "' + $newVersion + '"')
    Set-Content -Path $manifestPath -Value $updated -NoNewline -Encoding utf8
    Write-Host "    $manifestPath -> $newVersion"

    $otaManifestPath = $otaManifestPaths[$board]
    $otaRaw = Get-Content -Path $otaManifestPath -Raw
    $otaUpdated = $otaRaw -replace '"version":\s*"[0-9]+\.[0-9]+\.[0-9]+"', ('"version": "' + $newVersion + '"')
    $otaUpdated = $otaUpdated -replace '"md5":\s*"[0-9a-fA-F]{32}"', ('"md5": "' + $otaMd5s[$board] + '"')
    Set-Content -Path $otaManifestPath -Value $otaUpdated -NoNewline -Encoding utf8
    Write-Host "    $otaManifestPath -> $newVersion (md5 $($otaMd5s[$board]))"
}

Write-Host "==> Done. Review the diff, then:"
Write-Host "    git add firmware"
Write-Host "    git commit -m `"chore(firmware): release $newVersion`""
Write-Host "    git push"
