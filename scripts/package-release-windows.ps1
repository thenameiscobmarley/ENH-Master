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

# Fingerprints of everything in a folder, as "hash  path" lines (the format sha256sum -c reads)
function Write-Checksums([string] $folder) {
    $root = (Resolve-Path $folder).Path
    $lines = Get-ChildItem -Recurse -File $folder | Where-Object { $_.Name -ne "CHECKSUMS.txt" } | Sort-Object FullName | ForEach-Object {
        $rel = $_.FullName.Substring($root.Length).TrimStart('\').Replace('\', '/')
        "{0}  ./{1}" -f (Get-FileHash -Algorithm SHA256 $_.FullName).Hash.ToLower(), $rel
    }
    $lines | Set-Content -Encoding ASCII (Join-Path $folder "CHECKSUMS.txt")
}

$bundle = Join-Path $Build "EnhMaster_artefacts/Release/VST3/ENH Master.vst3"
if (-not (Test-Path $bundle)) { throw "no VST3 bundle at $bundle - build first" }
$standalone = Join-Path $Build "EnhMaster_artefacts/Release/Standalone/ENH Master.exe"
if (-not (Test-Path $standalone)) { throw "no standalone app at $standalone - the release needs it (it is the gamers' router)" }

$name = "ENH-Master-$Version-windows-x64"
$stage = Join-Path $Dist $name
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force -Path $stage | Out-Null

Copy-Item -Recurse $bundle $stage
Copy-Item $standalone $stage
foreach ($f in "README.md", "LICENSE", "NOTICE", "scripts/HOW-TO-CHECK.txt") {
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

"ENH Master.exe" is the standalone app: the rack with a router above it that puts the rack into
your PC's own audio (games, Discord, everything) and takes it out again - see QUICK-START-GAMERS.txt.

Requirements: 64-bit Windows 10 or 11 and a GPU with OpenGL 3.2.
"@ | Set-Content -Encoding UTF8 (Join-Path $stage "INSTALL.txt")

# --- The gamers' app on its own: just the exe and how to use it -----------------------------
$quick = @"
ENH Master $Version - the app for gamers (Windows)
=================================================

ENH Master.exe runs the ENH Master rack on everything your PC plays - no DAW, no plugin host.

ONE-TIME SETUP
  1. Install VB-Audio Cable (free, vb-audio.com/Cable) and restart. It gives Windows a playback
     device nobody listens to, which is where the rack picks your audio up. (ENH Master can open the
     download page for you: RACK INPUT > "Use VB-Audio Cable (Recommended)".)
  2. Start ENH Master.exe (no installation needed; keep it anywhere).

EVERY DAY
  - SOURCE:     Whole system (everything) or Chosen apps (tick your game / Discord under Apps...).
  - RACK INPUT: Use VB-Audio Cable (Recommended) - picked for you once it is installed.
  - LISTEN ON:  your headset or speakers.
  - LEVEL:      -18 LUFS makes the game, music and voice chat equally loud (optional).
  - Press INSERT RACK. Press it again (RACK IN - REMOVE) to put everything back as it was.

  To switch headset or speakers: REMOVE, pick another LISTEN ON, INSERT RACK again.
  DELAY (bottom right, while the rack is in) says how much later you hear the sound; under 30 ms
  nobody notices in a game. Lower the buffer size in ... > Audio settings if it is higher.
  Closing the window with the rack in keeps it running in the tray (click the icon to bring it back).
  The ... menu has "Start with the computer": it then starts in the tray with the rack in, every time.

SAFE BY DESIGN
  Your original sound settings are written down before anything changes. Remove, quit, a crash, an
  unplugged headset: the app (or its watchdog, or the next start) puts your audio back.

Requirements: 64-bit Windows 10 or 11 (per-app routing needs 1803 or later), a GPU with OpenGL 3.2.
"@
$quick | Set-Content -Encoding UTF8 (Join-Path $stage "QUICK-START-GAMERS.txt")

$gname = "ENH-Master-$Version-windows-gamer-app"
$gstage = Join-Path $Dist $gname
if (Test-Path $gstage) { Remove-Item -Recurse -Force $gstage }
New-Item -ItemType Directory -Force -Path $gstage | Out-Null
Copy-Item $standalone $gstage
$quick | Set-Content -Encoding UTF8 (Join-Path $gstage "QUICK-START-GAMERS.txt")
foreach ($f in "LICENSE", "NOTICE", "scripts/HOW-TO-CHECK.txt") {
    $p = Join-Path $Source $f
    if (Test-Path $p) { Copy-Item $p $gstage }
}
Write-Checksums $gstage
$gzip = Join-Path $Dist "$gname.zip"
if (Test-Path $gzip) { Remove-Item -Force $gzip }
Compress-Archive -Path $gstage -DestinationPath $gzip
Write-Host "Packaged $gzip"

Write-Checksums $stage
$zip = Join-Path $Dist "$name.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path $stage -DestinationPath $zip
Write-Host "Packaged $zip"
