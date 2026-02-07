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
      FConnected(false), FAgentCreated(false), FRunning(false),
      FCanResume(false), FResumeMode(false), FInputTokens(0), FOutputTokens(0),
      FTotalCostUsd(0.0), FSelectedEventIndex(-1)
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
            FEventDataList.clear();
            FEventCount = 0;
            mmoDetails->Clear();
            FSelectedEventIndex = -1;

            // Reset session info
            FSdkSessionId = "";
            FCanResume = false;
            FInputTokens = 0;
            FOutputTokens = 0;
            FTotalCostUsd = 0.0;
            UpdateSessionInfo();

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
        // Add resume flag if user selected Continue mode and session supports it
        if (FResumeMode && FCanResume) {
            body["resume"] = true;
        }

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
    TEventData eventData;
    eventData.Time = Now().FormatString("hh:nn:ss");
    eventData.DurationMs = 0;

    UnicodeString type = "";
    if (Event.contains("type")) {
        type = u(Event["type"].get<std::string>());
    }
    eventData.Type = type;

    if (type == "thinking") {
        if (Event.contains("content")) {
            eventData.Data = u(Event["content"].get<std::string>());
        }
    }
    else if (type == "tool_start") {
        if (Event.contains("tool")) {
            eventData.Data = u(Event["tool"].get<std::string>()) + ": started";
        }
        if (Event.contains("input")) {
            eventData.ToolInput = u(Event["input"].dump(2));
        }
        if (Event.contains("toolUseId")) {
            eventData.ToolUseId = u(Event["toolUseId"].get<std::string>());
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
        eventData.Data = tool + ": done (" + IntToStr(duration) + "ms)";
        eventData.DurationMs = duration;

        if (Event.contains("input")) {
            eventData.ToolInput = u(Event["input"].dump(2));
        }
        if (Event.contains("output")) {
            eventData.ToolOutput = u(Event["output"].dump(2));
        }
        if (Event.contains("toolUseId")) {
            eventData.ToolUseId = u(Event["toolUseId"].get<std::string>());
        }
    }
    else if (type == "tool_error") {
        UnicodeString tool = "";
        UnicodeString errorText = "Unknown tool error";

        if (Event.contains("tool")) {
            tool = u(Event["tool"].get<std::string>());
        }
        if (Event.contains("error")) {
            if (Event["error"].is_string()) {
                errorText = u(Event["error"].get<std::string>());
            } else {
                errorText = u(Event["error"].dump());
            }
        }

        eventData.Data = tool + ": error - " + errorText;
        eventData.ToolOutput = u(json{{"error", utf8(errorText)}}.dump(2));

        if (Event.contains("toolUseId")) {
            eventData.ToolUseId = u(Event["toolUseId"].get<std::string>());
        }
    }
    else if (type == "permission_request") {
        UnicodeString tool = "unknown";
        if (Event.contains("tool")) {
            tool = u(Event["tool"].get<std::string>());
        }
        eventData.Data = tool + ": permission requested";

        if (Event.contains("input")) {
            eventData.ToolInput = u(Event["input"].dump(2));
        }
        if (Event.contains("requestId")) {
            eventData.RequestId = u(Event["requestId"].get<std::string>());
        }

        UpdateStatus("Permission request: " + tool);
    }
    else if (type == "assistant_message") {
        if (Event.contains("content")) {
            eventData.Data = u(Event["content"].get<std::string>());
        }
    }
    else if (type == "session_start") {
        eventData.Data = "Session started";
    }
    else if (type == "session_info") {
        // Update session info from server
        if (Event.contains("sdkSessionId")) {
            FSdkSessionId = u(Event["sdkSessionId"].get<std::string>());
        }
        if (Event.contains("canResume")) {
            FCanResume = Event["canResume"].get<bool>();
        }
        UpdateSessionInfo();
        eventData.Data = "Session ID: " + FSdkSessionId.SubString(1, 12) + "...";
    }
    else if (type == "session_end") {
        UnicodeString reason = "";
        if (Event.contains("reason")) {
            reason = u(Event["reason"].get<std::string>());
        }
        // Extract usage stats
        if (Event.contains("usage")) {
            const auto &usage = Event["usage"];
            if (usage.contains("inputTokens")) {
                FInputTokens = usage["inputTokens"].get<int>();
            }
            if (usage.contains("outputTokens")) {
                FOutputTokens = usage["outputTokens"].get<int>();
            }
            if (usage.contains("totalCostUsd")) {
                FTotalCostUsd = usage["totalCostUsd"].get<double>();
            }
            UpdateSessionInfo();
        }
        eventData.Data = "Session ended: " + reason;
        SetControlsState(true, true, false);
        UpdateStatus("Completed");
    }
    else if (type == "user_message") {
        if (Event.contains("content")) {
            eventData.Data = "User: " + u(Event["content"].get<std::string>());
        }
    }
    else if (type == "connected") {
        eventData.Data = "SSE connected";
    }
    else if (type == "error") {
        if (Event.contains("message")) {
            eventData.Data = "Error: " + u(Event["message"].get<std::string>());
        } else {
            eventData.Data = "Error";
        }
    }
    else {
        eventData.Data = type;
    }

    AddEvent(eventData);
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
    StatusBar->Panels->Items[1]->Text = "Agent: " + (FCurrentAgentId.IsEmpty() ? "-" : FCurrentAgentId.SubString(1, 8));
    StatusBar->Panels->Items[2]->Text = "Events: " + IntToStr(FEventCount);
    // Session info in panel 3
    if (FCanResume) {
        StatusBar->Panels->Items[3]->Text = "Session: resumable";
    } else {
        StatusBar->Panels->Items[3]->Text = "Session: -";
    }
}
//---------------------------------------------------------------------------
void TfrmMain::AddEvent(const TEventData &eventData)
{
    // Store full event data
    FEventDataList.push_back(eventData);

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

    // Session controls
    grpSession->Enabled = Connected;
    rbNewSession->Enabled = Connected && !Running;
    rbContinue->Enabled = Connected && FCanResume && !Running;

    edtPrompt->Enabled = AgentCreated && !Running;
    btnSend->Enabled = AgentCreated && !Running;
    btnStop->Enabled = Running;
}
//---------------------------------------------------------------------------
void TfrmMain::UpdateSessionInfo()
{
    // Update session ID display
    if (FSdkSessionId.IsEmpty()) {
        lblSessionIdValue->Caption = "-";
    } else {
        lblSessionIdValue->Caption = FSdkSessionId.SubString(1, 10) + "...";
    }

    // Update tokens display
    lblTokensValue->Caption = IntToStr(FInputTokens) + "/" + IntToStr(FOutputTokens);

    // Update cost display
    lblCostValue->Caption = "$" + FormatFloat("0.000", FTotalCostUsd);

    // Enable/disable continue radio button based on canResume
    rbContinue->Enabled = FCanResume && !FRunning;
}
//---------------------------------------------------------------------------
void TfrmMain::ShowEventDetails(int index)
{
    mmoDetails->Clear();

    if (index < 0 || index >= (int)FEventDataList.size()) {
        return;
    }

    const TEventData &ev = FEventDataList[index];

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
    FResumeMode = false;
}
//---------------------------------------------------------------------------
void __fastcall TfrmMain::rbContinueClick(TObject *Sender)
{
    FResumeMode = true;
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

    int count = FEventDataList.size();
    int start = offset;
    int end = count;

    if (limit > 0 && start + limit < end)
        end = start + limit;

    for (int i = start; i < end; i++) {
        result.push_back(FEventDataList[i]);
    }

    return result;
}
//---------------------------------------------------------------------------
TEventData TfrmMain::GetEventDetails(int index) const
{
    if (index >= 0 && index < (int)FEventDataList.size()) {
        return FEventDataList[index];
    }
    return TEventData();
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
