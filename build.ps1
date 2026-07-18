<#
.SYNOPSIS
    Compiles both boards and exports the merged factory image each into
    web-flasher/firmware/<board>/, ready to commit and push.

.DESCRIPTION
    ESPHome/PlatformIO already merge the bootloader, partition table, OTA
    marker, and application into one flashable-at-offset-0 image named
    firmware.factory.bin. This script just compiles and copies that single
    file per board - no manual offset bookkeeping needed.

    esphome.build_path is not honored by "esphome compile" in this ESPHome
    version, so the build always lands in the default, hidden
    .esphome/build/<device_name>/ cache directory regardless of YAML config.

    The Web Flasher manifests (web-flasher/manifest_240.json,
    web-flasher/manifest_480.json) fetch this binary straight from GitHub
    (raw.githubusercontent.com), not from wherever web-flasher/index.html is
    hosted. So the only thing that has to happen after this script is
    "git add web-flasher/firmware", commit, and push - no separate upload to
    the web space.

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

foreach ($board in $boards) {
    $deviceName = $deviceNames[$board]

    Write-Host "==> Compiling $board.yaml"
    esphome compile "$board.yaml"
    if ($LASTEXITCODE -ne 0) {
        throw "esphome compile $board.yaml failed with exit code $LASTEXITCODE"
    }

    $pioenvDir = Join-Path ".esphome" "build\$deviceName\.pioenvs\$deviceName"
    $destDir = Join-Path "web-flasher\firmware" $board
    New-Item -ItemType Directory -Force -Path $destDir | Out-Null

    $src = Join-Path $pioenvDir "firmware.factory.bin"
    if (-not (Test-Path $src)) {
        Write-Error "Missing $src"
        Write-Error "PlatformIO may have named this file differently for your ESPHome/IDF version."
        Write-Error "List $pioenvDir\ and update this script plus web-flasher\manifest_*.json to match."
        exit 1
    }

    Write-Host "==> Exporting firmware.factory.bin to $destDir\"
    Copy-Item -Path $src -Destination (Join-Path $destDir "firmware.factory.bin") -Force
}

Write-Host "==> Done. Review the diff, then:"
Write-Host "    git add web-flasher/firmware"
Write-Host "    git commit -m `"chore(web-flasher): update firmware binaries`""
Write-Host "    git push"
