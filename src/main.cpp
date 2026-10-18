#include "app_window.h"
#include "anti_debug.h"
#include "logger.h"
#include <windows.h>
#include <iostream>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // 0. Anti-Reverse Engineering & Debugger Defense Watchdog
    K2::SecurityGuard::Instance().StartWatchdog();

    K2::Log("wWinMain started");

    // Enable Per-Monitor DPI Awareness v2 for pixel-exact monitor frame metrics
    typedef BOOL(WINAPI* SetProcessDpiAwarenessContextProc)(DPI_AWARENESS_CONTEXT);
    auto pSetProcessDpiAwarenessContext = (SetProcessDpiAwarenessContextProc)GetProcAddress(
        GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext");
    if (pSetProcessDpiAwarenessContext) {
        pSetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    } else {
        SetProcessDPIAware();
    }

    // Single instance mutex
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"K2DynamicIslandSingleInstanceMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        K2::Log("Mutex already exists, finding existing window");
        HWND existingWnd = FindWindowW(L"K2DynamicIslandWindowClass", L"K2 Dynamic Island");
        if (existingWnd) {
            ShowWindow(existingWnd, SW_SHOW);
            SetForegroundWindow(existingWnd);
        }
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    HRESULT hr = OleInitialize(NULL);
    K2::Log("OleInitialize result: " + std::to_string(hr));

    {
        K2::AppWindow window;
        if (window.Create()) {
            K2::Log("window.Create() succeeded, entering RunMessageLoop");
            window.RunMessageLoop();
            K2::Log("RunMessageLoop exited");
        } else {
            K2::Log("window.Create() returned false!");
        }
    }

    if (SUCCEEDED(hr)) {
        OleUninitialize();
    }

    if (hMutex) {
        CloseHandle(hMutex);
    }

    K2::Log("wWinMain finished");
    return 0;
}

int main() {
    return wWinMain(GetModuleHandle(NULL), NULL, GetCommandLineW(), SW_SHOWNORMAL);
}
