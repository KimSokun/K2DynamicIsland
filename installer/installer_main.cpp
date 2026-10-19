#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <vector>

#pragma comment(lib, "comctl32.lib")

#define IDR_PAYLOAD 201
#define IDI_APP_ICON 101

#define IDC_BTN_INSTALL    1001
#define IDC_BTN_CANCEL     1002
#define IDC_BTN_BROWSE     1003
#define IDC_EDIT_PATH      1004
#define IDC_CHK_DESKTOP    1005
#define IDC_CHK_LAUNCH     1006
#define IDC_PROGRESS       1007
#define IDC_STATUS_LABEL   1008

#define WM_INSTALL_PROGRESS (WM_APP + 10)
#define WM_INSTALL_COMPLETE (WM_APP + 11)
#define WM_INSTALL_ERROR    (WM_APP + 12)

struct InstallContext {
    HWND hWnd;
    std::wstring installDir;
    bool createDesktopShortcut;
    bool launchAfterInstall;
};

static InstallContext g_ctx;
static HFONT g_hFontNormal = NULL;
static HFONT g_hFontBold = NULL;
static HFONT g_hFontTitle = NULL;
static HBRUSH g_hBrushBanner = NULL;
static bool g_installFinished = false;

static bool IsWebView2Installed() {
    const wchar_t* guid = L"{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}";
    std::wstring subKeys[] = {
        L"SOFTWARE\\WOW6432Node\\Microsoft\\EdgeUpdate\\Clients\\" + std::wstring(guid),
        L"Software\\Microsoft\\EdgeUpdate\\Clients\\" + std::wstring(guid),
        L"SOFTWARE\\Microsoft\\EdgeUpdate\\Clients\\" + std::wstring(guid)
    };
    HKEY roots[] = { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };
    REGSAM samFlags[] = { KEY_READ | KEY_WOW64_32KEY, KEY_READ, KEY_READ | KEY_WOW64_64KEY };

    for (int i = 0; i < 3; ++i) {
        for (int s = 0; s < 3; ++s) {
            HKEY hKey;
            if (RegOpenKeyExW(roots[i], subKeys[i].c_str(), 0, samFlags[s], &hKey) == ERROR_SUCCESS) {
                wchar_t ver[128] = {};
                DWORD size = sizeof(ver);
                if (RegQueryValueExW(hKey, L"pv", NULL, NULL, (LPBYTE)ver, &size) == ERROR_SUCCESS && wcslen(ver) > 0) {
                    RegCloseKey(hKey);
                    return true;
                }
                RegCloseKey(hKey);
            }
        }
    }

    wchar_t progFiles[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILESX86, NULL, 0, progFiles))) {
        std::wstring edgePath = std::wstring(progFiles) + L"\\Microsoft\\Edge\\Application\\msedge.exe";
        if (GetFileAttributesW(edgePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return true;
        }
    }
    return false;
}

static bool CreateShortcut(const std::wstring& targetExe, const std::wstring& linkPath, const std::wstring& workingDir, const std::wstring& description) {
    HRESULT hr = CoInitialize(NULL);
    bool coInited = SUCCEEDED(hr);

    IShellLinkW* psl = nullptr;
    hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&psl);
    if (SUCCEEDED(hr)) {
        psl->SetPath(targetExe.c_str());
        psl->SetWorkingDirectory(workingDir.c_str());
        psl->SetDescription(description.c_str());

        IPersistFile* ppf = nullptr;
        hr = psl->QueryInterface(IID_IPersistFile, (void**)&ppf);
        if (SUCCEEDED(hr)) {
            hr = ppf->Save(linkPath.c_str(), TRUE);
            ppf->Release();
        }
        psl->Release();
    }

    if (coInited) CoUninitialize();
    return SUCCEEDED(hr);
}

