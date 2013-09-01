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

#ifndef PXC_UTIL_HPP
#define PXC_UTIL_HPP

#include <stdio.h>
#include <vector>
#include <string>
#include <deque>
#include <set>
#include "auto_free.hpp"
#include "auto_fp.hpp"

#include <dlfcn.h>
#include <unistd.h>

namespace pxc {

typedef std::set<std::string> strset;
typedef std::deque<std::string> strlist;
typedef std::deque< std::vector<std::string> > strarrlist;

struct tmpfile_guard {
  tmpfile_guard(const std::string fn) : fn(fn) { }
  ~tmpfile_guard() {
    unlink(fn.c_str());
  }
private:
  std::string fn;
  tmpfile_guard(const tmpfile_guard&);
  tmpfile_guard& operator =(const tmpfile_guard&);
};

struct auto_dl {
  auto_dl(void *ptr) : ptr(ptr) { }
  ~auto_dl() { reset(); }
  void *get() const { return ptr; }
  void reset() {
    if (ptr != 0) {
      dlclose(ptr);
    }
    ptr = 0;
  }
private:
  auto_dl(const auto_dl&);
  auto_dl& operator =(const auto_dl&);
  void *ptr;
};

std::string get_canonical_path(const std::string& x);
std::string read_file_content(const std::string& fn, bool err_thr);
std::string ulong_to_string_hexadecimal(unsigned long long v);
unsigned long long ulong_from_string_hexadecimal(const std::string& s);
unsigned char uchar_from_hexadecimal(int c);
std::string from_hexadecimal(const std::string& str);
int uchar_to_hexadecimal(unsigned char c);
std::string to_hexadecimal(const std::string& str);
bool is_valid_hexadecimal_char(char ch);
std::string unescape_c_str_literal(const char *str, bool& success_r);
std::string escape_c_str_literal(const std::string& s);
void parse_string_as_strarrlist(const std::string& data, strarrlist& r);
void split_string(const std::string& str, char delim, strlist& r);
void split_string_space(const std::string& str, strlist& r);
std::string trim_space(const std::string& str);
std::string join_string(const strlist& sl);
long long string_to_long(const std::string& s);
unsigned long long string_to_ulong(const std::string& s);
std::string long_to_string(long long v);
std::string ulong_to_string(unsigned long long v);

bool file_exist(const std::string& fn);
void mkdir_hier(const std::string& d);
void write_file_content(const std::string& fn, const std::string& data);
int popen_cmd(const std::string& cmd, std::string& out_r);
void unlink_recursive(const std::string& fn);
void unlink_if(const std::string& fn);
void rmdir_if(const std::string& fn);
void copy_file(const std::string& ffrom, const std::string& fto);

std::string get_home_directory();
std::string escape_hex_controls(const std::string& str);
std::string unescape_hex_controls(const std::string& str);
std::string escape_hex_non_alnum(const std::string& str);
std::string unescape_hex_non_alnum(const std::string& str);
std::string escape_hex_filename(const std::string& str);

double gettimeofday_double();

};

#endif

