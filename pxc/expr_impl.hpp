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

#ifndef PXC_EXPR_IMPL_HPP
#define PXC_EXPR_IMPL_HPP

#include <stdexcept>
#include <stdarg.h>
#include <signal.h>
#include <assert.h>
#include <set>
#include <list>
#include <vector>
#include <map>

#include "emit_context.hpp"
#include "expr.hpp"
#include "pxc_y.hpp"
#include "auto_fp.hpp"
#include "symbol_table.hpp"
#include "term.hpp"

namespace pxc {

template <typename T1, typename T2> T1 *ptr_down_cast(T2 *v)
{
  if (v == 0) { return 0; }
  return &(dynamic_cast<T1&>(*v));
}

template <typename T> size_t elist_length(T *p)
{ return p == 0 ? 0 : elist_length(p->rest) + 1; }

template <typename T> size_t argdecls_length(T *p)
{
  return p == 0 ? 0 : argdecls_length(p->get_rest()) + 1;
}

struct expr_te;

typedef std::list<expr_i *> expr_arena_type;
typedef std::list<char *> str_arena_type;
typedef std::list<expr_i *> topvals_type;
typedef std::map<std::string, imports_type> loaded_namespaces_type;
typedef std::list<std::string> errors_type;
typedef std::map<std::string, term> tvmap_type;


struct builtins_type {
  term type_void;
  term type_unit;
  term type_bool;
  term type_uchar;
  term type_char;
  term type_ushort;
  term type_short;
  term type_uint;
  term type_int;
  term type_ulong;
  term type_long;
  term type_size_t;
  term type_float;
  term type_double;
  term type_string;
  term type_strlit;
  term type_slice;
  term type_cslice;
  term type_tpdummy;
};

struct type_attribute {
  int significant_bits_min; /* 8 for uchar, 7 for char */
  int significant_bits_max; /* ditto */
  bool is_numeric : 1;
  bool is_integral : 1;
  bool is_signed : 1;
  type_attribute() : significant_bits_min(0), significant_bits_max(0),
    is_numeric(false), is_integral(false), is_signed(false) { }
  type_attribute(int sbmin, int sbmax, bool is_numeric, bool is_integral,
    bool is_signed)
    : significant_bits_min(sbmin), significant_bits_max(sbmax),
      is_numeric(is_numeric), is_integral(is_integral), is_signed(is_signed)
      { }
};

enum term_tostr_sort {
  term_tostr_sort_humanreadable,
  term_tostr_sort_strict,
  term_tostr_sort_cname,
  term_tostr_sort_cname_tparam,
};

enum conversion_e {
  conversion_e_none,
  conversion_e_cast,
  #if 0
  conversion_e_to_string,
  conversion_e_from_string,
  #endif
  conversion_e_subtype_obj, /* foo to ifoo etc. converted value has lvalue. */
  conversion_e_subtype_ptr, /* ptr{foo} to cptr{foo} etc. */
  conversion_e_container_range, /* vector{foo} to range{foo} etc. */
  conversion_e_boxing,
};

enum call_trait_e {
  call_trait_e_raw_pointer, /* pass-by-value, eliminates incref/decref */
  call_trait_e_const_ref_nonconst_ref, /* pass-by-const-reference */
  call_trait_e_value, /* pass-by-value */
};

enum funccall_e {
  funccall_e_funccall,
  funccall_e_explicit_conversion,
  funccall_e_default_constructor,
  funccall_e_struct_constructor,
};

enum typefamily_e {
  typefamily_e_none,             /* unknown */
  typefamily_e_ptr,              /* shared pointer */
  typefamily_e_cptr,             /* shared pointer, const target */
  typefamily_e_iptr,             /* shared pointer, immutable target */
  typefamily_e_tptr,             /* multithread-shared pointer */
  typefamily_e_tcptr,            /* multithread-shared pointer, const target */
  typefamily_e_tiptr,            /* multithread-shared pointer, immutable tgt */
  typefamily_e_lockobject,       /* lock object (local) */
  typefamily_e_clockobject,      /* lock object, const reference (local) */
  typefamily_e_extint,           /* c-defined int/long etc */
  typefamily_e_extuint,          /* c-defined unsigned int/long etc, */
  typefamily_e_extenum,          /* c-defined enum */
  typefamily_e_extbitmask,       /* c-defined bitmask */
  typefamily_e_extfloat,         /* c-defined float/double etc, */
  typefamily_e_extnumeric,       /* c-defined other numeric types */
  typefamily_e_varray,           /* resizable array */
  typefamily_e_darray,           /* dynamically allocated array */
  typefamily_e_farray,           /* fixed size array, fixed at compile time */
  typefamily_e_slice,            /* array slice (local) */
  typefamily_e_cslice,           /* array slice, const elements (local) */
  typefamily_e_tree_map,         /* rb-tree map */
  typefamily_e_tree_map_range,   /* range on tree_map (local) */
  typefamily_e_tree_map_crange,  /* range on tree_map, const elements (local) */
  typefamily_e_linear,           /* noncopyable, nodefaultcon */
  typefamily_e_noncopyable,      /* noncopyable */
  typefamily_e_nodefault,        /* nodefaultcon */
  typefamily_e_nocascade,        /* defcon even when tparam's not */
};

struct variable_info {
  passby_e passby : 3; /* value/reference, constness */
  bool guard_elements : 1; /* guard from invalidating container elems */
  bool scope_block : 1; /* block scope or statement scope */
  variable_info () : passby(passby_e_const_reference),
    guard_elements(false), scope_block(false) { }
};

struct symbol_table;

struct expr_stmts;
struct expr_block;
struct expr_funcdef;
struct expr_nssym;
struct expr_telist;
struct expr_tparams;
struct expr_symbol;

struct eval_context;
typedef term (*builtin_strict_metafunc_t)(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos);
typedef term (*builtin_nonstrict_metafunc_t)(expr_i *texpr,
  const term_list_range& targs, bool targs_evaluated, eval_context& ectx,
  expr_i *pos);

struct template_info {
  typedef std::map<std::string, expr_block *> instances_type;
  expr_tparams *tparams;
  instances_type instances;
  expr_block *instances_backref;
  std::string self_key;
  bool template_descent : 1;
  bool instantiated : 1; 
  template_info() : tparams(0), instances_backref(0),
    template_descent(false),
    instantiated(false) { }
  bool has_tparams() const { return tparams != 0; }
  bool is_uninstantiated() const {
    return template_descent && !instantiated;
  }
};

typedef std::list<std::string> nsalias_entries;
typedef std::map<std::pair<std::string, std::string>, nsalias_entries>
  nsaliases_type;

struct symbol_common {
  symbol_common(expr_i *parent_expr)
    : parent_expr(parent_expr), upvalue_flag(false),
      symtbl_defined(0), arg_hidden_this(0), symbol_def(0) { }
  std::string uniqns;
  std::string injectns;
  expr_i *parent_expr;
  bool upvalue_flag : 1;
  std::string fullsym;
  std::string sym_prefix;
  symbol_table *symtbl_defined; // FIXME: remove
  expr_i *arg_hidden_this;
  std::string arg_hidden_this_ns;
public:
  expr_i *resolve_symdef(symbol_table *lookup = 0);
  void set_symdef(expr_i *e);
  const expr_i *get_symdef() const;
  expr_i *get_symdef_nochk() { return symbol_def; }
  term& resolve_evaluated();
  void set_evaluated(const term& t); /* used for type auto-matching */
  const term& get_evaluated() const;
  const term& get_evaluated_nochk() const;
private:
  expr_i *symbol_def;
  term evaluated;
};

struct expr_i {
  expr_i(const char *fn, int line);
  virtual ~expr_i() { }
  virtual expr_i *clone() const = 0;
  virtual expr_e get_esort() const = 0;
  virtual int get_num_children() const = 0;
  virtual int get_num_blockcond() const { return 0; }
  virtual expr_i *get_child(int i) = 0;
  virtual void set_child(int i, expr_i *e) = 0;
  virtual expr_block *get_template_block() { return 0; }
  virtual std::string dump(int indent) const = 0;
  virtual symbol_common *get_sdef() { return 0; }
  virtual term& resolve_texpr() { return type_of_this_expr; }
  virtual void set_texpr(const term& t);
  virtual const term& get_value_texpr() { abort(); }
  virtual void set_value_texpr(const term&) { abort(); }
  virtual void set_unique_namespace_one(const std::string& uniqns,
    const std::string& injectns) { }
  virtual std::string get_unique_namespace() const { return std::string(); }
  virtual attribute_e get_attribute() const { return attribute_unknown; }
  virtual void set_attribute(attribute_e a) { abort(); }
  virtual void define_const_symbols_one() { }
  virtual void define_vars_one(expr_stmts *stmt) { }
  virtual void check_type(symbol_table *lookup) = 0;
  virtual std::string emit_symbol_str() const { return std::string(); }
  virtual void emit_symbol(emit_context& em) const { }
  virtual bool has_expr_to_emit() const = 0;
  virtual bool is_expression() const = 0;
public:
  const term& get_texpr() const { return type_of_this_expr; }
  const term& get_conv_to() const { return type_conv_to; }
private:
  virtual void emit_value(emit_context& em) = 0;
  friend void fn_emit_value(emit_context& em, expr_i *e, bool expand_tempvar,
    bool var_rhs);
    // FIXME: remove?
  friend void emit_value_internal(emit_context& em, expr_i *e);
protected:
  expr_i(const expr_i&);
private:
  expr_i& operator =(const expr_i&);
public:
  const char *fname;
  const int line;
  term type_of_this_expr; /* TODO: remove? */
  conversion_e conv;
  term type_conv_to;
  expr_i *parent_expr;
  symbol_table *symtbl_lexical;
  int tempvar_id;
  variable_info tempvar_varinfo;
  bool generated_flag : 1; /* this expr is generated using expand */
};

struct expr_te : public expr_i {
  expr_te(const char *fn, int line, expr_i *nssym, expr_i *tlarg);
public:
  expr_i *clone() const;
  expr_e get_esort() const { return expr_e_te; }
  int get_num_children() const { return 2; }
  expr_i *get_child(int i);
  void set_child(int i, expr_i *e);
  void set_unique_namespace_one(const std::string& u, const std::string& i) {
    sdef.uniqns = u; sdef.injectns = i; }
  std::string get_unique_namespace() const { return sdef.uniqns; }
  void check_type(symbol_table *lookup);
  bool has_expr_to_emit() const { return true; }
  bool is_expression() const { return true; }
  void emit_value(emit_context& em);
  std::string dump(int indent) const;
  virtual symbol_common *get_sdef() { return &sdef; }
  const expr_i *get_symdef() const;
  expr_i *resolve_symdef(symbol_table *lookup);
  void set_symdef(expr_i *e) { sdef.set_symdef(e); }
  term& resolve_texpr();
public:
  expr_nssym *nssym;
  expr_telist *tlarg;
  symbol_common sdef;
};

struct expr_telist : public expr_i {
  expr_telist(const char *fn, int line, expr_i *head, expr_i *rest);
public:
  expr_i *clone() const { return new expr_telist(*this); }
  expr_e get_esort() const { return expr_e_telist; }
  int get_num_children() const { return 2; }
  expr_i *get_child(int i) {
    if (i == 0) { return head; }
    else if (i == 1) { return rest; }
    return 0;
  }
  void set_child(int i, expr_i *e) {
    if (i == 0) { head = e; }
    else if (i == 1) { rest = ptr_down_cast<expr_telist>(e); }
  }
  void check_type(symbol_table *lookup);
  bool has_expr_to_emit() const { return true; }
  bool is_expression() const { return true; }
  void emit_value(emit_context& em) { }
  std::string dump(int indent) const;
public:
  expr_i *head;
  expr_telist *rest;
};

struct expr_inline_c : public expr_i {
  expr_inline_c(const char *fn, int line, const char *posstr,
    const char *cstr, bool declonly, expr_i *val);
  expr_i *clone() const { return new expr_inline_c(*this); }
  expr_e get_esort() const { return expr_e_inline_c; }
  int get_num_children() const { return 1; }
  expr_i *get_child(int i) { return i == 0 ? value : 0; }
  void set_child(int i, expr_i *e) { if (i ==0) { value = e; } }
  void check_type(symbol_table *lookup);
  bool has_expr_to_emit() const { return false; }
  bool is_expression() const { return false; }
  void emit_value(emit_context& em);
  std::string dump(int indent) const;
public:
  const std::string posstr;
  const char *const cstr;
  int pos;
  bool declonly;
  expr_i *value; /* for pragma */
};

struct expr_ns : public expr_i {
  /* 'namespace' or 'import' */
  expr_ns(const char *fn, int line, expr_i *uniq_nssym, bool import, bool pub,
    const char *nsalias, expr_i *inject_nssym);
  expr_i *clone() const { return new expr_ns(*this); }
  expr_e get_esort() const { return expr_e_ns; }
  int get_num_children() const { return 2; }
  expr_i *get_child(int i) {
    if (i == 0) { return uniq_nssym; }
    else if (i == 1) { return inject_nssym; }
    return 0;
  }
  void set_child(int i, expr_i *e) {
    if (i == 0) { uniq_nssym = e; }
    if (i == 1) { inject_nssym = e; }
  }
  void set_unique_namespace_one(const std::string& u, const std::string& i);
  void check_type(symbol_table *lookup) { }
  bool has_expr_to_emit() const { return false; }
  bool is_expression() const { return false; }
  void emit_value(emit_context& em);
  std::string dump(int indent) const;
public:
  expr_i *uniq_nssym;
  const std::string uniq_nsstr;
  const bool import;
  const bool pub;
  const char *nsalias;
  expr_i *inject_nssym;
  const std::string inject_nsstr;
};

struct expr_int_literal : public expr_i {
  expr_int_literal(const char *fn, int line, const char *str,
    bool is_unsigned);
  expr_i *clone() const { return new expr_int_literal(*this); }
  expr_e get_esort() const { return expr_e_int_literal; }
  int get_num_children() const { return 0; }
  expr_i *get_child(int i) { return 0; }
  void set_child(int i, expr_i *e) { }
  unsigned long long get_unsigned() const;
  long long get_signed() const;
  term& resolve_texpr();
  void check_type(symbol_table *lookup) { }
  bool has_expr_to_emit() const { return true; }
  bool is_expression() const { return true; }
  void emit_value(emit_context& em);
  std::string dump(int indent) const;
public:
  const char *const str;
  bool is_unsigned;
};

struct expr_float_literal : public expr_i {
  expr_float_literal(const char *fn, int line, const char *str);
  expr_i *clone() const { return new expr_float_literal(*this); }
  expr_e get_esort() const { return expr_e_float_literal; }
  int get_num_children() const { return 0; }
  expr_i *get_child(int i) { return 0; }
  void set_child(int i, expr_i *e) { }
  double get_value() const;
  void check_type(symbol_table *lookup) { }
  bool has_expr_to_emit() const { return true; }
  bool is_expression() const { return true; }
  void emit_value(emit_context& em);
  std::string dump(int indent) const;
public:
  const char *const str;
};

struct expr_bool_literal : public expr_i {
  expr_bool_literal(const char *fn, int line, bool v);
  expr_i *clone() const { return new expr_bool_literal(*this); }
  expr_e get_esort() const { return expr_e_bool_literal; }
  int get_num_children() const { return 0; }
  expr_i *get_child(int i) { return 0; }
  void set_child(int i, expr_i *e) { }
  term& resolve_texpr();
  void check_type(symbol_table *lookup) { }
  bool has_expr_to_emit() const { return true; }
  bool is_expression() const { return true; }
  void emit_value(emit_context& em);
  std::string dump(int indent) const;
public:
  const int value;
};

struct expr_str_literal : public expr_i {
  expr_str_literal(const char *fn, int line, const char *str);
  expr_i *clone() const { return new expr_str_literal(*this); }
  expr_e get_esort() const { return expr_e_str_literal; }
  int get_num_children() const { return 0; }
  expr_i *get_child(int i) { return 0; }
  void set_child(int i, expr_i *e) { }
  term& resolve_texpr();
  void check_type(symbol_table *lookup) { }
  bool has_expr_to_emit() const { return true; }
  bool is_expression() const { return true; }
  void emit_value(emit_context& em);
  std::string dump(int indent) const;
public:
  const char *const raw_value; /* escaped */
  bool valid_flag;
  std::string value; /* not escaped */
};

struct expr_nssym : public expr_i {
  expr_nssym(const char *fn, int line, expr_i *prefix, const char *sym);
  expr_i *clone() const { return new expr_nssym(*this); }
  expr_e get_esort() const { return expr_e_nssym; }
  int get_num_children() const { return 1; }
  expr_i *get_child(int i) { return i == 0 ? prefix : 0; }
  void set_child(int i, expr_i *e) { if (i == 0) { prefix = e; } }
  void check_type(symbol_table *lookup) { }
  bool has_expr_to_emit() const { return true; }
  bool is_expression() const { return true; }
  void emit_value(emit_context& em) { }
  std::string dump(int indent) const;
public:
  expr_i *prefix;
  const char *const sym;
};

struct expr_symbol : public expr_i {
  expr_symbol(const char *fn, int line, expr_i *nssym);
  expr_i *clone() const;
  expr_e get_esort() const { return expr_e_symbol; }
  int get_num_children() const { return 1; }
  expr_i *get_child(int i) { return i == 0 ? nssym : 0; }
  void set_child(int i, expr_i *e) {
    if (i == 0) { nssym = ptr_down_cast<expr_nssym>(e); }
  }
  void set_unique_namespace_one(const std::string& u, const std::string& i) {
    sdef.uniqns = u; sdef.injectns = i; }
  std::string get_unique_namespace() const { return sdef.uniqns; }
  void check_type(symbol_table *lookup);
  bool has_expr_to_emit() const { return true; }
  bool is_expression() const { return true; }
  void emit_value(emit_context& em);
  std::string dump(int indent) const;
  virtual symbol_common *get_sdef() { return &sdef; }
  const expr_i *get_symdef() const;
  expr_i *resolve_symdef(symbol_table *lookup);
  void set_symdef(expr_i *e) { sdef.set_symdef(e); }
  term& resolve_texpr();
public:
  expr_nssym *nssym;
  symbol_common sdef;
};

struct expr_var : public expr_i {
  expr_var(const char *fn, int line, const char *sym, expr_i *type_uneval,
    passby_e passby, attribute_e attr, expr_i *rhs_ref);
  expr_i *clone() const;
  expr_e get_esort() const { return expr_e_var; }
  int get_num_children() const { return 1; }
  expr_i *get_child(int i) { return i == 0 ? type_uneval : 0; }
  void set_child(int i, expr_i *e) {
    if (i == 0) { type_uneval = ptr_down_cast<expr_te>(e); }
  }
  void set_unique_namespace_one(const std::string& u, const std::string& i)
    { uniqns = u; injectns = i; }
  std::string get_unique_namespace() const { return uniqns; }
  attribute_e get_attribute() const { return attr; }
  void set_attribute(attribute_e a) { attr = a; }
  term& resolve_texpr();
  void define_vars_one(expr_stmts *stmt);
  void check_type(symbol_table *lookup);
  bool has_expr_to_emit() const { return true; }
  bool is_expression() const { return true; }
  std::string emit_symbol_str() const;
  void emit_symbol(emit_context& em) const;
  void emit_value(emit_context& em);
  std::string dump(int indent) const;
public:
  const char *sym;
  expr_te *type_uneval;
  std::string uniqns;
  std::string injectns;
  variable_info varinfo;
  // passby_e passby; // FIXME: remove
  attribute_e attr;
  expr_i *rhs_ref;
};

struct expr_enumval : public expr_i {
  expr_enumval(const char *fn, int line, const char *sym, expr_i *type_uneval,
    const char *cname, expr_i *val, attribute_e attr, expr_i *rest);
  expr_i *clone() const; //  { return new expr_enumval(*this); }
  expr_e get_esort() const { return expr_e_enumval; }
  int get_num_children() const { return 3; }
  expr_i *get_child(int i) {
    if (i == 0) { return type_uneval; }
    else if (i == 1) { return value; }
    else if (i == 2) { return rest; }
    return 0;
  }
  void set_child(int i, expr_i *e) {
    if (i == 0) { type_uneval = ptr_down_cast<expr_te>(e); }
    if (i == 1) { value = e; }
    if (i == 2) { rest = ptr_down_cast<expr_enumval>(e); }
  }
  void set_unique_namespace_one(const std::string& u, const std::string& i)
    { uniqns = u; injectns = i; }
  std::string get_unique_namespace() const { return uniqns; }
  attribute_e get_attribute() const { return attr; }
  void set_attribute(attribute_e a) { attr = a; }
  term& resolve_texpr();
  void define_vars_one(expr_stmts *stmt);
  void check_type(symbol_table *lookup);
  bool has_expr_to_emit() const { return false; }
  bool is_expression() const { return false; }
  std::string emit_symbol_str() const;
  void emit_symbol(emit_context& em) const;
  // void emit_cdecl(emit_context& em, bool is_argdecl) const;
  void emit_value(emit_context& em);
  std::string dump(int indent) const;
public:
  const char *const sym;
  expr_te *type_uneval;
  const char *const cname;
  expr_i *value;
  attribute_e attr;
  expr_enumval *rest;
  std::string uniqns;
  std::string injectns;
};

#if 0
enum asgnstmt_e {
  asgnstmt_e_other,
  asgnstmt_e_stmtscope_tempvar,
  asgnstmt_e_blockscope_tempvar,
  asgnstmt_e_var_defset,
  asgnstmt_e_var_def,
};

struct asgnstmt {
  expr_i *top;
  asgnstmt_e astmt;
  int blockcond_id;
  asgnstmt() : top(0), astmt(asgnstmt_e_other), blockcond_id(-1) { }
  asgnstmt(expr_i *top, asgnstmt_e astmt, int bid)
    : top(top), astmt(astmt), blockcond_id(bid) { }
};

typedef std::list<asgnstmt> asgnstmt_list;
#endif

struct expr_stmts : public expr_i {
  expr_stmts(const char *fn, int line, expr_i *head, expr_i *rest);
  expr_i *clone() const { return new expr_stmts(*this); }
  expr_e get_esort() const { return expr_e_stmts; }
  int get_num_children() const { return 2; }
  expr_i *get_child(int i) { return i == 0 ? head : i == 1 ? rest : 0; }
  void set_child(int i, expr_i *e) {
    if (i == 0) { head = e; }
    else if (i == 1) { rest = ptr_down_cast<expr_stmts>(e); }
  }
  void set_rest(expr_stmts *v) { rest = v; }
  void check_type(symbol_table *lookup);
  bool has_expr_to_emit() const { return true; }
  bool is_expression() const { return false; }
  void emit_value(emit_context& em);
  bool emit_local_decl_fastinit(emit_context& em);
  void emit_local_decl(emit_context& em);
  std::string dump(int indent) const;
public:
  // asgnstmt_list asts;
  expr_i *head;
  expr_stmts *rest; /* can be modified */
};

struct expr_tparams : public expr_i {
  expr_tparams(const char *fn, int line, const char *sym, expr_i *rest);
  expr_tparams *clone() const { return new expr_tparams(*this); }
  expr_e get_esort() const { return expr_e_tparams; }
  int get_num_children() const { return 1; }
  expr_i *get_child(int i) {
    if (i == 0) { return rest; }
    return 0;
  }
  void set_child(int i, expr_i *e) {
    if (i == 0) { rest = ptr_down_cast<expr_tparams>(e); }
  }
  void define_const_symbols_one() {
    symtbl_lexical->define_name(sym, "", this, attribute_private, 0);
  }
  std::string dump(int indent) const;
  void check_type(symbol_table *lookup) { }
  bool has_expr_to_emit() const { return true; }
  bool is_expression() const { return true; }
  void emit_value(emit_context& em) { }
public:
  const char *sym;
  expr_tparams *rest;
  term param_def;
};

struct expr_argdecls : public expr_i {
  expr_argdecls(const char *fn, int line, const char *sym, expr_i *type_uneval,
    passby_e passby, expr_i *rest);
  expr_i *clone() const;
  expr_e get_esort() const { return expr_e_argdecls; }
  int get_num_children() const { return 2; }
  expr_i *get_child(int i) {
    if (i == 0) { return type_uneval; }
    else if (i == 1) { return rest; }
    return 0;
  }
  void set_child(int i, expr_i *e) {
    if (i == 0) { type_uneval = ptr_down_cast<expr_te>(e); }
    else if (i == 1) { rest = e; }
  }
  void set_unique_namespace_one(const std::string& u, const std::string& i)
    { uniqns = u; injectns = i; }
  std::string get_unique_namespace() const { return uniqns; }
  void define_vars_one(expr_stmts *stmt);
  term& resolve_texpr();
  void check_type(symbol_table *lookup);
  expr_argdecls *get_rest() const;
  bool has_expr_to_emit() const { return true; }
  bool is_expression() const { return false; }
  std::string emit_symbol_str() const;
  void emit_symbol(emit_context& em) const;
  void emit_value(emit_context& em);
  std::string dump(int indent) const;
public:
  const char *sym;
  std::string uniqns;
  std::string injectns;
  expr_te *type_uneval;
  passby_e passby;
  expr_i *rest;
};

struct expr_block : public expr_i {
  expr_block(const char *fn, int line, expr_i *tparams, expr_i *inherit,
    expr_i *argdecls, expr_i *rettype_uneval, passby_e ret_passby,
    expr_i *stmts);
  expr_block *clone() const;
  expr_e get_esort() const { return expr_e_block; }
  int get_num_children() const { return 5; }
  expr_i *get_child(int i) {
    if (i == 0) { return tinfo.tparams; }
    else if (i == 1) { return inherit; }
    else if (i == 2) { return argdecls; }
    else if (i == 3) { return rettype_uneval; }
    else if (i == 4) { return stmts; }
    return 0;
  }
  void set_child(int i, expr_i *e) {
    if (i == 0) { tinfo.tparams = ptr_down_cast<expr_tparams>(e); }
    else if (i == 1) { inherit = ptr_down_cast<expr_telist>(e); }
    else if (i == 2) { argdecls = e; }
    else if (i == 3) { rettype_uneval = ptr_down_cast<expr_te>(e); }
    else if (i == 4) { stmts = ptr_down_cast<expr_stmts>(e); }
  }
  void check_type(symbol_table *lookup);
  expr_argdecls *get_argdecls() const;
  bool is_expand_argdecls() const {
    return argdecls != 0 && argdecls->get_esort() == expr_e_expand;
  }
  bool has_expr_to_emit() const { return true; }
  bool is_expression() const { return false; }
  void emit_value(emit_context& em);
  void emit_value_nobrace(emit_context& em);
  void emit_local_decl(emit_context& em, bool is_funcbody);
  void emit_memberfunc_decl(emit_context& em, bool pure_virtual);
  std::string dump(int indent) const;
public:
  template_info tinfo;
  unsigned int block_id_ns;
  expr_telist *inherit;
  expr_i *argdecls; /* expr_argdecls or expr_expand */
  expr_te *rettype_uneval;
  passby_e ret_passby;
  expr_stmts *stmts;
  symbol_table symtbl;
  typedef std::vector<expr_i *> inherit_list_type;
  inherit_list_type inherit_transitive;
  bool compiled_flag : 1;
};

struct expr_op : public expr_i {
  expr_op(const char *fn, int line, int op, expr_i *arg0, expr_i *arg1);
  expr_i *clone() const; // { return new expr_op(*this); }
  expr_e get_esort() const { return expr_e_op; }
  int get_num_children() const { return 2; }
  expr_i *get_child(int i) { return i == 0 ? arg0 : i == 1 ? arg1 : 0; }
  void set_child(int i, expr_i *e) {
    if (i == 0) { arg0 = e; }
    else if (i == 1) { arg1 = e; }
  }
  void check_type(symbol_table *lookup);
  bool has_expr_to_emit() const { return true; }
  bool is_expression() const { return true; }
  void emit_value(emit_context& em);
  std::string dump(int indent) const;
public:
  const int op;
  expr_i *arg0, *arg1;
};

struct expr_funccall : public expr_i {
  expr_funccall(const char *fn, int line, expr_i *func, expr_i *arg);
  expr_i *clone() const; //  { return new expr_funccall(*this); }
  expr_e get_esort() const { return expr_e_funccall; }
  int get_num_children() const { return 2; }
  expr_i *get_child(int i) { return i == 0 ? func : i == 1 ? arg : 0; }
  void set_child(int i, expr_i *e) {
    if (i == 0) { func = e; }
    else if (i == 1) { arg = e; }
  }
  void check_type(symbol_table *lookup);
  bool has_expr_to_emit() const { return true; }
  bool is_expression() const { return true; }
  void emit_value(emit_context& em);
  std::string dump(int indent) const;
public:
  expr_i *func;
  expr_i *arg;
  funccall_e funccall_sort;
};

struct expr_special : public expr_i {
  expr_special(const char *fn, int line, int tok, expr_i *arg);
  expr_i *clone() const { return new expr_special(*this); }
  expr_e get_esort() const { return expr_e_special; }
  int get_num_children() const { return 1; }
  expr_i *get_child(int i) { return i == 0 ? arg : 0; }
  void set_child(int i, expr_i *e) {
    if (i == 0) { arg = e; }
  }
  const char *tok_str() const;
  void check_type(symbol_table *lookup);
  bool has_expr_to_emit() const { return true; }
  bool is_expression() const { return false; }
  void emit_value(emit_context& em);
  std::string dump(int indent) const;
public:
  const int tok;
  expr_i *arg;
};

struct expr_if : public expr_i {
  expr_if(const char *fn, int line, expr_i *cond, expr_i *b1, expr_i *b2,
    expr_i *rest);
  expr_i *clone() const { return new expr_if(*this); }
  expr_e get_esort() const { return expr_e_if; }
  int get_num_children() const { return 4; }
  int get_num_blockcond() const { return 1; }
  expr_i *get_child(int i) {
    if (i == 0) { return cond; }
    else if (i == 1) { return block1; }
    else if (i == 2) { return block2; }
    else if (i == 3) { return rest; }
    return 0;
  }
  void set_child(int i, expr_i *e) {
    if (i == 0) { cond = e; }
    else if (i == 1) { block1 = ptr_down_cast<expr_block>(e); }
    else if (i == 2) { block2 = ptr_down_cast<expr_block>(e); }
    else if (i == 3) { rest = ptr_down_cast<expr_if>(e); }
  }
  void check_type(symbol_table *lookup);
  bool has_expr_to_emit() const { return true; }
  bool is_expression() const { return false; }
  void emit_value(emit_context& em);
  std::string dump(int indent) const;
public:
  expr_i *cond;
  expr_block *block1; /* then */
  expr_block *block2; /* else */
  expr_if *rest;      /* else if */
  int cond_static;    /* -1: undetermined, 0: false, 1: true */
};

struct expr_while : public expr_i {
  expr_while(const char *fn, int line, expr_i *cond, expr_i *block);
  expr_i *clone() const { return new expr_while(*this); }
  expr_e get_esort() const { return expr_e_while; }
  int get_num_children() const { return 2; }
  int get_num_blockcond() const { return 1; }
  expr_i *get_child(int i) { return i == 0 ? cond : i == 1 ? block : 0; }
  void set_child(int i, expr_i *e) {
    if (i == 0) { cond = e; }
    else if (i == 1) { block = ptr_down_cast<expr_block>(e); }
  }
  void check_type(symbol_table *lookup);
  bool has_expr_to_emit() const { return true; }
  bool is_expression() const { return false; }
  void emit_value(emit_context& em);
  std::string dump(int indent) const;
public:
  expr_i *cond;
  expr_block *block;
};

struct expr_for : public expr_i {
  expr_for(const char *fn, int line, expr_i *e0, expr_i *e1, expr_i *e2,
    expr_i *block);
  expr_i *clone() const { return new expr_for(*this); }
  expr_e get_esort() const { return expr_e_for; }
  int get_num_children() const { return 4; }
  int get_num_blockcond() const { return 3; }
  expr_i *get_child(int i) {
    if (i == 0) { return e0; }
    else if (i == 1) { return e1; }
    else if (i == 2) { return e2; }
    else if (i == 3) { return block; }
    return 0;
  }
  void set_child(int i, expr_i *e) {
    if (i == 0) { e0 = e; }
    else if (i == 1) { e1 = e; }
    else if (i == 2) { e2 = e; }
    else if (i == 3) { block = ptr_down_cast<expr_block>(e); }
  }
  void check_type(symbol_table *lookup);
  bool has_expr_to_emit() const { return true; }
  bool is_expression() const { return false; }
  void emit_value(emit_context& em);
  std::string dump(int indent) const;
public:
  expr_i *e0;
  expr_i *e1;
  expr_i *e2;
  expr_block *block;
};

struct expr_forrange : public expr_i {
  expr_forrange(const char *fn, int line, expr_i *r0, expr_i *r1,
    expr_i *block);
  expr_i *clone() const { return new expr_forrange(*this); }
  expr_e get_esort() const { return expr_e_forrange; }
  int get_num_children() const { return 3; }
  int get_num_blockcond() const { return 1; }
  expr_i *get_child(int i) {
    if (i == 0) { return r0; }
    else if (i == 1) { return r1; }
    else if (i == 2) { return block; }
    return 0;
  }
  void set_child(int i, expr_i *e) {
    if (i == 0) { r0 = e; }
    else if (i == 1) { r1 = e; }
    else if (i == 2) { block = ptr_down_cast<expr_block>(e); }
  }
  void check_type(symbol_table *lookup);
  bool has_expr_to_emit() const { return true; }
  bool is_expression() const { return false; }
  void emit_value(emit_context& em);
  std::string dump(int indent) const;
public:
  expr_i *r0;
  expr_i *r1;
  expr_block *block;
};

struct expr_feach : public expr_i {
  expr_feach(const char *fn, int line, expr_i *ce, expr_i *block);
  expr_i *clone() const { return new expr_feach(*this); }
  expr_e get_esort() const { return expr_e_feach; }
  int get_num_children() const { return 2; }
  int get_num_blockcond() const { return 1; }
  expr_i *get_child(int i) {
    if (i == 0) { return ce; }
    else if (i == 1) { return block; }
    return 0;
  }
  void set_child(int i, expr_i *e) {
    if (i == 0) { ce = e; }
    else if (i == 1) { block = ptr_down_cast<expr_block>(e); }
  }
  void check_type(symbol_table *lookup);
  bool has_expr_to_emit() const { return true; }
  bool is_expression() const { return false; }
  void emit_value(emit_context& em);
  std::string dump(int indent) const;
public:
  expr_i *ce;
  expr_block *block;
};

struct expr_fldfe : public expr_i {
  expr_fldfe(const char *fn, int line, const char *namesym,
    const char *fldsym, const char *idxsym, expr_i *te, expr_i *stmts);
  expr_i *clone() const { return new expr_fldfe(*this); }
  expr_e get_esort() const { return expr_e_fldfe; }
  int get_num_children() const { return 2; }
  expr_i *get_child(int i) {
    if (i == 0) { return te; }
    if (i == 1) { return stmts; }
    return 0;
  }
  void set_child(int i, expr_i *e) {
    if (i == 0) { te = ptr_down_cast<expr_te>(e); }
    if (i == 1) { stmts = ptr_down_cast<expr_stmts>(e); }
  }
  void set_unique_namespace_one(const std::string& u, const std::string& i)
    { uniqns = u; injectns = i; }
  std::string get_unique_namespace() const { return uniqns; }
  void check_type(symbol_table *lookup);
  bool has_expr_to_emit() const { return true; }
  bool is_expression() const { return false; }
  void emit_value(emit_context& em);
  std::string dump(int indent) const;
public:
  std::string uniqns;
  std::string injectns;
  const char *namesym;
  const char *fldsym;
  const char *idxsym;
  expr_te *te;
  expr_stmts *stmts;
};

struct expr_foldfe : public expr_i {
  expr_foldfe(const char *fn, int line, const char *itersym,
    expr_i *valueste, const char *embedsym, expr_i *embedexpr,
    const char *foldop, expr_i *stmts);
  expr_i *clone() const { return new expr_foldfe(*this); }
  expr_e get_esort() const { return expr_e_foldfe; }
  int get_num_children() const { return 2; }
  expr_i *get_child(int i) {
    if (i == 0) { return valueste; }
    if (i == 1) { return stmts; }
    return 0;
  }
  void set_child(int i, expr_i *e) {
    if (i == 0) { valueste = ptr_down_cast<expr_te>(e); }
    if (i == 1) { stmts = ptr_down_cast<expr_stmts>(e); }
  }
  void set_unique_namespace_one(const std::string& u, const std::string& i)
    { uniqns = u; injectns = i; }
  std::string get_unique_namespace() const { return uniqns; }
  void check_type(symbol_table *lookup);
  bool has_expr_to_emit() const { return true; }
  bool is_expression() const { return false; }
  void emit_value(emit_context& em);
  std::string dump(int indent) const;
public:
  std::string uniqns;
  std::string injectns;
  const char *itersym;
  expr_te *valueste;
  const char *embedsym;
  expr_i *embedexpr;
  const char *foldop;
  expr_stmts *stmts;
};

struct expr_expand : public expr_i {
  expr_expand(const char *fn, int line, const char *itersym,
    const char *idxsym, expr_i *valueste, expr_i *baseexpr, expand_e ex,
    expr_i *rest);
  expr_i *clone() const;
  expr_e get_esort() const { return expr_e_expand; }
  int get_num_children() const { return 3; }
  expr_i *get_child(int i) {
    if (i == 0) { return valueste; }
    if (i == 1) { return baseexpr; }
    if (i == 2) { return rest; }
    return 0;
  }
  void set_child(int i, expr_i *e) {
    if (i == 0) { valueste = ptr_down_cast<expr_te>(e); }
    if (i == 1) { baseexpr = e; }
    if (i == 2) { rest = e; }
  }
  void set_unique_namespace_one(const std::string& u, const std::string& i)
    { uniqns = u; injectns = i; }
  std::string get_unique_namespace() const { return uniqns; }
  void check_type(symbol_table *lookup);
  bool has_expr_to_emit() const { return false; }
  bool is_expression() const { return false; }
  void emit_value(emit_context& em) { }
  std::string dump(int indent) const;
public:
  std::string uniqns;
  std::string injectns;
  const char *itersym;
  const char *idxsym;
  expr_te *valueste;
  expr_i *baseexpr;
  expand_e ex;
  expr_i *rest;
  expr_i *generated_expr;
};

struct expr_funcdef : public expr_i {
  expr_funcdef(const char *fn, int line, const char *sym, const char *cname,
    bool is_const, expr_i *block, bool ext_decl, bool no_def,
    attribute_e attr);
  expr_i *clone() const;
  expr_e get_esort() const { return expr_e_funcdef; }
  int get_num_children() const { return 1; }
  expr_i *get_child(int i) {
    if (i == 0) { return block; }
    return 0;
  }
  void set_child(int i, expr_i *e) {
    if (i == 0) { block = ptr_down_cast<expr_block>(e); }
  }
  expr_block *get_template_block() { return block; }
  bool is_global_function() const;
  bool has_template_param_upvalues() const {
    return tpup_thisptr != 0 || !tpup_vec.empty(); }
    /* NOTE: can be used only after compiled */
  expr_struct *is_member_function() const;
  expr_interface *is_virtual_function() const;
  expr_i *is_virtual_or_member_function() const;
  #if 0
  expr_funcdef *is_member_function_descent();
  #endif
  const term& get_value_texpr() { return value_texpr; }
  void set_value_texpr(const term& t) { value_texpr = t; }
  void set_unique_namespace_one(const std::string& u, const std::string& i)
    { uniqns = u; injectns = i; }
  std::string get_unique_namespace() const { return uniqns; }
  attribute_e get_attribute() const { return attr; }
  void set_attribute(attribute_e a) { attr = a; }
  void define_const_symbols_one() {
    if (sym != 0) {
      symtbl_lexical->define_name(sym, injectns, this, attr, 0);
    }
  }
  const term& get_rettype();
  void check_type(symbol_table *lookup);
  std::string emit_symbol_str() const;
  bool has_expr_to_emit() const { return false; }
  bool is_expression() const { return false; }
  void emit_symbol(emit_context& em) const;
  void emit_value(emit_context& em);
  std::string dump(int indent) const;
public:
  const char *sym;
  std::string uniqns;
  std::string injectns;
  const char *const cname;
  bool is_const;
private:
  term rettype_eval;
public:
  expr_block *block;
  bool ext_decl : 1;  /* imported function and block may be null */
  bool no_def : 1;    /* extern c function or virtual function decl */
  term value_texpr;
  typedef std::vector<expr_funcdef *> callee_vec_type;
  typedef std::set<expr_funcdef *> callee_set_type;
  callee_vec_type callee_vec;
  callee_set_type callee_set;
  #if 0
  callee_vec_type callee_vec_tr;
  callee_set_type callee_set_tr;
  #endif
  /* following tpup_* flds are used when this function instance has
   * template parameter functions and they need upvalues. */
  typedef std::vector<expr_i *> tpup_vec_type;
  typedef std::set<expr_i *> tpup_set_type;
  tpup_vec_type tpup_vec;
  tpup_set_type tpup_set;
  expr_struct *tpup_thisptr;
  bool tpup_thisptr_nonconst : 1;
  attribute_e attr;
};

struct expr_typedef : public expr_i {
  expr_typedef(const char *fn, int line, const char *sym,
    const char *cname, const char *family, bool is_enum, bool is_bitmask,
    expr_i *enumvals, unsigned int num_tparams, attribute_e attr);
  expr_i *clone() const { return new expr_typedef(*this); }
  expr_e get_esort() const { return expr_e_typedef; }
  int get_num_children() const { return 1; }
  expr_i *get_child(int i) { return i == 0 ? enumvals : 0; }
  void set_child(int i, expr_i *e) {
    if (i == 0) { enumvals = ptr_down_cast<expr_enumval>(e); }
  }
  const term& get_value_texpr() { return value_texpr; }
  void set_value_texpr(const term& t) { value_texpr = t; }
  void set_unique_namespace_one(const std::string& u, const std::string& i);
  std::string get_unique_namespace() const { return uniqns; }
  attribute_e get_attribute() const { return attr; }
  void set_attribute(attribute_e a) { attr = a; }
  void define_const_symbols_one() {
    symtbl_lexical->define_name(sym, injectns, this, attr, 0);
  }
  void check_type(symbol_table *lookup);
  bool has_expr_to_emit() const { return false; }
  bool is_expression() const { return false; }
  std::string emit_symbol_str() const;
  void emit_symbol(emit_context& em) const;
  void emit_value(emit_context& em);
  std::string dump(int indent) const;
public:
  const char *const sym;
  std::string uniqns;
  std::string injectns;
  const char *const cname;
  const char *const typefamily_str; // TODO: unused
  bool is_enum : 1;
  bool is_bitmask : 1;
  expr_enumval *enumvals;
  const unsigned int num_tparams;
  attribute_e attr;
  type_attribute tattr;
  term value_texpr;
  typefamily_e typefamily; // TODO: unused
};

struct expr_metafdef : public expr_i {
  expr_metafdef(const char *fn, int line, const char *sym, expr_i *tparams,
    expr_i *rhs, attribute_e attr);
  expr_i *clone() const;
  expr_e get_esort() const { return expr_e_metafdef; }
  int get_num_children() const { return 1; }
  expr_i *get_child(int i) {
    if (i == 0) { return block; }
    return 0;
  }
  void set_child(int i, expr_i *e) {
    if (i == 0) { block = ptr_down_cast<expr_block>(e); }
  }
  void set_unique_namespace_one(const std::string& u, const std::string& i)
    { uniqns = u; injectns = i; }
  std::string get_unique_namespace() const { return uniqns; }
  attribute_e get_attribute() const { return attr; }
  void set_attribute(attribute_e a) { attr = a; }
  void define_const_symbols_one() {
    if (sym[0] != '\0') {
      symtbl_lexical->define_name(sym, injectns, this, attr, 0);
    }
  }
  void check_type(symbol_table *lookup);
  bool has_expr_to_emit() const { return false; }
  bool is_expression() const { return false; }
  std::string emit_symbol_str() const;
  void emit_symbol(emit_context& em) const;
  void emit_value(emit_context& em);
  std::string dump(int indent) const;
  expr_tparams *get_tparams() const { return block->tinfo.tparams; }
  expr_i *get_rhs() const { return block->stmts->head; }
public:
  const char *const sym;
  std::string uniqns;
  std::string injectns;
  expr_block *block;
  attribute_e attr;
  term metafdef_term; /* caches metafdef_to_term() */
};

struct expr_struct : public expr_i {
  expr_struct(const char *fn, int line, const char *sym, const char *cname,
    const char *family, expr_i *block, attribute_e attr, bool has_udcon);
  expr_struct *clone() const;
  expr_e get_esort() const { return expr_e_struct; }
  int get_num_children() const { return 1; }
  expr_i *get_child(int i) {
    if (i == 0) { return block; }
    return 0;
  }
  void set_child(int i, expr_i *e) {
    if (i == 0) { block = ptr_down_cast<expr_block>(e); }
  }
  expr_block *get_template_block() { return block; }
  void get_fields(std::list<expr_var *>& flds_r) const;
  const term& get_value_texpr() { return value_texpr; }
  void set_value_texpr(const term& t) { value_texpr = t; }
  void set_unique_namespace_one(const std::string& u, const std::string& i);
  std::string get_unique_namespace() const { return uniqns; }
  attribute_e get_attribute() const { return attr; }
  void set_attribute(attribute_e a) { attr = a; }
  void define_const_symbols_one() {
    symtbl_lexical->define_name(sym, injectns, this, attr, 0);
  }
  void check_type(symbol_table *lookup);
  bool has_expr_to_emit() const { return false; }
  bool is_expression() const { return false; }
  std::string emit_symbol_str() const;
  void emit_symbol(emit_context& em) const;
  void emit_value(emit_context& em);
  std::string dump(int indent) const;
  bool has_userdefined_constr() const;
public:
  const char *const sym;
  std::string uniqns;
  std::string injectns;
  const char *const cname;
  const char *const typefamily_str;
  expr_block *block;
  attribute_e attr;
  term value_texpr;
  typefamily_e typefamily;
  bool has_udcon;
  const term *builtin_typestub;
  builtin_strict_metafunc_t metafunc_strict;
  builtin_nonstrict_metafunc_t metafunc_nonstrict;
};

struct expr_dunion : public expr_i {
  expr_dunion(const char *fn, int line, const char *sym, expr_i *block,
    attribute_e attr);
  expr_i *clone() const;
  expr_e get_esort() const { return expr_e_dunion; }
  int get_num_children() const { return 1; }
  expr_i *get_child(int i) { return i == 0 ? block : 0; }
  void set_child(int i, expr_i *e) {
    if (i == 0) { block = ptr_down_cast<expr_block>(e); }
  }
  expr_block *get_template_block() { return block; }
  void get_fields(std::list<expr_var *>& flds_r) const;
  expr_var *get_first_field() const;
  const term& get_value_texpr() { return value_texpr; }
  void set_value_texpr(const term& t) { value_texpr = t; }
  void set_unique_namespace_one(const std::string& u, const std::string& i)
    { uniqns = u; injectns = i; }
  std::string get_unique_namespace() const { return uniqns; }
  attribute_e get_attribute() const { return attr; }
  void set_attribute(attribute_e a) { attr = a; }
  void define_const_symbols_one() {
    symtbl_lexical->define_name(sym, injectns, this, attr, 0);
  }
  void check_type(symbol_table *lookup);
  bool has_expr_to_emit() const { return false; }
  bool is_expression() const { return false; }
  std::string emit_symbol_str() const;
  void emit_symbol(emit_context& em) const;
  void emit_value(emit_context& em);
  std::string dump(int indent) const;
public:
  const char *const sym;
  std::string uniqns;
  std::string injectns;
  expr_block *block;
  attribute_e attr;
  term value_texpr;
};

struct expr_interface : public expr_i {
  expr_interface(const char *fn, int line, const char *sym, const char *cname,
    expr_i *block, attribute_e attr);
  expr_i *clone() const;
  expr_e get_esort() const { return expr_e_interface; }
  int get_num_children() const { return 1; }
  expr_i *get_child(int i) { return i == 0 ? block : 0; }
  void set_child(int i, expr_i *e) {
    if (i == 0) { block = ptr_down_cast<expr_block>(e); }
  }
  expr_block *get_template_block() { return block; }
  const term& get_value_texpr() { return value_texpr; }
  void set_value_texpr(const term& t) { value_texpr = t; }
  void set_unique_namespace_one(const std::string& u, const std::string& i)
    { uniqns = u; injectns = i; }
  std::string get_unique_namespace() const { return uniqns; }
  attribute_e get_attribute() const { return attr; }
  void set_attribute(attribute_e a) { attr = a; }
  void define_const_symbols_one() {
    symtbl_lexical->define_name(sym, injectns, this, attr, 0);
  }
  void check_type(symbol_table *lookup);
  bool has_expr_to_emit() const { return false; }
  bool is_expression() const { return false; }
  std::string emit_symbol_str() const;
  void emit_symbol(emit_context& em) const;
  void emit_value(emit_context& em);
  std::string dump(int indent) const;
public:
  const char *const sym;
  std::string uniqns;
  std::string injectns;
  const char *const cname;
  expr_block *block;
  attribute_e attr;
  term value_texpr;
};

struct expr_try : public expr_i {
  expr_try(const char *fn, int line, expr_i *tblock, expr_i *cblock,
    expr_i *rest);
  expr_i *clone() const { return new expr_try(*this); }
  expr_e get_esort() const { return expr_e_try; }
  int get_num_children() const { return 3; }
  expr_i *get_child(int i) {
    if (i == 0) { return tblock; }
    else if (i == 1) { return cblock; }
    else if (i == 2) { return rest; }
    return 0;
  }
  void set_child(int i, expr_i *e) {
    if (i == 0) { tblock = ptr_down_cast<expr_block>(e); }
    else if (i == 1) { cblock = ptr_down_cast<expr_block>(e); }
    else if (i == 2) { rest = ptr_down_cast<expr_try>(e); }
  }
  std::string dump(int indent) const;
  void check_type(symbol_table *lookup);
  bool has_expr_to_emit() const { return true; }
  bool is_expression() const { return false; }
  void emit_value(emit_context& em);
public:
  expr_block *tblock; /* try */
  expr_block *cblock; /* catch */
  expr_try *rest;
};

};

#endif

