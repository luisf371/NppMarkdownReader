#include "Utilities.h"

#include "PluginApi.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cwctype>
#include <limits>
#include <sstream>
#include <system_error>
#include <vector>

namespace nppmr {

std::wstring Utf8ToWide(const std::string_view value) {
    if (value.empty() || value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return {};
    const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (required <= 0) return {};
    std::wstring result(static_cast<std::size_t>(required), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        value.data(), static_cast<int>(value.size()), result.data(), required);
    return result;
}

std::string WideToUtf8(const std::wstring_view value) {
    if (value.empty() || value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return {};
    const int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) return {};
    std::string result(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        value.data(), static_cast<int>(value.size()), result.data(), required, nullptr, nullptr);
    return result;
}

std::wstring HResultMessage(const HRESULT result) {
    wchar_t* buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD count = FormatMessageW(flags, nullptr, static_cast<DWORD>(result),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    std::wstring message = count && buffer ? std::wstring(buffer, count) : L"Unknown error";
    if (buffer) LocalFree(buffer);
    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n' || message.back() == L' ')) {
        message.pop_back();
    }
    std::wostringstream stream;
    stream << message << L" (0x" << std::hex << std::uppercase << static_cast<unsigned long>(result) << L")";
    return stream.str();
}

std::string HtmlEscape(const std::string_view value) {
    std::string result;
    result.reserve(value.size() + value.size() / 8);
    for (const char ch : value) {
        switch (ch) {
        case '&': result += "&amp;"; break;
        case '<': result += "&lt;"; break;
        case '>': result += "&gt;"; break;
        case '"': result += "&quot;"; break;
        case '\'': result += "&#39;"; break;
        default: result.push_back(ch); break;
        }
    }
    return result;
}

std::string AttributeEscape(const std::string_view value) {
    return HtmlEscape(value);
}

std::string AsciiLower(const std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](const unsigned char ch) {
        return static_cast<char>(ch >= 'A' && ch <= 'Z' ? ch + ('a' - 'A') : ch);
    });
    return result;
}

bool StartsWithInsensitive(const std::string_view value, const std::string_view prefix) {
    if (value.size() < prefix.size()) return false;
    for (std::size_t i = 0; i < prefix.size(); ++i) {
        const unsigned char a = static_cast<unsigned char>(value[i]);
        const unsigned char b = static_cast<unsigned char>(prefix[i]);
        const unsigned char lowerA = a >= 'A' && a <= 'Z' ? a + ('a' - 'A') : a;
        const unsigned char lowerB = b >= 'A' && b <= 'Z' ? b + ('a' - 'A') : b;
        if (lowerA != lowerB) return false;
    }
    return true;
}

std::string PercentDecode(const std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            const auto hex = [](const char ch) -> int {
                if (ch >= '0' && ch <= '9') return ch - '0';
                if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
                if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
                return -1;
            };
            const int hi = hex(value[i + 1]);
            const int lo = hex(value[i + 2]);
            if (hi >= 0 && lo >= 0) {
                result.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        // A plus sign is literal in a URL path. Converting it to a space is
        // form-urlencoded behavior, not path decoding behavior.
        result.push_back(value[i]);
    }
    return result;
}

std::string UrlEncodePath(const std::string_view value) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(value.size() * 3 / 2);
    for (const unsigned char ch : value) {
        const bool unreserved = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.' || ch == '~';
        const bool pathSyntax = ch == '/';
        if (unreserved || pathSyntax) {
            result.push_back(static_cast<char>(ch));
        } else {
            result.push_back('%');
            result.push_back(hex[(ch >> 4) & 0xF]);
            result.push_back(hex[ch & 0xF]);
        }
    }
    return result;
}

