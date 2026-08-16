#include "ReaderController.h"

#include "Resource.h"
#include "Utilities.h"
#include "WebViewEnvironment.h"
#include "WebViewHost.h"

#include <commdlg.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>
#include <cwctype>
#include <exception>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <vector>

#ifndef NPPMR_VERSION_STRING
#define NPPMR_VERSION_STRING L"0.1.0-dev"
#endif

namespace nppmr {
namespace {

constexpr wchar_t kPluginName[] = L"Markdown Reader";

bool IsMarkdownExtension(std::filesystem::path path) {
    std::wstring extension = path.extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](const wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return extension == L".md" || extension == L".markdown" || extension == L".mdown" ||
        extension == L".mkd" || extension == L".mkdn";
}

std::wstring DefaultPdfName(const std::wstring& sourcePath) {
    const std::filesystem::path source(sourcePath);
    if (source.is_absolute() && !source.filename().empty()) {
        return (source.parent_path() / (source.stem().wstring() + L".pdf")).wstring();
    }
    if (!source.filename().empty()) {
        return source.stem().wstring() + L".pdf";
    }
    return L"Markdown Document.pdf";
}

bool OpenWithShell(const HWND owner, const std::wstring& target) {
    const HINSTANCE result = ShellExecuteW(owner, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
}

} // namespace

ReaderController& ReaderController::Instance() {
    static ReaderController instance;
    return instance;
}

std::size_t ReaderController::DocumentKeyHash::operator()(const DocumentKey& key) const noexcept {
    const std::size_t first = std::hash<std::uintptr_t>{}(key.bufferId);
    const std::size_t second = std::hash<int>{}(key.view);
    return first ^ (second + 0x9e3779b9u + (first << 6u) + (first >> 2u));
}

void ReaderController::SetPluginContext(const NppData nppData, const HINSTANCE moduleInstance) {
    nppData_ = nppData;
    moduleInstance_ = moduleInstance;
    shuttingDown_ = false;
}

void ReaderController::SetFunctionItems(FuncItem* const items, const int count) {
    functionItems_ = items;
    functionItemCount_ = count;
    UpdateMenuChecks();
}

void ReaderController::EnsureReady() {
    if (ready_ || shuttingDown_ || !nppData_._nppHandle) return;

    std::wstring configRoot = QueryNppString(nppData_._nppHandle, NPPM_GETPLUGINSCONFIGDIR);
    pluginDataDirectory_ = configRoot.empty()
        ? FallbackConfigDirectory()
        : std::filesystem::path(configRoot) / L"NppMarkdownReader";
    webViewDataDirectory_ = pluginDataDirectory_ / L"WebView2";

    settingsStore_.SetConfigDirectory(pluginDataDirectory_);
    settingsStore_.Load();

    comResult_ = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    comInitialized_ = SUCCEEDED(comResult_);
    ready_ = true;

    UpdateMenuChecks();
}

std::filesystem::path ReaderController::FallbackConfigDirectory() const {
    std::error_code error;
    const std::filesystem::path temp = std::filesystem::temp_directory_path(error);
    if (!error && !temp.empty()) return temp / L"NppMarkdownReader";
    return std::filesystem::path(L".") / L"NppMarkdownReader";
}

int ReaderController::ActiveView() const {
    int view = MAIN_VIEW;
    if (!nppData_._nppHandle ||
        !SendMessageW(nppData_._nppHandle, NPPM_GETCURRENTSCINTILLA, 0,
            reinterpret_cast<LPARAM>(&view))) {
        return MAIN_VIEW;
    }
    return view == SUB_VIEW ? SUB_VIEW : MAIN_VIEW;
}

HWND ReaderController::ScintillaForView(const int view) const {
    return view == SUB_VIEW ? nppData_._scintillaSecondHandle : nppData_._scintillaMainHandle;
}

std::optional<ReaderController::ViewDocument> ReaderController::CurrentDocumentForView(const int view) const {
    if (!nppData_._nppHandle || (view != MAIN_VIEW && view != SUB_VIEW)) return std::nullopt;
    const LRESULT indexResult = SendMessageW(nppData_._nppHandle, NPPM_GETCURRENTDOCINDEX, 0, view);
    if (indexResult < 0 || indexResult > std::numeric_limits<int>::max()) return std::nullopt;

    const auto bufferId = static_cast<std::uintptr_t>(SendMessageW(nppData_._nppHandle,
        NPPM_GETBUFFERIDFROMPOS, static_cast<WPARAM>(indexResult), view));
    if (bufferId == 0) return std::nullopt;

    ViewDocument document;
    document.key = {bufferId, view};
    document.scintilla = ScintillaForView(view);
    document.documentIndex = static_cast<int>(indexResult);
    document.fullPath = QueryBufferPath(nppData_._nppHandle, bufferId);
    if (!document.scintilla) return std::nullopt;
    return document;
}

std::optional<ReaderController::DocumentKey> ReaderController::CurrentKeyForView(const int view) const {
    const auto document = CurrentDocumentForView(view);
    if (!document) return std::nullopt;
    return document->key;
}

void ReaderController::ActivateViewDocument(const int view) const {
    const auto document = CurrentDocumentForView(view);
    if (!document) return;
    SendMessageW(nppData_._nppHandle, NPPM_ACTIVATEDOC,
        static_cast<WPARAM>(view), static_cast<LPARAM>(document->documentIndex));
}

std::string ReaderController::ReadScintillaTextUtf8(const HWND scintilla) const {
    if (!scintilla) return {};
    const LRESULT lengthResult = SendMessageW(scintilla, SCI_GETTEXTLENGTH, 0, 0);
    if (lengthResult <= 0) return {};

    constexpr std::size_t maximumDocumentBytes = 128ull * 1024ull * 1024ull;
    const auto unsignedLength = static_cast<unsigned long long>(lengthResult);
    if (unsignedLength > maximumDocumentBytes ||
        unsignedLength > static_cast<unsigned long long>(std::numeric_limits<int>::max() - 1)) {
        throw std::runtime_error("The document is too large for Reader Mode (128 MiB limit).");
    }

    const std::size_t length = static_cast<std::size_t>(unsignedLength);
    std::vector<char> buffer(length + 1, '\0');
    SendMessageW(scintilla, SCI_GETTEXT, static_cast<WPARAM>(buffer.size()),
        reinterpret_cast<LPARAM>(buffer.data()));
    std::string bytes(buffer.data(), length);

    const UINT codePage = static_cast<UINT>(SendMessageW(scintilla, SCI_GETCODEPAGE, 0, 0));
    if (codePage == SC_CP_UTF8) return bytes;

    const UINT sourceCodePage = codePage == 0 ? CP_ACP : codePage;
    const int wideLength = MultiByteToWideChar(sourceCodePage, 0, bytes.data(),
        static_cast<int>(bytes.size()), nullptr, 0);
    if (wideLength <= 0) return bytes;

    std::wstring wide(static_cast<std::size_t>(wideLength), L'\0');
    if (MultiByteToWideChar(sourceCodePage, 0, bytes.data(), static_cast<int>(bytes.size()),
            wide.data(), wideLength) <= 0) {
        return bytes;
    }
    return WideToUtf8(wide);
}

std::filesystem::path ReaderController::DocumentDirectory(const std::wstring& fullPath) const {
    if (fullPath.empty()) return {};
    const std::filesystem::path path(fullPath);
    if (!path.is_absolute() || path.parent_path().empty()) return {};
    return path.parent_path();
}

bool ReaderController::UseDarkTheme() const {
    switch (settingsStore_.Values().theme) {
    case ThemeMode::Light:
        return false;
    case ThemeMode::Dark:
        return true;
    case ThemeMode::FollowNotepad:
    default:
        return nppData_._nppHandle &&
            SendMessageW(nppData_._nppHandle, NPPM_ISDARKMODEENABLED, 0, 0) != FALSE;
    }
}

ReaderPresentation ReaderController::CurrentPresentation() const {
    const ReaderSettings& settings = settingsStore_.Values();
    return ReaderPresentation{
        settings.fontSizePx,
        settings.contentWidthPx,
        settings.lineHeight,
        UseDarkTheme(),
        settings.showHeader};
}

bool ReaderController::EnsureHost(const int view) {
    EnsureReady();
    if (view != MAIN_VIEW && view != SUB_VIEW) return false;
    if (hosts_[view]) return true;

    if (FAILED(comResult_)) {
        std::wstring message = L"The embedded Markdown reader could not initialize COM in a "
            L"single-threaded apartment.\n\n";
        message += HResultMessage(comResult_);
        ShowError(message);
        return false;
    }

    WebViewCallbacks callbacks;
    callbacks.onFocused = [this, view] { HandleReaderFocus(view); };
    callbacks.onScroll = [this, view](const double ratio) { HandleScroll(view, ratio); };
    callbacks.onExit = [this, view] { DisableView(view); };
    callbacks.onSave = [this, view] {
        ActivateViewDocument(view);
        SendMessageW(nppData_._nppHandle, NPPM_SAVECURRENTFILE, 0, 0);
    };
    callbacks.onSaveAs = [this, view] {
        ActivateViewDocument(view);
        SendMessageW(nppData_._nppHandle, NPPM_MENUCOMMAND, 0, IDM_FILE_SAVEAS);
    };
    callbacks.onRefreshRequested = [this, view] { RefreshView(view); };
    callbacks.onLocalNavigation = [this, view](std::string url) {
        HandleLocalNavigation(view, url);
    };
    callbacks.onExternalNavigation = [this](std::wstring uri) {
        HandleExternalNavigation(uri);
    };

    hosts_[view] = std::make_unique<WebViewHost>(moduleInstance_, nppData_._nppHandle,
        ScintillaForView(view), view, webViewDataDirectory_, std::move(callbacks));
    if (!hosts_[view]->IsCreated()) {
        hosts_[view].reset();
        ShowError(L"The reader overlay window could not be created.");
        return false;
    }
    return true;
}

void ReaderController::ToggleReader() {
    EnsureReady();
    if (shuttingDown_) return;

    const int view = ActiveView();
    const auto document = CurrentDocumentForView(view);
    if (!document) {
        ShowError(L"There is no active document in this editor view.");
        return;
    }

    DocumentState& state = documentStates_[document->key];
    if (state.enabled) {
        DisableView(view);
        return;
    }

    state.caretPosition = static_cast<std::intptr_t>(
        SendMessageW(document->scintilla, SCI_GETCURRENTPOS, 0, 0));
    state.firstVisibleLine = static_cast<int>(
        SendMessageW(document->scintilla, SCI_GETFIRSTVISIBLELINE, 0, 0));
    const int lineCount = std::max(1, static_cast<int>(
        SendMessageW(document->scintilla, SCI_GETLINECOUNT, 0, 0)));
    const int linesOnScreen = std::max(1, static_cast<int>(
        SendMessageW(document->scintilla, SCI_LINESONSCREEN, 0, 0)));
    const int maximumFirstLine = std::max(0, lineCount - linesOnScreen);
    state.scrollRatio = maximumFirstLine == 0
        ? 0.0
        : std::clamp(static_cast<double>(state.firstVisibleLine) / maximumFirstLine, 0.0, 1.0);
    state.enabled = true;

    RefreshView(view);
    UpdateMenuChecks();
}

void ReaderController::RefreshAllViews() {
    if (!ready_ || shuttingDown_) return;
    RefreshView(MAIN_VIEW);
    RefreshView(SUB_VIEW);
    UpdateMenuChecks();
}

void ReaderController::RefreshView(const int view) {
    if (!ready_ || shuttingDown_ || (view != MAIN_VIEW && view != SUB_VIEW)) return;
    const auto document = CurrentDocumentForView(view);
    if (!document) {
        if (hosts_[view]) hosts_[view]->Hide();
        displayedBuffers_[view] = 0;
        return;
    }

    const auto found = documentStates_.find(document->key);
    if (found == documentStates_.end() || !found->second.enabled) {
        if (hosts_[view]) hosts_[view]->Hide();
        displayedBuffers_[view] = 0;
        return;
    }

    if (TryReuseRetainedPage(view, *document)) return;
    RenderView(view, *document, found->second);
}

bool ReaderController::TryReuseRetainedPage(const int view, const ViewDocument& document) {
    const RetainedPage& retained = retainedPages_[view];
    if (!hosts_[view]) return false;
    if (retained.bufferId != document.key.bufferId || retained.renderSerial != renderSerial_) {
        return false;
    }

    // The retained page was rendered from this buffer's text; verify the text
    // still matches so edits — typed while visible or made while the page was
    // hidden, including reloads — force a fresh render instead of reuse.
    bool contentMatches = false;
    try {
        const std::string source = ReadScintillaTextUtf8(document.scintilla);
        contentMatches = source.size() == retained.contentLength &&
            std::hash<std::string_view>{}(source) == retained.contentHash;
    } catch (...) {
        contentMatches = false;
    }
    if (!contentMatches) return false;

    // Already showing this exact page: a no-op. This also makes the re-entrant
    // buffer activation triggered by Focus() during ShowDocument() harmless.
    if (hosts_[view]->IsVisible() && displayedBuffers_[view] == document.key.bufferId) {
        return true;
    }

    if (!hosts_[view]->HasReadyPage()) return false;

    displayedBuffers_[view] = document.key.bufferId;
    hosts_[view]->ReShow();
    if (retained.presentationSerial != presentationSerial_) {
        hosts_[view]->ApplyPresentation(CurrentPresentation());
    }
    return true;
}

void ReaderController::RenderView(const int view, const ViewDocument& document, DocumentState& state) {
    if (!EnsureHost(view)) {
        state.enabled = false;
        displayedBuffers_[view] = 0;
        UpdateMenuChecks();
        return;
    }

    try {
        const ReaderPresentation presentation = CurrentPresentation();
        RenderOptions options;
        options.title = FileNameForDisplay(document.fullPath);
        options.documentDirectory = DocumentDirectory(document.fullPath);
        options.darkTheme = presentation.darkTheme;
        options.allowRemoteImages = settingsStore_.Values().allowRemoteImages;
        options.fontSizePx = presentation.fontSizePx;
        options.contentWidthPx = presentation.contentWidthPx;
        options.showHeader = presentation.showHeader;
        options.lineHeight = presentation.lineHeight;
        options.initialScrollRatio = state.scrollRatio;

        const std::string source = ReadScintillaTextUtf8(document.scintilla);
        const RenderResult rendered = renderer_.Render(source, options);
        if (!rendered.success) {
            state.enabled = false;
            hosts_[view]->Hide();
            displayedBuffers_[view] = 0;
            ShowError(rendered.error.empty() ? L"The Markdown document could not be rendered." : rendered.error);
            UpdateMenuChecks();
            return;
        }

        // Record the page identity before ShowDocument(): the focus it takes
        // can synchronously re-enter RefreshView, which must observe this page
        // as current rather than render the document a second time.
        retainedPages_[view] = RetainedPage{document.key.bufferId,
            std::hash<std::string_view>{}(source), source.size(),
            renderSerial_, presentationSerial_};
        displayedBuffers_[view] = document.key.bufferId;
        hosts_[view]->ShowDocument(rendered.html, options.documentDirectory, presentation);
    } catch (const std::exception& exception) {
        state.enabled = false;
        hosts_[view]->Hide();
        displayedBuffers_[view] = 0;
        std::wstring message = L"The document could not be rendered.\n\n";
        message += Utf8ToWide(exception.what());
        ShowError(message);
        UpdateMenuChecks();
    } catch (...) {
        state.enabled = false;
        hosts_[view]->Hide();
        displayedBuffers_[view] = 0;
        ShowError(L"The document could not be rendered because of an unexpected error.");
        UpdateMenuChecks();
    }
}

void ReaderController::DisableView(const int view, const bool restoreEditorFocus) {
    if (view != MAIN_VIEW && view != SUB_VIEW) return;

    std::uintptr_t bufferId = displayedBuffers_[view];
    if (bufferId == 0) {
        const auto currentKey = CurrentKeyForView(view);
        if (currentKey) bufferId = currentKey->bufferId;
    }
    DocumentState* state = nullptr;
    if (bufferId != 0) {
        const auto found = documentStates_.find({bufferId, view});
        if (found != documentStates_.end()) state = &found->second;
    }

    if (state) state->enabled = false;
    if (hosts_[view]) hosts_[view]->Hide();
    displayedBuffers_[view] = 0;
    retainedPages_[view] = {};

    if (restoreEditorFocus) {
        ActivateViewDocument(view);
        const HWND scintilla = ScintillaForView(view);
        if (scintilla) {
            const int lineCount = std::max(1, static_cast<int>(
                SendMessageW(scintilla, SCI_GETLINECOUNT, 0, 0)));
            const int linesOnScreen = std::max(1, static_cast<int>(
                SendMessageW(scintilla, SCI_LINESONSCREEN, 0, 0)));
            const int maximumFirstLine = std::max(0, lineCount - linesOnScreen);
            const double ratio = state ? std::clamp(state->scrollRatio, 0.0, 1.0) : 0.0;
            const int targetLine = static_cast<int>(std::llround(ratio * maximumFirstLine));
            SendMessageW(scintilla, SCI_SETFIRSTVISIBLELINE, targetLine, 0);

            const std::intptr_t textLength = static_cast<std::intptr_t>(
                SendMessageW(scintilla, SCI_GETTEXTLENGTH, 0, 0));
            const std::intptr_t caret = state
                ? std::clamp(state->caretPosition, static_cast<std::intptr_t>(0), textLength)
                : 0;
            SendMessageW(scintilla, SCI_GOTOPOS, static_cast<WPARAM>(caret), 0);
            SendMessageW(scintilla, SCI_GRABFOCUS, 0, 0);
        }
    }

    UpdateMenuChecks();
}

void ReaderController::HandleReaderFocus(const int view) {
    ActivateViewDocument(view);
    UpdateMenuChecks();
}

void ReaderController::HandleScroll(const int view, const double ratio) {
    if (view != MAIN_VIEW && view != SUB_VIEW) return;
    const std::uintptr_t bufferId = displayedBuffers_[view];
    if (bufferId == 0) return;
    const auto found = documentStates_.find({bufferId, view});
    if (found != documentStates_.end()) found->second.scrollRatio = std::clamp(ratio, 0.0, 1.0);
}

void ReaderController::HandleLocalNavigation(const int view, const std::string& relativeUrl) {
    const std::uintptr_t bufferId = displayedBuffers_[view];
    if (bufferId == 0) return;

    const std::wstring sourcePath = QueryBufferPath(nppData_._nppHandle, bufferId);
    const std::filesystem::path baseDirectory = DocumentDirectory(sourcePath);
    const auto resolved = ResolveSafeRelativePath(baseDirectory, relativeUrl);
    if (!resolved) {
        ShowError(L"This local link is unavailable or falls outside the current document folder.");
        return;
    }

    std::error_code error;
    if (!std::filesystem::exists(*resolved, error) || error) {
        ShowError(L"The linked local file does not exist:\n\n" + resolved->wstring());
        return;
    }

    if (IsMarkdownExtension(*resolved)) {
        if (!SendMessageW(nppData_._nppHandle, NPPM_DOOPEN, 0,
                reinterpret_cast<LPARAM>(resolved->c_str()))) {
            ShowError(L"Notepad++ could not open the linked Markdown file:\n\n" + resolved->wstring());
        }
        return;
    }

    if (!OpenWithShell(nppData_._nppHandle, resolved->wstring())) {
        ShowError(L"Windows could not open the linked file:\n\n" + resolved->wstring());
    }
}

void ReaderController::HandleExternalNavigation(const std::wstring& uri) const {
    if (!OpenWithShell(nppData_._nppHandle, uri)) {
        ShowError(L"Windows could not open this link:\n\n" + uri);
    }
}

void ReaderController::EraseBufferState(const std::uintptr_t bufferId) {
    if (bufferId == 0) return;
    for (auto iterator = documentStates_.begin(); iterator != documentStates_.end();) {
        if (iterator->first.bufferId == bufferId) iterator = documentStates_.erase(iterator);
        else ++iterator;
    }
    for (auto& retained : retainedPages_) {
        if (retained.bufferId == bufferId) retained = {};
    }
    for (int view = MAIN_VIEW; view <= SUB_VIEW; ++view) {
        if (displayedBuffers_[view] == bufferId) {
            if (hosts_[view]) hosts_[view]->Hide();
            displayedBuffers_[view] = 0;
        }
    }
}

void ReaderController::ExportCurrentPdf() {
    EnsureReady();
    const int view = ActiveView();
    if (!hosts_[view] || !hosts_[view]->IsVisible() || !hosts_[view]->IsReady()) {
        ShowError(L"Turn on Reader Mode for the active document before exporting it to PDF.");
        return;
    }

    const std::uintptr_t bufferId = displayedBuffers_[view];
    const std::wstring sourcePath = QueryBufferPath(nppData_._nppHandle, bufferId);
    const std::wstring defaultPath = DefaultPdfName(sourcePath);

    std::array<wchar_t, 32768> fileBuffer{};
    wcsncpy_s(fileBuffer.data(), fileBuffer.size(), defaultPath.c_str(), _TRUNCATE);
    constexpr wchar_t filter[] = L"PDF document (*.pdf)\0*.pdf\0All files (*.*)\0*.*\0\0";

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = nppData_._nppHandle;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = fileBuffer.data();
    dialog.nMaxFile = static_cast<DWORD>(fileBuffer.size());
    dialog.lpstrDefExt = L"pdf";
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
    if (!GetSaveFileNameW(&dialog)) return;

    const std::filesystem::path destination(EnsurePdfExtension(fileBuffer.data()));
    const HWND owner = nppData_._nppHandle;
    const HRESULT started = hosts_[view]->ExportPdf(destination,
        [owner, destination](const HRESULT result, const bool success) {
            if (!IsWindow(owner)) return;
            if (SUCCEEDED(result) && success) {
                const std::wstring message = L"PDF exported successfully:\n\n" + destination.wstring();
                MessageBoxW(owner, message.c_str(), kPluginName, MB_OK | MB_ICONINFORMATION);
            } else {
                std::wstring message = L"The PDF could not be exported.\n\n";
                if (FAILED(result)) message += HResultMessage(result);
                else message += L"WebView2 completed the request but did not create the PDF.";
                MessageBoxW(owner, message.c_str(), kPluginName, MB_OK | MB_ICONERROR);
            }
        });
    if (FAILED(started)) {
        ShowError(L"The PDF export could not be started.\n\n" + HResultMessage(started));
    }
}

void ReaderController::IncreaseFontSize() {
    EnsureReady();
    settingsStore_.Values().fontSizePx = std::clamp(settingsStore_.Values().fontSizePx + 1, 12, 30);
    ApplyPresentationChange();
}

void ReaderController::DecreaseFontSize() {
    EnsureReady();
    settingsStore_.Values().fontSizePx = std::clamp(settingsStore_.Values().fontSizePx - 1, 12, 30);
    ApplyPresentationChange();
}

void ReaderController::ResetFontSize() {
    EnsureReady();
    settingsStore_.Values().fontSizePx = 17;
    ApplyPresentationChange();
}

void ReaderController::IncreaseContentWidth() {
    EnsureReady();
    settingsStore_.Values().contentWidthPx = std::clamp(
        settingsStore_.Values().contentWidthPx + 80, 560, 1400);
    ApplyPresentationChange();
}

void ReaderController::DecreaseContentWidth() {
    EnsureReady();
    settingsStore_.Values().contentWidthPx = std::clamp(
        settingsStore_.Values().contentWidthPx - 80, 560, 1400);
    ApplyPresentationChange();
}

void ReaderController::ResetContentWidth() {
    EnsureReady();
    settingsStore_.Values().contentWidthPx = 860;
    ApplyPresentationChange();
}

void ReaderController::ToggleReaderHeader() {
    EnsureReady();
    settingsStore_.Values().showHeader = !settingsStore_.Values().showHeader;
    ApplyPresentationChange();
}

void ReaderController::SetTheme(const ThemeMode mode) {
    EnsureReady();
    settingsStore_.Values().theme = mode;
    ApplyPresentationChange();
}

void ReaderController::ToggleRemoteImages() {
    EnsureReady();
    const bool enabling = !settingsStore_.Values().allowRemoteImages;
    if (enabling) {
        const int choice = MessageBoxW(nppData_._nppHandle,
            L"Remote images can contact third-party servers when a Markdown document is opened, "
            L"which may reveal your IP address and viewing activity.\n\nAllow remote images?",
            kPluginName, MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
        if (choice != IDYES) return;
    }
    settingsStore_.Values().allowRemoteImages = enabling;
    ApplySettingsChange();
}

void ReaderController::ApplyPresentationToVisibleViews() {
    if (!ready_ || shuttingDown_) return;
    const ReaderPresentation presentation = CurrentPresentation();
    for (int view = MAIN_VIEW; view <= SUB_VIEW; ++view) {
        if (hosts_[view] && hosts_[view]->IsVisible() && displayedBuffers_[view] != 0) {
            hosts_[view]->ApplyPresentation(presentation);
        }
    }
}

void ReaderController::ApplyPresentationChange() {
    ++presentationSerial_;
    settingsStore_.Save();
    ApplyPresentationToVisibleViews();
    UpdateMenuChecks();
}

void ReaderController::ApplySettingsChange() {
    // Remote-image policy is baked into the rendered HTML, so retained pages
    // must not be reused after this change.
    ++renderSerial_;
    settingsStore_.Save();
    RefreshAllViews();
    UpdateMenuChecks();
}

int ReaderController::CommandId(const CommandIndex index) const {
    const auto numeric = static_cast<std::size_t>(index);
    if (!functionItems_ || numeric >= static_cast<std::size_t>(functionItemCount_)) return 0;
    return functionItems_[numeric]._cmdID;
}

void ReaderController::SetMenuCheck(const CommandIndex index, const bool checked) const {
    const int commandId = CommandId(index);
    if (!nppData_._nppHandle || commandId <= 0) return;
    SendMessageW(nppData_._nppHandle, NPPM_SETMENUITEMCHECK,
        static_cast<WPARAM>(commandId), checked ? TRUE : FALSE);
}

void ReaderController::UpdateMenuChecks() {
    if (!nppData_._nppHandle || !functionItems_) return;

    bool activeEnabled = false;
    const int view = ActiveView();
    const auto key = CurrentKeyForView(view);
    if (key) {
        const auto found = documentStates_.find(*key);
        activeEnabled = found != documentStates_.end() && found->second.enabled;
    }

    SetMenuCheck(CommandIndex::ToggleReader, activeEnabled);
    SetMenuCheck(CommandIndex::ThemeFollowNotepad,
        settingsStore_.Values().theme == ThemeMode::FollowNotepad);
    SetMenuCheck(CommandIndex::ThemeLight,
        settingsStore_.Values().theme == ThemeMode::Light);
    SetMenuCheck(CommandIndex::ThemeDark,
        settingsStore_.Values().theme == ThemeMode::Dark);
    SetMenuCheck(CommandIndex::AllowRemoteImages,
        settingsStore_.Values().allowRemoteImages);
    SetMenuCheck(CommandIndex::ShowReaderHeader,
        settingsStore_.Values().showHeader);
}

void ReaderController::RegisterToolbarIcon() {
    if (!moduleInstance_ || !nppData_._nppHandle) return;
    const int commandId = CommandId(CommandIndex::ToggleReader);
    if (commandId <= 0) return;

    if (!toolbarBitmap_) {
        toolbarBitmap_ = static_cast<HBITMAP>(LoadImageW(moduleInstance_,
            MAKEINTRESOURCEW(IDB_READER_TOOLBAR), IMAGE_BITMAP, 0, 0,
            LR_LOADTRANSPARENT | LR_LOADMAP3DCOLORS));
    }
    if (!toolbarLightIcon_) {
        toolbarLightIcon_ = static_cast<HICON>(LoadImageW(moduleInstance_,
            MAKEINTRESOURCEW(IDI_READER_LIGHT), IMAGE_ICON, 16, 16, LR_LOADMAP3DCOLORS));
    }
    if (!toolbarDarkIcon_) {
        toolbarDarkIcon_ = static_cast<HICON>(LoadImageW(moduleInstance_,
            MAKEINTRESOURCEW(IDI_READER_DARK), IMAGE_ICON, 16, 16, LR_LOADMAP3DCOLORS));
    }
    if (!toolbarBitmap_ || !toolbarLightIcon_ || !toolbarDarkIcon_) return;

    toolbarIconsWithDarkMode icons;
    icons.hToolbarBmp = toolbarBitmap_;
    icons.hToolbarIcon = toolbarLightIcon_;
    icons.hToolbarIconDarkMode = toolbarDarkIcon_;
    SendMessageW(nppData_._nppHandle, NPPM_ADDTOOLBARICON_FORDARKMODE,
        static_cast<WPARAM>(commandId), reinterpret_cast<LPARAM>(&icons));
}

void ReaderController::ReleaseToolbarResources() {
    if (toolbarBitmap_) {
        DeleteObject(toolbarBitmap_);
        toolbarBitmap_ = nullptr;
    }
    if (toolbarLightIcon_) {
        DestroyIcon(toolbarLightIcon_);
        toolbarLightIcon_ = nullptr;
    }
    if (toolbarDarkIcon_) {
        DestroyIcon(toolbarDarkIcon_);
        toolbarDarkIcon_ = nullptr;
    }
}

void ReaderController::ShowAbout() const {
    std::wstring message = L"Markdown Reader " NPPMR_VERSION_STRING L"\n\n"
        L"A same-pane Markdown reading mode for Notepad++.\n\n"
        L"Included in this prototype:\n"
        L"• Main and secondary editor views\n"
        L"• GitHub-style Markdown, tables, task lists, links, and fenced code\n"
        L"• Light, dark, and Notepad++-matched themes\n"
        L"• Safe relative images and links\n"
        L"• Live refresh, scroll restoration, and PDF export\n\n"
        L"Shortcut: Ctrl+Alt+M toggles Reader Mode; Esc returns to source editing.";
    MessageBoxW(nppData_._nppHandle, message.c_str(), kPluginName, MB_OK | MB_ICONINFORMATION);
}

void ReaderController::ShowError(const std::wstring_view message, const std::wstring_view title) const {
    const std::wstring messageCopy(message);
    const std::wstring titleCopy(title);
    MessageBoxW(nppData_._nppHandle, messageCopy.c_str(), titleCopy.c_str(), MB_OK | MB_ICONERROR);
}

void ReaderController::ScheduleRefreshForBuffer(const std::uintptr_t bufferId) {
    if (bufferId == 0) return;
    for (int view = MAIN_VIEW; view <= SUB_VIEW; ++view) {
        if (hosts_[view] && hosts_[view]->IsVisible() && displayedBuffers_[view] == bufferId) {
            hosts_[view]->ScheduleRefresh();
        }
    }
}

void ReaderController::OnNotification(const SCNotification* const notification) {
    if (!notification || shuttingDown_) return;

    const UINT code = notification->nmhdr.code;
    switch (code) {
    case NPPN_READY:
        EnsureReady();
        RefreshAllViews();
        break;
    case NPPN_TBMODIFICATION:
    case NPPN_TOOLBARICONSETCHANGED:
        RegisterToolbarIcon();
        break;
    case NPPN_FILEBEFORECLOSE:
        // No proactive hiding here: this notification cannot tell which view a
        // cloned buffer is closing in, so hiding would also kill the reader in
        // a view that keeps the document open. The subsequent
        // NPPN_BUFFERACTIVATED (next document) and NPPN_FILECLOSED (buffer
        // fully removed) hide or re-render the hosts as appropriate.
        break;
    case NPPN_FILECLOSED:
        EraseBufferState(static_cast<std::uintptr_t>(notification->nmhdr.idFrom));
        RefreshAllViews();
        break;
    case NPPN_FILEOPENED:
    case NPPN_FILESAVED:
    case NPPN_FILERENAMED:
    case NPPN_FILEDELETED:
    case NPPN_BUFFERACTIVATED:
    case NPPN_LANGCHANGED:
        EnsureReady();
        // Title, file path, and language feed the rendered page, so saved or
        // renamed buffers must not fall back to their retained page. Reloaded
        // text also lands here (via NPPN_FILESAVED) because reload itself does
        // not emit a per-buffer modification stream.
        if (code != NPPN_BUFFERACTIVATED) ++renderSerial_;
        RefreshAllViews();
        break;
    case NPPN_DARKMODECHANGED:
        EnsureReady();
        if (settingsStore_.Values().theme == ThemeMode::FollowNotepad) {
            ++presentationSerial_;
            ApplyPresentationToVisibleViews();
        }
        UpdateMenuChecks();
        break;
    case NPPN_GLOBALMODIFIED:
        // Replace All intentionally does not emit the normal SCN_MODIFIED
        // stream in current Notepad++ versions. hwndFrom carries BufferID.
        ScheduleRefreshForBuffer(
            reinterpret_cast<std::uintptr_t>(notification->nmhdr.hwndFrom));
        break;
    case SCN_MODIFIED: {
        if ((notification->modificationType &
                static_cast<int>(SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)) == 0) {
            break;
        }
        int modifiedView = -1;
        if (notification->nmhdr.hwndFrom == nppData_._scintillaMainHandle) modifiedView = MAIN_VIEW;
        else if (notification->nmhdr.hwndFrom == nppData_._scintillaSecondHandle) modifiedView = SUB_VIEW;
        if (modifiedView < 0) break;

        const auto modifiedDocument = CurrentDocumentForView(modifiedView);
        if (!modifiedDocument) break;
        ScheduleRefreshForBuffer(modifiedDocument->key.bufferId);
        break;
    }
    case NPPN_SHUTDOWN:
        Shutdown();
        break;
    default:
        break;
    }
}

void ReaderController::Shutdown() {
    if (shuttingDown_) return;
    shuttingDown_ = true;

    if (ready_) settingsStore_.Save();
    for (auto& host : hosts_) {
        if (host) host->Shutdown();
        host.reset();
    }
    displayedBuffers_.fill(0);
    retainedPages_.fill(RetainedPage{});
    documentStates_.clear();
    WebViewEnvironment::Instance().Shutdown();
    ReleaseToolbarResources();

    if (comInitialized_) {
        CoUninitialize();
        comInitialized_ = false;
    }
    ready_ = false;
}

} // namespace nppmr
