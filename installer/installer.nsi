; ============================================================================
;  K2 Dynamic Island - Windows Installer Script (NSIS)
; ============================================================================

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"

; --- General Settings ---
Name "K2 Dynamic Island"
OutFile "..\dist\K2DynamicIsland-Setup.exe"
InstallDir "$PROGRAMFILES64\K2 Dynamic Island"
InstallDirRegKey HKLM "Software\K2DynamicIsland" "Install_Dir"
RequestExecutionLevel highest

; Visuals
!define MUI_ICON "app_icon.ico"
!define MUI_UNICON "app_icon.ico"
!define MUI_ABORTWARNING

; --- Installer Pages ---
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

; Finish page with launch option
!define MUI_FINISHPAGE_RUN "$INSTDIR\K2DynamicIsland.exe"
!define MUI_FINISHPAGE_RUN_PARAMETERS "--recenter"
!define MUI_FINISHPAGE_RUN_TEXT "Launch K2 Dynamic Island"
!insertmacro MUI_PAGE_FINISH

; --- Uninstaller Pages ---
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

; --- Functions ---
Function .onInit
    ${If} ${RunningX64}
        SetRegView 64
    ${Else}
        MessageBox MB_ICONSTOP "K2 Dynamic Island requires a 64-bit version of Windows."
        Abort
    ${EndIf}

    ; Check for WebView2 Runtime
    Var /GLOBAL WV2_VER
    ReadRegStr $WV2_VER HKLM "SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}" "pv"
    ${If} $WV2_VER == ""
        ReadRegStr $WV2_VER HKCU "Software\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}" "pv"
    ${EndIf}
    ${If} $WV2_VER == ""
        ReadRegStr $WV2_VER HKLM "SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}" "pv"
    ${EndIf}

    ${If} $WV2_VER == ""
        MessageBox MB_ICONEXCLAMATION "Microsoft Edge WebView2 Runtime is recommended for K2 Dynamic Island.$\n$\nIf it is not installed, please download it from Microsoft's website."
    ${EndIf}
FunctionEnd

; --- Installation Section ---
Section "K2 Dynamic Island (required)" SecMain
    SectionIn RO

    SetOutPath "$INSTDIR"
    File "..\build\K2DynamicIsland.exe"
    File "..\build\WebView2Loader.dll"

    SetOutPath "$INSTDIR\assets"
    File /r "..\build\assets\*.*"

    ; Store installation folder in registry
    WriteRegStr HKLM "Software\K2DynamicIsland" "Install_Dir" "$INSTDIR"

    ; Create uninstaller
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    ; Start Menu Shortcuts
    CreateDirectory "$SMPROGRAMS\K2 Dynamic Island"
    CreateShortcut "$SMPROGRAMS\K2 Dynamic Island\K2 Dynamic Island.lnk" "$INSTDIR\K2DynamicIsland.exe" "" "$INSTDIR\K2DynamicIsland.exe" 0
    CreateShortcut "$SMPROGRAMS\K2 Dynamic Island\Uninstall K2 Dynamic Island.lnk" "$INSTDIR\Uninstall.exe" "" "$INSTDIR\Uninstall.exe" 0

    ; Desktop Shortcut
    CreateShortcut "$DESKTOP\K2 Dynamic Island.lnk" "$INSTDIR\K2DynamicIsland.exe" "" "$INSTDIR\K2DynamicIsland.exe" 0

    ; Register in Windows Installed Apps (Programs & Features)
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\K2 Dynamic Island" "DisplayName" "K2 Dynamic Island"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\K2 Dynamic Island" "DisplayIcon" "$INSTDIR\K2DynamicIsland.exe,0"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\K2 Dynamic Island" "DisplayVersion" "1.0.0"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\K2 Dynamic Island" "Publisher" "K2 Studio"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\K2 Dynamic Island" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\K2 Dynamic Island" "InstallLocation" "$INSTDIR"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\K2 Dynamic Island" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\K2 Dynamic Island" "NoRepair" 1

    ; Estimate installed size (in KB)
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\K2 Dynamic Island" "EstimatedSize" "$0"
SectionEnd

; --- Uninstaller Section ---
Section "Uninstall"
    ${If} ${RunningX64}
        SetRegView 64
    ${EndIf}

    ; Delete Shortcuts
    Delete "$DESKTOP\K2 Dynamic Island.lnk"
    Delete "$SMPROGRAMS\K2 Dynamic Island\K2 Dynamic Island.lnk"
    Delete "$SMPROGRAMS\K2 Dynamic Island\Uninstall K2 Dynamic Island.lnk"
    RMDir "$SMPROGRAMS\K2 Dynamic Island"

    ; Delete installed files
    Delete "$INSTDIR\K2DynamicIsland.exe"
    Delete "$INSTDIR\WebView2Loader.dll"
    RMDir /r "$INSTDIR\assets"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir "$INSTDIR"

    ; Delete Registry Keys
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\K2 Dynamic Island"
    DeleteRegKey HKLM "Software\K2DynamicIsland"

    ; Note: User settings and reminders in %APPDATA%\K2DynamicIsland are intentionally preserved
SectionEnd
