; ============================================================================
;  K2 Dynamic Island - Windows Installer Script (Inno Setup)
; ============================================================================

[Setup]
AppName=K2 Dynamic Island
AppVersion=1.0.0
AppPublisher=K2 Studio
DefaultDirName={autopf}\K2 Dynamic Island
DefaultGroupName=K2 Dynamic Island
OutputDir=..\dist
OutputBaseFilename=K2DynamicIsland-Setup
SetupIconFile=app_icon.ico
UninstallDisplayIcon={app}\K2DynamicIsland.exe
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern

[Files]
Source: "..\build\K2DynamicIsland.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\WebView2Loader.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\assets\*"; DestDir: "{app}\assets"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\K2 Dynamic Island"; Filename: "{app}\K2DynamicIsland.exe"
Name: "{group}\Uninstall K2 Dynamic Island"; Filename: "{uninstallexe}"
Name: "{autodesktop}\K2 Dynamic Island"; Filename: "{app}\K2DynamicIsland.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Run]
Filename: "{app}\K2DynamicIsland.exe"; Parameters: "--recenter"; Description: "{cm:LaunchProgram,K2 Dynamic Island}"; Flags: nowait postinstall skipifsilent

[Code]
// Detect WebView2 Runtime during setup
function IsWebView2Installed(): Boolean;
var
  VersionStr: string;
begin
  Result := False;
  if RegQueryStringValue(HKLM, 'SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}', 'pv', VersionStr) then
    Result := (VersionStr <> '');
  if not Result and RegQueryStringValue(HKCU, 'Software\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}', 'pv', VersionStr) then
    Result := (VersionStr <> '');
  if not Result and RegQueryStringValue(HKLM, 'SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}', 'pv', VersionStr) then
    Result := (VersionStr <> '');
end;

function InitializeSetup(): Boolean;
begin
  Result := True;
  if not IsWebView2Installed() then
  begin
    MsgBox('Microsoft Edge WebView2 Runtime was not detected on this system.' + #13#10 +
           'K2 Dynamic Island requires WebView2 to render its user interface.' + #13#10#13#10 +
           'You can continue installation, but please ensure Microsoft Edge or the WebView2 Runtime is installed before launching.',
           mbInformation, MB_OK);
  end;
end;
