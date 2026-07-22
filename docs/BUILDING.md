# Building

## Prerequisites

Install on Windows:

1. Visual Studio 2022.
2. The **Desktop development with C++** workload.
3. A current Windows SDK.
4. CMake 3.24 or newer (the Visual Studio copy is sufficient when available).
5. Git only if building from a repository checkout rather than this archive.

The build fetches:

- MD4C release 0.5.3, verified with a pinned SHA-256 hash;
- Microsoft WebView2 SDK NuGet package 1.0.4078.44.

Set `NPPMR_MD4C_SOURCE_DIR` or `NPPMR_WEBVIEW2_SDK_DIR` to use pre-fetched dependencies in an offline build.

## Recommended build

Open **Developer PowerShell for VS 2022** in the project root:

```powershell
./scripts/build-release.ps1 -Architecture x64
```

Supported values are `x64`, `Win32`, and `ARM64`.

Useful options:

```powershell
./scripts/build-release.ps1 -Architecture x64 -Clean
./scripts/build-release.ps1 -Architecture Win32 -Configuration Debug -NoPackage
```

## Presets

```powershell
cmake --preset vs2022-x64
cmake --build --preset release-x64
ctest --preset release-x64
```

Equivalent presets are included for Win32 and ARM64. ARM64 tests compile in CI but are not executed on an x64 runner.

## Manual configuration

```powershell
cmake -S . -B out/build/x64 -G "Visual Studio 17 2022" -A x64 `
  -DNPPMR_BUILD_TESTS=ON
cmake --build out/build/x64 --config Release --parallel
ctest --test-dir out/build/x64 -C Release --output-on-failure
cmake --install out/build/x64 --config Release --prefix out/install/x64
```

## Outputs

The raw DLL is normally under:

```text
out/build/x64/bin/Release/NppMarkdownReader.dll
```

The staged install tree is:

```text
out/install/x64/NppMarkdownReader/NppMarkdownReader.dll
```

CMake/CPack can also create a ZIP package:

```powershell
cpack --config out/build/x64/CPackConfig.cmake -C Release
```

## Architecture matching

The plugin DLL architecture must match Notepad++:

- 64-bit Notepad++ → x64 plugin.
- 32-bit Notepad++ → Win32 plugin.
- ARM64 Notepad++ → ARM64 plugin.

A mismatched DLL will not load.

## WebView2 Runtime

The NuGet package is a build dependency, not the browser Runtime. The target computer must have Microsoft Edge WebView2 Runtime installed. The plugin reports a readable initialization error when the Runtime is unavailable.

## Offline builds

Example:

```powershell
cmake -S . -B out/build/x64 -G "Visual Studio 17 2022" -A x64 `
  -DNPPMR_MD4C_SOURCE_DIR=C:\deps\md4c-release-0.5.3 `
  -DNPPMR_WEBVIEW2_SDK_DIR=C:\deps\Microsoft.Web.WebView2.1.0.4078.44
```

The WebView2 directory must contain `build/native/include/WebView2.h` and the architecture-specific static loader library.

## Source validation

Run before compiling:

```powershell
python scripts/validate_source.py
node --check resources/reader.js
```
