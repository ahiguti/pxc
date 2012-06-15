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

#ifndef PXC_EMIT_CONTEXT_HPP
#define PXC_EMIT_CONTEXT_HPP

#include "expr.hpp"
#include "auto_fp.hpp"

namespace pxc {

struct emit_context {
  emit_context(const std::string& dest_filename);
  void start_ns();
  void finish_ns();
  void add_indent(int v);
  void indent(char ch);
  void set_ns(const std::string& ns);
  void set_file_line(const expr_i *e);
  void printf(const char *format, ...) __attribute__((format (printf, 2, 3)));
  void puts(const char *str);
  void puts(const std::string& str);
private:
  auto_fp fp;
  int cur_indent;
  std::string cur_ns;
  std::string dest_filename;
private:
  emit_context(const emit_context&);
  emit_context& operator =(const emit_context&);
};

std::string to_c_ns(const std::string& ns);

};

#endif

