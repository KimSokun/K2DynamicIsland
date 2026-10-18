#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <vector>

static void DeleteShortcut(int csidl, const std::wstring& filename) {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, csidl, NULL, 0, path))) {
        std::wstring link = std::wstring(path) + L"\\" + filename;
        DeleteFileW(link.c_str());
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    bool silent = (wcsstr(pCmdLine, L"/S") != nullptr || wcsstr(pCmdLine, L"/SILENT") != nullptr || wcsstr(pCmdLine, L"/s") != nullptr);

    if (!silent) {
        int res = MessageBoxW(NULL,
            L"Are you sure you want to completely remove K2 Dynamic Island and all of its components?",
            L"K2 Dynamic Island Uninstall",
            MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2);
        if (res != IDYES) {
            return 0;
        }
    }

    // Terminate running instance if any
    HWND hExisting = FindWindowW(L"K2DynamicIslandWindowClass", L"K2 Dynamic Island");
    if (hExisting) {
        PostMessageW(hExisting, WM_CLOSE, 0, 0);
        Sleep(500);
    }

    // Get current directory where uninstaller is located
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);
    std::wstring installDir = exePath;

    // Delete Shortcuts
    DeleteShortcut(CSIDL_PROGRAMS, L"K2 Dynamic Island.lnk");
    DeleteShortcut(CSIDL_DESKTOPDIRECTORY, L"K2 Dynamic Island.lnk");
    DeleteShortcut(CSIDL_COMMON_PROGRAMS, L"K2 Dynamic Island.lnk");
    DeleteShortcut(CSIDL_COMMON_DESKTOPDIRECTORY, L"K2 Dynamic Island.lnk");

    // Remove Registry Keys
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\K2DynamicIsland");
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\K2DynamicIsland");
    HKEY hRunKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hRunKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hRunKey, L"K2 Dynamic Island");
        RegCloseKey(hRunKey);
    }

    // Delete app files
    std::wstring appExe = installDir + L"\\K2DynamicIsland.exe";
    std::wstring loaderDll = installDir + L"\\WebView2Loader.dll";
    DeleteFileW(appExe.c_str());
    DeleteFileW(loaderDll.c_str());

    // Clean up directory via delayed command
    wchar_t cmd[1024];
    wsprintfW(cmd, L"/c ping 127.0.0.1 -n 2 > nul & rd /s /q \"%s\"", installDir.c_str());
    ShellExecuteW(NULL, L"open", L"cmd.exe", cmd, NULL, SW_HIDE);

    if (!silent) {
        MessageBoxW(NULL,
            L"K2 Dynamic Island was successfully removed from your computer.",
            L"K2 Dynamic Island Uninstall",
            MB_ICONINFORMATION | MB_OK);
    }

    return 0;
}
