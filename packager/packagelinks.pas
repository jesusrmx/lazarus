{  $Id$  }
{
 /***************************************************************************
                            packagelinks.pas
                            ----------------


 ***************************************************************************/

 ***************************************************************************
 *                                                                         *
 *   This source is free software; you can redistribute it and/or modify   *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This code is distributed in the hope that it will be useful, but      *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   General Public License for more details.                              *
 *                                                                         *
 *   A copy of the GNU General Public License is available on the World    *
 *   Wide Web at <http://www.gnu.org/copyleft/gpl.html>. You can also      *
 *   obtain it by writing to the Free Software Foundation,                 *
 *   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.        *
 *                                                                         *
 ***************************************************************************

  Author: Mattias Gaertner

  Abstract:
    Package links helps the IDE to find package filenames by name
}
unit PackageLinks;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, AVL_Tree, Laz_XMLCfg, FileCtrl, IDEProcs, EnvironmentOpts,
  PackageDefs, LazConf;
  
type
  TIteratePackagesEvent =
    procedure(APackage: TLazPackageID) of object;


  { TPackageLink }

  TPkgLinkOrigin = (
    ploGlobal,
    ploUser
    );
  TPkgLinkOrigins = set of TPkgLinkOrigin;

  TPackageLink = class(TLazPackageID)
  private
    FFilename: string;
    FOrigin: TPkgLinkOrigin;
    procedure SetFilename(const AValue: string);
    procedure SetOrigin(const AValue: TPkgLinkOrigin);
  public
    constructor Create;
    destructor Destroy; override;
  public
    property Origin: TPkgLinkOrigin read FOrigin write SetOrigin;
    property Filename: string read FFilename write SetFilename;
  end;


  { TPackageLinks }
  
  TPkgLinksState = (
    plsUserLinksNeedUpdate,
    plsGlobalLinksNeedUpdate
    );
  TPkgLinksStates = set of TPkgLinksState;

  TPackageLinks = class
  private
    FGlobalLinks: TAVLTree; // tree of TPackageLink
    FModified: boolean;
    FUserLinks: TAVLTree; // tree of TPackageLink
    fUpdateLock: integer;
    FStates: TPkgLinksStates;
    function FindLeftMostNode(LinkTree: TAVLTree;
      const PkgName: string): TAVLTreeNode;
    function FindLinkWithPkgNameInTree(LinkTree: TAVLTree;
      const PkgName: string): TPackageLink;
    function FindLinkWithDependencyInTree(LinkTree: TAVLTree;
      Dependency: TPkgDependency): TPackageLink;
    function FindLinkWithPackageIDInTree(LinkTree: TAVLTree;
      APackageID: TLazPackageID): TPackageLink;
    procedure IteratePackagesInTree(LinkTree: TAVLTree;
      Event: TIteratePackagesEvent);
    procedure SetModified(const AValue: boolean);
  public
    UserLinkLoadTime: longint;
    UserLinkLoadTimeValid: boolean;
    constructor Create;
    destructor Destroy; override;
    procedure Clear;
    function GetUserLinkFile: string;
    procedure UpdateGlobalLinks;
    procedure UpdateUserLinks;
    procedure UpdateAll;
    procedure BeginUpdate;
    procedure EndUpdate;
    procedure SaveUserLinks;
    function FindLinkWithPkgName(const PkgName: string): TPackageLink;
    function FindLinkWithDependency(Dependency: TPkgDependency): TPackageLink;
    function FindLinkWithPackageID(APackageID: TLazPackageID): TPackageLink;
    procedure IteratePackages(Event: TIteratePackagesEvent);
    procedure AddUserLink(APackage: TLazPackage);
    procedure RemoveLink(APackageID: TLazPackageID);
  public
    property Modified: boolean read FModified write SetModified;
  end;
  
var
  PkgLinks: TPackageLinks;


implementation


const
  UserPkgLinkFile = 'packagefiles.xml';

function ComparePackageLinks(Data1, Data2: Pointer): integer;
var
  Link1: TPackageLink;
  Link2: TPackageLink;
