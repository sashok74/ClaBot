#ifdef _WIN32

#include <windows.h>

#include <cassert>
#include <string>
//==============================================================================
//==============================================================================

template <class ch>
inline size_t str_length(const ch* text) noexcept {
  assert(text);
  const auto* cur = text;
  while (*cur != 0) ++cur;
  return static_cast<size_t>(cur - text);
}

//==============================================================================
//==============================================================================

namespace tools {
// Convert a wide Unicode string to an UTF8 string  (wchar_t ---> char)
namespace utf8 {
::std::string convert(const wchar_t* text, const size_t len) {
  assert(text);
  assert(::str_length(text) >= len);

  if (text[0] == 0) return ::std::string();

  const int size_needed = ::WideCharToMultiByte(
      CP_UTF8, 0, text, static_cast<int>(len), NULL, 0, NULL, NULL);

  assert(size_needed > 0);
  const size_t cnt = static_cast<size_t>(size_needed);
  ::std::string strTo(cnt, 0);

  ::WideCharToMultiByte(CP_UTF8, 0, text, static_cast<int>(len), &strTo[0],
                        size_needed, NULL, NULL);
  return strTo;
}

::std::string convert(const wchar_t* text) {
  assert(text);
  if (text[0] == 0) return ::std::string();
  const size_t len = ::str_length(text);
  return ::tools::utf8::convert(text, len);
}

::std::string convert(const ::std::wstring& text) {
  return ::tools::utf8::convert(text.c_str(), text.length());
}

}  // namespace utf8

// Convert an UTF8 string to a wide Unicode String  (char ---> wchar_t)
namespace utf8 {
::std::wstring convert(const char* text, const size_t len) {
  assert(text);
  assert(::str_length(text) >= len);

  if (text[0] == 0) return std::wstring();

  int size_needed =
      ::MultiByteToWideChar(CP_UTF8, 0, text, static_cast<int>(len), NULL, 0);

  assert(size_needed > 0);
  const size_t cnt = static_cast<size_t>(size_needed);
  ::std::wstring wstrTo(cnt, 0);

  ::MultiByteToWideChar(CP_UTF8, 0, text, static_cast<int>(len), &wstrTo[0],
                        size_needed);
  return wstrTo;
}

::std::wstring convert(const char* text) {
  assert(text);
  if (text[0] == 0) return ::std::wstring();
  const size_t len = ::str_length(text);
  return ::tools::utf8::convert(text, len);
}

::std::wstring convert(const ::std::string& text) {
  return ::tools::utf8::convert(text.c_str(), text.length());
}

}  // namespace utf8

}  // namespace tools

//==============================================================================
//==============================================================================

#endif  // _WIN32

