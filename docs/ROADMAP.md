# Roadmap

## Release gate for 0.1.0

1. Compile with MSVC for x64, Win32, and ARM64.
2. Fix all compiler warnings and test failures.
3. Complete the manual Windows test plan.
4. Run a focused security review of resource interception, URL normalization, and asynchronous lifetime handling.
5. Test against current stable Notepad++ and WebView2 Runtime builds.
6. Produce signed checksums and a reproducible release ZIP.

## 0.2 candidates

- Source-position metadata and click rendered block to edit.
- Exact source/rendered scroll synchronization.
- Optional automatic Reader Mode for Markdown files.
- Reader search and find-result highlighting.
- Width, line-height, and typography controls in a settings dialog.
- Custom CSS file with safe reload.
- Token-level syntax highlighting using a bundled, trusted highlighter.
- Better custom context menu.

## Later candidates

- Mermaid and math using bundled, offline, pinned scripts.
- Front-matter presentation.
- Document outline integration.
- Print settings and forced print theme.
- Session persistence of enabled documents.
- Plugin Admin packaging and update metadata.
- Localization and accessibility refinements.
