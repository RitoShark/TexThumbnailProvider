<#
.SYNOPSIS
    Build (optional) and install a LOCAL TexThumbnailProvider.dll for testing.

.DESCRIPTION
    For developing/testing on your own machine. Copies a locally built DLL to the
    same location the public installer uses (%LOCALAPPDATA%\RitoShark\TexThumbnailProvider)
    and registers it per-user (HKCU, no admin). Re-run it after each rebuild —
    it unregisters the old copy first, so it's safe to run repeatedly.

.PARAMETER Build
    Build Release|x64 via msbuild before installing.

.PARAMETER Configuration
    Which build to install when not using -Dll. Release (default) or Debug.

.PARAMETER Dll
    Explicit path to a DLL to install (overrides -Configuration).

.EXAMPLE
    # Build fresh and install:
    .\scripts\install-local.ps1 -Build

.EXAMPLE
    # Install whatever is already built (Release):
    .\scripts\install-local.ps1

.EXAMPLE
    # Install a Debug build:
    .\scripts\install-local.ps1 -Configuration Debug
#>
[CmdletBinding()]
param(
    [switch]$Build,
    [ValidateSet('Release','Debug')]
    [string]$Configuration = 'Release',
    [string]$Dll
)

$ErrorActionPreference = 'Stop'

# Repo root = parent of this script's folder.
$RepoRoot   = Split-Path -Parent $PSScriptRoot
$Solution   = Join-Path $RepoRoot 'TexThumbnailProvider.sln'
$InstallDir = Join-Path $env:LOCALAPPDATA 'RitoShark\TexThumbnailProvider'
$TargetDll  = Join-Path $InstallDir 'TexThumbnailProvider.dll'

function Write-Step($m) { Write-Host "==> $m" -ForegroundColor Cyan }
function Write-Ok($m)   { Write-Host "    $m" -ForegroundColor Green }

Write-Host ""
Write-Host "TexThumbnailProvider - LOCAL install ($Configuration)" -ForegroundColor Magenta
Write-Host "=====================================================" -ForegroundColor Magenta

# --- locate msbuild (only needed for -Build) -----------------------------------
function Find-MSBuild {
    $candidates = @(
        'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe'
    )
    $mb = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $mb) {
        $c = Get-Command msbuild -ErrorAction SilentlyContinue
        if ($c) { $mb = $c.Source }
    }
    if (-not $mb) {
        $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
        if (Test-Path $vswhere) {
            $found = & $vswhere -latest -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe'
            if ($found) { $mb = ($found | Select-Object -First 1) }
        }
    }
    return $mb
}

if ($Build) {
    $mb = Find-MSBuild
    if (-not $mb) { throw "MSBuild not found. Open a 'Developer PowerShell for VS' or install VS 2022 build tools." }
    Write-Step "Building $Configuration|x64 ..."
    & $mb $Solution /p:Configuration=$Configuration /p:Platform=x64 /m /v:minimal /nologo
    if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)." }
    Write-Ok "Build succeeded"
}

# --- resolve the DLL to install ------------------------------------------------
if (-not $Dll) {
    $Dll = Join-Path $RepoRoot "x64\$Configuration\TexThumbnailProvider.dll"
}
if (-not (Test-Path $Dll)) {
    throw "DLL not found: $Dll`nBuild it first (msbuild ... /p:Configuration=$Configuration /p:Platform=x64) or pass -Build."
}
Write-Ok "Source DLL: $Dll"

# --- unregister any previous copy, then install + register ---------------------
if (Test-Path $TargetDll) {
    Write-Step "Unregistering previous copy ..."
    Start-Process regsvr32 -ArgumentList '/s','/u',"`"$TargetDll`"" -Wait -NoNewWindow -ErrorAction SilentlyContinue
}

if (-not (Test-Path $InstallDir)) { New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null }

Write-Step "Copying to $TargetDll ..."
# Unblock so the shell host will load it (esp. if it came from a download).
try { Unblock-File $Dll -ErrorAction SilentlyContinue } catch {}
Copy-Item -Force $Dll $TargetDll
Write-Ok "Installed"

Write-Step "Registering (HKCU) ..."
$proc = Start-Process regsvr32 -ArgumentList '/s',"`"$TargetDll`"" -Wait -PassThru -NoNewWindow
if ($proc.ExitCode -ne 0) {
    throw "regsvr32 failed (exit $($proc.ExitCode)). Make sure it's the x64 DLL on an x64 system."
}
Write-Ok "Registered"

Write-Host ""
Write-Host "Done. Open a folder of .tex files in Explorer to test." -ForegroundColor Green
Write-Host "Tip: thumbnails are cached per file. To force a repaint of a test file:" -ForegroundColor Gray
Write-Host "     (Get-Item .\some.tex).LastWriteTime = Get-Date" -ForegroundColor Gray
Write-Host "Uninstall: regsvr32 /u `"$TargetDll`"   (or scripts\uninstall-thumbnail-handler.ps1)" -ForegroundColor Gray
Write-Host ""
