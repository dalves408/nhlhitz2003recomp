{ Shared by game Setup and the extraction-only standalone texture addon.
  The caller supplies pinned Texture* constants from texture_payload.iss. }
var
  TextureProgress: TOutputProgressWizardPage;
  TextureTransferActive, TextureTransferCancelled, TextureExtractActive: Boolean;
  TexturePackInstalled: Boolean;
  TexturePackAlreadyCurrent: Boolean;
  TextureUsedCache, TextureCacheChoiceAsked, TextureDeleteCacheOnExit: Boolean;
  TextureLastUiTick: LongWord;
  TexturePayloadPath, TextureCacheDir, TextureCancelFile, TextureError: String;
  TextureRatePhase: String;
  TextureRateTick: LongWord;
  TextureRateBytes, TextureRatePerSecond: Int64;
  TextureRateAvailable: Boolean;

function TextureSetupProcessId: LongWord;
  external 'GetCurrentProcessId@kernel32.dll stdcall';
function TextureTick: LongWord;
  external 'GetTickCount@kernel32.dll stdcall';

function TextureForceRepair: Boolean;
begin
  Result := ExpandConstant('{param:REPAIRTEXTURES|0}') = '1';
end;

function TextureInstalledPackCurrent(Dir: String): Boolean;
var
  Manifest, Rest, Relative: String;
  Lines: TArrayOfString;
  I, Space: Integer;
  ExpectedSize, ActualSize: Int64;
