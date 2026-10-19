#include "media_controller.h"
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audiopolicy.h>
#include <psapi.h>
#include <shlwapi.h>
#include "logger.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>

#ifndef __IAudioMeterInformation_INTERFACE_DEFINED__
#define __IAudioMeterInformation_INTERFACE_DEFINED__
DEFINE_GUID(IID_IAudioMeterInformation, 0xc02216f6, 0x8c67, 0x4b5b, 0x9d, 0x00, 0xd0, 0x08, 0xe7, 0x3e, 0x00, 0x64);
MIDL_INTERFACE("C02216F6-8C67-4B5B-9D00-D008E73E0064")
IAudioMeterInformation : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetPeakValue(FLOAT *pfPeak) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMeteringChannelCount(UINT *pnChannelCount) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetChannelsPeakValues(UINT32 u32ChannelCount, FLOAT *afPeakValues) = 0;
    virtual HRESULT STDMETHODCALLTYPE QueryHardwareSupport(DWORD *pdwHardwareSupportMask) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IAudioMeterInformation, 0xc02216f6, 0x8c67, 0x4b5b, 0x9d, 0x00, 0xd0, 0x08, 0xe7, 0x3e, 0x00, 0x64)
#endif
#endif

namespace K2 {

MediaController& MediaController::Instance() {
    static MediaController instance;
    return instance;
}

MediaController::MediaController() {
    UpdateStatusInternal();
    m_running.store(true);
    m_workerThread = std::thread(&MediaController::WorkerLoop, this);
}

MediaController::~MediaController() {
    m_running.store(false);
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

void MediaController::WorkerLoop() {
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    bool needUninit = SUCCEEDED(hr);

    while (m_running.load()) {
        UpdateStatusInternal();

        for (int i = 0; i < 35 && m_running.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    if (needUninit) {
        CoUninitialize();
    }
}

MediaStatus MediaController::GetStatus() {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    return m_cachedStatus;
}

void MediaController::SendMediaKey(BYTE vkCode) {
    DWORD cmd = 0;
    if (vkCode == VK_MEDIA_PLAY_PAUSE) cmd = APPCOMMAND_MEDIA_PLAY_PAUSE;
    else if (vkCode == VK_MEDIA_NEXT_TRACK) cmd = APPCOMMAND_MEDIA_NEXTTRACK;
    else if (vkCode == VK_MEDIA_PREV_TRACK) cmd = APPCOMMAND_MEDIA_PREVIOUSTRACK;
    else if (vkCode == VK_VOLUME_MUTE) cmd = APPCOMMAND_VOLUME_MUTE;

    if (cmd != 0 && m_cachedMediaHwnd && IsWindow(m_cachedMediaHwnd)) {
        PostMessageW(m_cachedMediaHwnd, WM_APPCOMMAND, 0, MAKELPARAM(0, cmd));
    }

    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = vkCode;
    inputs[0].ki.dwFlags = KEYEVENTF_EXTENDEDKEY;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = vkCode;
    inputs[1].ki.dwFlags = KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP;

    SendInput(2, inputs, sizeof(INPUT));
}

bool MediaController::PlayPause() {
    SendMediaKey(VK_MEDIA_PLAY_PAUSE);
    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_cachedStatus.isPlaying = !m_cachedStatus.isPlaying;
        if (m_cachedStatus.isPlaying) {
            m_cachedStatus.statusText = "Playing in " + (m_cachedStatus.appName.empty() ? "System Audio" : m_cachedStatus.appName);
            m_lastAudioPeakTick = GetTickCount64();
        } else {
            m_cachedStatus.statusText = (m_cachedStatus.appName.empty() ? "Audio" : m_cachedStatus.appName) + " (Paused)";
            m_lastAudioPeakTick = 0;
        }
    }
    return true;
}

bool MediaController::NextTrack() {
    SendMediaKey(VK_MEDIA_NEXT_TRACK);
    return true;
}

bool MediaController::PrevTrack() {
    SendMediaKey(VK_MEDIA_PREV_TRACK);
    return true;
}

bool MediaController::ToggleMute() {
    bool toggled = false;
    bool newMuteState = false;

    IMMDeviceEnumerator* pEnumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);

    if (SUCCEEDED(hr) && pEnumerator) {
        IMMDevice* pDevice = nullptr;
        hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
        if (SUCCEEDED(hr) && pDevice) {
            IAudioEndpointVolume* pEndpointVolume = nullptr;
            hr = pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&pEndpointVolume);
            if (SUCCEEDED(hr) && pEndpointVolume) {
                BOOL isMuted = FALSE;
                if (SUCCEEDED(pEndpointVolume->GetMute(&isMuted))) {
                    BOOL nextMute = !isMuted;
                    if (SUCCEEDED(pEndpointVolume->SetMute(nextMute, NULL))) {
                        newMuteState = (nextMute == TRUE);
                        toggled = true;
                    }
                }
                pEndpointVolume->Release();
            }
            pDevice->Release();
        }
        pEnumerator->Release();
    }

    if (!toggled) {
        SendMediaKey(VK_VOLUME_MUTE);
        return true;
    }

    return true;
}

struct WindowContext {
    DWORD pid = 0;
    std::wstring title = L"";
};

static BOOL CALLBACK EnumMediaWindowsProc(HWND hWnd, LPARAM lParam) {
    auto* ctx = reinterpret_cast<WindowContext*>(lParam);
    DWORD wndPid = 0;
    GetWindowThreadProcessId(hWnd, &wndPid);
    if (wndPid == ctx->pid && IsWindowVisible(hWnd)) {
        wchar_t buf[512] = {};
        int len = GetWindowTextW(hWnd, buf, 512);
        if (len > 0) {
            std::wstring ws(buf);
            if (ws != L"Default IME" && ws != L"MSCTFIME UI" && ws != L"Chrome Legacy Window" && ws.find(L"GDI+") == std::wstring::npos) {
                ctx->title = ws;
                return FALSE; // Stop enumeration once main title is found
            }
        }
    }
    return TRUE;
}

static std::string Utf16ToUtf8Str(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    if (len <= 1) return "";
    std::vector<char> buf(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, buf.data(), len, NULL, NULL);
    return std::string(buf.data());
}

static void ExtractTrackMetadata(const std::string& appExe, const std::wstring& wndTitle, MediaStatus& status) {
    std::string titleUtf8 = Utf16ToUtf8Str(wndTitle);
    std::string exeLower = appExe;
    std::transform(exeLower.begin(), exeLower.end(), exeLower.begin(), ::tolower);

    std::string appDisplay = appExe;
    if (appDisplay.size() > 4 && appDisplay.substr(appDisplay.size() - 4) == ".exe") {
        appDisplay = appDisplay.substr(0, appDisplay.size() - 4);
    }
    if (!appDisplay.empty()) {
        appDisplay[0] = (char)::toupper(appDisplay[0]);
    }

    if (exeLower.find("spotify") != std::string::npos) {
        appDisplay = "Spotify";
        if (!titleUtf8.empty() && titleUtf8 != "Spotify" && titleUtf8 != "Spotify Free" && titleUtf8 != "Spotify Premium") {
            size_t dash = titleUtf8.find(" - ");
            if (dash != std::string::npos) {
                status.artist = titleUtf8.substr(0, dash);
                status.title = titleUtf8.substr(dash + 3);
            } else {
                status.title = titleUtf8;
            }
        } else {
            status.title = "Spotify Music";
        }
    } else if (exeLower.find("chrome") != std::string::npos || exeLower.find("msedge") != std::string::npos ||
               exeLower.find("firefox") != std::string::npos || exeLower.find("brave") != std::string::npos) {
        if (titleUtf8.find(" - YouTube") != std::string::npos) {
            appDisplay = "YouTube";
            size_t ytPos = titleUtf8.find(" - YouTube");
            std::string raw = titleUtf8.substr(0, ytPos);
            size_t dash = raw.find(" - ");
            if (dash != std::string::npos) {
                status.artist = raw.substr(0, dash);
                status.title = raw.substr(dash + 3);
            } else {
                status.title = raw;
            }
        } else if (titleUtf8.find("SoundCloud") != std::string::npos) {
            appDisplay = "SoundCloud";
            status.title = titleUtf8;
        } else if (!titleUtf8.empty()) {
            size_t lastDash = titleUtf8.rfind(" - ");
            if (lastDash != std::string::npos && lastDash > 0) {
                status.title = titleUtf8.substr(0, lastDash);
            } else {
                status.title = titleUtf8;
            }
        }
    } else if (exeLower.find("vlc") != std::string::npos) {
        appDisplay = "VLC";
        size_t vlcPos = titleUtf8.find(" - VLC media player");
        if (vlcPos != std::string::npos) {
            status.title = titleUtf8.substr(0, vlcPos);
        } else {
            status.title = titleUtf8.empty() ? "VLC Media Player" : titleUtf8;
        }
    } else {
        if (!titleUtf8.empty()) {
            size_t dash = titleUtf8.find(" - ");
            if (dash != std::string::npos) {
                status.artist = titleUtf8.substr(0, dash);
                status.title = titleUtf8.substr(dash + 3);
            } else {
                status.title = titleUtf8;
            }
        }
    }

    if (status.title.empty()) {
        status.title = appDisplay + " Audio";
    }
    status.appName = appDisplay;
}

struct MediaWindowHunt {
    HWND hWnd = nullptr;
    std::string appName = "";
    std::string title = "";
    std::string artist = "";
    bool found = false;
};

static BOOL CALLBACK HuntMediaWindowsProc(HWND hWnd, LPARAM lParam) {
    auto* hunt = reinterpret_cast<MediaWindowHunt*>(lParam);
    if (!IsWindowVisible(hWnd)) return TRUE;

    wchar_t buf[512] = {};
    int len = GetWindowTextW(hWnd, buf, 512);
    if (len <= 0) return TRUE;

    std::wstring ws(buf);
    std::string titleUtf8 = Utf16ToUtf8Str(ws);

    // Check Spotify: "Artist - Track"
    wchar_t cls[256] = {};
    GetClassNameW(hWnd, cls, 256);
    std::wstring clsName(cls);

    if (clsName == L"Chrome_WidgetWin_0" && titleUtf8.find(" - ") != std::string::npos && 
        titleUtf8 != "Spotify Free" && titleUtf8 != "Spotify Premium" && titleUtf8 != "Spotify") {
        DWORD pid = 0;
        GetWindowThreadProcessId(hWnd, &pid);
        HANDLE hP = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hP) {
            wchar_t pPath[MAX_PATH] = {};
            DWORD sz = MAX_PATH;
            if (QueryFullProcessImageNameW(hP, 0, pPath, &sz)) {
                std::wstring exeName = PathFindFileNameW(pPath);
                if (_wcsicmp(exeName.c_str(), L"spotify.exe") == 0) {
                    hunt->hWnd = hWnd;
                    hunt->appName = "Spotify";
                    size_t dash = titleUtf8.find(" - ");
                    hunt->artist = titleUtf8.substr(0, dash);
                    hunt->title = titleUtf8.substr(dash + 3);
                    hunt->found = true;
                    CloseHandle(hP);
                    return FALSE;
                }
            }
            CloseHandle(hP);
        }
    }

