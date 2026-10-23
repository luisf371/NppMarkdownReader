#!/usr/bin/env python3
"""Fast, cross-platform structural validation for the source package."""

from __future__ import annotations

import json
import re
import struct
import sys
from pathlib import Path

try:
    import yaml
except ImportError:  # Optional outside the packaged validation environment.
    yaml = None

ROOT = Path(__file__).resolve().parents[1]
ERRORS: list[str] = []
WARNINGS: list[str] = []


def require(condition: bool, message: str) -> None:
    if not condition:
        ERRORS.append(message)


def text(relative: str) -> str:
    path = ROOT / relative
    require(path.is_file(), f"Missing file: {relative}")
    return path.read_text(encoding="utf-8") if path.is_file() else ""


required = [
    "CMakeLists.txt", "CMakePresets.json", "LICENSE", "README.md", "STATUS.md",
    "CHANGELOG.md", "SECURITY.md", "THIRD_PARTY_NOTICES.md",
    "src/PluginMain.cpp", "src/ReaderController.cpp", "src/WebViewHost.cpp",
    "src/MarkdownRenderer.cpp", "src/PluginResources.rc.in",
    "resources/reader.css", "resources/reader.js",
    "tests/RendererTests.cpp", "docs/ARCHITECTURE.md", "docs/TEST-PLAN.md",
    ".github/workflows/build.yml", "examples/ReaderDemo.md",
]
for item in required:
    require((ROOT / item).is_file(), f"Missing required project file: {item}")

cmake = text("CMakeLists.txt")
source_entries = re.findall(r"^\s+(src/[A-Za-z0-9_./-]+\.(?:cpp|rc))\s*$", cmake, re.MULTILINE)
for entry in source_entries:
    require((ROOT / entry).is_file(), f"CMake references missing source: {entry}")
require("URL_HASH SHA256=" in cmake, "MD4C download is not hash pinned")
require("WebView2LoaderStatic.lib" in text("cmake/FetchWebView2.cmake"), "Static WebView2 loader is not selected")
require("generated/PluginResources.rc" in cmake, "Configured resource script is not used")

resource_script = text("src/PluginResources.rc.in")
for asset in ("reader_light.ico", "reader_dark.ico", "reader_toolbar.bmp"):
    require(f"@CMAKE_CURRENT_SOURCE_DIR@/assets/{asset}" in resource_script,
            f"Resource script does not use a stable absolute asset path: {asset}")

plugin = text("src/PluginMain.cpp")
for export in ("setInfo", "getName", "getFuncsArray", "beNotified", "messageProc", "isUnicode"):
    require(re.search(rf'__declspec\(dllexport\).*\b{export}\s*\(', plugin) is not None,
            f"Missing plugin export: {export}")
require(plugin.count("SetCommand(CommandIndex::") == 17, "Expected exactly seventeen command registrations")

api = text("src/PluginApi.h")
require("int modificationType" in api, "SCNotification does not expose modificationType")
require("NPPN_DARKMODECHANGED" in api, "Dark-mode notification constant is missing")
require("NPPN_GLOBALMODIFIED" in api, "Replace All notification constant is missing")
require("NPPN_TOOLBARICONSETCHANGED" in api, "Toolbar icon-set notification constant is missing")

renderer = text("src/MarkdownRenderer.cpp")
for marker in ("MD_FLAG_NOHTML", "Content-Security-Policy", "https://document.local/", "#blocked-link"):
    require(marker in renderer, f"Renderer security marker missing: {marker}")
require("MD_FLAG_HIGHLIGHT |" not in renderer, "Optional MD4C flags are used without feature tests")

utilities = text("src/Utilities.cpp")
for marker in ("PercentDecode", "weakly_canonical", "segment == \"..\"", "decoded[1] == ':'"):
    require(marker in utilities, f"Path security control missing: {marker}")

