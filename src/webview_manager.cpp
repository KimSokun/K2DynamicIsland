#include "webview_manager.h"
#include "com_callbacks.h"
#include "bridge_dispatcher.h"
#include "storage_manager.h"
#include "logger.h"
#include <shlwapi.h>
#include <iostream>
#include <vector>

typedef HRESULT (STDAPICALLTYPE *CreateEnvWithOptionsFn)(
    PCWSTR browserExecutableFolder,
    PCWSTR userDataFolder,
    ICoreWebView2EnvironmentOptions* environmentOptions,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* environment_created_handler);

namespace K2 {

static std::wstring Utf8ToUtf16(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, NULL, 0);
    if (len <= 1) return L"";
    std::vector<wchar_t> buf(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, buf.data(), len);
    return std::wstring(buf.data());
}

static std::string Utf16ToUtf8(const std::wstring& utf16) {
    if (utf16.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, NULL, 0, NULL, NULL);
    if (len <= 1) return "";
    std::vector<char> buf(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, buf.data(), len, NULL, NULL);
    return std::string(buf.data());
}

WebViewManager::WebViewManager(HWND hWnd, BridgeDispatcher* dispatcher)
    : m_hWnd(hWnd), m_dispatcher(dispatcher) {
}

WebViewManager::~WebViewManager() {
    if (m_webView) {
        if (m_messageToken.value != 0) m_webView->remove_WebMessageReceived(m_messageToken);
        if (m_navigationToken.value != 0) m_webView->remove_NavigationStarting(m_navigationToken);
        if (m_newWindowToken.value != 0) m_webView->remove_NewWindowRequested(m_newWindowToken);
        m_webView->Release();
        m_webView = nullptr;
    }
    if (m_controller) {
        m_controller->Close();
        m_controller->Release();
        m_controller = nullptr;
    }
    if (m_environment) {
        m_environment->Release();
        m_environment = nullptr;
    }
    if (m_hWebView2Loader) {
        FreeLibrary(m_hWebView2Loader);
        m_hWebView2Loader = NULL;
    }
}

