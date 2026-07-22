#include "MarkdownRenderer.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void Require(const bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main() {
    nppmr::MarkdownRenderer renderer;
    nppmr::RenderOptions options;
    options.title = L"Reader test";
    options.documentDirectory = LR"(C:\Docs\Notes)";

    const auto rendered = renderer.Render(
        "# Heading\n\n- [x] done\n\n| A | B |\n|---|---|\n| 1 | 2 |\n\n"
        "![local](<images/picture one.png>)\n\n"
        "![remote](https://example.com/tracker.png)\n\n"
        "[bad](javascript:alert(1))\n\n<script>alert(1)</script>", options);

    Require(rendered.success, "Markdown should render");
    Require(rendered.html.find("<h1>") != std::string::npos, "heading output missing");
    Require(rendered.html.find("<table>") != std::string::npos, "table output missing");
    Require(rendered.html.find("https://document.local/images/picture%20one.png") != std::string::npos,
        "relative image was not rewritten");
    Require(rendered.html.find("https://reader.local/blocked-image.svg") != std::string::npos,
        "remote image was not blocked");
    Require(rendered.html.find("href=\"#blocked-link\"") != std::string::npos,
        "javascript link was not blocked");
    Require(rendered.html.find("<script>alert(1)</script>") == std::string::npos,
        "raw HTML script survived rendering");
    Require(rendered.html.find("&lt;script&gt;") != std::string::npos,
        "disabled raw HTML should remain visible as escaped text");

    options.allowRemoteImages = true;
    const auto remoteAllowed = renderer.Render("![remote](https://example.com/image.png)", options);
    Require(remoteAllowed.html.find("https://example.com/image.png") != std::string::npos,
        "allowed remote image was rewritten");

    const std::string traversal = nppmr::MarkdownRenderer::RewriteGeneratedUrls(
        "<img src=\"%2e%2e/secret.png\"><a href=\"../secret.md\">x</a>"
        "<img src=\"%252e%252e/secret.png\">", options);
    Require(traversal.find("document.local") == std::string::npos,
        "path traversal was accepted");

    const std::string plusName = nppmr::MarkdownRenderer::RewriteGeneratedUrls(
        "<img src=\"images/a+b.png\">", options);
    Require(plusName.find("images/a%2Bb.png") != std::string::npos,
        "literal plus sign was treated as form encoding");

    const std::string encodedHash = nppmr::MarkdownRenderer::RewriteGeneratedUrls(
        "<img src=\"images/chapter%231.png?size=large\">", options);
    Require(encodedHash.find("images/chapter%231.png") != std::string::npos,
        "encoded filename hash was treated as a URL fragment");
    Require(encodedHash.find("size=large") == std::string::npos,
        "local resource query was retained");

    const std::string encodedAmpersand = nppmr::MarkdownRenderer::RewriteGeneratedUrls(
        "<a href=\"https://example.com/?a=1&amp;b=2\">x</a>", options);
    Require(encodedAmpersand.find("&amp;amp;") == std::string::npos,
        "existing URL entity was double encoded");
    Require(encodedAmpersand.find("a=1&amp;b=2") != std::string::npos,
        "safe external URL was not preserved");

    const std::string driveAndAds = nppmr::MarkdownRenderer::RewriteGeneratedUrls(
        "<img src=\"C:%5cWindows%5csecret.png\"><img src=\"image.png:stream\">", options);
    Require(driveAndAds.find("document.local") == std::string::npos,
        "absolute path or alternate data stream was accepted");

    const std::string devices = nppmr::MarkdownRenderer::RewriteGeneratedUrls(
        "<img src=\"NUL.png\"><img src=\"images/COM1.txt\">"
        "<img src=\"images/secret.png.%20\">", options);
    Require(devices.find("document.local") == std::string::npos,
        "Windows device name or trailing-dot/space path was accepted");

    std::cout << "All renderer tests passed.\n";
    return 0;
}
