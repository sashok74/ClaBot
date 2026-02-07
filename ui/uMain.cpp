//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop
#include "uMain.h"
#include "uMcpServer.h"
#include "services/uEventParser.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TfrmMain *frmMain;
//---------------------------------------------------------------------------
__fastcall TfrmMain::TfrmMain(TComponent* Owner)
    : TForm(Owner), FHttpClient(nullptr), FSSEClient(nullptr),
      FSelectedEventIndex(-1)
{
}
//---------------------------------------------------------------------------
void __fastcall TfrmMain::FormCreate(TObject *Sender)
{
    FHttpClient = new THttpClient();
    FSSEClient = new TSSEClient();

    // Setup SSE callbacks
    FSSEClient->OnEvent = OnSSEEvent;
    FSSEClient->OnError = OnSSEError;
    FSSEClient->OnConnected = OnSSEConnected;
    FSSEClient->OnDisconnected = OnSSEDisconnected;

    // Initial state
    SetControlsState(false, false, false);
    UpdateStatus("Disconnected");

    // Start MCP server for UI control
    FMcpServer = std::make_unique<TUiMcpServer>();
    FMcpServer->RegisterUiTools(this);
    FMcpServer->Start(8767);
}
//---------------------------------------------------------------------------
void __fastcall TfrmMain::FormDestroy(TObject *Sender)
{
    // Stop MCP server first
    if (FMcpServer) {
        FMcpServer->Stop();
        FMcpServer.reset();
    }

    if (FSSEClient) {
        FSSEClient->Disconnect();
        delete FSSEClient;
    }
    if (FHttpClient) {
        delete FHttpClient;
    }
}
//---------------------------------------------------------------------------
void __fastcall TfrmMain::btnConnectClick(TObject *Sender)
{
    UnicodeString baseUrl = edtServer->Text.Trim();
    if (baseUrl.IsEmpty()) {
        ShowMessage("Enter server URL");
        return;
    }

    FHttpClient->BaseUrl = baseUrl;

    try {
        json response = FHttpClient->Get("/health");
        if (!response.is_discarded() && response.contains("status")) {
            std::string status = response["status"].get<std::string>();
            if (status == "ok") {
                SetControlsState(true, false, false);
                UpdateStatus("Connected to " + baseUrl);
            }
        }
    }
    catch (Exception &e) {
        ShowMessage("Connection failed: " + e.Message);
        SetControlsState(false, false, false);
        UpdateStatus("Connection failed");
    }
}
//---------------------------------------------------------------------------
void __fastcall TfrmMain::btnCreateAgentClick(TObject *Sender)
{
    UnicodeString name = edtAgentName->Text.Trim();
    if (name.IsEmpty()) {
        name = "Test Agent";
    }

    try {
        json body;
        body["name"] = utf8(name);

        json response = FHttpClient->Post("/agent/create", body);

        if (!response.is_discarded() && response.contains("id")) {
            std::string id = response["id"].get<std::string>();
            FState.CurrentAgentId = u(id);
            edtAgentId->Text = FState.CurrentAgentId;

            // Connect SSE
            UnicodeString sseUrl = FHttpClient->BaseUrl + "/agent/" + FState.CurrentAgentId + "/events";
            FSSEClient->Connect(sseUrl);

            // Clear events
            lvEvents->Items->Clear();
            FEventStore.Clear();
            mmoDetails->Clear();
            FSelectedEventIndex = -1;

            // Reset session info
            FState.Reset();
            UpdateSessionInfo();

            SetControlsState(true, true, false);
            UpdateStatus("Agent created: " + FState.CurrentAgentId.SubString(1, 8) + "...");
        }
    }
    catch (Exception &e) {
        ShowMessage("Failed to create agent: " + e.Message);
    }
}
//---------------------------------------------------------------------------
void __fastcall TfrmMain::btnSendClick(TObject *Sender)
{
    UnicodeString prompt = edtPrompt->Text.Trim();
    if (prompt.IsEmpty()) {
        ShowMessage("Enter a prompt");
        return;
    }

    if (FState.CurrentAgentId.IsEmpty()) {
        ShowMessage("Create an agent first");
        return;
    }

    try {
        json body;
        body["prompt"] = utf8(prompt);
        // Add resume flag if user selected Continue mode and session supports it
        if (FState.ResumeMode && FState.CanResume) {
            body["resume"] = true;
        }

        json response = FHttpClient->Post("/agent/" + FState.CurrentAgentId + "/query", body);

        if (!response.is_discarded() && response.contains("status")) {
            std::string status = response["status"].get<std::string>();

            if (status == "processing") {
                SetControlsState(true, true, true);
                UpdateStatus("Processing...");
                edtPrompt->Text = "";
            }
        }
    }
    catch (Exception &e) {
        ShowMessage("Failed to send query: " + e.Message);
    }
}
//---------------------------------------------------------------------------
void __fastcall TfrmMain::btnStopClick(TObject *Sender)
{
    if (FState.CurrentAgentId.IsEmpty()) {
        return;
    }

    try {
        json response = FHttpClient->Post("/agent/" + FState.CurrentAgentId + "/interrupt", json::object());
        if (!response.is_discarded()) {
            SetControlsState(true, true, false);
            UpdateStatus("Interrupted");
        }
    }
    catch (Exception &e) {
        ShowMessage("Failed to interrupt: " + e.Message);
    }
}
//---------------------------------------------------------------------------
void __fastcall TfrmMain::OnSSEEvent(TObject *Sender, const json &Event)
{
    TEventParseResult r = TEventParser::Parse(Event);

    // Apply side-effects
    if (r.HasSessionInfo) {
        FState.SdkSessionId = r.NewSdkSessionId;
        FState.CanResume = r.NewCanResume;
        UpdateSessionInfo();
    }

    if (r.HasUsage) {
        FState.InputTokens = r.InputTokens;
        FState.OutputTokens = r.OutputTokens;
        FState.TotalCostUsd = r.TotalCostUsd;
        UpdateSessionInfo();
    }

    if (r.SessionEnded) {
        SetControlsState(true, true, false);
    }

    if (r.HasStatusUpdate) {
        UpdateStatus(r.StatusText);
    }

    AddEvent(r.EventData);
}
//---------------------------------------------------------------------------
void __fastcall TfrmMain::OnSSEError(TObject *Sender, const UnicodeString &Error)
{
    TEventData eventData;
    eventData.Time = Now().FormatString("hh:nn:ss");
    eventData.Type = "error";
    eventData.Data = Error;
    eventData.DurationMs = 0;
    AddEvent(eventData);
    UpdateStatus("SSE Error: " + Error);
}
//---------------------------------------------------------------------------
void __fastcall TfrmMain::OnSSEConnected(TObject *Sender)
{
    UpdateStatus("SSE Connected");
}
//---------------------------------------------------------------------------
void __fastcall TfrmMain::OnSSEDisconnected(TObject *Sender)
{
    UpdateStatus("SSE Disconnected");
}
//---------------------------------------------------------------------------
void TfrmMain::UpdateStatus(const UnicodeString &Status)
{
    StatusBar->Panels->Items[0]->Text = Status;
    StatusBar->Panels->Items[1]->Text = "Agent: " + (FState.CurrentAgentId.IsEmpty() ? "-" : FState.CurrentAgentId.SubString(1, 8));
    StatusBar->Panels->Items[2]->Text = "Events: " + IntToStr(FEventStore.Count());
    // Session info in panel 3
    if (FState.CanResume) {
        StatusBar->Panels->Items[3]->Text = "Session: resumable";
    } else {
        StatusBar->Panels->Items[3]->Text = "Session: -";
    }
}
//---------------------------------------------------------------------------
void TfrmMain::AddEvent(const TEventData &eventData)
{
    // Store full event data
    FEventStore.Add(eventData);

    // Add to ListView
    TListItem *item = lvEvents->Items->Add();
    item->Caption = eventData.Time;
    item->SubItems->Add(eventData.Type);
    // Truncate display data for list view
    UnicodeString displayData = eventData.Data;
    if (displayData.Length() > 100) {
        displayData = displayData.SubString(1, 100) + "...";
    }
    item->SubItems->Add(displayData);

    StatusBar->Panels->Items[2]->Text = "Events: " + IntToStr(FEventStore.Count());

    // Auto-scroll to bottom
    item->MakeVisible(false);
}
//---------------------------------------------------------------------------
void TfrmMain::SetControlsState(bool Connected, bool AgentCreated, bool Running)
{
    // Track state
    FState.Connected = Connected;
    FState.AgentCreated = AgentCreated;
    FState.Running = Running;

    btnConnect->Enabled = !Connected;
    edtServer->Enabled = !Connected;

    grpAgent->Enabled = Connected;
    btnCreateAgent->Enabled = Connected && !Running;
    edtAgentName->Enabled = Connected && !AgentCreated;

    // Session controls
    grpSession->Enabled = Connected;
    rbNewSession->Enabled = Connected && !Running;
    rbContinue->Enabled = Connected && FState.CanResume && !Running;

    edtPrompt->Enabled = AgentCreated && !Running;
    btnSend->Enabled = AgentCreated && !Running;
    btnStop->Enabled = Running;
}
//---------------------------------------------------------------------------
void TfrmMain::UpdateSessionInfo()
{
    // Update session ID display
    if (FState.SdkSessionId.IsEmpty()) {
        lblSessionIdValue->Caption = "-";
    } else {
        lblSessionIdValue->Caption = FState.SdkSessionId.SubString(1, 10) + "...";
    }

    // Update tokens display
    lblTokensValue->Caption = IntToStr(FState.InputTokens) + "/" + IntToStr(FState.OutputTokens);

    // Update cost display
    lblCostValue->Caption = "$" + FormatFloat("0.000", FState.TotalCostUsd);

    // Enable/disable continue radio button based on canResume
    rbContinue->Enabled = FState.CanResume && !FState.Running;
}
//---------------------------------------------------------------------------
void TfrmMain::ShowEventDetails(int index)
{
    mmoDetails->Clear();

    if (index < 0 || index >= FEventStore.Count()) {
        return;
    }

    const TEventData &ev = FEventStore.Get(index);

    mmoDetails->Lines->Add("Type: " + ev.Type);
    mmoDetails->Lines->Add("Time: " + ev.Time);
    mmoDetails->Lines->Add("");

    if (!ev.ToolUseId.IsEmpty()) {
        mmoDetails->Lines->Add("Tool Use ID: " + ev.ToolUseId);
    }
    if (!ev.RequestId.IsEmpty()) {
        mmoDetails->Lines->Add("Request ID: " + ev.RequestId);
    }

    if (ev.DurationMs > 0) {
        mmoDetails->Lines->Add("Duration: " + IntToStr(ev.DurationMs) + "ms");
    }

    if (!ev.ToolInput.IsEmpty()) {
        mmoDetails->Lines->Add("");
        mmoDetails->Lines->Add("=== Input ===");
        mmoDetails->Lines->Add(ev.ToolInput);
    }

    if (!ev.ToolOutput.IsEmpty()) {
        mmoDetails->Lines->Add("");
        mmoDetails->Lines->Add("=== Output ===");
        mmoDetails->Lines->Add(ev.ToolOutput);
    }

    if (ev.ToolInput.IsEmpty() && ev.ToolOutput.IsEmpty()) {
        mmoDetails->Lines->Add("Data:");
        mmoDetails->Lines->Add(ev.Data);
    }
}
//---------------------------------------------------------------------------
void __fastcall TfrmMain::lvEventsSelectItem(TObject *Sender, TListItem *Item, bool Selected)
{
    if (Selected && Item) {
        FSelectedEventIndex = Item->Index;
        ShowEventDetails(FSelectedEventIndex);
    }
}
//---------------------------------------------------------------------------
void __fastcall TfrmMain::rbNewSessionClick(TObject *Sender)
{
    FState.ResumeMode = false;
}
//---------------------------------------------------------------------------
void __fastcall TfrmMain::rbContinueClick(TObject *Sender)
{
    FState.ResumeMode = true;
}
//---------------------------------------------------------------------------
UnicodeString TfrmMain::GetStatusText() const
{
    if (StatusBar->Panels->Count > 0)
        return StatusBar->Panels->Items[0]->Text;
    return "";
}
//---------------------------------------------------------------------------
std::vector<TEventData> TfrmMain::GetEvents(int limit, int offset) const
{
    return FEventStore.GetRange(limit, offset);
}
//---------------------------------------------------------------------------
TEventData TfrmMain::GetEventDetails(int index) const
{
    return FEventStore.GetSafe(index);
}
//---------------------------------------------------------------------------
json TfrmMain::GetStatusBarPanels() const
{
    json panels = json::array();
    for (int i = 0; i < StatusBar->Panels->Count; i++) {
        panels.push_back(utf8(StatusBar->Panels->Items[i]->Text));
    }
    return panels;
}
//---------------------------------------------------------------------------
