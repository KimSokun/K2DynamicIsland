#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <windows.h>
#include "../src/bridge_dispatcher.h"
#include "../src/storage_manager.h"
#include "../src/system_stats.h"
#include "../src/media_controller.h"
#include "../src/update_manager.h"
#include "../src/drop_target.h"
#include <fstream>
#include "json.hpp"

using namespace K2;
using json = nlohmann::json;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "[-] Assertion FAILED: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
            return false; \
        } \
    } while(0)

#define RUN_TEST(fn) \
    do { \
        std::cout << "[RUNNING] " << #fn << "..." << std::flush; \
        if (fn()) { \
            std::cout << " [PASS]" << std::endl; \
            passed++; \
        } else { \
            std::cout << " [FAIL]" << std::endl; \
            failed++; \
        } \
    } while(0)

bool Test_PathValidation() {
    wchar_t currentDir[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, currentDir);

    // Existing directory
    TEST_ASSERT(BridgeDispatcher::ValidatePathExists(currentDir), "Current directory must exist");

    // Existing file (check README.md in current dir or parent dir, or CMakeCache.txt)
    std::wstring readme = std::wstring(currentDir) + L"\\README.md";
    std::wstring readmeParent = std::wstring(currentDir) + L"\\..\\README.md";
    bool hasReadme = BridgeDispatcher::ValidatePathExists(readme) || BridgeDispatcher::ValidatePathExists(readmeParent);
    TEST_ASSERT(hasReadme, "README.md must exist in current or parent directory");

    // Windows system directory exists
    TEST_ASSERT(BridgeDispatcher::ValidatePathExists(L"C:\\Windows"), "C:\\Windows must exist");

    // Non-existent path
    std::wstring fake = L"C:\\This_Path_Does_Not_Exist_123456789.xyz";
    TEST_ASSERT(!BridgeDispatcher::ValidatePathExists(fake), "Non-existent path must return false");

    // Empty path
    TEST_ASSERT(!BridgeDispatcher::ValidatePathExists(L""), "Empty path must return false");

    return true;
}

bool Test_ClipboardCopy() {
    std::wstring testStr = L"C:\\Users\\K2\\Desktop\\Island_Special_文件_🚀.txt";
    TEST_ASSERT(BridgeDispatcher::CopyTextToClipboard(testStr), "Copy to clipboard should succeed");

    // Verify against actual Windows Clipboard
    TEST_ASSERT(OpenClipboard(NULL), "OpenClipboard must succeed");
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    TEST_ASSERT(hData != NULL, "GetClipboardData must return handle");

    wchar_t* pText = static_cast<wchar_t*>(GlobalLock(hData));
    TEST_ASSERT(pText != NULL, "GlobalLock on clipboard data must succeed");

    std::wstring retrieved(pText);
    GlobalUnlock(hData);
    CloseClipboard();

    TEST_ASSERT(retrieved == testStr, "Retrieved clipboard text must match exact Unicode string");
    return true;
}

bool Test_SettingsPersistence() {
    AppSettings original = StorageManager::Instance().LoadSettings();

    AppSettings modified = original;
    modified.alwaysOnTop = !original.alwaysOnTop;
    modified.compactWidth = 310;
    modified.activeTab = "reminders";
    modified.themeMode = "light";
    modified.backgroundType = "lively";

    TEST_ASSERT(StorageManager::Instance().SaveSettings(modified), "SaveSettings must succeed");

    AppSettings reloaded = StorageManager::Instance().LoadSettings();
    TEST_ASSERT(reloaded.alwaysOnTop == modified.alwaysOnTop, "alwaysOnTop must persist");
    TEST_ASSERT(reloaded.compactWidth == 310, "compactWidth must persist");
    TEST_ASSERT(reloaded.activeTab == "reminders", "activeTab must persist");
    TEST_ASSERT(reloaded.themeMode == "light", "themeMode must persist");
    TEST_ASSERT(reloaded.backgroundType == "lively", "backgroundType must persist");

    // Restore original settings
    StorageManager::Instance().SaveSettings(original);
    return true;
}