host = text("src/WebViewHost.cpp")
for marker in (
    "COREWEBVIEW2_WEB_RESOURCE_CONTEXT_IMAGE", "maximumImageBytes", "PrintToPdf",
    "put_Cancel(TRUE)", "get_Source(&rawSource)", "DispatchShortcut", "WM_KEYDOWN",
    "ERROR_FILE_TOO_LARGE", "ExecuteScript", "add_NavigationCompleted", "pageReady_",
):
    require(marker in host, f"WebView host control missing: {marker}")
require("SetVirtualHostNameToFolderMapping" not in host, "Unrestricted virtual-host folder mapping is present")

controller = text("src/ReaderController.cpp")
for marker in ("case NPPN_GLOBALMODIFIED", "case NPPN_TOOLBARICONSETCHANGED", "ScheduleRefreshForBuffer"):
    require(marker in controller, f"Notepad++ lifecycle handling missing: {marker}")

resources = {
    "NPPMR_CSS": text("resources/reader.css"),
    "NPPMR_JS": text("resources/reader.js"),
    "NPPMR_SVG": text("resources/blocked-image.svg"),
}
for delimiter, body in resources.items():
    require(f"){delimiter}\"" not in body, f"Embedded raw-string delimiter appears in resource: {delimiter}")

with (ROOT / "CMakePresets.json").open(encoding="utf-8") as handle:
    presets = json.load(handle)
require(presets.get("version", 0) >= 6, "CMake preset schema version is too old")
architectures = {p.get("architecture") for p in presets.get("configurePresets", [])}
require({"x64", "Win32", "ARM64"}.issubset(architectures), "CMake presets do not cover x64, Win32, and ARM64")

workflow_path = ROOT / ".github/workflows/build.yml"
if yaml is not None:
    with workflow_path.open(encoding="utf-8") as handle:
        workflow = yaml.safe_load(handle)
    require(isinstance(workflow, dict) and "jobs" in workflow, "Workflow YAML has no jobs")
else:
    WARNINGS.append("PyYAML unavailable; workflow YAML was not parsed")

for icon in (ROOT / "assets/reader_light.ico", ROOT / "assets/reader_dark.ico"):
    data = icon.read_bytes() if icon.is_file() else b""
    require(len(data) >= 6 and data[:4] == b"\x00\x00\x01\x00", f"Invalid ICO header: {icon.name}")
    if len(data) >= 6:
        count = struct.unpack_from("<H", data, 4)[0]
        require(count >= 1, f"ICO has no images: {icon.name}")

bmp = ROOT / "assets/reader_toolbar.bmp"
require(bmp.is_file() and bmp.read_bytes()[:2] == b"BM", "Toolbar BMP is invalid")

# Detect accidental placeholders and generated build output in the source archive.
for path in ROOT.rglob("*"):
    if path.is_file() and path.suffix.lower() in {".cpp", ".h", ".md", ".py", ".ps1", ".js", ".css"}:
        body = path.read_text(encoding="utf-8", errors="replace")
        placeholder = "TODO" + "_IMPLEMENT_ME"
        require(placeholder not in body, f"Implementation placeholder remains: {path.relative_to(ROOT)}")
require(not (ROOT / "out").exists(), "Build output directory should not be included in the clean source tree")

line_count = 0
for path in ROOT.rglob("*"):
    if path.is_file() and path.suffix.lower() in {".cpp", ".h", ".rc", ".js", ".css", ".py", ".ps1"}:
        line_count += path.read_text(encoding="utf-8", errors="replace").count("\n") + 1

if WARNINGS:
    for warning in WARNINGS:
        print(f"WARNING: {warning}")
if ERRORS:
    for error in ERRORS:
        print(f"ERROR: {error}")
    print(f"Validation failed with {len(ERRORS)} error(s).")
    sys.exit(1)

print(f"Source validation passed: {len(required)} required files, {len(source_entries)} CMake sources, {line_count:,} implementation lines checked.")
