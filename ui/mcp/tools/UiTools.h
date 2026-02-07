//---------------------------------------------------------------------------
// UiTools.h — MCP tools for controlling ClaBot UI
//
// Provides tools for Claude to interact with the UI:
// - ui_get_status: Get general UI status
// - ui_get_events: Get list of events
// - ui_get_controls: Get state of all controls
// - ui_click_*: Click buttons
// - ui_set_*: Set control values
// - ui_wait_events: Wait for N events
//---------------------------------------------------------------------------

#ifndef UiToolsH
#define UiToolsH

//---------------------------------------------------------------------------
#include "../McpServer.h"
#include "UcodeUtf8.h"
#include "../../uMain.h"
#include <Vcl.Forms.hpp>
#include <Vcl.StdCtrls.hpp>
#include <Vcl.ComCtrls.hpp>
#include <chrono>
#include <thread>

namespace Mcp { namespace Tools {

//---------------------------------------------------------------------------
// Helper to safely execute on main VCL thread
//---------------------------------------------------------------------------
template<typename Func>
void SyncCall(Func func)
{
    TThread::Synchronize(nullptr, [&func]() { func(); });
}

//---------------------------------------------------------------------------
// Register all UI tools with the MCP server
// @param server The MCP server to register tools with
// @param form Pointer to the main form (TfrmMain)
//---------------------------------------------------------------------------
inline void RegisterUiTools(TMcpServer &server, TfrmMain *form)
{
    // ui_get_status - Get general UI status
    server.RegisterLambda(
        "ui_get_status",
        "Get general status of the ClaBot UI: connection state, agent ID, events count, status bar text",
        TMcpToolSchema(),
        [form](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
            if (!form)
                return TMcpToolResult::Error("Form not initialized");

            json result;
            SyncCall([&]() {
                // Access form properties via public methods we'll add
                result["connected"] = form->IsConnected();
                result["agentId"] = utf8(form->GetAgentId());
                result["eventsCount"] = form->GetEventCount();
                result["statusText"] = utf8(form->GetStatusText());
            });
            return TMcpToolResult::Success(result);
        }
    );

    // ui_get_events - Get list of events
    server.RegisterLambda(
        "ui_get_events",
        "Get list of all events from the events list. Returns array of {time, type, data, toolInput, toolOutput, toolUseId, requestId, durationMs} objects",
        TMcpToolSchema()
            .AddInteger("limit", "Maximum number of events to return (0 = all, default 100)")
            .AddInteger("offset", "Skip first N events (default 0)")
            .AddBoolean("include_details", "Include full tool input/output (default false)", false),
        [form](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
            if (!form)
                return TMcpToolResult::Error("Form not initialized");

            int limit = TMcpToolBase::GetInt(args, "limit", 100);
            int offset = TMcpToolBase::GetInt(args, "offset", 0);
            bool includeDetails = TMcpToolBase::GetBool(args, "include_details", false);

            json events = json::array();
            SyncCall([&]() {
                auto eventList = form->GetEvents(limit, offset);
                for (const auto &ev : eventList) {
                    json eventJson = {
                        {"time", utf8(ev.Time)},
                        {"type", utf8(ev.Type)},
                        {"data", utf8(ev.Data)}
                    };
                    if (includeDetails) {
                        eventJson["toolInput"] = utf8(ev.ToolInput);
                        eventJson["toolOutput"] = utf8(ev.ToolOutput);
                        eventJson["toolUseId"] = utf8(ev.ToolUseId);
                        eventJson["requestId"] = utf8(ev.RequestId);
                        eventJson["durationMs"] = ev.DurationMs;
                    }
                    events.push_back(eventJson);
                }
            });

            json result;
            result["events"] = events;
            result["total"] = form->GetEventCount();
            return TMcpToolResult::Success(result);
        }
    );

    // ui_get_controls - Get state of all controls
    server.RegisterLambda(
        "ui_get_controls",
        "Get state of all UI controls: server URL, agent name, prompt, and button states (enabled/disabled)",
        TMcpToolSchema(),
        [form](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
            if (!form)
                return TMcpToolResult::Error("Form not initialized");

            json result;
            SyncCall([&]() {
                result["serverUrl"] = utf8(form->GetServerUrl());
                result["agentName"] = utf8(form->GetAgentName());
                result["agentId"] = utf8(form->GetAgentId());
                result["prompt"] = utf8(form->GetPrompt());
                result["buttons"] = json{
                    {"connect", json{{"enabled", form->IsConnectEnabled()}}},
                    {"createAgent", json{{"enabled", form->IsCreateAgentEnabled()}}},
                    {"send", json{{"enabled", form->IsSendEnabled()}}},
                    {"stop", json{{"enabled", form->IsStopEnabled()}}}
                };
            });
            return TMcpToolResult::Success(result);
        }
    );

    // ui_click_connect - Click Connect button
    server.RegisterLambda(
        "ui_click_connect",
        "Click the Connect button to connect to the orchestrator server",
        TMcpToolSchema(),
        [form](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
            if (!form)
                return TMcpToolResult::Error("Form not initialized");

            bool clicked = false;
            SyncCall([&]() {
                if (form->IsConnectEnabled()) {
                    form->ClickConnect();
                    clicked = true;
                }
            });

            if (!clicked)
                return TMcpToolResult::Error("Connect button is disabled");

            return TMcpToolResult::Success(json{{"clicked", true}});
        }
    );

    // ui_click_create_agent - Click Create Agent button
    server.RegisterLambda(
        "ui_click_create_agent",
        "Click the Create Agent button to create a new agent session",
        TMcpToolSchema(),
        [form](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
            if (!form)
                return TMcpToolResult::Error("Form not initialized");

            bool clicked = false;
            SyncCall([&]() {
                if (form->IsCreateAgentEnabled()) {
                    form->ClickCreateAgent();
                    clicked = true;
                }
            });

            if (!clicked)
                return TMcpToolResult::Error("Create Agent button is disabled");

            return TMcpToolResult::Success(json{{"clicked", true}});
        }
    );

    // ui_click_send - Click Send button
    server.RegisterLambda(
        "ui_click_send",
        "Click the Send button to send the current prompt to the agent",
        TMcpToolSchema(),
        [form](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
            if (!form)
                return TMcpToolResult::Error("Form not initialized");

            bool clicked = false;
            SyncCall([&]() {
                if (form->IsSendEnabled()) {
                    form->ClickSend();
                    clicked = true;
                }
            });

            if (!clicked)
                return TMcpToolResult::Error("Send button is disabled");

            return TMcpToolResult::Success(json{{"clicked", true}});
        }
    );

    // ui_click_stop - Click Stop button
    server.RegisterLambda(
        "ui_click_stop",
        "Click the Stop button to interrupt the current agent execution",
        TMcpToolSchema(),
        [form](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
            if (!form)
                return TMcpToolResult::Error("Form not initialized");

            bool clicked = false;
            SyncCall([&]() {
                if (form->IsStopEnabled()) {
                    form->ClickStop();
                    clicked = true;
                }
            });

            if (!clicked)
                return TMcpToolResult::Error("Stop button is disabled");

            return TMcpToolResult::Success(json{{"clicked", true}});
        }
    );

    // ui_set_server_url - Set server URL
    server.RegisterLambda(
        "ui_set_server_url",
        "Set the orchestrator server URL",
        TMcpToolSchema()
            .AddString("url", "The server URL (e.g., http://localhost:3000)", true),
        [form](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
            if (!form)
                return TMcpToolResult::Error("Form not initialized");

            std::string url = TMcpToolBase::GetString(args, "url");
            if (url.empty())
                return TMcpToolResult::Error("URL is required");

            SyncCall([&]() {
                form->SetServerUrl(u(url));
            });

            return TMcpToolResult::Success(json{{"set", true}, {"url", url}});
        }
    );

    // ui_set_agent_name - Set agent name
    server.RegisterLambda(
        "ui_set_agent_name",
        "Set the name for the agent to be created",
        TMcpToolSchema()
            .AddString("name", "The agent name", true),
        [form](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
            if (!form)
                return TMcpToolResult::Error("Form not initialized");

            std::string name = TMcpToolBase::GetString(args, "name");
            if (name.empty())
                return TMcpToolResult::Error("Name is required");

            SyncCall([&]() {
                form->SetAgentName(u(name));
            });

            return TMcpToolResult::Success(json{{"set", true}, {"name", name}});
        }
    );

    // ui_set_prompt - Set prompt text
    server.RegisterLambda(
        "ui_set_prompt",
        "Set the prompt text to send to the agent",
        TMcpToolSchema()
            .AddString("text", "The prompt text", true),
        [form](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
            if (!form)
                return TMcpToolResult::Error("Form not initialized");

            std::string text = TMcpToolBase::GetString(args, "text");

            SyncCall([&]() {
                form->SetPrompt(u(text));
            });

            return TMcpToolResult::Success(json{{"set", true}, {"text", text}});
        }
    );

    // ui_wait_events - Wait for N events
    server.RegisterLambda(
        "ui_wait_events",
        "Wait until the events count reaches at least N, or timeout occurs",
        TMcpToolSchema()
            .AddInteger("count", "Minimum number of events to wait for", true)
            .AddInteger("timeout_ms", "Timeout in milliseconds (default 30000)", false),
        [form](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
            if (!form)
                return TMcpToolResult::Error("Form not initialized");

            int targetCount = TMcpToolBase::GetInt(args, "count", 1);
            int timeoutMs = TMcpToolBase::GetInt(args, "timeout_ms", 30000);

            auto start = std::chrono::steady_clock::now();
            int currentCount = 0;

            while (true) {
                SyncCall([&]() {
                    currentCount = form->GetEventCount();
                });

                if (currentCount >= targetCount) {
                    return TMcpToolResult::Success(json{
                        {"reached", true},
                        {"eventsCount", currentCount},
                        {"targetCount", targetCount}
                    });
                }

                auto elapsed = std::chrono::steady_clock::now() - start;
                if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= timeoutMs) {
                    return TMcpToolResult::Success(json{
                        {"reached", false},
                        {"timedOut", true},
                        {"eventsCount", currentCount},
                        {"targetCount", targetCount}
                    });
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    );

    // ui_get_all - Get complete UI state
    server.RegisterLambda(
        "ui_get_all",
        "Get complete state of the entire UI: all edit fields, all status bar panels, button states, and recent events",
        TMcpToolSchema()
            .AddInteger("events_limit", "Maximum events to include (default 10)", false),
        [form](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
            if (!form)
                return TMcpToolResult::Error("Form not initialized");

            int eventsLimit = TMcpToolBase::GetInt(args, "events_limit", 10);

            json result;
            SyncCall([&]() {
                // Edit fields
                result["fields"] = json{
                    {"serverUrl", utf8(form->GetServerUrl())},
                    {"agentName", utf8(form->GetAgentName())},
                    {"agentId", utf8(form->GetAgentId())},
                    {"prompt", utf8(form->GetPrompt())}
                };

                // Status bar panels
                result["statusBar"] = form->GetStatusBarPanels();

                // Button states
                result["buttons"] = json{
                    {"connect", form->IsConnectEnabled()},
                    {"createAgent", form->IsCreateAgentEnabled()},
                    {"send", form->IsSendEnabled()},
                    {"stop", form->IsStopEnabled()}
                };

                // Internal state
                result["state"] = json{
                    {"connected", form->IsConnected()},
                    {"eventsCount", form->GetEventCount()}
                };

                // Session info
                result["session"] = json{
                    {"sdkSessionId", utf8(form->GetSdkSessionId())},
                    {"canResume", form->GetCanResume()},
                    {"resumeMode", form->GetResumeMode()},
                    {"inputTokens", form->GetInputTokens()},
                    {"outputTokens", form->GetOutputTokens()},
                    {"totalCostUsd", form->GetTotalCostUsd()}
                };

                // Recent events
                json events = json::array();
                auto eventList = form->GetEvents(eventsLimit, 0);
                for (const auto &ev : eventList) {
                    events.push_back(json{
                        {"time", utf8(ev.Time)},
                        {"type", utf8(ev.Type)},
                        {"data", utf8(ev.Data)}
                    });
                }
                result["recentEvents"] = events;
            });

            return TMcpToolResult::Success(result);
        }
    );

    // ui_get_session - Get session info for resume
    server.RegisterLambda(
        "ui_get_session",
        "Get session information including SDK session ID, resume capability, token usage, and cost",
        TMcpToolSchema(),
        [form](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
            if (!form)
                return TMcpToolResult::Error("Form not initialized");

            json result;
            SyncCall([&]() {
                result["sdkSessionId"] = utf8(form->GetSdkSessionId());
                result["canResume"] = form->GetCanResume();
                result["resumeMode"] = form->GetResumeMode();
                result["inputTokens"] = form->GetInputTokens();
                result["outputTokens"] = form->GetOutputTokens();
                result["totalCostUsd"] = form->GetTotalCostUsd();
            });
            return TMcpToolResult::Success(result);
        }
    );

    // ui_set_resume_mode - Set resume mode on/off
    server.RegisterLambda(
        "ui_set_resume_mode",
        "Enable or disable session resume mode. When enabled and session supports it, subsequent queries will continue the same session",
        TMcpToolSchema()
            .AddBoolean("resume", "true to continue existing session, false to start new session", true),
        [form](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
            if (!form)
                return TMcpToolResult::Error("Form not initialized");

            bool resume = TMcpToolBase::GetBool(args, "resume", false);

            SyncCall([&]() {
                form->SetResumeMode(resume);
            });

            json result;
            result["set"] = true;
            result["resumeMode"] = resume;
            result["canResume"] = form->GetCanResume();
            return TMcpToolResult::Success(result);
        }
    );

    // ui_get_event_details - Get full details of an event by index
    server.RegisterLambda(
        "ui_get_event_details",
        "Get full details of a specific event by index, including tool input/output JSON",
        TMcpToolSchema()
            .AddInteger("index", "Event index (0-based)", true),
        [form](const json &args, TMcpToolContext &ctx) -> TMcpToolResult {
            if (!form)
                return TMcpToolResult::Error("Form not initialized");

            int index = TMcpToolBase::GetInt(args, "index", -1);
            if (index < 0)
                return TMcpToolResult::Error("Invalid index");

            json result;
            SyncCall([&]() {
                auto ev = form->GetEventDetails(index);
                result["time"] = utf8(ev.Time);
                result["type"] = utf8(ev.Type);
                result["data"] = utf8(ev.Data);
                result["toolInput"] = utf8(ev.ToolInput);
                result["toolOutput"] = utf8(ev.ToolOutput);
                result["toolUseId"] = utf8(ev.ToolUseId);
                result["requestId"] = utf8(ev.RequestId);
                result["durationMs"] = ev.DurationMs;
            });

            return TMcpToolResult::Success(result);
        }
    );
}

}} // namespace Mcp::Tools

//---------------------------------------------------------------------------
#endif
