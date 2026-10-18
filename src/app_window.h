#pragma once

#include <windows.h>
#include <shellapi.h>
#include <string>
#include <memory>
#include "json.hpp"

namespace K2 {

class WebViewManager;
class BridgeDispatcher;

class IWindowController {
public:
    virtual ~IWindowController() = default;
    virtual void SetWindowSize(int width, int height) = 0;
    virtual void StartWindowDrag() = 0;
    virtual void SetAlwaysOnTop(bool alwaysOnTop) = 0;
    virtual void RecenterWindow() = 0;
    virtual void HideWindow() = 0;
    virtual void ShowWindow() = 0;
    virtual void CloseApp() = 0;
};

class AppWindow : public IWindowController {
public:
    AppWindow();
    ~AppWindow() override;

    bool Create();
    void RunMessageLoop();

    void SetWindowSize(int width, int height) override;
    void StartWindowDrag() override;
    void SetAlwaysOnTop(bool alwaysOnTop) override;
    void RecenterWindow() override;
    void HideWindow() override;
    void ShowWindow() override;
    void CloseApp() override;

    HWND GetHwnd() const { return m_hWnd; }
    WebViewManager* GetWebViewManager() const { return m_webViewManager.get(); }

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void InitTrayIcon();
    void RemoveTrayIcon();
    void ShowTrayMenu();
    void HandleDropFiles(HDROP hDrop);
    void HookChildDropTargets();

    HWND m_hWnd = NULL;
    std::unique_ptr<BridgeDispatcher> m_dispatcher;
    std::unique_ptr<WebViewManager> m_webViewManager;
    class DropTarget* m_dropTarget = nullptr;
    NOTIFYICONDATAW m_nid = {};
    bool m_trayAdded = false;
    bool m_alwaysOnTop = true;
    int m_currentWidth = 290;
    int m_currentHeight = 44;
};

} // namespace K2
