//---------------------------------------------------------------------------
// uIAppState.h — Abstract interface for MCP tools (header-only)
//
// Decouples UiTools from TfrmMain. TfrmMain implements IAppState.
// All methods already exist on TfrmMain — just adds virtual interface.
//---------------------------------------------------------------------------

#ifndef uIAppStateH
#define uIAppStateH

//---------------------------------------------------------------------------
#include <System.SysUtils.hpp>
#include "json.hpp"
#include "../services/uEventStore.h"
#include <vector>

//---------------------------------------------------------------------------
// IAppState — interface used by MCP tools to query/control the UI
//---------------------------------------------------------------------------
class IAppState
{
protected:
    ~IAppState() {}

public:

    // Status
    virtual bool IsConnected() const = 0;
    virtual UnicodeString GetAgentId() const = 0;
    virtual int GetEventCount() const = 0;
    virtual UnicodeString GetStatusText() const = 0;
    virtual nlohmann::json GetStatusBarPanels() const = 0;

    // Events
    virtual std::vector<TEventData> GetEvents(int limit, int offset) const = 0;
    virtual TEventData GetEventDetails(int index) const = 0;

    // Control getters
    virtual UnicodeString GetServerUrl() const = 0;
    virtual UnicodeString GetAgentName() const = 0;
    virtual UnicodeString GetPrompt() const = 0;

    // Button states
    virtual bool IsConnectEnabled() const = 0;
    virtual bool IsCreateAgentEnabled() const = 0;
    virtual bool IsSendEnabled() const = 0;
    virtual bool IsStopEnabled() const = 0;

    // Button click actions
    virtual void ClickConnect() = 0;
    virtual void ClickCreateAgent() = 0;
    virtual void ClickSend() = 0;
    virtual void ClickStop() = 0;

    // Control setters
    virtual void SetServerUrl(const UnicodeString &url) = 0;
    virtual void SetAgentName(const UnicodeString &name) = 0;
    virtual void SetPrompt(const UnicodeString &text) = 0;

    // Session info
    virtual UnicodeString GetSdkSessionId() const = 0;
    virtual bool GetCanResume() const = 0;
    virtual bool GetResumeMode() const = 0;
    virtual int GetInputTokens() const = 0;
    virtual int GetOutputTokens() const = 0;
    virtual double GetTotalCostUsd() const = 0;

    // Resume mode
    virtual void SetResumeMode(bool resume) = 0;
};

//---------------------------------------------------------------------------
#endif
