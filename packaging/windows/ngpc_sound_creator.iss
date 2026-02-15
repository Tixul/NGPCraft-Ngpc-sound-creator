#define MyAppName "NGPC Sound Creator"
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef SourceRoot
  #define SourceRoot "..\..\dist\stage\NGPC Sound Creator"
#endif
#ifndef OutputRoot
  #define OutputRoot "..\..\dist"
#endif

[Setup]
AppId={{D9E01B77-D95A-4D00-A7D8-89E344D9DF9A}
AppName={#MyAppName}
AppVersion={#AppVersion}
AppPublisher=NGPC Sound Creator
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir={#OutputRoot}
OutputBaseFilename=ngpc_sound_creator_setup_{#AppVersion}
Compression=lzma
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\ngpc_sound_creator.exe

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; Flags: unchecked

[Files]
Source: "{#SourceRoot}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\NGPC Sound Creator"; Filename: "{app}\ngpc_sound_creator.exe"
Name: "{group}\Uninstall NGPC Sound Creator"; Filename: "{uninstallexe}"
Name: "{autodesktop}\NGPC Sound Creator"; Filename: "{app}\ngpc_sound_creator.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\ngpc_sound_creator.exe"; Description: "{cm:LaunchProgram,NGPC Sound Creator}"; Flags: nowait postinstall skipifsilent
