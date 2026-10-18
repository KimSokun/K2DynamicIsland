#pragma once

#include <windows.h>
#include <string>

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

    MediaStatus GetStatus();
    bool PlayPause();
    bool NextTrack();
    bool PrevTrack();
    bool ToggleMute();

private:
    MediaController();
    void SendMediaKey(BYTE vkCode);
    ULONGLONG m_lastAudioPeakTick = 0;
};

} // namespace K2
