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
| **v1.1.0** | 2026-10-19 | **1,202,176 B** (~1.14 MB) | **~2.5 MB** | **0 External DLLs** (Fully Static UCRT/Win32) | **Active Release** |
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
  - Persistent notification area (systray) icon with context menu:
    - *Open Island*
    - *Auto-Start with Windows* (HKCU Registry integration)
    - *Restart Application*
    - *Exit*
- **Custom Native Installer / Uninstaller**:
  - Standalone single-file C++ installer (`K2DynamicIsland-Setup.exe`) with `/silent` switch and desktop/start menu shortcut generation.
  - Clean C++ uninstaller (`Uninstall.exe`) with complete registry and directory cleanup.

---

### ⚡ [v1.1.0] — Zero-Dependency Static Runtime & High-DPI Polish (Current)
*Released: 2026-10-19*

#### 🐛 Critical Bug Fixes:
1. **Zero-Dependency Native Runtime (Missing `libc++.dll` Fixed)**:
   - **Root Cause**: Previous MinGW build dynamically linked against LLVM `libc++.dll` and `libunwind.dll`, causing `The code execution cannot proceed because libc++.dll was not found` on clean Windows PCs without developer tools.
   - **Solution**: Enforced `-static -static-libgcc -static-libstdc++` across all compilation targets (`K2DynamicIsland.exe`, `K2DynamicIslandTests.exe`, `Uninstall.exe`, `K2DynamicIsland-Setup.exe`).
   - **Verification**: Zero imports from external third-party DLLs. Only standard Windows system libraries (`KERNEL32`, `USER32`, `SHELL32`, `ole32`, `ADVAPI32`, `api-ms-win-crt-*`) are linked.
2. **High-DPI Half-Cut Frame (`ដាច់ Dynamic Island Frame ពាក់កណ្តាល` Fixed)**:
   - **Root Cause**: On high-DPI displays (125%, 150%, 175%, 200%), Win32 `CreateWindowExW` and `SetWindowPos` received unscaled physical pixels (290×44), whereas WebView2 internally scaled CSS pixels by the monitor DPI factor (44px → 55px-66px physical), resulting in the bottom half of the pill being clipped.
   - **Solution**: Added `GetWindowDpi()` and `ScaleForDpi()` in `src/app_window.cpp` using `MulDiv(value, dpi, 96)`. All window creation, sizing, and dynamic resizing now dynamically scale with monitor DPI.
   - **CSS Fix**: Updated `.capsule-pill` to `height: 100%` and `#island-container` to flex center alignment, preventing container overflow.

#### 🎨 UI & UX Enhancements:
- **Dynamic Capsule Width Resizing**:
  - **Idle State**: Automatically shrinks to a sleek, compact **215px** pill to minimize screen distraction.
  - **Audio / Media Playing State**: Smoothly expands to **315px** to display scrolling track titles, platform badge (YouTube, Spotify, etc.), and lively animated equalizer.
  - **File Drop State**: Expands to **270px** displaying file drop confirmations.
- **True OLED Pure Black Glass Backdrop**:
  - Replaced semi-transparent gray acrylic with deep OLED black: `rgba(10, 12, 16, 0.94)` paired with 28px backdrop blur and a crisp `rgba(255, 255, 255, 0.16)` border. Prevents desktop wallpaper colors and text from bleeding into the pill.
- **Vertical Mascot Centering**:
  - Adjusted companion slot (`.compact-anim-slot`) to `height: 100%` with flex centering. The Spider-Man mascot sits cleanly aligned without clipping or touching the pill edges.
- **Idle Equalizer Clean State**:
  - Mini-equalizer now cleanly hides (`display: none`) when media is stopped or idle, removing awkward static dots (`....`).

#### 📦 Distribution & Packaging:
- Package Size strictly controlled at **1,202,176 bytes** (~1.14 MB).
- SHA256 Verification: `5FD8B148F5F56A036428613645C4E31FC9DE6DAF32CBD08D2B540E209E20B03D`.
- Automated GitHub Release deployment to `K2DynamicIsland.github.io` and `KimSokun/K2DynamicIsland`.

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
5. **Silent Background Update Check**:
   - Non-intrusive daily check against `https://raw.githubusercontent.com/K2DynamicIsland/K2DynamicIsland.github.io/main/version.json`.
   - Displays a sleek notification pill with "Update to v1.X.X Available" with one-click background upgrade.

---

## 🛠️ Admin Build & Release Checklist

For every new release, admins and developers must follow these strict verification steps:

1. [ ] **Version Bump**:
   - Update `APP_VERSION` in `src/resource.h` / `src/main.cpp`.
   - Update `version` and `download_url` in `version.json`.
   - Update `CHANGELOG.md` with new features and fixes.
2. [ ] **Static Compilation**:
   - Run `build.bat` using MinGW-w64 with `-static` flag.
   - Run `dumpbin /dependents dist\K2DynamicIsland.exe` or `test_runtime.ps1` to verify zero non-system DLL dependencies (`libc++.dll`, `msvcp140.dll` must **not** appear).
3. [ ] **Test Suite**:
   - Ensure all 12/12 unit tests in `bin\K2DynamicIslandTests.exe` pass without errors.
4. [ ] **Size Verification**:
   - Ensure `K2DynamicIsland-Setup.exe` remains under the **1.5 MB** budget.
5. [ ] **High-DPI Testing**:
   - Test on 100%, 125%, and 150% Windows display scaling to ensure the frame is never cut off.
6. [ ] **Release Deployment**:
   - Run `python scratch/create_github_release.py` to publish the binary to GitHub Releases and sync with the official website.
