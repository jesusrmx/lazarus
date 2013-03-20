unit Unit1;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, FileUtil, Forms, Controls, Graphics, Dialogs, Grids,
  StdCtrls;

type

  { TForm1 }

  TForm1 = class(TForm)
    Button1: TButton;
    btn100: TButton;
    btn2: TButton;
    CheckBox1: TCheckBox;
    StringGrid1: TStringGrid;
    procedure Button1Click(Sender: TObject);
    procedure btn100Click(Sender: TObject);
    procedure CheckBox1Click(Sender: TObject);
    procedure FormCreate(Sender: TObject);
  private
    { private declarations }
  public
    { public declarations }
  end;

var
  Form1: TForm1;

implementation

{$R *.lfm}

{ TForm1 }

procedure TForm1.Button1Click(Sender: TObject);
begin
  Stringgrid1.Report;
end;

procedure TForm1.btn100Click(Sender: TObject);
begin
  Stringgrid1.ColWidths[TComponent(Sender).Tag] := -1;
end;

procedure TForm1.CheckBox1Click(Sender: TObject);
begin
  Stringgrid1.NegativeColRowSizeHide := Checkbox1.Checked;
end;

procedure TForm1.FormCreate(Sender: TObject);
var
  i: Integer;
begin
  Checkbox1.Checked := StringGrid1.NegativeColRowSizeHide;
  for i:=StringGrid1.FixedCols to Stringgrid1.ColCount-1 do
    Stringgrid1.Cells[i,0] := Chr( ord('A') + i-StringGrid1.FixedCols);
end;

end.

