#pragma once

#include "MarkdownRenderer.h"
#include "PluginApi.h"
#include "Settings.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace nppmr {

struct ReaderPresentation;
class WebViewHost;

enum class CommandIndex : std::size_t {
    ToggleReader = 0,
    ExportPdf,
    IncreaseFont,
    DecreaseFont,
    ResetFont,
    IncreaseContentWidth,
    DecreaseContentWidth,
    ResetContentWidth,
    ShowReaderHeader,
    ThemeFollowNotepad,
    ThemeLight,
    ThemeDark,
    AllowRemoteImages,
    About,
    Count
};

inline constexpr std::size_t kCommandCount = static_cast<std::size_t>(CommandIndex::Count);

class ReaderController final {
public:
    static ReaderController& Instance();

    void SetPluginContext(NppData nppData, HINSTANCE moduleInstance);
    void SetFunctionItems(FuncItem* items, int count);
    void OnNotification(const SCNotification* notification);
    void Shutdown();

    void ToggleReader();
    void ExportCurrentPdf();
    void IncreaseFontSize();
    void DecreaseFontSize();
    void ResetFontSize();
    void IncreaseContentWidth();
    void DecreaseContentWidth();
    void ResetContentWidth();
    void ToggleReaderHeader();
    void SetTheme(ThemeMode mode);
    void ToggleRemoteImages();
    void ShowAbout() const;

private:
    struct DocumentKey {
        std::uintptr_t bufferId = 0;
        int view = MAIN_VIEW;

        bool operator==(const DocumentKey&) const = default;
    };

    struct DocumentKeyHash {
        std::size_t operator()(const DocumentKey& key) const noexcept;
    };

    struct DocumentState {
        bool enabled = false;
        double scrollRatio = 0.0;
        std::intptr_t caretPosition = 0;
        int firstVisibleLine = 0;
    };

    struct ViewDocument {
        DocumentKey key;
        HWND scintilla = nullptr;
        int documentIndex = -1;
        std::wstring fullPath;
    };

    // Identifies the page a host still holds after being hidden, so switching
    // back to the same buffer can re-show it without a fresh render/navigation.
    struct RetainedPage {
        std::uintptr_t bufferId = 0;
        std::size_t contentHash = 0;
        std::size_t contentLength = 0;
        unsigned long long renderSerial = 0;
        unsigned long long presentationSerial = 0;
    };

    ReaderController() = default;
    ~ReaderController() = default;
    ReaderController(const ReaderController&) = delete;
    ReaderController& operator=(const ReaderController&) = delete;

    void EnsureReady();
    void RegisterToolbarIcon();
    void ReleaseToolbarResources();

    int ActiveView() const;
    HWND ScintillaForView(int view) const;
    std::optional<ViewDocument> CurrentDocumentForView(int view) const;
    std::optional<DocumentKey> CurrentKeyForView(int view) const;
    void ActivateViewDocument(int view) const;

    bool EnsureHost(int view);
    void RefreshAllViews();
    void RefreshView(int view);
    bool TryReuseRetainedPage(int view, const ViewDocument& document);
    void RenderView(int view, const ViewDocument& document, DocumentState& state);
    void ScheduleRefreshForBuffer(std::uintptr_t bufferId);
    void DisableView(int view, bool restoreEditorFocus = true);
    void EraseBufferState(std::uintptr_t bufferId);

    std::string ReadScintillaTextUtf8(HWND scintilla) const;
    std::filesystem::path DocumentDirectory(const std::wstring& fullPath) const;
    bool UseDarkTheme() const;

    void HandleReaderFocus(int view);
    void HandleScroll(int view, double ratio);
    void HandleLocalNavigation(int view, const std::string& relativeUrl);
    void HandleExternalNavigation(const std::wstring& uri) const;

    ReaderPresentation CurrentPresentation() const;
    void ApplyPresentationToVisibleViews();
    void ApplyPresentationChange();
    void ApplySettingsChange();
    void UpdateMenuChecks();
    void SetMenuCheck(CommandIndex index, bool checked) const;
    int CommandId(CommandIndex index) const;

    void ShowError(std::wstring_view message, std::wstring_view title = L"Markdown Reader") const;
    std::filesystem::path FallbackConfigDirectory() const;

    NppData nppData_{};
    HINSTANCE moduleInstance_ = nullptr;
    FuncItem* functionItems_ = nullptr;
    int functionItemCount_ = 0;

    bool ready_ = false;
    bool shuttingDown_ = false;
    bool comInitialized_ = false;
    HRESULT comResult_ = E_PENDING;

    SettingsStore settingsStore_;
    MarkdownRenderer renderer_;
    std::filesystem::path pluginDataDirectory_;
    std::filesystem::path webViewDataDirectory_;

    std::array<std::unique_ptr<WebViewHost>, 2> hosts_;
    std::array<std::uintptr_t, 2> displayedBuffers_{};
    std::array<RetainedPage, 2> retainedPages_{};
    std::unordered_map<DocumentKey, DocumentState, DocumentKeyHash> documentStates_;

    // Bumped when anything outside the buffer text changes what a render would
    // produce (title/path/policy), or when presentation values change in place.
    unsigned long long renderSerial_ = 0;
    unsigned long long presentationSerial_ = 0;

    HBITMAP toolbarBitmap_ = nullptr;
    HICON toolbarLightIcon_ = nullptr;
    HICON toolbarDarkIcon_ = nullptr;
};

} // namespace nppmr