begin
  Link1:=TPackageLink(Data1);
  Link2:=TPackageLink(Data2);
  Result:=Link1.Compare(Link2);
end;

function ComparePackageIDAndLink(Key, Data: Pointer): integer;
var
  Link: TPackageLink;
  PkgID: TLazPackageID;
begin
  if Key=nil then
    Result:=-1
  else begin
    PkgID:=TLazPackageID(Key);
    Link:=TPackageLink(Data);
    Result:=PkgID.Compare(Link);
  end;
end;

function ComparePkgNameAndLink(Key, Data: Pointer): integer;
var
  PkgName: String;
  Link: TPackageLink;
begin
  if Key=nil then
    Result:=-1
  else begin
    PkgName:=AnsiString(Key);
    Link:=TPackageLink(Data);
    Result:=AnsiCompareText(PkgName,Link.Name);
  end;
end;

{ TPackageLink }

procedure TPackageLink.SetOrigin(const AValue: TPkgLinkOrigin);
begin
  if FOrigin=AValue then exit;
  FOrigin:=AValue;
end;

procedure TPackageLink.SetFilename(const AValue: string);
begin
  if FFilename=AValue then exit;
  FFilename:=CleanAndExpandFilename(AValue);
end;

constructor TPackageLink.Create;
begin
  inherited Create;
end;

destructor TPackageLink.Destroy;
begin
  inherited Destroy;
end;

{ TPackageLinks }

function TPackageLinks.FindLeftMostNode(LinkTree: TAVLTree;
  const PkgName: string): TAVLTreeNode;
// find left most link with PkgName
var
  PriorNode: TAVLTreeNode;
begin
  Result:=nil;
  if PkgName='' then exit;
  Result:=LinkTree.FindKey(PChar(PkgName),@ComparePkgNameAndLink);
  if Result=nil then exit;
  // find left most
  while Result<>nil do begin
    PriorNode:=LinkTree.FindPrecessor(Result);
    if (PriorNode=nil)
    or (AnsiCompareText(TPackageLink(PriorNode.Data).Name,PkgName)<>0)
    then
      break;
    Result:=PriorNode;
  end;
end;

constructor TPackageLinks.Create;
begin
  UserLinkLoadTimeValid:=false;
  FGlobalLinks:=TAVLTree.Create(@ComparePackageLinks);
  FUserLinks:=TAVLTree.Create(@ComparePackageLinks);
end;

destructor TPackageLinks.Destroy;
begin
  Clear;
  FGlobalLinks.Free;
  FUserLinks.Free;
  inherited Destroy;
end;

procedure TPackageLinks.Clear;
begin
  FGlobalLinks.FreeAndClear;
  FUserLinks.FreeAndClear;
  FStates:=[plsUserLinksNeedUpdate,plsGlobalLinksNeedUpdate];
end;

function TPackageLinks.GetUserLinkFile: string;
begin
  Result:=AppendPathDelim(GetPrimaryConfigPath)+'packagefiles.xml';
end;

procedure TPackageLinks.UpdateGlobalLinks;

  function ParseFilename(const Filename: string;
    var PkgName: string; var PkgVersion: TPkgVersion): boolean;
  // checks if filename has the form
  // identifier-int.int.int.int.lpl
  var
    StartPos: Integer;
    i: Integer;
    EndPos: Integer;
    ints: array[1..4] of integer;
  begin
    Result:=false;
    StartPos:=1;
    // parse identifier
    if (StartPos>length(Filename))
    or (not (Filename[StartPos] in ['a'..'z','A'..'Z'])) then exit;
    inc(StartPos);
    while (StartPos<=length(Filename))
    and (Filename[StartPos] in ['a'..'z','A'..'Z']) do
      inc(StartPos);
    PkgName:=lowercase(copy(Filename,1,StartPos-1));
    // parse -
    if (StartPos>length(Filename)) or (Filename[StartPos]<>'-') then exit;
    inc(StartPos);
    // parse 4 times 'int.'
    for i:=Low(ints) to High(ints) do begin
      // parse int
      EndPos:=StartPos;
      while (EndPos<=length(Filename))
      and (Filename[EndPos] in ['0'..'9']) do inc(EndPos);
      ints[i]:=StrToIntDef(copy(Filename,StartPos,EndPos-StartPos),-1);
      if (ints[i]<0) or (ints[i]>99999) then exit;
      StartPos:=EndPos;
      // parse .
      if (StartPos>length(Filename)) or (Filename[StartPos]<>'.') then exit;
      inc(StartPos);
    end;
    // parse lpl
    if (AnsiCompareText(copy(Filename,StartPos,length(Filename)-StartPos+1),
                        'lpl')<>0) then exit;
    PkgVersion.Major:=ints[1];
    PkgVersion.Minor:=ints[2];
    PkgVersion.Release:=ints[3];
    PkgVersion.Build:=ints[4];
    Result:=true;
  end;

