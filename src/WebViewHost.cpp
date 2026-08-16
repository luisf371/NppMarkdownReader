#include "WebViewHost.h"

#include "EmbeddedResources.h"
#include "Utilities.h"
#include "WebViewEnvironment.h"

#include <shlwapi.h>
#include <wrl.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <iomanip>
#include <fstream>
#include <limits>
#include <locale>
#include <memory>
#include <sstream>
#include <string_view>

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace nppmr {
namespace {

std::wstring TakeCoTaskString(wchar_t* value) {
    std::wstring result = value ? value : L"";
    CoTaskMemFree(value);
    return result;
}

std::wstring_view StripQueryAndFragment(const std::wstring_view uri) {
    const std::size_t cut = uri.find_first_of(L"?#");
    return uri.substr(0, cut);
}

bool IsExternalScheme(const std::wstring_view uri) {
    std::wstring lower(uri);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](const wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return lower.starts_with(L"https://") || lower.starts_with(L"http://") ||
        lower.starts_with(L"mailto:") || lower.starts_with(L"tel:");
}

const wchar_t* ImageContentType(std::filesystem::path path) {
    std::wstring extension = path.extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](const wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    if (extension == L".png") return L"image/png";
    if (extension == L".jpg" || extension == L".jpeg" || extension == L".jfif") return L"image/jpeg";
    if (extension == L".gif") return L"image/gif";
    if (extension == L".webp") return L"image/webp";
    if (extension == L".bmp") return L"image/bmp";
    if (extension == L".ico") return L"image/x-icon";
    if (extension == L".avif") return L"image/avif";
    return nullptr;
}

} // namespace

WebViewHost::WebViewHost(const HINSTANCE instance, const HWND nppWindow, const HWND targetScintilla,
    const int viewIndex, std::filesystem::path userDataFolder, WebViewCallbacks callbacks)
    : instance_(instance), nppWindow_(nppWindow), targetScintilla_(targetScintilla),
      parentWindow_(GetParent(targetScintilla)), viewIndex_(viewIndex),
      userDataFolder_(std::move(userDataFolder)), callbacks_(std::move(callbacks)),
      lifetime_(std::make_shared<LifetimeState>()) {
    lifetime_->owner = this;
    if (!instance_ || !nppWindow_ || !targetScintilla_ || !parentWindow_ || !EnsureWindowClass(instance_)) return;
    hostWindow_ = CreateWindowExW(0, kWindowClassName, L"", WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        0, 0, 0, 0, parentWindow_, nullptr, instance_, this);
}

WebViewHost::~WebViewHost() {
    Shutdown();
}

bool WebViewHost::EnsureWindowClass(const HINSTANCE instance) {
    static ATOM atom = 0;
    if (atom) return true;
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = instance;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.lpszClassName = kWindowClassName;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    atom = RegisterClassExW(&windowClass);
    return atom != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

LRESULT CALLBACK WebViewHost::WindowProc(const HWND window, const UINT message,
    const WPARAM wParam, const LPARAM lParam) {
    WebViewHost* self = reinterpret_cast<WebViewHost*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        self = static_cast<WebViewHost*>(create->lpCreateParams);
        self->hostWindow_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        return TRUE;
    }
    if (self) return self->HandleMessage(message, wParam, lParam);
    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT WebViewHost::HandleMessage(const UINT message, const WPARAM wParam, const LPARAM lParam) {
    switch (message) {
    case WM_SIZE:
        if (controller_) {
            RECT bounds{};
            GetClientRect(hostWindow_, &bounds);
            controller_->put_Bounds(bounds);
        }
        return 0;
    case WM_SETFOCUS:
        if (callbacks_.onFocused) callbacks_.onFocused();
        if (controller_) controller_->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        // The host itself keeps focus while WebView2 is still starting or has
        // failed to initialize. Reader shortcuts must remain usable in that
        // state so the overlay can never trap the user.
        if (DispatchShortcut(static_cast<UINT>(wParam))) return 0;
        break;
    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS;
    case WM_TIMER:
        if (wParam == kSyncTimer) {
            SyncToTarget();
            return 0;
        }
        if (wParam == kRefreshTimer) {
            KillTimer(hostWindow_, kRefreshTimer);
            if (callbacks_.onRefreshRequested) callbacks_.onRefreshRequested();
            return 0;
        }
        break;
    case WM_PAINT:
        PaintPlaceholder();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_NCDESTROY:
        SetWindowLongPtrW(hostWindow_, GWLP_USERDATA, 0);
        break;
    default:
        break;
    }
    return DefWindowProcW(hostWindow_, message, wParam, lParam);
}

void WebViewHost::ShowDocument(std::string html, std::filesystem::path documentDirectory,
    ReaderPresentation presentation) {
    if (!hostWindow_) return;
    const bool shouldTakeFocus = !visible_;
    currentHtml_ = std::move(html);
    documentDirectory_ = std::move(documentDirectory);
    pendingPresentation_ = presentation;
    ++revision_;
    ++presentationRevision_;
    pageReady_ = false;
    visible_ = true;
    statusText_ = L"Starting Markdown Reader…";
    ShowWindow(hostWindow_, SW_SHOWNA);
    SetTimer(hostWindow_, kSyncTimer, 125, nullptr);
    SyncToTarget();
    EnsureInitialized();
    if (webView_) NavigateCurrentDocument();
    if (shouldTakeFocus) Focus();
}

void WebViewHost::ApplyPresentation(const ReaderPresentation& presentation) {
    if (!hostWindow_) return;
    pendingPresentation_ = presentation;
    ++presentationRevision_;
    if (pageReady_) ApplyPendingPresentation();
}

void WebViewHost::ReShow() {
    // Re-attach the still-loaded page without navigating again: the browser
    // keeps the exact DOM and scroll offset, so nothing has to be restored.
    if (!hostWindow_ || !webView_) return;
    visible_ = true;
    ShowWindow(hostWindow_, SW_SHOWNA);
    SetTimer(hostWindow_, kSyncTimer, 125, nullptr);
    SyncToTarget();
    if (controller_) controller_->put_IsVisible(TRUE);
    Focus();
}

void WebViewHost::Hide() {
    visible_ = false;
    if (hostWindow_) {
        KillTimer(hostWindow_, kSyncTimer);
        KillTimer(hostWindow_, kRefreshTimer);
        ShowWindow(hostWindow_, SW_HIDE);
    }
    if (controller_) controller_->put_IsVisible(FALSE);
}

void WebViewHost::Focus() {
    if (!visible_ || !hostWindow_) return;
    SetFocus(hostWindow_);
    if (controller_) controller_->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
}

void WebViewHost::ScheduleRefresh(const unsigned delayMilliseconds) {
    if (hostWindow_ && visible_) SetTimer(hostWindow_, kRefreshTimer, delayMilliseconds, nullptr);
}

void WebViewHost::EnsureInitialized() {
    if (initializationStarted_ || webView_) return;
    initializationStarted_ = true;
    const std::weak_ptr<LifetimeState> lifetime = lifetime_;
    WebViewEnvironment::Instance().Initialize(userDataFolder_,
        [lifetime](const HRESULT result, ComPtr<ICoreWebView2Environment> environment) {
            const auto state = lifetime.lock();
            if (!state || !state->owner) return;
            WebViewHost* self = state->owner;
            if (FAILED(result) || !environment) {
                self->SetFailure(result, L"WebView2 Runtime could not be initialized");
                return;
            }
            self->environment_ = std::move(environment);
            const std::weak_ptr<LifetimeState> controllerLifetime = lifetime;
            const HRESULT createResult = self->environment_->CreateCoreWebView2Controller(self->hostWindow_,
                Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    [controllerLifetime](const HRESULT controllerResult,
                        ICoreWebView2Controller* controller) -> HRESULT {
                        const auto controllerState = controllerLifetime.lock();
                        if (controllerState && controllerState->owner) {
                            controllerState->owner->CompleteControllerCreation(controllerResult, controller);
                        }
                        return S_OK;
                    }).Get());
            if (FAILED(createResult)) {
                self->SetFailure(createResult, L"The Markdown browser control could not be created");
            }
        });
}

void WebViewHost::CompleteControllerCreation(const HRESULT result, ICoreWebView2Controller* controller) {
    if (FAILED(result) || !controller) {
        SetFailure(result, L"The Markdown browser control could not be created");
        return;
    }
    controller_ = controller;
    const HRESULT coreResult = controller_->get_CoreWebView2(&webView_);
    if (FAILED(coreResult) || !webView_) {
        SetFailure(coreResult, L"The Markdown browser control is unavailable");
        return;
    }
    ConfigureWebView();
    SyncToTarget();
    if (visible_ && !currentHtml_.empty()) NavigateCurrentDocument();
}

void WebViewHost::ConfigureWebView() {
    ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(webView_->get_Settings(&settings)) && settings) {
        settings->put_AreDevToolsEnabled(FALSE);
        settings->put_IsStatusBarEnabled(FALSE);
        settings->put_AreDefaultScriptDialogsEnabled(FALSE);
        settings->put_AreDefaultContextMenusEnabled(TRUE);
        settings->put_IsZoomControlEnabled(FALSE);
    }

    const std::weak_ptr<LifetimeState> lifetime = lifetime_;
    webView_->AddWebResourceRequestedFilter(L"https://reader.local/*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    webView_->AddWebResourceRequestedFilter(L"https://document.local/*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    webView_->add_WebResourceRequested(
        Callback<ICoreWebView2WebResourceRequestedEventHandler>(
            [lifetime](ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
                const auto state = lifetime.lock();
                return state && state->owner ? state->owner->HandleWebResourceRequested(args) : S_OK;
            }).Get(), &resourceToken_);

    webView_->add_NavigationStarting(
        Callback<ICoreWebView2NavigationStartingEventHandler>(
            [lifetime](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                const auto state = lifetime.lock();
                return state && state->owner ? state->owner->HandleNavigationStarting(args) : S_OK;
            }).Get(), &navigationToken_);

    webView_->add_NavigationCompleted(
        Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [lifetime](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                const auto state = lifetime.lock();
                if (!state || !state->owner) return S_OK;
                return state->owner->HandleNavigationCompleted(args);
            }).Get(), &navigationCompletedToken_);

    webView_->add_NewWindowRequested(
        Callback<ICoreWebView2NewWindowRequestedEventHandler>(
            [lifetime](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                const auto state = lifetime.lock();
                return state && state->owner ? state->owner->HandleNewWindowRequested(args) : S_OK;
            }).Get(), &newWindowToken_);

    webView_->add_WebMessageReceived(
        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [lifetime](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                const auto state = lifetime.lock();
                return state && state->owner ? state->owner->HandleWebMessage(args) : S_OK;
            }).Get(), &messageToken_);

    controller_->add_AcceleratorKeyPressed(
        Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
            [lifetime](ICoreWebView2Controller*, ICoreWebView2AcceleratorKeyPressedEventArgs* args) -> HRESULT {
                const auto state = lifetime.lock();
                return state && state->owner ? state->owner->HandleAccelerator(args) : S_OK;
            }).Get(), &acceleratorToken_);
}

