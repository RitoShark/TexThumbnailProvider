<#
.SYNOPSIS
    Installs the RitoShark .tex Explorer thumbnail handler.

.DESCRIPTION
    Downloads the latest TexThumbnailProvider.dll from the GitHub Releases of
    RitoShark/TexThumbnailProvider, copies it to a stable per-user location, and
    registers it as the Windows Explorer thumbnail provider for .tex files.

    Registration is per-user (HKCU only) so administrator rights are NOT required.
    After install, Explorer shows thumbnails for League of Legends .tex textures
    (BGRA8, BC1/DXT1, BC3/DXT5, BC5, BC7).

.PARAMETER Version
    Release tag to install (e.g. "v1.1.0"). Defaults to the latest release.

.PARAMETER Dll
    Install a local DLL instead of downloading (useful for testing a build).

.EXAMPLE
    # Run in PowerShell, then:
    iwr -useb https://raw.githubusercontent.com/RitoShark/TexThumbnailProvider/master/scripts/install-thumbnail-handler.ps1 | iex

.EXAMPLE
    # Pin a specific version:
    & ([scriptblock]::Create((iwr -useb https://raw.githubusercontent.com/RitoShark/TexThumbnailProvider/master/scripts/install-thumbnail-handler.ps1))) -Version v1.1.0
#>
[CmdletBinding()]
param(
    [string]$Version = 'latest',
    [string]$Dll
)

$ErrorActionPreference = 'Stop'

$Repo       = 'RitoShark/TexThumbnailProvider'
$AssetName  = 'TexThumbnailProvider.dll'
$InstallDir = Join-Path $env:LOCALAPPDATA 'RitoShark\TexThumbnailProvider'
$TargetDll  = Join-Path $InstallDir $AssetName

function Write-Step($msg)  { Write-Host "==> $msg" -ForegroundColor Cyan }
function Write-Ok($msg)    { Write-Host "    $msg" -ForegroundColor Green }
function Write-Warn2($msg) { Write-Host "    $msg" -ForegroundColor Yellow }

Write-Host ""
Write-Host "RitoShark .tex thumbnail handler installer" -ForegroundColor Magenta
Write-Host "===========================================" -ForegroundColor Magenta

# --- 1. Get the DLL (local override or download from Releases) -----------------
if (-not (Test-Path $InstallDir)) {
    New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
}

if ($Dll) {
    Write-Step "Using local DLL: $Dll"
    if (-not (Test-Path $Dll)) { throw "Local DLL not found: $Dll" }
    Copy-Item -Force $Dll $TargetDll
}
else {
    if ($Version -eq 'latest') {
        $apiUrl = "https://api.github.com/repos/$Repo/releases/latest"
    } else {
        $apiUrl = "https://api.github.com/repos/$Repo/releases/tags/$Version"
    }

    Write-Step "Looking up release ($Version) ..."
    $headers = @{ 'User-Agent' = 'TexThumbnailProvider-Installer'; 'Accept' = 'application/vnd.github+json' }
    try {
        $release = Invoke-RestMethod -Uri $apiUrl -Headers $headers
    } catch {
        throw "Could not query GitHub Releases for $Repo ($Version). $($_.Exception.Message)"
    }

    $asset = $release.assets | Where-Object { $_.name -eq $AssetName } | Select-Object -First 1
    if (-not $asset) {
        throw "Release '$($release.tag_name)' has no asset named '$AssetName'. Available: $(($release.assets.name) -join ', ')"
    }

    Write-Ok "Found $AssetName in release $($release.tag_name)"
    Write-Step "Downloading ..."
    Invoke-WebRequest -Uri $asset.browser_download_url -Headers $headers -OutFile $TargetDll
}

Write-Ok "Installed to $TargetDll"

# --- 2. Register (unregister any previous copy first, idempotent) --------------
# regsvr32 calls our DllRegisterServer, which writes only HKCU keys -> no admin.
Write-Step "Registering thumbnail handler ..."
Start-Process regsvr32 -ArgumentList "/s","/u","`"$TargetDll`"" -Wait -NoNewWindow -ErrorAction SilentlyContinue
$proc = Start-Process regsvr32 -ArgumentList "/s","`"$TargetDll`"" -Wait -PassThru -NoNewWindow
if ($proc.ExitCode -ne 0) {
    throw "regsvr32 failed (exit $($proc.ExitCode)). The DLL may be x86 on an x64 system, or blocked. Try unblocking it: Unblock-File '$TargetDll'"
}
Write-Ok "Registered (HKCU)"

# --- 3. Refresh Explorer's thumbnail cache so existing .tex files repaint ------
Write-Step "Refreshing thumbnail cache ..."
Start-Process regsvr32 -ArgumentList "/s","`"$TargetDll`"" -Wait -NoNewWindow -ErrorAction SilentlyContinue
# DllRegisterServer already calls SHChangeNotify(SHCNE_ASSOCCHANGED). Already-cached
# per-file thumbnails only refresh once their mtime changes or the cache is cleared.
Write-Warn2 "Already-cached thumbnails may need a cache clear (Disk Cleanup -> Thumbnails) to repaint."

Write-Host ""
Write-Host "Done. Open a folder of .tex files in Explorer to see thumbnails." -ForegroundColor Green
Write-Host "To uninstall: run uninstall-thumbnail-handler.ps1 (or: regsvr32 /u `"$TargetDll`")" -ForegroundColor Gray
Write-Host ""
