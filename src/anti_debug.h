#pragma once
#include <windows.h>
#include <thread>
#include <atomic>
#include <chrono>

namespace K2 {

class SecurityGuard {
public:
    static SecurityGuard& Instance() {
        static SecurityGuard instance;
        return instance;
    }

    // Direct Anti-Debug checks
    static bool IsDebugged() {
        // 1. Standard Win32 API Check
        if (IsDebuggerPresent()) return true;

        // 2. Remote Debugger Attached Check
        BOOL isRemoteDebugger = FALSE;
        if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &isRemoteDebugger) && isRemoteDebugger) {
            return true;
        }

        // 3. Direct PEB (Process Environment Block) Query on x64 / x86
        #if defined(_M_X64) || defined(__x86_64__)
        unsigned char* ppeb = (unsigned char*)__readgsqword(0x60);
        if (ppeb) {
            // BeingDebugged flag (offset 0x02)
            if (ppeb[2] != 0) return true;
            // NtGlobalFlag (offset 0xBC on x64)
            DWORD ntGlobalFlag = *(DWORD*)(ppeb + 0xBC);
            if (ntGlobalFlag & 0x70) return true;
        }
        #elif defined(_M_IX86) || defined(__i386__)
        unsigned char* ppeb = (unsigned char*)__readfsdword(0x30);
        if (ppeb) {
            if (ppeb[2] != 0) return true;
            DWORD ntGlobalFlag = *(DWORD*)(ppeb + 0x68);
            if (ntGlobalFlag & 0x70) return true;
        }
        #endif

        // 4. Hardware Breakpoints Detection (DR0 - DR3)
        CONTEXT ctx = {};
        ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        HANDLE hThread = GetCurrentThread();
        if (GetThreadContext(hThread, &ctx)) {
            if (ctx.Dr0 != 0 || ctx.Dr1 != 0 || ctx.Dr2 != 0 || ctx.Dr3 != 0) {
                return true;
            }
        }

        return false;
    }

    // Thread Hide From Debugger (causes debuggers to fail/detach)
    static void HideCurrentThread() {
        HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
        if (hNtdll) {
            typedef NTSTATUS(NTAPI* pfnNtSetInformationThread)(
                HANDLE ThreadHandle,
                ULONG ThreadInformationClass,
                PVOID ThreadInformation,
                ULONG ThreadInformationLength
            );
            auto NtSetInformationThread = (pfnNtSetInformationThread)GetProcAddress(hNtdll, "NtSetInformationThread");
            if (NtSetInformationThread) {
                // ThreadHideFromDebugger = 0x11
                NtSetInformationThread(GetCurrentThread(), 0x11, 0, 0);
            }
        }
    }

    // Start background watchdog thread
    void StartWatchdog() {
        if (m_running.exchange(true)) return;

        HideCurrentThread();

        m_watchdog = std::thread([this]() {
            HideCurrentThread();
            while (m_running) {
                if (IsDebugged()) {
                    // Silently terminate process when reverse-engineering debugger is detected
                    ExitProcess(0);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(2500));
            }
        });
        m_watchdog.detach();
    }

    void StopWatchdog() {
        m_running = false;
    }

private:
    SecurityGuard() : m_running(false) {}
    ~SecurityGuard() { StopWatchdog(); }

    std::atomic<bool> m_running;
    std::thread m_watchdog;
};

} // namespace K2
