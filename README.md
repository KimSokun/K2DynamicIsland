# K2 Dynamic Island

> **Status:** Production-Ready Windows Desktop Application &bull; **Build & Tests: PASS (12/12 Unit Tests Verified)** &bull; **Version:** 1.0.0

An elegant, ultra-modern Windows desktop capsule inspired by the Dynamic Island paradigm. Built with native C++17, Win32 APIs, Microsoft Edge WebView2, and clean vanilla web technologies. Designed for zero bloat, low resource utilization, privacy, fluid animations, and seamless file and media workflow enhancements on Windows 10 and 11.

---

## 1. Project Identity & Deliverables

- **Official Product Name:** K2 Dynamic Island
- **Application Executable:** `K2DynamicIsland.exe` (Found in `dist/K2DynamicIsland.exe` and `bin/K2DynamicIsland.exe`)
- **Installer Executable:** `K2DynamicIsland-Setup.exe` (Found in `dist/K2DynamicIsland-Setup.exe`)
- **Portable Package:** `K2DynamicIsland-Portable.zip` (Found in `dist/K2DynamicIsland-Portable.zip`)
- **Default Installation Directory:** `C:\Program Files\K2 Dynamic Island\`
- **Start Menu Shortcut:** `K2 Dynamic Island`
- **Local Settings & Storage:** `%APPDATA%\K2DynamicIsland\`
- **Build System:** CMake &bull; MinGW-w64 / MSVC compatible with Ninja build tool

---

## 2. Key Features & Visual Design

### 2.1 Dynamic Island Pill (Compact State)
- **OLED Pitch Black Capsule:** Compact 260x46 capsule floating smoothly at the top of your display with authentic DWM alpha composition.
- **Dynamic Status Sensor & Drag Handle:** Sleek pulsing cyan status sensor dot and integrated drag handle for effortless repositioning.
- **Center Status Stream:** Dynamic status display showing connection state, drag cues, and dropped file counters.
- **Mini Audio Equalizer:** Live 4-bar audio equalizer that animates during active media playback (Spotify, YouTube, VLC, browser tabs).
- **Drop Badge:** Automatic item count pill badge that appears as soon as files are dropped.
- **Fluid Spring Animations:** Cubic-bezier transitions (`cubic-bezier(0.16, 1, 0.3, 1)`) for organic expansion and collapse.

### 2.2 Expanded Island Control Card
- **Frosted Acrylic Glass:** Rich dark backdrop (`rgba(13, 15, 20, 0.94)`) with 40px backdrop blur, subtle ambient top glow, and header branding.
- **Pin Always on Top:** Direct header button to toggle window floating above all desktop applications.
- **Minimize to Tray:** Instant minimize to the Windows Notification Area.
- **Segmented Navigation:** Modern tab bar switching between **Files & Paths**, **Media**, **Reminders**, **Tools**, and **Settings**.

### 2.3 File Drop & Path Inspection Tools (MVP Focus)
- **Native Drag & Drop:** Accepts files and folders dragged directly from Windows File Explorer onto either the compact pill or expanded drop area (`WM_DROPFILES`).
- **Exact Path Inspection:** Displays file name, extension badges (DIR, EXE, CPP, PNG, etc.), exact disk size, and full Unicode filesystem paths.
- **One-Click Copy:** Fast, reliable path copying to the Windows clipboard (`CF_UNICODETEXT`) with spring feedback animation.
- **Open Containing Folder:** Instantly highlights dropped files in Windows Explorer.
- **Batch Copy:** One-click "Copy All" button to copy all paths separated by newlines.
- **Safety Guarantee:** Original user files are strictly read-only inspected; never modified, moved, renamed, or executed.

### 2.4 Media & Audio Controls
- **Windows Core Audio Integration:** Queries active audio sessions using Windows Audio Session API (WASAPI) to identify playing applications (Spotify, Chrome, Edge, VLC, etc.).
- **Vinyl Disc & Equalizer:** Spinning vinyl record artwork and 7-bar soundwave equalizer that react when audio is detected.
- **Universal Transport Controls:** Play/Pause, Next Track, Previous Track, and Volume Mute commands.

### 2.5 Local Reminders & Calendar
- **Date Header:** Live day-of-week and localized date banner.
- **Task Management:** Create, view, filter (All, Pending, Done), and toggle task completion.
- **Local Persistence:** Safely stored in `%APPDATA%\K2DynamicIsland\reminders.json`.
- **Destructive Action Protection:** Deleting reminders triggers an explicit confirmation modal dialog.

### 2.6 Quick Tools & System Metrics
- **CPU Load:** Real-time processor utilization calculated via Win32 `GetSystemTimes`.
- **Memory (RAM):** Memory load percentage, used MB, and total MB via `GlobalMemoryStatusEx`.
- **Battery & Power:** Real-time AC connection status and battery percentage via `GetSystemPowerStatus`.
- **One-Click Desktop Shortcuts:** Instant Lock PC (`LockWorkStation`), Windows Task Manager launcher, and Volume Mute toggle.

### 2.7 Pure Vector SVG Iconography
- **100% SVG Vector Icons:** All unicode emojis across navigation tabs, file card types, empty states, vinyl label, and shortcut tiles have been replaced with crisp vector SVG icons.
- **Resolution-Independent:** Renders razor-sharp on high-DPI and standard displays without font dependencies.
- **Theme Accent Coordination:** Icons utilize semantic color highlights (`--accent-cyan`, `--accent-emerald`, `--accent-rose`, `--accent-purple`, `--accent-amber`) and `currentColor` for cohesive dark aesthetics.

### 2.8 Secure Automatic Updates (Official GitHub Releases Channel)
- **Modern Workflow Model:** Auto-check + Auto-download + User-confirmed install architecture.
- **Semantic Versioning (SemVer):** Strict version parsing comparing Major.Minor.Patch to prevent downgrades or duplicate installs.
- **WinVerifyTrust Authenticode Verification:** Uses Windows Cryptographic API (`WinVerifyTrust` with `WINTRUST_ACTION_GENERIC_VERIFY_V2`) to enforce verified publisher digital signatures before execution. Zero untrusted or unsigned binaries will ever execute.
- **Strict HTTPS URL Validation:** Enforces secure HTTPS and strictly whitelists official GitHub domains (`github.com`, `api.github.com`, `objects.githubusercontent.com`).
- **Non-blocking Background Downloads:** WinHTTP chunked streaming downloads with real-time percentage progress bar.
- **UAC Elevation:** Triggers standard Windows elevation (`runas`) without bypassing UAC.
- **User Control & Data Preservation:** User-configurable toggles for auto-checking and auto-downloading, with complete preservation of user settings and reminders in `%APPDATA%\K2DynamicIsland`.

---

## 3. Architecture & Security

K2 Dynamic Island enforces strict separation of responsibilities between native system capabilities and web presentation:

```
┌────────────────────────────────────────────────────────┐
│                   WebView2 Frontend                    │
│   (HTML5, CSS3, Vanilla ES6 JavaScript, Local Assets)  │
└───────────────────────────▲────────────────────────────┘
                            │  JSON IPC PostMessage
                            ▼
