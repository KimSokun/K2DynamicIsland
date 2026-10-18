#include "storage_manager.h"
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <iostream>
#include "json.hpp"

using json = nlohmann::json;

namespace K2 {

StorageManager& StorageManager::Instance() {
    static StorageManager instance;
    return instance;
}

StorageManager::StorageManager() {
    EnsureDirectoryExists();
}

std::wstring StorageManager::GetAppDataDirectory() const {
    if (!m_appDataDir.empty()) {
        return m_appDataDir;
    }

    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        m_appDataDir = std::wstring(path) + L"\\K2DynamicIsland";
    } else {
        m_appDataDir = L"C:\\Users\\Default\\AppData\\Roaming\\K2DynamicIsland";
    }
    return m_appDataDir;
}

void StorageManager::EnsureDirectoryExists() const {
    std::wstring dir = GetAppDataDirectory();
    CreateDirectoryW(dir.c_str(), NULL);
}

std::wstring StorageManager::GetSettingsFilePath() const {
    return GetAppDataDirectory() + L"\\settings.json";
}

std::wstring StorageManager::GetRemindersFilePath() const {
    return GetAppDataDirectory() + L"\\reminders.json";
}

std::wstring StorageManager::GetWallpaperFilePath() const {
    return GetAppDataDirectory() + L"\\wallpaper.dat";
}

AppSettings StorageManager::LoadSettings() {
    AppSettings settings;
    EnsureDirectoryExists();
    std::wstring path = GetSettingsFilePath();

    std::ifstream file(path.c_str());
    if (!file.is_open()) {
        // Return defaults if file doesn't exist yet
        return settings;
    }

    try {
        json j;
        file >> j;
        if (j.contains("alwaysOnTop")) settings.alwaysOnTop = j["alwaysOnTop"].get<bool>();
        if (j.contains("reducedMotion")) settings.reducedMotion = j["reducedMotion"].get<bool>();
        if (j.contains("launchAtStartup")) settings.launchAtStartup = j["launchAtStartup"].get<bool>();
        if (j.contains("compactWidth")) settings.compactWidth = j["compactWidth"].get<int>();
        if (j.contains("compactHeight")) settings.compactHeight = j["compactHeight"].get<int>();
        if (j.contains("expandedWidth")) settings.expandedWidth = j["expandedWidth"].get<int>();
        if (j.contains("expandedHeight")) settings.expandedHeight = j["expandedHeight"].get<int>();
        if (j.contains("positionX")) settings.positionX = j["positionX"].get<int>();
        if (j.contains("positionY")) settings.positionY = j["positionY"].get<int>();
        if (j.contains("activeTab")) settings.activeTab = j["activeTab"].get<std::string>();
        if (j.contains("themeMode")) settings.themeMode = j["themeMode"].get<std::string>();
        if (j.contains("backgroundType")) settings.backgroundType = j["backgroundType"].get<std::string>();
        if (j.contains("customBackground")) {
            settings.customBackground = j["customBackground"].get<std::string>();
            if (settings.customBackground == "custom_wallpaper") {
                std::ifstream wfile(GetWallpaperFilePath().c_str(), std::ios::binary);
                if (wfile.is_open()) {
                    std::stringstream ss;
                    ss << wfile.rdbuf();
                    settings.customBackground = ss.str();
                }
            }
        }
        if (j.contains("islandAnimation")) settings.islandAnimation = j["islandAnimation"].get<std::string>();
        if (j.contains("autoCheckUpdates")) settings.autoCheckUpdates = j["autoCheckUpdates"].get<bool>();
        if (j.contains("autoDownloadUpdates")) settings.autoDownloadUpdates = j["autoDownloadUpdates"].get<bool>();
        if (j.contains("lastUpdateCheck")) settings.lastUpdateCheck = j["lastUpdateCheck"].get<std::string>();
    } catch (...) {
        // If parsing fails, fall back to defaults
    }

    // Enforce ergonomic capsule dimensions to prevent UI clipping
    if (settings.compactWidth < 290) settings.compactWidth = 290;
    if (settings.compactHeight < 44) settings.compactHeight = 44;
    if (settings.expandedWidth < 460) settings.expandedWidth = 480;
    if (settings.expandedHeight < 340) settings.expandedHeight = 360;

    return settings;
}