bool Test_ReminderCRUD() {
    Reminder testRem;
    testRem.id = "test-rem-" + std::to_string(GetTickCount64());
    testRem.title = "Unit Test Verification Reminder";
    testRem.category = "Health";
    testRem.time = "18:00";
    testRem.date = "2026-10-18";
    testRem.completed = false;

    // 1. Add
    TEST_ASSERT(StorageManager::Instance().AddReminder(testRem), "AddReminder must succeed");

    // 2. Load & Verify
    auto list = StorageManager::Instance().LoadReminders();
    bool found = false;
    for (const auto& r : list) {
        if (r.id == testRem.id && r.title == testRem.title) {
            found = true;
            break;
        }
    }
    TEST_ASSERT(found, "Added reminder must be found in LoadReminders");

    // 3. Update
    testRem.completed = true;
    TEST_ASSERT(StorageManager::Instance().UpdateReminder(testRem), "UpdateReminder must succeed");

    auto list2 = StorageManager::Instance().LoadReminders();
    bool updated = false;
    for (const auto& r : list2) {
        if (r.id == testRem.id && r.completed == true) {
            updated = true;
            break;
        }
    }
    TEST_ASSERT(updated, "Updated reminder state must persist");

    // 4. Delete
    TEST_ASSERT(StorageManager::Instance().DeleteReminder(testRem.id), "DeleteReminder must succeed");

    auto list3 = StorageManager::Instance().LoadReminders();
    bool stillExists = false;
    for (const auto& r : list3) {
        if (r.id == testRem.id) {
            stillExists = true;
            break;
        }
    }
    TEST_ASSERT(!stillExists, "Deleted reminder must no longer exist");

    return true;
}

bool Test_BridgeDispatcherValidation() {
    BridgeDispatcher dispatcher(nullptr);

    // Test ping
    std::string pingResp = dispatcher.DispatchMessage(R"({"command":"ping"})");
    json jPing = json::parse(pingResp);
    TEST_ASSERT(jPing["success"] == true, "ping must succeed");
    TEST_ASSERT(jPing["message"] == "pong", "ping must return pong");

    // Test app info
    std::string infoResp = dispatcher.DispatchMessage(R"({"command":"get_app_info"})");
    json jInfo = json::parse(infoResp);
    TEST_ASSERT(jInfo["success"] == true, "get_app_info must succeed");
    TEST_ASSERT(jInfo["appName"] == "K2 Dynamic Island", "appName must be K2 Dynamic Island");

    // Test invalid command
    std::string badResp = dispatcher.DispatchMessage(R"({"command":"malicious_exec_shell"})");
    json jBad = json::parse(badResp);
    TEST_ASSERT(jBad["success"] == false, "Unknown command must fail");
    TEST_ASSERT(jBad.contains("error"), "Must contain error message");

    // Test malformed JSON
    std::string malformed = dispatcher.DispatchMessage("{ not_valid_json: 123 }");
    json jMal = json::parse(malformed);
    TEST_ASSERT(jMal["success"] == false, "Malformed JSON must fail safely");

    // Test delete reminder without confirmation rejected
    std::string delNoConfirm = dispatcher.DispatchMessage(R"({"command":"delete_reminder", "id":"123", "confirmed":false})");
    json jDel = json::parse(delNoConfirm);
    TEST_ASSERT(jDel["success"] == false, "Delete without confirmation must be rejected");

    return true;
}

bool Test_SystemStats() {
    SystemMetrics metrics = SystemStats::Instance().GetCurrentMetrics();
    TEST_ASSERT(metrics.cpuPercent >= 0.0 && metrics.cpuPercent <= 100.0, "CPU percent must be between 0 and 100");
    TEST_ASSERT(metrics.ramPercent >= 0 && metrics.ramPercent <= 100, "RAM percent must be between 0 and 100");
    TEST_ASSERT(metrics.ramTotalMb > 0, "Total RAM must be greater than 0");
    return true;
}

bool Test_MediaController() {
    MediaStatus status = MediaController::Instance().GetStatus();
    std::cout << "\n   --> [STATUS] hasActiveSession=" << (status.hasActiveSession ? "true" : "false")
              << ", isPlaying=" << (status.isPlaying ? "true" : "false")
              << ", appName='" << status.appName << "'"
              << ", title='" << status.title << "'"
              << ", peakLevel=" << status.peakLevel << std::endl;
    TEST_ASSERT(!status.statusText.empty(), "Media statusText must be populated");
    TEST_ASSERT(MediaController::Instance().PlayPause(), "PlayPause command must execute successfully");
    TEST_ASSERT(MediaController::Instance().NextTrack(), "NextTrack command must execute successfully");
    TEST_ASSERT(MediaController::Instance().PrevTrack(), "PrevTrack command must execute successfully");
    TEST_ASSERT(MediaController::Instance().ToggleMute(), "ToggleMute command must execute successfully");
    return true;
}