std::string DecodeUrlEntities(const std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (std::size_t i = 0; i < value.size();) {
        if (value.substr(i, 5) == "&amp;") {
            result.push_back('&');
            i += 5;
        } else if (value.substr(i, 6) == "&quot;") {
            result.push_back('"');
            i += 6;
        } else if (i + 3 < value.size() && value[i] == '&' && value[i + 1] == '#') {
            const std::size_t end = value.find(';', i + 2);
            if (end != std::string_view::npos && end - i <= 10) {
                const bool hexValue = i + 2 < end && (value[i + 2] == 'x' || value[i + 2] == 'X');
                const std::size_t start = i + (hexValue ? 3 : 2);
                unsigned int code = 0;
                const auto parse = std::from_chars(value.data() + start, value.data() + end, code,
                    hexValue ? 16 : 10);
                if (parse.ec == std::errc{} && parse.ptr == value.data() + end && code <= 0x7F) {
                    result.push_back(static_cast<char>(code));
                    i = end + 1;
                    continue;
                }
            }
            result.push_back(value[i++]);
        } else {
            result.push_back(value[i++]);
        }
    }
    return result;
}

std::optional<std::string> NormalizeSafeRelativeUrlPath(const std::string_view value) {
    if (value.empty()) return std::nullopt;

    // Remove an actual URL query or fragment before percent decoding. This
    // keeps encoded filename characters such as %23 (a literal '#') valid.
    std::string encoded = DecodeUrlEntities(value);
    const std::size_t cut = encoded.find_first_of("?#");
    if (cut != std::string::npos) encoded.resize(cut);
    if (encoded.empty()) return std::nullopt;

    std::string decoded = encoded;
    for (int pass = 0; pass < 4; ++pass) {
        const std::string next = PercentDecode(decoded);
        if (next == decoded) break;
        decoded = next;
    }

    std::replace(decoded.begin(), decoded.end(), '\\', '/');
    if (decoded.empty() || decoded.front() == '/' || decoded.front() == '\0') return std::nullopt;
    if (decoded.size() >= 2 && std::isalpha(static_cast<unsigned char>(decoded[0])) && decoded[1] == ':') {
        return std::nullopt;
    }

    // Decode passes are bounded. Reject any remaining percent-encoded path
    // syntax instead of allowing the browser or filesystem to decode it later.
    for (std::size_t i = 0; i + 2 < decoded.size(); ++i) {
        if (decoded[i] != '%') continue;
        const auto isHex = [](const unsigned char ch) {
            return std::isxdigit(ch) != 0;
        };
        if (isHex(static_cast<unsigned char>(decoded[i + 1])) &&
            isHex(static_cast<unsigned char>(decoded[i + 2]))) {
            return std::nullopt;
        }
    }

    std::string normalized;
    normalized.reserve(decoded.size());
    std::size_t begin = 0;
    while (begin <= decoded.size()) {
        const std::size_t slash = decoded.find('/', begin);
        const std::size_t length = slash == std::string::npos ? decoded.size() - begin : slash - begin;
        const std::string_view segment(decoded.data() + begin, length);
        if (segment == ".." || segment.find('\0') != std::string_view::npos) return std::nullopt;

        for (const unsigned char ch : segment) {
            if (ch < 0x20 || ch == 0x7F) return std::nullopt;
            // These are invalid in ordinary Windows file names. In
            // particular, ':' could otherwise address an NTFS alternate
            // data stream rather than the linked file itself.
            if (ch == ':' || ch == '"' || ch == '<' || ch == '>' || ch == '|' || ch == '*' || ch == '?') {
                return std::nullopt;
            }
        }

        if (!segment.empty() && segment != ".") {
            if (segment.back() == ' ' || segment.back() == '.') return std::nullopt;
            std::string deviceCandidate = AsciiLower(segment.substr(0, segment.find('.')));
            const bool reservedDevice = deviceCandidate == "con" || deviceCandidate == "prn" ||
                deviceCandidate == "aux" || deviceCandidate == "nul" ||
                (deviceCandidate.size() == 4 &&
                    (deviceCandidate.starts_with("com") || deviceCandidate.starts_with("lpt")) &&
                    deviceCandidate[3] >= '1' && deviceCandidate[3] <= '9');
            if (reservedDevice) return std::nullopt;


            if (!normalized.empty()) normalized.push_back('/');
            normalized.append(segment);
        }
        if (slash == std::string_view::npos) break;
        begin = slash + 1;
    }

    if (normalized.empty()) return std::nullopt;
    return normalized;
}

