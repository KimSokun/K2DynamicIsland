#pragma once

#include <windows.h>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>

namespace K2 {

struct MediaStatus {
    bool hasActiveSession = false;
    bool isPlaying = false;
    bool isMuted = false;
    float peakLevel = 0.0f;
    std::string title = "";
    std::string artist = "";
    std::string appName = "";
    std::string statusText = "No active media session";
};

class MediaController {
public:
    static MediaController& Instance();
    ~MediaController();

    // Instant thread-safe status retrieval (< 0.01ms) - zero UI blocking
    MediaStatus GetStatus();
    bool PlayPause();
    bool NextTrack();
    bool PrevTrack();
    bool ToggleMute();

private:
    MediaController();
    void SendMediaKey(BYTE vkCode);
    void WorkerLoop();
    void UpdateStatusInternal();

    std::atomic<bool> m_running{true};
    std::thread m_workerThread;
    std::mutex m_statusMutex;
    MediaStatus m_cachedStatus;

    ULONGLONG m_lastAudioPeakTick = 0;
    HWND m_cachedMediaHwnd = nullptr;
    std::string m_cachedAppName = "";
    std::string m_cachedTitle = "";
    std::string m_cachedArtist = "";
    ULONGLONG m_lastFullHuntTick = 0;
};

} // namespace K2
