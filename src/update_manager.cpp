#include "update_manager.h"
#include "storage_manager.h"
#include "logger.h"
#include <winhttp.h>
#include <wintrust.h>
#include <softpub.h>
#include <wincrypt.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <iomanip>

namespace K2 {

UpdateManager& UpdateManager::Instance() {
    static UpdateManager instance;
    return instance;
}

UpdateManager::UpdateManager() {
    m_info.currentVersion = m_appVersion;
    CleanupOldDownloads();
}

UpdateManager::~UpdateManager() {
    Shutdown();
}

void UpdateManager::Initialize(std::function<void(const std::string& event, const nlohmann::json& data)> eventCallback) {
    m_eventCallback = eventCallback;
}

void UpdateManager::Shutdown() {
    m_abortDownload = true;
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

std::string UpdateManager::CleanVersionTag(const std::string& tag) {
    std::string s = tag;
    while (!s.empty() && (s[0] == 'v' || s[0] == 'V' || s[0] == ' ' || s[0] == '\t')) {
        s.erase(0, 1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) {
        s.pop_back();
    }
    return s;
}

static bool ParseSemVer(const std::string& versionStr, int& major, int& minor, int& patch) {
    major = 0;
    minor = 0;
    patch = 0;
    std::string s = UpdateManager::CleanVersionTag(versionStr);
    size_t dash = s.find('-');
    if (dash != std::string::npos) {
        s = s.substr(0, dash);
    }

    std::stringstream ss(s);
    char dot1 = 0, dot2 = 0;
    if (ss >> major) {
        if (ss >> dot1 && dot1 == '.' && ss >> minor) {
            if (ss >> dot2 && dot2 == '.' && ss >> patch) {
                return true;
            }
            return true;
        }
        return true;
    }
    return false;
}

bool UpdateManager::IsVersionNewer(const std::string& remoteVersion, const std::string& currentVersion) {
    int rMajor = 0, rMinor = 0, rPatch = 0;
    int cMajor = 0, cMinor = 0, cPatch = 0;

    if (!ParseSemVer(remoteVersion, rMajor, rMinor, rPatch)) return false;
    if (!ParseSemVer(currentVersion, cMajor, cMinor, cPatch)) return false;

    if (rMajor > cMajor) return true;
    if (rMajor < cMajor) return false;

    if (rMinor > cMinor) return true;
    if (rMinor < cMinor) return false;

    return rPatch > cPatch;
}

bool UpdateManager::ValidateHttpsUrl(const std::string& url) {
    if (url.rfind("https://", 0) != 0) {
        return false;
    }

    std::string withoutProto = url.substr(8);
    size_t slash = withoutProto.find('/');
    std::string host = (slash != std::string::npos) ? withoutProto.substr(0, slash) : withoutProto;
    for (char& c : host) c = (char)::tolower(c);

    if (host == "github.com" || 
        host == "api.github.com" || 
        host == "objects.githubusercontent.com" ||
        (host.size() > 24 && host.substr(host.size() - 24) == ".objects.githubusercontent.com") ||
        (host.size() > 10 && host.substr(host.size() - 10) == ".github.io")) {
        return true;
    }
    return false;
}

bool UpdateManager::VerifyInstallerSignature(const std::wstring& filePath, std::string& errorDetail) {
    if (GetFileAttributesW(filePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        errorDetail = "Installer file not found on disk.";
        return false;
    }

    WINTRUST_FILE_INFO fileData = {};
    fileData.cbStruct = sizeof(WINTRUST_FILE_INFO);
    fileData.pcwszFilePath = filePath.c_str();
    fileData.hFile = NULL;
    fileData.pgKnownSubject = NULL;

    WINTRUST_DATA winTrustData = {};
    winTrustData.cbStruct = sizeof(WINTRUST_DATA);
    winTrustData.pPolicyCallbackData = NULL;
    winTrustData.pSIPClientData = NULL;
    winTrustData.dwUIChoice = WTD_UI_NONE;
    winTrustData.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN;
    winTrustData.dwUnionChoice = WTD_CHOICE_FILE;
    winTrustData.dwStateAction = WTD_STATEACTION_VERIFY;
    winTrustData.hWVTStateData = NULL;
    winTrustData.pwszURLReference = NULL;
    winTrustData.dwProvFlags = WTD_SAFER_FLAG | WTD_REVOCATION_CHECK_CHAIN;
    winTrustData.dwUIContext = 0;
    winTrustData.pFile = &fileData;

    GUID policyGuid = WINTRUST_ACTION_GENERIC_VERIFY_V2;

    LONG lStatus = WinVerifyTrust(NULL, &policyGuid, &winTrustData);

    // Close WinTrust state
    winTrustData.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(NULL, &policyGuid, &winTrustData);

    if (lStatus == ERROR_SUCCESS) {
        return true;
    }

    switch (lStatus) {
    case TRUST_E_NOSIGNATURE:
        errorDetail = "File is unsigned. No Authenticode digital signature found.";
        break;
    case TRUST_E_EXPLICIT_DISTRUST:
        errorDetail = "Digital signature is present, but publisher certificate is explicitly distrusted.";
        break;
    case TRUST_E_SUBJECT_NOT_TRUSTED:
        errorDetail = "Digital certificate subject is not trusted by the Windows trust store.";
        break;
    case TRUST_E_BAD_DIGEST:
        errorDetail = "File signature digest mismatch. The installer has been tampered with or corrupted.";
        break;
    default:
        errorDetail = "Authenticode verification rejected (Status Code: 0x" + std::to_string(lStatus) + ").";
        break;
    }

    return false;
}

std::wstring UpdateManager::GetUpdateDownloadDirectory() const {
    wchar_t tempPath[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring dir = std::wstring(tempPath) + L"K2DynamicIsland_Updates";
    CreateDirectoryW(dir.c_str(), NULL);
    return dir;
}

void UpdateManager::CleanupOldDownloads() {
    std::wstring dir = GetUpdateDownloadDirectory();
    std::wstring search = dir + L"\\*.*";

    WIN32_FIND_DATAW fd = {};
    HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                std::wstring file = dir + L"\\" + fd.cFileName;
                DeleteFileW(file.c_str());
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }
}

UpdateState UpdateManager::GetState() const {
    return m_state.load();
}

UpdateInfo UpdateManager::GetUpdateInfo() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_info;
}

UpdateProgress UpdateManager::GetProgress() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_progress;
}

nlohmann::json UpdateManager::GetStatusJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string stateStr = "idle";
    switch (m_state.load()) {
    case UpdateState::Checking: stateStr = "checking"; break;
    case UpdateState::UpdateAvailable: stateStr = "available"; break;
    case UpdateState::NoUpdate: stateStr = "no_update"; break;
    case UpdateState::Downloading: stateStr = "downloading"; break;
    case UpdateState::ReadyToInstall: stateStr = "ready_to_install"; break;
    case UpdateState::Error: stateStr = "error"; break;
    default: stateStr = "idle"; break;
    }

    return {
        {"state", stateStr},
        {"currentVersion", m_info.currentVersion},
        {"latestVersion", m_info.latestVersion},
        {"releaseNotes", m_info.releaseNotes},
        {"assetName", m_info.assetName},
        {"assetSize", m_info.assetSize},
        {"publishedAt", m_info.publishedAt},
        {"percent", m_progress.percent},
        {"bytesReceived", m_progress.bytesReceived},
        {"totalBytes", m_progress.totalBytes},
        {"message", m_progress.message},
        {"error", m_progress.error}
    };
}

void UpdateManager::CheckForUpdates(bool manual) {
    if (m_state == UpdateState::Checking || m_state == UpdateState::Downloading) {
        return;
    }

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    m_workerThread = std::thread(&UpdateManager::WorkerCheckForUpdates, this, manual);
}

void UpdateManager::WorkerCheckForUpdates(bool manual) {
    ULONGLONG now = GetTickCount64();
    if (!manual && m_lastCheckTick != 0 && (now - m_lastCheckTick) < 3600000ULL) {
        // Less than 1 hour since last auto check; skip to avoid rate limits
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = UpdateState::Checking;
        m_progress.message = "Checking GitHub Releases for updates...";
        m_progress.error = "";
    }
    if (m_eventCallback) {
        m_eventCallback("updateStatusChanged", GetStatusJson());
    }

    HINTERNET hSession = WinHttpOpen(L"K2DynamicIsland-Updater/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

    if (!hSession) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = UpdateState::Error;
        m_progress.error = "Failed to initialize WinHTTP session.";
        if (m_eventCallback) m_eventCallback("updateStatusChanged", GetStatusJson());
        return;
    }

    // Connect to GitHub API
    HINTERNET hConnect = WinHttpConnect(hSession, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = UpdateState::Error;
        m_progress.error = "Could not connect to api.github.com.";
        if (m_eventCallback) m_eventCallback("updateStatusChanged", GetStatusJson());
        return;
    }

    std::wstring requestPath = L"/repos/" + std::wstring(m_repoOwner.begin(), m_repoOwner.end()) +
                               L"/" + std::wstring(m_repoName.begin(), m_repoName.end()) +
                               L"/releases/latest";

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", requestPath.c_str(),
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = UpdateState::Error;
        m_progress.error = "Failed to create HTTP request.";
        if (m_eventCallback) m_eventCallback("updateStatusChanged", GetStatusJson());
        return;
    }

