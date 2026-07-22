# Installing

## Manual installation

1. Close Notepad++.
2. Build the architecture matching your Notepad++ installation.
3. Create this directory beneath the Notepad++ installation:

   ```text
   plugins\NppMarkdownReader
   ```

4. Copy `NppMarkdownReader.dll` into that directory.
5. Start Notepad++.
6. Confirm **Plugins → Markdown Reader** appears.

Complete layout:

```text
<Notepad++>\
  notepad++.exe
  plugins\
    NppMarkdownReader\
      NppMarkdownReader.dll
```

Do not place the DLL directly in `plugins` without the same-named subfolder.

## Helper script

After building:

```powershell
./scripts/install-local.ps1 `
  -NotepadPlusPlusDirectory "C:\Program Files\Notepad++" `
  -Architecture x64
```

The script refuses to overwrite a loaded plugin while Notepad++ is running unless `-Force` is supplied. Administrative rights may be required for a system installation.

## Portable Notepad++

Point the script at the portable root, or copy the plugin folder manually. Settings still use the configuration directory reported by that Notepad++ instance.

## Uninstall

1. Close Notepad++.
2. Delete `<Notepad++>\plugins\NppMarkdownReader`.
3. Optionally remove the settings and WebView2 profile directory under Notepad++'s plugin configuration folder.

Removing the plugin never modifies Markdown documents.

## Troubleshooting

### Plugin menu is absent

- Confirm the DLL is in the same-named subfolder.
- Confirm DLL and Notepad++ architectures match.
- Check Notepad++'s plugin loading diagnostics.

### Reader shows a WebView2 error

Install or repair Microsoft Edge WebView2 Runtime, then restart Notepad++.

### Local image is unavailable

- Save the Markdown document first.
- Use a path relative to the Markdown file.
- Keep the image inside the Markdown document's directory tree.
- Use a supported raster extension: PNG, JPEG/JFIF, GIF, WebP, BMP, ICO, or AVIF.
- SVG from the document directory is intentionally blocked.

### Remote image is blocked

Enable **Plugins → Markdown Reader → Allow Remote Images** only after accepting the privacy warning.
