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
Name: "chinese"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加图标:"; Flags: unchecked
Name: "autostart"; Description: "开机自动启动"; GroupDescription: "其他选项:"; Flags: unchecked

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
  // 检查 D 盘是否存在
  if not DirExists('D:\') then
  begin
    MsgBox('警告: D 盘不存在，将使用默认安装路径。', mbInformation, MB_OK);
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    // 创建必要的目录
    CreateDir(ExpandConstant('{app}\Screenshots'));
    CreateDir(ExpandConstant('{app}\Config'));
    CreateDir(ExpandConstant('{app}\Temp'));
  end;
end;
