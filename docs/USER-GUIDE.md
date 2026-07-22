# User Guide

## Enter Reader Mode

Open any document containing Markdown and use one of:

- **Plugins → Markdown Reader → Toggle Reader Mode**;
- the Markdown Reader toolbar icon;
- `Ctrl+Alt+M`.

Reader Mode is not restricted solely by filename extension, so an unsaved Markdown buffer can be rendered. Relative images and local links require a saved file because an untitled buffer has no safe base directory.

## Leave Reader Mode

Press `Esc`, press `Ctrl+Alt+M`, click the toolbar button, or choose the toggle command again. The plugin restores the source caret and an approximate matching scroll position.

## Navigate

- Select and copy rendered text normally.
- Click heading `#` anchors to create stable in-document navigation.
- A table of contents appears for documents with at least three H2/H3 headings.
- External links open with the Windows default handler.
- Relative Markdown links open in Notepad++.
- Other safe relative files open with their Windows default handler.

## Save

While the reader has focus:

- `Ctrl+S` saves the active source document.
- `Ctrl+Shift+S` opens Notepad++ Save As.

## Images

Relative images are resolved against the saved Markdown file's directory:

```markdown
![Diagram](images/architecture.png)
```

Paths are constrained to that directory tree. Remote images are blocked by default. Enabling them is persistent and applies to visible/future reader documents until disabled again.

## Themes and sizing

Use the plugin menu to follow Notepad++ dark mode or force light/dark. Font size changes apply immediately to all visible reader panes. Defaults:

- font size: 17 px;
- line height: 1.72;
- content width: 860 px.

The INI file can also set `ContentWidth` and `LineHeight` within the enforced limits.

## Split view

Each Notepad++ view has an independent reader host. A cloned document can be in Reader Mode in one pane and source mode in the other.

## PDF export

1. Turn on Reader Mode for the active pane.
2. Select **Export Reader to PDF…**.
3. Choose a destination.

The export uses the reader's print stylesheet and current theme/content.

## Live refresh

When source changes while Reader Mode is active, the reader re-renders after a short debounce. This is most useful with a cloned source in the opposite split pane. Reader Mode itself is read-only; edit in Scintilla.
