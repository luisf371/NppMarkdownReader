# Changelog

## 0.1.0-source — 2026-07-22

Initial source prototype.

### Added

- Same-pane WebView2 Markdown reader over Notepad++ Scintilla views.
- Main/secondary view and per-buffer state handling.
- GitHub-style Markdown rendering with MD4C.
- Embedded responsive light/dark themes.
- Reading progress, table of contents, heading links, table wrappers, code copy, and language labels.
- Live refresh and proportional scroll restoration.
- Replace All refresh support through `NPPN_GLOBALMODIFIED` and toolbar re-registration after icon-set changes.
- Local images/links with canonical containment validation.
- Remote-image privacy control and warning.
- External navigation policy and restrictive Content Security Policy.
- PDF export.
- Toolbar icon, settings, unit tests, CI, documentation, and build/install scripts.
- Origin validation for web-to-native messages and keyboard escape/save handling even when WebView2 initialization fails.

### Known status

- Source and static validation only in the generation environment; Windows/MSVC build and runtime verification remain required.
