#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audiopolicy.h>
#include <psapi.h>
#include <shlwapi.h>
#include <iostream>

int main() {
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    IMMDeviceEnumerator* pEnum = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnum);
    if (FAILED(hr) || !pEnum) {
        std::cout << "CoCreateInstance failed\n";
        return 1;
    }

    ERole roles[] = { eConsole, eMultimedia, eCommunications };
    const char* roleNames[] = { "eConsole", "eMultimedia", "eCommunications" };

    for (int r = 0; r < 3; ++r) {
        IMMDevice* pDev = nullptr;
        hr = pEnum->GetDefaultAudioEndpoint(eRender, roles[r], &pDev);
        std::cout << "Role " << roleNames[r] << ": " << (SUCCEEDED(hr) ? "FOUND" : "NOT FOUND") << "\n";
        if (SUCCEEDED(hr) && pDev) {
            LPWSTR devId = nullptr;
            pDev->GetId(&devId);
            if (devId) {
                std::wcout << L"  Device ID: " << devId << L"\n";
                CoTaskMemFree(devId);
            }

            IAudioSessionManager2* pMgr = nullptr;
            hr = pDev->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, NULL, (void**)&pMgr);
            if (SUCCEEDED(hr) && pMgr) {
                IAudioSessionEnumerator* pList = nullptr;
                hr = pMgr->GetSessionEnumerator(&pList);
                if (SUCCEEDED(hr) && pList) {
                    int count = 0;
                    pList->GetCount(&count);
                    std::cout << "  Session count: " << count << "\n";
                    for (int i = 0; i < count; ++i) {
                        IAudioSessionControl* pCtrl = nullptr;
                        if (SUCCEEDED(pList->GetSession(i, &pCtrl)) && pCtrl) {
                            AudioSessionState state;
                            pCtrl->GetState(&state);
                            std::cout << "    Session " << i << " state=" << state << "\n";

                            IAudioSessionControl2* pCtrl2 = nullptr;
                            if (SUCCEEDED(pCtrl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pCtrl2)) && pCtrl2) {
                                DWORD pid = 0;
                                pCtrl2->GetProcessId(&pid);
                                std::cout << "      PID=" << pid << "\n";
                                if (pid != 0) {
                                    HANDLE hP = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
                                    if (hP) {
                                        wchar_t pPath[MAX_PATH] = {};
                                        DWORD sz = MAX_PATH;
                                        if (QueryFullProcessImageNameW(hP, 0, pPath, &sz)) {
                                            std::wcout << L"      Process: " << PathFindFileNameW(pPath) << L"\n";
                                        }
                                        CloseHandle(hP);
                                    }
                                }
                                pCtrl2->Release();
                            }
                            pCtrl->Release();
                        }
                    }
                    pList->Release();
                }
                pMgr->Release();
            }
            pDev->Release();
        }
    }
    pEnum->Release();
    CoUninitialize();
    return 0;
}
