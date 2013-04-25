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
    ScaleX,ScaleY: Double;
    DataRect: TRect;
    procedure AddShape(Data: TShapeData; x, y, h, w: integer);
    procedure DefaultShowView(View: TfrView; nx, ny, ndy, ndx: Integer);
    function GetBackend: TlrCairoBackend;
    procedure SetBackend(AValue: TlrCairoBackend);
    procedure DbgPoint(x, y: Integer; color: TColor; delta:Integer=5);
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
    property Backend: TlrCairoBackend read GetBackend write SetBackend;
  end;

implementation


{ TlrCairoExportFilter }

procedure TlrCairoExportFilter.AddShape(Data: TShapeData; x, y, h, w: integer);
var
  points: array of tpoint;
begin
  fCairoPrinter.Canvas.Brush.Color := Data.FillColor;
  fCairoPrinter.Canvas.Pen.Color := Data.FrameColor;
  fCairoPrinter.Canvas.Pen.Style := TPenStyle(Data.FrameStyle);
  fCairoPrinter.Canvas.Pen.Width:=round(Data.FrameWidth-0.5);

  with fCairoPrinter.Canvas do
  case Data.ShapeType of
    frstRectangle:
      Rectangle(x, y, x+w, y+h);

    frstEllipse:
      Ellipse(x, y, x+w, y+h);

    frstRoundRect:
      begin
        RoundRect(x, y, x+w, y+h, round(data.Radius), round(data.Radius));
        // TODO: SquaredCorners
      end;

    frstTriangle:
      begin
        SetLength(Points, 3);
        Points[0] := point(x+w, y+h);
        Points[1] := point(x, y+h);
        Points[2] := point(round(x+w/2), y);
        Polygon(Points);
        SetLength(Points, 0);
      end;

    frstDiagonal1:
      begin
        SetLength(Points, 2);
        Points[0] := Point(x, y);
        Points[1] := Point(x+w, y+h);
        Polygon(Points);
        SetLength(Points, 0);
      end;

    frstDiagonal2:
      begin
        SetLength(Points, 2);
        Points[0] := Point(x, y+h);
        Points[1] := Point(x+w, y);
        Polygon(Points);
        SetLength(Points, 0);
      end;
  end;
end;

procedure TlrCairoExportFilter.DefaultShowView(View: TfrView; nx, ny, ndy,
  ndx: Integer);
begin
  if (View.FillColor <> clNone)
     and not (View is TfrBarCodeView)
     and not (View is TfrPictureView)
  then
    ShowBackGround(View, nx, ny, ndy, ndx);

  if View is TfrBarCodeView then
      ShowBarCode(TfrBarCodeView(View), nx, ny, ndy, ndx)
  else if View is TfrPictureView then
      ShowPicture(TfrPictureView(View), nx, ny, ndy, ndx);

  if (View.Frames<>[]) and not (View is TfrBarCodeView) then
     ShowFrame(View, nx, ny, ndy, ndx);
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

procedure TlrCairoExportFilter.DbgPoint(x, y: Integer; color: TColor; delta:Integer=5);
begin
  fCairoPrinter.Canvas.Brush.Color := color;
  fCairoPrinter.Canvas.Brush.Style := bsSolid;
  fCairoPrinter.Canvas.Ellipse(x-Delta, y-Delta, x+Delta, y+Delta);
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

  with CurReport.EMFPages[FPageNo - 1]^ do begin
    fCairoPrinter.PaperHeight := PrnInfo.PPgh;
    fCairoPrinter.PaperWidth := PrnInfo.PPgw;
  end;

  // TODO: non paged backends ...
end;

procedure TlrCairoExportFilter.OnEndPage;
begin
  inherited OnEndPage;
end;

procedure TlrCairoExportFilter.OnBeginDoc;
begin
  fCairoPrinter.XDPI := CurReport.EMFPages[0]^.Page.PrnInfo.ResX;
  fCairoPrinter.YDPI := CurReport.EMFPages[0]^.Page.PrnInfo.ResY;
  ScaleX := fCairoPrinter.XDPI / (93 / 1.022);  // scaling factor X Printer DPI / Design Screen DPI
  ScaleY := fCairoPrinter.YDPI / (93 / 1.015);  // "              Y "
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
  fCairoPrinter.Canvas.Brush.Style:=bsSolid;
  fCairoPrinter.Canvas.Brush.Color:=View.FillColor;
  fCairoPrinter.Canvas.FillRect(x, y, x+w, y+h);
end;

procedure TlrCairoExportFilter.Frame(View: TfrView; x, y, h, w: integer);
begin
  fCairoPrinter.Canvas.Pen.Style:=TPenStyle(View.FrameStyle);
  fCairoPrinter.Canvas.Pen.Color:=View.FrameColor;
  fCairoPrinter.Canvas.Pen.Width:=round(View.FrameWidth - 0.5);
  fCairoPrinter.Canvas.Frame(x, y, x+w, y+h);
