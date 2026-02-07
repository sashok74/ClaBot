//---------------------------------------------------------------------------
// uEventParser.h — SSE JSON event parser (header-only)
//
// Parses incoming JSON SSE events into TEventData + side-effects.
// Extracted from TfrmMain::OnSSEEvent to follow Single Responsibility.
//---------------------------------------------------------------------------

#ifndef uEventParserH
#define uEventParserH

//---------------------------------------------------------------------------
#include "json.hpp"
#include "UcodeUtf8.h"
#include "uEventStore.h"

//---------------------------------------------------------------------------
// Side-effects produced by parsing an event
//---------------------------------------------------------------------------
struct TEventParseResult
{
    TEventData EventData;

    // Side-effects: session info updates
    bool HasSessionInfo = false;
    UnicodeString NewSdkSessionId;
    bool NewCanResume = false;

    // Side-effects: usage stats updates
    bool HasUsage = false;
    int InputTokens = 0;
    int OutputTokens = 0;
    double TotalCostUsd = 0.0;

    // Side-effects: status updates
    bool HasStatusUpdate = false;
    UnicodeString StatusText;

    // Side-effects: session ended
    bool SessionEnded = false;
};

//---------------------------------------------------------------------------
// TEventParser — stateless parser for SSE JSON events
//---------------------------------------------------------------------------
class TEventParser
{
public:
    static TEventParseResult Parse(const nlohmann::json &Event)
    {
        TEventParseResult r;
        r.EventData.Time = Now().FormatString("hh:nn:ss");
        r.EventData.DurationMs = 0;

        UnicodeString type = "";
        if (Event.contains("type")) {
            type = u(Event["type"].get<std::string>());
        }
        r.EventData.Type = type;

        if (type == "thinking") {
            if (Event.contains("content")) {
                r.EventData.Data = u(Event["content"].get<std::string>());
            }
        }
        else if (type == "tool_start") {
            if (Event.contains("tool")) {
                r.EventData.Data = u(Event["tool"].get<std::string>()) + ": started";
            }
            if (Event.contains("input")) {
                r.EventData.ToolInput = u(Event["input"].dump(2));
            }
            if (Event.contains("toolUseId")) {
                r.EventData.ToolUseId = u(Event["toolUseId"].get<std::string>());
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
            r.EventData.Data = tool + ": done (" + IntToStr(duration) + "ms)";
            r.EventData.DurationMs = duration;

            if (Event.contains("input")) {
                r.EventData.ToolInput = u(Event["input"].dump(2));
            }
            if (Event.contains("output")) {
                r.EventData.ToolOutput = u(Event["output"].dump(2));
            }
            if (Event.contains("toolUseId")) {
                r.EventData.ToolUseId = u(Event["toolUseId"].get<std::string>());
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

            r.EventData.Data = tool + ": error - " + errorText;
            r.EventData.ToolOutput = u(nlohmann::json{{"error", utf8(errorText)}}.dump(2));

            if (Event.contains("toolUseId")) {
                r.EventData.ToolUseId = u(Event["toolUseId"].get<std::string>());
            }
        }
        else if (type == "permission_request") {
            UnicodeString tool = "unknown";
            if (Event.contains("tool")) {
                tool = u(Event["tool"].get<std::string>());
            }
            r.EventData.Data = tool + ": permission requested";

            if (Event.contains("input")) {
                r.EventData.ToolInput = u(Event["input"].dump(2));
            }
            if (Event.contains("requestId")) {
                r.EventData.RequestId = u(Event["requestId"].get<std::string>());
            }

            r.HasStatusUpdate = true;
            r.StatusText = "Permission request: " + tool;
        }
        else if (type == "assistant_message") {
            if (Event.contains("content")) {
                r.EventData.Data = u(Event["content"].get<std::string>());
            }
        }
        else if (type == "session_start") {
            r.EventData.Data = "Session started";
        }
        else if (type == "session_info") {
            r.HasSessionInfo = true;
            if (Event.contains("sdkSessionId")) {
                r.NewSdkSessionId = u(Event["sdkSessionId"].get<std::string>());
            }
            if (Event.contains("canResume")) {
                r.NewCanResume = Event["canResume"].get<bool>();
            }
            r.EventData.Data = "Session ID: " + r.NewSdkSessionId.SubString(1, 12) + "...";
        }
        else if (type == "session_end") {
            UnicodeString reason = "";
            if (Event.contains("reason")) {
                reason = u(Event["reason"].get<std::string>());
            }
            if (Event.contains("usage")) {
                const auto &usage = Event["usage"];
                r.HasUsage = true;
                if (usage.contains("inputTokens")) {
                    r.InputTokens = usage["inputTokens"].get<int>();
                }
                if (usage.contains("outputTokens")) {
                    r.OutputTokens = usage["outputTokens"].get<int>();
                }
                if (usage.contains("totalCostUsd")) {
                    r.TotalCostUsd = usage["totalCostUsd"].get<double>();
                }
            }
            r.EventData.Data = "Session ended: " + reason;
            r.SessionEnded = true;
            r.HasStatusUpdate = true;
            r.StatusText = "Completed";
        }
        else if (type == "user_message") {
            if (Event.contains("content")) {
                r.EventData.Data = "User: " + u(Event["content"].get<std::string>());
            }
        }
        else if (type == "connected") {
            r.EventData.Data = "SSE connected";
        }
        else if (type == "error") {
            if (Event.contains("message")) {
                r.EventData.Data = "Error: " + u(Event["message"].get<std::string>());
            } else {
                r.EventData.Data = "Error";
            }
        }
        else {
            r.EventData.Data = type;
        }

        return r;
    }
};

//---------------------------------------------------------------------------
#endif
