# Packages a built plugin into a Windows release zip.
#   package-release-windows.ps1 <version> [build dir] [source dir] [output dir]
# Used by .github/workflows/release.yml and by convert-to-windows.bat.
param(
    [Parameter(Mandatory = $true)][string] $Version,
    [string] $Build = "build",
    [string] $Source = ".",
    [string] $Dist = "dist"
)
$ErrorActionPreference = "Stop"

$bundle = Join-Path $Build "EnhMaster_artefacts/Release/VST3/ENH Master.vst3"
if (-not (Test-Path $bundle)) { throw "no VST3 bundle at $bundle - build first" }
$standalone = Join-Path $Build "EnhMaster_artefacts/Release/Standalone/ENH Master.exe"

$name = "ENH-Master-$Version-windows-x64"
$stage = Join-Path $Dist $name
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force -Path $stage | Out-Null

Copy-Item -Recurse $bundle $stage
if (Test-Path $standalone) { Copy-Item $standalone $stage }
foreach ($f in "README.md", "LICENSE", "NOTICE") {
    $p = Join-Path $Source $f
    if (Test-Path $p) { Copy-Item $p $stage }
}

@"
ENH Master $Version - installation (Windows, VST3)
===================================================

1. Copy the "ENH Master.vst3" folder into your VST3 folder:

       C:\Program Files\Common Files\VST3

   (Copying there asks for administrator rights.)

2. Rescan plugins in your host (Reaper, FL Studio, Ableton Live, Bitwig, Cubase...).

"ENH Master.exe" is the standalone app: it runs without a host.

Requirements: 64-bit Windows 10 or 11 and a GPU with OpenGL 3.2.
"@ | Set-Content -Encoding UTF8 (Join-Path $stage "INSTALL.txt")

$zip = Join-Path $Dist "$name.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path $stage -DestinationPath $zip
Write-Host "Packaged $zip"
