#pragma once

#include <string>
#include <vector>
#include <windows.h>

namespace K2 {

struct AppSettings {
    bool alwaysOnTop = true;
    bool reducedMotion = false;
    bool launchAtStartup = false;
    int compactWidth = 290;
    int compactHeight = 44;
    int expandedWidth = 480;
    int expandedHeight = 360;
    int positionX = -1; // -1 means default center-top
    int positionY = -1;
    std::string activeTab = "files";
    std::string themeMode = "dark";
    std::string backgroundType = "glass";
    std::string customBackground = "";
    std::string islandAnimation = "spiderverse";
    bool autoCheckUpdates = true;
    bool autoDownloadUpdates = true;
    std::string lastUpdateCheck = "";
};

struct Reminder {
    std::string id;
    std::string title;
    std::string time;
    std::string date;
    std::string category;
    bool completed = false;
    std::string createdAt;
};

class StorageManager {
public:
    static StorageManager& Instance();

    std::wstring GetAppDataDirectory() const;
    std::wstring GetSettingsFilePath() const;
    std::wstring GetRemindersFilePath() const;
    std::wstring GetWallpaperFilePath() const;

    AppSettings LoadSettings();
    bool SaveSettings(const AppSettings& settings);

    std::vector<Reminder> LoadReminders();
    bool SaveReminders(const std::vector<Reminder>& reminders);
    bool AddReminder(const Reminder& reminder);
    bool UpdateReminder(const Reminder& reminder);
    bool DeleteReminder(const std::string& id);

    bool SetLaunchAtStartup(bool enable);
    bool IsLaunchAtStartupEnabled() const;

private:
    StorageManager();
    void EnsureDirectoryExists() const;

    mutable std::wstring m_appDataDir;
};

} // namespace K2
