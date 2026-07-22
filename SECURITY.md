# Security Policy

## Supported versions

This is a pre-release source prototype. Security fixes should target the current main branch until the first binary release exists.

## Content trust model

Every Markdown document is treated as potentially hostile.

The renderer therefore:

- disables raw Markdown HTML;
- allows only the plugin's embedded JavaScript and stylesheet;
- applies a restrictive Content Security Policy;
- blocks `javascript:`, `file:`, custom, and obfuscated URL schemes;
- opens approved `http`, `https`, `mailto`, and `tel` links outside the WebView;
- blocks remote images by default;
- limits local content to supported raster images;
- rejects path traversal, drive-qualified paths, control characters, NTFS alternate-data-stream syntax, and invalid Windows filename characters;
- repeatedly normalizes percent encoding before validation;
- canonicalizes both the document directory and candidate path so existing junction/symlink escapes are rejected;
- limits a local image to 64 MiB and a source document to 128 MiB;
- does not use unrestricted virtual-folder mapping or expose native host objects to document content.

## Remaining risks

- Filesystem validation and file opening are separate operations, leaving a narrow time-of-check/time-of-use race if an attacker can alter junctions or files concurrently.
- WebView2 and MD4C are third-party components and inherit their upstream security posture.
- Enabling remote images permits requests to third-party servers and can disclose network metadata.
- External links are handed to the Windows shell after scheme validation; the configured system handler controls subsequent behavior.
- A Windows compile/runtime review is still required before treating the plugin as production-ready.

## Reporting a vulnerability

Do not include private documents or secrets in a public issue. Report the minimal reproduction, affected version/commit, expected behavior, actual behavior, and relevant Windows/Notepad++ architecture. Until a repository owner establishes a private reporting channel, keep sensitive details out of public artifacts.
