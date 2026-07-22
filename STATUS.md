# Project Status

## Overall

NppMarkdownReader is a **substantial, implementation-complete prototype source tree**, not a tested binary release.

The native plugin lifecycle, same-pane overlay, rendering pipeline, settings, security controls, split-view state, live refresh, keyboard integration, PDF export, assets, tests, documentation, and Windows CI definition are present.

## Completed in this package

- Native plugin exports and ten Notepad++ commands.
- ABI-compatible Notepad++/Scintilla declarations used by the implementation.
- Main-view and secondary-view WebView2 child hosts.
- Per-buffer/per-view state and tab lifecycle handling, including Replace All refresh and toolbar icon-set changes.
- MD4C Markdown-to-HTML rendering.
- Embedded reader CSS and JavaScript.
- Local-resource interception and containment checks.
- Remote-image opt-in privacy protection.
- PDF export.
- CMake projects and presets for x64, Win32, and ARM64.
- Native renderer/security tests.
- Static source validator.
- Windows GitHub Actions build matrix.
- Build, install, architecture, usage, limitations, roadmap, and manual test documentation.

## Validation performed in the source-generation environment

- JavaScript syntax checked with Node.js.
- CMake preset JSON parsed.
- Workflow YAML parsed.
- Required files, exports, resources, commands, security controls, and CMake source references checked by `scripts/validate_source.py`.
- Archive contents, source-manifest hashes, and the downloadable checksum were verified from a fresh extraction.

## Not performed here

- MSVC compilation and linkage.
- Loading the DLL into a real Notepad++ process.
- WebView2 runtime interaction testing.
- x86/ARM64 execution testing.
- PDF output inspection.
- High-DPI, multi-monitor, and accessibility testing on Windows.
- Plugin Admin submission or code signing.

Those are release gates, not claims of completed verification. Follow [docs/TEST-PLAN.md](docs/TEST-PLAN.md) before distributing a binary.
