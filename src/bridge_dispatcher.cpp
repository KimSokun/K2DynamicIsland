#include "bridge_dispatcher.h"
#include "app_window.h"
#include "storage_manager.h"
#include "system_stats.h"
#include "media_controller.h"
#include "update_manager.h"
#include "logger.h"
#include <shlobj.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <iostream>
#include <fstream>
#include <algorithm>

using json = nlohmann::json;

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

static std::string Base64Encode(const std::vector<unsigned char>& data) {
    static const char s_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    encoded.reserve(((data.size() + 2) / 3) * 4);
    for (size_t i = 0; i < data.size(); i += 3) {
        uint32_t b = ((uint32_t)data[i] << 16) |
                     ((i + 1 < data.size() ? (uint32_t)data[i + 1] : 0) << 8) |
                     (i + 2 < data.size() ? (uint32_t)data[i + 2] : 0);
        encoded.push_back(s_table[(b >> 18) & 0x3F]);
        encoded.push_back(s_table[(b >> 12) & 0x3F]);
        encoded.push_back((i + 1 < data.size()) ? s_table[(b >> 6) & 0x3F] : '=');
        encoded.push_back((i + 2 < data.size()) ? s_table[b & 0x3F] : '=');
    }
    return encoded;
}

static std::string ReadFileAsDataUrl(const std::wstring& path) {
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f.is_open()) return "";
    f.seekg(0, std::ios::end);
    size_t size = (size_t)f.tellg();
    f.seekg(0, std::ios::beg);
    if (size == 0 || size > 50 * 1024 * 1024) return ""; // limit 50MB
    std::vector<unsigned char> buf(size);
    f.read(reinterpret_cast<char*>(buf.data()), size);

    std::wstring ext = PathFindExtensionW(path.c_str());
    std::string mime = "image/png";
    if (_wcsicmp(ext.c_str(), L".gif") == 0) mime = "image/gif";
    else if (_wcsicmp(ext.c_str(), L".jpg") == 0 || _wcsicmp(ext.c_str(), L".jpeg") == 0) mime = "image/jpeg";
    else if (_wcsicmp(ext.c_str(), L".webp") == 0) mime = "image/webp";
    else if (_wcsicmp(ext.c_str(), L".bmp") == 0) mime = "image/bmp";
    else if (_wcsicmp(ext.c_str(), L".svg") == 0) mime = "image/svg+xml";

    return "data:" + mime + ";base64," + Base64Encode(buf);
}

BridgeDispatcher::BridgeDispatcher(IWindowController* window)
    : m_window(window) {
}

bool BridgeDispatcher::ValidatePathExists(const std::wstring& path) {
    if (path.empty()) return false;
    DWORD attrs = GetFileAttributesW(path.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES);
}

bool BridgeDispatcher::CopyTextToClipboard(const std::wstring& text) {
    if (text.empty()) return false;
    if (!OpenClipboard(NULL)) return false;

    EmptyClipboard();
    size_t bytes = (text.length() + 1) * sizeof(wchar_t);
    HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!hGlob) {
        CloseClipboard();
        return false;
    }

    void* pMem = GlobalLock(hGlob);
    if (!pMem) {
        GlobalFree(hGlob);
        CloseClipboard();
        return false;
    }

    memcpy(pMem, text.c_str(), bytes);
    GlobalUnlock(hGlob);

    SetClipboardData(CF_UNICODETEXT, hGlob);
    CloseClipboard();
    return true;
}

bool BridgeDispatcher::OpenContainingFolder(const std::wstring& path) {
    if (!ValidatePathExists(path)) return false;

    DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) return false;

    if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
        // It's a directory, open it directly
        HINSTANCE res = ShellExecuteW(NULL, L"open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
        return ((INT_PTR)res > 32);
    } else {
        // It's a file, select it in Explorer
        std::wstring params = L"/select,\"" + path + L"\"";
        HINSTANCE res = ShellExecuteW(NULL, L"open", L"explorer.exe", params.c_str(), NULL, SW_SHOWNORMAL);
        return ((INT_PTR)res > 32);
    }
}

