<# : convert-to-windows.bat
@echo off
rem ============================================================================
rem  ENH Master - turn any release into a Windows plugin.
rem
rem  Double-click this file. It lists every ENH Master release on GitHub, you pick
rem  one, and it gives you the Windows VST3 (and the standalone app):
rem    - releases that already have a Windows build are downloaded;
rem    - older releases are built from their source for you (it can install the
rem      free build tools it needs: Git, CMake and Visual Studio Build Tools).
rem
rem  Unattended use (for scripts / CI), all optional:
rem    ENH_TAG=v1.3.0     the release (tag, or its number in the list; default newest)
rem    ENH_MODE=source    download (default when there is a Windows build) or source
rem    ENH_OUT=C:\path    where the result goes (default: your Desktop)
rem    ENH_YES=1          never ask; take the defaults; do not install or pause
rem ============================================================================
setlocal
title ENH Master - convert a release to Windows
powershell -NoProfile -ExecutionPolicy Bypass -Command "iex (${%~f0} | Out-String)"
set "ENH_EXIT=%ERRORLEVEL%"
if not defined ENH_YES pause
exit /b %ENH_EXIT%
#>

# ---------------------------------------------------------------------------------------------------
# The PowerShell part (Windows PowerShell 5.1 or newer). The lines above are skipped as a comment.
# ---------------------------------------------------------------------------------------------------
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'   # Invoke-WebRequest is many times faster without the progress bar
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$repo    = 'thenameiscobmarley/ENH-Master'
$hwkRepo = 'thenameiscobmarley/HardwareKit'
$unattended = [bool] $env:ENH_YES

function Say ([string] $text, [string] $colour = 'Gray') { Write-Host $text -ForegroundColor $colour }
function Ask ([string] $question, [string] $default)
{
    if ($unattended) { return $default }
    $answer = Read-Host "$question [$default]"
    if ([string]::IsNullOrWhiteSpace($answer)) { return $default }
    return $answer.Trim()
}
function Run ([string] $what, [scriptblock] $command)
{
    Say "  $what" 'DarkGray'
    & $command
    if ($LASTEXITCODE -ne 0) { throw "$what failed (exit code $LASTEXITCODE)" }
}
function RefreshPath
{
    $env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')
}
function FindVisualStudio
{
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { return $null }
    $path = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ([string]::IsNullOrWhiteSpace($path)) { return $null }
    return $path
}

