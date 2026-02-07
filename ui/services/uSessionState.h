//---------------------------------------------------------------------------
// uSessionState.h — Session state tracking (header-only)
//
// Tracks agent session state: IDs, resume capability, token usage, costs.
// Extracted from TfrmMain to follow Single Responsibility Principle.
//---------------------------------------------------------------------------

#ifndef uSessionStateH
#define uSessionStateH

//---------------------------------------------------------------------------
#include <System.SysUtils.hpp>

//---------------------------------------------------------------------------
// TSessionState — tracks session-related state
//---------------------------------------------------------------------------
class TSessionState
{
public:
    UnicodeString SdkSessionId;
    bool CanResume = false;
    bool ResumeMode = false;   // User's choice: resume or new session
    int InputTokens = 0;
    int OutputTokens = 0;
    double TotalCostUsd = 0.0;
    UnicodeString CurrentAgentId;
    bool Connected = false;
    bool AgentCreated = false;
    bool Running = false;

    void Reset()
    {
        SdkSessionId = "";
        CanResume = false;
        InputTokens = 0;
        OutputTokens = 0;
        TotalCostUsd = 0.0;
    }

    void ResetAll()
    {
        Reset();
        CurrentAgentId = "";
        Connected = false;
        AgentCreated = false;
        Running = false;
        ResumeMode = false;
    }
};

//---------------------------------------------------------------------------
#endif
