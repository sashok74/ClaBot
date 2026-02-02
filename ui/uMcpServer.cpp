//---------------------------------------------------------------------------
// uMcpServer.cpp — MCP server wrapper for ClaBot UI
//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "uMcpServer.h"
#include "mcp/tools/UiTools.h"
#include "mcp/transport/http/HttpRequest.h"
#include "mcp/transport/http/HttpResponse.h"

//---------------------------------------------------------------------------
#pragma package(smart_init)

//---------------------------------------------------------------------------
TUiMcpServer::TUiMcpServer()
{
    // Create MCP server
    FMcpServer = std::make_unique<Mcp::TMcpServer>("ClaBot-UI-MCP", "1.0.0");

    // Create HTTP server
    FHttpServer = std::make_unique<TIdHTTPServer>(nullptr);

    // Create transport with CORS config allowing localhost
    Mcp::Transport::TCorsConfig corsConfig;
    corsConfig.AllowLocalhost = true;
    FTransport = std::make_unique<Mcp::Transport::HttpTransport>(
        FHttpServer.get(), corsConfig);

    // Set up HTTP event handler
    FHttpServer->OnCommandGet = OnCommandGet;

    // Set up MCP request handler
    FTransport->SetRequestHandler(
        [this](Mcp::Transport::ITransportRequest &req,
               Mcp::Transport::ITransportResponse &resp) {
            std::string body = req.GetBody();
            std::string result = FMcpServer->HandleRequest(body);

            resp.SetStatus(200, "OK");
            resp.SetContentType("application/json; charset=utf-8");
            resp.SetBody(result);
        }
    );
}

//---------------------------------------------------------------------------
TUiMcpServer::~TUiMcpServer()
{
    Stop();
}

//---------------------------------------------------------------------------
void TUiMcpServer::Start(int port)
{
    if (IsRunning())
        return;

    FHttpServer->DefaultPort = port;
    FHttpServer->Bindings->Clear();

    TIdSocketHandle *binding = FHttpServer->Bindings->Add();
    binding->IP = "127.0.0.1";
    binding->Port = port;

    FTransport->Start();
}

//---------------------------------------------------------------------------
void TUiMcpServer::Stop()
{
    if (FTransport)
        FTransport->Stop();
}

//---------------------------------------------------------------------------
bool TUiMcpServer::IsRunning() const
{
    return FTransport && FTransport->IsRunning();
}

//---------------------------------------------------------------------------
void TUiMcpServer::RegisterUiTools(TfrmMain *form)
{
    if (FMcpServer && form)
        Mcp::Tools::RegisterUiTools(*FMcpServer, form);
}

//---------------------------------------------------------------------------
void __fastcall TUiMcpServer::OnCommandGet(TIdContext *AContext,
    TIdHTTPRequestInfo *ARequestInfo, TIdHTTPResponseInfo *AResponseInfo)
{
    if (FTransport)
        FTransport->HandleCommandGet(AContext, ARequestInfo, AResponseInfo);
}
//---------------------------------------------------------------------------