bool WebViewManager::Initialize(const std::wstring& htmlPath) {
    // Locate WebView2Loader.dll
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);

    std::wstring loaderPath = std::wstring(exePath) + L"\\WebView2Loader.dll";
    m_hWebView2Loader = LoadLibraryW(loaderPath.c_str());

    if (!m_hWebView2Loader) {
        // Fallback to current working directory or system PATH
        m_hWebView2Loader = LoadLibraryW(L"WebView2Loader.dll");
    }

    if (!m_hWebView2Loader) {
        MessageBoxW(m_hWnd,
            L"WebView2Loader.dll could not be loaded.\n\nPlease ensure WebView2Loader.dll is in the application folder.",
            L"K2 Dynamic Island - Initialization Error",
            MB_ICONERROR | MB_OK);
        return false;
    }

    auto pfnCreateEnv = (CreateEnvWithOptionsFn)GetProcAddress(m_hWebView2Loader, "CreateCoreWebView2EnvironmentWithOptions");
    if (!pfnCreateEnv) {
        MessageBoxW(m_hWnd,
            L"Could not locate CreateCoreWebView2EnvironmentWithOptions in WebView2Loader.dll.",
            L"K2 Dynamic Island - Initialization Error",
            MB_ICONERROR | MB_OK);
        return false;
    }

    std::wstring userDataDir = StorageManager::Instance().GetAppDataDirectory() + L"\\WebView2Cache";
    CreateDirectoryW(userDataDir.c_str(), NULL);

    // Enforce high-performance GPU hardware acceleration and prevent background occlusion throttling
    SetEnvironmentVariableW(L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS",
        L"--enable-gpu-rasterization --enable-zero-copy --disable-features=CalculateNativeWinOcclusion,BackgroundResourceBudget --disable-backgrounding-occluded-windows");

    HRESULT hr = pfnCreateEnv(
        nullptr,
        userDataDir.c_str(),
        nullptr,
        new EnvironmentCompletedHandler([this, htmlPath](HRESULT hrResult, ICoreWebView2Environment* env) -> HRESULT {
            K2::Log("EnvironmentCompletedHandler invoked with hrResult: " + std::to_string(hrResult));
            if (FAILED(hrResult) || !env) {
                K2::Log("EnvironmentCompletedHandler: env is null or failed");
                MessageBoxW(m_hWnd,
                    L"Failed to initialize Edge WebView2 Environment.\n\nPlease install the Microsoft Edge WebView2 Runtime.",
                    L"K2 Dynamic Island - Runtime Missing",
                    MB_ICONERROR | MB_OK);
                return hrResult;
            }

            m_environment = env;
            m_environment->AddRef();
            K2::Log("Creating CoreWebView2Controller...");

            HRESULT ctrlCreateHr = m_environment->CreateCoreWebView2Controller(m_hWnd, new ControllerCompletedHandler(
                [this, htmlPath](HRESULT ctrlHr, ICoreWebView2Controller* controller) -> HRESULT {
                    K2::Log("ControllerCompletedHandler invoked with ctrlHr: " + std::to_string(ctrlHr));
                    if (FAILED(ctrlHr) || !controller) {
                        K2::Log("ControllerCompletedHandler failed or controller is null");
                        return ctrlHr;
                    }

                    m_controller = controller;
                    m_controller->AddRef();

                    // Transparent background
                    ICoreWebView2Controller2* controller2 = nullptr;
                    if (SUCCEEDED(m_controller->QueryInterface(IID_ICoreWebView2Controller2, (void**)&controller2)) && controller2) {
                        COREWEBVIEW2_COLOR transparent = { 0, 0, 0, 0 };
                        controller2->put_DefaultBackgroundColor(transparent);
                        controller2->Release();
                        K2::Log("Transparent background configured");
                    }

                    // Disable WebView2 default external drop so native OLE IDropTarget receives file drops
                    ICoreWebView2Controller4* controller4 = nullptr;
                    if (SUCCEEDED(m_controller->QueryInterface(IID_ICoreWebView2Controller4, (void**)&controller4)) && controller4) {
                        controller4->put_AllowExternalDrop(FALSE);
                        controller4->Release();
                        K2::Log("controller4->put_AllowExternalDrop(FALSE) applied");
                    }

                    // Sizing
                    RECT bounds;
                    GetClientRect(m_hWnd, &bounds);
                    m_controller->put_Bounds(bounds);
                    m_controller->put_IsVisible(TRUE);
                    K2::Log("Controller bounds set to: " + std::to_string(bounds.right) + "x" + std::to_string(bounds.bottom));

                    m_controller->get_CoreWebView2(&m_webView);
                    if (!m_webView) {
                        K2::Log("get_CoreWebView2 failed");
                        return E_FAIL;
                    }

                    // Settings
                    ICoreWebView2Settings* settings = nullptr;
                    if (SUCCEEDED(m_webView->get_Settings(&settings)) && settings) {
                        settings->put_AreDevToolsEnabled(FALSE);
                        settings->put_AreDefaultContextMenusEnabled(FALSE);
                        settings->put_IsStatusBarEnabled(FALSE);
                        settings->put_IsZoomControlEnabled(FALSE);
                        settings->Release();
                    }

                    // Web message handler
                    m_webView->add_WebMessageReceived(new WebMessageReceivedHandler(
                        [this](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                            LPWSTR rawMsg = nullptr;
                            HRESULT hrMsg = args->TryGetWebMessageAsString(&rawMsg);
                            if (FAILED(hrMsg) || !rawMsg) {
                                args->get_WebMessageAsJson(&rawMsg);
                            }

                            if (rawMsg) {
                                std::string utf8Msg = Utf16ToUtf8(rawMsg);
                                CoTaskMemFree(rawMsg);

                                if (m_dispatcher) {
                                    std::string resp = m_dispatcher->DispatchMessage(utf8Msg);
                                    PostJsonMessage(resp);
                                }
                            }
                            return S_OK;
                        }), &m_messageToken);

                    // Convert htmlPath to valid file:/// URI
                    std::wstring navUri = htmlPath;
                    for (auto& ch : navUri) {
                        if (ch == L'\\') ch = L'/';
                    }
                    if (navUri.find(L"file:///") != 0) {
                        navUri = L"file:///" + navUri;
                    }
                    K2::Log("Navigating to URI: " + Utf16ToUtf8(navUri));

                    // Restrict navigation to local files only
                    m_webView->add_NavigationStarting(new NavigationStartingHandler(
                        [](ICoreWebView2* sender, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                            LPWSTR uri = nullptr;
                            if (SUCCEEDED(args->get_Uri(&uri)) && uri) {
                                std::wstring uriStr(uri);
                                CoTaskMemFree(uri);
                                // Cancel external http/https navigation
                                if (uriStr.find(L"http://") == 0 || uriStr.find(L"https://") == 0) {
                                    args->put_Cancel(TRUE);
                                }
                            }
                            return S_OK;
                        }), &m_navigationToken);

                    // Block popup windows
                    m_webView->add_NewWindowRequested(new NewWindowRequestedHandler(
                        [](ICoreWebView2* sender, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                            args->put_Handled(TRUE);
                            return S_OK;
                        }), &m_newWindowToken);

                    // Navigate to HTML file
                    HRESULT navHr = m_webView->Navigate(navUri.c_str());
                    K2::Log("Navigate returned hr: " + std::to_string(navHr));
                    m_isReady = true;

                    return S_OK;
                }));
            K2::Log("CreateCoreWebView2Controller returned: " + std::to_string(ctrlCreateHr));

            return S_OK;
        }));

    return SUCCEEDED(hr);
}

void WebViewManager::Resize(int width, int height) {
    if (m_controller) {
        RECT bounds = { 0, 0, width, height };
        m_controller->put_Bounds(bounds);
    }
}

void WebViewManager::PostJsonMessage(const std::string& jsonString) {
    if (m_webView) {
        std::wstring wMsg = Utf8ToUtf16(jsonString);
        m_webView->PostWebMessageAsJson(wMsg.c_str());
    }
}

void WebViewManager::PostEvent(const std::string& eventType, const nlohmann::json& payload) {
    nlohmann::json evt;
    evt["event"] = eventType;
    evt["data"] = payload;
    PostJsonMessage(evt.dump());
}

} // namespace K2
