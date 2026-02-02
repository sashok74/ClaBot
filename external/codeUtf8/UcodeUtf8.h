#ifndef UcodeUtf8H
#define UcodeUtf8H

#include <System.Classes.hpp>
#include <string>

#include "utf8.hpp"
namespace X = ::tools::utf8;

// encoding function
std::string utf8(UnicodeString u);

UnicodeString u(std::string s);

#endif
