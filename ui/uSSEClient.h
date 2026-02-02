//---------------------------------------------------------------------------
#ifndef uSSEClientH
#define uSSEClientH
//---------------------------------------------------------------------------
#include <System.Classes.hpp>
#include <IdHTTP.hpp>
#include <IdSSLOpenSSL.hpp>
#include <string>
#include "json.hpp"
#include "UcodeUtf8.h"

using json = nlohmann::json;
//---------------------------------------------------------------------------
// Event types
typedef void __fastcall (__closure *TSSEEventHandler)(TObject *Sender, const json &Event);
typedef void __fastcall (__closure *TSSEErrorHandler)(TObject *Sender, const UnicodeString &Error);
typedef void __fastcall (__closure *TSSENotifyHandler)(TObject *Sender);

//---------------------------------------------------------------------------
// SSE Reader Thread
class TSSEReaderThread : public TThread
{
private:
    UnicodeString FUrl;
    TIdHTTP *FHttp;
    TIdSSLIOHandlerSocketOpenSSL *FSSLHandler;
    TSSEEventHandler FOnEvent;
    TSSEErrorHandler FOnError;
    TSSENotifyHandler FOnConnected;
    TSSENotifyHandler FOnDisconnected;

protected:
    void __fastcall Execute();
    void ProcessLine(const UnicodeString &Line);

public:
    __fastcall TSSEReaderThread(const UnicodeString &Url);
    __fastcall ~TSSEReaderThread();

    void StopReading();

    __property TSSEEventHandler OnEvent = { read = FOnEvent, write = FOnEvent };
    __property TSSEErrorHandler OnError = { read = FOnError, write = FOnError };
    __property TSSENotifyHandler OnConnected = { read = FOnConnected, write = FOnConnected };
    __property TSSENotifyHandler OnDisconnected = { read = FOnDisconnected, write = FOnDisconnected };
};

//---------------------------------------------------------------------------
// SSE Client wrapper
class TSSEClient
{
private:
    TSSEReaderThread *FThread;
    TSSEEventHandler FOnEvent;
    TSSEErrorHandler FOnError;
    TSSENotifyHandler FOnConnected;
    TSSENotifyHandler FOnDisconnected;
    bool FConnected;

    // Internal handlers for thread callbacks
    void __fastcall InternalOnConnected(TObject *Sender);
    void __fastcall InternalOnDisconnected(TObject *Sender);

public:
    TSSEClient();
    ~TSSEClient();

    void Connect(const UnicodeString &Url);
    void Disconnect();

    __property bool Connected = { read = FConnected };
    __property TSSEEventHandler OnEvent = { read = FOnEvent, write = FOnEvent };
    __property TSSEErrorHandler OnError = { read = FOnError, write = FOnError };
    __property TSSENotifyHandler OnConnected = { read = FOnConnected, write = FOnConnected };
    __property TSSENotifyHandler OnDisconnected = { read = FOnDisconnected, write = FOnDisconnected };
};
//---------------------------------------------------------------------------
#endif
