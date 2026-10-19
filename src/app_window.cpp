#include "app_window.h"
#include "bridge_dispatcher.h"
#include "webview_manager.h"
#include "storage_manager.h"
#include "drop_target.h"
#include "update_manager.h"
#include "logger.h"
#include <shlwapi.h>
#include <dwmapi.h>
#include <iostream>
#include <vector>

#define WM_TRAYICON (WM_APP + 1)
#define WM_APP_DISPATCH_EVENT (WM_APP + 2)
#define ID_TRAY_OPEN 1001
#define ID_TRAY_SETTINGS 1002
#define ID_TRAY_EXIT 1003
#define IDT_DROPTARGET_HOOK 2001
#define IDT_UPDATE_CHECK 2002
#define IDT_UPDATE_PERIODIC 2003

namespace K2 {

static std::string Utf16ToUtf8(const std::wstring& utf16) {
    if (utf16.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, NULL, 0, NULL, NULL);
    if (len <= 1) return "";
    std::vector<char> buf(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, buf.data(), len, NULL, NULL);
    return std::string(buf.data());
}

AppWindow::AppWindow() {
    m_dispatcher = std::make_unique<BridgeDispatcher>(this);
}

AppWindow::~AppWindow() {
    if (m_hWnd) {
        KillTimer(m_hWnd, IDT_DROPTARGET_HOOK);
        KillTimer(m_hWnd, IDT_UPDATE_CHECK);
        KillTimer(m_hWnd, IDT_UPDATE_PERIODIC);
        RevokeDragDrop(m_hWnd);
    }
    UpdateManager::Instance().Shutdown();
    if (m_dropTarget) {
        m_dropTarget->Release();
        m_dropTarget = nullptr;
    }
    RemoveTrayIcon();
}

bool AppWindow::Create() {
    // Enable Per-Monitor V2 DPI awareness if supported
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        typedef BOOL (WINAPI *SetProcessDpiAwarenessContextFn)(DPI_AWARENESS_CONTEXT);
        auto pfnSetDpi = (SetProcessDpiAwarenessContextFn)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
        if (pfnSetDpi) {
            pfnSetDpi(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        }
    }

    AppSettings settings = StorageManager::Instance().LoadSettings();
    m_alwaysOnTop = settings.alwaysOnTop;
    m_currentWidth = settings.compactWidth;
    m_currentHeight = settings.compactHeight;

    const wchar_t CLASS_NAME[] = L"K2DynamicIslandWindowClass";

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = AppWindow::WndProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    RegisterClassExW(&wc);

    // Scan monitor frame to middle-top default
    POINT pt = {};
    GetCursorPos(&pt);
    HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = { sizeof(mi) };
    int posX = 0;
    int posY = 12;

    if (GetMonitorInfoW(hMon, &mi)) {
        int workWidth = mi.rcWork.right - mi.rcWork.left;
        posX = mi.rcWork.left + (workWidth - m_currentWidth) / 2;
        posY = mi.rcWork.top + 12;
    } else {
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        posX = (screenW - m_currentWidth) / 2;
        posY = 12;
    }

    bool forceRecenter = (wcsstr(GetCommandLineW(), L"--recenter") != nullptr);
    if (!forceRecenter && settings.positionX >= 0 && settings.positionY >= 0) {
        POINT ptCheck = { settings.positionX + m_currentWidth / 2, settings.positionY + 10 };
        if (MonitorFromPoint(ptCheck, MONITOR_DEFAULTTONULL) != NULL) {
            posX = settings.positionX;
            posY = settings.positionY;
        }
    }

    if (forceRecenter || settings.positionX < 0) {
        settings.positionX = posX;
        settings.positionY = posY;
        StorageManager::Instance().SaveSettings(settings);
    }

    DWORD exStyle = WS_EX_TOOLWINDOW;
    if (m_alwaysOnTop) {
        exStyle |= WS_EX_TOPMOST;
    }

    K2::Log("AppWindow::Create calling CreateWindowExW");
    m_hWnd = CreateWindowExW(
        exStyle,
        CLASS_NAME,
        L"K2 Dynamic Island",
        WS_POPUP | WS_VISIBLE,
        posX, posY,
        m_currentWidth, m_currentHeight,
        NULL, NULL,
        GetModuleHandleW(NULL),
        this
    );

    if (!m_hWnd) {
        return false;
    }

    // Enable DWM alpha composition for transparent window frame
    MARGINS margins = {-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(m_hWnd, &margins);

    // Disable default rectangular non-client rendering and drop shadow
    DWMNCRENDERINGPOLICY ncPolicy = DWMNCRP_DISABLED;
    DwmSetWindowAttribute(m_hWnd, DWMWA_NCRENDERING_POLICY, &ncPolicy, sizeof(ncPolicy));

    // Disable DWM rectangular corner rounding on the HWND (handled cleanly in HTML/CSS)
    DWM_WINDOW_CORNER_PREFERENCE cornerPref = DWMWCP_DONOTROUND;
    DwmSetWindowAttribute(m_hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));

    // Initialize OLE Drop Target
    m_dropTarget = new DropTarget(
        [this](HDROP hDrop) { HandleDropFiles(hDrop); },
        [this](bool isDragging) {
            if (m_webViewManager) {
                nlohmann::json data;
                data["isDragging"] = isDragging;
                m_webViewManager->PostEvent("dragStateChanged", data);
            }
        }
    );
    RegisterDragDrop(m_hWnd, m_dropTarget);

    // Also support legacy WM_DROPFILES
    DragAcceptFiles(m_hWnd, TRUE);

    // Locate HTML file
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);

