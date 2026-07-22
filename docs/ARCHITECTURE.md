# Architecture

## Design goal

Reader Mode must occupy the same visible pane as the active Markdown source without replacing Notepad++'s document model or opening a docked preview.

## Process structure

```text
Notepad++ process
├─ Notepad++ main window
├─ Main Scintilla view
│  └─ NppMarkdownReader child host (visible only in Reader Mode)
│     └─ WebView2 controller
└─ Secondary Scintilla view
   └─ NppMarkdownReader child host (visible only in Reader Mode)
      └─ WebView2 controller
```

The host is an opaque child of the same parent as the target Scintilla control. It is repositioned to the Scintilla rectangle on a lightweight timer. Scintilla remains alive and continues to own source text, encoding, undo history, save state, file monitoring, and compatibility with other plugins.

## Major components

### `PluginMain.cpp`

Exports the Notepad++ plugin entry points, defines the command table, and delegates to the singleton controller.

### `ReaderController`

Owns application state and coordinates Notepad++:

- resolves the active view and buffer;
- maintains `DocumentKey { bufferId, view }` state;
- captures caret and initial source scroll position;
- extracts Scintilla text;
- renders and sends HTML to the view host;
- responds to tab, save, rename, delete, language, dark-mode, toolbar-set, normal modification, and Replace All notifications;
- opens local/external links;
- stores settings and coordinates PDF export.

### `MarkdownRenderer`

Calls MD4C's HTML renderer with GitHub-style extensions and raw HTML disabled. It then rewrites generated `href` and `src` values into one of three categories:

1. allowed external URL;
2. internal `https://document.local/...` URL for a safe relative path;
3. blocked placeholder.

It wraps the fragment in a complete HTML page with an embedded-origin CSS/JavaScript policy, theme variables, title, and initial scroll ratio.

### `WebViewEnvironment`

Creates one shared WebView2 environment and queues host callbacks while initialization is in progress.

### `WebViewHost`

Owns one Win32 child window and one WebView2 controller for a Notepad++ editor view. It:

- synchronizes bounds and visibility;
- serves the generated document and embedded assets through `WebResourceRequested`;
- serves validated local raster images;
- cancels navigation away from the internal page;
- forwards approved external/local navigation to the controller;
- receives scroll/save/exit messages only from the trusted reader origin;
- handles keyboard accelerators both in WebView2 and on the startup/error placeholder;
- exports PDF.

### `SettingsStore`

Loads and writes a small INI file under Notepad++'s plugin configuration directory. Settings are bounded and validated before use.

## Internal origins

- `https://reader.local/document` — generated HTML.
- `https://reader.local/reader.css` — embedded reader stylesheet.
- `https://reader.local/reader.js` — embedded trusted script.
- `https://reader.local/blocked-image.svg` — embedded blocked/unavailable image.
- `https://document.local/<encoded-relative-path>` — intercepted local resource identifier.

These are synthetic origins. The plugin responds through WebView2 resource interception; it does not run a network server.

## Document lifecycle

1. User toggles Reader Mode.
2. Controller identifies active view/buffer and records caret/scroll state.
3. Source is read from Scintilla and converted to UTF-8 when needed.
4. MD4C produces HTML; URLs are rewritten and the security envelope is added.
5. The view host is shown over Scintilla and navigates to the synthetic document URL.
6. Embedded assets and safe local images are supplied by the host.
7. Source modifications—including Replace All—schedule a debounced re-render.
8. Exiting hides the host, restores proportional Scintilla scroll, restores the caret, and returns focus.

## Split views

A document state is keyed by both buffer ID and view. The same buffer cloned into two views can therefore have Reader Mode and scroll state independently in each pane.

## Failure behavior

The underlying editor is never destroyed or detached. A rendering failure disables Reader Mode for that state, hides the overlay, and reports the error. A WebView2 initialization failure displays a diagnostic placeholder, and `Esc` or `Ctrl+Alt+M` always reveals the untouched source document.
