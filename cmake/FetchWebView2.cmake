if(NPPMR_WEBVIEW2_SDK_DIR)
    set(_wv2_root "${NPPMR_WEBVIEW2_SDK_DIR}")
else()
    set(_wv2_root "${CMAKE_BINARY_DIR}/_deps/webview2-${NPPMR_WEBVIEW2_VERSION}")
    set(_wv2_package "${CMAKE_BINARY_DIR}/_deps/Microsoft.Web.WebView2.${NPPMR_WEBVIEW2_VERSION}.nupkg")
    if(NOT EXISTS "${_wv2_root}/build/native/include/WebView2.h")
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/_deps")
        message(STATUS "Downloading Microsoft.Web.WebView2 ${NPPMR_WEBVIEW2_VERSION}")
        file(DOWNLOAD
            "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${NPPMR_WEBVIEW2_VERSION}"
            "${_wv2_package}"
            TLS_VERIFY ON
            EXPECTED_HASH SHA256=dc4d1d9168df26b830398303e50210b6e1729f6ce5a7ac69d2c766852f489962
            STATUS _wv2_download_status
            SHOW_PROGRESS)
        list(GET _wv2_download_status 0 _wv2_status_code)
        if(NOT _wv2_status_code EQUAL 0)
            list(GET _wv2_download_status 1 _wv2_status_text)
            message(FATAL_ERROR "WebView2 SDK download failed: ${_wv2_status_text}")
        endif()
        file(MAKE_DIRECTORY "${_wv2_root}")
        file(ARCHIVE_EXTRACT INPUT "${_wv2_package}" DESTINATION "${_wv2_root}")
    endif()
endif()

if(CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
    set(_wv2_arch arm64)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_wv2_arch x64)
else()
    set(_wv2_arch x86)
endif()

set(NPPMR_WEBVIEW2_INCLUDE_DIR "${_wv2_root}/build/native/include")
set(NPPMR_WEBVIEW2_LOADER_LIBRARY "${_wv2_root}/build/native/${_wv2_arch}/WebView2LoaderStatic.lib")

if(NOT EXISTS "${NPPMR_WEBVIEW2_INCLUDE_DIR}/WebView2.h")
    message(FATAL_ERROR "WebView2.h was not found under ${NPPMR_WEBVIEW2_INCLUDE_DIR}")
endif()
if(NOT EXISTS "${NPPMR_WEBVIEW2_LOADER_LIBRARY}")
    message(FATAL_ERROR "WebView2LoaderStatic.lib was not found at ${NPPMR_WEBVIEW2_LOADER_LIBRARY}")
endif()
