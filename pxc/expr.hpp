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

#ifndef PXC_EXPR_HPP
#define PXC_EXPR_HPP

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <list>
#include <map>
#include <set>
#include <string>
#include <errno.h>

namespace pxc {

enum attribute_e {
  attribute_unknown        = 0,
  attribute_private        = 1,
  attribute_public         = 2,
  attribute_3              = 3,
  attribute_threaded       = 4,
  attribute_5              = 5,
  attribute_6              = 6,
  attribute_7              = 7,
  attribute_multithr       = 8,
  attribute_9              = 9,
  attribute_10             = 10,
  attribute_11             = 11,
  attribute_12             = 12,
  attribute_13             = 13,
  attribute_14             = 14,
  attribute_15             = 15,
};

struct expr_i;

expr_i *expr_te_new(const char *fn, int line, expr_i *nssym, expr_i *arg);
expr_i *expr_te_anonfunc_new(const char *fn, int line, expr_i *fdef);
expr_i *expr_telist_new(const char *fn, int line, expr_i *head, expr_i *rest);
expr_i *expr_inline_c_new(const char *fn, int lin, const char *posstr,
  const char *cstr, bool declonly);
expr_i *expr_ns_new(const char *fn, int line, expr_i *nssym, bool import,
  bool pub, const char *nsalias);
expr_i *expr_int_literal_new(const char *fn, int line, const char *str,
  bool is_unsigned);
expr_i *expr_float_literal_new(const char *fn, int line, const char *str);
expr_i *expr_bool_literal_new(const char *fn, int line, bool v);
expr_i *expr_str_literal_new(const char *fn, int line, const char *str);
expr_i *expr_symbol_new(const char *fn, int line, expr_i *nssym);
expr_i *expr_nssym_new(const char *fn, int line, expr_i *prefix,
  const char *sym);
expr_i *expr_var_new(const char *fn, int line, const char *sym, expr_i *typ,
  attribute_e visi);
expr_i *expr_extval_new(const char *fn, int line, const char *sym,
  expr_i *typ, const char *cname, attribute_e visi);
expr_i *expr_argdecls_new(const char *fn, int line, const char *sym,
  expr_i *typ, bool byref_flag, expr_i *rest);
expr_i *expr_stmts_new(const char *fn, int line, expr_i *head, expr_i *rest);
expr_i *expr_block_new(const char *fn, int line, expr_i *tparams,
  expr_i *inherit, expr_i *argdecls, expr_i *rettyp, expr_i *stmts);
expr_i *expr_op_new(const char *fn, int line, int op, expr_i *arg0,
  expr_i *arg1);
expr_i *expr_funccall_new(const char *fn, int line, expr_i *func, expr_i *arg);
expr_i *expr_if_new(const char *fn, int line, expr_i *cond, expr_i *b1,
  expr_i *b2, expr_i *rest);
expr_i *expr_while_new(const char *fn, int line, expr_i *cond, expr_i *block);
expr_i *expr_for_new(const char *fn, int line, expr_i *e0, expr_i *e1,
  expr_i *e2, expr_i *block);
expr_i *expr_feach_new(const char *fn, int line, expr_i *ce, expr_i *block);
expr_i *expr_fldfe_new(const char *fn, int line, const char *namesym,
  const char *fldsym, expr_i *te, expr_i *stmts);
expr_i *expr_foldfe_new(const char *fn, int line, const char *itersym,
  expr_i *valueste, const char *embedsym, expr_i *embedexpr,
  const char *foldop, expr_i *stmts);
expr_i *expr_special_new(const char *fn, int line, int tok, expr_i *arg);
expr_i *expr_funcdef_new(const char *fn, int line, const char *sym,
  const char *cname, bool is_const, expr_i *block, bool ext_decl,
  bool extc_decl, attribute_e visi);
expr_i *expr_typedef_new(const char *fn, int line, const char *sym,
  const char *cname, const char *category, bool is_pod,
  unsigned int num_tparams, attribute_e visi);
expr_i *expr_macrodef_new(const char *fn, int line, const char *sym,
  expr_i *tparams, expr_i *rhs, attribute_e visi);
expr_i *expr_inherit_new(const char *fn, int line, expr_i *nssym,
  expr_i *rest);
expr_i *expr_struct_new(const char *fn, int line, const char *sym,
  const char *cname, const char *category, expr_i *block, attribute_e visi);
expr_i *expr_variant_new(const char *fn, int line, const char *sym,
  expr_i *block, attribute_e visi);
expr_i *expr_interface_new(const char *fn, int line, const char *sym,
  expr_i *block, attribute_e visi);
expr_i *expr_try_new(const char *fn, int line, expr_i *tblock, expr_i *cblock,
  expr_i *rest);
expr_i *expr_tparams_new(const char *fn, int line, const char *sym,
  expr_i *rest);
expr_i *expr_te_local_chain_new(expr_i *te1, expr_i *te2);
expr_i *expr_metalist_new(expr_i *tl);

// TODO: move to parser.hpp
struct checksum_type {
  time_t timestamp;
  std::string md5sum;
  bool operator ==(const checksum_type& x) const {
    return timestamp == x.timestamp && md5sum == x.md5sum;
  }
  bool operator !=(const checksum_type& x) const {
    return !(*this == x);
  }
  checksum_type() : timestamp(0) { }
};

struct import_info {
  std::string ns;
  bool import_public;
  #if 0
  checksum_type saved_source_checksum;
  #endif
  import_info() : import_public(false) { }
};

struct imports_type {
  typedef std::list<import_info> deps_type;
  deps_type deps;
  std::string main_ns; /* set by arena_append_topval */
};

struct coptions {
  struct optvalues {
    typedef std::list<std::string> list_val_type;
    list_val_type list_val;
    typedef std::set<std::string> set_val_type;
    set_val_type set_val;
    void append_if(const std::string& x) {
      if (set_val.find(x) == set_val.end()) {
	list_val.push_back(x);
	set_val.insert(x);
      }
    }
    void append(const optvalues& x) {
      for (set_val_type::const_iterator i = x.set_val.begin();
	i != x.set_val.end(); ++i) {
	append_if(*i);
      }
    }
  };
  optvalues incdir, libdir, link, cflags, ldflags;
  void append(const coptions& x) {
    incdir.append(x.incdir);
    libdir.append(x.libdir);
    link.append(x.link);
    cflags.append(x.cflags);
    ldflags.append(x.ldflags);
  }
};

enum generate_main_e {
  generate_main_none,
  generate_main_dl,
  generate_main_exe,
};

void arena_init();
void arena_clear();
void arena_append_topval(const std::list<expr_i *>& topval, bool is_main,
  imports_type& imports_r);
void arena_compile(const std::string& dest_filename, coptions& copt_apnd,
  generate_main_e gmain);
char *arena_strdup(const char *str);
char *arena_dequote_strdup(const char *str);
char *arena_decode_inline_strdup(const char *str);
void arena_error_throw(const expr_i *e, const char *format, ...)
  __attribute__((format (printf, 2, 3)));
void arena_error_push(const expr_i *e, const char *format, ...)
  __attribute__((format (printf, 2, 3)));
void arena_error_throw_pushed();
std::string arena_get_ns_main_funcname(const std::string& ns);

};

#endif

