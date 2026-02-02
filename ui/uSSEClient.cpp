//---------------------------------------------------------------------------
#pragma hdrstop
#include "uSSEClient.h"
#include <IdGlobal.hpp>
#include <IdURI.hpp>
#include <IdGlobalProtocols.hpp>
#include <System.SysUtils.hpp>
#include <memory>
//---------------------------------------------------------------------------
#pragma package(smart_init)

//---------------------------------------------------------------------------
// TSSEReaderThread
//---------------------------------------------------------------------------
__fastcall TSSEReaderThread::TSSEReaderThread(const UnicodeString &Url)
    : TThread(true), FUrl(Url), FOnEvent(nullptr), FOnError(nullptr),
      FOnConnected(nullptr), FOnDisconnected(nullptr)
{
    FreeOnTerminate = false;

    FHttp = new TIdHTTP(nullptr);
    FSSLHandler = new TIdSSLIOHandlerSocketOpenSSL(nullptr);

    FSSLHandler->SSLOptions->Method = sslvTLSv1_2;
    FSSLHandler->SSLOptions->Mode = sslmClient;
    FHttp->IOHandler = FSSLHandler;

    FHttp->Request->Accept = "text/event-stream";
    FHttp->Request->UserAgent = "ClaBot/1.0";
    FHttp->ConnectTimeout = 5000;
    FHttp->ReadTimeout = 0; // No timeout for SSE

    // Set UTF-8 encoding for proper handling of international characters
    FHttp->IOHandler->DefStringEncoding = IndyTextEncoding_UTF8();
}
//---------------------------------------------------------------------------
__fastcall TSSEReaderThread::~TSSEReaderThread()
{
    delete FHttp;
    delete FSSLHandler;
}
//---------------------------------------------------------------------------
void TSSEReaderThread::StopReading()
{
    Terminate();
    if (FHttp->Connected()) {
        FHttp->Disconnect();
    }
}
//---------------------------------------------------------------------------
void __fastcall TSSEReaderThread::Execute()
{
    UnicodeString buffer = "";

    try {
        // Connect to server
        FHttp->Request->Accept = "text/event-stream";
        FHttp->Request->CacheControl = "no-cache";

        // Parse URL for host/port/path
        TIdURI *uri = new TIdURI(FUrl);
        UnicodeString host = uri->Host;
        int port = uri->Port.IsEmpty() ? 80 : uri->Port.ToInt();
        UnicodeString path = uri->Path + uri->Document;
        if (path.IsEmpty()) path = "/";
        if (!uri->Params.IsEmpty()) path = path + "?" + uri->Params;
        delete uri;

        // Connect
        FHttp->IOHandler->Open();
        FHttp->IOHandler->Host = host;
        FHttp->IOHandler->Port = port;

        // Manually connect using IOHandler
        if (!FHttp->IOHandler->Connected()) {
            if (FSSLHandler) {
                FSSLHandler->Host = host;
                FSSLHandler->Port = port;
            }
            FHttp->IOHandler->Open();
        }

        // Send HTTP GET request manually
        UnicodeString request = "GET " + path + " HTTP/1.1\r\n";
        request = request + "Host: " + host + "\r\n";
        request = request + "Accept: text/event-stream\r\n";
        request = request + "Cache-Control: no-cache\r\n";
        request = request + "Connection: keep-alive\r\n";
        request = request + "\r\n";

        FHttp->IOHandler->Write(request);

        // Read HTTP response headers
        UnicodeString statusLine = FHttp->IOHandler->ReadLn();
        if (statusLine.Pos("200") == 0) {
            // Not a 200 OK response
            if (FOnError) {
                TThread::Synchronize(nullptr, [this, statusLine]() {
                    FOnError(this, "Server error: " + statusLine);
                });
            }
            return;
        }

        // Skip remaining headers until empty line
        UnicodeString headerLine;
        do {
            headerLine = FHttp->IOHandler->ReadLn();
        } while (!headerLine.IsEmpty() && !Terminated);

        // Notify connected
        if (FOnConnected) {
            TThread::Synchronize(nullptr, [this]() {
                FOnConnected(this);
            });
        }

        // Read SSE stream line by line
        while (!Terminated && FHttp->IOHandler->Connected()) {
            try {
                // Check for available data
                if (FHttp->IOHandler->InputBufferIsEmpty()) {
                    FHttp->IOHandler->CheckForDataOnSource(100);
                    if (FHttp->IOHandler->InputBufferIsEmpty()) {
                        Sleep(50);
                        continue;
                    }
                }

                // Read a line
                UnicodeString line = FHttp->IOHandler->ReadLn();

                if (!line.IsEmpty()) {
                    ProcessLine(line);
                }
            }
            catch (EIdReadTimeout &) {
                // Normal for SSE, continue
                continue;
            }
            catch (EIdConnClosedGracefully &) {
                break;
            }
        }
    }
    catch (Exception &e) {
        if (FOnError) {
            UnicodeString errMsg = e.Message;
            TThread::Synchronize(nullptr, [this, errMsg]() {
                FOnError(this, errMsg);
            });
        }
    }

    // Cleanup - notify disconnected
    if (FOnDisconnected) {
        TThread::Synchronize(nullptr, [this]() {
            FOnDisconnected(this);
        });
    }
}
//---------------------------------------------------------------------------
void TSSEReaderThread::ProcessLine(const UnicodeString &Line)
{
    // SSE format: "data: {json}"
    if (Line.Pos("data:") == 1) {
        UnicodeString jsonStr = Line.SubString(6, Line.Length() - 5).Trim();

        if (!jsonStr.IsEmpty() && FOnEvent) {
            std::string utf8Str = utf8(jsonStr);
            json eventObj = json::parse(utf8Str, nullptr, false);

            if (!eventObj.is_discarded() && eventObj.is_object()) {
                TThread::Synchronize(nullptr, [this, eventObj]() {
                    FOnEvent(this, eventObj);
                });
            }
        }
    }
}

//---------------------------------------------------------------------------
// TSSEClient
//---------------------------------------------------------------------------
TSSEClient::TSSEClient()
    : FThread(nullptr), FConnected(false), FOnEvent(nullptr),
      FOnError(nullptr), FOnConnected(nullptr), FOnDisconnected(nullptr)
{
}
//---------------------------------------------------------------------------
TSSEClient::~TSSEClient()
{
    Disconnect();
}
//---------------------------------------------------------------------------
void __fastcall TSSEClient::InternalOnConnected(TObject *Sender)
{
    FConnected = true;
    if (FOnConnected) FOnConnected(Sender);
}
//---------------------------------------------------------------------------
void __fastcall TSSEClient::InternalOnDisconnected(TObject *Sender)
{
    FConnected = false;
    if (FOnDisconnected) FOnDisconnected(Sender);
}
//---------------------------------------------------------------------------
void TSSEClient::Connect(const UnicodeString &Url)
{
    Disconnect();

    FThread = new TSSEReaderThread(Url);
    FThread->OnEvent = FOnEvent;
    FThread->OnError = FOnError;
    FThread->OnConnected = InternalOnConnected;
    FThread->OnDisconnected = InternalOnDisconnected;

    FThread->Start();
}
//---------------------------------------------------------------------------
void TSSEClient::Disconnect()
{
    if (FThread) {
        FThread->StopReading();
        FThread->WaitFor();
        delete FThread;
        FThread = nullptr;
    }
    FConnected = false;
}
//---------------------------------------------------------------------------