HRESULT WebViewHost::HandleNavigationCompleted(
    ICoreWebView2NavigationCompletedEventArgs* args) {
    if (!args) return E_POINTER;

    UINT64 navigationId = 0;
    BOOL succeeded = FALSE;
    if (FAILED(args->get_NavigationId(&navigationId)) ||
        FAILED(args->get_IsSuccess(&succeeded))) {
        return S_OK;
    }
    if (navigationId != activeNavigationId_) return S_OK;

    pageReady_ = succeeded != FALSE;
    if (pageReady_) {
        // Fresh HTML already embeds the presentation values it was rendered
        // with; only re-apply in place when a setting changed mid-navigation.
        if (presentationRevision_ != navigationStartPresentationRevision_) {
            ApplyPendingPresentation();
        }
    } else if (visible_) {
        SetFailure(E_FAIL, L"The rendered Markdown could not be loaded");
    }
    return S_OK;
}

void WebViewHost::ApplyPendingPresentation() {
    if (pageReady_ && webView_) {
        ExecutePresentation(pendingPresentation_, presentationRevision_);
    }
}

void WebViewHost::RequestPresentationFallback(
    const unsigned long long presentationRevision) {
    if (presentationRevision != presentationRevision_ ||
        presentationRevision == fallbackRevision_) {
        return;
    }
    fallbackRevision_ = presentationRevision;
    if (callbacks_.onRefreshRequested) callbacks_.onRefreshRequested();
}

