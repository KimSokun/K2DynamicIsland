#pragma once

#include <windows.h>
#include <string>
#include <memory>
#include "WebView2.h"
#include "json.hpp"

namespace K2 {

class BridgeDispatcher;

class WebViewManager {
public:
    WebViewManager(HWND hWnd, BridgeDispatcher* dispatcher);
    ~WebViewManager();

    bool Initialize(const std::wstring& htmlPath);
    void Resize(int width, int height);
    void PostJsonMessage(const std::string& jsonString);
    void PostEvent(const std::string& eventType, const nlohmann::json& payload);

    bool IsReady() const { return m_isReady; }

private:
    HWND m_hWnd;
    BridgeDispatcher* m_dispatcher;
    bool m_isReady = false;

    ICoreWebView2Environment* m_environment = nullptr;
    ICoreWebView2Controller* m_controller = nullptr;
    ICoreWebView2* m_webView = nullptr;

    EventRegistrationToken m_messageToken = {};
    EventRegistrationToken m_navigationToken = {};
    EventRegistrationToken m_newWindowToken = {};

    HMODULE m_hWebView2Loader = NULL;
};

} // namespace K2