    std::wstring headers = L"User-Agent: K2DynamicIsland-Updater/1.0\r\nAccept: application/vnd.github.v3+json\r\n";
    BOOL sent = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)headers.length(), WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

    if (!sent || !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = UpdateState::Error;
        m_progress.error = "Failed to receive response from GitHub.";
        if (m_eventCallback) m_eventCallback("updateStatusChanged", GetStatusJson());
        return;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX);

    std::string jsonBody;

    if (statusCode == 200) {
        DWORD bytesAvailable = 0;
        while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
            std::vector<char> buf(bytesAvailable + 1, 0);
            DWORD bytesRead = 0;
            if (WinHttpReadData(hRequest, buf.data(), bytesAvailable, &bytesRead) && bytesRead > 0) {
                jsonBody.append(buf.data(), bytesRead);
            }
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);

    // Fallback to checking public update metadata on user's GitHub Pages site (kimsokun.github.io)
    if (jsonBody.empty() || statusCode == 404) {
        std::wstring pagesHost = L"kimsokun.github.io";
        HINTERNET hPagesConnect = WinHttpConnect(hSession, pagesHost.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (hPagesConnect) {
            HINTERNET hPagesReq = WinHttpOpenRequest(hPagesConnect, L"GET", L"/version.json",
                NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
            if (hPagesReq) {
                if (WinHttpSendRequest(hPagesReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                    WinHttpReceiveResponse(hPagesReq, NULL)) {
                    DWORD pCode = 0;
                    DWORD pSize = sizeof(pCode);
                    WinHttpQueryHeaders(hPagesReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &pCode, &pSize, WINHTTP_NO_HEADER_INDEX);
                    if (pCode == 200) {
                        DWORD avail = 0;
                        while (WinHttpQueryDataAvailable(hPagesReq, &avail) && avail > 0) {
                            std::vector<char> buf(avail + 1, 0);
                            DWORD read = 0;
                            if (WinHttpReadData(hPagesReq, buf.data(), avail, &read) && read > 0) {
                                jsonBody.append(buf.data(), read);
                            }
                        }
                        if (!jsonBody.empty()) {
                            statusCode = 200;
                        }
                    }
                }
                WinHttpCloseHandle(hPagesReq);
            }
            WinHttpCloseHandle(hPagesConnect);
        }
    }

    WinHttpCloseHandle(hSession);
    m_lastCheckTick = now;

    if (jsonBody.empty()) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = UpdateState::NoUpdate;
        m_progress.message = "You are running the latest version (v" + m_appVersion + ").";
        if (m_eventCallback) m_eventCallback("updateStatusChanged", GetStatusJson());
        return;
    }

    try {
        nlohmann::json rel = nlohmann::json::parse(jsonBody);
        std::string tag = rel.value("tag_name", "");
        if (tag.empty()) tag = rel.value("version", "");
        std::string cleanTag = CleanVersionTag(tag);
        std::string body = rel.value("body", "");
        if (body.empty()) body = rel.value("release_notes", "");
        std::string publishedAt = rel.value("published_at", "");

        std::string downloadUrl = "";
        std::string assetName = "";
        unsigned long long assetSize = 0;

        if (rel.contains("assets") && rel["assets"].is_array()) {
            for (const auto& a : rel["assets"]) {
                std::string name = a.value("name", "");
                if (name == "K2DynamicIsland-Setup.exe" || name.rfind("K2DynamicIsland-Setup", 0) == 0) {
                    downloadUrl = a.value("browser_download_url", "");
                    assetName = name;
                    assetSize = a.value("size", 0ULL);
                    break;
                }
            }
        }

        // Direct public download link fallback
        if (downloadUrl.empty() && rel.contains("download_url") && rel["download_url"].is_string()) {
            downloadUrl = rel["download_url"].get<std::string>();
            assetName = "K2DynamicIsland-Setup.exe";
            assetSize = rel.value("installer_size", 0ULL);
        }

        bool newer = IsVersionNewer(cleanTag, m_appVersion);

        std::lock_guard<std::mutex> lock(m_mutex);
        m_info.latestVersion = cleanTag;
        m_info.releaseNotes = body;
        m_info.downloadUrl = downloadUrl;
        m_info.assetName = assetName;
        m_info.assetSize = assetSize;
        m_info.publishedAt = publishedAt;

        if (newer && !downloadUrl.empty() && ValidateHttpsUrl(downloadUrl)) {
            m_state = UpdateState::UpdateAvailable;
            m_progress.message = "New stable update v" + cleanTag + " is available.";

            // Save last checked date
            AppSettings s = StorageManager::Instance().LoadSettings();
            s.lastUpdateCheck = publishedAt;
            StorageManager::Instance().SaveSettings(s);

            if (m_eventCallback) {
                m_eventCallback("updateStatusChanged", GetStatusJson());
            }

            // Auto-download if policy is enabled
            if (s.autoDownloadUpdates) {
                // Trigger background download
                std::thread(&UpdateManager::DownloadUpdate, this).detach();
            }
        } else {
            m_state = UpdateState::NoUpdate;
            m_progress.message = "You are up to date! (v" + m_appVersion + ")";
            if (m_eventCallback) {
                m_eventCallback("updateStatusChanged", GetStatusJson());
            }
        }
    } catch (...) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = UpdateState::Error;
        m_progress.error = "Failed to parse release information from GitHub.";
        if (m_eventCallback) m_eventCallback("updateStatusChanged", GetStatusJson());
    }
}