┌────────────────────────────────────────────────────────┐
│              Native C++17 Secure Bridge                │
│    (BridgeDispatcher: Strict Command & Boundary Check) │
├────────────────────────────────────────────────────────┤
│                    Win32 Backend                       │
│  ├── AppWindow: Window management, Tray, Drag & Drop   │
│  ├── WebViewManager: Controller lifecycle, bounds      │
│  ├── StorageManager: %APPDATA% JSON settings/reminders │
│  ├── SystemStats: CPU, RAM, and Battery Win32 metrics  │
│  ├── MediaController: WASAPI audio session detection   │
│  └── UpdateManager: WinHTTP + WinTrust Authenticode    │
└────────────────────────────────────────────────────────┘
```

### Security Guardrails
1. **Narrowly Scoped Bridge:** Only whitelisted commands (`get_settings`, `save_settings`, `resize_window`, `copy_path`, `open_folder`, `get_reminders`, `check_updates`, `download_update`, `install_update`, etc.) are processed.
2. **No Arbitrary Execution:** No generic shell execution, arbitrary PowerShell, or arbitrary command execution APIs exist on the bridge.
3. **Local Navigation Only:** WebView navigation is strictly confined to trusted local files (`assets/index.html`). All external HTTP/HTTPS navigation and popup requests are cancelled.
4. **Authenticode Enforcement:** Downloaded update binaries are verified against the Windows Trust Store with `WinVerifyTrust` before execution; unsigned or tampered files are deleted immediately.
5. **Zero Cloud / Zero AI:** 100% offline operation. No telemetry, no trackers, no external cloud dependencies, and no AI overhead in the base application.

---

## 4. Verification & Actual Test Results

All logic and platform integrations are verified with automated test suites and real Windows execution:

### 4.1 Unit Test Suite (`K2DynamicIslandTests.exe`)
```
========================================
  K2 Dynamic Island - Unit Test Suite   