var
  GlobalLinksDir: String;
  FileInfo: TSearchRec;
  NewPkgName: string;
  PkgVersion: TPkgVersion;
  NewPkgLink: TPackageLink;
  sl: TStringList;
  CurFilename: String;
  NewFilename: string;
begin
  if fUpdateLock>0 then begin
    Include(FStates,plsGlobalLinksNeedUpdate);
    exit;
  end;
  Exclude(FStates,plsGlobalLinksNeedUpdate);
  
  FGlobalLinks.FreeAndClear;
  GlobalLinksDir:=AppendPathDelim(EnvironmentOptions.LazarusDirectory)
                                  +'packager'+PathDelim+'globallinks'+PathDelim;
  if FindFirst(GlobalLinksDir+'*.lpl', faAnyFile, FileInfo)=0 then begin
    PkgVersion:=TPkgVersion.Create;
    repeat
      CurFilename:=GlobalLinksDir+FileInfo.Name;
      if ((FileInfo.Attr and faDirectory)<>0)
      or (not ParseFilename(FileInfo.Name,NewPkgName,PkgVersion))
      then begin
        writeln('WARNING: suspicious pkg link file found (name): ',CurFilename);
        continue;
      end;
      sl:=TStringList.Create;
      try
        sl.LoadFromFile(CurFilename);
        if sl.Count<0 then begin
          writeln('WARNING: suspicious pkg link file found (content): ',CurFilename);
          continue;
        end;
        NewFilename:=sl[0];
      except
        on E: Exception do begin
          writeln('ERROR: unable to read pkg link file: ',CurFilename,' : ',E.Message);
        end;
      end;
      sl.Free;
      
      NewPkgLink:=TPackageLink.Create;
      NewPkgLink.Origin:=ploGlobal;
      NewPkgLink.Name:=NewPkgName;
      NewPkgLink.Version.Assign(PkgVersion);
      NewPkgLink.Filename:=NewFilename;
      if IsValidIdent(NewPkgLink.Name) then
        FGlobalLinks.Add(NewPkgLink)
      else
        NewPkgLink.Free;
      
    until FindNext(FileInfo)<>0;
    if PkgVersion<>nil then PkgVersion.Free;
  end;
  FindClose(FileInfo);
end;

procedure TPackageLinks.UpdateUserLinks;
var
  ConfigFilename: String;
  Path: String;
  XMLConfig: TXMLConfig;
  LinkCount: Integer;
  i: Integer;
  NewPkgLink: TPackageLink;
  ItemPath: String;
begin
  if fUpdateLock>0 then begin
    Include(FStates,plsUserLinksNeedUpdate);
    exit;
  end;
  Exclude(FStates,plsUserLinksNeedUpdate);

  // check if file has changed
  ConfigFilename:=GetUserLinkFile;
  if UserLinkLoadTimeValid and FileExists(ConfigFilename)
  and (FileAge(ConfigFilename)=UserLinkLoadTime) then
    exit;
  
  FUserLinks.FreeAndClear;
  XMLConfig:=nil;
  try
    XMLConfig:=TXMLConfig.Create(ConfigFilename);
    
    Path:='UserPkgLinks/';
    LinkCount:=XMLConfig.GetValue(Path+'Count',0);
    for i:=1 to LinkCount do begin
      ItemPath:=Path+'Item'+IntToStr(i)+'/';
      NewPkgLink:=TPackageLink.Create;
      NewPkgLink.Origin:=ploUser;
      NewPkgLink.Name:=XMLConfig.GetValue(ItemPath+'Name/Value','');
      NewPkgLink.Version.LoadFromXMLConfig(XMLConfig,ItemPath+'Version/',
                                                          LazPkgXMLFileVersion);
      NewPkgLink.Filename:=XMLConfig.GetValue(ItemPath+'Filename/Value','');
      if IsValidIdent(NewPkgLink.Name) then
        FUserLinks.Add(NewPkgLink)
      else
        NewPkgLink.Free;
    end;
    XMLConfig.Free;
    
    UserLinkLoadTime:=FileAge(ConfigFilename);
    UserLinkLoadTimeValid:=true;
  except
    on E: Exception do begin
      writeln('NOTE: unable to read ',ConfigFilename,' ',E.Message);
      exit;
    end;
  end;
  Modified:=false;
