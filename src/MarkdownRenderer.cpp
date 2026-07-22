#include "MarkdownRenderer.h"

#include "Utilities.h"

#include <md4c-html.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace nppmr {
namespace {

void AppendHtml(const MD_CHAR* text, const MD_SIZE size, void* userdata) {
    auto* output = static_cast<std::string*>(userdata);
    output->append(text, static_cast<std::size_t>(size));
}

std::string TrimAscii(std::string value) {
    const auto notSpace = [](const unsigned char ch) { return ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n'; };
    const auto begin = std::find_if(value.begin(), value.end(), notSpace);
    const auto end = std::find_if(value.rbegin(), value.rend(), notSpace).base();
    if (begin >= end) return {};
    return std::string(begin, end);
}

bool IsAllowedDataImage(const std::string_view value) {
    return StartsWithInsensitive(value, "data:image/png;base64,") ||
        StartsWithInsensitive(value, "data:image/jpeg;base64,") ||
        StartsWithInsensitive(value, "data:image/jpg;base64,") ||
        StartsWithInsensitive(value, "data:image/gif;base64,") ||
        StartsWithInsensitive(value, "data:image/webp;base64,");
}

std::string RewriteUrl(const std::string_view original, const bool image, const RenderOptions& options) {
    const std::string decoded = TrimAscii(DecodeUrlEntities(original));
    const std::string lower = AsciiLower(decoded);
    if (decoded.empty()) return image ? "https://reader.local/blocked-image.svg" : "#";

    if (!image && decoded.front() == '#') return decoded;

    const bool http = StartsWithInsensitive(lower, "http://");
    const bool https = StartsWithInsensitive(lower, "https://");
    if (http || https) {
        if (image && !options.allowRemoteImages) return "https://reader.local/blocked-image.svg";
        return decoded;
    }

    if (!image && (StartsWithInsensitive(lower, "mailto:") || StartsWithInsensitive(lower, "tel:"))) {
        return decoded;
    }

    if (image && IsAllowedDataImage(decoded)) return decoded;

    // Any explicit or obfuscated scheme that reached this point is blocked.
    const std::size_t colon = lower.find(':');
    const std::size_t firstSeparator = lower.find_first_of("/?#");
    if (colon != std::string::npos && (firstSeparator == std::string::npos || colon < firstSeparator)) {
        return image ? "https://reader.local/blocked-image.svg" : "#blocked-link";
    }

    if (options.documentDirectory.empty()) {
        return image ? "https://reader.local/blocked-image.svg" : "#unavailable-local-link";
    }

    const auto normalized = NormalizeSafeRelativeUrlPath(decoded);
    if (!normalized) return image ? "https://reader.local/blocked-image.svg" : "#unavailable-local-link";
    return "https://document.local/" + UrlEncodePath(*normalized);
}

void RewriteAttribute(std::string& html, const std::string_view attribute, const bool image,
    const RenderOptions& options) {
    const std::string marker = std::string(attribute) + "=\"";
    std::size_t position = 0;
    while ((position = html.find(marker, position)) != std::string::npos) {
        const std::size_t valueBegin = position + marker.size();
        const std::size_t valueEnd = html.find('"', valueBegin);
        if (valueEnd == std::string::npos) break;
        const std::string original = html.substr(valueBegin, valueEnd - valueBegin);
        const std::string rewritten = AttributeEscape(RewriteUrl(original, image, options));
        html.replace(valueBegin, valueEnd - valueBegin, rewritten);
        position = valueBegin + rewritten.size() + 1;
    }
}

} // namespace

std::string MarkdownRenderer::RewriteGeneratedUrls(std::string html, const RenderOptions& options) {
    RewriteAttribute(html, "href", false, options);
    RewriteAttribute(html, "src", true, options);
    return html;
}

RenderResult MarkdownRenderer::Render(const std::string_view markdown, const RenderOptions& options) const {
    RenderResult result;
    if (markdown.size() > std::numeric_limits<MD_SIZE>::max()) {
        result.error = L"The document is too large for the Markdown renderer.";
        return result;
    }

    std::string fragment;
    fragment.reserve(markdown.size() + markdown.size() / 3);
    unsigned parserFlags = MD_DIALECT_GITHUB | MD_FLAG_NOHTML;
#ifdef MD_FLAG_HIGHLIGHT
    parserFlags |= MD_FLAG_HIGHLIGHT;
#endif
#ifdef MD_FLAG_SUPERSCRIPTS
    parserFlags |= MD_FLAG_SUPERSCRIPTS;
#endif
#ifdef MD_FLAG_SUBSCRIPTS
    parserFlags |= MD_FLAG_SUBSCRIPTS;
#endif
#ifdef MD_FLAG_SPOILERS
    parserFlags |= MD_FLAG_SPOILERS;
#endif
#ifdef MD_FLAG_FOOTNOTES
    parserFlags |= MD_FLAG_FOOTNOTES;
#endif
    constexpr unsigned htmlFlags = MD_HTML_FLAG_SKIP_UTF8_BOM;
    const int renderStatus = md_html(markdown.data(), static_cast<MD_SIZE>(markdown.size()),
        AppendHtml, &fragment, parserFlags, htmlFlags);
    if (renderStatus != 0) {
        result.error = L"MD4C could not parse this document.";
        return result;
    }

    fragment = RewriteGeneratedUrls(std::move(fragment), options);

    const std::string title = HtmlEscape(WideToUtf8(options.title.empty() ? L"Markdown Reader" : options.title));
    const double scroll = std::clamp(std::isfinite(options.initialScrollRatio) ? options.initialScrollRatio : 0.0, 0.0, 1.0);
    const double lineHeight = std::clamp(options.lineHeight, 1.2, 2.2);
    const int fontSize = std::clamp(options.fontSizePx, 12, 30);
    const int contentWidth = std::clamp(options.contentWidthPx, 560, 1400);

    std::ostringstream page;
    page.imbue(std::locale::classic());
    page << "<!doctype html>\n"
        << "<html lang=\"en\" data-theme=\"" << (options.darkTheme ? "dark" : "light") << "\">\n"
        << "<head>\n"
        << "<meta charset=\"utf-8\">\n"
        << "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
        << "<meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; "
        << "style-src https://reader.local 'nonce-nppmr-style'; script-src https://reader.local; "
        << "img-src https://reader.local https://document.local data:"
        << (options.allowRemoteImages ? " https: http:" : "")
        << "; connect-src 'none'; media-src 'none'; object-src 'none'; frame-src 'none'; "
        << "font-src 'none'; base-uri 'none'; form-action 'none'\">\n"
        << "<title>" << title << "</title>\n"
        << "<link rel=\"stylesheet\" href=\"https://reader.local/reader.css\">\n"
        << "<style nonce=\"nppmr-style\">:root{--font-size:" << fontSize << "px;--line-height:"
        << std::fixed << std::setprecision(2) << lineHeight << ";--content-width:"
        << contentWidth << "px}</style>\n"
        << "<script defer src=\"https://reader.local/reader.js\"></script>\n"
        << "</head>\n"
        << "<body data-scroll=\"" << std::fixed << std::setprecision(6) << scroll << "\">\n"
        << "<div id=\"reading-progress\" aria-hidden=\"true\"></div>\n"
        << "<header class=\"reader-toolbar\"><div class=\"reader-title\">" << title
        << "</div><div class=\"reader-hint\"><kbd>Esc</kbd> edit source</div></header>\n"
        << "<div class=\"reader-layout\">\n"
        << "<main id=\"reader-content\" class=\"markdown-body\">\n"
        << fragment
        << "\n</main><nav id=\"reader-toc\" aria-label=\"Table of contents\"></nav>\n"
        << "</div></body></html>";

    result.success = true;
    result.html = page.str();
    return result;
}

} // namespace nppmr
