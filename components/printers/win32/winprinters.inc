{%MainUnit ../osprinters.pas}
{**************************************************************
Implementation for winprinter
***************************************************************}
uses
  InterfaceBase, LCLIntf, WinVer, WinUtilPrn
  {todo: use WinSpool when it will be released with fpc, WinSpool};

// todo: this ^ is a mess: mixed WinUtilPrn/Windows units clean...

// todo: this should be a method, can not be atm because mixed units ^
{$IFDEF USEUNICODE}
function GetCurrentDevMode(out DM:PDeviceModeW): Boolean;
{$ELSE}
function GetCurrentDevMode(out DM:PDeviceMode): Boolean;
{$ENDIF}
var
  PDev: TPrinterDevice;
begin
  Result := false;
  if (Printer.Printers.Count > 0) then
  begin
    PDev := TPrinterDevice(Printer.Printers.Objects[Printer.PrinterIndex]);
    DM := PDev.DevMode;
    Result := DM <> nil;
  end;
end;

{ TWinPrinter }

constructor TWinPrinter.Create;
begin
  inherited Create;

  fLastHandleType := htNone;
  fPrinterHandle := 0; //None
end;

destructor TWinPrinter.Destroy;
begin
  fDestroying := true;

  ClearDC;

  DoResetPrintersList;

  if fPrinterHandle <> 0 then
    ClosePrinter(fPrinterHandle);

  inherited Destroy;
end;

function TWinPrinter.Write(const Buffer; Count: Integer;
  var Written: Integer): Boolean;
begin
  CheckRawMode(True);
  Result := WritePrinter(FPrinterHandle, @Buffer, Count, pdword(@Written));
end;

function TWinPrinter.GetHandlePrinter : HDC;
begin
  SetIC;
  Result := fDC;
end;


procedure TWinPrinter.SetHandlePrinter(aValue : HDC);
begin
  CheckRawMode(False);
  if aValue <> fDC then
  begin
    ClearDC;
    fDC := aValue;
    if Assigned(Canvas) then
      Canvas.Handle := fDC;
    fLastHandleType := htDC;
  end;
end;

procedure TWinPrinter.RawModeChanging;
begin
  // if old mode was standard free DC if it was created
  if not RawMode and (fDC <> 0) then
    FreeDC;
end;

procedure TWinPrinter.PrinterSelected;
begin
  if not fDestroying and (PrinterIndex >= 0) and not RawMode then
    SetDC;
end;

function TWinPrinter.GetXDPI: Integer;
begin
  Result:=72;
  if (Printers.Count > 0) and not RawMode then
  begin
    SetDC;
    Result:=windows.GetDeviceCaps(fDC, LOGPIXELSX);
  end;
end;

function TWinPrinter.GetYDPI: Integer;
begin
  Result:=72;
  if (Printers.Count>0) and not RawMode then
  begin
    SetDC;
    Result:=windows.GetDeviceCaps(fDC,LOGPIXELSY);
  end;
end;

procedure TWinPrinter.SetIC;
var PDev : TPrinterDevice;
begin
  if (fLastHandleType=htNone) and (Printers.Count>0) then
  begin
    PDev:=TPrinterDevice(Printers.Objects[PrinterIndex]);
    {$IFDEF USEUNICODE}
    fDC:=CreateICW(
      PWidechar(UTF8Decode(PDev.Driver)),
      PWidechar(UTF8Decode(PDev.Device)),
      PWidechar(UTF8Decode(PDev.Port)),
      PDev.DevMode);
    {$ELSE}
    fDC:=CreateIC(PChar(PDev.Driver),PChar(PDev.Device),
           PChar(PDev.Port),PDev.DevMode);
    {$ENDIF}
    if fDC=0 then
      {$IFDEF USEUNICODE}
      fDC:=CreateICW(
        PWidechar('WINSPOOL'),
        PWidechar(UTF8Decode(PDev.Device)),
        PWidechar(UTF8Decode(PDev.Port)),
        PDev.DevMode);
      {$ELSE}
      fDC:=CreateIC(PChar('WINSPOOL'),PChar(PDev.Device),
           PChar(PDev.Port),PDev.DevMode);
      {$ENDIF}
    if fDC=0 then
      raise EPrinter.Create(
        Format('Invalid printer (DC=%d Driver=%s Device=%s Port=%s)',
          [fDC,Pdev.Driver,PDev.Device,PDev.Port]));
          
    if Assigned(Canvas) then
      Canvas.Handle:=fDC;
      
    fLastHandleType:=htIC;
  end;
end;

