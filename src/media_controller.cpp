#include "media_controller.h"
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audiopolicy.h>
#include <psapi.h>
#include <shlwapi.h>
#include <iostream>
#include <algorithm>

#ifndef __IAudioMeterInformation_INTERFACE_DEFINED__
#define __IAudioMeterInformation_INTERFACE_DEFINED__
DEFINE_GUID(IID_IAudioMeterInformation, 0xc0221673, 0x0301, 0x4b45, 0x9e,0x9a, 0xfd,0x3f,0x5d,0xe4,0x78,0x7a);
MIDL_INTERFACE("c0221673-0301-4b45-9e9a-fd3f5de4787a")
IAudioMeterInformation : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetPeakValue(FLOAT *pfPeak) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMeteringChannelCount(UINT *pnChannelCount) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetChannelsPeakValues(UINT32 u32ChannelCount, FLOAT *afPeakValues) = 0;
    virtual HRESULT STDMETHODCALLTYPE QueryHardwareSupport(DWORD *pdwHardwareSupportMask) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IAudioMeterInformation, 0xc0221673, 0x0301, 0x4b45, 0x9e,0x9a, 0xfd,0x3f,0x5d,0xe4,0x78,0x7a)
#endif
#endif

namespace K2 {

MediaController& MediaController::Instance() {
    static MediaController instance;
    return instance;
}

MediaController::MediaController() {
}

void MediaController::SendMediaKey(BYTE vkCode) {
    WORD scanCode = (WORD)MapVirtualKeyW(vkCode, MAPVK_VK_TO_VSC);

    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = vkCode;
    inputs[0].ki.wScan = scanCode;
    inputs[0].ki.dwFlags = KEYEVENTF_EXTENDEDKEY;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = vkCode;
    inputs[1].ki.wScan = scanCode;
    inputs[1].ki.dwFlags = KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP;

    SendInput(2, inputs, sizeof(INPUT));
}

bool MediaController::PlayPause() {
    SendMediaKey(VK_MEDIA_PLAY_PAUSE);
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

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    bool coInitializedByMe = SUCCEEDED(hr);

    IMMDeviceEnumerator* pEnumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
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

    if (coInitializedByMe) {
        CoUninitialize();
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
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string s(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &s[0], len, NULL, NULL);
    return s;
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
    if (titleUtf8.find(" - YouTube") != std::string::npos) {
        hunt->appName = "YouTube";
        size_t ytPos = titleUtf8.find(" - YouTube");
        std::string raw = titleUtf8.substr(0, ytPos);
        size_t dash = raw.find(" - ");
        if (dash != std::string::npos) {
            hunt->artist = raw.substr(0, dash);
            hunt->title = raw.substr(dash + 3);
        } else {
            hunt->title = raw;
        }
        hunt->found = true;
        return FALSE;
    }

    return TRUE;
}

MediaStatus MediaController::GetStatus() {
    MediaStatus status;

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    bool coInitializedByMe = SUCCEEDED(hr);

    IMMDeviceEnumerator* pEnumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);

    if (SUCCEEDED(hr) && pEnumerator) {
        IMMDevice* pDevice = nullptr;
        // Check default playback endpoint (eConsole first, fallback to eMultimedia)
        hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
        if (FAILED(hr) || !pDevice) {
            hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
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

            // 2. Real-time audio meter (detect active sound wave)
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
                            AudioSessionState state;
                            if (SUCCEEDED(pSessionControl->GetState(&state)) && state == AudioSessionStateActive) {
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
                                                int len = WideCharToMultiByte(CP_UTF8, 0, fileName, -1, NULL, 0, NULL, NULL);
                                                if (len > 0) {
                                                    std::string appNameStr(len - 1, '\0');
                                                    WideCharToMultiByte(CP_UTF8, 0, fileName, -1, &appNameStr[0], len, NULL, NULL);
                                                    rawExeName = appNameStr;
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

                                // Only accept session if it is NOT a generic silent browser session
                                if (!isBrowser || !wndTitle.empty()) {
                                    status.hasActiveSession = true;
                                    ExtractTrackMetadata(rawExeName, wndTitle, status);
                                    pSessionControl->Release();
                                    break;
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
        pEnumerator->Release();
    }

    if (coInitializedByMe) {
        CoUninitialize();
    }

    // Always hunt for active YouTube or Spotify windows to ensure precise track title & platform
    MediaWindowHunt hunt;
    EnumWindows(HuntMediaWindowsProc, reinterpret_cast<LPARAM>(&hunt));
    if (hunt.found) {
        status.hasActiveSession = true;
        status.appName = hunt.appName;
        status.title = hunt.title;
        status.artist = hunt.artist;
    }

    // Determine actual playback status from real-time audio output
    ULONGLONG now = GetTickCount64();
    if (status.peakLevel > 0.008f) {
        m_lastAudioPeakTick = now;
        status.isPlaying = true;
        if (!status.hasActiveSession) {
            status.hasActiveSession = true;
            status.appName = "Audio";
            status.title = "System Audio";
        }
    } else {
        // Bridge micro-pauses between musical notes for 800ms
        if (m_lastAudioPeakTick != 0 && (now - m_lastAudioPeakTick) < 800) {
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

    return status;
}

} // namespace K2
