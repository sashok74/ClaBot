//---------------------------------------------------------------------------

#pragma hdrstop

#include "UcodeUtf8.h"
//---------------------------------------------------------------------------

// encoding function
std::string utf8(UnicodeString u) {
  std::string s = X::convert(u.w_str());
  return s;
}

UnicodeString u(std::string s) {
  std::wstring u = X::convert(s);
  return UnicodeString(u.c_str(), u.length());
};

#pragma package(smart_init)
