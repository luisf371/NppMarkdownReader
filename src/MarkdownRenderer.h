#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace nppmr {

struct RenderOptions {
    std::wstring title;
    std::filesystem::path documentDirectory;
    bool darkTheme = false;
    bool allowRemoteImages = false;
    int fontSizePx = 17;
    int contentWidthPx = 860;
    bool showHeader = true;
    bool showToc = true;
    bool alignLeft = false;
    bool fillWidth = false;
    double lineHeight = 1.72;
    double initialScrollRatio = 0.0;
};

struct RenderResult {
    bool success = false;
    std::string html;
    std::wstring error;
};

class MarkdownRenderer {
public:
    RenderResult Render(std::string_view markdown, const RenderOptions& options) const;

    // Public for focused unit tests and future render backends.
    static std::string RewriteGeneratedUrls(std::string html, const RenderOptions& options);
};

} // namespace nppmr