try
{
    Say ''
    Say 'ENH Master - convert a release to Windows' 'Cyan'
    Say '-----------------------------------------' 'Cyan'

    # --- 1. every release ------------------------------------------------------------------------
    Say 'Looking up the releases on GitHub...'
    $headers = @{ 'User-Agent' = 'ENH-Master-convert-to-windows'; 'Accept' = 'application/vnd.github+json' }
    $releases = @(Invoke-RestMethod -Uri "https://api.github.com/repos/$repo/releases?per_page=100" -Headers $headers |
                  Where-Object { -not $_.draft } |
                  Sort-Object -Property { [datetime] $_.published_at } -Descending)
    if ($releases.Count -eq 0) { throw 'No releases were found.' }

    Say ''
    for ($i = 0; $i -lt $releases.Count; $i++)
    {
        $r = $releases[$i]
        $win = @($r.assets | Where-Object { $_.name -like '*windows*.zip' })
        $date = ([datetime] $r.published_at).ToString('yyyy-MM-dd')
        $note = if ($win.Count -gt 0) { 'Windows build ready - quick download' } else { 'built from source (10-30 min)' }
        $newest = if ($i -eq 0) { '  (newest)' } else { '' }
        Say ('  [{0,2}]  {1,-10} {2}   {3}{4}' -f ($i + 1), $r.tag_name, $date, $note, $newest) $(if ($win.Count -gt 0) { 'White' } else { 'Gray' })
    }
    Say ''

    # --- 2. which one --------------------------------------------------------------------------
    $pick = if ($env:ENH_TAG) { $env:ENH_TAG } else { Ask 'Which release? Type its number' '1' }
    $release = $null
    $number = 0
    if ([int]::TryParse($pick, [ref] $number) -and $number -ge 1 -and $number -le $releases.Count) { $release = $releases[$number - 1] }
    else { $release = $releases | Where-Object { $_.tag_name -eq $pick -or $_.tag_name -eq "v$pick" } | Select-Object -First 1 }
    if ($null -eq $release) { throw "There is no release '$pick'." }
    $tag = $release.tag_name
    Say "Converting $tag ($($release.name))." 'Cyan'

    $outRoot = if ($env:ENH_OUT) { $env:ENH_OUT } else { [Environment]::GetFolderPath('Desktop') }
    $out = Join-Path $outRoot "ENH-Master-$tag-windows"
    $asset = @($release.assets | Where-Object { $_.name -like '*windows*.zip' }) | Select-Object -First 1

    $mode = 'source'
    if ($asset)
    {
        $mode = if ($env:ENH_MODE) { $env:ENH_MODE } else {
            Say 'This release already has a Windows build.'
            Say '  1) download it (seconds)'
            Say '  2) build it yourself from the source (needs the build tools, 10-30 minutes)'
            if ((Ask 'Choose' '1') -eq '2') { 'source' } else { 'download' }
        }
    }
    if (Test-Path $out) { Remove-Item -Recurse -Force $out }
    New-Item -ItemType Directory -Force -Path $out | Out-Null

    if ($mode -ne 'source')
    {
        # --- 3a. download the ready-made build ---------------------------------------------------
        $zip = Join-Path $env:TEMP $asset.name
        Say "Downloading $($asset.name)..."
        Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $zip -Headers @{ 'User-Agent' = 'ENH-Master-convert-to-windows' }
        $unpack = Join-Path $env:TEMP "ENH-Master-$tag-unpack"
        if (Test-Path $unpack) { Remove-Item -Recurse -Force $unpack }
        Expand-Archive -Path $zip -DestinationPath $unpack -Force
        $inner = Get-ChildItem $unpack -Directory | Select-Object -First 1
        $from = if ($inner) { $inner.FullName } else { $unpack }
        Copy-Item -Recurse -Force (Join-Path $from '*') $out
        Remove-Item -Recurse -Force $unpack, $zip
    }
    else
    {
        # --- 3b. build it from its source ---------------------------------------------------------
        Say 'Checking the build tools...'
        RefreshPath
        $missing = @()
        if (-not (Get-Command git -ErrorAction SilentlyContinue))   { $missing += 'Git' }
        if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { $missing += 'CMake' }
        if (-not (FindVisualStudio))                                { $missing += 'Visual Studio Build Tools (C++)' }
        if ($missing.Count -gt 0)
        {
            Say ('Missing: ' + ($missing -join ', ')) 'Yellow'
            $winget = Get-Command winget -ErrorAction SilentlyContinue
            if ($winget -and -not $unattended -and (Ask 'Install them now with winget? (y/n)' 'y') -like 'y*')
            {
                if ($missing -contains 'Git')   { Run 'Installing Git'   { winget install --id Git.Git -e --silent --accept-package-agreements --accept-source-agreements } }
                if ($missing -contains 'CMake') { Run 'Installing CMake' { winget install --id Kitware.CMake -e --silent --accept-package-agreements --accept-source-agreements } }
                if ($missing -like 'Visual Studio*')
                {
                    Run 'Installing Visual Studio Build Tools with C++ (this one takes a while)' {
                        winget install --id Microsoft.VisualStudio.2022.BuildTools -e --silent --accept-package-agreements --accept-source-agreements `
                            --override '--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended'
                    }
                }
                RefreshPath
            }
            else
            {
                Say 'Install these, then run this file again:' 'Yellow'
                Say '  winget install Git.Git'
                Say '  winget install Kitware.CMake'
                Say '  winget install Microsoft.VisualStudio.2022.BuildTools --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"'
                throw 'The build tools are missing.'
            }
            if (-not (Get-Command git -ErrorAction SilentlyContinue) -or -not (Get-Command cmake -ErrorAction SilentlyContinue) -or -not (FindVisualStudio))
            {
                throw 'The build tools are still not found. Restart Windows (or open a new window) and run this file again.'
            }
        }

        $root = Join-Path $env:LOCALAPPDATA 'ENH-Master-convert'
        $work = Join-Path $root $tag
        if (Test-Path $work) { Remove-Item -Recurse -Force $work }
        New-Item -ItemType Directory -Force -Path $work | Out-Null
        $plugin = Join-Path $work 'plugin'
        $hwk = Join-Path $work 'HardwareKit'
        $juce = Join-Path $root 'JUCE'

        Run "Downloading the $tag source" { git clone --quiet --depth 1 --branch $tag "https://github.com/$repo.git" $plugin }
        if (-not (Test-Path (Join-Path $juce 'CMakeLists.txt')))
        {
            Run 'Downloading JUCE (once)' { git clone --quiet --depth 1 https://github.com/juce-framework/JUCE.git $juce }
        }

        # HardwareKit as it was when this release came out, plus today's Windows platform files
        # (releases before 1.2.4.1 were Linux-only: their UI polled the pointer from the X server)
        Run 'Downloading HardwareKit' { git clone --quiet "https://github.com/$hwkRepo.git" $hwk }
        $when = ([datetime] $release.published_at).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')
        $commit = (git -C $hwk rev-list -1 "--before=$when" HEAD)
        if ($commit) { Run "Using HardwareKit as of $when" { git -C $hwk checkout --quiet $commit } }
        $platformFiles = @('modules/hardwarekit/hardwarekit_x11.cpp', 'modules/hardwarekit/hardwarekit_win32.cpp',
                           'modules/hardwarekit/input/PointerPoller_win32.cpp', 'modules/hardwarekit/input/WindowVisibility_win32.cpp')
        Run 'Adding the Windows platform files' { git -C $hwk checkout --quiet origin/HEAD -- $platformFiles }
        $moduleHeader = Join-Path $hwk 'modules/hardwarekit/hardwarekit.h'
        $text = [IO.File]::ReadAllText($moduleHeader)
        if ($text -notmatch 'windowsLibs')
        {
            $text = $text -replace '(linuxLibs:\s+X11)', "`$1`r`n  windowsLibs:        user32"
            [IO.File]::WriteAllText($moduleHeader, $text)
        }

        # The release's build file, made Windows-friendly: X11 only on Linux, and no copy into the
        # system VST3 folder while building (that needs administrator rights; it is offered below)
        $cmakeFile = Join-Path $plugin 'CMakeLists.txt'
        $text = [IO.File]::ReadAllText($cmakeFile)
        if ($text -notmatch 'PLATFORM_ID:Linux')
        {
            $text = $text.Replace('find_package(X11 REQUIRED)', "if(UNIX AND NOT APPLE)`r`n    find_package(X11 REQUIRED)`r`nendif()")
            $text = $text.Replace('X11::X11', '$<$<PLATFORM_ID:Linux>:X11::X11>')
        }
        $text = $text -replace 'COPY_PLUGIN_AFTER_BUILD\s+TRUE', 'COPY_PLUGIN_AFTER_BUILD     FALSE'
        [IO.File]::WriteAllText($cmakeFile, $text)

        $build = Join-Path $work 'build'
        Run 'Configuring' {
            cmake -S $plugin -B $build -A x64 -DENH_BUILD_TESTS=OFF -DENH_BUILD_TOOLS=OFF -DENH_COPY_PLUGIN=OFF `
                  "-DJUCE_PATH=$juce" "-DHARDWAREKIT_PATH=$hwk"
        }
        Run 'Building (this is the long part)' { cmake --build $build --config Release --parallel }

        $bundle = Join-Path $build 'EnhMaster_artefacts/Release/VST3/ENH Master.vst3'
        if (-not (Test-Path $bundle)) { throw "The build finished but there is no VST3 at $bundle" }
        Copy-Item -Recurse $bundle $out
        $standalone = Join-Path $build 'EnhMaster_artefacts/Release/Standalone/ENH Master.exe'
        if (Test-Path $standalone) { Copy-Item $standalone $out }
        foreach ($f in 'README.md', 'LICENSE', 'NOTICE') { $p = Join-Path $plugin $f; if (Test-Path $p) { Copy-Item $p $out } }
    }

    # --- 4. done ---------------------------------------------------------------------------------
    $vst3 = Join-Path $out 'ENH Master.vst3'
    if (-not (Test-Path $vst3)) { throw "Something went wrong: there is no ENH Master.vst3 in $out" }
    Say ''
    Say "Done: ENH Master $tag for Windows is in" 'Green'
    Say "  $out" 'Green'

    $target = Join-Path $env:CommonProgramFiles 'VST3'
    if (-not $unattended -and (Ask "Install the plugin into $target now? It asks for administrator rights. (y/n)" 'y') -like 'y*')
    {
        $copy = "New-Item -ItemType Directory -Force -Path '$target' | Out-Null; Copy-Item -Recurse -Force '$vst3' '$target'"
        Start-Process powershell -Verb RunAs -Wait -ArgumentList '-NoProfile', '-Command', $copy
        if (Test-Path (Join-Path $target 'ENH Master.vst3')) { Say "Installed. Rescan plugins in your host." 'Green' }
        else { Say "It was not installed (was the administrator prompt declined?). Copy the folder by hand: $vst3 -> $target" 'Yellow' }
    }
    else
    {
        Say "To install: copy the 'ENH Master.vst3' folder into $target, then rescan plugins in your host."
    }
    if (-not $unattended) { Start-Process explorer.exe $out }
    exit 0
}
catch
{
    Say ''
    Say ('Stopped: ' + $_.Exception.Message) 'Red'
    exit 1
}