procedure TWinPrinter.SetDC;
var PDev : TPrinterDevice;
begin

  if (fLastHandleType<>htDC) and (Printers.Count>0) then
  begin
    ClearDC;
    PDev:=TPrinterDevice(Printers.Objects[PrinterIndex]);
    try

    //Device is only 32 chars long,
    //if the Printername or share is longer than 32 chars, this will return 0
    {$IFDEF USEUNICODE}
    fDC := CreateDCW(nil, PWidechar(UTF8Decode(PDev.Name)), nil, PDev.DevMode);
    {$ELSE}
    fDC := CreateDC(nil, PChar(PDev.Name), nil, PDev.DevMode);
    {$ENDIF}
    if fDC=0 then
      {$IFDEF USEUNICODE}
      fDC := CreateDCW(PWidechar('WINSPOOL'),PWidechar(UTF8Decode(PDev.Name)), nil, PDev.DevMode);
      {$ELSE}
      fDC := CreateDC(PChar('WINSPOOL'),PChar(PDev.Name), nil, PDev.DevMode);
      {$ENDIF}

    {Workaround (hack) for Lexmark 1020 JetPrinter (Mono)}
    if fDC=0 then
      {$IFDEF USEUNICODE}
      fDC:=CreateDCW(nil,PWidechar(UTF8Decode(PDev.Driver)),nil, PDev.DevMode);
      {$ELSE}
      fDC:=CreateDC(nil,PChar(PDev.Driver),nil, PDev.DevMode);
      {$ENDIF}
    if fDC=0 then
      {$IFDEF USEUNICODE}
      fDC:=CreateDCW(PWideChar('WINSPOOL'),PWideChar(UTF8Decode(PDev.Driver)),nil,PDev.DevMode);
      {$ELSE}
      fDC:=CreateDC(pChar('WINSPOOL'),PChar(PDev.Driver),nil,PDev.DevMode);
      {$ENDIF}

    except on E:Exception do
      raise EPrinter.Create(Format('CreateDC Exception:"%s" (Error:"%s", '+
          'DC=%d Driver="%s" Device="%s" Port="%s")', [E.Message,
          SysErrorMessage(GetLastError),fDC, Pdev.Driver,
          Printers[PrinterIndex],PDev.Port]));
    end;
    
    if fDC=0 then
      raise EPrinter.Create(Format('Invalid printer (Error:%s, '+
          'DC=%d Driver="%s" Device="%s" Port="%s")',
          [SysErrorMessage(GetLastError),fDC,Pdev.Driver,Printers[PrinterIndex],
           PDev.Port]));
      
    if Assigned(Canvas) then
      Canvas.Handle:=fDC;
      
    fLastHandleType:=htDC;

  end;
end;

procedure TWinPrinter.ClearDC;
begin
  if not RawMode then
    FreeDC
end;

procedure TWinPrinter.FreeDC;
begin
  if Assigned(Canvas) then
    Canvas.Handle:=0;

  if fDC<>0 then
  begin
    DeleteDC(fDC);
    fDc := 0;
  end;

  fLastHandleType:=htNone;
end;

// Based on MS Article Q167345
function TWinPrinter.UpdateDevMode(APrinterIndex:Integer): boolean;
var
  PDev: TPrinterDevice;
  dwRet: Integer;
begin
  if FPrinterHandle=0 then begin
    result := false;
    exit;
  end;

  // now we have a right FPrinterHandle, get current printer settings
  PDev := TPrinterDevice(Printers.Objects[APrinterIndex]);

  // 1.	Determine the required size of the buffer from the device,
  //    and then allocate enough memory for it.
  {$IFDEF USEUNICODE}
  PDev.DevModeSize := DocumentPropertiesW(0, FPrinterHandle, Pwidechar(UTF8Decode(PDev.Name)),
                      nil, nil, 0);
  {$ELSE}
  PDev.DevModeSize := DocumentProperties(0, FPrinterHandle, pchar(PDev.Name),
                      nil, nil, 0);
  {$ENDIF}
  ReallocMem(Pdev.DevMode, PDev.DevModeSize);
  if PDev.DevModeSize=0 then begin
    result := false;
    exit;
  end;
  
  // 2.	Ask the device driver to initialize the DEVMODE buffer with
  //    the default settings.
  {$IFDEF USEUNICODE}
  dwRet := DocumentPropertiesW(0, FPrinterHandle, PWideChar(UTF8Decode(Pdev.Name)),
           PDev.DevMode, nil, DM_OUT_BUFFER);
  {$ELSE}
  dwRet := DocumentProperties(0, FPrinterHandle, pchar(Pdev.Name),
           PDev.DevMode, nil, DM_OUT_BUFFER);
  {$ENDIF}
  result := (dwRet=IDOK);
  if not result then begin
    ReallocMem(PDev.Devmode, 0);
    exit;
  end;
  
end;

procedure TWinPrinter.DoBeginDoc;
var
  Inf: TDocInfo;
  Doc1: DOC_INFO_1;
begin
  inherited DoBeginDoc;

  if fPrinterHandle = 0 then
    raise EPrinter.Create('Printer handle not defined');

  if RawMode then
  begin
    Doc1.pDocName := PChar(Title);
    if Filename <> '' then
      Doc1.pOutputFile := PChar(Filename)
    else
      Doc1.pOutputFile := nil;
    Doc1.pDataType := 'RAW';
    
    if StartDocPrinter(FPrinterHandle, 1, PByte(@Doc1)) = 0 then
    begin
      ClosePrinter(FPrinterHandle);
      FPrinterHandle := 0;
    end
    else
    if not StartPagePrinter(FPrinterHandle) then
    begin
      EndDocPrinter(FPrinterHandle);
      ClosePrinter(FPrinterHandle);
      FPrinterHandle := 0;
    end;
  end
  else
  begin
    SetDC;
    Canvas.Handle := fDC;
    Canvas.Refresh;

    FillChar(Inf, SizeOf(Inf), 0);
    Inf.cbSize := SizeOf(Inf);
    Inf.lpszDocName := PChar(Title);
    if FileName <> '' then
      Inf.lpszOutput := PChar(Filename);

    StartDoc(fDC,@Inf);
    StartPage(fDC);
  end;
