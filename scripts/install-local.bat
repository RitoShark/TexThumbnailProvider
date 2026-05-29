@echo off
setlocal enabledelayedexpansion

rem ============================================================================
rem  Build (optional) and install a LOCAL TexThumbnailProvider.dll for testing.
rem
rem  For developing/testing on your own machine. Copies a locally built DLL to
rem  the same location the public installer uses
rem  (%LOCALAPPDATA%\RitoShark\TexThumbnailProvider) and registers it per-user
rem  (HKCU, no admin). Re-run after each rebuild - it unregisters the old copy
rem  first, so it's safe to run repeatedly.
rem
rem  Usage:
rem    install-local.bat                 install the already-built Release DLL
rem    install-local.bat build           build Release^|x64 first, then install
rem    install-local.bat Debug           install the Debug DLL
rem    install-local.bat build Debug     build Debug, then install
rem    install-local.bat <path-to.dll>   install an explicit DLL (overrides config)
rem ============================================================================

rem Repo root = parent of this script's folder (script lives in scripts\).
set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..") do set "REPO_ROOT=%%~fI"

set "CONFIG=Release"
set "DO_BUILD="
set "DLL="
for %%A in (%*) do (
    if /I "%%~A"=="build"   ( set "DO_BUILD=1"
    ) else if /I "%%~A"=="Release" ( set "CONFIG=Release"
    ) else if /I "%%~A"=="Debug"   ( set "CONFIG=Debug"
    ) else (
        set "ARG=%%~A"
        if /I "!ARG:~-4!"==".dll" set "DLL=%%~fA"
    )
)

set "SOLUTION=%REPO_ROOT%\TexThumbnailProvider.sln"
set "INSTALL_DIR=%LOCALAPPDATA%\RitoShark\TexThumbnailProvider"
set "TARGET_DLL=%INSTALL_DIR%\TexThumbnailProvider.dll"

echo.
echo TexThumbnailProvider - LOCAL install (%CONFIG%)
echo =====================================================

rem --- optional build --------------------------------------------------------
if defined DO_BUILD (
    call :find_msbuild
    if not defined MSBUILD (
        echo ERROR: MSBuild not found. Open a "Developer Command Prompt for VS" or install VS 2022 build tools.
        exit /b 1
    )
    echo ==^> Building %CONFIG%^|x64 ...
    "!MSBUILD!" "%SOLUTION%" /p:Configuration=%CONFIG% /p:Platform=x64 /m /v:minimal /nologo
    if errorlevel 1 (
        echo ERROR: Build failed.
        exit /b 1
    )
    echo     Build succeeded
)

rem --- resolve the DLL to install --------------------------------------------
if not defined DLL set "DLL=%REPO_ROOT%\x64\%CONFIG%\TexThumbnailProvider.dll"
if not exist "%DLL%" (
    echo ERROR: DLL not found: %DLL%
    echo Build it first ^(run: install-local.bat build^) or pass a path to a .dll.
    exit /b 1
)
echo     Source DLL: %DLL%

rem --- unregister any previous copy ------------------------------------------
if exist "%TARGET_DLL%" (
    echo ==^> Unregistering previous copy ...
    regsvr32 /s /u "%TARGET_DLL%"
)

if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"

rem --- copy + unblock (strip mark-of-the-web so the shell host will load it) --
echo ==^> Copying to %TARGET_DLL% ...
copy /Y "%DLL%" "%TARGET_DLL%" >nul
if errorlevel 1 (
    echo ERROR: copy failed.
    exit /b 1
)
del "%TARGET_DLL%:Zone.Identifier" >nul 2>nul
echo     Installed

rem --- register (HKCU) -------------------------------------------------------
echo ==^> Registering ^(HKCU^) ...
regsvr32 /s "%TARGET_DLL%"
if errorlevel 1 (
    echo ERROR: regsvr32 failed. Make sure it's the x64 DLL on an x64 system.
    exit /b 1
)
echo     Registered

echo.
echo Done. Open a folder of .tex files in Explorer to test.
echo Tip: thumbnails are cached per file. To force a repaint of a test file,
echo      change its modified time, e.g.:  copy /b some.tex +,,
echo Uninstall: regsvr32 /u "%TARGET_DLL%"   (or scripts\uninstall-thumbnail-handler.ps1)
echo.
exit /b 0

rem ---------------------------------------------------------------------------
:find_msbuild
set "MSBUILD="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe"`) do (
        if not defined MSBUILD set "MSBUILD=%%i"
    )
)
if not defined MSBUILD (
    for %%P in (
        "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
        "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
    ) do if not defined MSBUILD if exist %%P set "MSBUILD=%%~P"
)
if not defined MSBUILD (
    where msbuild >nul 2>nul && for /f "usebackq delims=" %%i in (`where msbuild`) do if not defined MSBUILD set "MSBUILD=%%i"
)
exit /b 0