void WebViewHost::ExecutePresentation(const ReaderPresentation& presentation,
    const unsigned long long presentationRevision) {
    if (!webView_ || !pageReady_) return;

    wchar_t* rawSource = nullptr;
    if (FAILED(webView_->get_Source(&rawSource))) {
        RequestPresentationFallback(presentationRevision);
        return;
    }
    const std::wstring source = TakeCoTaskString(rawSource);
    if (StripQueryAndFragment(source) != L"https://reader.local/document") {
        RequestPresentationFallback(presentationRevision);
        return;
    }

    const int fontSize = std::clamp(presentation.fontSizePx, 12, 30);
    const int contentWidth = std::clamp(presentation.contentWidthPx, 560, 1400);
    const double lineHeight = std::clamp(
        std::isfinite(presentation.lineHeight) ? presentation.lineHeight : 1.72, 1.2, 2.2);

    std::wostringstream script;
    script.imbue(std::locale::classic());
    script << LR"JS((() => {
const s = {revision:)JS" << presentationRevision
        << L",fontSizePx:" << fontSize
        << L",contentWidthPx:" << contentWidth
        << L",lineHeight:" << std::fixed << std::setprecision(2) << lineHeight
        << L",dark:" << (presentation.darkTheme ? L"true" : L"false")
        << L",showHeader:" << (presentation.showHeader ? L"true" : L"false")
        << LR"JS(};
if (location.origin !== "https://reader.local" ||
    location.pathname !== "/document") return false;
const root = document.documentElement;
const header = document.querySelector(".reader-toolbar");
if (!root || !header) return false;
if (Number(root.dataset.settingsRevision || "0") >= s.revision) return true;
const oldMax = Math.max(0, root.scrollHeight - window.innerHeight);
const ratio = oldMax === 0 ? 0 : Math.min(1, Math.max(0, window.scrollY / oldMax));
root.style.setProperty("--font-size", String(s.fontSizePx) + "px");
root.style.setProperty("--content-width", String(s.contentWidthPx) + "px");
root.style.setProperty("--line-height", String(s.lineHeight));
root.dataset.theme = s.dark ? "dark" : "light";
header.hidden = !s.showHeader;
root.dataset.settingsRevision = String(s.revision);
const newMax = Math.max(0, root.scrollHeight - window.innerHeight);
window.scrollTo({top: ratio * newMax, behavior: "instant"});
return true;
})())JS";

    const std::weak_ptr<LifetimeState> lifetime = lifetime_;
    const HRESULT started = webView_->ExecuteScript(
        script.str().c_str(),
        Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
            [lifetime, presentationRevision](const HRESULT result,
                const wchar_t* resultJson) -> HRESULT {
                const auto state = lifetime.lock();
                if (!state || !state->owner) return S_OK;

                WebViewHost* self = state->owner;
                const bool applied = SUCCEEDED(result) && resultJson &&
                    std::wcscmp(resultJson, L"true") == 0;
                if (!applied) self->RequestPresentationFallback(presentationRevision);
                return S_OK;
            }).Get());

    if (FAILED(started)) RequestPresentationFallback(presentationRevision);
}