end;

procedure TWinPrinter.DoNewPage;
begin
  inherited DoNewPage;
  
  if RawMode then begin

    EndPagePrinter(FPrinterHandle);
    StartPagePrinter(FPrinterHandle);
    
  end else begin
    EndPage(fDC);
    StartPage(fDC);
    Canvas.Refresh;
  end;
end;

procedure TWinPrinter.DoEndDoc(aAborded: Boolean);
begin
  inherited DoEndDoc(aAborded);

  if RawMode then begin

    EndPagePrinter(FPrinterHandle);
    EndDocPrinter(FPrinterHandle);
    {
    ClosePrinter(FPrinterHandle);
    FPrinterHandle:=0;
    }
  end else begin
    EndPage(fDC);
    if not aAborded then
      WinUtilPrn.EndDoc(fDC);
  end;
end;

procedure TWinPrinter.DoAbort;
begin
  inherited DoAbort;
  if RawMode then
    AbortPrinter(FPrinterHandle)
  else
    AbortDoc(fDC);
end;

function TWinPrinter.GetDefaultPrinter: string;

const
  MAXBUFSIZE = 512;

var
  Needed, PrtCount: DWORD;
  BoolRes: BOOL;
  IntRes: Integer;
  PrintInfo2Buf: PByte;
  GetDefPrnFunc: function(buffer: LPTSTR; var bufSize: DWORD): BOOL; stdcall;
  SpoolerHandle: HINST;
  AName: widestring;
begin
  // retrieve default printer using ms blessed method, see
  // see: http://support.microsoft.com/default.aspx?scid=kb;en-us;246772
  Result := '';
  if Win32Platform = VER_PLATFORM_WIN32_WINDOWS then
  begin
    // Get PRINT_INFO_2 record size
    SetLastError(0);
    {$IFDEF USEUNICODE}
    BoolRes := EnumPrintersW(PRINTER_ENUM_DEFAULT, nil, 2, nil, 0, @Needed, @PrtCount);
    {$ELSE}
    BoolRes := EnumPrinters(PRINTER_ENUM_DEFAULT, nil, 2, nil, 0,
                              @Needed, @PrtCount);
    {$ENDIF}
    if not BoolRes and
      ((GetLastError <> ERROR_INSUFFICIENT_BUFFER) or (Needed = 0)) then
      Exit;

    // Get PRINT_INFO_2 record
    GetMem(PrintInfo2Buf, Needed);
    {$IFDEF USEUNICODE}
    BoolRes := EnumPrintersW(PRINTER_ENUM_DEFAULT, nil, 2, PrintInfo2Buf,
                              Needed, @Needed, @PrtCount);
    {$ELSE}
    BoolRes := EnumPrinters(PRINTER_ENUM_DEFAULT, nil, 2, PrintInfo2Buf,
                              Needed, @Needed, @PrtCount);
    {$ENDIF}
    if not BoolRes then
    begin
      FreeMem(PrintInfo2Buf);
      Exit;
    end;

    {$IFDEF USEUNICODE}
    Result := UTF8Encode(widestring(PPRINTER_INFO_2(PrintInfo2Buf)^.pPrinterName));
    {$ELSE}
    Result := PPRINTER_INFO_2(PrintInfo2Buf)^.pPrinterName;
    Result := AnsiToUTF8(Result);
    {$ENDIF}
    FreeMem(PrintInfo2Buf);
  end
  else
  if Win32Platform=VER_PLATFORM_WIN32_NT then
  begin
    if Win32MajorVersion >=5 then
    begin
      // for Windows 2000 or later, use api GetDefaultPrinter
      // TODO: needs to check WindowsUnicodeSupport
      SpoolerHandle := LoadLibrary(LibWinSpool);
      if SpoolerHandle = 0 then
        Exit;
      {$IFDEF USEUNICODE}
      Pointer(GetDefPrnFunc) := GetProcAddress(SpoolerHandle, 'GetDefaultPrinterW');
      {$ELSE}
      Pointer(GetDefPrnFunc) := GetProcAddress(SpoolerHandle, 'GetDefaultPrinterA');
      {$ENDIF}
      if GetDefPrnFunc = nil then
      begin
        FreeLibrary(SpoolerHandle);
        Exit;
      end;
      Boolres := GetDefPrnFunc(nil, PrtCount);
      result := '';
      if (prtcount>0) then begin
        {$IFDEF USEUNICODE}
        SetLength(AName, PrtCount-1); // this includes the #0 terminator
        BoolRes := GetDefPrnFunc(@AName[1], prtCount);
        result := UTF8Encode(AName);
        {$ELSE}
        SetLength(Result, PrtCount); // make room for printer name
        BoolRes := GetDefPrnFunc(pchar(Result), prtCount);
        Result := AnsiToUTF8(Result);
        {$ENDIF}
      end;
      FreeLibrary(SpoolerHandle);
    end else
    begin
      // for NT, use GetProfileString
      SetLength(result, MAXBUFSIZE);
      IntRes := GetProfileString('windows', 'device', ',,,', PChar(result),
                                                                  MAXBUFSIZE);
      if (IntRes>0) and (pos(',',Result)<>0) then
        Result := AnsiToUTF8(copy(Result, 1, pos(',', Result)-1))
      else
        Result := ''
    end;
  end;


