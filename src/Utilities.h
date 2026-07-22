#pragma once

#include <windows.h>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace nppmr {

std::wstring Utf8ToWide(std::string_view value);
std::string WideToUtf8(std::wstring_view value);
std::wstring HResultMessage(HRESULT result);
std::string HtmlEscape(std::string_view value);
std::string AttributeEscape(std::string_view value);
std::string AsciiLower(std::string_view value);
bool StartsWithInsensitive(std::string_view value, std::string_view prefix);
std::string PercentDecode(std::string_view value);
std::string UrlEncodePath(std::string_view value);
std::string DecodeUrlEntities(std::string_view value);
std::optional<std::string> NormalizeSafeRelativeUrlPath(std::string_view value);
bool IsSafeRelativeUrlPath(std::string_view value);
std::optional<std::filesystem::path> ResolveSafeRelativePath(
    const std::filesystem::path& baseDirectory,
    std::string_view urlPath);
std::wstring QueryNppString(HWND npp, UINT message);
std::wstring QueryBufferPath(HWND npp, std::uintptr_t bufferId);
std::wstring FileNameForDisplay(const std::wstring& fullPath);
std::wstring EnsurePdfExtension(std::wstring path);

} // namespace nppmr
