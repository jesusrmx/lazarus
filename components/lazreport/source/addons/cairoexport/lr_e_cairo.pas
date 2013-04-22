unit lr_e_cairo;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, LResources, Forms, Controls, Graphics, Dialogs,
  lr_class, lr_barc, lr_rrect, lr_shape, CairoCanvas, CairoPrinter;

type
  TShapeData = record
    ShapeType: TfrShapeType;
    FillColor: TColor;
    FrameStyle: TfrFrameStyle;
    FrameWidth: Double;
    FrameColor: TColor;
    Radius: Single;
    Corners: TCornerSet;
  end;

  TlrCairoExport = class(TComponent)
  end;

  TlrCairoBackend = (cePDF, cePS);

  { TlrCairoExportFilter }

  TlrCairoExportFilter = class(TfrExportFilter)
  private
    NewPage: Boolean;
    fCairoPrinter: TCairoFilePrinter;
    FPageNo : Integer;
    DummyControl: TForm;
    procedure AddShape(Data: TShapeData; x, y, h, w: integer);
    procedure DefaultShowView(View: TfrView; nx, ny, ndy, ndx: Integer);
    function GetBackend: TlrCairoBackend;
    function GetResolution: Integer;
    procedure SetBackend(AValue: TlrCairoBackend);
    procedure SetResolution(AValue: Integer);
  public
    constructor Create(AStream: TStream); override;
    destructor Destroy; override;
    procedure OnBeginPage; override;
    procedure OnEndPage; override;
    procedure OnBeginDoc; override;
    procedure OnEndDoc; override;
    procedure ShowBackGround(View: TfrView; x, y, h, w: integer);
    procedure Frame(View: TfrView; x, y, h, w: integer);
    procedure ShowFrame(View: TfrView; x, y, h, w: integer);
    procedure ShowBarCode(View: TfrBarCodeView; x, y, h, w: integer);
    procedure ShowPicture(View: TfrPictureView; x, y, h, w: integer);
    procedure ShowRoundRect(View: TfrRoundRectView; x, y, h, w: integer);
    procedure ShowShape(View: TfrShapeView; x, y, h, w: integer);
    procedure OnText(X, Y: Integer; const Text: string; View: TfrView); override;
    procedure OnData(x, y: Integer; View: TfrView); override;
  public
    property Resolution: Integer read GetResolution write SetResolution;
    property Backend: TlrCairoBackend read GetBackend write SetBackend;
  end;

implementation


{ TlrCairoExportFilter }

procedure TlrCairoExportFilter.AddShape(Data: TShapeData; x, y, h, w: integer);
begin

end;

procedure TlrCairoExportFilter.DefaultShowView(View: TfrView; nx, ny, ndy,
  ndx: Integer);
begin

end;

function TlrCairoExportFilter.GetBackend: TlrCairoBackend;
begin
  case fCairoPrinter.CairoBackend of
    cbPS:
      result := cePS;
    else
      result := cePDF;
  end;
end;

function TlrCairoExportFilter.GetResolution: Integer;
begin
  result := fCairoPrinter.YDPI;
end;

procedure TlrCairoExportFilter.SetBackend(AValue: TlrCairoBackend);
begin
  if Backend=AValue then
    Exit;

  case AValue of
    cePS:
      fCairoPrinter.CairoBackend := cbPS;
    else
      fCairoPrinter.CairoBackend := cbPDF;
  end;
end;

procedure TlrCairoExportFilter.SetResolution(AValue: Integer);
begin
  if Resolution=AValue then
    Exit;
  fCairoPrinter.XDPI:=AValue;
  fCairoPrinter.YDPI:=AValue;
end;

constructor TlrCairoExportFilter.Create(AStream: TStream);
var
  ext: string;
begin
  inherited Create(AStream);
  fCairoPrinter := TCairoFilePrinter.create;
  fCairoPrinter.Stream := AStream;
  fCairoPrinter.CairoBackend := cbPDF;
  if AStream is TFileStream then begin
    ext := ExtractFileExt(TFileStream(AStream).FileName);
    case lowercase(ext) of
      '.ps':
        Backend := cePS;
      else
        Backend := cePDF;
    end;
  end;

end;

destructor TlrCairoExportFilter.Destroy;
begin
  fCairoPrinter.Free;
  inherited Destroy;
end;

procedure TlrCairoExportFilter.OnBeginPage;
begin
  inc(fPageNo);

  if fPageNo>1 then
    fCairoPrinter.NewPage;

  fCairoPrinter.PaperHeight := CurReport.EMFPages[FPageNo - 1]^.PrnInfo.PPgh;
  fCairoPrinter.PaperWidth := CurReport.EMFPages[FPageNo - 1]^.PrnInfo.PPgw;

  // TODO: non paged backends ...
end;

procedure TlrCairoExportFilter.OnEndPage;
begin
  inherited OnEndPage;
end;

procedure TlrCairoExportFilter.OnBeginDoc;
begin
  fCairoPrinter.BeginDoc;
  fCairoPrinter.Canvas.Handle; // make sure handle is created
end;

procedure TlrCairoExportFilter.OnEndDoc;
begin
  fCairoPrinter.EndDoc;
end;

procedure TlrCairoExportFilter.ShowBackGround(View: TfrView; x, y, h, w: integer
  );
begin

end;

procedure TlrCairoExportFilter.Frame(View: TfrView; x, y, h, w: integer);
begin

end;

procedure TlrCairoExportFilter.ShowFrame(View: TfrView; x, y, h, w: integer);
begin

end;

procedure TlrCairoExportFilter.ShowBarCode(View: TfrBarCodeView; x, y, h,
  w: integer);
begin

end;

procedure TlrCairoExportFilter.ShowPicture(View: TfrPictureView; x, y, h,
  w: integer);
begin

end;

procedure TlrCairoExportFilter.ShowRoundRect(View: TfrRoundRectView; x, y, h,
  w: integer);
begin

end;

procedure TlrCairoExportFilter.ShowShape(View: TfrShapeView; x, y, h, w: integer
  );
begin

end;

procedure TlrCairoExportFilter.OnText(X, Y: Integer; const Text: string;
  View: TfrView);
begin
  inherited OnText(X, Y, Text, View);
end;

procedure TlrCairoExportFilter.OnData(x, y: Integer; View: TfrView);
begin
  inherited OnData(x, y, View);
end;

initialization
    frRegisterExportFilter(TlrCairoExportFilter, 'Cairo Adobe Acrobat PDF (*.pdf)', '*.pdf');
    frRegisterExportFilter(TlrCairoExportFilter, 'Cairo Postscript (*.ps)', '*.ps');

end.