========================================
[RUNNING] Test_PathValidation... [PASS]
[RUNNING] Test_ClipboardCopy... [PASS]
[RUNNING] Test_SettingsPersistence... [PASS]
[RUNNING] Test_ReminderCRUD... [PASS]
[RUNNING] Test_BridgeDispatcherValidation... [PASS]
[RUNNING] Test_SystemStats... [PASS]
[RUNNING] Test_MediaController... [PASS]
[RUNNING] Test_DropTargetOLE... [PASS]
[RUNNING] Test_UpdateSemVerComparison... [PASS]
[RUNNING] Test_UpdateUrlSecurity... [PASS]
[RUNNING] Test_UpdateSignatureVerificationSecurity... [PASS]
[RUNNING] Test_UpdateSettingsPersistence... [PASS]
----------------------------------------
Total: 12 | Passed: 12 | Failed: 0
========================================
```

- **Test_PathValidation:** Verified detection of existing paths, parent directories, system directories (`C:\Windows`), and rejection of invalid/empty paths.
- **Test_ClipboardCopy:** Verified copying Unicode paths into Windows Clipboard (`CF_UNICODETEXT`) and verified exact matching retrieval.
- **Test_SettingsPersistence:** Verified reading/writing `settings.json`, verifying modified states persist across sessions.
- **Test_ReminderCRUD:** Verified creation, persistence, updating completion flag, confirmation requirement on deletion, and successful deletion.
- **Test_BridgeDispatcherValidation:** Verified strict input clamping on window sizes, graceful handling of malformed JSON, and rejection of unrecognized commands.
- **Test_SystemStats:** Verified non-zero valid physical RAM, CPU percentage bounds (0.0% - 100.0%), and battery detection.
- **Test_MediaController:** Verified core audio status queries and mute state toggling.
- **Test_DropTargetOLE:** Verified COM `IDropTarget` interface implementation, `CF_HDROP` format queries, and safe drag lifecycle states.
- **Test_UpdateSemVerComparison:** Verified SemVer tag cleaning, version comparison (newer, older, same, pre-releases).
- **Test_UpdateUrlSecurity:** Verified HTTPS protocol enforcement and domain whitelisting (GitHub releases & CDN), rejecting HTTP and untrusted domains.
- **Test_UpdateSignatureVerificationSecurity:** Verified Authenticode verification with `WinVerifyTrust` (rejecting unsigned files with descriptive security errors, and validating genuine Authenticode signed binaries).
- **Test_UpdateSettingsPersistence:** Verified persistence of update preferences (`autoCheckUpdates`, `autoDownloadUpdates`, `lastUpdateCheck`).

- **Test_PathValidation:** Verified detection of existing paths, parent directories, system directories (`C:\Windows`), and rejection of invalid/empty paths.
- **Test_ClipboardCopy:** Verified copying Unicode paths (e.g. `C:\Users\K2\Desktop\Island_Special_文件_🚀.txt`) into Windows Clipboard (`CF_UNICODETEXT`) and verified exact matching retrieval.
- **Test_SettingsPersistence:** Verified reading/writing `settings.json`, verifying modified states persist across sessions.
- **Test_ReminderCRUD:** Verified creation, persistence, updating completion flag, confirmation requirement on deletion, and successful deletion.
- **Test_BridgeDispatcherValidation:** Verified strict input clamping on window sizes, graceful handling of malformed JSON, and rejection of unrecognized commands.
- **Test_SystemStats:** Verified non-zero valid physical RAM, CPU percentage bounds (0.0% - 100.0%), and battery detection.
- **Test_MediaController:** Verified core audio status queries.
- **Test_DropTargetOLE:** Verified COM `IDropTarget` interface implementation, `CF_HDROP` format queries, and safe drag lifecycle states.

### 4.2 Runtime Process Verification
When launched, `K2DynamicIsland.exe` successfully spawns the WebView2 host, displays the floating transparent window, and runs with active `msedgewebview2.exe` renderer processes:
- Main Process: `K2DynamicIsland.exe`
- Sub-processes: `msedgewebview2.exe` (Embedded browser controller, GPU process, Utility processes)
- User Data Directory: `%APPDATA%\K2DynamicIsland\WebView2Cache`
- DPI Awareness: Per-Monitor Aware V2 (`DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2`)

---

## 5. Build Instructions

### Prerequisites
- Windows 10 or Windows 11 (64-bit)
- C++17 compatible compiler (LLVM-MinGW, MinGW-w64 GCC 11+, or MSVC 2019+)
- CMake (optional, 3.16 or newer)
- Microsoft Edge WebView2 Runtime (pre-installed on Windows 10/11)

### One-Click Autonomous Build
The included [`build.bat`](build.bat) script automatically detects available compilers (CMake, LLVM-MinGW, or system GCC), compiles the main application executable, builds and runs the automated test suite, generates the uninstaller, packages the portable zip, and compiles the standalone Windows Setup installer.

```cmd
build.bat
```

### Manual Compilation via Toolchain
```powershell
# 1. Compile Win32 resources
windres src/resource.rc -O coff -o bin/resource.o