    std::wstring htmlPath = std::wstring(exePath) + L"\\assets\\index.html";
    if (GetFileAttributesW(htmlPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        // Fallback to relative path
        wchar_t currentDir[MAX_PATH] = {};
        GetCurrentDirectoryW(MAX_PATH, currentDir);
        htmlPath = std::wstring(currentDir) + L"\\assets\\index.html";
    }

    // Initialize WebView2
    m_webViewManager = std::make_unique<WebViewManager>(m_hWnd, m_dispatcher.get());
    if (!m_webViewManager->Initialize(htmlPath)) {
        return false;
    }

    // Initialize UpdateManager with UI thread dispatched event handler
    UpdateManager::Instance().Initialize([this](const std::string& event, const nlohmann::json& data) {
        DispatchToUI([this, event, data]() {
            if (m_webViewManager) {
                m_webViewManager->PostEvent(event, data);
            }
        });
    });

    HookChildDropTargets();

    // Schedule background update check if enabled
    if (settings.autoCheckUpdates) {
        SetTimer(m_hWnd, IDT_UPDATE_CHECK, 5000, NULL);
    }

    InitTrayIcon();
    return true;
}

void AppWindow::RunMessageLoop() {
    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void AppWindow::DispatchToUI(std::function<void()> fn) {
    if (!fn) return;
    {
        std::lock_guard<std::mutex> lock(m_uiDispatchMutex);
        m_uiDispatchQueue.push_back(std::move(fn));
    }
    if (m_hWnd) {
        PostMessageW(m_hWnd, WM_APP_DISPATCH_EVENT, 0, 0);
    }
}

LRESULT CALLBACK AppWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<AppWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hWnd = hWnd;
    } else {
        pThis = reinterpret_cast<AppWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandleMessage(msg, wParam, lParam);
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT AppWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_APP_DISPATCH_EVENT: {
        std::vector<std::function<void()>> tasks;
        {
            std::lock_guard<std::mutex> lock(m_uiDispatchMutex);
            tasks.swap(m_uiDispatchQueue);
        }
        for (auto& task : tasks) {
            if (task) task();
        }
        return 0;
    }

    case WM_TIMER:
        if (wParam == IDT_UPDATE_CHECK) {
            KillTimer(m_hWnd, IDT_UPDATE_CHECK);
            AppSettings s = StorageManager::Instance().LoadSettings();
            if (s.autoCheckUpdates) {
                UpdateManager::Instance().CheckForUpdates(false);
            }
            // Periodic check every 4 hours (14400000 ms)
            SetTimer(m_hWnd, IDT_UPDATE_PERIODIC, 4 * 60 * 60 * 1000, NULL);
        } else if (wParam == IDT_UPDATE_PERIODIC) {
            AppSettings s = StorageManager::Instance().LoadSettings();
            if (s.autoCheckUpdates) {
                UpdateManager::Instance().CheckForUpdates(false);
            }
        }
        return 0;

    case WM_DROPFILES:
        HandleDropFiles((HDROP)wParam);
        DragFinish((HDROP)wParam);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(m_hWnd, &ps);
        HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &ps.rcPaint, hBrush);
        DeleteObject(hBrush);
        EndPaint(m_hWnd, &ps);
        return 0;
    }

    case WM_SIZE:
        if (m_webViewManager) {
            RECT rc;
            GetClientRect(m_hWnd, &rc);
            m_webViewManager->Resize(rc.right - rc.left, rc.bottom - rc.top);
        }
        HookChildDropTargets();
        return 0;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP || lParam == WM_CONTEXTMENU) {
            ShowTrayMenu();
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_TRAY_OPEN:
            ShowWindow();
            break;
        case ID_TRAY_SETTINGS:
            ShowWindow();
            if (m_webViewManager) {
                m_webViewManager->PostEvent("switchTab", "settings");
            }
            break;
        case ID_TRAY_EXIT:
            CloseApp();
            break;
        }
        return 0;
    case WM_EXITSIZEMOVE: {
        RECT rc;
        if (GetWindowRect(m_hWnd, &rc)) {
            AppSettings s = StorageManager::Instance().LoadSettings();
            s.positionX = rc.left;
            s.positionY = rc.top;
            StorageManager::Instance().SaveSettings(s);
        }
        return 0;
    }

    case WM_CLOSE:
        K2::Log("AppWindow: WM_CLOSE received");
        CloseApp();
        return 0;

    case WM_DESTROY:
        K2::Log("AppWindow: WM_DESTROY received");
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(m_hWnd, msg, wParam, lParam);
}