end;


//Enum all defined printers. First printer it's default
procedure TWinPrinter.DoEnumPrinters(Lst: TStrings);
var
  Flags          : DWORD;
  Level          : DWORD;
  PrtCount       : DWORD;
  Needed         : DWORD;
  Buffer         : PByte;
  InfoPrt        : PByte;
  i              : Integer;
  DefaultPrinter : string;
  PDev           : TPrinterDevice;
  {$IFDEF USEUNICODE}
  TmpDevMode     : PDeviceModeW;
  {$ELSE}
  TmpDevMode     : PDeviceMode;
  {$ENDIF}
  PrtStr         : string;
begin
  {$IFDEF NOPRINTERS}
  Lst.Clear;
  exit;
  {$ENDIF}
  DefaultPrinter := GetDefaultPrinter;

  Flags := PRINTER_ENUM_CONNECTIONS or PRINTER_ENUM_LOCAL;
  Level := 2;

  //Evaluate buffer size
  Needed := 0;
  {$IFDEF USEUNICODE}
  EnumPrintersW(Flags, nil, Level, nil, 0, @Needed, @PrtCount);
  {$ELSE}
  EnumPrinters(Flags, nil, Level, nil, 0, @Needed, @PrtCount);
  {$ENDIF}
  if Needed <> 0 then
  begin
    GetMem(Buffer, Needed);
    Fillchar(Buffer^, Needed, 0);
    try
      //Enumerate Printers
      {$IFDEF USEUNICODE}
      if EnumPrintersW(Flags, nil, Level, Buffer, Needed, @Needed, @PrtCount) then
      {$ELSE}
      if EnumPrinters(Flags, nil, Level, Buffer, Needed, @Needed, @PrtCount) then
      {$ENDIF}
      begin
        InfoPrt := Buffer;
        for i := 0 to PrtCount - 1 do
        begin
          if Level = 2 then
          begin
            PDev := TPrinterDevice.Create;
            {$IFDEF USEUNICODE}
            PDev.Name := UTF8Encode(widestring(PPRINTER_INFO_2(InfoPrt)^.pPrinterName));
            PDev.Driver := UTF8Encode(widestring(PPRINTER_INFO_2(InfoPrt)^.pDriverName));
            PDev.Port := UTF8Encode(widestring(PPRINTER_INFO_2(InfoPrt)^.pPortName));
            {$ELSE}
            PDev.Name := PPRINTER_INFO_2(InfoPrt)^.pPrinterName;
            PDev.Driver := PPRINTER_INFO_2(InfoPrt)^.pDriverName;
            PDev.Port := PPRINTER_INFO_2(InfoPrt)^.pPortName;
            {$ENDIF}
            TmpDevMode := PPRINTER_INFO_2(InfoPrt)^.pDevMode;
            if TmpDevMode <> nil then
            begin
              // the devmode structure obtained this way have two problems
              // 1. It's not the full devmode, because it doesn't have
              //    the private info
              // 2. It's not initialized with the current settings and
              //    have not extra settings at all.
              //
              // PDev.DevMode:=PPRINTER_INFO_2(InfoPrt)^.PDevMode^;
              {$IFDEF USEUNICODE}
              PDev.Device := UTF8Encode(widestring(TmpDevMode^.dmDeviceName));
              {$ELSE}
              PDev.Device := PChar(PByte(TmpDevMode^.dmDeviceName));
              {$ENDIF}
              PDev.DefaultPaper := TmpDevMode^.dmPaperSize;
              PDev.DefaultBin   := TmpDevMode^.dmDefaultSource;
            end
            else begin
              PDev.Device:='';
              PDev.DefaultPaper:=0;
              PDev.DefaultBin  := 0
            end;
            {$IFDEF USEUNICODE}
            PrtStr := PDev.Name;
            if CompareText(PrtStr, DefaultPrinter)<>0 then
            {$ELSE}
            PrtStr := AnsiToUTF8(PDev.Name);
            if AnsiCompareText(PrtStr,DefaultPrinter)<>0 then
            {$ENDIF}
              Lst.AddObject(PrtStr,PDev)
            else
            begin
              Lst.Insert(0,PrtStr);
              Lst.Objects[0]:=PDev;
            end;
            Inc(InfoPrt,SizeOf(_PRINTER_INFO_2A));
          end;
        end;
      end;
    finally
      FreeMem(Buffer);
    end;
  end;
end;

procedure TWinPrinter.DoResetPrintersList;
var i   : Integer;
    Obj : TObject;
begin
  for i:=0 to Printers.Count-1 do
  begin
    Obj:=Printers.Objects[i];
    Printers.Objects[i]:=nil;
    Obj.Free;
  end;
  inherited DoResetPrintersList;
