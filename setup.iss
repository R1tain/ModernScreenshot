; ModernScreenshot 安装脚本
; 使用 Inno Setup 编译器生成安装程序

[Setup]
AppName=ModernScreenshot
AppVersion=1.0.0
AppPublisher=ModernScreenshot Team
DefaultDirName=D:\ModernScreenshot
DefaultGroupName=ModernScreenshot
OutputDir=.
OutputBaseFilename=ModernScreenshot_Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
DisableDirPage=no
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\ModernScreenshot.exe
PrivilegesRequired=lowest

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create desktop shortcut"; GroupDescription: "Additional icons:"; Flags: unchecked
Name: "autostart"; Description: "Launch at Windows startup"; GroupDescription: "Other options:"; Flags: unchecked

[Files]
Source: "bin\Release\net6.0-windows\publish\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\ModernScreenshot"; Filename: "{app}\ModernScreenshot.exe"
Name: "{group}\卸载 ModernScreenshot"; Filename: "{uninstallexe}"
Name: "{autodesktop}\ModernScreenshot"; Filename: "{app}\ModernScreenshot.exe"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "ModernScreenshot"; ValueData: """{app}\ModernScreenshot.exe"""; Flags: uninsdeletevalue; Tasks: autostart

[Run]
Filename: "{app}\ModernScreenshot.exe"; Description: "立即启动 ModernScreenshot"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}\Screenshots"
Type: filesandordirs; Name: "{app}\Config"
Type: filesandordirs; Name: "{app}\Temp"

[Code]
function InitializeSetup(): Boolean;
begin
  Result := True;
  // Check if D drive exists
  if not DirExists('D:\') then
  begin
    MsgBox('Warning: D drive not found. Will use default installation path.', mbInformation, MB_OK);
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    // Create necessary directories
    CreateDir(ExpandConstant('{app}\Screenshots'));
    CreateDir(ExpandConstant('{app}\Config'));
    CreateDir(ExpandConstant('{app}\Temp'));
  end;
end;