end;

procedure TPackageLinks.UpdateAll;
begin
  UpdateGlobalLinks;
  UpdateUserLinks;
end;

procedure TPackageLinks.BeginUpdate;
begin
  inc(fUpdateLock);
end;

procedure TPackageLinks.EndUpdate;
begin
  if fUpdateLock<=0 then RaiseException('TPackageLinks.EndUpdate');
  dec(fUpdateLock);
  if (plsGlobalLinksNeedUpdate in FStates) then UpdateGlobalLinks;
  if (plsUserLinksNeedUpdate in FStates) then UpdateUserLinks;
end;

procedure TPackageLinks.SaveUserLinks;
var
  ConfigFilename: String;
  Path: String;
  CurPkgLink: TPackageLink;
  XMLConfig: TXMLConfig;
  ANode: TAVLTreeNode;
  ItemPath: String;
  i: Integer;
begin
  ConfigFilename:=GetUserLinkFile;
  
  // check if file need saving
  if (not Modified)
  and UserLinkLoadTimeValid and FileExists(ConfigFilename)
  and (FileAge(ConfigFilename)=UserLinkLoadTime) then
    exit;

  XMLConfig:=nil;
  try
    ClearFile(ConfigFilename,true);
    XMLConfig:=TXMLConfig.Create(ConfigFilename);

    Path:='UserPkgLinks/';
    ANode:=FUserLinks.FindLowest;
    i:=0;
    while ANode<>nil do begin
      inc(i);
      ItemPath:=Path+'Item'+IntToStr(i)+'/';
      CurPkgLink:=TPackageLink(ANode.Data);
      XMLConfig.SetDeleteValue(ItemPath+'Name/Value',CurPkgLink.Name,'');
      CurPkgLink.Version.SaveToXMLConfig(XMLConfig,ItemPath+'Version/');
      XMLConfig.SetDeleteValue(ItemPath+'Filename/Value',CurPkgLink.Filename,'');
      ANode:=FUserLinks.FindSuccessor(ANode);
    end;
    XMLConfig.SetDeleteValue(Path+'Count',FUserLinks.Count,0);
    
    XMLConfig.Flush;
    XMLConfig.Free;

    UserLinkLoadTime:=FileAge(ConfigFilename);
    UserLinkLoadTimeValid:=true;
  except
    on E: Exception do begin
      writeln('NOTE: unable to read ',ConfigFilename,' ',E.Message);
      exit;
    end;
  end;
  Modified:=false;
end;

function TPackageLinks.FindLinkWithPkgNameInTree(LinkTree: TAVLTree;
  const PkgName: string): TPackageLink;
// find left most link with PkgName
var
  CurNode: TAVLTreeNode;
begin
  Result:=nil;
  if PkgName='' then exit;
  CurNode:=FindLeftMostNode(LinkTree,PkgName);
  if CurNode=nil then exit;
  Result:=TPackageLink(CurNode.Data);
end;

function TPackageLinks.FindLinkWithDependencyInTree(LinkTree: TAVLTree;
  Dependency: TPkgDependency): TPackageLink;
var
  Link: TPackageLink;
  CurNode: TAVLTreeNode;
