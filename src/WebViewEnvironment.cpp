#include "WebViewEnvironment.h"

#include <WebView2EnvironmentOptions.h>

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace nppmr {

WebViewEnvironment& WebViewEnvironment::Instance() {
    static WebViewEnvironment instance;
    return instance;
}

void WebViewEnvironment::Initialize(const std::filesystem::path& userDataFolder, Completion completion) {
    if (state_ == State::Ready) {
        completion(S_OK, environment_);
        return;
    }
    if (state_ == State::Failed || state_ == State::Stopped) {
        completion(result_, nullptr);
        return;
    }

    pending_.push_back(std::move(completion));
    if (state_ == State::Starting) return;
    state_ = State::Starting;

    std::error_code error;
    std::filesystem::create_directories(userDataFolder, error);
    const std::wstring folder = userDataFolder.wstring();

    const HRESULT started = CreateCoreWebView2EnvironmentWithOptions(
        nullptr,
        folder.empty() ? nullptr : folder.c_str(),
        nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](const HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                if (state_ == State::Stopped) {
                    result_ = E_ABORT;
                    environment_.Reset();
                    return S_OK;
                }
                result_ = result;
                if (SUCCEEDED(result) && environment) {
                    environment_ = environment;
                    state_ = State::Ready;
                } else {
                    state_ = State::Failed;
                }

                auto callbacks = std::move(pending_);
                pending_.clear();
                for (auto& callback : callbacks) callback(result_, environment_);
                return S_OK;
            }).Get());

    if (FAILED(started)) {
        result_ = started;
        state_ = State::Failed;
        auto callbacks = std::move(pending_);
        pending_.clear();
        for (auto& callback : callbacks) callback(result_, nullptr);
    }
}

void WebViewEnvironment::Shutdown() {
    result_ = E_ABORT;
    state_ = State::Stopped;
    environment_.Reset();
    auto callbacks = std::move(pending_);
    pending_.clear();
    for (auto& callback : callbacks) callback(E_ABORT, nullptr);
}

} // namespace nppmr