bool BridgeDispatcher::SafeCopyFile(const std::wstring& source, const std::wstring& destFolder, std::string& outError) {
    if (!ValidatePathExists(source)) {
        outError = "Source file does not exist.";
        return false;
    }

    if (!ValidatePathExists(destFolder)) {
        outError = "Destination folder does not exist.";
        return false;
    }

    DWORD destAttrs = GetFileAttributesW(destFolder.c_str());
    if (!(destAttrs & FILE_ATTRIBUTE_DIRECTORY)) {
        outError = "Destination is not a directory.";
        return false;
    }

    const wchar_t* fileName = PathFindFileNameW(source.c_str());
    std::wstring targetPath = destFolder;
    if (!targetPath.empty() && targetPath.back() != L'\\' && targetPath.back() != L'/') {
        targetPath += L'\\';
    }
    targetPath += fileName;

    // Check if destination file already exists - NEVER overwrite silently
    if (ValidatePathExists(targetPath)) {
        outError = "Target file already exists. Overwrite rejected.";
        return false;
    }

    // Copy without overwrite
    if (!CopyFileW(source.c_str(), targetPath.c_str(), TRUE)) {
        outError = "Copy operation failed (Error code: " + std::to_string(GetLastError()) + ")";
        return false;
    }

    return true;
}

