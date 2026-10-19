# 📋 K2 Dynamic Island — Version History & Admin Changelog

> **Admin & Engineering Control Document**  
> **Project:** K2 Dynamic Island (Windows 10/11)  
> **Repository:** `https://github.com/K2DynamicIsland/K2DynamicIsland.github.io`  
> **Binary Target:** `K2DynamicIsland.exe` / `K2DynamicIsland-Setup.exe` (x86_64 standalone)  
> **Last Updated:** 2026-10-19  

---

## 📊 Version & Binary Size Control Dashboard

To ensure peak performance, instant startup (< 50ms), and true portable deployment on clean Windows installations without requiring external runtimes, all builds must adhere to strict size and dependency budgets:

| Version | Release Date | Setup Size | Installed Footprint | Runtime Dependencies | Status |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **v1.0.0** | 2026-10-18 | ~1.12 MB | ~2.4 MB | Dynamic CRT (required `libc++.dll`) | Deprecated |
| **v1.1.0** | 2026-10-19 | ~1.14 MB | ~2.5 MB | 0 External DLLs (Fully Static UCRT/Win32) | Superceded |
| **v1.1.1** | **2026-10-19** | **1,202,688 B** (~1.14 MB) | **~2.5 MB** | **0 External DLLs** (Fully Static UCRT/Win32) | **Active Release** |
| **v1.2.0** | *Upcoming* | Target < 1.35 MB | Target < 3.0 MB | 0 External DLLs (Pure WinRT/WASAPI) | Planned |

---

## 🏷️ Version History

---

### 🚀 [v1.0.0] — Initial Foundation & Core Dynamic Island
*Released: 2026-10-18*

#### 🌟 Features & Capabilities:
- **Floating Island Shell**:
  - Developed custom Win32 Per-Monitor DPI aware frameless window anchored to top-center of primary screen.
  - Transparent backdrop utilizing Windows WebView2 evergreen runtime for fluid 60fps CSS animations.
  - Compact pill dimensions: 290px × 44px.
  - Expanded interactive card: 360px × 180px with hover/click trigger.
- **Audio Session Detection (WASAPI)**:
  - Low-level Windows Audio Session API listener polling active system audio sessions without elevated privileges.
  - Animated sound visualizer waveform rendering when active audio is detected.
- **Quick Drag & Drop Zone**:
  - Integrated Win32 OLE drag-and-drop dropzone directly into the Island pill.
  - Instant file path detection, copy file path, and drop status notifications.
- **Companion Mascot Slot**:
  - Interactive Spider-Verse Miles Morales pixel/GIF animation slot integrated directly on the capsule pill.
- **System Tray & Lifecycle**:
  - Persistent notification area (systray) icon with context menu (Open, Restart, Auto-start toggle, Exit).
- **Custom Native Installer / Uninstaller**:
  - Standalone single-file C++ installer (`K2DynamicIsland-Setup.exe`) with `/silent` switch.
  - Clean C++ uninstaller (`Uninstall.exe`) with complete registry and directory cleanup.

---

### ⚡ [v1.1.0] — Zero-Dependency Static Runtime & High-DPI Scaling Fix
*Released: 2026-10-19*

#### 🐛 Bug Fixes & Improvements:
- **Fixed Missing `libc++.dll`**: Replaced dynamic MinGW linking with `-static -static-libgcc -static-libstdc++`.
- **Fixed Half-Cut Frame (`ដាច់ Frame ពាក់កណ្តាល`)**: Added `ScaleForDpi()` in `src/app_window.cpp` to correctly size the Win32 window on 125%, 150%, and 175% Windows display scaling.
- **Dynamic Width Resizing**: Auto-resizes between 215px (idle), 315px (music playback), and 270px (file drop).
- **Companion Centering & Equalizer Cleanup**: Mascot vertically aligned in center; mini-equalizer cleanly hidden during idle.

---

### 💎 [v1.1.1] — Frosted Glass Acrylic Restoration, Auto-Update & Pin Screen Recenter (Current)
*Released: 2026-10-19*

#### 🎨 Aesthetic & UX Polish:
- **Restored Frosted Glass Acrylic Theme**:
  - Reverted opaque dark background to signature transparent glass acrylic: `rgba(13, 17, 24, 0.48)` with `backdrop-filter: blur(14px) saturate(180%)`.
  - Preserved High-DPI frame scaling fix, keeping the glass translucent without bottom-border clipping.
