object frmMain: TfrmMain
  Left = 0
  Top = 0
  Caption = 'ClaBot - Claude Agent Monitor'
  ClientHeight = 550
  ClientWidth = 900
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -12
  Font.Name = 'Segoe UI'
  Font.Style = []
  OnCreate = FormCreate
  OnDestroy = FormDestroy
  DesignSize = (
    900
    550)
  TextHeight = 15
  object pnlTop: TPanel
    Left = 0
    Top = 0
    Width = 900
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
  object grpSession: TGroupBox
    Left = 8
    Top = 47
    Width = 150
    Height = 90
    Caption = ' Session '
    TabOrder = 1
    object rbNewSession: TRadioButton
      Left = 12
      Top = 22
      Width = 60
      Height = 17
      Caption = 'New'
      Checked = True
      TabOrder = 0
      TabStop = True
      OnClick = rbNewSessionClick
    end
    object rbContinue: TRadioButton
      Left = 80
      Top = 22
      Width = 65
      Height = 17
      Caption = 'Continue'
      TabOrder = 1
      OnClick = rbContinueClick
    end
    object lblSessionId: TLabel
      Left = 12
      Top = 44
      Width = 14
      Height = 15
      Caption = 'ID:'
    end
    object lblSessionIdValue: TLabel
      Left = 30
      Top = 44
      Width = 110
      Height = 15
      AutoSize = False
      Caption = '-'
      EllipsisPosition = epEndEllipsis
    end
    object lblTokens: TLabel
      Left = 12
      Top = 62
      Width = 40
      Height = 15
      Caption = 'Tokens:'
    end
    object lblTokensValue: TLabel
      Left = 56
      Top = 62
      Width = 84
      Height = 15
      AutoSize = False
      Caption = '0/0'
    end
    object lblCost: TLabel
      Left = 12
      Top = 80
      Width = 27
      Height = 15
      Caption = 'Cost:'
      Visible = False
    end
    object lblCostValue: TLabel
      Left = 45
      Top = 80
      Width = 95
      Height = 15
      AutoSize = False
      Caption = '$0.000'
      Visible = False
    end
  end
  object grpAgent: TGroupBox
    Left = 164
    Top = 47
    Width = 335
    Height = 90
    Caption = ' Agent '
    TabOrder = 2
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
    Width = 480
    Height = 350
    Anchors = [akLeft, akTop, akRight, akBottom]
    Caption = ' Events '
    TabOrder = 3
    object lvEvents: TListView
      Left = 8
      Top = 20
      Width = 464
      Height = 322
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
      OnSelectItem = lvEventsSelectItem
    end
  end
  object grpDetails: TGroupBox
    Left = 494
    Top = 143
    Width = 398
    Height = 350
    Anchors = [akTop, akRight, akBottom]
    Caption = ' Details '
    TabOrder = 4
    object mmoDetails: TMemo
      Left = 8
      Top = 20
      Width = 382
      Height = 322
      Anchors = [akLeft, akTop, akRight, akBottom]
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'Consolas'
      Font.Style = []
      ParentFont = False
      ReadOnly = True
      ScrollBars = ssBoth
      TabOrder = 0
      WordWrap = False
    end
  end
  object pnlPrompt: TPanel
    Left = 8
    Top = 499
    Width = 884
    Height = 40
    Anchors = [akLeft, akRight, akBottom]
    BevelOuter = bvNone
    TabOrder = 5
    object edtPrompt: TEdit
      Left = 0
      Top = 8
      Width = 709
      Height = 23
      Anchors = [akLeft, akTop, akRight]
      TabOrder = 0
      TextHint = 'Enter prompt...'
    end
    object btnSend: TButton
      Left = 715
      Top = 7
      Width = 80
      Height = 25
      Anchors = [akTop, akRight]
      Caption = 'Send'
      TabOrder = 1
      OnClick = btnSendClick
    end
    object btnStop: TButton
      Left = 801
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
    Top = 531
    Width = 900
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
      end
      item
        Width = 150
      end>
  end
end
