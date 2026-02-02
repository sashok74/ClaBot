//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop
#include "uMain.h"
#include "uMcpServer.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TfrmMain *frmMain;
//---------------------------------------------------------------------------
__fastcall TfrmMain::TfrmMain(TComponent* Owner)
    : TForm(Owner), FHttpClient(nullptr), FSSEClient(nullptr), FEventCount(0),
      FConnected(false), FAgentCreated(false), FRunning(false)
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
            FCurrentAgentId = u(id);
            edtAgentId->Text = FCurrentAgentId;

            // Connect SSE
            UnicodeString sseUrl = FHttpClient->BaseUrl + "/agent/" + FCurrentAgentId + "/events";
            FSSEClient->Connect(sseUrl);

            // Clear events
            lvEvents->Items->Clear();
            FEventCount = 0;

            SetControlsState(true, true, false);
            UpdateStatus("Agent created: " + FCurrentAgentId.SubString(1, 8) + "...");
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

    if (FCurrentAgentId.IsEmpty()) {
        ShowMessage("Create an agent first");
        return;
    }

    try {
        json body;
        body["prompt"] = utf8(prompt);

        json response = FHttpClient->Post("/agent/" + FCurrentAgentId + "/query", body);

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
    if (FCurrentAgentId.IsEmpty()) {
        return;
    }

    try {
        json response = FHttpClient->Post("/agent/" + FCurrentAgentId + "/interrupt", json::object());
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
    UnicodeString type = "";
    UnicodeString data = "";

    if (Event.contains("type")) {
        type = u(Event["type"].get<std::string>());
    }

    if (type == "thinking") {
        if (Event.contains("content")) {
            data = u(Event["content"].get<std::string>());
        }
    }
    else if (type == "tool_start") {
        if (Event.contains("tool")) {
            data = u(Event["tool"].get<std::string>()) + ": started";
        }
    }
    else if (type == "tool_end") {
        UnicodeString tool = "";
        int duration = 0;
        if (Event.contains("tool")) {
            tool = u(Event["tool"].get<std::string>());
        }
        if (Event.contains("durationMs")) {
            duration = Event["durationMs"].get<int>();
        }
        data = tool + ": done (" + IntToStr(duration) + "ms)";
    }
    else if (type == "assistant_message") {
        if (Event.contains("content")) {
            data = u(Event["content"].get<std::string>());
        }
    }
    else if (type == "session_start") {
        data = "Session started";
    }
    else if (type == "session_end") {
        UnicodeString reason = "";
        if (Event.contains("reason")) {
            reason = u(Event["reason"].get<std::string>());
        }
        data = "Session ended: " + reason;
        SetControlsState(true, true, false);
        UpdateStatus("Completed");
    }
    else if (type == "user_message") {
        if (Event.contains("content")) {
            data = "User: " + u(Event["content"].get<std::string>());
        }
    }
    else if (type == "connected") {
        data = "SSE connected";
    }
    else {
        data = type;
    }

    AddEvent(Now().FormatString("hh:nn:ss"), type, data);
}
//---------------------------------------------------------------------------
void __fastcall TfrmMain::OnSSEError(TObject *Sender, const UnicodeString &Error)
{
    AddEvent(Now().FormatString("hh:nn:ss"), "error", Error);
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
    StatusBar->Panels->Items[1]->Text = "Agent: " + (FCurrentAgentId.IsEmpty() ? "-" : FCurrentAgentId.SubString(1, 8));
    StatusBar->Panels->Items[2]->Text = "Events: " + IntToStr(FEventCount);
}
//---------------------------------------------------------------------------
void TfrmMain::AddEvent(const UnicodeString &Time, const UnicodeString &Type, const UnicodeString &Data)
{
    TListItem *item = lvEvents->Items->Add();
    item->Caption = Time;
    item->SubItems->Add(Type);
    item->SubItems->Add(Data);

    FEventCount++;
    StatusBar->Panels->Items[2]->Text = "Events: " + IntToStr(FEventCount);

    // Auto-scroll to bottom
    item->MakeVisible(false);
}
//---------------------------------------------------------------------------
void TfrmMain::SetControlsState(bool Connected, bool AgentCreated, bool Running)
{
    // Track state for MCP tools
    FConnected = Connected;
    FAgentCreated = AgentCreated;
    FRunning = Running;

    btnConnect->Enabled = !Connected;
    edtServer->Enabled = !Connected;

    grpAgent->Enabled = Connected;
    btnCreateAgent->Enabled = Connected && !Running;
    edtAgentName->Enabled = Connected && !AgentCreated;

    edtPrompt->Enabled = AgentCreated && !Running;
    btnSend->Enabled = AgentCreated && !Running;
    btnStop->Enabled = Running;
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
    std::vector<TEventData> result;

    int count = lvEvents->Items->Count;
    int start = offset;
    int end = count;

    if (limit > 0 && start + limit < end)
        end = start + limit;

    for (int i = start; i < end; i++) {
        TListItem *item = lvEvents->Items->Item[i];
        TEventData ev;
        ev.Time = item->Caption;
        if (item->SubItems->Count > 0)
            ev.Type = item->SubItems->Strings[0];
        if (item->SubItems->Count > 1)
            ev.Data = item->SubItems->Strings[1];
        result.push_back(ev);
    }

    return result;
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