- **Direct ZIP Package Distribution**:
  - Added `K2DynamicIsland-v1.1.0-Windows.zip` (967 KB) to GitHub Releases and downloads page.
  - Solves Chromium browser `Unconfirmed .crdownload` pauses when downloading unsigned `.exe` files.

#### 🔧 Core Architectural Fixes:
1. **Auto-Update Open-Source PE Integrity Verification (`VerifyInstallerIntegrity`)**:
   - **Root Cause**: `VerifyInstallerSignature()` strictly required Authenticode signatures, falsely flagging community open-source builds as untrusted (`TRUST_E_NOSIGNATURE`) and deleting downloaded updates.
   - **Solution**: Implemented dual-stage verification: checks Authenticode first; if unsigned, validates Windows PE binary format (`IMAGE_DOS_SIGNATURE`, `IMAGE_NT_SIGNATURE`, valid file size > 500 KB).
   - Added `release-assets.githubusercontent.com` and `*.githubusercontent.com` to HTTPS security whitelist.
   - Enhanced UI with interactive 1-click install toast notification.
2. **Pin SVG Button Monitor Frame Scan & Recenter (`recenter_island`)**:
   - **Root Cause**: Clicking the Pin SVG button previously toggled `alwaysOnTop` without updating window coordinates.
   - **Solution**: Wired `btn-pin-toggle` to invoke `recenter_island`. It scans the active monitor (`MonitorFromPoint` / `GetMonitorInfoW`), calculates the exact horizontal center (`newX = mi.rcWork.left + (workWidth - curW) / 2`), positions the island 12px from top (`ScaleForDpi(12, dpi)`), and saves the coordinates to local storage.

---

### 🔮 [v1.2.0] — Next Version Roadmap & Planned Features
*Target Milestone: Next Major Release*

#### 📋 Planned Features:
1. **Windows SMTC (System Media Transport Controls) Native Integration**:
   - Query `Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager` via WinRT C++ to retrieve:
     - Real-time track title, artist name, and album title.
     - Embedded thumbnail / album art preview inside the expanded island card.
     - Native Media Play/Pause, Next Track, Previous Track controls sent directly to the active media player.
2. **Companion Mascot Customizer**:
   - Settings option allowing users to toggle between different companion mascots (Spider-Man, Neko Cat, Cyber Bouncing Orb, or custom user-uploaded `.gif` / `.webp`).
3. **Multi-Monitor Positioning Engine**:
   - Preferences menu option:
     - *Pin to Primary Monitor* (default).
     - *Follow Active Cursor / Focused Window*.
     - *Specify Monitor Index (1, 2, 3)*.
4. **Hardware & System Pulse HUD**:
   - Optional pill widgets toggleable in settings:
     - Laptop Battery % status & charging pulse indicator.
     - Volume adjustment HUD pill overlay.
     - CapsLock / NumLock toggle pill indicator.
5. **Silent Background Auto-Updater**:
   - Fully silent background upgrade using `/silent` mode that updates the app seamlessly while preserving user state.

---

## 🛠️ Admin Build & Release Checklist

For every new release, admins and developers must follow these strict verification steps:

1. [ ] **Version Bump**:
   - Update `APP_VERSION` in `src/resource.h` / `src/main.cpp`.
   - Update `version` and `download_url` in `version.json`.
   - Update `CHANGELOG.md` with new features and fixes.
2. [ ] **Static Compilation**:
   - Run `build.bat` using MinGW-w64 with `-static` flag.
   - Verify zero non-system DLL dependencies (`libc++.dll`, `msvcp140.dll` must **not** appear).
3. [ ] **Test Suite**:
   - Ensure all 12/12 unit tests in `bin\K2DynamicIslandTests.exe` pass without errors.
4. [ ] **Size Verification**:
   - Ensure `K2DynamicIsland-Setup.exe` remains under the **1.5 MB** budget.
5. [ ] **Release Deployment**:
   - Run `python scratch/create_github_release.py` and `python scratch/upload_zip.py` to publish `.exe` and `.zip` binaries to GitHub Releases and sync with official website.