bool StorageManager::SaveSettings(const AppSettings& settings) {
    EnsureDirectoryExists();
    std::wstring path = GetSettingsFilePath();

    try {
        json j;
        j["alwaysOnTop"] = settings.alwaysOnTop;
        j["reducedMotion"] = settings.reducedMotion;
        j["launchAtStartup"] = settings.launchAtStartup;
        j["compactWidth"] = settings.compactWidth;
        j["compactHeight"] = settings.compactHeight;
        j["expandedWidth"] = settings.expandedWidth;
        j["expandedHeight"] = settings.expandedHeight;
        j["positionX"] = settings.positionX;
        j["positionY"] = settings.positionY;
        j["activeTab"] = settings.activeTab;
        j["themeMode"] = settings.themeMode;
        j["backgroundType"] = settings.backgroundType;

        std::string bgToStore = settings.customBackground;
        if (bgToStore.rfind("data:image", 0) == 0) {
            std::ofstream wfile(GetWallpaperFilePath().c_str(), std::ios::binary | std::ios::trunc);
            if (wfile.is_open()) {
                wfile << bgToStore;
            }
            bgToStore = "custom_wallpaper";
        }
        j["customBackground"] = bgToStore;
        j["islandAnimation"] = settings.islandAnimation;
        j["autoCheckUpdates"] = settings.autoCheckUpdates;
        j["autoDownloadUpdates"] = settings.autoDownloadUpdates;
        j["lastUpdateCheck"] = settings.lastUpdateCheck;

        std::ofstream file(path.c_str());
        if (!file.is_open()) return false;
        file << j.dump(4);
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<Reminder> StorageManager::LoadReminders() {
    std::vector<Reminder> list;
    EnsureDirectoryExists();
    std::wstring path = GetRemindersFilePath();

    std::ifstream file(path.c_str());
    if (!file.is_open()) {
        return list;
    }

    try {
        json j;
        file >> j;
        if (j.is_array()) {
            for (const auto& item : j) {
                Reminder r;
                if (item.contains("id")) r.id = item["id"].get<std::string>();
                if (item.contains("title")) r.title = item["title"].get<std::string>();
                if (item.contains("time")) r.time = item["time"].get<std::string>();
                if (item.contains("date")) r.date = item["date"].get<std::string>();
                if (item.contains("category")) r.category = item["category"].get<std::string>();
                if (item.contains("completed")) r.completed = item["completed"].get<bool>();
                if (item.contains("createdAt")) r.createdAt = item["createdAt"].get<std::string>();
                list.push_back(r);
            }
        }
    } catch (...) {
    }

    return list;
}

bool StorageManager::SaveReminders(const std::vector<Reminder>& reminders) {
    EnsureDirectoryExists();
    std::wstring path = GetRemindersFilePath();

    try {
        json j = json::array();
        for (const auto& r : reminders) {
            json item;
            item["id"] = r.id;
            item["title"] = r.title;
            item["time"] = r.time;
            item["date"] = r.date;
            item["category"] = r.category;
            item["completed"] = r.completed;
            item["createdAt"] = r.createdAt;
            j.push_back(item);
        }

        std::ofstream file(path.c_str());
        if (!file.is_open()) return false;
        file << j.dump(4);
        return true;
    } catch (...) {
        return false;
    }
}

bool StorageManager::AddReminder(const Reminder& reminder) {
    if (reminder.title.empty()) return false;
    auto list = LoadReminders();
    list.push_back(reminder);
    return SaveReminders(list);
}

bool StorageManager::UpdateReminder(const Reminder& reminder) {
    auto list = LoadReminders();
    bool found = false;
    for (auto& item : list) {
        if (item.id == reminder.id) {
            item = reminder;
            found = true;
            break;
        }
    }
    if (!found) return false;
    return SaveReminders(list);
}

bool StorageManager::DeleteReminder(const std::string& id) {
    auto list = LoadReminders();
    auto initialSize = list.size();
    list.erase(std::remove_if(list.begin(), list.end(), [&](const Reminder& r) {
        return r.id == id;
    }), list.end());

    if (list.size() == initialSize) return false;
    return SaveReminders(list);
}

bool StorageManager::SetLaunchAtStartup(bool enable) {
    HKEY hKey;
    const wchar_t* subKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    if (RegOpenKeyExW(HKEY_CURRENT_USER, subKey, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    const wchar_t* valName = L"K2 Dynamic Island";
    if (enable) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        std::wstring quotedPath = L"\"" + std::wstring(exePath) + L"\"";
        LONG res = RegSetValueExW(hKey, valName, 0, REG_SZ,
            (const BYTE*)quotedPath.c_str(),
            static_cast<DWORD>((quotedPath.length() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
        return (res == ERROR_SUCCESS);
    } else {
        RegDeleteValueW(hKey, valName);
        RegCloseKey(hKey);
        return true;
    }
}

bool StorageManager::IsLaunchAtStartupEnabled() const {
    HKEY hKey;
    const wchar_t* subKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    if (RegOpenKeyExW(HKEY_CURRENT_USER, subKey, 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    const wchar_t* valName = L"K2 Dynamic Island";
    DWORD type = 0;
    LONG res = RegQueryValueExW(hKey, valName, NULL, &type, NULL, NULL);
    RegCloseKey(hKey);
    return (res == ERROR_SUCCESS);
}

} // namespace K2
