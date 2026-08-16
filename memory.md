# Project Memory

## 2026-07-22

- `cmake-4.4.0` in the project is the CMake source archive, not a Windows binary; use an extracted official Windows ZIP locally and prepend its `bin` directory to the process `PATH`.
- CMake's downloader could not reach NuGet in this environment; seed `NPPMR_WEBVIEW2_SDK_DIR` and `NPPMR_MD4C_SOURCE_DIR` in the existing build cache with locally downloaded, pinned dependencies.
- WebView2 compilation requires `windows.h`, `wrl.h`, then `WebView2.h`; `GetPrivateProfileIntW` returns `UINT`, so use unsigned bounds with `std::clamp`.
- MD4C requires angle-bracket destinations for Markdown image filenames containing spaces, e.g. `![x](<images/file name.png>)`.
- Verified x64 release: build, renderer test, install staging, and ZIP packaging all pass.
- Runtime fix: bind the child HWND during WM_NCCREATE before dispatching messages; load toolbar resources with transparency/color mapping. Rebuilt x64 release and CTest passed.
- Reader settings persist in the `[Reader]` INI section; the header toggle omits only `.reader-toolbar`, while the progress element remains. Width commands adjust `560–1400px` in `80px` steps.
- Presentation settings now use WebView2 `ExecuteScript` with navigation readiness, revision ordering, scroll-ratio preservation, and one guarded full-refresh fallback; source and remote-image policy changes still use full navigation.

## 2026-08-15

- Tab-switch scroll reset root cause: `html{scroll-behavior:smooth}` + `scrollTo({behavior:"auto"})` animates, so post-scroll `scrollY` reads reported 0 and poisoned `DocumentState::scrollRatio`; `ExecutePresentation` could cancel the in-flight restore. Fixed via `behavior:"instant"`, a `scroll-ready:<revision>:<ratio>` page→host handshake that gates `scroll:` reports, and dropping the synthetic scroll dispatch.
- Re-show fast path: `RetainedPage{bufferId, contentHash+len, renderSerial, presentationSerial}` per view survives `Hide()`; `RefreshView` re-shows the live page (no navigation) when the fingerprint still matches. Serials bump on save/rename/lang/remote-image policy (render) and font/theme/dark-mode (presentation). `NPPN_FILEBEFORECLOSE` no longer hides hosts (clone-safe; BUFFERACTIVATED/FILECLOSED cover it).
- This checkout was missing gitignored `cmake/FetchWebView2.cmake` — restored from `C:\Users\Freki\Downloads\NppMarkdownReader\cmake\`. Machine-wide build deps: WebView2 SDK + MD4C at `C:\tmp\nppmarkdownreader-deps\`, CMake at `C:\tmp\nppmarkdownreader-cmake-4.4.0\cmake-4.4.0-windows-x86_64\bin` (prepend to PATH). VS2022 Community is the generator. x64 Release build + renderer tests pass.