begin
  Result:=nil;
  if (Dependency=nil) or (not Dependency.MakeSense) then exit;
  CurNode:=FindLeftMostNode(LinkTree,Dependency.PackageName);
  while CurNode<>nil do begin
    Link:=TPackageLink(CurNode.Data);
    if Dependency.IsCompatible(Link.Version) then begin
      Result:=Link;
      break;
    end;
    CurNode:=LinkTree.FindSuccessor(CurNode);
    if AnsiCompareText(TPackageLink(CurNode.Data).Name,Dependency.PackageName)
    <>0
    then begin
      CurNode:=nil;
      break;
    end;
  end;
end;

function TPackageLinks.FindLinkWithPackageIDInTree(LinkTree: TAVLTree;
  APackageID: TLazPackageID): TPackageLink;
var
  ANode: TAVLTreeNode;
begin
  ANode:=LinkTree.FindKey(APackageID,@ComparePackageIDAndLink);
  if ANode<>nil then
    Result:=TPackageLink(ANode.Data)
  else
    Result:=nil;
end;

procedure TPackageLinks.IteratePackagesInTree(LinkTree: TAVLTree;
  Event: TIteratePackagesEvent);
var
  ANode: TAVLTreeNode;
begin
  ANode:=LinkTree.FindLowest;
  while ANode<>nil do begin
    Event(TPackageLink(ANode.Data));
    ANode:=LinkTree.FindSuccessor(ANode);
  end;
end;

procedure TPackageLinks.SetModified(const AValue: boolean);
begin
  if FModified=AValue then exit;
  FModified:=AValue;
end;

function TPackageLinks.FindLinkWithPkgName(const PkgName: string): TPackageLink;
begin
  Result:=FindLinkWithPkgNameInTree(FUserLinks,PkgName);
  if Result=nil then
    Result:=FindLinkWithPkgNameInTree(FGlobalLinks,PkgName);
end;

function TPackageLinks.FindLinkWithDependency(Dependency: TPkgDependency
  ): TPackageLink;
begin
  Result:=FindLinkWithDependencyInTree(FUserLinks,Dependency);
  if Result=nil then
    Result:=FindLinkWithDependencyInTree(FGlobalLinks,Dependency);
end;

function TPackageLinks.FindLinkWithPackageID(APackageID: TLazPackageID
  ): TPackageLink;
begin
  Result:=FindLinkWithPackageIDInTree(FUserLinks,APackageID);
  if Result=nil then
    Result:=FindLinkWithPackageIDInTree(FGlobalLinks,APackageID);
end;

procedure TPackageLinks.IteratePackages(Event: TIteratePackagesEvent);
begin
  IteratePackagesInTree(FUserLinks,Event);
  IteratePackagesInTree(FGlobalLinks,Event);
end;

procedure TPackageLinks.AddUserLink(APackage: TLazPackage);
var
  OldLink: TPackageLink;
  NewLink: TPackageLink;
begin
  // check if link already exists
  OldLink:=FindLinkWithPackageID(APackage);
  if (OldLink<>nil) then begin
    // link exists -> check if it is already the right value
    if (OldLink.Compare(APackage)=0)
    and (OldLink.Filename=APackage.Filename) then exit;
    RemoveLink(APackage);
  end;
  // add user link
  NewLink:=TPackageLink.Create;
  NewLink.AssignID(APackage);
  NewLink.Filename:=APackage.Filename;
  FUserLinks.Add(NewLink);
  Modified:=true;
end;

procedure TPackageLinks.RemoveLink(APackageID: TLazPackageID);
var
  ANode: TAVLTreeNode;
  OldLink: TPackageLink;
begin
  // remove from user links
  ANode:=FUserLinks.Find(APackageID);
  if ANode<>nil then begin
    OldLink:=TPackageLink(ANode.Data);
    FUserLinks.Remove(ANode);
    OldLink.Free;
  end;
  // remove from global links
  ANode:=FGlobalLinks.Find(APackageID);
  if ANode<>nil then begin
    OldLink:=TPackageLink(ANode.Data);
    FGlobalLinks.Remove(ANode);
    OldLink.Free;
  end;
  Modified:=true;
end;


initialization
  PkgLinks:=nil;

end.

