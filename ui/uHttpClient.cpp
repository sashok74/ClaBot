//---------------------------------------------------------------------------
#pragma hdrstop
#include "uHttpClient.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
//---------------------------------------------------------------------------
THttpClient::THttpClient()
{
    FHttp = new TIdHTTP(nullptr);
    FSSLHandler = new TIdSSLIOHandlerSocketOpenSSL(nullptr);

    FSSLHandler->SSLOptions->Method = sslvTLSv1_2;
    FSSLHandler->SSLOptions->Mode = sslmClient;
    FHttp->IOHandler = FSSLHandler;

    FHttp->Request->ContentType = "application/json; charset=utf-8";
    FHttp->Request->Accept = "application/json";
    FHttp->Request->UserAgent = "ClaBot/1.0";
    FHttp->ConnectTimeout = 5000;
    FHttp->ReadTimeout = 30000;
}
//---------------------------------------------------------------------------
THttpClient::~THttpClient()
{
    delete FHttp;
    delete FSSLHandler;
}
//---------------------------------------------------------------------------
json THttpClient::Get(const UnicodeString &Endpoint)
{
    UnicodeString url = FBaseUrl + Endpoint;
    UnicodeString response = FHttp->Get(url);

    if (response.IsEmpty()) {
        return json::object();
    }

    std::string utf8Response = utf8(response);
    return json::parse(utf8Response, nullptr, false);
}
//---------------------------------------------------------------------------
json THttpClient::Post(const UnicodeString &Endpoint, const json &Body)
{
    UnicodeString url = FBaseUrl + Endpoint;

    std::string bodyStr = Body.is_null() ? "{}" : Body.dump();
    UnicodeString bodyUni = u(bodyStr);

    TStringStream *requestStream = new TStringStream(bodyUni, TEncoding::UTF8, false);

    try {
        UnicodeString response = FHttp->Post(url, requestStream);
        delete requestStream;

        if (response.IsEmpty()) {
            return json::object();
        }

        std::string utf8Response = utf8(response);
        return json::parse(utf8Response, nullptr, false);
    }
    catch (...) {
        delete requestStream;
        throw;
    }
}
//---------------------------------------------------------------------------
json THttpClient::Delete(const UnicodeString &Endpoint)
{
    UnicodeString url = FBaseUrl + Endpoint;
    UnicodeString response = FHttp->Delete(url);

    if (response.IsEmpty()) {
        return json::object();
    }

    std::string utf8Response = utf8(response);
    return json::parse(utf8Response, nullptr, false);
}
//---------------------------------------------------------------------------