void WebViewHost::NavigateCurrentDocument() {
    if (!webView_) return;
    pageReady_ = false;
    scrollReportsAccepted_ = false;
    navigationStartPresentationRevision_ = presentationRevision_;
    std::wostringstream uri;
    uri << L"https://reader.local/document?revision=" << revision_;
    const HRESULT result = webView_->Navigate(uri.str().c_str());
    if (FAILED(result)) SetFailure(result, L"The rendered Markdown could not be loaded");
    if (controller_) controller_->put_IsVisible(visible_ ? TRUE : FALSE);
}

void WebViewHost::SyncToTarget() {
    if (!visible_ || !hostWindow_ || !targetScintilla_ || !parentWindow_) return;
    if (!IsWindowVisible(targetScintilla_)) {
        ShowWindow(hostWindow_, SW_HIDE);
        if (controller_) controller_->put_IsVisible(FALSE);
        return;
    }

    RECT targetRect{};
    if (!GetWindowRect(targetScintilla_, &targetRect)) return;
    MapWindowPoints(nullptr, parentWindow_, reinterpret_cast<POINT*>(&targetRect), 2);
    const int width = static_cast<int>(std::max<LONG>(0, targetRect.right - targetRect.left));
    const int height = static_cast<int>(std::max<LONG>(0, targetRect.bottom - targetRect.top));
    SetWindowPos(hostWindow_, HWND_TOP, targetRect.left, targetRect.top, width, height,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
    if (controller_) {
        RECT bounds{0, 0, width, height};
        controller_->put_Bounds(bounds);
        controller_->put_IsVisible(TRUE);
    }
}

void WebViewHost::PaintPlaceholder() {
    PAINTSTRUCT paint{};
    const HDC dc = BeginPaint(hostWindow_, &paint);
    RECT client{};
    GetClientRect(hostWindow_, &client);
    FillRect(dc, &client, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(78, 86, 101));
    RECT textRect = client;
    InflateRect(&textRect, -32, -32);
    DrawTextW(dc, statusText_.c_str(), -1, &textRect, DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_NOPREFIX);
    EndPaint(hostWindow_, &paint);
}

void WebViewHost::SetFailure(const HRESULT result, std::wstring context) {
    statusText_ = std::move(context) + L".\n\n" + HResultMessage(result) +
        L"\n\nInstall or repair the Microsoft Edge WebView2 Runtime, then restart Notepad++.";
    if (hostWindow_) InvalidateRect(hostWindow_, nullptr, TRUE);
}

HRESULT WebViewHost::Respond(ICoreWebView2WebResourceRequestedEventArgs* args,
    const std::string_view content, const wchar_t* contentType, const int status, const wchar_t* reason) {
    if (!args || !environment_) return E_POINTER;
    if (content.size() > std::numeric_limits<UINT>::max()) {
        return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
    }
    ComPtr<IStream> stream;
    const BYTE* bytes = content.empty()
        ? nullptr
        : reinterpret_cast<const BYTE*>(content.data());
    stream.Attach(SHCreateMemStream(bytes,
        static_cast<UINT>(content.size())));
    if (!stream) return E_OUTOFMEMORY;

    std::wstring headers = L"Content-Type: ";
    headers += contentType;
    headers += L"\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff";
    ComPtr<ICoreWebView2WebResourceResponse> response;
    const HRESULT result = environment_->CreateWebResourceResponse(stream.Get(), status, reason,
        headers.c_str(), &response);
    if (FAILED(result)) return result;
    return args->put_Response(response.Get());
}

HRESULT WebViewHost::HandleWebResourceRequested(ICoreWebView2WebResourceRequestedEventArgs* args) {
    ComPtr<ICoreWebView2WebResourceRequest> request;
    if (!args || FAILED(args->get_Request(&request)) || !request) return S_OK;
    wchar_t* rawUri = nullptr;
    if (FAILED(request->get_Uri(&rawUri))) return S_OK;
    const std::wstring uri = TakeCoTaskString(rawUri);
    const std::wstring_view path = StripQueryAndFragment(uri);

    if (path == L"https://reader.local/document") {
        return Respond(args, currentHtml_, L"text/html; charset=utf-8");
    }
    if (path == L"https://reader.local/reader.css") {
        return Respond(args, resources::kReaderCss, L"text/css; charset=utf-8");
    }
    if (path == L"https://reader.local/reader.js") {
        return Respond(args, resources::kReaderJs, L"text/javascript; charset=utf-8");
    }
    if (path == L"https://reader.local/blocked-image.svg") {
        return Respond(args, resources::kBlockedImageSvg, L"image/svg+xml; charset=utf-8");
    }
    if (uri.starts_with(L"https://document.local/")) {
        return HandleLocalImageRequest(args, uri);
    }
    return Respond(args, "Not found", L"text/plain; charset=utf-8", 404, L"Not Found");
}

HRESULT WebViewHost::HandleLocalImageRequest(ICoreWebView2WebResourceRequestedEventArgs* args,
    const std::wstring_view uri) {
    if (!args || documentDirectory_.empty()) {
        return Respond(args, "Not found", L"text/plain; charset=utf-8", 404, L"Not Found");
    }

    COREWEBVIEW2_WEB_RESOURCE_CONTEXT context{};
    if (FAILED(args->get_ResourceContext(&context)) ||
        context != COREWEBVIEW2_WEB_RESOURCE_CONTEXT_IMAGE) {
        return Respond(args, "Only local image resources are allowed", L"text/plain; charset=utf-8",
            403, L"Forbidden");
    }

    constexpr std::wstring_view prefix = L"https://document.local/";
    const std::wstring_view withoutOrigin = uri.substr(prefix.size());
    const auto resolved = ResolveSafeRelativePath(documentDirectory_, WideToUtf8(withoutOrigin));
    if (!resolved) {
        return Respond(args, "Unsafe local path", L"text/plain; charset=utf-8", 403, L"Forbidden");
    }

    const wchar_t* contentType = ImageContentType(*resolved);
    if (!contentType) {
        return Respond(args, "Unsupported local image type", L"text/plain; charset=utf-8",
            415, L"Unsupported Media Type");
    }

    std::error_code error;
    if (!std::filesystem::is_regular_file(*resolved, error) || error) {
        return Respond(args, "Image not found", L"text/plain; charset=utf-8", 404, L"Not Found");
    }

    constexpr std::uintmax_t maximumImageBytes = 64ull * 1024ull * 1024ull;
    const std::uintmax_t size = std::filesystem::file_size(*resolved, error);
    if (error || size > maximumImageBytes || size > std::numeric_limits<std::size_t>::max()) {
        return Respond(args, "Image is too large", L"text/plain; charset=utf-8",
            413, L"Content Too Large");
    }

    std::ifstream input(*resolved, std::ios::binary);
    if (!input) return Respond(args, "Image not found", L"text/plain; charset=utf-8", 404, L"Not Found");
    std::string bytes(static_cast<std::size_t>(size), '\0');
    if (!bytes.empty()) {
        input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!input) return Respond(args, "Image read failed", L"text/plain; charset=utf-8", 500,
            L"Internal Server Error");
    }
    return Respond(args, bytes, contentType);
}