bool IsSafeRelativeUrlPath(const std::string_view value) {
    return NormalizeSafeRelativeUrlPath(value).has_value();
}

std::optional<std::filesystem::path> ResolveSafeRelativePath(
    const std::filesystem::path& baseDirectory,
    const std::string_view urlPath) {
    if (baseDirectory.empty()) return std::nullopt;
    const auto normalizedUrl = NormalizeSafeRelativeUrlPath(urlPath);
    if (!normalizedUrl) return std::nullopt;
    std::string decoded = *normalizedUrl;
    std::replace(decoded.begin(), decoded.end(), '/', '\\');
    const std::wstring decodedWide = Utf8ToWide(decoded);
    if (!decoded.empty() && decodedWide.empty()) return std::nullopt;

    std::error_code error;
    const auto base = std::filesystem::weakly_canonical(baseDirectory, error);
    if (error) return std::nullopt;
    const auto candidate = std::filesystem::weakly_canonical(base / decodedWide, error);
    if (error) return std::nullopt;

    auto baseIt = base.begin();
    auto candidateIt = candidate.begin();
    for (; baseIt != base.end(); ++baseIt, ++candidateIt) {
        if (candidateIt == candidate.end()) return std::nullopt;
        std::wstring a = baseIt->wstring();
        std::wstring b = candidateIt->wstring();
        const auto lower = [](const wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); };
        std::transform(a.begin(), a.end(), a.begin(), lower);
        std::transform(b.begin(), b.end(), b.begin(), lower);
        if (a != b) return std::nullopt;
    }
    return candidate;
}

std::wstring QueryNppString(const HWND npp, const UINT message) {
    const auto length = static_cast<std::size_t>(SendMessageW(npp, message, 0, 0));
    if (length == 0 || length > 32767) return {};
    std::vector<wchar_t> buffer(length + 1, L'\0');
    const LRESULT result = SendMessageW(npp, message, static_cast<WPARAM>(buffer.size()),
        reinterpret_cast<LPARAM>(buffer.data()));
    if (!result) return {};
    return buffer.data();
}

std::wstring QueryBufferPath(const HWND npp, const std::uintptr_t bufferId) {
    const LRESULT lengthResult = SendMessageW(npp, NPPM_GETFULLPATHFROMBUFFERID,
        static_cast<WPARAM>(bufferId), 0);
    if (lengthResult < 0 || lengthResult > 32767) return {};
    const std::size_t length = static_cast<std::size_t>(lengthResult);
    std::vector<wchar_t> buffer(length + 1, L'\0');
    const LRESULT copied = SendMessageW(npp, NPPM_GETFULLPATHFROMBUFFERID,
        static_cast<WPARAM>(bufferId), reinterpret_cast<LPARAM>(buffer.data()));
    if (copied < 0) return {};
    return buffer.data();
}

std::wstring FileNameForDisplay(const std::wstring& fullPath) {
    if (fullPath.empty()) return L"Untitled Markdown";
    const std::filesystem::path path(fullPath);
    const std::wstring name = path.filename().wstring();
    return name.empty() ? fullPath : name;
}

std::wstring EnsurePdfExtension(std::wstring path) {
    std::filesystem::path fsPath(path);
    std::wstring extension = fsPath.extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](const wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    if (extension != L".pdf") fsPath.replace_extension(L".pdf");
    return fsPath.wstring();
}

} // namespace nppmr
