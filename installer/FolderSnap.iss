#ifndef AppVersion
  #define AppVersion "0.1.1"
#endif
#ifndef ProjectRoot
  #define ProjectRoot ".."
#endif
#ifndef SourceDir
  #define SourceDir "..\build\deploy"
#endif
#ifndef OutputDir
  #define OutputDir "..\build\installer"
#endif

[Setup]
AppId={{B2F49B61-4D03-4DE9-9F9A-8D20B0B5B6A1}
AppName=FolderSnap
AppVersion={#AppVersion}
AppPublisher=FolderSnap
DefaultDirName={localappdata}\Programs\FolderSnap
DefaultGroupName=FolderSnap
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=FolderSnap-Setup-{#AppVersion}
SetupIconFile={#ProjectRoot}\resources\icons\foldersnap-icon.ico
UninstallDisplayIcon={app}\FolderSnap.exe
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\FolderSnap"; Filename: "{app}\FolderSnap.exe"
Name: "{commondesktop}\FolderSnap"; Filename: "{app}\FolderSnap.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"

[Run]
Filename: "{app}\FolderSnap.exe"; Description: "Launch FolderSnap"; Flags: nowait postinstall skipifsilent
