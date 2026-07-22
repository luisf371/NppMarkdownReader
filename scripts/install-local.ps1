[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)]
    [string]$NotepadPlusPlusDirectory,

    [ValidateSet("x64", "Win32", "ARM64")]
    [string]$Architecture = "x64",

    [ValidateSet("Release", "Debug", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",

    [switch]$Force
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$slug = $Architecture.ToLowerInvariant()
$dll = Join-Path $root "out/build/$slug/bin/$Configuration/NppMarkdownReader.dll"
$npp = Join-Path $NotepadPlusPlusDirectory "notepad++.exe"
$destination = Join-Path $NotepadPlusPlusDirectory "plugins/NppMarkdownReader"

if (-not (Test-Path -LiteralPath $npp -PathType Leaf)) {
    throw "Notepad++ was not found at: $npp"
}
if (-not (Test-Path -LiteralPath $dll -PathType Leaf)) {
    throw "Built plugin was not found at: $dll"
}

$running = Get-Process notepad++ -ErrorAction SilentlyContinue
if ($running -and -not $Force) {
    throw "Notepad++ is running. Close it first, or use -Force only if you accept copy failure/undefined loading behavior."
}

if ($PSCmdlet.ShouldProcess($destination, "Install NppMarkdownReader")) {
    New-Item -ItemType Directory -Force -Path $destination | Out-Null
    Copy-Item -Force -LiteralPath $dll -Destination (Join-Path $destination "NppMarkdownReader.dll")
    Write-Host "Installed to $destination" -ForegroundColor Green
}