bool Test_DropTargetOLE() {
    bool dropInvoked = false;
    bool dragState = false;

    DropTarget* pTarget = new DropTarget(
        [&dropInvoked](HDROP) { dropInvoked = true; },
        [&dragState](bool state) { dragState = state; }
    );

    // 1. QueryInterface
    IDropTarget* pDT = nullptr;
    HRESULT hrQI = pTarget->QueryInterface(IID_IDropTarget, (void**)&pDT);
    TEST_ASSERT(SUCCEEDED(hrQI) && pDT != nullptr, "DropTarget must implement IDropTarget");
    pDT->Release();

    // 2. DragEnter with null data object
    DWORD effect = 0;
    POINTL pt = { 0, 0 };
    pTarget->DragEnter(nullptr, 0, pt, &effect);
    TEST_ASSERT(effect == DROPEFFECT_NONE, "DragEnter with null data must yield DROPEFFECT_NONE");

    // 3. DragLeave
    pTarget->DragLeave();
    TEST_ASSERT(!dragState, "DragLeave must reset drag state");

    pTarget->Release();
    return true;
}

bool Test_UpdateSemVerComparison() {
    // 1. Clean tag
    TEST_ASSERT(UpdateManager::CleanVersionTag("v1.0.1") == "1.0.1", "v prefix should be stripped");
    TEST_ASSERT(UpdateManager::CleanVersionTag("  1.2.3  ") == "1.2.3", "Whitespace should be trimmed");

    // 2. Newer versions
    TEST_ASSERT(UpdateManager::IsVersionNewer("1.0.1", "1.0.0"), "1.0.1 must be newer than 1.0.0");
    TEST_ASSERT(UpdateManager::IsVersionNewer("1.1.0", "1.0.9"), "1.1.0 must be newer than 1.0.9");
    TEST_ASSERT(UpdateManager::IsVersionNewer("2.0.0", "1.99.99"), "2.0.0 must be newer than 1.99.99");
    TEST_ASSERT(UpdateManager::IsVersionNewer("v1.0.5", "1.0.0"), "v1.0.5 must be newer than 1.0.0");

    // 3. Same or older versions
    TEST_ASSERT(!UpdateManager::IsVersionNewer("1.0.0", "1.0.0"), "Same version must not be newer");
    TEST_ASSERT(!UpdateManager::IsVersionNewer("0.9.9", "1.0.0"), "Older version must not be newer");
    TEST_ASSERT(!UpdateManager::IsVersionNewer("1.0.0", "1.0.1"), "1.0.0 must not be newer than 1.0.1");

    return true;
}

bool Test_UpdateUrlSecurity() {
    // 1. Whitelisted HTTPS GitHub domains
    TEST_ASSERT(UpdateManager::ValidateHttpsUrl("https://github.com/KimSokun/K2DynamicIsland/releases/download/v1.0.1/K2DynamicIsland-Setup.exe"), "Valid GitHub release URL should pass");
    TEST_ASSERT(UpdateManager::ValidateHttpsUrl("https://api.github.com/repos/KimSokun/K2DynamicIsland/releases"), "GitHub API URL should pass");
    TEST_ASSERT(UpdateManager::ValidateHttpsUrl("https://objects.githubusercontent.com/github-production-release-asset/installer.exe"), "GitHub usercontent URL should pass");
    TEST_ASSERT(UpdateManager::ValidateHttpsUrl("https://kimsokun.github.io/downloads/K2DynamicIsland-Setup.exe"), "Valid GitHub Pages public download URL should pass");
    TEST_ASSERT(UpdateManager::ValidateHttpsUrl("https://k2dynamicisland.github.io/downloads/K2DynamicIsland-Setup.exe"), "Valid K2DynamicIsland GitHub Pages URL should pass");

    // 2. Insecure or untrusted URLs MUST fail
    TEST_ASSERT(!UpdateManager::ValidateHttpsUrl("http://github.com/insecure.exe"), "Plain HTTP must be rejected");
    TEST_ASSERT(!UpdateManager::ValidateHttpsUrl("https://evil-hacker.com/malicious.exe"), "Untrusted external domains must be rejected");
    TEST_ASSERT(!UpdateManager::ValidateHttpsUrl("ftp://github.com/file.exe"), "FTP URLs must be rejected");
    TEST_ASSERT(!UpdateManager::ValidateHttpsUrl(""), "Empty URL must be rejected");

    return true;
}

