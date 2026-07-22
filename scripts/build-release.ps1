[CmdletBinding()]
param(
    [ValidateSet("x64", "Win32", "ARM64")]
    [string]$Architecture = "x64",

    [ValidateSet("Release", "Debug", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",

    [switch]$Clean,
    [switch]$NoPackage
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$slug = $Architecture.ToLowerInvariant()
$build = Join-Path $root "out/build/$slug"
$install = Join-Path $root "out/install/$slug"

if ($Clean) {
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $build, $install
}

cmake -S $root -B $build -G "Visual Studio 17 2022" -A $Architecture -DNPPMR_BUILD_TESTS=ON
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

cmake --build $build --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }

if ($Architecture -ne "ARM64") {
    ctest --test-dir $build -C $Configuration --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Tests failed." }
}

Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $install
cmake --install $build --config $Configuration --prefix $install
if ($LASTEXITCODE -ne 0) { throw "Install staging failed." }

if (-not $NoPackage) {
    Push-Location $build
    try {
        cpack -C $Configuration
        if ($LASTEXITCODE -ne 0) { throw "CPack failed." }
    }
    finally {
        Pop-Location
    }
}

Write-Host "Staged plugin: $install/NppMarkdownReader" -ForegroundColor Green