    // Check YouTube in browser window
    if (titleUtf8.find("YouTube") != std::string::npos) {
        hunt->hWnd = hWnd;
        hunt->appName = "YouTube";
        size_t ytPos = titleUtf8.find(" - YouTube");
        if (ytPos != std::string::npos) {
            std::string raw = titleUtf8.substr(0, ytPos);
            // Strip leading notification indicator e.g. "(1) " or "(9+) "
            if (raw.size() > 3 && raw[0] == '(' && raw.find(") ") != std::string::npos) {
                raw = raw.substr(raw.find(") ") + 2);
            }
            size_t dash = raw.find(" - ");
            if (dash != std::string::npos) {
                hunt->artist = raw.substr(0, dash);
                hunt->title = raw.substr(dash + 3);
            } else {
                hunt->title = raw;
            }
        } else {
            // Window title is e.g. "YouTube - Google Chrome"
            size_t bDash = titleUtf8.find(" - ");
            if (bDash != std::string::npos && bDash > 0) {
                std::string raw = titleUtf8.substr(0, bDash);
                if (raw != "YouTube") {
                    hunt->title = raw;
                } else {
                    hunt->title = "YouTube Video";
                }
            } else {
                hunt->title = "YouTube Video";
            }
        }
        if (hunt->title.empty()) hunt->title = "YouTube Video";
        hunt->hWnd = hWnd;
        hunt->found = true;
        return FALSE;
    }

