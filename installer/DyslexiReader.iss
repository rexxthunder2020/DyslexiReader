#define AppVersion "1.1.0"

[Setup]
#ifdef SmokeTest
AppId=DyslexiReader.InstallerSmokeTest
AppName=DyslexiReader Installer Test
OutputBaseFilename=DyslexiReader-Setup-Test
#else
AppId={{CB823682-52AA-45F3-B772-FC4703C42681}
AppName=DyslexiReader
OutputBaseFilename=DyslexiReader-Setup-1.1.0-x64
#endif
AppVersion={#AppVersion}
AppVerName=DyslexiReader {#AppVersion}
DefaultDirName={localappdata}\Programs\DyslexiReader
DefaultGroupName=DyslexiReader
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir=..\dist
WizardStyle=modern
Compression=lzma2
SolidCompression=yes
UninstallDisplayIcon={app}\DyslexiReader.exe
CloseApplications=yes
RestartApplications=no
ChangesAssociations=yes
SetupLogging=yes
VersionInfoVersion={#AppVersion}
VersionInfoDescription=DyslexiReader Installer

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked
Name: "pdfassoc"; Description: "Add DyslexiReader to PDF Open with choices"

[Files]
Source: "..\build\DyslexiReader.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\pdfium.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\PDFium-LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\licenses\*"; DestDir: "{app}\licenses"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion

#ifndef SmokeTest
[Icons]
Name: "{group}\DyslexiReader"; Filename: "{app}\DyslexiReader.exe"
Name: "{group}\Uninstall DyslexiReader"; Filename: "{uninstallexe}"
Name: "{autodesktop}\DyslexiReader"; Filename: "{app}\DyslexiReader.exe"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\Classes\DyslexiReader.PDF"; ValueType: string; ValueData: "PDF document"; Flags: uninsdeletekey; Tasks: pdfassoc
Root: HKCU; Subkey: "Software\Classes\DyslexiReader.PDF\DefaultIcon"; ValueType: string; ValueData: """{app}\DyslexiReader.exe"",0"; Tasks: pdfassoc
Root: HKCU; Subkey: "Software\Classes\DyslexiReader.PDF\shell\open\command"; ValueType: string; ValueData: """{app}\DyslexiReader.exe"" ""%1"""; Tasks: pdfassoc
Root: HKCU; Subkey: "Software\Classes\.pdf\OpenWithProgids"; ValueType: none; ValueName: "DyslexiReader.PDF"; Flags: uninsdeletevalue; Tasks: pdfassoc
Root: HKCU; Subkey: "Software\Classes\Applications\DyslexiReader.exe"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "DyslexiReader"; Flags: uninsdeletekey; Tasks: pdfassoc
Root: HKCU; Subkey: "Software\Classes\Applications\DyslexiReader.exe\shell\open\command"; ValueType: string; ValueData: """{app}\DyslexiReader.exe"" ""%1"""; Tasks: pdfassoc
Root: HKCU; Subkey: "Software\Classes\Applications\DyslexiReader.exe\SupportedTypes"; ValueType: string; ValueName: ".pdf"; ValueData: ""; Tasks: pdfassoc
Root: HKCU; Subkey: "Software\DyslexiReader\Capabilities"; ValueType: string; ValueName: "ApplicationName"; ValueData: "DyslexiReader"; Flags: uninsdeletekey; Tasks: pdfassoc
Root: HKCU; Subkey: "Software\DyslexiReader\Capabilities"; ValueType: string; ValueName: "ApplicationDescription"; ValueData: "View PDF documents and read them aloud with Windows voices."; Tasks: pdfassoc
Root: HKCU; Subkey: "Software\DyslexiReader\Capabilities\FileAssociations"; ValueType: string; ValueName: ".pdf"; ValueData: "DyslexiReader.PDF"; Tasks: pdfassoc
Root: HKCU; Subkey: "Software\RegisteredApplications"; ValueType: string; ValueName: "DyslexiReader"; ValueData: "Software\DyslexiReader\Capabilities"; Flags: uninsdeletevalue; Tasks: pdfassoc

[Run]
Filename: "{app}\DyslexiReader.exe"; Description: "Launch DyslexiReader"; Flags: nowait postinstall skipifsilent

[Messages]
FinishedLabel=Setup has installed DyslexiReader.%n%nTo make it your default PDF reader, right-click a PDF, choose Open with > Choose another app, select DyslexiReader, and choose Always.
#endif

