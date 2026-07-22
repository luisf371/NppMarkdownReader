#pragma once

#include <windows.h>
#include <wrl.h>
#include <WebView2.h>

#include <filesystem>
#include <functional>
#include <vector>

namespace nppmr {

class WebViewEnvironment final {
public:
    using Completion = std::function<void(HRESULT, Microsoft::WRL::ComPtr<ICoreWebView2Environment>)>;

    static WebViewEnvironment& Instance();

    void Initialize(const std::filesystem::path& userDataFolder, Completion completion);
    void Shutdown();

private:
    enum class State { NotStarted, Starting, Ready, Failed, Stopped };

    State state_ = State::NotStarted;
    HRESULT result_ = E_PENDING;
    Microsoft::WRL::ComPtr<ICoreWebView2Environment> environment_;
    std::vector<Completion> pending_;
};

} // namespace nppmr