void UpdateManager::DownloadUpdate() {
    if (m_state == UpdateState::Downloading) return;

    std::string url;
    std::string ver;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_info.downloadUrl.empty()) return;
        url = m_info.downloadUrl;
        ver = m_info.latestVersion;
    }

    if (!ValidateHttpsUrl(url)) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = UpdateState::Error;
        m_progress.error = "Security Policy Violation: Insecure or untrusted download URL rejected.";
        if (m_eventCallback) m_eventCallback("updateStatusChanged", GetStatusJson());
        return;
    }

    std::wstring dir = GetUpdateDownloadDirectory();
    std::wstring targetPath = dir + L"\\K2DynamicIsland-Setup-v" + std::wstring(ver.begin(), ver.end()) + L".exe";

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    m_abortDownload = false;
    m_workerThread = std::thread(&UpdateManager::WorkerDownloadUpdate, this, url, targetPath);
}

void UpdateManager::WorkerDownloadUpdate(std::string url, std::wstring targetPath) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = UpdateState::Downloading;
        m_progress.percent = 0;
        m_progress.bytesReceived = 0;
        m_progress.totalBytes = m_info.assetSize;
        m_progress.message = "Downloading update installer...";
        m_progress.error = "";
    }
    if (m_eventCallback) m_eventCallback("updateStatusChanged", GetStatusJson());

    std::wstring tempDownloadPath = targetPath + L".download";
    HANDLE hFile = CreateFileW(tempDownloadPath.c_str(), GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = UpdateState::Error;
        m_progress.error = "Could not create temporary download file.";
        if (m_eventCallback) m_eventCallback("updateStatusChanged", GetStatusJson());
        return;
    }

    // Parse URL with WinHttpCrackUrl
    std::wstring urlW(url.begin(), url.end());
    URL_COMPONENTS urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);
    wchar_t hostName[256] = {};
    wchar_t urlPath[2048] = {};
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = 256;
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = 2048;

    if (!WinHttpCrackUrl(urlW.c_str(), (DWORD)urlW.length(), 0, &urlComp)) {
        CloseHandle(hFile);
        DeleteFileW(tempDownloadPath.c_str());
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = UpdateState::Error;
        m_progress.error = "Failed to crack download URL.";
        if (m_eventCallback) m_eventCallback("updateStatusChanged", GetStatusJson());
        return;
    }

    HINTERNET hSession = WinHttpOpen(L"K2DynamicIsland-Updater/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

    HINTERNET hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlPath, NULL,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        (urlComp.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0));

    // Follow redirects automatically (required for GitHub AWS S3 assets)
    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

    std::wstring headers = L"User-Agent: K2DynamicIsland-Updater/1.0\r\n";
    BOOL sent = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)headers.length(), WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

    if (!sent || !WinHttpReceiveResponse(hRequest, NULL)) {
        CloseHandle(hFile);
        DeleteFileW(tempDownloadPath.c_str());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = UpdateState::Error;
        m_progress.error = "Failed to connect to asset download server.";
        if (m_eventCallback) m_eventCallback("updateStatusChanged", GetStatusJson());
        return;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX);

    if (statusCode != 200) {
        CloseHandle(hFile);
        DeleteFileW(tempDownloadPath.c_str());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = UpdateState::Error;
        m_progress.error = "Asset server returned HTTP " + std::to_string(statusCode);
        if (m_eventCallback) m_eventCallback("updateStatusChanged", GetStatusJson());
        return;
    }

    DWORD contentLength = 0;
    DWORD clSize = sizeof(contentLength);
    if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &contentLength, &clSize, WINHTTP_NO_HEADER_INDEX)) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_progress.totalBytes = contentLength;
    }

    unsigned long long received = 0;
    const DWORD bufferSize = 65536;
    std::vector<char> buffer(bufferSize);
    int lastReportedPercent = -1;

    bool downloadSuccess = true;
    while (!m_abortDownload) {
        DWORD bytesAvailable = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable) || bytesAvailable == 0) {
            break; // Finished reading
        }

        DWORD toRead = (std::min)(bytesAvailable, bufferSize);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest, buffer.data(), toRead, &bytesRead) && bytesRead > 0) {
            DWORD written = 0;
            WriteFile(hFile, buffer.data(), bytesRead, &written, NULL);
            received += bytesRead;

            int percent = 0;
            if (m_progress.totalBytes > 0) {
                percent = (int)((received * 100ULL) / m_progress.totalBytes);
            }

            if (percent != lastReportedPercent && (percent % 2 == 0 || percent == 100)) {
                lastReportedPercent = percent;
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_progress.percent = percent;
                    m_progress.bytesReceived = received;
                }
                if (m_eventCallback) m_eventCallback("updateStatusChanged", GetStatusJson());
            }
        } else {
            downloadSuccess = false;
            break;
        }
    }

    CloseHandle(hFile);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (m_abortDownload || !downloadSuccess) {
        DeleteFileW(tempDownloadPath.c_str());
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = UpdateState::Idle;
        m_progress.message = "Download cancelled or interrupted.";
        if (m_eventCallback) m_eventCallback("updateStatusChanged", GetStatusJson());
        return;
    }

    // --- CRITICAL SECURITY CHECK: Authenticode Signature Verification ---
    std::string sigError;
    bool isSignatureTrusted = VerifyInstallerSignature(tempDownloadPath, sigError);

    if (!isSignatureTrusted) {
        // REJECT AND DELETE IMMEDIATELY
        DeleteFileW(tempDownloadPath.c_str());
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = UpdateState::Error;
        m_progress.error = "SECURITY REJECTION: " + sigError;
        m_progress.message = "Installer rejected due to untrusted publisher signature.";
        if (m_eventCallback) m_eventCallback("updateStatusChanged", GetStatusJson());
        return;
    }

    // Signature verified; move to final installer path
    DeleteFileW(targetPath.c_str());
    MoveFileW(tempDownloadPath.c_str(), targetPath.c_str());

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = UpdateState::ReadyToInstall;
        m_verifiedInstallerPath = targetPath;
        m_progress.percent = 100;
        m_progress.message = "Update v" + m_info.latestVersion + " verified and ready to install.";
    }
    if (m_eventCallback) m_eventCallback("updateStatusChanged", GetStatusJson());
}

