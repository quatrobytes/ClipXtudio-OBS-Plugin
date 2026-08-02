#ifndef SourceRoot
  #define SourceRoot AddBackslash(SourcePath) + "..\..\artifacts\staging"
#endif
#ifndef OutputDir
  #define OutputDir AddBackslash(SourcePath) + "..\..\artifacts\dist"
#endif
#ifndef AppVersion
  #define AppVersion "0.5.99"
#endif
#ifndef OutputBaseFilename
  #define OutputBaseFilename "ClipXtudio-Setup"
#endif

#define PluginRoot SourceRoot + "\clipxtudio"
#define PluginDll PluginRoot + "\bin\64bit\clipxtudio.dll"
#define LicensePublicKey PluginRoot + "\data\license-public.pem"
#define SetupIcon AddBackslash(SourcePath) + "..\..\data\assets\branding\clipx-studio.ico"
#ifnexist PluginDll
  #error Staged plugin DLL missing. Prepare artifacts\staging first.
#endif
#ifnexist LicensePublicKey
  #error license-public.pem missing. Package only the public verification key.
#endif
#ifnexist SetupIcon
  #error ClipXtudio setup icon is missing.
#endif

#define AppName "ClipXtudio"
#define AppPublisher "QuatroBytes.com"
#define AppId "{{CBFD46E2-BE4F-4B9E-B38D-E2991803C8E8}"

[Setup]
AppId={#AppId}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL=https://quatrobytes.com
AppSupportURL=https://clipxtudio.com/support
AppUpdatesURL=https://clipxtudio.com/updates/latest.json
DefaultDirName={commonappdata}\obs-studio\plugins\clipxtudio
UsePreviousAppDir=no
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename={#OutputBaseFilename}
SetupIconFile={#SetupIcon}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
; Production installs still request elevation. The command-line override exists
; exclusively for the isolated packaging smoke test, which installs into %TEMP%.
PrivilegesRequiredOverridesAllowed=commandline
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=no
RestartApplications=no
Uninstallable=yes
UninstallDisplayName={#AppName} {#AppVersion}
VersionInfoVersion={#AppVersion}
VersionInfoCompany={#AppPublisher}
VersionInfoDescription=Native OBS Studio plugin installer
VersionInfoProductName={#AppName}
VersionInfoProductVersion={#AppVersion}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Files]
Source: "{#PluginRoot}\bin\64bit\*.dll"; DestDir: "{app}\bin\64bit"; Flags: ignoreversion
Source: "{#PluginRoot}\data\locale\*"; DestDir: "{app}\data\locale"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#PluginRoot}\data\assets\*"; DestDir: "{app}\data\assets"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#PluginRoot}\data\models\*"; DestDir: "{app}\data\models"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#PluginRoot}\data\tools\*"; DestDir: "{app}\data\tools"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#PluginRoot}\data\qt-plugins\*"; DestDir: "{app}\data\qt-plugins"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#PluginRoot}\data\license-public.pem"; DestDir: "{app}\data"; Flags: ignoreversion
Source: "{#PluginRoot}\README-INSTALL.txt"; DestDir: "{app}"; Flags: ignoreversion

[InstallDelete]
; Remove only the legacy packaged plugin directory. Clips, exports, settings
; and the SQLite library live in the user's data directory and are untouched.
Type: filesandordirs; Name: "{commonappdata}\obs-studio\plugins\clipcoach-studio"
Type: files; Name: "{app}\bin\64bit\clipcoach-studio.dll"
; v0.5.87 briefly shipped Qt 6.8 Multimedia beside OBS' Qt runtime. Remove it
; during upgrade; previews now use ClipXtudio's bundled FFmpeg.
Type: files; Name: "{app}\bin\64bit\Qt6Multimedia.dll"
Type: files; Name: "{app}\bin\64bit\Qt6MultimediaWidgets.dll"
Type: filesandordirs; Name: "{app}\data\qt-plugins\multimedia"

[Code]
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
    MsgBox(
      'ClipXtudio was removed. Your clips, exports, thumbnails, ' +
      'settings and local library were preserved.',
      mbInformation,
      MB_OK);
end;
