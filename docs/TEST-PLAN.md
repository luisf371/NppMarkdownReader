# Test Plan

A binary should not be published until the following tests pass on Windows.

## Automated build matrix

Build Release for:

- x64;
- Win32;
- ARM64.

Run native tests on x64 and Win32. Confirm warnings do not indicate ABI, narrowing, lifetime, or security defects.

## Plugin loading

For each executable architecture:

1. Place the DLL in the required plugin subfolder.
2. Start a clean portable Notepad++ instance.
3. Confirm no plugin load error.
4. Confirm all ten commands and toolbar icon appear.
5. Confirm unload/exit produces no crash.

## Core rendering

Use `examples/ReaderDemo.md` and additional fixtures:

- headings H1–H6;
- emphasis/strong/strikethrough;
- nested ordered/unordered lists;
- task lists;
- tables with horizontal overflow;
- blockquotes;
- fenced and indented code;
- Unicode and right-to-left text;
- very long paragraphs and long unbroken strings;
- empty document;
- 128 MiB boundary behavior.

Confirm raw HTML/scripts do not execute.

## Pane lifecycle

- Toggle in main view.
- Toggle in secondary view.
- Enable different documents in both views.
- Clone one buffer and use independent modes/scroll positions.
- Switch tabs rapidly.
- Close an enabled buffer.
- Rename, Save As, delete externally, reload, and change language.
- Move the splitter while Reader Mode is visible.
- Minimize/restore and resize Notepad++ repeatedly.

## Editing and refresh

With source cloned into the opposite view:

- insert/delete text;
- undo/redo;
- run Replace All across the active buffer and confirm the reader refreshes;
- paste a large block;
- save and Save As;
- verify debounce prevents a render per keystroke;
- verify Reader Mode exits to the expected caret and approximate scroll position.

## Keyboard/focus

- `Esc` exits.
- `Ctrl+Alt+M` exits and re-enters.
- `Ctrl+S` saves.
- `Ctrl+Shift+S` invokes Save As.
- with WebView2 unavailable, the startup/error placeholder still exits with `Esc` or `Ctrl+Alt+M` and routes Save/Save As;
- text selection and `Ctrl+C` work in reader.
- Tab/Shift+Tab focus visibility is usable.
- code Copy works with Clipboard API and fallback.

## Links and images

Allowed:

- relative PNG/JPEG/GIF/WebP/BMP/ICO/AVIF inside document tree;
- filenames containing spaces, plus signs, Unicode, and encoded `#`;
- relative Markdown link;
- approved external HTTP/HTTPS/mailto/tel link.

Blocked:

- `../` and multiply encoded traversal;
- absolute, UNC, and drive-qualified paths;
- NTFS alternate data streams;
- a junction/symlink inside the tree targeting outside it;
- document-local SVG;
- unsupported file types requested as images;
- local resources over 64 MiB;
- `javascript:`, `file:`, `data:text/html`, and custom schemes;
- remote image while opt-in is off.

## PDF

- export light and dark documents;
- inspect page breaks, tables, code blocks, images, headings, and hidden reader chrome;
- cancel dialog;
- reject overwrite then accept;
- attempt concurrent export;
- test unwritable destination and long path.

## Environment and display

- WebView2 Runtime missing and repaired.
- 100%, 125%, 150%, 200% DPI.
- multiple monitors with different DPI.
- Windows light/dark changes while visible.
- Notepad++ light/dark changes while following theme.
- switch among Notepad++ toolbar icon sets and verify the plugin icon remains registered.
- reduced-motion/high-contrast/accessibility review.

## Security review

- inspect final generated CSP;
- fuzz URL attributes and malformed percent encoding;
- fuzz Markdown around raw HTML and link destinations;
- verify no native host object is exposed;
- review asynchronous callbacks during shutdown;
- run static analysis and dependency vulnerability review;
- repeat junction tests under concurrent mutation to understand TOCTOU exposure.
