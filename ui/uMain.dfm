object frmMain: TfrmMain
  Left = 0
  Top = 0
  Caption = 'ClaBot - Claude Agent Monitor'
  ClientHeight = 500
  ClientWidth = 700
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -12
  Font.Name = 'Segoe UI'
  Font.Style = []
  OnCreate = FormCreate
  OnDestroy = FormDestroy
  DesignSize = (
    700
    500)
  TextHeight = 15
  object pnlTop: TPanel
    Left = 0
    Top = 0
    Width = 700
    Height = 41
    Align = alTop
    BevelOuter = bvNone
    TabOrder = 0
    object lblServer: TLabel
      Left = 12
      Top = 12
      Width = 37
      Height = 15
      Caption = 'Server:'
    end
    object edtServer: TEdit
      Left = 56
      Top = 9
      Width = 200
      Height = 23
      TabOrder = 0
      Text = 'http://localhost:3000'
    end
    object btnConnect: TButton
      Left = 262
      Top = 8
      Width = 75
      Height = 25
      Caption = 'Connect'
      TabOrder = 1
      OnClick = btnConnectClick
    end
  end
  object grpAgent: TGroupBox
    Left = 8
    Top = 47
    Width = 335
    Height = 90
    Caption = ' Agent '
    TabOrder = 1
    object lblAgentName: TLabel
      Left = 12
      Top = 24
      Width = 35
      Height = 15
      Caption = 'Name:'
    end
    object lblAgentId: TLabel
      Left = 12
      Top = 56
      Width = 14
      Height = 15
      Caption = 'ID:'
    end
    object edtAgentName: TEdit
      Left = 56
      Top = 21
      Width = 160
      Height = 23
      TabOrder = 0
      Text = 'Test Agent'
    end
    object btnCreateAgent: TButton
      Left = 222
      Top = 20
      Width = 100
      Height = 25
      Caption = 'Create Agent'
      TabOrder = 1
      OnClick = btnCreateAgentClick
    end
    object edtAgentId: TEdit
      Left = 56
      Top = 53
      Width = 266
      Height = 23
      Color = clBtnFace
      ReadOnly = True
      TabOrder = 2
    end
  end
  object grpEvents: TGroupBox
    Left = 8
    Top = 143
    Width = 684
    Height = 300
    Anchors = [akLeft, akTop, akRight, akBottom]
    Caption = ' Events '
    TabOrder = 2
    object lvEvents: TListView
      Left = 8
      Top = 20
      Width = 668
      Height = 272
      Anchors = [akLeft, akTop, akRight, akBottom]
      Columns = <
        item
          Caption = 'Time'
          Width = 70
        end
        item
          Caption = 'Type'
          Width = 100
        end
        item
          AutoSize = True
          Caption = 'Data'
        end>
      ReadOnly = True
      RowSelect = True
      TabOrder = 0
      ViewStyle = vsReport
    end
  end
  object pnlPrompt: TPanel
    Left = 8
    Top = 449
    Width = 684
    Height = 40
    Anchors = [akLeft, akRight, akBottom]
    BevelOuter = bvNone
    TabOrder = 3
    object edtPrompt: TEdit
      Left = 0
      Top = 8
      Width = 509
      Height = 23
      Anchors = [akLeft, akTop, akRight]
      TabOrder = 0
      TextHint = 'Enter prompt...'
    end
    object btnSend: TButton
      Left = 515
      Top = 7
      Width = 80
      Height = 25
      Anchors = [akTop, akRight]
      Caption = 'Send'
      TabOrder = 1
      OnClick = btnSendClick
    end
    object btnStop: TButton
      Left = 601
      Top = 7
      Width = 80
      Height = 25
      Anchors = [akTop, akRight]
      Caption = 'Stop'
      TabOrder = 2
      OnClick = btnStopClick
    end
  end
  object StatusBar: TStatusBar
    Left = 0
    Top = 481
    Width = 700
    Height = 19
    Panels = <
      item
        Width = 300
      end
      item
        Width = 200
      end
      item
        Width = 100
      end>
  end
end