end;

procedure TWinPrinter.DoEnumPapers(Lst: TStrings);
var 
    {$IFDEF USEUNICODE}
    Buffer   : PWideChar;
    {$ELSE}
    Buffer   : PChar;
    {$ENDIF}
    PaperN   : String;
    PaperC,i : Integer;
    Count    : Integer;
    PDev     : TPrinterDevice;
    ArPapers : Array[0..255] of Word;
begin
  inherited DoEnumPapers(Lst);

  if (Printers.Count>0) then
  begin
    PDev:=TPrinterDevice(Printers.Objects[PrinterIndex]);

    if fPrinterHandle=0 then
      SetPrinter(Printers.Strings[PrinterIndex]);

    if fPrinterHandle=0 then
      raise EPrinter.Create('Printer handle not defined');

    //Retreive the supported papers
    PaperC:=0;
    {$IFDEF USEUNICODE}
    Count := DeviceCapabilitiesW(
      PWidechar(UTF8Decode(Pdev.Name)),
      PWidechar(UTF8Decode(PDev.Port)), DC_PAPERNAMES, nil, nil);
    {$ELSE}
    Count := DeviceCapabilities(PChar(Pdev.Name), PCHar(PDev.Port), DC_PAPERNAMES, nil, nil);
    {$ENDIF}
    if Count<=0 then
      raise EPrinter.CreateFmt('DoEnumPapers<DC_PAPERNAMES> error : %d, (%s)',
          [GetLastError,SysErrorMessage(GetLastError)]);
    try
      {$IFDEF USEUNICODE}
      GetMem(Buffer,64*SizeOf(Widechar)*Count);
      PaperC := DeviceCapabilitiesW(
        PWidechar(UTF8Decode(Pdev.Name)),
        PWidechar(UTF8Decode(PDev.Port)),
        DC_PAPERNAMES,
        Buffer,
        nil);
      for i:=0 to PaperC-1 do
      begin
        PaperN:=UTF8Encode(Widestring(Buffer+i*64));
        Lst.Add(PaperN);
      end;
      {$ELSE}
      GetMem(Buffer,64*Count);
      PaperC:=DeviceCapabilities(PChar(Pdev.Name),PCHar(PDev.Port),
          DC_PAPERNAMES,Buffer,nil);
      for i:=0 to PaperC-1 do
      begin
        PaperN:=StrPas(Buffer+i*64);
        Lst.Add(PaperN);
      end;
      {$ENDIF}
    finally
      FreeMem(Buffer);
    end;

    //Retreive the code of papers
    FillChar(ArPapers,SizeOf(ArPapers),0);
    {$IFDEF USEUNICODE}
    PaperC:=DeviceCapabilitiesW(
      PWidechar(UTF8Decode(Pdev.Name)),
      PWidechar(UTF8Decode(PDev.Port)),
      DC_PAPERS,
      PWidechar(@ArPapers[0]),
      nil);
    {$ELSE}
    PaperC:=DeviceCapabilities(PChar(Pdev.Name),PChar(PDev.Port),
          DC_PAPERS,PChar(@ArPapers[0]),nil);
    {$ENDIF}
    if PaperC<=0 then
      raise EPrinter.CreateFmt('DoEnumPapers<DC_PAPERS> error : %d, (%s)',
          [GetLastError,SysErrorMessage(GetLastError)])
    else if PaperC>Lst.Count then
      PaperC := Lst.Count;
      
    for i:=0 to PaperC-1 do
      Lst.Objects[i]:=TObject(ptrint(ArPapers[i]));
  end;
end;

function TWinPrinter.DoGetPaperName: string;
var
  i    : Integer;
  {$IFDEF USEUNICODE}
  dm   : PDeviceModeW;
  {$ELSE}
  dm   : PDeviceMode;
  {$ENDIF}
begin
  Result:=inherited DoGetPaperName;
  if GetCurrentDevMode(dm) then
    with PaperSize.SupportedPapers do begin
      i := IndexOfObject(TObject(ptrInt(dm^.dmPaperSize)));
      if i>=0 then
        result := PaperSize.SupportedPapers[i];
    end;
end;

function TWinPrinter.DoGetDefaultPaperName: string;
var i    : Integer;
    PDev : TPrinterDevice;
begin
  Result:=inherited DoGetDefaultPaperName;

  if (Printers.Count>0) then
  begin
    PDev:=TPrinterDevice(Printers.Objects[PrinterIndex]);
    with PaperSize.SupportedPapers do begin
      i:=IndexOfObject(TObject(ptrint(PDev.DefaultPaper)));
      if i<>-1 then
        Result:= Strings[i];
    end;
  end;
end;

procedure TWinPrinter.DoSetPaperName(aName: string);
var i    : Integer;
    {$IFDEF USEUNICODE}
    dm   : PDeviceModeW;
    {$ELSE}
    dm   : PDeviceMode;
    {$ENDIF}
begin
  inherited DoSetPaperName(aName);
  if GetCurrentDevMode(dm) then begin
    i:=PaperSize.SupportedPapers.IndexOf(aName);
    if i<>-1 then begin
      ClearDC;
      dm^.dmPaperSize := SHORT(ptrint(PaperSize.SupportedPapers.Objects[i]));
    end;
  end;
