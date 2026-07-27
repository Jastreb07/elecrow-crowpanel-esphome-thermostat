<#
.SYNOPSIS
    Bumps firmware/version.txt (Patch/Minor/Major) - the single source of
    truth for the firmware version.

.DESCRIPTION
    Building and releasing firmware no longer happens locally - pushing
    firmware/version.txt to master (via a merged PR) triggers
    .github/workflows/release-firmware.yml, which compiles both boards,
    generates manifest.json/ota-manifest.json/checksums.txt, and publishes
    a GitHub Release. This script's only job is choosing and writing the
    next version number, exactly like the interactive prompt the old
    (removed) compile-and-export version of this script had - just without
    the compile step, since that's CI's job now (see
    docs/GITHUB_RELEASES_PLAN.md).

    After running this script, commit the updated firmware/version.txt and
    open a PR. Merging it to master triggers the release.

.EXAMPLE
    .\build.ps1
#>

$ErrorActionPreference = "Stop"

Set-Location -Path $PSScriptRoot

$versionPath = Join-Path "firmware" "version.txt"
if (-not (Test-Path $versionPath)) {
    throw "Missing $versionPath"
}
$currentVersion = (Get-Content -Path $versionPath -Raw).Trim()
Write-Host "==> Current firmware version: $currentVersion"

Write-Host ""
Write-Host "Bump version how?"
Write-Host "  [1] Patch"
Write-Host "  [2] Minor"
Write-Host "  [3] Major"
$choice = Read-Host "Choice (1-3)"

$parts = $currentVersion.Split(".")
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

Set-Content -Path $versionPath -Value $newVersion -NoNewline -Encoding utf8
Add-Content -Path $versionPath -Value "" -Encoding utf8

Write-Host "==> $versionPath -> $newVersion"
Write-Host ""
Write-Host "==> Don't forget to add a `"## $newVersion`" section to CHANGELOG.md"
Write-Host "    (used as the GitHub Release body by the workflow)."
Write-Host ""
Write-Host "==> Done. Review the diff, then:"
Write-Host "    git add firmware/version.txt CHANGELOG.md"
Write-Host "    git commit -m `"chore(firmware): bump version to $newVersion`""
Write-Host "    (open a PR, merging to master triggers the release workflow)"