static DWORD WINAPI InstallThreadProc(LPVOID lpParam) {
    auto* ctx = static_cast<InstallContext*>(lpParam);
    HWND hProgress = GetDlgItem(ctx->hWnd, IDC_PROGRESS);
    HWND hStatus = GetDlgItem(ctx->hWnd, IDC_STATUS_LABEL);

    // 1. Preparing
    PostMessageW(ctx->hWnd, WM_INSTALL_PROGRESS, 10, (LPARAM)L"Preparing installation...");
    Sleep(250);

    // Forcibly terminate any running instance of K2DynamicIsland.exe so files and mutex are freed
    PostMessageW(ctx->hWnd, WM_INSTALL_PROGRESS, 15, (LPARAM)L"Stopping running application instance...");
    system("taskkill /f /im K2DynamicIsland.exe 2>nul");
    Sleep(600);

    // Clear stale WebView2 cache synchronously before extraction so there are no file locks or background race conditions
    wchar_t appDataPath[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appDataPath))) {
        std::wstring cacheDir = std::wstring(appDataPath) + L"\\K2DynamicIsland\\WebView2Cache";
        std::wstring rmCmd = L"/c rmdir /s /q \"" + cacheDir + L"\" 2>nul";
        SHELLEXECUTEINFOW rmSei = {};
        rmSei.cbSize = sizeof(rmSei);
        rmSei.fMask = SEE_MASK_NOCLOSEPROCESS;
        rmSei.lpVerb = L"open";
        rmSei.lpFile = L"cmd.exe";
        rmSei.lpParameters = rmCmd.c_str();
        rmSei.nShow = SW_HIDE;
        if (ShellExecuteExW(&rmSei) && rmSei.hProcess) {
            WaitForSingleObject(rmSei.hProcess, 5000);
            CloseHandle(rmSei.hProcess);
        }
    }

    // 2. Ensure install directory exists and reset attributes
    SHCreateDirectoryExW(NULL, ctx->installDir.c_str(), NULL);
    std::wstring assetsFolder = ctx->installDir + L"\\assets";
    SetFileAttributesW(assetsFolder.c_str(), FILE_ATTRIBUTE_NORMAL);
    std::wstring targetExe = ctx->installDir + L"\\K2DynamicIsland.exe";
    SetFileAttributesW(targetExe.c_str(), FILE_ATTRIBUTE_NORMAL);

    // 3. Extract payload resource
    PostMessageW(ctx->hWnd, WM_INSTALL_PROGRESS, 25, (LPARAM)L"Unpacking package archives...");
    HMODULE hMod = GetModuleHandleW(NULL);
    HRSRC hRes = FindResourceW(hMod, MAKEINTRESOURCEW(IDR_PAYLOAD), RT_RCDATA);
    if (!hRes) {
        PostMessageW(ctx->hWnd, WM_INSTALL_ERROR, 0, (LPARAM)L"Installation payload resource not found.");
        return 1;
    }

    HGLOBAL hMem = LoadResource(hMod, hRes);
    if (!hMem) {
        PostMessageW(ctx->hWnd, WM_INSTALL_ERROR, 0, (LPARAM)L"Failed to load package data.");
        return 1;
    }

    DWORD resSize = SizeofResource(hMod, hRes);
    void* pData = LockResource(hMem);
    if (!pData || resSize == 0) {
        PostMessageW(ctx->hWnd, WM_INSTALL_ERROR, 0, (LPARAM)L"Corrupt installation payload.");
        return 1;
    }

    wchar_t tempDir[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tempDir);
    std::wstring tempZip = std::wstring(tempDir) + L"k2_setup_" + std::to_wstring(GetTickCount64()) + L".zip";

    HANDLE hFile = CreateFileW(tempZip.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(hFile, pData, resSize, &written, NULL);
        CloseHandle(hFile);
    } else {
        PostMessageW(ctx->hWnd, WM_INSTALL_ERROR, 0, (LPARAM)L"Failed to write temporary files.");
        return 1;
    }

    // 4. Extracting files
    PostMessageW(ctx->hWnd, WM_INSTALL_PROGRESS, 50, (LPARAM)L"Installing application files...");
    std::wstring cmd = L"/c tar.exe -xf \"" + tempZip + L"\" -C \"" + ctx->installDir + L"\"";
    bool tarSuccess = false;
    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"open";
    sei.lpFile = L"cmd.exe";
    sei.lpParameters = cmd.c_str();
    sei.nShow = SW_HIDE;

    if (ShellExecuteExW(&sei) && sei.hProcess) {
        if (WaitForSingleObject(sei.hProcess, 15000) == WAIT_OBJECT_0) {
            DWORD exitCode = 1;
            GetExitCodeProcess(sei.hProcess, &exitCode);
            tarSuccess = (exitCode == 0);
        }
        CloseHandle(sei.hProcess);
    }

    // Fallback extraction via PowerShell if tar did not succeed
    if (!tarSuccess) {
        std::wstring psCmd = L"-NoProfile -ExecutionPolicy Bypass -Command \"Expand-Archive -LiteralPath '" + tempZip + L"' -DestinationPath '" + ctx->installDir + L"' -Force\"";
        SHELLEXECUTEINFOW psSei = {};
        psSei.cbSize = sizeof(psSei);
        psSei.fMask = SEE_MASK_NOCLOSEPROCESS;
        psSei.lpVerb = L"open";
        psSei.lpFile = L"powershell.exe";
        psSei.lpParameters = psCmd.c_str();
        psSei.nShow = SW_HIDE;
        if (ShellExecuteExW(&psSei) && psSei.hProcess) {
            WaitForSingleObject(psSei.hProcess, 30000);
            CloseHandle(psSei.hProcess);
        }
    }

    DeleteFileW(tempZip.c_str());

    if (GetFileAttributesW(targetExe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        PostMessageW(ctx->hWnd, WM_INSTALL_ERROR, 0, (LPARAM)L"Extraction failed. Could not write application binaries.");
        return 1;
    }

    // 5. Creating Shortcuts
    PostMessageW(ctx->hWnd, WM_INSTALL_PROGRESS, 80, (LPARAM)L"Creating system shortcuts...");
    wchar_t startMenuPath[MAX_PATH] = {};
    SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, 0, startMenuPath);
    std::wstring startMenuLink = std::wstring(startMenuPath) + L"\\K2 Dynamic Island.lnk";
    CreateShortcut(targetExe, startMenuLink, ctx->installDir, L"K2 Dynamic Island Desktop Capsule");

    if (ctx->createDesktopShortcut) {
        wchar_t desktopPath[MAX_PATH] = {};
        SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktopPath);
        std::wstring desktopLink = std::wstring(desktopPath) + L"\\K2 Dynamic Island.lnk";
        CreateShortcut(targetExe, desktopLink, ctx->installDir, L"K2 Dynamic Island Desktop Capsule");
    }

    // 6. Registering Uninstaller in Windows Registry
    PostMessageW(ctx->hWnd, WM_INSTALL_PROGRESS, 92, (LPARAM)L"Registering application in Windows...");
    HKEY hUninstKey;
    const wchar_t* uninstSubKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\K2DynamicIsland";
    if (RegCreateKeyExW(HKEY_CURRENT_USER, uninstSubKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hUninstKey, NULL) == ERROR_SUCCESS) {
        const wchar_t* appName = L"K2 Dynamic Island";
        const wchar_t* appVer = L"1.1.0";
        const wchar_t* publisher = L"K2 Studio";
        std::wstring uninstCmd = L"\"" + ctx->installDir + L"\\Uninstall.exe\"";
        std::wstring iconPath = L"\"" + targetExe + L"\",0";
        DWORD dwordVal = 1;
        DWORD sizeVal = 3200; // ~3.2 MB

        RegSetValueExW(hUninstKey, L"DisplayName", 0, REG_SZ, (const BYTE*)appName, (DWORD)((wcslen(appName) + 1) * sizeof(wchar_t)));
        RegSetValueExW(hUninstKey, L"DisplayVersion", 0, REG_SZ, (const BYTE*)appVer, (DWORD)((wcslen(appVer) + 1) * sizeof(wchar_t)));
        RegSetValueExW(hUninstKey, L"Publisher", 0, REG_SZ, (const BYTE*)publisher, (DWORD)((wcslen(publisher) + 1) * sizeof(wchar_t)));
        RegSetValueExW(hUninstKey, L"InstallLocation", 0, REG_SZ, (const BYTE*)ctx->installDir.c_str(), (DWORD)((ctx->installDir.length() + 1) * sizeof(wchar_t)));
        RegSetValueExW(hUninstKey, L"UninstallString", 0, REG_SZ, (const BYTE*)uninstCmd.c_str(), (DWORD)((uninstCmd.length() + 1) * sizeof(wchar_t)));
        RegSetValueExW(hUninstKey, L"DisplayIcon", 0, REG_SZ, (const BYTE*)iconPath.c_str(), (DWORD)((iconPath.length() + 1) * sizeof(wchar_t)));
        RegSetValueExW(hUninstKey, L"NoModify", 0, REG_DWORD, (const BYTE*)&dwordVal, sizeof(DWORD));
        RegSetValueExW(hUninstKey, L"NoRepair", 0, REG_DWORD, (const BYTE*)&dwordVal, sizeof(DWORD));
        RegSetValueExW(hUninstKey, L"EstimatedSize", 0, REG_DWORD, (const BYTE*)&sizeVal, sizeof(DWORD));
        RegCloseKey(hUninstKey);
    }

    // 7. Finish
    PostMessageW(ctx->hWnd, WM_INSTALL_PROGRESS, 100, (LPARAM)L"Installation completed successfully!");
    PostMessageW(ctx->hWnd, WM_INSTALL_COMPLETE, 0, 0);
    return 0;
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_ctx.hWnd = hWnd;

        // Fonts
        g_hFontNormal = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_hFontBold = CreateFontW(-12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_hFontTitle = CreateFontW(-18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        g_hBrushBanner = CreateSolidBrush(RGB(18, 22, 28));

        // Destination Folder Label
        HWND hLblPath = CreateWindowW(L"STATIC", L"Destination Directory:",
            WS_CHILD | WS_VISIBLE, 24, 85, 300, 18, hWnd, NULL, NULL, NULL);
        SendMessageW(hLblPath, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

        // Edit box for install directory
        HWND hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_ctx.installDir.c_str(),
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 24, 108, 380, 24, hWnd, (HMENU)IDC_EDIT_PATH, NULL, NULL);
        SendMessageW(hEdit, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        // Browse Button
        HWND hBtnBrowse = CreateWindowW(L"BUTTON", L"Browse...",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 412, 107, 84, 26, hWnd, (HMENU)IDC_BTN_BROWSE, NULL, NULL);
        SendMessageW(hBtnBrowse, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        // Options Checkboxes
        HWND hChkDesktop = CreateWindowW(L"BUTTON", L"Create Desktop shortcut",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 24, 146, 260, 20, hWnd, (HMENU)IDC_CHK_DESKTOP, NULL, NULL);
        SendMessageW(hChkDesktop, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        SendMessageW(hChkDesktop, BM_SETCHECK, BST_CHECKED, 0);

        HWND hChkLaunch = CreateWindowW(L"BUTTON", L"Launch K2 Dynamic Island after installation",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 24, 172, 320, 20, hWnd, (HMENU)IDC_CHK_LAUNCH, NULL, NULL);
        SendMessageW(hChkLaunch, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        SendMessageW(hChkLaunch, BM_SETCHECK, BST_CHECKED, 0);

        // Status Label
        HWND hLblStatus = CreateWindowW(L"STATIC", L"Ready to install. Click 'Install' to begin setup.",
            WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP, 24, 210, 472, 18, hWnd, (HMENU)IDC_STATUS_LABEL, NULL, NULL);
        SendMessageW(hLblStatus, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        // Progress Bar
        HWND hProg = CreateWindowExW(0, PROGRESS_CLASSW, NULL,
            WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 24, 232, 472, 18, hWnd, (HMENU)IDC_PROGRESS, NULL, NULL);
        SendMessageW(hProg, PBM_SETRANGE32, 0, 100);
        SendMessageW(hProg, PBM_SETPOS, 0, 0);

        // Bottom Action Buttons
        HWND hBtnInstall = CreateWindowW(L"BUTTON", L"Install",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 304, 290, 96, 28, hWnd, (HMENU)IDC_BTN_INSTALL, NULL, NULL);
        SendMessageW(hBtnInstall, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

        HWND hBtnCancel = CreateWindowW(L"BUTTON", L"Cancel",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 408, 290, 88, 28, hWnd, (HMENU)IDC_BTN_CANCEL, NULL, NULL);
        SendMessageW(hBtnCancel, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // Draw top banner
        RECT rcBanner = { 0, 0, 520, 72 };
        FillRect(hdc, &rcBanner, g_hBrushBanner);

        // Banner text
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        SelectObject(hdc, g_hFontTitle);
        TextOutW(hdc, 24, 14, L"K2 Dynamic Island", 17);

        SetTextColor(hdc, RGB(148, 163, 184));
        const wchar_t bannerSub[] = L"Native Desktop Capsule Utility \u2022 Setup Wizard";
        TextOutW(hdc, 24, 40, bannerSub, (int)wcslen(bannerSub));

        // Top banner divider line
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(45, 55, 72));
        HGDIOBJ hOldPen = SelectObject(hdc, hPen);
        MoveToEx(hdc, 0, 72, NULL);
        LineTo(hdc, 520, 72);

        // Bottom divider line
        MoveToEx(hdc, 0, 276, NULL);
        LineTo(hdc, 520, 276);

        SelectObject(hdc, hOldPen);
        DeleteObject(hPen);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_COMMAND: {
        WORD id = LOWORD(wParam);
        if (id == IDC_BTN_BROWSE) {
            BROWSEINFOW bi = {};
            bi.hwndOwner = hWnd;
            bi.lpszTitle = L"Select Installation Folder for K2 Dynamic Island:";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
            if (pidl) {
                wchar_t selected[MAX_PATH];
                if (SHGetPathFromIDListW(pidl, selected)) {
                    std::wstring full = std::wstring(selected) + L"\\K2 Dynamic Island";
                    g_ctx.installDir = full;
                    SetDlgItemTextW(hWnd, IDC_EDIT_PATH, full.c_str());
                }
                CoTaskMemFree(pidl);
            }
        }
        else if (id == IDC_BTN_INSTALL) {
            if (g_installFinished) {
                // Launch app if checked
                if (g_ctx.launchAfterInstall) {
                    std::wstring targetExe = g_ctx.installDir + L"\\K2DynamicIsland.exe";
                    ShellExecuteW(NULL, L"open", targetExe.c_str(), L"--recenter", g_ctx.installDir.c_str(), SW_SHOWNORMAL);
                }
                DestroyWindow(hWnd);
                return 0;
            }

            // Retrieve updated install path from edit box
            wchar_t buf[MAX_PATH] = {};
            GetDlgItemTextW(hWnd, IDC_EDIT_PATH, buf, MAX_PATH);
            if (wcslen(buf) > 0) g_ctx.installDir = buf;

            g_ctx.createDesktopShortcut = (IsDlgButtonChecked(hWnd, IDC_CHK_DESKTOP) == BST_CHECKED);
            g_ctx.launchAfterInstall = (IsDlgButtonChecked(hWnd, IDC_CHK_LAUNCH) == BST_CHECKED);

            // Disable controls during installation
            EnableWindow(GetDlgItem(hWnd, IDC_BTN_INSTALL), FALSE);
            EnableWindow(GetDlgItem(hWnd, IDC_BTN_BROWSE), FALSE);
            EnableWindow(GetDlgItem(hWnd, IDC_EDIT_PATH), FALSE);
            EnableWindow(GetDlgItem(hWnd, IDC_CHK_DESKTOP), FALSE);
            EnableWindow(GetDlgItem(hWnd, IDC_CHK_LAUNCH), FALSE);

            CreateThread(NULL, 0, InstallThreadProc, &g_ctx, 0, NULL);
        }
        else if (id == IDC_BTN_CANCEL) {
            DestroyWindow(hWnd);
        }
        return 0;
    }

    case WM_INSTALL_PROGRESS: {
        int pos = (int)wParam;
        const wchar_t* msgText = (const wchar_t*)lParam;
        SendMessageW(GetDlgItem(hWnd, IDC_PROGRESS), PBM_SETPOS, pos, 0);
        if (msgText) {
            SetDlgItemTextW(hWnd, IDC_STATUS_LABEL, msgText);
        }
        return 0;
    }

    case WM_INSTALL_COMPLETE: {
        g_installFinished = true;
        SetDlgItemTextW(hWnd, IDC_STATUS_LABEL, L"Installation completed successfully!");
        SetDlgItemTextW(hWnd, IDC_BTN_INSTALL, L"Finish");
        EnableWindow(GetDlgItem(hWnd, IDC_BTN_INSTALL), TRUE);
        EnableWindow(GetDlgItem(hWnd, IDC_BTN_CANCEL), FALSE);
        return 0;
    }

    case WM_INSTALL_ERROR: {
        const wchar_t* errMsg = (const wchar_t*)lParam;
        SetDlgItemTextW(hWnd, IDC_STATUS_LABEL, errMsg ? errMsg : L"Installation failed.");
        MessageBoxW(hWnd, errMsg ? errMsg : L"Installation failed.", L"Installation Error", MB_ICONERROR | MB_OK);
        EnableWindow(GetDlgItem(hWnd, IDC_BTN_INSTALL), TRUE);
        EnableWindow(GetDlgItem(hWnd, IDC_BTN_CANCEL), TRUE);
        return 0;
    }

    case WM_DESTROY: {
        if (g_hFontNormal) DeleteObject(g_hFontNormal);
        if (g_hFontBold) DeleteObject(g_hFontBold);
        if (g_hFontTitle) DeleteObject(g_hFontTitle);
        if (g_hBrushBanner) DeleteObject(g_hBrushBanner);
        PostQuitMessage(0);
        return 0;
    }
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    // Default Install Directory
    wchar_t localAppPath[MAX_PATH] = {};
    SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppPath);
    g_ctx.installDir = std::wstring(localAppPath) + L"\\Programs\\K2 Dynamic Island";
    g_ctx.createDesktopShortcut = true;
    g_ctx.launchAfterInstall = true;

    // Parse command line arguments safely
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool silent = false;
    bool noRun = false;

    if (argv) {
        for (int i = 1; i < argc; ++i) {
            if (_wcsicmp(argv[i], L"/S") == 0 || _wcsicmp(argv[i], L"/SILENT") == 0) {
                silent = true;
            } else if (_wcsicmp(argv[i], L"/NO_RUN") == 0) {
                noRun = true;
            } else if (_wcsnicmp(argv[i], L"/DIR=", 5) == 0) {
                g_ctx.installDir = argv[i] + 5;
            }
        }
        LocalFree(argv);
    }

    if (silent) {
        // Execute silent install directly in background
        g_ctx.hWnd = NULL;
        g_ctx.launchAfterInstall = !noRun;
        InstallThreadProc(&g_ctx);
        if (!noRun) {
            std::wstring targetExe = g_ctx.installDir + L"\\K2DynamicIsland.exe";
            ShellExecuteW(NULL, L"open", targetExe.c_str(), NULL, g_ctx.installDir.c_str(), SW_SHOWNORMAL);
        }
        return 0;
    }

    // Graphical Setup Wizard Window
    const wchar_t CLASS_NAME[] = L"K2SetupWizardWindowClass";
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);

    RegisterClassExW(&wc);

    // Center window on screen
    int winW = 536;
    int winH = 372;
    int posX = (GetSystemMetrics(SM_CXSCREEN) - winW) / 2;
    int posY = (GetSystemMetrics(SM_CYSCREEN) - winH) / 2;

    HWND hWnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        CLASS_NAME,
        L"K2 Dynamic Island Setup",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        posX, posY, winW, winH,
        NULL, NULL, hInstance, NULL
    );

    if (!hWnd) return 1;

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