end;

function TWinPrinter.DoGetPaperRect(aName: string; 
  var aPaperRc: TPaperRect): Integer;
var NSize, i : Integer;
    PDev     : TPrinterDevice;
    ArSizes  : Array[0..255] of TPoint;
begin
  Result:=Inherited DoGetPaperRect(aName,aPaperRc);

  if (Printers.Count>0) and not RawMode then
  begin
    // Information for physical sizes can be obtained for any paper supported
    // by the printer, the same is not true for printable paper size, this can
    // be obtained only(?) for currently selected paper.
    //
    if DoGetPaperName=AName then begin
      SetDC;
      with aPaperRC.PhysicalRect do begin
        Left  :=0;
        Top   :=0;
        Right :=Windows.GetDeviceCaps(fDC, PHYSICALWIDTH);
        Bottom:=Windows.GetDeviceCaps(fDC, PHYSICALHEIGHT);
      end;
      with aPaperRC.WorkRect do begin
        Left  :=Windows.GetDeviceCaps(fDC, PHYSICALOFFSETX);
        Top   :=Windows.GetDeviceCaps(fDC, PHYSICALOFFSETY);
        Right :=Left   + Windows.GetDeviceCaps(fDC, HORZRES);
        Bottom:=Top    + Windows.GetDeviceCaps(fDC, VERTRES);
      end;
    end else begin
      // for other papers return at least the physical size
      // note: old implementation was using DeviceCapabilities function with
      //       index DC_PAPERSIZE, unfortunately this returns dimensions in
      //       tenths of millimeter which is wrong, we need points (not font
      //       points, but printer "pixels" at current resolution).
      //
      PDev:=TPrinterDevice(Printers.Objects[PrinterIndex]);

      //Retreive the Width and Height of aName paper
      FillChar(ArSizes,SizeOf(ArSizes),0);
      NSize:=DeviceCapabilities(PChar(Pdev.Name),PChar(PDev.Port),
            DC_PAPERSIZE,PChar(@ArSizes[0]),nil);
      i:=PaperSize.SupportedPapers.IndexOf(aName);
      if (i>=0) and (i<NSize) and (NSize<>0) then
      begin
        aPaperRc.PhysicalRect:=Classes.Rect(0,0,ArSizes[i].X,ArSizes[i].Y);
        with aPaperRC.PhysicalRect do begin
          // convert from tenths of millimeter to points
          Right  := round(Right * XDPI / 254);
          Bottom := round(Bottom* YDPI / 254);
        end;
        aPaperRc.WorkRect := aPaperRC.PhysicalRect;
      end;
    end;
    Result:=1;
  end;
end;

function TWinPrinter.DoSetPrinter(aName: string): Integer;
var
  i: Integer;
  PDev: TPrinterDevice;
begin
  Result := inherited DoSetPrinter(aName);

  i := Printers.IndexOf(aName);
  if i <> -1 then
  begin
    ClearDC;

    if FPrinterHandle <> 0 then
      ClosePrinter(FPrinterHandle);

    if fDestroying then
       result := i
    else begin
      PDev := TPrinterDevice(Printers.Objects[i]);
      {$IFDEF USEUNICODE}
      if not OpenPrinterW(PWideChar(UTF8Decode(PDev.Name)), @fPrinterHandle, nil) then
      {$ELSE}
      if not OpenPrinter(PChar(PDev.Name), @fPrinterHandle, nil) then
      {$ENDIF}
      begin
        FprinterHandle := 0;
        raise EPrinter.CreateFmt('OpenPrinter exception : %s',
                                     [SysErrorMessage(GetlastError)]);
      end;

      if UpdateDevMode(i) then
        Result := i
      else
        Result := -1;
    end;
  end;
end;

function TWinPrinter.DoGetCopies: Integer;
var
  {$IFDEF USEUNICODE}
  dm: PDeviceModeW;
  {$ELSE}
  dm: PDeviceMode;
  {$ENDIF}
begin
  if GetCurrentDevMode(dm) then begin
    if dm^.dmCopies<>0 then
      result := dm^.dmCopies;
  end
  else
    Result:=inherited DoGetCopies;
end;

procedure TWinPrinter.DoSetCopies(aValue: Integer);
var
  {$IFDEF USEUNICODE}
  dm: PDeviceModeW;
  {$ELSE}
  dm: PDeviceMode;
  {$ENDIF}
begin
  inherited DoSetCopies(aValue);
  if (AValue>0) and GetCurrentDevMode(dm) then begin
    ClearDC;
    dm^.dmCopies := SHORT(aValue)
  end;
end;

function TWinPrinter.DoGetOrientation: TPrinterOrientation;
var
  {$IFDEF USEUNICODE}
  dm: PDeviceModeW;
  {$ELSE}
  dm: PDeviceMode;
  {$ENDIF}
begin
  Result:=inherited DoGetOrientation;
  if GetCurrentDevMode(dm) then begin
    case dm^.dmOrientation of
      DMORIENT_PORTRAIT : result:=poPortrait;
      DMORIENT_LANDSCAPE: result:=poLandscape;
    end;
  end;
