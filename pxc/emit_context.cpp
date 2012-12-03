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

#include <stdarg.h>
#include "emit_context.hpp"
#include "expr_impl.hpp"

namespace pxc {

emit_context::emit_context(const std::string& dest_filename)
  : fp(fopen(dest_filename.c_str(), "w")), cur_indent(0), cur_ns_extc(false)
{
  if (fp.get() == 0) {
    arena_error_throw(0, "-:0: failed to open %s: errno=%d\n",
      dest_filename.c_str(), errno);
  }
}

void emit_context::add_indent(int v)
{
  cur_indent += v;
}

void emit_context::indent(char ch)
{
  for (int i = 0; i < cur_indent; ++i) {
    fputc(' ', fp.get());
    // fputc(ch, fp.get());
  }
}

void emit_context::start_ns()
{
  if (cur_ns.empty()) {
    this->puts("namespace $n {\n");
  } else {
    size_t pos = 0;
    while (true) {
      std::string s;
      size_t delim = cur_ns.find("::", pos);
      if (delim != cur_ns.npos) {
	s = cur_ns.substr(pos, delim - pos);
	if (cur_ns_extc) {
	  this->printf("namespace %s { ", s.c_str());
	} else {
	  this->printf("namespace %s$n { ", s.c_str());
	}
	pos = delim + 2;
      } else {
	s = cur_ns.substr(pos);
	if (cur_ns_extc) {
	  this->printf("namespace %s { ", s.c_str());
	} else {
	  this->printf("namespace %s$n { ", s.c_str());
	}
	break;
      }
    }
    this->puts("\n");
  }
}

void emit_context::finish_ns()
{
  if (cur_ns.empty()) {
    this->puts("}; /* namespace */\n");
  } else {
    size_t pos = 0;
    while (true) {
      size_t delim = cur_ns.find("::", pos);
      if (delim != cur_ns.npos) {
	this->puts("};");
	pos = delim + 2;
      } else {
	this->puts("};");
	break;
      }
    }
    this->printf(" /* namespace %s */\n", cur_ns.c_str());
  }
}

void emit_context::set_ns(const std::string& ns, bool ns_extc)
{
  if (ns == cur_ns && cur_ns_extc == ns_extc) {
    return;
  }
  finish_ns();
  cur_ns = ns;
  cur_ns_extc = ns_extc;
  start_ns();
  this->indent('n');
}

void emit_context::set_file_line(const expr_i *e)
{
  #if 0
  this->indent('f');
  this->printf("#line %d \"%s\"\n", e->line, e->fname);
  #endif
}

void emit_context::set_stmt_context(const emit_context_stmt& s)
{
// if (s.stmt) { fprintf(stderr, "set_stmt_context %p %s\n", s.stmt, s.stmt->dump(0).c_str()); }
  sct = s;
}

void emit_context::reset_stmt_context()	
{
// if (sct.stmt) { fprintf(stderr, "reset_stmt_context %p\n", sct.stmt, sct.stmt->dump(0).c_str()); }
  sct = emit_context_stmt();
}

const emit_context_stmt& emit_context::get_stmt_context() const
{
  return sct;
}

void emit_context::printf(const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  vfprintf(fp.get(), format, ap);
  va_end(ap);
}

void emit_context::puts(const char *str)
{
  fputs(str, fp.get());
}

void emit_context::puts(const std::string& str)
{
  fputs(str.c_str(), fp.get());
}

std::string to_c_ns(const std::string& ns)
{
  size_t pos = 0;
  std::string r = "::";
  while (true) {
    size_t delim = ns.find("::", pos);
    if (delim != ns.npos) {
      r += ns.substr(pos, delim - pos) + "$n::";
      pos = delim + 2;
    } else {
      r += ns.substr(pos) + "$n";
      break;
    }
  }
  return r;
}

emit_expr_info& emit_context::get_expr_info(const expr_i *key)
{
  return eeimap[key];
}

};

