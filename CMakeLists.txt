cmake_minimum_required(VERSION 3.24)

project(NppMarkdownReader
    VERSION 0.1.0
    DESCRIPTION "Same-pane Markdown reader for Notepad++"
    LANGUAGES C CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "NppMarkdownReader is a Windows/Notepad++ plugin and must be built on Windows.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(NPPMR_BUILD_TESTS "Build unit tests" ON)
set(NPPMR_WEBVIEW2_VERSION "1.0.4078.44" CACHE STRING "Microsoft.Web.WebView2 NuGet version")
set(NPPMR_WEBVIEW2_SDK_DIR "" CACHE PATH "Pre-extracted Microsoft.Web.WebView2 package root")
set(NPPMR_MD4C_SOURCE_DIR "" CACHE PATH "Optional local md4c source directory")

include(FetchContent)
include(cmake/FetchWebView2.cmake)

if(NPPMR_MD4C_SOURCE_DIR)
    add_subdirectory("${NPPMR_MD4C_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/_deps/md4c-build" EXCLUDE_FROM_ALL)
else()
    set(BUILD_MD2HTML_EXECUTABLE OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        md4c
        URL https://github.com/mity/md4c/archive/refs/tags/release-0.5.3.tar.gz
        URL_HASH SHA256=353c346f376b87c954a13f3415ede2d51264cc61dc5abcd38ff1d2aa0d059b9e
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(md4c)
endif()

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/resources/reader.css" NPPMR_READER_CSS)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/resources/reader.js" NPPMR_READER_JS)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/resources/blocked-image.svg" NPPMR_BLOCKED_IMAGE_SVG)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/EmbeddedResources.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/EmbeddedResources.h"
    @ONLY)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/PluginResources.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/PluginResources.rc"
    @ONLY)

set(NPPMR_SOURCES
    src/DllMain.cpp
    src/PluginMain.cpp
    src/ReaderController.cpp
    src/WebViewEnvironment.cpp
    src/WebViewHost.cpp
    src/MarkdownRenderer.cpp
    src/Settings.cpp
    src/Utilities.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/generated/PluginResources.rc")

add_library(NppMarkdownReader SHARED ${NPPMR_SOURCES})
set_target_properties(NppMarkdownReader PROPERTIES
    PREFIX ""
    OUTPUT_NAME "NppMarkdownReader")

target_include_directories(NppMarkdownReader PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
    "${NPPMR_WEBVIEW2_INCLUDE_DIR}")

target_compile_definitions(NppMarkdownReader PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
    NPPMR_VERSION_STRING=L"${PROJECT_VERSION}")

if(MSVC)
    target_compile_options(NppMarkdownReader PRIVATE /W4 /permissive- /utf-8 /EHsc /Zc:__cplusplus)
endif()

target_link_libraries(NppMarkdownReader PRIVATE
    md4c-html
    "${NPPMR_WEBVIEW2_LOADER_LIBRARY}"
    comctl32
    comdlg32
    gdi32
    ole32
    oleaut32
    shell32
    shlwapi
    user32
    version)

set_target_properties(NppMarkdownReader PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")

if(NPPMR_BUILD_TESTS)
    enable_testing()
    add_executable(NppMarkdownReaderTests
        tests/RendererTests.cpp
        src/MarkdownRenderer.cpp
        src/Utilities.cpp)
    target_include_directories(NppMarkdownReaderTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_compile_definitions(NppMarkdownReaderTests PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(NppMarkdownReaderTests PRIVATE /W4 /permissive- /utf-8 /EHsc)
    endif()
    target_link_libraries(NppMarkdownReaderTests PRIVATE md4c-html shlwapi user32)
    add_test(NAME renderer-tests COMMAND NppMarkdownReaderTests)
endif()

install(TARGETS NppMarkdownReader
    RUNTIME DESTINATION "NppMarkdownReader")
install(FILES
    README.md
    LICENSE
    CHANGELOG.md
    SECURITY.md
    STATUS.md
    THIRD_PARTY_NOTICES.md
    DESTINATION "NppMarkdownReader")
install(DIRECTORY docs DESTINATION "NppMarkdownReader")
install(DIRECTORY examples DESTINATION "NppMarkdownReader")

set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME "NppMarkdownReader")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
include(CPack)
