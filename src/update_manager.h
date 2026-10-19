#pragma once

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <windows.h>
#include "json.hpp"

namespace K2 {

enum class UpdateState {
    Idle,
    Checking,
    UpdateAvailable,
    NoUpdate,
    Downloading,
    ReadyToInstall,
    Error
};

struct UpdateInfo {
    std::string currentVersion = "1.1.0";
    std::string latestVersion = "";
    std::string releaseNotes = "";
    std::string downloadUrl = "";
    std::string assetName = "";
    unsigned long long assetSize = 0;
    std::string publishedAt = "";
};

struct UpdateProgress {
    UpdateState state = UpdateState::Idle;
    int percent = 0;
    unsigned long long bytesReceived = 0;
    unsigned long long totalBytes = 0;
    std::string message = "";
    std::string error = "";
};

class UpdateManager {
public:
    static UpdateManager& Instance();

    void Initialize(std::function<void(const std::string& event, const nlohmann::json& data)> eventCallback);
    void Shutdown();

    // User & Automatic Actions
    void CheckForUpdates(bool manual = false);
    void DownloadUpdate();
    bool InstallUpdate(bool silent = false);
    void DismissUpdate();

    // Status Queries
    UpdateState GetState() const;
    UpdateInfo GetUpdateInfo() const;
    UpdateProgress GetProgress() const;
    nlohmann::json GetStatusJson() const;

    // Static Utilities for SemVer and Security Validation
    static bool IsVersionNewer(const std::string& remoteVersion, const std::string& currentVersion);
    static std::string CleanVersionTag(const std::string& tag);
    static bool ValidateHttpsUrl(const std::string& url);
    static bool VerifyInstallerSignature(const std::wstring& filePath, std::string& errorDetail);

private:
    UpdateManager();
    ~UpdateManager();

    void WorkerCheckForUpdates(bool manual);
    void WorkerDownloadUpdate(std::string url, std::wstring targetPath);

    std::wstring GetUpdateDownloadDirectory() const;
    void CleanupOldDownloads();

    mutable std::mutex m_mutex;
    std::atomic<UpdateState> m_state{UpdateState::Idle};
    UpdateInfo m_info;
    UpdateProgress m_progress;

    std::function<void(const std::string& event, const nlohmann::json& data)> m_eventCallback;
    std::thread m_workerThread;
    std::atomic<bool> m_abortDownload{false};
    std::wstring m_verifiedInstallerPath;

    ULONGLONG m_lastCheckTick = 0;
    const std::string m_appVersion = "1.1.0";
    const std::string m_repoOwner = "K2DynamicIsland";
    const std::string m_repoName = "K2DynamicIsland.github.io";
};

} // namespace K2
