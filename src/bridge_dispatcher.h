#pragma once

#include <windows.h>
#include <string>
#include <functional>
#include "json.hpp"

namespace K2 {

class IWindowController;

class BridgeDispatcher {
public:
    explicit BridgeDispatcher(IWindowController* window);

    // Dispatches a message from JavaScript. Returns response JSON string.
    std::string DispatchMessage(const std::string& rawJson);

    // Helpers to validate paths
    static bool ValidatePathExists(const std::wstring& path);
    static bool CopyTextToClipboard(const std::wstring& text);
    static bool OpenContainingFolder(const std::wstring& path);
    static bool SafeCopyFile(const std::wstring& source, const std::wstring& destFolder, std::string& outError);

private:
    IWindowController* m_window;
};

} // namespace K2
