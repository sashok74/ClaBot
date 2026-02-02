//---------------------------------------------------------------------------
#ifndef uHttpClientH
#define uHttpClientH
//---------------------------------------------------------------------------
#include <System.Classes.hpp>
#include <IdHTTP.hpp>
#include <IdSSLOpenSSL.hpp>
#include <string>
#include "json.hpp"
#include "UcodeUtf8.h"

using json = nlohmann::json;
//---------------------------------------------------------------------------
class THttpClient
{
private:
    TIdHTTP *FHttp;
    TIdSSLIOHandlerSocketOpenSSL *FSSLHandler;
    UnicodeString FBaseUrl;

public:
    THttpClient();
    ~THttpClient();

    // HTTP methods returning JSON
    json Get(const UnicodeString &Endpoint);
    json Post(const UnicodeString &Endpoint, const json &Body);
    json Delete(const UnicodeString &Endpoint);

    // Properties
    __property UnicodeString BaseUrl = { read = FBaseUrl, write = FBaseUrl };
};
//---------------------------------------------------------------------------
#endif