std::string BridgeDispatcher::DispatchMessage(const std::string& rawJson) {
    K2::Log("DispatchMessage: " + rawJson);
    json response;
    response["success"] = false;

    try {
        json req = json::parse(rawJson);
        std::string command = req.value("command", "");
        std::string reqId = req.value("id", "");
        if (!reqId.empty()) {
            response["id"] = reqId;
        }

        if (command == "ping") {
            response["success"] = true;
            response["message"] = "pong";
        }
        else if (command == "get_app_info") {
            response["success"] = true;
            response["appName"] = "K2 Dynamic Island";
            response["version"] = "1.1.0";
            response["arch"] = "x64";
            response["dataDirectory"] = Utf16ToUtf8(StorageManager::Instance().GetAppDataDirectory());
        }
        else if (command == "resize_window") {
            int width = req.value("width", 290);
            int height = req.value("height", 44);
            // Strict bounds validation
            width = std::clamp(width, 240, 900);
            height = std::clamp(height, 40, 700);
            if (m_window) {
                m_window->SetWindowSize(width, height);
                response["success"] = true;
            }
        }
        else if (command == "drag_window") {
            if (m_window) {
                m_window->StartWindowDrag();
                response["success"] = true;
            }
        }
        else if (command == "copy_path") {
            std::string pathUtf8 = req.value("path", "");
            std::wstring pathW = Utf8ToUtf16(pathUtf8);
            if (!pathW.empty() && CopyTextToClipboard(pathW)) {
                response["success"] = true;
            } else {
                response["error"] = "Failed to copy to clipboard";
            }
        }
        else if (command == "open_folder") {
            std::string pathUtf8 = req.value("path", "");
            std::wstring pathW = Utf8ToUtf16(pathUtf8);
            if (OpenContainingFolder(pathW)) {
                response["success"] = true;
            } else {
                response["error"] = "File or folder does not exist.";
            }
        }
        else if (command == "copy_file") {
            std::string srcUtf8 = req.value("source", "");
            std::string destUtf8 = req.value("destination", "");
            std::string err;
            if (SafeCopyFile(Utf8ToUtf16(srcUtf8), Utf8ToUtf16(destUtf8), err)) {
                response["success"] = true;
            } else {
                response["error"] = err;
            }
        }
        else if (command == "choose_image") {
            wchar_t szFile[MAX_PATH] = { 0 };
            OPENFILENAMEW ofn = { 0 };
            ofn.lStructSize = sizeof(OPENFILENAMEW);
            ofn.hwndOwner = NULL;
            ofn.lpstrFilter = L"Images & GIFs (*.gif;*.png;*.jpg;*.jpeg;*.webp)\0*.gif;*.png;*.jpg;*.jpeg;*.webp\0All Files (*.*)\0*.*\0";
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
            if (GetOpenFileNameW(&ofn)) {
                std::wstring selectedPath(szFile);
                std::string dataUrl = ReadFileAsDataUrl(selectedPath);
                if (!dataUrl.empty()) {
                    response["success"] = true;
                    response["dataUrl"] = dataUrl;
                    response["path"] = Utf16ToUtf8(selectedPath);
                } else {
                    response["error"] = "Failed to read image file";
                }
            } else {
                response["cancelled"] = true;
            }
        }
        else if (command == "read_image_base64") {
            std::string pathUtf8 = req.value("path", "");
            std::wstring pathW = Utf8ToUtf16(pathUtf8);
            std::string dataUrl = ReadFileAsDataUrl(pathW);
            if (!dataUrl.empty()) {
                response["success"] = true;
                response["dataUrl"] = dataUrl;
            } else {
                response["error"] = "Could not read image file";
            }
        }
        else if (command == "get_settings") {
            AppSettings s = StorageManager::Instance().LoadSettings();
            response["success"] = true;
            response["settings"] = {
                {"alwaysOnTop", s.alwaysOnTop},
                {"reducedMotion", s.reducedMotion},
                {"launchAtStartup", StorageManager::Instance().IsLaunchAtStartupEnabled()},
                {"compactWidth", s.compactWidth},
                {"compactHeight", s.compactHeight},
                {"expandedWidth", s.expandedWidth},
                {"expandedHeight", s.expandedHeight},
                {"activeTab", s.activeTab},
                {"themeMode", s.themeMode},
                {"backgroundType", s.backgroundType},
                {"customBackground", s.customBackground},
                {"islandAnimation", s.islandAnimation},
                {"autoCheckUpdates", s.autoCheckUpdates},
                {"autoDownloadUpdates", s.autoDownloadUpdates},
                {"lastUpdateCheck", s.lastUpdateCheck},
                {"appVersion", "1.1.0"}
            };
        }
        else if (command == "save_settings") {
            if (req.contains("settings")) {
                auto sj = req["settings"];
                AppSettings s = StorageManager::Instance().LoadSettings();
                if (sj.contains("alwaysOnTop")) {
                    s.alwaysOnTop = sj["alwaysOnTop"].get<bool>();
                    if (m_window) m_window->SetAlwaysOnTop(s.alwaysOnTop);
                }
                if (sj.contains("reducedMotion")) s.reducedMotion = sj["reducedMotion"].get<bool>();
                if (sj.contains("launchAtStartup")) {
                    s.launchAtStartup = sj["launchAtStartup"].get<bool>();
                    StorageManager::Instance().SetLaunchAtStartup(s.launchAtStartup);
                }
                if (sj.contains("activeTab")) s.activeTab = sj["activeTab"].get<std::string>();
                if (sj.contains("themeMode")) s.themeMode = sj["themeMode"].get<std::string>();
                if (sj.contains("backgroundType")) s.backgroundType = sj["backgroundType"].get<std::string>();
                if (sj.contains("customBackground")) s.customBackground = sj["customBackground"].get<std::string>();
                if (sj.contains("islandAnimation")) s.islandAnimation = sj["islandAnimation"].get<std::string>();
                if (sj.contains("autoCheckUpdates")) s.autoCheckUpdates = sj["autoCheckUpdates"].get<bool>();
                if (sj.contains("autoDownloadUpdates")) s.autoDownloadUpdates = sj["autoDownloadUpdates"].get<bool>();
                if (sj.contains("lastUpdateCheck")) s.lastUpdateCheck = sj["lastUpdateCheck"].get<std::string>();

                StorageManager::Instance().SaveSettings(s);
                response["success"] = true;
            }
        }
        else if (command == "get_reminders") {
            auto list = StorageManager::Instance().LoadReminders();
            json arr = json::array();
            for (const auto& r : list) {
                arr.push_back({
                    {"id", r.id},
                    {"title", r.title},
                    {"time", r.time},
                    {"date", r.date},
                    {"category", r.category},
                    {"completed", r.completed},
                    {"createdAt", r.createdAt}
                });
            }
            response["success"] = true;
            response["reminders"] = arr;
        }
        else if (command == "save_reminder") {
            if (req.contains("reminder")) {
                auto rj = req["reminder"];
                Reminder r;
                r.id = rj.value("id", "");
                r.title = rj.value("title", "");
                r.time = rj.value("time", "");
                r.date = rj.value("date", "");
                r.category = rj.value("category", "General");
                r.completed = rj.value("completed", false);
                r.createdAt = rj.value("createdAt", "");

                if (r.id.empty()) {
                    // Generate ID
                    r.id = "rem-" + std::to_string(GetTickCount64());
                    StorageManager::Instance().AddReminder(r);
                } else {
                    StorageManager::Instance().UpdateReminder(r);
                }
                response["success"] = true;
                response["reminderId"] = r.id;
            }
        }
        else if (command == "delete_reminder") {
            std::string remId = req.value("id", "");
            bool confirmed = req.value("confirmed", false);
            if (!confirmed) {
                response["error"] = "Deletion requires explicit confirmation.";
            } else if (!remId.empty() && StorageManager::Instance().DeleteReminder(remId)) {
                response["success"] = true;
            } else {
                response["error"] = "Reminder not found or deletion failed.";
            }
        }
        else if (command == "get_system_stats") {
            SystemMetrics m = SystemStats::Instance().GetCurrentMetrics();
            response["success"] = true;
            response["stats"] = {
                {"cpuPercent", m.cpuPercent},
                {"ramPercent", m.ramPercent},
                {"ramUsedMb", m.ramUsedMb},
                {"ramTotalMb", m.ramTotalMb},
                {"batteryPercent", m.batteryPercent},
                {"isCharging", m.isCharging},
                {"hasBattery", m.hasBattery}
            };
        }
        else if (command == "get_media_status") {
            MediaStatus s = MediaController::Instance().GetStatus();
            response["success"] = true;
            response["media"] = {
                {"hasActiveSession", s.hasActiveSession},
                {"isPlaying", s.isPlaying},
                {"isMuted", s.isMuted},
                {"peakLevel", s.peakLevel},
                {"title", s.title},
                {"artist", s.artist},
                {"appName", s.appName},
                {"statusText", s.statusText}
            };
        }
        else if (command == "media_command") {
            std::string action = req.value("action", "");
            if (action == "playPause") MediaController::Instance().PlayPause();
            else if (action == "next") MediaController::Instance().NextTrack();
            else if (action == "prev") MediaController::Instance().PrevTrack();
            else if (action == "mute") {
                bool isMuted = MediaController::Instance().ToggleMute();
                response["isMuted"] = isMuted;
            }
            response["success"] = true;
        }
        else if (command == "quick_action") {
            std::string action = req.value("action", "");
            if (action == "lock_pc") {
                LockWorkStation();
                response["success"] = true;
            } else if (action == "task_manager") {
                ShellExecuteW(NULL, L"open", L"taskmgr.exe", NULL, NULL, SW_SHOWNORMAL);
                response["success"] = true;
            } else {
                response["error"] = "Unknown quick action";
            }
        }
        else if (command == "get_update_status") {
            response["success"] = true;
            response["status"] = UpdateManager::Instance().GetStatusJson();
        }
        else if (command == "check_updates") {
            bool manual = req.value("manual", true);
            UpdateManager::Instance().CheckForUpdates(manual);
            response["success"] = true;
            response["status"] = UpdateManager::Instance().GetStatusJson();
        }
        else if (command == "download_update") {
            UpdateManager::Instance().DownloadUpdate();
            response["success"] = true;
            response["status"] = UpdateManager::Instance().GetStatusJson();
        }
        else if (command == "install_update") {
            bool silent = req.value("silent", false);
            bool ok = UpdateManager::Instance().InstallUpdate(silent);
            response["success"] = ok;
            if (!ok) {
                response["error"] = "Failed to launch installer or signature verification failed";
            }
        }
        else if (command == "dismiss_update") {
            UpdateManager::Instance().DismissUpdate();
            response["success"] = true;
        }
        else if (command == "minimize_to_tray") {
            if (m_window) {
                m_window->HideWindow();
                response["success"] = true;
            }
        }
        else if (command == "set_always_on_top") {
            bool enabled = req.value("enabled", true);
            if (m_window) {
                m_window->SetAlwaysOnTop(enabled);
                response["success"] = true;
            }
        }
        else if (command == "recenter_island") {
            if (m_window) {
                m_window->RecenterWindow();
                response["success"] = true;
            }
        }
        else if (command == "quit") {
            if (m_window) {
                m_window->CloseApp();
                response["success"] = true;
            }
        }
        else {
            response["error"] = "Unknown command: " + command;
        }
    }
    catch (const std::exception& e) {
        response["error"] = e.what();
    }
    catch (...) {
        response["error"] = "Unhandled exception processing command";
    }

    return response.dump();
}

} // namespace K2
