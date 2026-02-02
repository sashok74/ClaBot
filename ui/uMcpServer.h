//---------------------------------------------------------------------------
// uMcpServer.h — MCP server wrapper for ClaBot UI
//
// Encapsulates TIdHTTPServer + TMcpServer + HttpTransport
// Listens on localhost:8767 by default
//---------------------------------------------------------------------------

#ifndef uMcpServerH
#define uMcpServerH

//---------------------------------------------------------------------------
#include <memory>
#include <IdHTTPServer.hpp>
#include "mcp/McpServer.h"
#include "mcp/transport/http/HttpTransport.h"

// Forward declaration
class TfrmMain;

//---------------------------------------------------------------------------
// TUiMcpServer — MCP server for UI control
//---------------------------------------------------------------------------
class TUiMcpServer
{
public:
    TUiMcpServer();
    ~TUiMcpServer();

    // Start the MCP server on specified port
    void Start(int port = 8767);

    // Stop the server
    void Stop();

    // Check if server is running
    bool IsRunning() const;

    // Register UI tools (call after Start, before using)
    void RegisterUiTools(TfrmMain *form);

    // Get the MCP server (for additional tool registration)
    Mcp::TMcpServer* GetMcpServer() { return FMcpServer.get(); }

private:
    std::unique_ptr<TIdHTTPServer> FHttpServer;
    std::unique_ptr<Mcp::TMcpServer> FMcpServer;
    std::unique_ptr<Mcp::Transport::HttpTransport> FTransport;

    // Indy event handler
    void __fastcall OnCommandGet(TIdContext *AContext,
        TIdHTTPRequestInfo *ARequestInfo, TIdHTTPResponseInfo *AResponseInfo);
};

//---------------------------------------------------------------------------
#endif
