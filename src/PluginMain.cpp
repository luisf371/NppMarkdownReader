#include "PluginApi.h"
#include "PluginGlobals.h"
#include "ReaderController.h"

#include <array>
#include <cwchar>

namespace {

using nppmr::CommandIndex;
using nppmr::ReaderController;

constexpr wchar_t kPluginName[] = L"Markdown Reader";
std::array<FuncItem, nppmr::kCommandCount> g_functionItems{};
ShortcutKey g_toggleShortcut{true, true, false, static_cast<UCHAR>('M')};
bool g_commandsInitialized = false;

void __cdecl ToggleReader() { ReaderController::Instance().ToggleReader(); }
void __cdecl ExportPdf() { ReaderController::Instance().ExportCurrentPdf(); }
void __cdecl IncreaseFont() { ReaderController::Instance().IncreaseFontSize(); }
void __cdecl DecreaseFont() { ReaderController::Instance().DecreaseFontSize(); }
void __cdecl ResetFont() { ReaderController::Instance().ResetFontSize(); }
void __cdecl IncreaseWidth() { ReaderController::Instance().IncreaseContentWidth(); }
void __cdecl DecreaseWidth() { ReaderController::Instance().DecreaseContentWidth(); }
void __cdecl ResetWidth() { ReaderController::Instance().ResetContentWidth(); }
void __cdecl AlignLeft() { ReaderController::Instance().ToggleAlignLeft(); }
void __cdecl FillWidth() { ReaderController::Instance().ToggleFillWidth(); }
void __cdecl ToggleHeader() { ReaderController::Instance().ToggleReaderHeader(); }
void __cdecl ToggleToc() { ReaderController::Instance().ToggleTableOfContents(); }
void __cdecl ThemeFollow() { ReaderController::Instance().SetTheme(nppmr::ThemeMode::FollowNotepad); }
void __cdecl ThemeLight() { ReaderController::Instance().SetTheme(nppmr::ThemeMode::Light); }
void __cdecl ThemeDark() { ReaderController::Instance().SetTheme(nppmr::ThemeMode::Dark); }
void __cdecl RemoteImages() { ReaderController::Instance().ToggleRemoteImages(); }
void __cdecl About() { ReaderController::Instance().ShowAbout(); }

void SetCommand(const CommandIndex index, const wchar_t* const name,
    const PFUNCPLUGINCMD function, ShortcutKey* const shortcut = nullptr,
    const bool initiallyChecked = false) {
    FuncItem& item = g_functionItems[static_cast<std::size_t>(index)];
    wcsncpy_s(item._itemName, name, _TRUNCATE);
    item._pFunc = function;
    item._init2Check = initiallyChecked;
    item._pShKey = shortcut;
}

void InitializeCommands() {
    if (g_commandsInitialized) return;
    g_commandsInitialized = true;

    SetCommand(CommandIndex::ToggleReader, L"Toggle Reader Mode", ToggleReader, &g_toggleShortcut);
    SetCommand(CommandIndex::ExportPdf, L"Export Reader to PDF…", ExportPdf);
    SetCommand(CommandIndex::IncreaseFont, L"Increase Reader Text Size", IncreaseFont);
    SetCommand(CommandIndex::DecreaseFont, L"Decrease Reader Text Size", DecreaseFont);
    SetCommand(CommandIndex::ResetFont, L"Reset Reader Text Size", ResetFont);
    SetCommand(CommandIndex::IncreaseContentWidth, L"Increase Reader Text Width", IncreaseWidth);
    SetCommand(CommandIndex::DecreaseContentWidth, L"Decrease Reader Text Width", DecreaseWidth);
    SetCommand(CommandIndex::ResetContentWidth, L"Reset Reader Text Width", ResetWidth);
    SetCommand(CommandIndex::AlignContentLeft, L"Align Reader Content Left", AlignLeft);
    SetCommand(CommandIndex::FillContentWidth, L"Fit Reader Content to Window", FillWidth);
    SetCommand(CommandIndex::ShowReaderHeader, L"Show Reader Header", ToggleHeader, nullptr, true);
    SetCommand(CommandIndex::ShowTableOfContents, L"Show Table of Contents", ToggleToc, nullptr, true);
    SetCommand(CommandIndex::ThemeFollowNotepad, L"Theme: Follow Notepad++", ThemeFollow, nullptr, true);
    SetCommand(CommandIndex::ThemeLight, L"Theme: Light", ThemeLight);
    SetCommand(CommandIndex::ThemeDark, L"Theme: Dark", ThemeDark);
    SetCommand(CommandIndex::AllowRemoteImages, L"Allow Remote Images", RemoteImages);
    SetCommand(CommandIndex::About, L"About Markdown Reader", About);
}

} // namespace

extern "C" __declspec(dllexport) void setInfo(const NppData nppData) {
    InitializeCommands();
    ReaderController::Instance().SetPluginContext(nppData, g_moduleInstance);
    ReaderController::Instance().SetFunctionItems(g_functionItems.data(),
        static_cast<int>(g_functionItems.size()));
}

extern "C" __declspec(dllexport) const wchar_t* getName() {
    return kPluginName;
}

extern "C" __declspec(dllexport) FuncItem* getFuncsArray(int* const count) {
    InitializeCommands();
    if (count) *count = static_cast<int>(g_functionItems.size());
    ReaderController::Instance().SetFunctionItems(g_functionItems.data(),
        static_cast<int>(g_functionItems.size()));
    return g_functionItems.data();
}

extern "C" __declspec(dllexport) void beNotified(SCNotification* const notification) {
    ReaderController::Instance().OnNotification(notification);
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT, WPARAM, LPARAM) {
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL isUnicode() {
    return TRUE;
}
