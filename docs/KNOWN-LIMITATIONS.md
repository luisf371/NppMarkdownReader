# Known Limitations

## Verification status

This package was created and statically inspected outside Windows. It has not yet passed an MSVC build or real Notepad++ runtime test.

## Reader behavior

- Reader Mode is manually toggled.
- The rendered surface is read-only.
- Click-to-edit source mapping is not implemented.
- Scroll restoration uses a ratio, so images, tables, and code blocks can prevent exact source-to-rendered alignment.
- Source changes and remote-image policy changes reload the page; presentation-only settings update in place and preserve transient selection/focus when the page is ready.
- Code blocks are styled and labeled but do not have token-level syntax highlighting.
- Table of contents includes H2/H3 only and appears after a minimum heading count.

## Markdown

- The default pinned MD4C 0.5.3 build provides GitHub-style tables, task lists, strikethrough, and permissive autolinks. Newer MD4C extension flags are enabled only when the supplied dependency defines them.
- Raw HTML is intentionally disabled rather than sanitized and retained.
- Mermaid, mathematical notation, custom admonition syntax, and document scripts are not implemented.

## Resources

- Untitled buffers cannot resolve relative files.
- Local files are constrained to the Markdown file's directory tree; deliberate references to parent directories are blocked.
- Local SVG is blocked because active SVG content has a broader security surface.
- Remote images require a global opt-in and are not cached by the plugin.
- Audio, video, frames, forms, remote fonts, and arbitrary network requests are blocked.
- A narrow filesystem time-of-check/time-of-use race remains between canonical validation and file opening.

## Notepad++ integration

- Only Reader-specific shortcuts plus Save/Save As are explicitly handled while WebView has focus; broader Notepad++ shortcut behavior needs runtime testing.
- The host follows Scintilla bounds with a timer rather than a direct layout callback.
- Toolbar icons are basic prototype assets.
- No Plugin Admin metadata, installer, updater, code signing, or automatic crash reporting is included.

## PDF

- PDF output depends on the installed WebView2 Runtime.
- Export uses the current rendered theme; a separate forced-print theme is not exposed.
- Headers, footers, page size, margins, and other advanced print settings are not configurable in the first version.
