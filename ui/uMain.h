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
#include <vector>
#include <memory>

using json = nlohmann::json;

// Forward declaration
class TUiMcpServer;

// Event data structure for MCP tools
struct TEventData
{
    UnicodeString Time;
    UnicodeString Type;
    UnicodeString Data;
    // Extended fields for detailed tool info
    UnicodeString ToolInput;   // JSON input for tool_start/tool_end
    UnicodeString ToolOutput;  // JSON output for tool_end
    UnicodeString ToolUseId;   // Tool use ID
    UnicodeString RequestId;   // Permission request ID
    int DurationMs;            // Duration for tool_end
};
//---------------------------------------------------------------------------
class TfrmMain : public TForm
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
    UnicodeString FCurrentAgentId;
    int FEventCount;
    bool FConnected;
    bool FAgentCreated;
    bool FRunning;
    // Session tracking
    UnicodeString FSdkSessionId;
    bool FCanResume;
    bool FResumeMode;  // User's choice: resume or new session
    int FInputTokens;
    int FOutputTokens;
    double FTotalCostUsd;
    // Currently selected event index
    int FSelectedEventIndex;
    // Full event data storage (not just displayed text)
    std::vector<TEventData> FEventDataList;

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

    // MCP UI tool accessors
    bool IsConnected() const { return FConnected; }
    UnicodeString GetAgentId() const { return FCurrentAgentId; }
    int GetEventCount() const { return FEventCount; }
    UnicodeString GetStatusText() const;
    std::vector<TEventData> GetEvents(int limit, int offset) const;
    nlohmann::json GetStatusBarPanels() const;

    // Control getters
    UnicodeString GetServerUrl() const { return edtServer->Text; }
    UnicodeString GetAgentName() const { return edtAgentName->Text; }
    UnicodeString GetPrompt() const { return edtPrompt->Text; }

    // Button state getters
    bool IsConnectEnabled() const { return btnConnect->Enabled; }
    bool IsCreateAgentEnabled() const { return btnCreateAgent->Enabled; }
    bool IsSendEnabled() const { return btnSend->Enabled; }
    bool IsStopEnabled() const { return btnStop->Enabled; }

    // Button click actions (for MCP tools)
    void ClickConnect() { btnConnectClick(nullptr); }
    void ClickCreateAgent() { btnCreateAgentClick(nullptr); }
    void ClickSend() { btnSendClick(nullptr); }
    void ClickStop() { btnStopClick(nullptr); }

    // Control setters (for MCP tools)
    void SetServerUrl(const UnicodeString &url) { edtServer->Text = url; }
    void SetAgentName(const UnicodeString &name) { edtAgentName->Text = name; }
    void SetPrompt(const UnicodeString &text) { edtPrompt->Text = text; }

    // Session info accessors (for MCP tools)
    UnicodeString GetSdkSessionId() const { return FSdkSessionId; }
    bool GetCanResume() const { return FCanResume; }
    bool GetResumeMode() const { return FResumeMode; }
    int GetInputTokens() const { return FInputTokens; }
    int GetOutputTokens() const { return FOutputTokens; }
    double GetTotalCostUsd() const { return FTotalCostUsd; }

    // Resume mode setter
    void SetResumeMode(bool resume) { FResumeMode = resume; }

    // Get event details by index
    TEventData GetEventDetails(int index) const;
};
//---------------------------------------------------------------------------
extern PACKAGE TfrmMain *frmMain;
//---------------------------------------------------------------------------
#endif