end;

procedure TWinPrinter.DoSetOrientation(aValue: TPrinterOrientation);
var
  {$IFDEF USEUNICODE}
  dm: PDeviceModeW;
  {$ELSE}
  dm: PDeviceMode;
  {$ENDIF}
begin
  inherited DoSetOrientation(aValue);
  if GetCurrentDevMode(dm) then begin
    ClearDC;
    dm^.dmOrientation := Win32Orientations[aValue];
  end;
end;

function TWinPrinter.GetPrinterType: TPrinterType;
var
  Size: Dword;
  InfoPrt: Pointer;
begin
  Result := ptLocal;
  //On Win9X all printers are local
  if Win32Platform <> VER_PLATFORM_WIN32_NT then Exit;
  
  GetPrinter(fPrinterHandle, 4, nil, 0, @Size);
  GetMem(InfoPrt, Size);
  try
  if not GetPrinter(fPRinterHandle, 4, InfoPrt, Size, @Size)
  then
    raise EPrinter.CreateFmt('GetPrinterType failed : %s',
        [SysErrorMessage(GetLastError)]);
  if (PPRINTER_INFO_4(InfoPrt)^.Attributes and PRINTER_ATTRIBUTE_NETWORK)<>0 then
     Result := ptNetwork;
  finally
    FreeMem(InfoPrt);
  end;

end;


function TWinPrinter.DoGetPrinterState: TPrinterState;
var
  Size, Status, Jobs : DWord;
  InfoPrt: Pointer;
begin
  Result := psNoDefine;
  GetPrinter(fPrinterHandle, 2, nil, 0, @Size);
  GetMem(InfoPrt,Size);
  try
  if not GetPrinter(fPrinterHandle, 2, InfoPrt, Size, @Size)
  then
    raise EPrinter.CreateFmt('GetPrinterState failed : %s',
        [SysErrorMessage(GetLastError)]);

  Jobs := PPRINTER_INFO_2(InfoPrt)^.cJobs;
  Status := PPRINTER_INFO_2(InfoPrt)^.Status;
  case Status of
    0: Result := psReady;
    PRINTER_STATUS_PRINTING,
    PRINTER_STATUS_PROCESSING,
    PRINTER_STATUS_WARMING_UP,
    PRINTER_STATUS_WAITING,
    PRINTER_STATUS_IO_ACTIVE,
    PRINTER_STATUS_PENDING_DELETION,
    PRINTER_STATUS_INITIALIZING: Result := psPrinting;
    PRINTER_STATUS_PAPER_JAM,
    PRINTER_STATUS_PAPER_OUT,
    PRINTER_STATUS_PAPER_PROBLEM,
    PRINTER_STATUS_USER_INTERVENTION,
    PRINTER_STATUS_NO_TONER,
    PRINTER_STATUS_ERROR,
    PRINTER_STATUS_DOOR_OPEN,
    PRINTER_STATUS_PAGE_PUNT,
    PRINTER_STATUS_OUT_OF_MEMORY,
    PRINTER_STATUS_PAUSED: Result := psStopped;
  end;
   
  if (Result = psReady) and (Jobs > 0) then
    Result := psPrinting;
  finally
    FreeMem(InfoPrt);
  end;
end;

function TWinPrinter.GetCanPrint: Boolean;
begin
 Result := (DoGetPrinterState <> psStopped);
end;

function TWinPrinter.GetCanRenderCopies: Boolean;
var
 pDev : TPrinterDevice;
  Count : Integer;
begin
  if (Printers.Count>0) then
  begin
    PDev:=TPrinterDevice(Printers.Objects[PrinterIndex]);
    {$IFDEF USEUNICODE}
    Count := DeviceCapabilitiesW(
      PWidechar(UTF8Decode(Pdev.Name)),
      PWidechar(UTF8Decode(PDev.Port)),
      DC_COPIES,
      nil,PDev.DevMode);
    {$ELSE}
    Count := DeviceCapabilities(PChar(Pdev.Name),PCHar(PDev.Port),
                                DC_COPIES,nil,PDev.DevMode);
    {$ENDIF}
    Result := (Count>1);
  end
  else
    Result := inherited GetCanRenderCopies;
end;

procedure TWinPrinter.AdvancedProperties;
var
  PDev: TPrinterDevice;
begin
  if Printers.Count>0 then
  begin
    PDev:=TPrinterDevice(Printers.Objects[PrinterIndex]);
    {$IFDEF USEUNICODE}
    DocumentPropertiesW(
      Widgetset.AppHandle,
      FPrinterHandle,
      PWidechar(UTF8Decode(PDev.Name)),
      Pdev.DevMode, Pdev.DevMode,
      DM_OUT_BUFFER or DM_IN_BUFFER or DM_IN_PROMPT);
    {$ELSE}
    DocumentProperties(
      Widgetset.AppHandle,
      FPrinterHandle,
      pchar(PDev.Name),
      Pdev.DevMode, Pdev.DevMode,
      DM_OUT_BUFFER or DM_IN_BUFFER or DM_IN_PROMPT);
    {$ENDIF}
    //PrinterProperties(Widgetset.AppHandle,fPrinterHandle)
  end;