bool UpdateManager::InstallUpdate(bool silent) {
    std::wstring installerPath;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_state != UpdateState::ReadyToInstall || m_verifiedInstallerPath.empty()) {
            return false;
        }
        installerPath = m_verifiedInstallerPath;
    }

    // Pre-execution integrity check
    std::string sigErr;
    if (!VerifyInstallerSignature(installerPath, sigErr)) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = UpdateState::Error;
        m_progress.error = "Pre-execution signature check failed: " + sigErr;
        DeleteFileW(installerPath.c_str());
        if (m_eventCallback) m_eventCallback("updateStatusChanged", GetStatusJson());
        return false;
    }

    std::wstring params = L"--recenter";
    if (silent) {
        params += L" /S";
    }

    // Standard Windows UAC elevation flow
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd = NULL;
    sei.lpVerb = L"runas"; // Elevate with UAC
    sei.lpFile = installerPath.c_str();
    sei.lpParameters = params.c_str();
    sei.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&sei)) {
        DWORD err = GetLastError();
        if (err == ERROR_CANCELLED) {
            // User rejected UAC dialog
            std::lock_guard<std::mutex> lock(m_mutex);
            m_progress.message = "Installation cancelled by user.";
            if (m_eventCallback) m_eventCallback("updateStatusChanged", GetStatusJson());
            return false;
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = UpdateState::Error;
        m_progress.error = "Failed to launch installer (Windows Error: " + std::to_string(err) + ")";
        if (m_eventCallback) m_eventCallback("updateStatusChanged", GetStatusJson());
        return false;
    }

    if (sei.hProcess) {
        CloseHandle(sei.hProcess);
    }

    // Gracefully terminate this instance so installer can update the files
    HWND hWnd = FindWindowW(L"K2DynamicIslandWindowClass", L"K2 Dynamic Island");
    if (hWnd) {
        PostMessageW(hWnd, WM_CLOSE, 0, 0);
    } else {
        ExitProcess(0);
    }

    return true;
}

void UpdateManager::DismissUpdate() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != UpdateState::Downloading) {
        m_state = UpdateState::Idle;
        m_progress.message = "";
        m_progress.error = "";
    }
    if (m_eventCallback) m_eventCallback("updateStatusChanged", GetStatusJson());
}

} // namespace K2