HRESULT WebViewHost::HandleNavigationStarting(ICoreWebView2NavigationStartingEventArgs* args) {
    if (!args) return E_POINTER;
    wchar_t* rawUri = nullptr;
    if (FAILED(args->get_Uri(&rawUri))) return S_OK;
    const std::wstring uri = TakeCoTaskString(rawUri);
    if (StripQueryAndFragment(uri) == L"https://reader.local/document") {
        UINT64 navigationId = 0;
        if (SUCCEEDED(args->get_NavigationId(&navigationId))) {
            pageReady_ = false;
            scrollReportsAccepted_ = false;
            activeNavigationId_ = navigationId;
        }
        return S_OK;
    }
    if (uri == L"about:blank") return S_OK;

    args->put_Cancel(TRUE);
    constexpr std::wstring_view localPrefix = L"https://document.local/";
    if (uri.starts_with(localPrefix)) {
        if (callbacks_.onLocalNavigation) {
            callbacks_.onLocalNavigation(WideToUtf8(uri.substr(localPrefix.size())));
        }
    } else if (IsExternalScheme(uri) && callbacks_.onExternalNavigation) {
        callbacks_.onExternalNavigation(uri);
    }
    return S_OK;
}

HRESULT WebViewHost::HandleNewWindowRequested(ICoreWebView2NewWindowRequestedEventArgs* args) {
    if (!args) return E_POINTER;
    args->put_Handled(TRUE);
    wchar_t* rawUri = nullptr;
    if (SUCCEEDED(args->get_Uri(&rawUri))) {
        const std::wstring uri = TakeCoTaskString(rawUri);
        constexpr std::wstring_view localPrefix = L"https://document.local/";
        if (uri.starts_with(localPrefix)) {
            if (callbacks_.onLocalNavigation) {
                callbacks_.onLocalNavigation(WideToUtf8(uri.substr(localPrefix.size())));
            }
        } else if (IsExternalScheme(uri) && callbacks_.onExternalNavigation) {
            callbacks_.onExternalNavigation(uri);
        }
    }
    return S_OK;
}