end;

procedure TlrCairoExportFilter.ShowFrame(View: TfrView; x, y, h, w: integer);
begin
  if ([frbLeft,frbTop,frbRight,frbBottom]-View.Frames=[]) and
     (View.FrameStyle = frsSolid) then
  begin
    Frame(View, x, y, h, w);
  end
  else
  begin
    if frbRight in View.Frames then
      Frame(View, x + w - 1, y, h, 0); //Right
    if frbLeft in View.Frames then
      Frame(View, x, y, h, 0); //Left
    if frbBottom in View.Frames then
      Frame(View, x, y + h - 1, 0, w); //Botton
    if frbTop in View.Frames then
      Frame(View, x, y, 0, w); //Top
  end;
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
var
  Data: TShapeData;
  SWidth: Integer;
begin

  if view.ShowGradian then
    // not supported yet
    DefaultShowView(View, x, y, h, w)

  else
  begin

    SWidth := trunc((View.RoundRectCurve/2) * ScaleX + 0.5);
    if View.RoundRect then
      Data.Radius := SWidth
    else
      Data.Radius := 0.0;
    Data.Corners:=View.SquaredCorners;

    // draw shadow
    Data.ShapeType := frstRoundRect;
    Data.FillColor := ColorToRGB(View.ShadowColor);
    Data.FrameColor := Data.FillColor; //ColorToRGB(View.FrameColor);
    Data.FrameWidth := 0;
    Data.FrameStyle := frsSolid;
    SWidth := trunc(View.ShadowWidth * ScaleX + 0.5);
    if View.ShadowWidth>0 then
      AddShape(Data, x + SWidth, y + SWidth, h - SWidth, w - SWidth);

    // draw roundrect
    Data.ShapeType := frstRoundRect;
    if View.FillColor=clNone then
      Data.FillColor := clNone
    else
      Data.FillColor := ColorToRGB(View.FillColor);
    if View.Frames=[] then
      Data.FrameColor := Data.FillColor
    else
      Data.FrameColor := ColorToRGB(View.FrameColor);
    Data.FrameWidth := View.FrameWidth;
    Data.FrameStyle := View.FrameStyle;
    AddShape(Data, x, y, h - SWidth, w - SWidth);
  end;
end;

procedure TlrCairoExportFilter.ShowShape(View: TfrShapeView; x, y, h, w: integer
  );
var
  Data: TShapeData;
begin
  Data.ShapeType := View.ShapeType;
  Data.FillColor := View.FillColor;
  Data.FrameColor := View.FrameColor;
  Data.FrameStyle := View.FrameStyle;
  Data.FrameWidth := View.FrameWidth;
  Data.Radius := -1.0;
  Data.Corners := [];
  AddShape(Data, x, y, h, w);
end;

type
  TfrMemoView_ = class(TfrMemoView);

procedure TlrCairoExportFilter.OnText(X, Y: Integer; const Text: string;
  View: TfrView);
var
  nx, ny: Integer;
  aStyle: TTextStyle;
begin

  aStyle := fCairoPrinter.Canvas.TextStyle;
  aStyle.Alignment:=TfrMemoView_(View).Alignment;
  aStyle.Layout:=TfrMemoView_(View).Layout;

  fCairoPrinter.Canvas.Font := TfrMemoView_(View).Font;
  fCairoPrinter.Canvas.Font.Orientation := (View as TfrMemoView).Angle * 10;

  nx := DataRect.Left;
  if fCairoPrinter.Canvas.Font.Orientation<>0 then
    ny := DataRect.Bottom
  else
    ny := DataRect.Top;
  fCairoPrinter.Canvas.TextRect(DataRect, nx, ny, Text, aStyle);

end;

procedure TlrCairoExportFilter.OnData(x, y: Integer; View: TfrView);
var
  nx, ny, ndx, ndy: Integer;
begin

  nx := Trunc( x * ScaleX + 0.5 );
  ny := Trunc( y * ScaleY + 0.5 );
  ndx := Trunc( View.dx * ScaleX + 1.5 );
  ndy := Trunc( View.dy * ScaleY + 1.5 );

  DataRect := Rect(nx, ny, nx+ndx, ny+ndy);

  if View is TfrShapeView then begin

    ShowShape(TfrShapeView(View), nx, ny, ndy, ndx);

  end else
  if View is TfrRoundRectView then begin

    ShowRoundRect(TfrRoundRectView(View), nx, ny, ndy, ndx);

  end else
    DefaultShowView(View, nx, ny, ndy, ndx);
end;

initialization
    frRegisterExportFilter(TlrCairoExportFilter, 'Cairo Adobe Acrobat PDF (*.pdf)', '*.pdf');
    frRegisterExportFilter(TlrCairoExportFilter, 'Cairo Postscript (*.ps)', '*.ps');

end.