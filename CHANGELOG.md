# Changelog

## Unreleased

### Added

- **Show Table of Contents** menu toggle: enables or disables the "On this page" pane entirely. On by default (INI: `ShowToc`).
- **Align Reader Content Left** menu toggle: pins the content card to the left edge instead of centering it, keeping the right side clear for the table of contents (INI: `AlignLeft`).
- **Fit Reader Content to Window** menu toggle: sizes the content card to the available pane width instead of a fixed width, reserving space for the table of contents when one is shown (INI: `FillWidth`).

### Changed

- The table of contents is now anchored to the content card's edge and shown only when measured space beside the card fits it, instead of relying on a fixed 1370px window breakpoint. This fixes the contents overlapping the content card when the text width was enlarged, and shows it in narrower windows when content is left-aligned.

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