HRESULT WebViewHost::HandleWebMessage(ICoreWebView2WebMessageReceivedEventArgs* args) {
    if (!args) return E_POINTER;
    wchar_t* rawSource = nullptr;
    if (FAILED(args->get_Source(&rawSource))) return S_OK;
    const std::wstring source = TakeCoTaskString(rawSource);
    if (StripQueryAndFragment(source) != L"https://reader.local/document") return S_OK;

    wchar_t* rawMessage = nullptr;
    if (FAILED(args->TryGetWebMessageAsString(&rawMessage))) return S_OK;
    const std::wstring message = TakeCoTaskString(rawMessage);
    if (message.starts_with(L"scroll:")) {
        // Reports emitted while a page is still restoring its initial offset
        // describe setup, not the reader position; the gate stays closed until
        // the page announces "scroll-ready" for the current revision.
        if (!scrollReportsAccepted_) return S_OK;
        try {
            const double ratio = std::stod(message.substr(7));
            if (callbacks_.onScroll) callbacks_.onScroll(std::clamp(ratio, 0.0, 1.0));
        } catch (...) {
            // Ignore malformed messages from the embedded page.
        }
    } else if (message.starts_with(L"scroll-ready:")) {
        constexpr std::size_t prefixLength = 13;
        const std::wstring_view payload(message.data() + prefixLength,
            message.size() - prefixLength);
        const std::size_t separator = payload.find(L':');
        if (separator == std::wstring_view::npos) return S_OK;
        try {
            const unsigned long long pageRevision =
                std::stoull(std::wstring(payload.substr(0, separator)));
            if (pageRevision != revision_) return S_OK;
            scrollReportsAccepted_ = true;
            const double ratio = std::stod(std::wstring(payload.substr(separator + 1)));
            if (callbacks_.onScroll) callbacks_.onScroll(std::clamp(ratio, 0.0, 1.0));
        } catch (...) {
            // Ignore malformed messages from the embedded page.
        }
    } else if (message == L"exit") {
        if (callbacks_.onExit) callbacks_.onExit();
    } else if (message == L"save") {
        if (callbacks_.onSave) callbacks_.onSave();
    } else if (message == L"save-as") {
        if (callbacks_.onSaveAs) callbacks_.onSaveAs();
    }
    return S_OK;
}