void AppWindow::HookChildDropTargets() {
    if (!m_hWnd || !m_dropTarget) return;
    RegisterDragDrop(m_hWnd, m_dropTarget);
}

void AppWindow::HandleDropFiles(HDROP hDrop) {
    if (!hDrop) return;
    UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
    nlohmann::json filesArray = nlohmann::json::array();

    for (UINT i = 0; i < count; ++i) {
        wchar_t path[MAX_PATH] = {};
        if (DragQueryFileW(hDrop, i, path, MAX_PATH)) {
            WIN32_FILE_ATTRIBUTE_DATA fad = {};
            BOOL hasAttrs = GetFileAttributesExW(path, GetFileExInfoStandard, &fad);

            bool isDir = (hasAttrs && (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
            unsigned long long size = 0;
            if (hasAttrs && !isDir) {
                size = ((unsigned long long)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
            }

            const wchar_t* fileName = PathFindFileNameW(path);
            const wchar_t* ext = PathFindExtensionW(path);

            filesArray.push_back({
                {"name", Utf16ToUtf8(fileName)},
                {"path", Utf16ToUtf8(path)},
                {"extension", Utf16ToUtf8(ext)},
                {"size", size},
                {"isDirectory", isDir}
            });
        }
    }

    if (m_webViewManager && !filesArray.empty()) {
        m_webViewManager->PostEvent("filesDropped", filesArray);
    }
}

void AppWindow::SetWindowSize(int width, int height) {
    RECT rect;
    GetWindowRect(m_hWnd, &rect);

    // Expand symmetrically around center
    int currentCenterX = rect.left + (rect.right - rect.left) / 2;
    int newX = currentCenterX - (width / 2);
    int newY = rect.top;

    // Ensure within monitor work area boundaries
    HMONITOR hMon = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    if (GetMonitorInfoW(hMon, &mi)) {
        if (newX < mi.rcWork.left + 8) newX = mi.rcWork.left + 8;
        if (newX + width > mi.rcWork.right - 8) newX = mi.rcWork.right - width - 8;
    } else {
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        if (newX < 10) newX = 10;
        if (newX + width > screenW - 10) newX = screenW - width - 10;
    }

    m_currentWidth = width;
    m_currentHeight = height;

    SetWindowPos(m_hWnd, NULL, newX, newY, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    if (m_webViewManager) {
        m_webViewManager->Resize(width, height);
    }
    HookChildDropTargets();
}

void AppWindow::StartWindowDrag() {
    ReleaseCapture();
    SendMessageW(m_hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
}

void AppWindow::RecenterWindow() {
    if (!m_hWnd) return;

    // Scan monitor frame where the window currently is
    HMONITOR hMon = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };

    RECT rcWnd = {};
    GetWindowRect(m_hWnd, &rcWnd);
    int curW = rcWnd.right - rcWnd.left;
    int curH = rcWnd.bottom - rcWnd.top;
    if (curW <= 0) curW = m_currentWidth;
    if (curH <= 0) curH = m_currentHeight;

    int newX = 0;
    int newY = 12;

    if (GetMonitorInfoW(hMon, &mi)) {
        int workWidth = mi.rcWork.right - mi.rcWork.left;
        newX = mi.rcWork.left + (workWidth - curW) / 2;
        newY = mi.rcWork.top + 12;
    } else {
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        newX = (screenW - curW) / 2;
        newY = 12;
    }

    SetWindowPos(m_hWnd, m_alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
        newX, newY, curW, curH, SWP_SHOWWINDOW | SWP_NOACTIVATE);

    // Save centered position so restart preserves it
    AppSettings s = StorageManager::Instance().LoadSettings();
    s.positionX = newX;
    s.positionY = newY;
    s.alwaysOnTop = m_alwaysOnTop;
    StorageManager::Instance().SaveSettings(s);
}

void AppWindow::SetAlwaysOnTop(bool alwaysOnTop) {
    m_alwaysOnTop = alwaysOnTop;
    if (m_hWnd) {
        SetWindowPos(m_hWnd, m_alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
            0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void AppWindow::HideWindow() {
    ::ShowWindow(m_hWnd, SW_HIDE);
}

void AppWindow::ShowWindow() {
    ::ShowWindow(m_hWnd, SW_SHOW);
    SetForegroundWindow(m_hWnd);
}

void AppWindow::CloseApp() {
    K2::Log("AppWindow: CloseApp called");
    RemoveTrayIcon();
    DestroyWindow(m_hWnd);
}

void AppWindow::InitTrayIcon() {
    m_nid.cbSize = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd = m_hWnd;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;

    HICON hAppIcon = LoadIcon(GetModuleHandleW(NULL), MAKEINTRESOURCE(101));
    if (!hAppIcon) {
        hAppIcon = LoadIcon(NULL, IDI_APPLICATION);
    }
    m_nid.hIcon = hAppIcon;
    wcscpy_s(m_nid.szTip, L"K2 Dynamic Island");

    Shell_NotifyIconW(NIM_ADD, &m_nid);
    m_trayAdded = true;
}

void AppWindow::RemoveTrayIcon() {
    if (m_trayAdded) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        m_trayAdded = false;
    }
}

void AppWindow::ShowTrayMenu() {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    InsertMenuW(hMenu, 0, MF_BYPOSITION | MF_STRING, ID_TRAY_OPEN, L"Open K2 Dynamic Island");
    InsertMenuW(hMenu, 1, MF_BYPOSITION | MF_STRING, ID_TRAY_SETTINGS, L"Settings");
    InsertMenuW(hMenu, 2, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    InsertMenuW(hMenu, 3, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"Exit");

    SetForegroundWindow(m_hWnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_RIGHTALIGN, pt.x, pt.y, 0, m_hWnd, NULL);
    PostMessageW(m_hWnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

} // namespace K2
