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

struct expr_stmts;

struct emit_context_stmt {
  expr_stmts *stmt;
  bool is_blockcond : 1;
  bool sep_blockscope_var : 1;
  bool is_struct_or_global_block : 1;
  emit_context_stmt() : stmt(0), is_blockcond(false),
    sep_blockscope_var(false), is_struct_or_global_block(false) { }
};

struct emit_expr_info {
  bool defset_sep : 1;
  emit_expr_info() : defset_sep(false) { }
};

struct emit_context {
  emit_context(const std::string& dest_filename);
  void start_ns();
  void finish_ns();
  void add_indent(int v);
  void indent(char ch);
  void set_ns(const std::string& ns, bool ns_extc = false);
  void set_file_line(const expr_i *e);
  void set_stmt_context(const emit_context_stmt& sct);
  const emit_context_stmt& get_stmt_context() const;
  void reset_stmt_context();
  void printf(const char *format, ...) __attribute__((format (printf, 2, 3)));
  void puts(const char *str);
  void puts(const std::string& str);
  emit_expr_info& get_expr_info(const expr_i *key);
private:
  auto_fp fp;
  int cur_indent;
  std::string cur_ns;
  bool cur_ns_extc;
  std::string dest_filename;
  emit_context_stmt sct;
  std::map<const expr_i *, emit_expr_info> eeimap;
private:
  emit_context(const emit_context&);
  emit_context& operator =(const emit_context&);
};

struct stmt_context_scoped {
  stmt_context_scoped(emit_context& em, const emit_context_stmt& sct)
    : em(em) {
    prev = em.get_stmt_context();
    em.set_stmt_context(sct);
  }
  ~stmt_context_scoped() {
    em.set_stmt_context(prev);
  }
private:
  stmt_context_scoped(const stmt_context_scoped&);
  stmt_context_scoped& operator =(const stmt_context_scoped&);
private:
  emit_context_stmt prev;
  emit_context& em;
};

std::string to_c_ns(const std::string& ns);

};

#endif

