# NppMarkdownReader

**Project status: substantial source prototype; no prebuilt DLL is included yet.**

NppMarkdownReader is a native Notepad++ plugin that places a polished Markdown reading surface directly over the active Scintilla editor pane. It keeps Notepad++'s tabs, menus, toolbar, split views, save behavior, undo history, and document ownership intact while replacing only the visible source pane during Reader Mode.

## What is implemented

- Same-pane Reader Mode using a borderless Win32 child window and WebView2.
- Independent state for Notepad++'s main and secondary editor views.
- GitHub-style Markdown through MD4C: headings, lists, task lists, tables, blockquotes, links, fenced code, and strikethrough.
- Responsive light and dark reader themes, plus automatic Notepad++ dark-mode following.
- Reading progress, generated table of contents, heading anchors, code-copy controls, language labels, and print styling.
- Live refresh after source edits and Replace All operations with a short debounce.
- Per-document/view scroll restoration and return-to-source caret restoration.
- Safe relative local images and local links rooted to the Markdown document's directory.
- Remote images blocked by default, with an explicit privacy warning before enabling them.
- External links opened through Windows rather than inside the embedded browser.
- Raw Markdown HTML disabled, restrictive Content Security Policy, blocked arbitrary schemes, and path traversal/junction containment checks.
- PDF export through WebView2.
- Toolbar icon and fourteen plugin commands.
- Settings stored in Notepad++'s plugin configuration directory.
- Unit tests for rendering and URL/path rewriting, source validation, and a Windows CI workflow.

## Commands

| Command | Behavior |
|---|---|
| **Toggle Reader Mode** | Switches the active document/view between source and rendered Markdown. Default shortcut: `Ctrl+Alt+M`. |
| **Export Reader to PDF…** | Exports the currently visible rendered document. |
| **Increase/Decrease/Reset Reader Text Size** | Updates visible readers in place when ready. |
| **Increase/Decrease/Reset Reader Text Width** | Changes the readable text width in 80px steps, from 560px to 1400px. |
| **Show Reader Header** | Shows or hides the filename bar while keeping the reading progress bar. |
| **Theme: Follow Notepad++ / Light / Dark** | Chooses the reader theme. |
| **Allow Remote Images** | Opt-in setting with a privacy warning. Off by default. |
| **About Markdown Reader** | Displays version and feature information. |

While the WebView has focus, `Esc` or `Ctrl+Alt+M` returns to source editing, `Ctrl+S` saves, and `Ctrl+Shift+S` invokes Notepad++ Save As. Those escape/save controls also remain available on the startup/error placeholder when WebView2 cannot initialize.

## Build requirements

- Windows 10 or Windows 11.
- Visual Studio 2022 with **Desktop development with C++**.
- A Windows SDK.
- CMake 3.24 or newer.
- A target architecture matching the Notepad++ installation: x64, Win32, or ARM64.
- Microsoft Edge WebView2 Runtime on the machine running the plugin.

The build downloads the pinned MD4C source archive and the pinned Microsoft WebView2 SDK NuGet package unless local dependency paths are supplied.

## Build x64

From a Visual Studio Developer PowerShell:

```powershell
./scripts/build-release.ps1 -Architecture x64
```

Or manually:

```powershell
cmake --preset vs2022-x64
cmake --build --preset release-x64
ctest --preset release-x64
cmake --install out/build/x64 --config Release --prefix out/install/x64
```

The staged plugin directory will be:

```text
out/install/x64/NppMarkdownReader/
  NppMarkdownReader.dll
  README.md
  LICENSE
  ...
```

See [Building](docs/BUILDING.md) and [Installing](docs/INSTALLING.md) for full instructions.

## Install layout

Notepad++ requires a plugin DLL to be placed in a same-named subdirectory:

```text
<Notepad++>\plugins\NppMarkdownReader\NppMarkdownReader.dll
```

Restart Notepad++, open a Markdown file, and select **Plugins → Markdown Reader → Toggle Reader Mode**.

## Security model

Markdown is treated as untrusted content. Raw HTML is disabled. Local resources are restricted to the saved Markdown file's directory, canonicalized before access, and limited to supported raster image formats. SVG files from the document folder are intentionally not rendered. Remote images are disabled by default. JavaScript from the document is never executed; only the plugin's embedded reader script is allowed.

See [Security](SECURITY.md) for assumptions, reporting guidance, and remaining limitations.

## Current limitations

- The project has not yet been compiled or interaction-tested under Windows/MSVC in the environment that produced this source package.
- Click-a-rendered-paragraph-to-edit source mapping is not implemented.
- Scroll synchronization is proportional, not exact source-block mapping.
- Reader Mode is manual rather than automatically enabled for every Markdown tab.
- Code blocks are styled and labeled but not token-level syntax highlighted.
- Remote fonts, audio, video, embedded frames, forms, and document-provided scripts are blocked.
- A saved file is required for relative local resources; untitled buffers have no safe base directory.

The full list is in [Known Limitations](docs/KNOWN-LIMITATIONS.md), and the verification checklist is in [Test Plan](docs/TEST-PLAN.md).

## Repository map

```text
src/                 Native Notepad++ and WebView2 integration
resources/           Embedded CSS, JavaScript, and blocked-image asset
assets/              Toolbar/icon resources
cmake/               WebView2 SDK acquisition
scripts/             Build, install, and validation helpers
tests/               Native renderer/security unit tests
docs/                Architecture, build, use, security, and test documentation
examples/             Markdown document and local-image fixture
.github/workflows/    Windows build/test/package matrix
```

## License

NppMarkdownReader is licensed under GNU GPL version 3. Third-party dependencies retain their own licenses; see [Third-Party Notices](THIRD_PARTY_NOTICES.md).
