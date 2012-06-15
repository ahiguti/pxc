/*
 *  This file is part of PXC, the p extension compiler.
 *  Copyright (C) 2006 A.Higuchi. All rights reserved.
 *  See COPYRIGHT.txt for details.
 *
 *  This file is part of ASE, the abstract script engines.
 *  Copyright (C) 2006 A.Higuchi. All rights reserved.
 *  See COPYRIGHT.txt for details.
 */


// vim:sw=2:ts=8:ai

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pxcrt/builtin.hpp"
#include "builtin_impl.hpp"
#include "expr_impl.hpp"

namespace pxcrt {

vector<string> pxc_argv;

string void_to_string(void)
{
  return string();
}

string unit_to_string(unit x)
{
  return string();
}

string bool_to_string(bool v)
{
  static const string str_t("true");
  static const string str_f("false");
  return v ? str_t : str_f;
}

string uchar_to_string(unsigned char v)
{
  char buf[4];
  const int len = snprintf(buf, sizeof(buf), "%hhu", v);
  return string(buf, buf + len);
}

string char_to_string(char v)
{
  char buf[4];
  const int len = snprintf(buf, sizeof(buf), "%hhd", v);
  return string(buf, buf + len);
}

string ushort_to_string(unsigned short v)
{
  char buf[8];
  const int len = snprintf(buf, sizeof(buf), "%hu", v);
  return string(buf, buf + len);
}

string short_to_string(short v)
{
  char buf[8];
  const int len = snprintf(buf, sizeof(buf), "%hd", v);
  return string(buf, buf + len);
}

string uint_to_string(unsigned int v)
{
  char buf[16];
  const int len = snprintf(buf, sizeof(buf), "%u", v);
  return string(buf, buf + len);
}

string int_to_string(int v)
{
  char buf[16];
  const int len = snprintf(buf, sizeof(buf), "%d", v);
  return string(buf, buf + len);
}

string ulong_to_string(unsigned long long v)
{
  char buf[32];
  const int len = snprintf(buf, sizeof(buf), "%llu", v);
  return string(buf, buf + len);
}

string long_to_string(long long v)
{
  char buf[32];
  const int len = snprintf(buf, sizeof(buf), "%lld", v);
  return string(buf, buf + len);
}

string size_t_to_string(size_t v)
{
  char buf[32];
  const int len = snprintf(buf, sizeof(buf), "%zu", v);
  return string(buf, buf + len);
}

string double_to_string(double v)
{
  char buf[32];
  const int len = snprintf(buf, sizeof(buf), "%f", v);
  return string(buf, buf + len);
}

string float_to_string(float v)
{
  char buf[32];
  const int len = snprintf(buf, sizeof(buf), "%f", double(v));
  return string(buf, buf + len);
}

void string_to_void(const string& v)
{
}

bool string_to_bool(const string& v)
{
  const string::size_type len = v.size();
  if (len == 4 && memcmp(v.data(), "true", 4) == 0) {
    return true;
  }
  if (len == 5 && memcmp(v.data(), "false", 5) == 0) {
    return false;
  }
  if (len == 1 && v.data()[0] == '0') {
    return false;
  }
  return true;
}

unsigned char string_to_uchar(const string& v)
{
  return strtoul(v.c_str(), 0, 10);
}

char string_to_char(const string& v)
{
  return strtol(v.c_str(), 0, 10);
}

unsigned short string_to_ushort(const string& v)
{
  return strtoul(v.c_str(), 0, 10);
}

short string_to_short(const string& v)
{
  return strtol(v.c_str(), 0, 10);
}

unsigned int string_to_uint(const string& v)
{
  return strtoul(v.c_str(), 0, 10);
}

int string_to_int(const string& v)
{
  return strtol(v.c_str(), 0, 10);
}

unsigned long long string_to_ulong(const string& v)
{
  return strtoull(v.c_str(), 0, 10);
}

long long string_to_long(const string& v)
{
  return strtoll(v.c_str(), 0, 10);
}

size_t string_to_size_t(const string& v)
{
  return strtoull(v.c_str(), 0, 10);
}

double string_to_double(const string& v)
{
  return atof(v.c_str());
}

float string_to_float(const string& v)
{
  return atof(v.c_str());
}

#if 0
static inline string exception_message(const string& m)
{
  string mess = m + "\n";
  void *bt_buffer[100];
  int nptrs = backtrace(bt_buffer, 100);
  char **strs = backtrace_symbols(bt_buffer, nptrs);
  try {
    if (strs != 0) {
      for (int j = 0; j < nptrs; ++j) {
	mess += string(strs[j]) + "\n";
      }
    }
  } catch (...) { }
  free(strs);
  return mess;
}
#endif

invalid_index::invalid_index() : std::logic_error("invalid_index") { }
invalid_field::invalid_field() : std::logic_error("invalid_field") { }
null_dereference::null_dereference() : std::logic_error("null_dereference") { }
would_invalidate::would_invalidate() : std::logic_error("would_invalidate") { }

void throw_invalid_index() { throw invalid_index(); }
void throw_invalid_field() { throw invalid_field(); }
void throw_null_dereference() { throw null_dereference(); }
void throw_would_invalidate() { throw would_invalidate(); }

}; // namespace pxcrt

