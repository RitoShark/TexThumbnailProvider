# TexThumbnailProvider

Windows Explorer **thumbnail handler** for League of Legends `.tex` textures..

Once installed, Explorer renders real thumbnails for `.tex` files — in the file
list, the preview pane, and "extra large icons" view — instead of a blank/generic
icon. It decodes the texture directly, so no app needs to be open.

![preview](https://img.shields.io/badge/Explorer-thumbnails-blue) ![platform](https://img.shields.io/badge/platform-Windows%20x64-lightgrey)

## Supported formats

| `.tex` code | Format            | Notes                                    |
|-------------|-------------------|------------------------------------------|
| `0x0A`      | DXT1 / BC1        |                                          |
| `0x0C`      | DXT5 / BC3        |                                          |
| `0x0D`      | BC7               | full 8-mode software decode              |
| `0x0E`      | BC5               | two-channel; blue reconstructed as normal-map Z |
| `0x14`      | BGRA8             | uncompressed                             |

Mipmapped textures are handled (the largest mip is decoded for the thumbnail).

## Install

Run in PowerShell (admin **not** required — registration is per-user):

```powershell
iwr -useb https://raw.githubusercontent.com/RitoShark/TexThumbnailProvider/master/scripts/install-thumbnail-handler.ps1 | iex
```

This downloads `TexThumbnailProvider.dll` from the [latest release](https://github.com/RitoShark/TexThumbnailProvider/releases/latest),
installs it to `%LOCALAPPDATA%\RitoShark\TexThumbnailProvider`, and registers it.

Pin a specific version:

```powershell
& ([scriptblock]::Create((iwr -useb https://raw.githubusercontent.com/RitoShark/TexThumbnailProvider/master/scripts/install-thumbnail-handler.ps1))) -Version v1.1.0
```

### Uninstall

```powershell
iwr -useb https://raw.githubusercontent.com/RitoShark/TexThumbnailProvider/master/scripts/uninstall-thumbnail-handler.ps1 | iex
```

> **Already-cached thumbnails not updating?** Windows caches thumbnails per file.
> After installing, existing `.tex` thumbnails repaint only once the file's
> modified-time changes or the cache is cleared (Disk Cleanup → *Thumbnails*).

## Priority over Photoshop

If you also have the [RitoTex Photoshop plugin](https://github.com/RitoShark/RitoTex-Photoshop)
installed, Photoshop registers itself as the `.tex` thumbnail handler and would
normally win. Windows resolves the handler by precedence:

```
ProgID(.tex)  >  SystemFileAssociations\.tex  >  .tex\ShellEx
```

Photoshop owns the `.tex` ProgID's handler (in `HKLM`), so a plain `.tex\ShellEx`
registration never gets picked. The installer therefore also writes this handler's
CLSID under the current `.tex` ProgID and under `SystemFileAssociations\.tex` in
`HKCU` — which overrides `HKLM` in the merged `HKEY_CLASSES_ROOT` view — so these
thumbnails win. Only the thumbnail subkey is touched; double-clicking a `.tex`
still opens whatever app you have associated.

## Build from source

Requires Visual Studio 2022 (v143 toolset) with the *Desktop development with C++*
workload.

```powershell
msbuild TexThumbnailProvider.sln /p:Configuration=Release /p:Platform=x64
```

Output: `x64\Release\TexThumbnailProvider.dll`. Build and install a local copy for
testing (same install location as the public installer, re-runnable after each
rebuild) with:

```bat
scripts\install-local.bat build
```

CI builds every push ([Build workflow](.github/workflows/build.yml)); tagging
`v*` builds and attaches the DLL to a GitHub Release ([Release workflow](.github/workflows/release.yml)).

## How it works

| File                        | Responsibility                                                            |
|-----------------------------|---------------------------------------------------------------------------|
| `Dll.cpp`                   | COM class factory + `DllRegisterServer`/`DllUnregisterServer` (incl. priority registration). |
| `TexThumbnailProvider.cpp`  | `IThumbnailProvider` / `IInitializeWithStream`: parses the `.tex` header, decodes, builds the `HBITMAP`. |
| `s3tc.cpp` / `s3tc.h`       | Self-contained block decoders: DXT1, DXT5, BC5, BC7. No external deps.     |

The decoders are deliberately standalone (no DirectXTex) to keep the shell
extension small and fast to load inside `DllHost.exe`. The BC7 tables and decode
flow match the BC7 specification.

## Debugging

Explorer hosts thumbnail providers in an isolated `DllHost.exe` process, and
thumbnails are cached per file, which makes debugging awkward. The easiest loop is
to test the handler in a standalone test app (e.g. the Microsoft
[UsingThumbnailProviders](https://github.com/microsoft/Windows-classic-samples/tree/master/Samples/Win7Samples/winui/shell/appplatform/UsingThumbnailProviders)
sample) before testing inside Explorer. To clear the cache between runs, change the
file's modified-time or run Disk Cleanup → *Thumbnails*.

## License

See [LICENSE.txt](LICENSE.txt). Originally derived from the Microsoft Windows
classic samples thumbnail-provider recipe.