end;

procedure TWinPrinter.DoEnumBins(Lst : TStrings);
var
    {$IFDEF USEUNICODE}
    Buffer   : PWideChar;
    {$ELSE}
    Buffer   : PChar;
    {$ENDIF}
    BinN   : String;
    BinC,i : Integer;
    Count  : Integer;
    PDev   : TPrinterDevice;
    arBins : Array[0..255] of Word;
begin

  if Lst=nil then
    exit;

  Lst.Clear;

  if (Printers.Count>0) then
  begin
    PDev:=TPrinterDevice(Printers.Objects[PrinterIndex]);

    if fPrinterHandle=0 then
      SetPrinter(Printers.Strings[PrinterIndex]);

    if fPrinterHandle=0 then
      raise EPrinter.Create('Printer handle not defined');

    //Retreive the supported bins
    BinC:=0;
    {$IFDEF USEUNICODE}
    Count := DeviceCapabilitiesW(
      PWidechar(UTF8Decode(Pdev.Name)),
      PWidechar(UTF8Decode(PDev.Port)), DC_BINNAMES, nil, nil);
    {$ELSE}
    Count := DeviceCapabilities(PChar(Pdev.Name), PCHar(PDev.Port), DC_BINNAMES, nil, nil);
    {$ENDIF}
    if Count<=0 then
      raise EPrinter.CreateFmt('DoEnumBins<DC_BINNAMES> error : %d, (%s)',
          [GetLastError,SysErrorMessage(GetLastError)]);

    try
      {$IFDEF USEUNICODE}
      GetMem(Buffer,24*SizeOf(Widechar)*Count);
      BinC := DeviceCapabilitiesW(
        PWidechar(UTF8Decode(Pdev.Name)),
        PWidechar(UTF8Decode(PDev.Port)),
        DC_BINNAMES,
        Buffer,
        nil);
      for i:=0 to BinC-1 do
      begin
        BinN:=UTF8Encode(Widestring(Buffer+i*24));
        Lst.Add(BinN);
      end;
      {$ELSE}
      GetMem(Buffer,24*Count);
      BinC:=DeviceCapabilities(PChar(Pdev.Name),PCHar(PDev.Port),
          DC_BINNAMES,Buffer,nil);
      for i:=0 to BinC-1 do
      begin
        BinN:=StrPas(Buffer+i*24);
        Lst.Add(BinN);
      end;
      {$ENDIF}
    finally
      FreeMem(Buffer);
    end;

    //Retreive the code of bins
    FillChar(arBins,SizeOf(arBins),0);
    {$IFDEF USEUNICODE}
    BinC:=DeviceCapabilitiesW(
      PWidechar(UTF8Decode(Pdev.Name)),
      PWidechar(UTF8Decode(PDev.Port)),
      DC_BINS,
      PWidechar(@ArBins[0]),
      nil);
    {$ELSE}
    BinC:=DeviceCapabilities(PChar(Pdev.Name),PChar(PDev.Port),
          DC_BINS,PChar(@ArBins[0]),nil);
    {$ENDIF}
    if BinC<=0 then
      raise EPrinter.CreateFmt('DoEnumBinss<DC_BINS> error : %d, (%s)',
          [GetLastError,SysErrorMessage(GetLastError)])
    else if BinC>Lst.Count then
      BinC := Lst.Count;

    for i:=0 to BinC-1 do
      Lst.Objects[i]:=TObject(ptrint(arBins[i]));
  end;
end;

function TWinPrinter.DoGetDefaultBinName: string;
var i    : Integer;
    PDev : TPrinterDevice;
begin
  Result:=inherited DoGetDefaultBinName;

  with SupportedBins do
    if (Printers.Count>0) then
    begin
      PDev:=TPrinterDevice(Printers.Objects[PrinterIndex]);
      i:=IndexOfObject(TObject(ptrint(PDev.DefaultBin)));
      if i<>-1 then
        Result:= Strings[i];
    end;
end;

function TWinPrinter.DoGetBinName: string;
var
  i    : Integer;
  {$IFDEF USEUNICODE}
  dm: PDeviceModeW;
  {$ELSE}
  dm: PDeviceMode;
  {$ENDIF}
begin
  Result:=inherited DoGetBinName;
  if GetCurrentDevMode(dm) then
    with SupportedBins do begin
      i := IndexOfObject(TObject(ptrInt(dm^.dmDefaultSource)));
      if i>=0 then
        result := Strings[i];
    end;
end;

procedure TWinPrinter.DoSetBinName(aName: string);
var
  i    : Integer;
  {$IFDEF USEUNICODE}
  dm: PDeviceModeW;
  {$ELSE}
  dm: PDeviceMode;
  {$ENDIF}
begin
  with SupportedBins do begin

    if not GetCurrentDevMode(dm) then
      raise EPrinter.Create('DoSetBinName error : unable to get current DevMode');

    i := IndexOf(aName);
    if (i>=0) then begin
      ClearDC;
      dm^.dmDefaultSource := SHORT(ptrint(Objects[i]));
    end else
      inherited DoSetBinName(aName); // handle uknown bin name

  end;
end;

initialization
  Printer:=TWinPrinter.Create;

{end.}
