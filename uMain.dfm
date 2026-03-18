object frmMain: TfrmMain
  Left = 0
  Top = 0
  Caption = 'MCPHub'
  ClientHeight = 600
  ClientWidth = 900
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -12
  Font.Name = 'Segoe UI'
  Font.Style = []
  Position = poScreenCenter
  OnClose = FormClose
  OnShow = FormShow
  TextHeight = 15
  object Splitter: TSplitter
    Left = 425
    Top = 0
    Width = 5
    Height = 561
    ExplicitLeft = 220
    ExplicitHeight = 559
  end
  object panLeft: TPanel
    Left = 0
    Top = 0
    Width = 425
    Height = 561
    Align = alLeft
    BevelOuter = bvNone
    TabOrder = 0
    object lvModules: TListView
      Left = 0
      Top = 0
      Width = 425
      Height = 561
      Align = alClient
      Columns = <
        item
          Caption = 'Name'
          Width = 100
        end
        item
          Caption = 'Type'
          Width = 60
        end
        item
          Caption = 'Port'
          Width = 40
        end
        item
          Caption = 'Status'
          Width = 60
        end
        item
          Caption = 'Auto'
          Width = 35
        end
        item
          Caption = 'Reqs'
          Width = 40
        end>
      ColumnClick = False
      HideSelection = False
      ReadOnly = True
      RowSelect = True
      TabOrder = 0
      ViewStyle = vsReport
      OnSelectItem = lvModulesSelectItem
    end
  end
  object panRight: TPanel
    Left = 430
    Top = 0
    Width = 470
    Height = 561
    Align = alClient
    BevelOuter = bvNone
    TabOrder = 1
    ExplicitLeft = 355
    ExplicitWidth = 545
    object pcDetails: TPageControl
      Left = 0
      Top = 0
      Width = 470
      Height = 561
      ActivePage = tsSettings
      Align = alClient
      TabOrder = 0
      OnChange = pcDetailsChange
      ExplicitWidth = 545
      object tsSettings: TTabSheet
        Caption = 'Settings'
        object scrollConfig: TScrollBox
          Left = 0
          Top = 0
          Width = 462
          Height = 489
          Align = alClient
          TabOrder = 0
          ExplicitWidth = 537
        end
        object panActions: TPanel
          Left = 0
          Top = 489
          Width = 462
          Height = 42
          Align = alBottom
          BevelOuter = bvNone
          TabOrder = 1
          ExplicitWidth = 537
          object btnStart: TButton
            Left = 8
            Top = 8
            Width = 75
            Height = 28
            Caption = 'Start'
            TabOrder = 0
            OnClick = btnStartClick
          end
          object btnStop: TButton
            Left = 92
            Top = 8
            Width = 75
            Height = 28
            Caption = 'Stop'
            TabOrder = 1
            OnClick = btnStopClick
          end
          object btnDelete: TButton
            Left = 580
            Top = 8
            Width = 75
            Height = 28
            Caption = 'Delete'
            TabOrder = 2
            OnClick = btnDeleteClick
          end
        end
      end
      object tsTools: TTabSheet
        Caption = 'Tools'
        ImageIndex = 1
        object lvTools: TListView
          Left = 0
          Top = 0
          Width = 462
          Height = 531
          Align = alClient
          Columns = <
            item
              Caption = 'Tool Name'
              Width = 250
            end
            item
              Caption = 'Index'
            end>
          ColumnClick = False
          ReadOnly = True
          RowSelect = True
          TabOrder = 0
          ViewStyle = vsReport
          ExplicitWidth = 537
        end
      end
      object tsLog: TTabSheet
        Caption = 'Log'
        ImageIndex = 2
        object Splitter1: TSplitter
          Left = 0
          Top = 265
          Width = 462
          Height = 6
          Cursor = crVSplit
          Align = alTop
        end
        object gbRequest: TGroupBox
          Left = 0
          Top = 0
          Width = 462
          Height = 265
          Align = alTop
          Caption = 'Request'
          TabOrder = 0
          ExplicitWidth = 537
          object memoRequest: TMemo
            Left = 2
            Top = 17
            Width = 458
            Height = 246
            Align = alTop
            ReadOnly = True
            ScrollBars = ssBoth
            TabOrder = 0
            WordWrap = False
            ExplicitWidth = 533
          end
        end
        object gbResponse: TGroupBox
          Left = 0
          Top = 271
          Width = 462
          Height = 260
          Align = alClient
          Caption = 'Response'
          TabOrder = 1
          ExplicitTop = 265
          ExplicitWidth = 537
          ExplicitHeight = 266
          object memoResponse: TMemo
            Left = 2
            Top = 17
            Width = 458
            Height = 241
            Align = alClient
            ReadOnly = True
            ScrollBars = ssBoth
            TabOrder = 0
            WordWrap = False
            ExplicitWidth = 533
            ExplicitHeight = 247
          end
        end
      end
    end
  end
  object panBottom: TPanel
    Left = 0
    Top = 561
    Width = 900
    Height = 39
    Align = alBottom
    BevelOuter = bvNone
    TabOrder = 2
    object btnAdd: TButton
      Left = 8
      Top = 6
      Width = 80
      Height = 28
      Caption = '+ Add'
      TabOrder = 0
      OnClick = btnAddClick
    end
    object btnSaveConfig: TButton
      Left = 96
      Top = 6
      Width = 90
      Height = 28
      Caption = 'Save Config'
      TabOrder = 1
      OnClick = btnSaveConfigClick
    end
    object btnStartAll: TButton
      Left = 706
      Top = 6
      Width = 85
      Height = 28
      Caption = 'Start All'
      TabOrder = 2
      OnClick = btnStartAllClick
    end
    object btnStopAll: TButton
      Left = 800
      Top = 6
      Width = 85
      Height = 28
      Caption = 'Stop All'
      TabOrder = 3
      OnClick = btnStopAllClick
    end
  end
  object StatusBar: TStatusBar
    Left = 0
    Top = 600
    Width = 900
    Height = 0
    Panels = <
      item
        Width = 120
      end
      item
        Width = 120
      end
      item
        Width = 120
      end
      item
        Width = 300
      end>
  end
  object pmAddModule: TPopupMenu
    Left = 112
    Top = 568
  end
  object FDPhysMSSQLDriverLink: TFDPhysMSSQLDriverLink
    Left = 400
    Top = 568
  end
  object FDPhysFBDriverLink: TFDPhysFBDriverLink
    Left = 480
    Top = 568
  end
  object FDPhysPgDriverLink: TFDPhysPgDriverLink
    Left = 560
    Top = 568
  end
  object FDGUIxWaitCursor: TFDGUIxWaitCursor
    Provider = 'Forms'
    Left = 640
    Top = 568
  end
  object OpenDialog: TOpenDialog
    Filter = 'SQLite Database|*.db;*.sqlite|All Files|*.*'
    Left = 200
    Top = 568
  end
end
