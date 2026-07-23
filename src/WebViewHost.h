#pragma once

#include <windows.h>
#include <wrl.h>
#include <WebView2.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace nppmr {

struct WebViewCallbacks {
    std::function<void()> onFocused;
    std::function<void(double)> onScroll;
    std::function<void()> onExit;
    std::function<void()> onSave;
    std::function<void()> onSaveAs;
    std::function<void()> onRefreshRequested;
    std::function<void(std::string)> onLocalNavigation;
    std::function<void(std::wstring)> onExternalNavigation;
};

struct ReaderPresentation {
    int fontSizePx = 17;
    int contentWidthPx = 860;
    double lineHeight = 1.72;
    bool darkTheme = false;
    bool showHeader = true;
};

class WebViewHost final {
public:
    WebViewHost(HINSTANCE instance, HWND nppWindow, HWND targetScintilla, int viewIndex,
        std::filesystem::path userDataFolder, WebViewCallbacks callbacks);
    ~WebViewHost();

    WebViewHost(const WebViewHost&) = delete;
    WebViewHost& operator=(const WebViewHost&) = delete;

    bool IsCreated() const noexcept { return hostWindow_ != nullptr; }
    bool IsReady() const noexcept { return webView_ != nullptr; }
    bool IsVisible() const noexcept { return visible_; }
    int ViewIndex() const noexcept { return viewIndex_; }

    void ShowDocument(std::string html, std::filesystem::path documentDirectory,
        ReaderPresentation presentation);
    void ApplyPresentation(const ReaderPresentation& presentation);
    void Hide();
    void Focus();
    void ScheduleRefresh(unsigned delayMilliseconds = 220);
    HRESULT ExportPdf(const std::filesystem::path& destination,
        std::function<void(HRESULT, bool)> completion);
    void Shutdown();

private:
    struct LifetimeState {
        WebViewHost* owner = nullptr;
    };

    static constexpr UINT_PTR kSyncTimer = 1;
    static constexpr UINT_PTR kRefreshTimer = 2;
    static constexpr wchar_t kWindowClassName[] = L"NppMarkdownReader.WebViewHost";

    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    static bool EnsureWindowClass(HINSTANCE instance);

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void EnsureInitialized();
    void CompleteControllerCreation(HRESULT result, ICoreWebView2Controller* controller);
    void ConfigureWebView();
    HRESULT HandleNavigationCompleted(ICoreWebView2NavigationCompletedEventArgs* args);
    void NavigateCurrentDocument();
    void ApplyPendingPresentation();
    void RequestPresentationFallback(unsigned long long presentationRevision);
    void ExecutePresentation(const ReaderPresentation& presentation,
        unsigned long long presentationRevision);
    void SyncToTarget();
    void PaintPlaceholder();
    void SetFailure(HRESULT result, std::wstring context);

    HRESULT HandleWebResourceRequested(ICoreWebView2WebResourceRequestedEventArgs* args);
    HRESULT HandleNavigationStarting(ICoreWebView2NavigationStartingEventArgs* args);
    HRESULT HandleNewWindowRequested(ICoreWebView2NewWindowRequestedEventArgs* args);
    HRESULT HandleWebMessage(ICoreWebView2WebMessageReceivedEventArgs* args);
    HRESULT HandleAccelerator(ICoreWebView2AcceleratorKeyPressedEventArgs* args);
    bool DispatchShortcut(UINT virtualKey);
    HRESULT HandleLocalImageRequest(ICoreWebView2WebResourceRequestedEventArgs* args,
        std::wstring_view uri);
    HRESULT Respond(ICoreWebView2WebResourceRequestedEventArgs* args, std::string_view content,
        const wchar_t* contentType, int status = 200, const wchar_t* reason = L"OK");

    HINSTANCE instance_ = nullptr;
    HWND nppWindow_ = nullptr;
    HWND targetScintilla_ = nullptr;
    HWND parentWindow_ = nullptr;
    HWND hostWindow_ = nullptr;
    int viewIndex_ = 0;
    std::filesystem::path userDataFolder_;
    WebViewCallbacks callbacks_;
    std::shared_ptr<LifetimeState> lifetime_;

    bool initializationStarted_ = false;
    bool visible_ = false;
    bool pageReady_ = false;
    bool pdfInProgress_ = false;
    UINT64 activeNavigationId_ = 0;
    unsigned long long revision_ = 0;
    unsigned long long presentationRevision_ = 0;
    unsigned long long fallbackRevision_ = 0;
    std::wstring statusText_ = L"Starting Markdown Reader…";
    std::string currentHtml_;
    std::filesystem::path documentDirectory_;
    ReaderPresentation pendingPresentation_{};

    Microsoft::WRL::ComPtr<ICoreWebView2Environment> environment_;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
    Microsoft::WRL::ComPtr<ICoreWebView2> webView_;

    EventRegistrationToken resourceToken_{};
    EventRegistrationToken navigationToken_{};
    EventRegistrationToken navigationCompletedToken_{};
    EventRegistrationToken newWindowToken_{};
    EventRegistrationToken messageToken_{};
    EventRegistrationToken acceleratorToken_{};
};

} // namespace nppmr
