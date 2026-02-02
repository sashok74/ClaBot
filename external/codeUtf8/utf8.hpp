// utf8.hpp

#pragma once
#ifndef dTOOLS_UTF8_USED_
#define dTOOLS_UTF8_USED_ 1

#include <cassert>
#include <string>
#include <type_traits>

//==============================================================================
//==============================================================================
namespace tools {
namespace utf8 {
// Convert a wide Unicode string to an UTF8 string
::std::string convert(const wchar_t* text, const size_t len);
::std::string convert(const wchar_t* text);
::std::string convert(const ::std::wstring& text);

// Convert an UTF8 string to a wide Unicode String
std::wstring convert(const char* text, const size_t len);
std::wstring convert(const char* text);
std::wstring convert(const ::std::string& text);

// char ------------> std::string
// wchar_t ---------> std::wstring
template <class s>
auto convert(const s& src) {
  const auto* p = &src[0];
  assert(p);
  return ::tools::utf8::convert(p);
}

template <class ch, class s>
decltype(auto) convert_to(s&& src);

}  // namespace utf8

}  // namespace tools

//==============================================================================
//=== dfor_lvalue ==============================================================
#ifndef dTOOLS_FOR_LVALUE_USED_
#define dTOOLS_FOR_LVALUE_USED_
namespace tools {
#define dfor_lvalue(t) ::tools::for_lvalue<t>* = nullptr
template <class t, class ret = void>
using for_lvalue =
    ::std::enable_if_t< ::std::is_lvalue_reference<t>::value, ret>;

#define dfor_not_lvalue(t) ::tools::for_not_lvalue<t>* = nullptr
template <class t, class ret = void>
using for_not_lvalue =
    ::std::enable_if_t<!::std::is_lvalue_reference<t>::value, ret>;

}  // namespace tools
#endif  // !dTOOLS_FOR_LVALUE_USED_

//==============================================================================
//==============================================================================
namespace tools {
namespace utf8 {
namespace detail {
template <bool>
struct converter {
  template <class s, dfor_lvalue(s&&)>
  static decltype(auto) convert(s&& src) noexcept {
    return ::std::forward<s>(src);
  }

  template <class s, dfor_not_lvalue(s&&)>
  static auto convert(s&& src) noexcept {
    return src;
  }
};

template <>
struct converter<false> {
  template <class s>
  static auto convert(const s& src) {
    return ::tools::utf8::convert(src);
  }
};

}  // namespace detail

template <class ch, class s>
decltype(auto) convert_to(s&& src) {
  const auto* p = &src[0];
  assert(p);
  (void)p;

  using x = ::std::remove_pointer_t<decltype(p)>;
  using z = ::std::remove_cv_t<x>;
  enum { v = ::std::is_same<z, ch>::value };
  using conv_t = ::tools::utf8::detail::converter<v>;
  return conv_t::convert(::std::forward<s>(src));
}

}  // namespace utf8

}  // namespace tools

//==============================================================================
//==============================================================================

#endif  // !dTOOLS_UTF8_USED_

