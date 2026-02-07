//---------------------------------------------------------------------------
#ifndef uMainH
#define uMainH
//---------------------------------------------------------------------------
#include <System.Classes.hpp>
#include <Vcl.Controls.hpp>
#include <Vcl.StdCtrls.hpp>
#include <Vcl.Forms.hpp>
#include <Vcl.ComCtrls.hpp>
#include <Vcl.ExtCtrls.hpp>
#include "json.hpp"
#include "UcodeUtf8.h"
#include "uHttpClient.h"
#include "uSSEClient.h"
#include "services/uEventStore.h"
#include "services/uSessionState.h"
#include "interfaces/uIAppState.h"
#include <memory>

using json = nlohmann::json;

// Forward declaration
class TUiMcpServer;

//---------------------------------------------------------------------------
class TfrmMain : public TForm, public IAppState
{
__published:    // IDE-managed Components
    TPanel *pnlTop;
    TLabel *lblServer;
    TEdit *edtServer;
    TButton *btnConnect;
    TGroupBox *grpAgent;
    TLabel *lblAgentName;
    TEdit *edtAgentName;
    TButton *btnCreateAgent;
    TGroupBox *grpEvents;
    TListView *lvEvents;
    TPanel *pnlPrompt;
    TEdit *edtPrompt;
    TButton *btnSend;
    TButton *btnStop;
    TStatusBar *StatusBar;
    TLabel *lblAgentId;
    TEdit *edtAgentId;
    // Session controls
    TGroupBox *grpSession;
    TRadioButton *rbNewSession;
    TRadioButton *rbContinue;
    TLabel *lblSessionId;
    TLabel *lblSessionIdValue;
    TLabel *lblTokens;
    TLabel *lblTokensValue;
    TLabel *lblCost;
    TLabel *lblCostValue;
    // Details panel
    TGroupBox *grpDetails;
    TMemo *mmoDetails;

    void __fastcall btnConnectClick(TObject *Sender);
    void __fastcall btnCreateAgentClick(TObject *Sender);
    void __fastcall btnSendClick(TObject *Sender);
    void __fastcall btnStopClick(TObject *Sender);
    void __fastcall FormCreate(TObject *Sender);
    void __fastcall FormDestroy(TObject *Sender);
    void __fastcall lvEventsSelectItem(TObject *Sender, TListItem *Item, bool Selected);
    void __fastcall rbNewSessionClick(TObject *Sender);
    void __fastcall rbContinueClick(TObject *Sender);

private:    // User declarations
    THttpClient *FHttpClient;
    TSSEClient *FSSEClient;
    std::unique_ptr<TUiMcpServer> FMcpServer;
    TEventStore FEventStore;
    TSessionState FState;
    // Currently selected event index
    int FSelectedEventIndex;

    void __fastcall OnSSEEvent(TObject *Sender, const json &Event);
    void __fastcall OnSSEError(TObject *Sender, const UnicodeString &Error);
    void __fastcall OnSSEConnected(TObject *Sender);
    void __fastcall OnSSEDisconnected(TObject *Sender);

    void UpdateStatus(const UnicodeString &Status);
    void AddEvent(const TEventData &eventData);
    void SetControlsState(bool Connected, bool AgentCreated, bool Running);
    void UpdateSessionInfo();
    void ShowEventDetails(int index);

public:     // User declarations
    __fastcall TfrmMain(TComponent* Owner);

    // === IAppState implementation ===
    bool IsConnected() const override { return FState.Connected; }
    UnicodeString GetAgentId() const override { return FState.CurrentAgentId; }
    int GetEventCount() const override { return FEventStore.Count(); }
    UnicodeString GetStatusText() const override;
    nlohmann::json GetStatusBarPanels() const override;
    std::vector<TEventData> GetEvents(int limit, int offset) const override;
    TEventData GetEventDetails(int index) const override;

    // Control getters
    UnicodeString GetServerUrl() const override { return edtServer->Text; }
    UnicodeString GetAgentName() const override { return edtAgentName->Text; }
    UnicodeString GetPrompt() const override { return edtPrompt->Text; }

    // Button state getters
    bool IsConnectEnabled() const override { return btnConnect->Enabled; }
    bool IsCreateAgentEnabled() const override { return btnCreateAgent->Enabled; }
    bool IsSendEnabled() const override { return btnSend->Enabled; }
    bool IsStopEnabled() const override { return btnStop->Enabled; }

    // Button click actions (for MCP tools)
    void ClickConnect() override { btnConnectClick(nullptr); }
    void ClickCreateAgent() override { btnCreateAgentClick(nullptr); }
    void ClickSend() override { btnSendClick(nullptr); }
    void ClickStop() override { btnStopClick(nullptr); }

    // Control setters (for MCP tools)
    void SetServerUrl(const UnicodeString &url) override { edtServer->Text = url; }
    void SetAgentName(const UnicodeString &name) override { edtAgentName->Text = name; }
    void SetPrompt(const UnicodeString &text) override { edtPrompt->Text = text; }

    // Session info accessors (for MCP tools)
    UnicodeString GetSdkSessionId() const override { return FState.SdkSessionId; }
    bool GetCanResume() const override { return FState.CanResume; }
    bool GetResumeMode() const override { return FState.ResumeMode; }
    int GetInputTokens() const override { return FState.InputTokens; }
    int GetOutputTokens() const override { return FState.OutputTokens; }
    double GetTotalCostUsd() const override { return FState.TotalCostUsd; }

    // Resume mode setter
    void SetResumeMode(bool resume) override { FState.ResumeMode = resume; }
};
//---------------------------------------------------------------------------
extern PACKAGE TfrmMain *frmMain;
//---------------------------------------------------------------------------
#endif
