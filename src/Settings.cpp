#include "Settings.h"

#include <windows.h>
#include <algorithm>
#include <array>
#include <cwchar>
#include <cmath>
#include <cwctype>
#include <string>

namespace nppmr {

void SettingsStore::SetConfigDirectory(const std::filesystem::path& directory) {
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    iniPath_ = directory / L"NppMarkdownReader.ini";
}

void SettingsStore::Load() {
    if (iniPath_.empty()) return;
    std::array<wchar_t, 64> theme{};
    GetPrivateProfileStringW(L"Reader", L"Theme", L"follow", theme.data(),
        static_cast<DWORD>(theme.size()), iniPath_.c_str());
    const std::wstring themeValue(theme.data());
    if (_wcsicmp(themeValue.c_str(), L"dark") == 0) values_.theme = ThemeMode::Dark;
    else if (_wcsicmp(themeValue.c_str(), L"light") == 0) values_.theme = ThemeMode::Light;
    else values_.theme = ThemeMode::FollowNotepad;

    values_.fontSizePx = static_cast<int>(std::clamp(
        GetPrivateProfileIntW(L"Reader", L"FontSize", 17, iniPath_.c_str()), 12u, 30u));
    values_.contentWidthPx = static_cast<int>(std::clamp(
        GetPrivateProfileIntW(L"Reader", L"ContentWidth", 860, iniPath_.c_str()), 560u, 1400u));
    values_.allowRemoteImages = GetPrivateProfileIntW(L"Reader", L"AllowRemoteImages", 0, iniPath_.c_str()) != 0;

    std::array<wchar_t, 32> lineHeight{};
    GetPrivateProfileStringW(L"Reader", L"LineHeight", L"1.72", lineHeight.data(),
        static_cast<DWORD>(lineHeight.size()), iniPath_.c_str());
    wchar_t* end = nullptr;
    const double parsed = std::wcstod(lineHeight.data(), &end);
    while (end && *end != L'\0' && std::iswspace(*end)) ++end;
    if (end != lineHeight.data() && end && *end == L'\0' && std::isfinite(parsed)) {
        values_.lineHeight = std::clamp(parsed, 1.2, 2.2);
    }
}

void SettingsStore::Save() const {
    if (iniPath_.empty()) return;
    const wchar_t* theme = L"follow";
    if (values_.theme == ThemeMode::Light) theme = L"light";
    else if (values_.theme == ThemeMode::Dark) theme = L"dark";

    WritePrivateProfileStringW(L"Reader", L"Theme", theme, iniPath_.c_str());
    WritePrivateProfileStringW(L"Reader", L"FontSize", std::to_wstring(values_.fontSizePx).c_str(), iniPath_.c_str());
    WritePrivateProfileStringW(L"Reader", L"ContentWidth", std::to_wstring(values_.contentWidthPx).c_str(), iniPath_.c_str());
    WritePrivateProfileStringW(L"Reader", L"AllowRemoteImages", values_.allowRemoteImages ? L"1" : L"0", iniPath_.c_str());
    wchar_t lineHeight[32]{};
    swprintf_s(lineHeight, L"%.2f", values_.lineHeight);
    WritePrivateProfileStringW(L"Reader", L"LineHeight", lineHeight, iniPath_.c_str());
}

} // namespace nppmr