bool Test_UpdateSignatureVerificationSecurity() {
    // 1. Check non-existent file
    std::string err;
    TEST_ASSERT(!UpdateManager::VerifyInstallerSignature(L"C:\\non_existent_installer_12345.exe", err), "Non-existent file must fail verification");
    TEST_ASSERT(!err.empty(), "Error message must be populated on failure");

    // 2. Unsigned binary test: Create a temporary unsigned .exe file
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring dummyFile = std::wstring(tempPath) + L"K2_unsigned_test.exe";
    std::ofstream out(dummyFile.c_str(), std::ios::binary);
    out << "MZ\x90\x00\x03\x00\x00\x00\x04\x00\x00\x00\xFF\xFF\x00\x00" << "Dummy unsigned executable";
    out.close();

    bool unsignedResult = UpdateManager::VerifyInstallerSignature(dummyFile, err);
    DeleteFileW(dummyFile.c_str());

    TEST_ASSERT(!unsignedResult, "Unsigned binary MUST fail Authenticode verification");
    TEST_ASSERT(!err.empty(), "Error detail must be provided for unsigned rejection");

    // 3. Trusted Authenticode binary test: Verify that valid Authenticode signed binary succeeds
    wchar_t curDir[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, curDir);
    std::wstring signedFile = std::wstring(curDir) + L"\\packages\\build\\native\\x64\\WebView2Loader.dll";
    if (GetFileAttributesW(signedFile.c_str()) == INVALID_FILE_ATTRIBUTES) {
        signedFile = std::wstring(curDir) + L"\\..\\packages\\build\\native\\x64\\WebView2Loader.dll";
    }

    if (GetFileAttributesW(signedFile.c_str()) != INVALID_FILE_ATTRIBUTES) {
        std::string signedErr;
        bool sysResult = UpdateManager::VerifyInstallerSignature(signedFile, signedErr);
        if (!sysResult) {
            std::cout << "\n   --> [SIG-DEBUG] WebView2Loader.dll error: " << signedErr << std::endl;
        }
        TEST_ASSERT(sysResult, "Valid Microsoft Authenticode signed binary should pass verification");
    }

    return true;
}

bool Test_UpdateSettingsPersistence() {
    AppSettings s = StorageManager::Instance().LoadSettings();
    bool prevAutoCheck = s.autoCheckUpdates;
    bool prevAutoDownload = s.autoDownloadUpdates;

    s.autoCheckUpdates = !prevAutoCheck;
    s.autoDownloadUpdates = !prevAutoDownload;
    s.lastUpdateCheck = "2026-10-18T20:00:00Z";

    TEST_ASSERT(StorageManager::Instance().SaveSettings(s), "Saving update settings must succeed");

    AppSettings reloaded = StorageManager::Instance().LoadSettings();
    TEST_ASSERT(reloaded.autoCheckUpdates == !prevAutoCheck, "autoCheckUpdates must persist");
    TEST_ASSERT(reloaded.autoDownloadUpdates == !prevAutoDownload, "autoDownloadUpdates must persist");
    TEST_ASSERT(reloaded.lastUpdateCheck == "2026-10-18T20:00:00Z", "lastUpdateCheck must persist");

    // Restore original
    s.autoCheckUpdates = prevAutoCheck;
    s.autoDownloadUpdates = prevAutoDownload;
    StorageManager::Instance().SaveSettings(s);
    return true;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  K2 Dynamic Island - Unit Test Suite   " << std::endl;
    std::cout << "========================================" << std::endl;

    int passed = 0;
    int failed = 0;

    RUN_TEST(Test_PathValidation);
    RUN_TEST(Test_ClipboardCopy);
    RUN_TEST(Test_SettingsPersistence);
    RUN_TEST(Test_ReminderCRUD);
    RUN_TEST(Test_BridgeDispatcherValidation);
    RUN_TEST(Test_SystemStats);
    RUN_TEST(Test_MediaController);
    RUN_TEST(Test_DropTargetOLE);
    RUN_TEST(Test_UpdateSemVerComparison);
    RUN_TEST(Test_UpdateUrlSecurity);
    RUN_TEST(Test_UpdateSignatureVerificationSecurity);
    RUN_TEST(Test_UpdateSettingsPersistence);

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Total: " << (passed + failed) << " | Passed: " << passed << " | Failed: " << failed << std::endl;
    std::cout << "========================================" << std::endl;

    return (failed == 0) ? 0 : 1;
}
