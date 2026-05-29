@echo off
setlocal enabledelayedexpansion

rem ============================================================================
rem  Build (optional) and install a LOCAL TexThumbnailProvider.dll for testing.
rem
rem  Copies the locally built DLL to the same location the public installer uses
rem  (%LOCALAPPDATA%\RitoShark\TexThumbnailProvider) and registers it per-user
rem  (HKCU, no admin). Safe to re-run after every rebuild - it unregisters the
rem  old copy first.
rem
rem    install-local.bat            install the already-built Release DLL
rem    install-local.bat build      build Release^|x64 first, then install
rem    install-local.bat Debug      install the Debug DLL
rem    install-local.bat build Debug   build Debug, then install
rem ============================================================================

set "REPO_ROOT=%~dp0"
if "%REPO_ROOT:~-1%"=="\" set "REPO_ROOT=%REPO_ROOT:~0,-1%"

set "CONFIG=Release"
set "DO_BUILD="
for %%A in (%*) do (
    if /I "%%~A"=="build"   set "DO_BUILD=1"
    if /I "%%~A"=="Release" set "CONFIG=Release"
    if /I "%%~A"=="Debug"   set "CONFIG=Debug"
)

set "SOLUTION=%REPO_ROOT%\TexThumbnailProvider.sln"
set "INSTALL_DIR=%LOCALAPPDATA%\RitoShark\TexThumbnailProvider"
set "TARGET_DLL=%INSTALL_DIR%\TexThumbnailProvider.dll"
set "SOURCE_DLL=%REPO_ROOT%\x64\%CONFIG%\TexThumbnailProvider.dll"

echo.
echo TexThumbnailProvider - LOCAL install (%CONFIG%)
echo =====================================================

rem --- optional build --------------------------------------------------------
if defined DO_BUILD (
    call :find_msbuild
    if not defined MSBUILD (
        echo ERROR: MSBuild not found. Install VS 2022 build tools.
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

rem --- resolve DLL -----------------------------------------------------------
if not exist "%SOURCE_DLL%" (
    echo ERROR: DLL not found: %SOURCE_DLL%
    echo Build it first ^(run: install-local.bat build^).
    exit /b 1
)
echo     Source DLL: %SOURCE_DLL%

rem --- unregister previous copy ----------------------------------------------
if exist "%TARGET_DLL%" (
    echo ==^> Unregistering previous copy ...
    regsvr32 /s /u "%TARGET_DLL%"
)

rem --- copy + register -------------------------------------------------------
if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"

echo ==^> Copying to %TARGET_DLL% ...
copy /Y "%SOURCE_DLL%" "%TARGET_DLL%" >nul
if errorlevel 1 (
    echo ERROR: copy failed.
    exit /b 1
)
echo     Installed

echo ==^> Registering ^(HKCU^) ...
regsvr32 /s "%TARGET_DLL%"
if errorlevel 1 (
    echo ERROR: regsvr32 failed. Make sure it's the x64 DLL on an x64 system.
    exit /b 1
)
echo     Registered

echo.
echo Done. Open a folder of .tex files in Explorer to test.
echo Uninstall: regsvr32 /u "%TARGET_DLL%"
echo.
exit /b 0

rem ---------------------------------------------------------------------------
:find_msbuild
set "MSBUILD="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe"`) do (
        set "MSBUILD=%%i"
    )
)
if not defined MSBUILD (
    where msbuild >nul 2>nul && for /f "usebackq delims=" %%i in (`where msbuild`) do if not defined MSBUILD set "MSBUILD=%%i"
)
exit /b 0