begin
  Result := False;
  if Dir = '' then Exit;
  Dir := AddBackslash(Dir) + 'texture_pack\';
  Manifest := Dir + 'manifest.txt';
  if not FileExists(Manifest) then Exit;
  try
    { Pin the expected file list rather than trusting a folder or editable
      version label. Stat every listed file to reject missing/truncated files.
      Explicit repair restores all files from the verified archive. }
    if CompareText(GetSHA256OfFile(Manifest), '{#TextureManifestSHA256}') <> 0 then Exit;
    if not LoadStringsFromFile(Manifest, Lines) then Exit;
    if GetArrayLength(Lines) <> {#TextureFileCount} then Exit;
    for I := 0 to GetArrayLength(Lines) - 1 do begin
      Rest := Lines[I];
      Space := Pos(' ', Rest);
      if Space <> 65 then Exit;
      Delete(Rest, 1, Space);
      Space := Pos(' ', Rest);
      if Space <= 1 then Exit;
      ExpectedSize := StrToInt64Def(Copy(Rest, 1, Space - 1), -1);
      Relative := Copy(Rest, Space + 1, Length(Rest));
      StringChangeEx(Relative, '/', '\', True);
      if (Relative = '') or (ExpectedSize < 0) then Exit;
      if not FileSize64(Dir + Relative, ActualSize) then Exit;
      if ActualSize <> ExpectedSize then Exit;
    end;
    Result := True;
  except
    Result := False; { Unreadable metadata/files never suppress an install. }
  end;
end;

function TexturePackNeedsInstall: Boolean;
begin
  Result := TextureForceRepair or not TextureInstalledPackCurrent(WizardDirValue);
  if not Result then Log('The current texture pack is installed; the texture download offer is hidden.');
end;

function WantTexturePack: Boolean;
begin
#ifdef TextureOptional
  Result := WizardIsTaskSelected('hires');
#else
  Result := True;
#endif
end;

function GetTexturePayloadPath(Param: String): String;
begin
  Result := TexturePayloadPath;
end;

procedure TextureCancelButtonClick(var Cancel, Confirm: Boolean);
begin
  if TextureTransferActive then begin
    TextureTransferCancelled := True;
    SaveStringToFile(TextureCancelFile, 'cancel', False);
    if TextureExtractActive then
      TextureProgress.SetText('Stopping extraction...', 'The verified download is kept for next time.')
    else
      TextureProgress.SetText('Stopping download...', 'Saved progress will be resumed next time.');
    Cancel := False;
    Confirm := False;
  end;
end;

function TextureNetworkPhase(Phase: String): Boolean;
begin
  Result := (Phase = 'DOWNLOADING') or (Phase = 'RESUMING') or (Phase = 'RESTARTING');
end;

function TextureFormatBytes(Bytes: Int64): String;
begin
  if Bytes >= 1000000000 then Result := Format('%.2f GB', [Bytes / 1000000000.0])
  else if Bytes >= 1000000 then Result := Format('%.1f MB', [Bytes / 1000000.0])
  else if Bytes >= 1000 then Result := Format('%.1f KB', [Bytes / 1000.0])
  else Result := IntToStr(Bytes) + ' B';
end;

procedure TextureUpdateDownloadRate(Phase: String; Done: Int64; Tick: LongWord);
begin
  { Begin at the first reported offset: saved/resumed bytes are not new network
    traffic. Verification, retries restarting at zero and clock wrap reset the
    sample. Refresh speed about once a second; idle intervals show zero. }
  if not TextureNetworkPhase(Phase) or (Phase <> TextureRatePhase) or
     (Tick < TextureRateTick) or (Done < TextureRateBytes) then begin
    TextureRatePhase := Phase;
    TextureRateTick := Tick;
    TextureRateBytes := Done;
    TextureRatePerSecond := 0;
    TextureRateAvailable := False;
  end else if Tick - TextureRateTick >= 1000 then begin
    TextureRatePerSecond := (Done - TextureRateBytes) * 1000 div (Tick - TextureRateTick);
    TextureRateAvailable := True;
    TextureRateTick := Tick;
    TextureRateBytes := Done;
  end;
end;

function TextureProgressDetails(Phase: String; Done, Total: Int64): String;
var Percent: Extended; Speed: String;
begin
  if Done < 0 then Done := 0;
  if Done > Total then Done := Total;
  Percent := 0;
  if Total > 0 then Percent := Done * 100.0 / Total;
  if TextureNetworkPhase(Phase) then begin
    if TextureRateAvailable then Speed := TextureFormatBytes(TextureRatePerSecond) + '/s'
    else Speed := 'Measuring speed...';
    Result := Format('%.1f%% - %.1f / %.1f MB - %s', [Percent, Done / 1000000.0, Total / 1000000.0, Speed]);
  end else if Phase = 'VERIFYING' then
    Result := Format('texture.bin - %s - %.1f%% verified', [TextureFormatBytes(Total), Percent])
  else Result := 'texture.bin - Size: ' + TextureFormatBytes(Total);
end;

procedure TextureDownloadLog(const S: String; const Error, FirstLine: Boolean);
var
  Rest, Phase, LabelText: String;
  K: Integer;
  Done, Total: Int64;
begin
  if Error then begin TextureError := S; Log(S); Exit; end;
  if Copy(S, 1, 9) = 'PROGRESS ' then begin
    Rest := Copy(S, 10, Length(S));
    K := Pos(' ', Rest);
    Done := StrToInt64Def(Copy(Rest, 1, K - 1), 0);
    Delete(Rest, 1, K);
    K := Pos(' ', Rest);
    Total := StrToInt64Def(Copy(Rest, 1, K - 1), 0);
    Phase := Copy(Rest, K + 1, Length(Rest));
    if TextureTransferCancelled then LabelText := 'Stopping download...'
    else if Phase = 'WAITING' then LabelText := 'Waiting for another texture download to finish...'
    else if Phase = 'RESUMING' then LabelText := 'Resuming texture pack download...'
    else if Phase = 'VERIFYING' then LabelText := 'Verifying texture pack...'
    else if Phase = 'RESTARTING' then LabelText := 'The server cannot resume this copy. Restarting download...'
    else if Phase = 'CHECKING' then LabelText := 'Checking for a local texture pack...'
    else LabelText := 'Downloading texture pack...';
    TextureUpdateDownloadRate(Phase, Done, TextureTick);
    if TextureNetworkPhase(Phase) and not TextureTransferCancelled then
      LabelText := LabelText + ' (' + TextureFormatBytes(Total) + ')';
    TextureProgress.SetText(LabelText, TextureProgressDetails(Phase, Done, Total));
    TextureProgress.SetProgress(Done div 1024, Total div 1024);
  end else begin
    Log(S);
    if S = 'READY local' then begin
      TexturePayloadPath := ExpandConstant('{src}\texture.bin');
      TextureUsedCache := False;
    end else if S = 'READY cache' then begin
      TexturePayloadPath := TextureCacheDir + '\texture.bin';
      TextureUsedCache := True;
    end
    else if Copy(S, 1, 6) = 'ERROR ' then TextureError := Copy(S, 7, Length(S));
  end;
end;

function PrepareTexturePayload(OtherRequiredBytes: Int64): String;
var
  Params, LocalParam: String;
  Code: Integer;
  Started, RetryWithoutLocal: Boolean;
  FreeBytes, TotalBytes: Int64;
begin
  Result := '';
  TexturePackAlreadyCurrent := False;
  if not WantTexturePack then Exit;
  if not TextureForceRepair and TextureInstalledPackCurrent(WizardDirValue) then begin
    TexturePackAlreadyCurrent := True;
    TexturePackInstalled := True;
    TextureUsedCache := False;
    TexturePayloadPath := '';
    Log('The current texture pack is already installed; no download or extraction is needed.');
    Exit;
  end;
  MapArchiveExtensions('.bin', '.7z');
  TextureCacheDir := ExpandConstant('{#TextureCacheRoot}\{#TextureSHA256}');
  TextureCancelFile := ExpandConstant('{tmp}\texture-download.cancel');
  ExtractTemporaryFile('nhl_texture_download.exe');
  if TextureProgress = nil then
    TextureProgress := CreateOutputProgressPage('Hi-res texture pack', 'Preparing the 4x texture pack.');
  TextureProgress.SetText('Checking for a local texture pack...', 'texture.bin');
  TextureProgress.SetProgress(0, 0);
  if not FileExists(ExpandConstant('{src}\texture.bin')) and
     not FileExists(TextureCacheDir + '\texture.bin') then begin
    if FileExists(TextureCacheDir + '\texture.bin.part') then
      TextureProgress.SetText('Resuming texture pack download...', 'Saved progress will be used.')
    else
      TextureProgress.SetText('The texture pack will be downloaded.', 'You can stop and resume it later.');
  end;
  TextureProgress.Show;
  TextureTransferActive := True;
  TextureTransferCancelled := False;
  WizardForm.CancelButton.Enabled := True;
  WizardForm.CancelButton.Visible := True;
  LocalParam := ' --local ' + AddQuotes(ExpandConstant('{src}\texture.bin'));
  try
    repeat
      DeleteFile(TextureCancelFile);
      TexturePayloadPath := '';
      TextureUsedCache := False;
      TextureError := '';
      TextureRatePhase := '';
      TextureRateAvailable := False;
      Params := '--url ' + AddQuotes('{#TextureURL}') +
                ' --cache ' + AddQuotes(TextureCacheDir) +
                ' --sha256 {#TextureSHA256} --bytes {#TextureBytes}' +
                ' --cancel ' + AddQuotes(TextureCancelFile) +
                ' --parent ' + IntToStr(TextureSetupProcessId) + LocalParam;
      Code := 99;
      Started := ExecAndLogOutput(ExpandConstant('{tmp}\nhl_texture_download.exe'), Params, '',
                                 SW_SHOWNORMAL, ewWaitUntilTerminated, Code, @TextureDownloadLog);
      RetryWithoutLocal := False;
      if Started and (Code = 5) and not TextureTransferCancelled then begin
        if WizardSilent then RetryWithoutLocal := True
        else RetryWithoutLocal := MsgBox('The texture.bin beside Setup does not match this texture pack.' + #13#10#13#10 +
          'Download the correct copy instead? Your local file will be kept.', mbConfirmation, MB_YESNO) = IDYES;
        if RetryWithoutLocal then LocalParam := '';
      end;
    until not RetryWithoutLocal;
    if TextureTransferCancelled or (Code = 2) then
      Result := 'Download stopped. Saved progress is kept. Retry to resume, or Cancel to close Setup.'
    else if not Started or (Code <> 0) or (TexturePayloadPath = '') then begin
      if TextureError = '' then TextureError := 'The texture pack could not be prepared. Retry to continue the saved download.';
      Result := TextureError;
    end else begin
      if not GetSpaceOnDisk64(AddBackslash(ExtractFileDrive(ExpandConstant('{app}'))), FreeBytes, TotalBytes) then
        Result := 'Could not check free space on the selected game drive.'
      else if FreeBytes < ({#TextureExtractedBytes} + OtherRequiredBytes) then
        Result := 'There is not enough free space to extract the textures and install the selected game files. The verified download is kept; choose another folder or free some space and retry.';
    end;
  finally
    TextureTransferActive := False;
    TextureProgress.Hide;
  end;
end;

procedure TextureFinishCacheChoice;
var Choice: String;
begin
  { ssDone is reached only after a successful install. Never offer to discard
    partial downloads or a verified BIN needed after failed extraction. }
  if not TexturePackInstalled or not TextureUsedCache or TextureCacheChoiceAsked then Exit;
  if (CompareText(TexturePayloadPath, TextureCacheDir + '\texture.bin') <> 0) or
     (CompareText(TexturePayloadPath, ExpandConstant('{src}\texture.bin')) = 0) or
     not FileExists(TexturePayloadPath) then Exit;
  TextureCacheChoiceAsked := True;
  Choice := ExpandConstant('{param:KEEPTEXTUREBIN|}');
  if Choice = '0' then TextureDeleteCacheOnExit := True
  else if (Choice = '1') or WizardSilent then TextureDeleteCacheOnExit := False
  else
    TextureDeleteCacheOnExit := MsgBox(
      'Keep the downloaded texture.bin for future repairs or reinstalls?' + #13#10#13#10 +
      'Yes keeps the cached download. No deletes it when Setup closes.',
      mbConfirmation, MB_YESNO) = IDNO;
  if TextureDeleteCacheOnExit then Log('The user chose to delete the cached texture.bin on Setup exit.')
  else Log('The cached texture.bin is kept for future repairs or reinstalls.');
end;

procedure TextureCleanupOnExit;
begin
  if not TextureDeleteCacheOnExit or not TexturePackInstalled or not TextureUsedCache then Exit;
  if (TextureCacheDir = '') or
     (CompareText(TexturePayloadPath, TextureCacheDir + '\texture.bin') <> 0) or
     (CompareText(TexturePayloadPath, ExpandConstant('{src}\texture.bin')) = 0) then Exit;
  { Delete only the completed cache file selected above. Partial data, installed
    textures and the user's sibling BIN are never cleanup targets. A busy file
    is left in place rather than interrupting another installer. }
  if FileExists(TexturePayloadPath) then begin
    if DeleteFile(TexturePayloadPath) then Log('Removed cached texture.bin on Setup exit.')
    else Log('Could not remove cached texture.bin; it may be in use by another installer.');
  end;
end;

function TextureExtractionProgress(const ArchiveName, FileName: String; const Progress, ProgressMax: Int64): Boolean;
var Tick: LongWord;
begin
  Tick := TextureTick;
  if (Tick - TextureLastUiTick >= 100) or (Progress = ProgressMax) then begin
    TextureLastUiTick := Tick;
    TextureProgress.SetText('Extracting hi-res textures...', ExtractFileName(FileName));
    TextureProgress.SetProgress(Progress div 1024, ProgressMax div 1024);
  end;
  Result := not TextureTransferCancelled;
end;

procedure InstallTexturePack;
begin
  if not WantTexturePack then Exit;
  if TexturePackAlreadyCurrent then Exit;
  if TexturePayloadPath = '' then RaiseException('The texture pack has not been verified.');
  TextureTransferCancelled := False;
  TextureExtractActive := True;
  TextureTransferActive := True;
  TextureLastUiTick := 0;
  TextureProgress.SetText('Extracting hi-res textures...', 'texture.bin');
  TextureProgress.SetProgress(0, 0);
  TextureProgress.Show;
  WizardForm.CancelButton.Enabled := True;
  WizardForm.CancelButton.Visible := True;
  try
    try
      { One streaming extraction avoids repeatedly decoding solid blocks for
        individual [Files] entries. Portable pack files need no uninstall log. }
      Log('Extracting the verified texture payload in one pass.');
      ExtractArchive(TexturePayloadPath, ExpandConstant('{app}'), '', True, @TextureExtractionProgress);
      TexturePackInstalled := True;
      Log('Texture pack extraction completed.');
    except
      if TextureTransferCancelled then
        RaiseException('Texture extraction stopped. The verified download is kept. Run Setup again to finish extracting the pack.')
      else
        RaiseException('Texture extraction could not finish. The verified download is kept. Run Setup again to retry. ' + GetExceptionMessage);
    end;
  finally
    TextureTransferActive := False;
    TextureExtractActive := False;
    TextureProgress.Hide;
  end;
end;
