; Standalone add-on only. Build with scripts/build_texture_installer.py.
; Game installer/download integration intentionally remains separate.
#ifndef PackVersion
#define PackVersion "1.0.0"
#endif
#ifdef DownloadBuild
#include "texture_payload.iss"
#ifndef BuildDir
#define BuildDir "..\build-release\Release"
#endif
#else
#ifndef PackFiles
#error Build with scripts/build_texture_installer.py to generate the exact payload list.
#endif
#endif
#ifndef PackGuid
#define PackGuid "5304328F-4C6F-4C06-B73F-3975FD8F43F2"
#endif
#ifndef GameGuid
#define GameGuid "4C948091-6EB5-49DE-AE93-21EF35A8A0B3"
#endif
#ifndef GameMutexNames
#define GameMutexNames "Local\NHLHitz2003Player,Local\NHLHitz2003Settings"
#endif
#ifndef DownloadBuild
#ifndef PackConfig
#error Supply the release config.exe with the texture controls.
#endif
#endif

[Setup]
AppId={{{#PackGuid}}
AppName=NHL Hitz 2003 Hi-Res Textures
AppVersion={#PackVersion}
AppPublisher=dalves408/nhlhitz2003recomp
AppPublisherURL=https://github.com/dalves408/nhlhitz2003recomp
DefaultDirName={code:GameDir}
UsePreviousAppDir=no
AppendDefaultDirName=no
DisableDirPage=no
DisableProgramGroupPage=yes
DirExistsWarning=no
Uninstallable=no
CreateUninstallRegKey=no
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
AppMutex={#GameMutexNames}
CloseApplications=no
SetupIconFile=..\resources\nhl_hitz_2003.ico
WizardSmallImageFile=wizard_small_55.bmp,wizard_small_64.bmp,wizard_small_80.bmp,wizard_small_96.bmp,wizard_small_128.bmp
#ifdef DownloadBuild
OutputBaseFilename=NHLHitz2003-HiRes-Textures-{#PackVersion}-Setup
ArchiveExtraction=enhanced/nopassword
#else
OutputBaseFilename=NHLHitz2003-HiRes-Textures-{#PackVersion}
#endif
Compression=lzma2/max
SolidCompression=yes
; Keep compression resource use bounded while other work runs on the host.
LZMANumBlockThreads=1
WizardStyle=modern

[Messages]
#ifdef DownloadBuild
WelcomeLabel2=Add the 4x hi-res textures to NHL Hitz 2003.%n%nSetup uses texture.bin beside this EXE if available. Otherwise it downloads the pack. Interrupted downloads can be resumed by running Setup again.%n%nRequires game version 1.0.1 or newer.
#else
WelcomeLabel2=Extract the 4x hi-res textures into your NHL Hitz 2003 game folder.%n%nRequires game version 1.0.1 or newer.
#endif
SelectDirLabel3=Choose your installed NHL Hitz 2003 game folder.
ButtonInstall=&Extract
ReadyLabel1=Ready to extract the hi-res texture pack.
ReadyLabel2a=Click Extract to continue, or Back to choose another game folder.
StatusExtractFiles=Extracting texture pack...
FinishedHeadingLabel=Texture pack extracted
FinishedLabel=Open config.exe: set Render Scale to 2x or higher, enable Use the hi-res textures on the Textures tab, then Save.%n%nOr press F1 in the game: set Render Scale to 2x or higher and turn Hi-res textures On.

[Files]
#ifdef DownloadBuild
Source: "{#BuildDir}\nhl_texture_download.exe"; Flags: dontcopy
Source: "{#BuildDir}\config.exe"; DestDir: "{app}"; Flags: replacesameversion
#else
; Generated, explicit DDS paths preserve the source tree and filenames.
#include PackFiles
Source: "{#PackManifest}"; DestDir: "{app}\texture_pack"; DestName: "manifest.txt"; Flags: ignoreversion
Source: "{#PackInfo}"; DestDir: "{app}\texture_pack"; DestName: "pack-info.json"; Flags: ignoreversion
Source: "{#PackReadme}"; DestDir: "{app}\texture_pack"; DestName: "README.txt"; Flags: ignoreversion
; Update older/same-version Settings with the Textures tab; keep newer game versions.
Source: "{#PackConfig}"; DestDir: "{app}"; DestName: "config.exe"; Flags: replacesameversion
#endif

[Code]
#ifdef DownloadBuild
#include "texture_download_code.iss"
#endif
const
  GameKey = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{{#GameGuid}}_is1';
  LegacyPackKey = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{{#PackGuid}}_is1';

function GameExists(Dir: String): Boolean;
begin
  Result := FileExists(Dir + '\nhl_hitz_2003.exe') or
            FileExists(Dir + '\nhl_hitz_2003-db.exe') or
            FileExists(Dir + '\nhl_hitz_2003_recomp.exe');
end;

function FindGameDir: String;
var Dir: String;
begin
  Result := '';
  if RegQueryStringValue(HKCU, GameKey, 'Inno Setup: App Path', Dir) then
    if GameExists(RemoveBackslashUnlessRoot(Dir)) then
      Result := RemoveBackslashUnlessRoot(Dir);
  if Result = '' then
    if RegQueryStringValue(HKLM, GameKey, 'Inno Setup: App Path', Dir) then
      if GameExists(RemoveBackslashUnlessRoot(Dir)) then
        Result := RemoveBackslashUnlessRoot(Dir);
end;

function GameDir(Param: String): String;
begin
  Result := FindGameDir;
  if Result = '' then Result := 'C:\Games\NHL Hitz 20-03';
end;

function SupportedGame(Exe: String): Boolean;
var MS, LS: Cardinal;
begin
  Result := False;
  if GetVersionNumbers(Exe, MS, LS) then
    Result := (MS > $00010000) or ((MS = $00010000) and (LS >= $00010000));
end;

function CheckGame(Dir: String): String;
begin
  Result := '';
  if not GameExists(Dir) then
    Result := 'Choose the installed NHL Hitz 2003 game folder (normal or debug build).'
  else if not (SupportedGame(Dir + '\nhl_hitz_2003.exe') or
               SupportedGame(Dir + '\nhl_hitz_2003-db.exe') or
               SupportedGame(Dir + '\nhl_hitz_2003_recomp.exe')) then
    Result := 'This DDS texture add-on requires NHL Hitz 2003 version 1.0.1 or newer. Update the game first.';
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var Error: String;
begin
  Result := True;
  if (CurPageID = wpSelectDir) and not WizardSilent then begin
    Error := CheckGame(RemoveBackslashUnlessRoot(WizardDirValue));
    if Error <> '' then begin
      MsgBox(Error, mbError, MB_OK);
      Result := False;
    end;
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  { Silent installs must fail cleanly instead of waiting at a directory-page dialog. }
  Result := CheckGame(RemoveBackslashUnlessRoot(ExpandConstant('{app}')));
#ifdef DownloadBuild
  if Result = '' then Result := PrepareTexturePayload(1048576);
#endif
end;

#ifdef DownloadBuild
procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpReady then begin
    if TexturePackNeedsInstall then begin
      WizardForm.NextButton.Caption := SetupMessage(msgButtonInstall);
      WizardForm.PageNameLabel.Caption := SetupMessage(msgWizardReady);
      WizardForm.PageDescriptionLabel.Caption := SetupMessage(msgReadyLabel1);
      WizardForm.ReadyLabel.Caption := SetupMessage(msgReadyLabel2a);
    end else begin
      WizardForm.NextButton.Caption := '&Continue';
      WizardForm.PageNameLabel.Caption := 'Texture pack already installed';
      WizardForm.PageDescriptionLabel.Caption := 'The current release is already present in this game folder.';
      WizardForm.ReadyLabel.Caption := 'Click Continue to finish. No texture download or extraction is needed.';
    end;
  end;
  if (CurPageID = wpFinished) and TexturePackAlreadyCurrent then begin
    WizardForm.FinishedHeadingLabel.Caption := 'Texture pack already installed';
    WizardForm.FinishedLabel.Caption := 'The current 4x texture pack is already installed. No download or extraction was needed.';
  end;
end;

function UpdateReadyMemo(Space, NewLine, MemoUserInfoInfo, MemoDirInfo, MemoTypeInfo, MemoComponentsInfo,
  MemoGroupInfo, MemoTasksInfo: String): String;
begin
  Result := MemoDirInfo;
  if not TexturePackNeedsInstall then
    Result := Result + NewLine + NewLine + 'The current 4x texture pack is already installed. No download or extraction is needed.';
end;

procedure CancelButtonClick(CurPageID: Integer; var Cancel, Confirm: Boolean);
begin
  TextureCancelButtonClick(Cancel, Confirm);
end;
#endif

procedure RemoveLegacyPackEntry(RootKey: Integer);
var Dir, Command, LegacyExe: String;
begin
  { Migrate only this add-on's old entry for this exact game folder. Never run
    its uninstaller: that would remove the textures we just extracted. }
  if RegQueryStringValue(RootKey, LegacyPackKey, 'Inno Setup: App Path', Dir) then
    if CompareText(RemoveBackslashUnlessRoot(Dir),
                   RemoveBackslashUnlessRoot(ExpandConstant('{app}'))) = 0 then begin
      Command := '';
      RegQueryStringValue(RootKey, LegacyPackKey, 'UninstallString', Command);
      if RegDeleteKeyIncludingSubkeys(RootKey, LegacyPackKey) then begin
        LegacyExe := ExpandConstant('{app}\addons\hires_textures\unins000.exe');
        if (CompareText(Command, LegacyExe) = 0) or
           (CompareText(Command, '"' + LegacyExe + '"') = 0) then begin
          DeleteFile(LegacyExe);
          DeleteFile(ChangeFileExt(LegacyExe, '.dat'));
          DeleteFile(ChangeFileExt(LegacyExe, '.msg'));
        end;
        Log('Removed the legacy texture add-on registration for this game folder.');
      end else
        Log('Unable to remove a legacy add-on entry with the current permissions.');
    end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then begin
#ifdef DownloadBuild
    InstallTexturePack;
#endif
    RemoveLegacyPackEntry(HKCU);
    RemoveLegacyPackEntry(HKLM);
  end;
#ifdef DownloadBuild
  if CurStep = ssDone then TextureFinishCacheChoice;
#endif
end;

#ifdef DownloadBuild
procedure DeinitializeSetup;
begin
  TextureCleanupOnExit;
end;

function GetCustomSetupExitCode: Integer;
begin
  Result := 0;
  if not TexturePackInstalled then Result := 1;
end;
#endif
