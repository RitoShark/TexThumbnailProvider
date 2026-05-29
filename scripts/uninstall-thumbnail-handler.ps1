<#
.SYNOPSIS
    Uninstalls the RitoShark .tex Explorer thumbnail handler.

.DESCRIPTION
    Unregisters the thumbnail handler (removing the HKCU registry entries created
    by DllRegisterServer) and deletes the installed DLL. No admin rights required.

.EXAMPLE
    iwr -useb https://raw.githubusercontent.com/RitoShark/TexThumbnailProvider/master/scripts/uninstall-thumbnail-handler.ps1 | iex
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$InstallDir = Join-Path $env:LOCALAPPDATA 'RitoShark\TexThumbnailProvider'
$TargetDll  = Join-Path $InstallDir 'TexThumbnailProvider.dll'

Write-Host ""
Write-Host "RitoShark .tex thumbnail handler uninstaller" -ForegroundColor Magenta
Write-Host "=============================================" -ForegroundColor Magenta

if (Test-Path $TargetDll) {
    Write-Host "==> Unregistering ..." -ForegroundColor Cyan
    Start-Process regsvr32 -ArgumentList "/s","/u","`"$TargetDll`"" -Wait -NoNewWindow -ErrorAction SilentlyContinue
    Write-Host "    Unregistered" -ForegroundColor Green

    Write-Host "==> Removing files ..." -ForegroundColor Cyan
    Remove-Item -Force $TargetDll -ErrorAction SilentlyContinue
    if (-not (Get-ChildItem -Force $InstallDir -ErrorAction SilentlyContinue)) {
        Remove-Item -Force -Recurse $InstallDir -ErrorAction SilentlyContinue
    }
    Write-Host "    Removed $TargetDll" -ForegroundColor Green
}
else {
    Write-Host "    Nothing installed at $TargetDll" -ForegroundColor Yellow
    # Best-effort cleanup in case the DLL is gone but keys linger.
    $clsid = '{243B3EEC-8FD0-44CD-95AD-BEAFDCE52CBF}'
    Remove-Item -Recurse -Force "HKCU:\Software\Classes\CLSID\$clsid" -ErrorAction SilentlyContinue
}

Write-Host ""
Write-Host "Done." -ForegroundColor Green
Write-Host ""