    return TRUE;
}

void MediaController::UpdateStatusInternal() {
    MediaStatus status;
    bool hasActiveStreamingSession = false;

    IMMDeviceEnumerator* pEnumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);

    if (SUCCEEDED(hr) && pEnumerator) {
        IMMDevice* pDevice = nullptr;
        // Check default multimedia playback endpoint first, fallback to eConsole
        hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
        if (FAILED(hr) || !pDevice) {
            hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
        }

        if (SUCCEEDED(hr) && pDevice) {
            // 1. Mute status
            IAudioEndpointVolume* pEndpointVolume = nullptr;
            if (SUCCEEDED(pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&pEndpointVolume)) && pEndpointVolume) {
                BOOL isMuted = FALSE;
                if (SUCCEEDED(pEndpointVolume->GetMute(&isMuted))) {
                    status.isMuted = (isMuted == TRUE);
                }
                pEndpointVolume->Release();
            }

            // 2. Real-time audio meter on default endpoint
            IAudioMeterInformation* pMeterInfo = nullptr;
            if (SUCCEEDED(pDevice->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL, NULL, (void**)&pMeterInfo)) && pMeterInfo) {
                float peak = 0.0f;
                if (SUCCEEDED(pMeterInfo->GetPeakValue(&peak))) {
                    status.peakLevel = peak;
                }
                pMeterInfo->Release();
            }

            // 3. Audio sessions inspection
            IAudioSessionManager2* pSessionManager = nullptr;
            hr = pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, NULL, (void**)&pSessionManager);
            if (SUCCEEDED(hr) && pSessionManager) {
                IAudioSessionEnumerator* pSessionList = nullptr;
                hr = pSessionManager->GetSessionEnumerator(&pSessionList);
                if (SUCCEEDED(hr) && pSessionList) {
                    int sessionCount = 0;
                    pSessionList->GetCount(&sessionCount);

                    for (int i = 0; i < sessionCount; ++i) {
                        IAudioSessionControl* pSessionControl = nullptr;
                        if (SUCCEEDED(pSessionList->GetSession(i, &pSessionControl)) && pSessionControl) {
                            // Check per-session audio meter
                            IAudioMeterInformation* pSMeter = nullptr;
                            if (SUCCEEDED(pSessionControl->QueryInterface(__uuidof(IAudioMeterInformation), (void**)&pSMeter)) && pSMeter) {
                                float sPeak = 0.0f;
                                if (SUCCEEDED(pSMeter->GetPeakValue(&sPeak)) && sPeak > status.peakLevel) {
                                    status.peakLevel = sPeak;
                                }
                                pSMeter->Release();
                            }

                            AudioSessionState state;
                            if (SUCCEEDED(pSessionControl->GetState(&state)) && state == AudioSessionStateActive) {
                                hasActiveStreamingSession = true;
                                DWORD pid = 0;
                                std::string rawExeName = "";
                                std::wstring wndTitle = L"";

                                IAudioSessionControl2* pSessionControl2 = nullptr;
                                if (SUCCEEDED(pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pSessionControl2)) && pSessionControl2) {
                                    if (SUCCEEDED(pSessionControl2->GetProcessId(&pid)) && pid != 0) {
                                        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
                                        if (hProcess) {
                                            wchar_t processPath[MAX_PATH] = {};
                                            DWORD size = MAX_PATH;
                                            if (QueryFullProcessImageNameW(hProcess, 0, processPath, &size)) {
                                                wchar_t* fileName = PathFindFileNameW(processPath);
                                                if (fileName && fileName[0] != L'\0') {
                                                    rawExeName = Utf16ToUtf8Str(fileName);
                                                }
                                            }
                                            CloseHandle(hProcess);
                                        }

                                        // Enumerate windows for this PID
                                        WindowContext winCtx;
                                        winCtx.pid = pid;
                                        EnumWindows(EnumMediaWindowsProc, reinterpret_cast<LPARAM>(&winCtx));
                                        wndTitle = winCtx.title;
                                    }
                                    pSessionControl2->Release();
                                }

                                std::string exeLower = rawExeName;
                                std::transform(exeLower.begin(), exeLower.end(), exeLower.begin(), ::tolower);
                                bool isBrowser = (exeLower.find("chrome") != std::string::npos || 
                                                  exeLower.find("msedge") != std::string::npos ||
                                                  exeLower.find("firefox") != std::string::npos || 
                                                  exeLower.find("brave") != std::string::npos);

                                if (!isBrowser || !wndTitle.empty()) {
                                    status.hasActiveSession = true;
                                    ExtractTrackMetadata(rawExeName, wndTitle, status);
                                }
                            }
                            pSessionControl->Release();
                        }
                    }
                    pSessionList->Release();
                }
                pSessionManager->Release();
            }
            pDevice->Release();
        }

        // Only scan secondary audio endpoints if peak is still 0
        if (status.peakLevel <= 0.0001f) {
            IMMDeviceCollection* pCol = nullptr;
            if (SUCCEEDED(pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCol)) && pCol) {
                UINT devCount = 0;
                pCol->GetCount(&devCount);
                for (UINT d = 0; d < devCount; ++d) {
                    IMMDevice* pDevItem = nullptr;
                    if (SUCCEEDED(pCol->Item(d, &pDevItem)) && pDevItem) {
                        IAudioMeterInformation* pM = nullptr;
                        if (SUCCEEDED(pDevItem->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL, NULL, (void**)&pM)) && pM) {
                            float pVal = 0.0f;
                            if (SUCCEEDED(pM->GetPeakValue(&pVal)) && pVal > status.peakLevel) {
                                status.peakLevel = pVal;
                            }
                            pM->Release();
                        }
                        pDevItem->Release();
                    }
                }
                pCol->Release();
            }
        }

        pEnumerator->Release();
    }

    // High-performance cached check for active YouTube or Spotify windows
    bool huntFound = false;
    std::string huntApp = "";
    std::string huntTitle = "";
    std::string huntArtist = "";

    if (m_cachedMediaHwnd && IsWindow(m_cachedMediaHwnd) && IsWindowVisible(m_cachedMediaHwnd)) {
        wchar_t buf[512] = {};
        int len = GetWindowTextW(m_cachedMediaHwnd, buf, 512);
        if (len > 0) {
            std::string titleUtf8 = Utf16ToUtf8Str(std::wstring(buf));
            if (m_cachedAppName == "Spotify") {
                if (titleUtf8.find(" - ") != std::string::npos &&
                    titleUtf8 != "Spotify Free" && titleUtf8 != "Spotify Premium" && titleUtf8 != "Spotify") {
                    huntFound = true;
                    huntApp = "Spotify";
                    size_t dash = titleUtf8.find(" - ");
                    huntArtist = titleUtf8.substr(0, dash);
                    huntTitle = titleUtf8.substr(dash + 3);
                }
            } else if (m_cachedAppName == "YouTube") {
                if (titleUtf8.find("YouTube") != std::string::npos) {
                    huntFound = true;
                    huntApp = "YouTube";
                    size_t ytPos = titleUtf8.find(" - YouTube");
                    if (ytPos != std::string::npos) {
                        std::string raw = titleUtf8.substr(0, ytPos);
                        if (raw.size() > 3 && raw[0] == '(' && raw.find(") ") != std::string::npos) {
                            raw = raw.substr(raw.find(") ") + 2);
                        }
                        size_t dash = raw.find(" - ");
                        if (dash != std::string::npos) {
                            huntArtist = raw.substr(0, dash);
                            huntTitle = raw.substr(dash + 3);
                        } else {
                            huntTitle = raw;
                        }
                    } else {
                        size_t bDash = titleUtf8.find(" - ");
                        if (bDash != std::string::npos && bDash > 0) {
                            std::string raw = titleUtf8.substr(0, bDash);
                            huntTitle = (raw != "YouTube") ? raw : "YouTube Video";
                        } else {
                            huntTitle = "YouTube Video";
                        }
                    }
                }
            }
        }
        if (!huntFound) {
            m_cachedMediaHwnd = nullptr;
        }
    }

    ULONGLONG now = GetTickCount64();
    // If not found in cache, only run full OS EnumWindows hunt if sound is active or throttled interval (2.5s)
    if (!huntFound && (status.peakLevel > 0.0005f || (now - m_lastFullHuntTick >= 2500))) {
        MediaWindowHunt hunt;
        EnumWindows(HuntMediaWindowsProc, reinterpret_cast<LPARAM>(&hunt));
        m_lastFullHuntTick = now;
        if (hunt.found) {
            m_cachedMediaHwnd = hunt.hWnd;
            m_cachedAppName = hunt.appName;
            huntFound = true;
            huntApp = hunt.appName;
            huntTitle = hunt.title;
            huntArtist = hunt.artist;
        }
    }

    if (huntFound) {
        status.hasActiveSession = true;
        status.appName = huntApp;
        if (!huntTitle.empty()) status.title = huntTitle;
        if (!huntArtist.empty()) status.artist = huntArtist;
    }

    // Determine actual playback status with sensitive audio threshold and active session detection
    if (status.peakLevel > 0.0001f || hasActiveStreamingSession) {
        m_lastAudioPeakTick = now;
        status.isPlaying = true;
        if (!status.hasActiveSession) {
            status.hasActiveSession = true;
            status.appName = "Audio";
            status.title = "System Audio";
        }
    } else {
        // Retain playing state for 2.5s to prevent brief silent gap flickering
        if (now - m_lastAudioPeakTick < 2500) {
            status.isPlaying = true;
        } else {
            status.isPlaying = false;
        }
    }

    if (status.isPlaying) {
        status.statusText = "Playing in " + (status.appName.empty() ? "System Audio" : status.appName);
    } else if (status.hasActiveSession) {
        status.statusText = (status.appName.empty() ? "Audio" : status.appName) + " (Paused)";
    } else {
        status.statusText = "Idle";
    }

    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_cachedStatus = status;
    }
}

} // namespace K2
