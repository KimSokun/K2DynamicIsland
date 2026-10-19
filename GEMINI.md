# K2 Dynamic Island — Project Guidelines & Version Control Rules

## 1. Strict Version Control & Changelog Tracking
- Whenever updates, bug fixes, or new features are developed, always update versioning across all targets:
  - `src/update_manager.h` (`m_appVersion`)
  - `version.json` (root & downloads)
  - `CHANGELOG.md` (keep full record of fixes, feature list, and binary size control dashboard for admin review)
- Binary Size Budget:
  - Keep `K2DynamicIsland-Setup.exe` strictly below 1.5 MB.
  - Zero external runtime DLL dependencies (`-static -static-libgcc -static-libstdc++`).

## 2. Seamless & Silent Updates ("User Doesn't Know")
- Updates must deploy smoothly and unobtrusively in the background without causing disruptions:
  - Support silent installation (`/silent` or `/S`) that automatically preserves settings, screen position, and centered pin alignment.
  - Ensure updater supports verified open-source community PE binaries without failing Authenticode signature checks.
  - Provide both `.exe` and `.zip` archives on GitHub Releases to avoid Chromium `Unconfirmed .crdownload` pauses.
