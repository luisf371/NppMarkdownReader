# Project Memory

## 2026-07-22

- `cmake-4.4.0` in the project is the CMake source archive, not a Windows binary; use an extracted official Windows ZIP locally and prepend its `bin` directory to the process `PATH`.
- CMake's downloader could not reach NuGet in this environment; seed `NPPMR_WEBVIEW2_SDK_DIR` and `NPPMR_MD4C_SOURCE_DIR` in the existing build cache with locally downloaded, pinned dependencies.
- WebView2 compilation requires `windows.h`, `wrl.h`, then `WebView2.h`; `GetPrivateProfileIntW` returns `UINT`, so use unsigned bounds with `std::clamp`.
- MD4C requires angle-bracket destinations for Markdown image filenames containing spaces, e.g. `![x](<images/file name.png>)`.
- Verified x64 release: build, renderer test, install staging, and ZIP packaging all pass.- Runtime fix: bind the child HWND during WM_NCCREATE before dispatching messages; load toolbar resources with transparency/color mapping. Rebuilt x64 release and CTest passed.
