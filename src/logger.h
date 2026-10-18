#pragma once
#include <windows.h>
#include <fstream>
#include <string>
#include <sstream>

namespace K2 {
    inline void Log(const std::string& msg) {
        wchar_t tempDir[MAX_PATH];
        if (GetTempPathW(MAX_PATH, tempDir) > 0) {
            std::wstring logFilePath = std::wstring(tempDir) + L"K2DynamicIsland_debug.log";
            std::ofstream f(logFilePath.c_str(), std::ios::app);
            if (f.is_open()) {
                f << "[" << GetTickCount64() << "] " << msg << std::endl;
            }
        }
    }
}