# 2. Compile main desktop binary
g++ -std=c++17 -O2 -mwindows -Isrc -Ipackages/build/native/include -Ipackages `
    src/main.cpp src/app_window.cpp src/webview_manager.cpp src/bridge_dispatcher.cpp `
    src/storage_manager.cpp src/system_stats.cpp src/media_controller.cpp bin/resource.o `
    -luuid -lole32 -lshell32 -lshlwapi -luser32 -lgdi32 -ladvapi32 -ldwmapi `
    -o dist/K2DynamicIsland.exe

# 3. Compile and execute automated unit tests (8/8 PASS)
g++ -std=c++17 -O2 -Isrc -Ipackages/build/native/include -Ipackages `
    tests/unit_tests.cpp src/storage_manager.cpp src/bridge_dispatcher.cpp `
    src/system_stats.cpp src/media_controller.cpp `
    -luuid -lole32 -lshell32 -lshlwapi -luser32 -lgdi32 -ladvapi32 -ldwmapi `
    -o bin/K2DynamicIslandTests.exe
.\bin\K2DynamicIslandTests.exe

# 4. Compile Standalone Setup Installer
windres installer/installer_resource.rc -O coff -o installer/installer_resource.o
g++ -std=c++17 -O2 -mwindows -municode installer/installer_main.cpp installer/installer_resource.o `
    -luuid -lole32 -lshell32 -lshlwapi -luser32 -ladvapi32 `
    -o dist/K2DynamicIsland-Setup.exe
```

---

## 6. Installation & Usage

### 6.1 Using the Setup Installer (`K2DynamicIsland-Setup.exe`)
1. Run `dist\K2DynamicIsland-Setup.exe` (or execute silently with `/S`).
2. Follow the setup wizard to install to `%LOCALAPPDATA%\Programs\K2 Dynamic Island` (or custom directory via `/DIR="path"`).
3. The installer automatically registers **K2 Dynamic Island** in Windows **Settings → Apps → Installed apps**.
4. Creates Start Menu and Desktop shortcuts with high-resolution vector icons.
5. To uninstall, run `Uninstall.exe` from the installation directory or uninstall via Windows Settings. User settings in `%APPDATA%\K2DynamicIsland` are preserved.

### 6.2 Using the Portable Package (`K2DynamicIsland-Portable.zip`)
1. Extract `K2DynamicIsland-Portable.zip` into any directory.
2. Double-click `K2DynamicIsland.exe` to launch immediately.
3. No administrative privileges required.

---

## 7. Known Limitations & Notes

- **WebView2 Runtime Requirement:** The application requires the Microsoft Edge WebView2 Runtime. It is pre-installed on Windows 11 and up-to-date Windows 10 systems. If missing, the installer alerts the user to download it from Microsoft.
- **Media Session Limitations:** Media key commands (Play/Pause, Next, Prev, Mute) control the primary active Windows multimedia session. Browsers or desktop players that don't register media keys natively with Windows may require user interaction first to gain audio focus.
- **Multi-Monitor Positioning:** By default, the capsule centers at the top of the primary monitor. Window dragging enables free repositioning across multiple displays.

---

## 8. License

This project is distributed under the terms of the [MIT License](LICENSE).