HRESULT WebViewHost::HandleAccelerator(ICoreWebView2AcceleratorKeyPressedEventArgs* args) {
    if (!args) return E_POINTER;
    COREWEBVIEW2_KEY_EVENT_KIND kind{};
    UINT key = 0;
    args->get_KeyEventKind(&kind);
    args->get_VirtualKey(&key);
    if (kind != COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN &&
        kind != COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN) return S_OK;

    if (DispatchShortcut(key)) args->put_Handled(TRUE);
    return S_OK;
}

bool WebViewHost::DispatchShortcut(const UINT virtualKey) {
    const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    if (virtualKey == VK_ESCAPE || (ctrl && alt && virtualKey == 'M')) {
        if (callbacks_.onExit) callbacks_.onExit();
        return true;
    }
    if (ctrl && !alt && virtualKey == 'S') {
        if (shift) {
            if (callbacks_.onSaveAs) callbacks_.onSaveAs();
        } else if (callbacks_.onSave) {
            callbacks_.onSave();
        }
        return true;
    }
    return false;
}

HRESULT WebViewHost::ExportPdf(const std::filesystem::path& destination,
    std::function<void(HRESULT, bool)> completion) {
    if (!webView_) return E_PENDING;
    if (pdfInProgress_) return HRESULT_FROM_WIN32(ERROR_BUSY);
    ComPtr<ICoreWebView2_7> webView7;
    const HRESULT query = webView_.As(&webView7);
    if (FAILED(query) || !webView7) return E_NOINTERFACE;
    pdfInProgress_ = true;
    const std::weak_ptr<LifetimeState> lifetime = lifetime_;
    const HRESULT started = webView7->PrintToPdf(destination.c_str(), nullptr,
        Callback<ICoreWebView2PrintToPdfCompletedHandler>(
            [lifetime, completion = std::move(completion)](const HRESULT result,
                const BOOL successful) -> HRESULT {
                const auto state = lifetime.lock();
                if (state && state->owner) {
                    state->owner->pdfInProgress_ = false;
                    if (completion) completion(result, successful != FALSE);
                }
                return S_OK;
            }).Get());
    if (FAILED(started)) pdfInProgress_ = false;
    return started;
}

void WebViewHost::Shutdown() {
    if (lifetime_) lifetime_->owner = nullptr;
    if (hostWindow_) {
        KillTimer(hostWindow_, kSyncTimer);
        KillTimer(hostWindow_, kRefreshTimer);
    }
    pageReady_ = false;
    if (controller_) controller_->Close();
    webView_.Reset();
    controller_.Reset();
    environment_.Reset();
    if (hostWindow_) {
        DestroyWindow(hostWindow_);
        hostWindow_ = nullptr;
    }
    visible_ = false;
}

} // namespace nppmr
