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

#include <vector>
#include "expr_misc.hpp"
#include "checktype.hpp"
#include "sort_dep.hpp"
#include "eval.hpp"
#include "util.hpp"

#define DBG_UP(x)
#define DBG(x)
#define DBG_TMPL(x)
#define DBG_CLONE(x)
#define DBG_SUB(x)
#define DBG_CONV(x)
#define DBG_NEWTE(x)
#define DBG_INST(x)
#define DBG_COMPILE(x)
#define DBG_COPT(x)
#define DBG_SYMTBL(x)
#define DBG_THR(x)
#define DBG_LV(x)
#define DBG_SLICE(x)
#define DBG_CALLEE(x)

namespace pxc {

/* begin: global variables */
expr_arena_type expr_arena;
str_arena_type str_arena;
topvals_type topvals;
expr_block *global_block = 0;
loaded_namespaces_type loaded_namespaces;
unsigned int expr_block_id = 1;
builtins_type builtins; 
errors_type cur_errors;
std::string main_namespace;
int compile_phase = 0;
nsaliases_type nsaliases;
/* end: global variables */

void print_space(int n, char c, FILE *fp)
{
  for (int i = 0; i < n; ++i) {
    // fputc(c, stdout);
    fputc(' ', stdout);
  }
}

std::string space_string(int n, char c)
{
  std::string s;
  s.resize(n);
  for (int i = 0; i < n; ++i) {
    // s[i] = c;
    s[i] = ' ';
  }
  return s;
}

#if 0
std::string ulong_to_string(unsigned long long v)
{
  char buf[32];
  snprintf(buf, sizeof(buf), "%llu", v);
  return std::string(buf);
}

std::string long_to_string(long long v)
{
  char buf[32];
  snprintf(buf, sizeof(buf), "%lld", v);
  return std::string(buf);
}
#endif

const char *tok_string(const expr_i *e, int tok)
{
  switch (tok) {
  case ',': return ",";
  case '=': return "=";
  case '?': return "?";
  case ':': return ":";
  case '!': return "!";
  case '~': return "~";
  case '|': return "|";
  case '^': return "^";
  case '&': return "&";
  case '<': return "<";
  case '>': return ">";
  case '+': return "+";
  case '-': return "-";
  case '*': return "*";
  case '/': return "/";
  case '%': return "%";
  case '[': return "[]";
  case '.': return ".";
  case '(': return "()";
  case TOK_LOR: return "||";
  case TOK_LAND: return "&&";
  case TOK_SHIFTL: return "<<";
  case TOK_SHIFTR: return ">>";
  case TOK_PLUS: return "+";
  case TOK_MINUS: return "-";
  case TOK_INC: return "++";
  case TOK_DEC: return "--";
  case TOK_EQ: return "==";
  case TOK_NE: return "!=";
  case TOK_GE: return ">=";
  case TOK_LE: return "<=";
  case TOK_PTR_REF: return "&";
  case TOK_PTR_DEREF: return "*";
  case TOK_ARROW: return "->";
  case TOK_ADD_ASSIGN: return "+=";
  case TOK_SUB_ASSIGN: return "-=";
  case TOK_MUL_ASSIGN: return "*=";
  case TOK_DIV_ASSIGN: return "/=";
  case TOK_MOD_ASSIGN: return "%=";
  case TOK_OR_ASSIGN: return "|=";
  case TOK_AND_ASSIGN: return "&=";
  case TOK_XOR_ASSIGN: return "^=";
  case TOK_CASE: return "case";
  case TOK_SLICE: return "::";
  default:
    arena_error_throw(e, "internal error: unknown token %d", tok);
  }
  return "";
}

int count_newline(const char *str)
{
  int r = 0;
  for (const char *p = str; *p; ++p) {
    if (*p == '\n') {
      ++r;
    }
  }
  return r;
}

std::string get_full_name(expr_i *nssym)
{
  expr_nssym *ns = ptr_down_cast<expr_nssym>(nssym);
  if (ns == 0) {
    return std::string();
  }
  std::string s(ns->sym);
  while (true) {
    ns = ptr_down_cast<expr_nssym>(ns->prefix);
    if (ns == 0) {
      break;
    }
    s = std::string(ns->sym) + "::" + s;
  }
  return s;
}

std::string to_short_name(const std::string& fullname)
{
  const std::string::size_type tp = fullname.find('<');
  const std::string::size_type pos = fullname.rfind(':', tp);
  if (pos == fullname.npos) {
    return fullname;
  }
  return fullname.substr(pos + 1);
}

std::string get_namespace_part(const std::string& fullname)
{
  const std::string::size_type tp = fullname.find('<');
  const std::string::size_type pos = fullname.rfind(':', tp);
  if (pos == fullname.npos || pos <= 1 || fullname[pos - 1] != ':') {
    return std::string();
  }
  return fullname.substr(0, pos - 1);
}

bool has_namespace(const std::string& name)
{
  const std::string::size_type tp = name.find('<');
  const std::string::size_type pos = name.rfind(':', tp);
  return pos != name.npos;
}

bool is_type(const term& t)
{
  if (t.get_expr() == 0) {
    return false;
  }
  return is_type_esort(t.get_expr()->get_esort());
}

bool is_void_type(const term& t)
{
  return t == builtins.type_void;
}

bool is_unit_type(const term& t)
{
  return t == builtins.type_unit;
}

bool is_tpdummy_type(const term& t)
{
  return t == builtins.type_tpdummy;
}

bool is_bool_type(const term& t)
{
  return t == builtins.type_bool;
}

bool is_numeric_type(const term& t)
{
  return (is_integral_type(t) || is_float_type(t));
}

bool is_enum(const term& t)
{
  expr_typedef *etd = dynamic_cast<expr_typedef *>(t.get_expr());
  if (etd != 0) {
    return etd->is_enum;
  }
  const typefamily_e cat = get_family(t);
  return cat == typefamily_e_extenum;
}

bool is_bitmask(const term& t)
{
  expr_typedef *etd = dynamic_cast<expr_typedef *>(t.get_expr());
  if (etd != 0) {
    return etd->is_bitmask;
  }
  const typefamily_e cat = get_family(t);
  return cat == typefamily_e_extbitmask;
}

bool is_boolean_algebra(const term& t)
{
  if (is_bool_type(t) || is_bitmask(t) || is_integral_type(t)) {
    return true;
  }
  const typefamily_e cat = get_family(t);
  return cat == typefamily_e_extint || cat == typefamily_e_extuint
    || cat == typefamily_e_extbitmask;
}

static bool is_builtin_pod(const term& t)
{
  const expr_typedef *const td = dynamic_cast<const expr_typedef *>(
    t.get_expr());
  return td != 0;
}

bool is_equality_type(const term& t)
{
  if (is_ordered_type(t)) {
    return true;
  }
  if (is_enum(t) || is_bitmask(t)) {
    return true;
  }
  if (is_cm_pointer_family(t)) {
    return true;
  }
  return false;
}

bool is_ordered_type(const term& t)
{
  if (is_numeric_type(t)) {
  // if (is_builtin_pod(t)) {
    return true;
  }
  if (t == builtins.type_string || t == builtins.type_strlit) {
    return true;
  }
  if (is_cm_slice_family(t) || is_array_family(t)) {
    term et = get_array_elem_texpr(0, t);
    if (is_builtin_pod(et) && is_integral_type(et)) {
      /* op == is allowed only if it is memcmp-compatible */
      return true;
    }
  }
  return false;
}

bool is_possibly_pod(const term& t)
{
  if (is_builtin_pod(t)) {
    return true;
  }
  const typefamily_e cat = get_family(t);
  switch (cat) {
  case typefamily_e_extint:
  case typefamily_e_extuint:
  case typefamily_e_extenum:
  case typefamily_e_extbitmask:
  case typefamily_e_extfloat:
    return true;
  case typefamily_e_noncopyable:
    return false; /* default constructible but noncopyable */
  default:
    break;
  }
  const expr_struct *est = dynamic_cast<const expr_struct *>(t.get_expr());
  if (est != 0 && est->cname != 0) {
    return true;
  }
  return false;
}

bool is_possibly_nonpod(const term& t)
{
  if (is_builtin_pod(t)) {
    return false;
  }
  const typefamily_e cat = get_family(t);
  switch (cat) {
  case typefamily_e_extint:
  case typefamily_e_extuint:
  case typefamily_e_extenum:
  case typefamily_e_extbitmask:
  case typefamily_e_extfloat:
    return false;
  case typefamily_e_noncopyable:
    return true; /* default constructible but noncopyable */
  default:
    break;
  }
  return true;
}

#if 0
bool is_string_type(const term& t)
{
  return t == builtins.type_string;
}
#endif

static bool is_pod_integral_type(const term& t)
{
  if (
    t == builtins.type_bool ||
    t == builtins.type_uchar ||
    t == builtins.type_char ||
    t == builtins.type_ushort ||
    t == builtins.type_short ||
    t == builtins.type_uint ||
    t == builtins.type_int ||
    t == builtins.type_ulong ||
    t == builtins.type_long ||
    t == builtins.type_size_t
    ) {
    return true;
  }
  const typefamily_e cat = get_family(t);
  return cat == typefamily_e_extint || cat == typefamily_e_extuint;
}

bool is_integral_type(const term& t)
{
  if (is_pod_integral_type(t)) {
    return true;
  }
  const typefamily_e cat = get_family(t);
  return cat == typefamily_e_extint || cat == typefamily_e_extuint;
}

bool is_unsigned_integral_type(const term& t)
{
  if (
    t == builtins.type_bool ||
    t == builtins.type_uchar ||
    t == builtins.type_ushort ||
    t == builtins.type_uint ||
    t == builtins.type_ulong ||
    t == builtins.type_size_t
    ) {
    return true;
  }
  const typefamily_e cat = get_family(t);
  return cat == typefamily_e_extuint;
}

bool is_float_type(const term& t)
{
  return t == builtins.type_double || t == builtins.type_float;
}

static typefamily_e get_family(const expr_i *e)
{
  const expr_struct *est = dynamic_cast<const expr_struct *>(e);
  std::string cat;
  if (est != 0) {
    return est->typefamily;
  }
  return typefamily_e_none;
}

typefamily_e get_family(const term& t)
{
  return get_family(t.get_expr());
}

typefamily_e get_family_from_string(const std::string& s)
{
  if (s == "ptr") return typefamily_e_ptr;
  if (s == "cptr") return typefamily_e_cptr;
  if (s == "iptr") return typefamily_e_iptr;
  if (s == "tptr") return typefamily_e_tptr;
  if (s == "tcptr") return typefamily_e_tcptr;
  if (s == "tiptr") return typefamily_e_tiptr;
  if (s == "lockobject") return typefamily_e_lockobject;
  if (s == "clockobject") return typefamily_e_clockobject;
  if (s == "extint") return typefamily_e_extint;
  if (s == "extuint") return typefamily_e_extuint;
  if (s == "extenum") return typefamily_e_extenum;
  if (s == "extbitmask") return typefamily_e_extbitmask;
  if (s == "extfloat") return typefamily_e_extfloat;
  if (s == "extnumeric") return typefamily_e_extnumeric;
  if (s == "varray") return typefamily_e_varray;
  if (s == "darray") return typefamily_e_darray;
  if (s == "farray") return typefamily_e_farray;
  if (s == "slice") return typefamily_e_slice;
  if (s == "cslice") return typefamily_e_cslice;
  if (s == "tree_map") return typefamily_e_tree_map;
  if (s == "tree_map_range") return typefamily_e_tree_map_range;
  if (s == "tree_map_crange") return typefamily_e_tree_map_crange;
  if (s == "linear") return typefamily_e_linear;
  if (s == "noncopyable") return typefamily_e_noncopyable;
  if (s == "nodefault") return typefamily_e_nodefault;
  if (s == "nocascade") return typefamily_e_nocascade;
  // if (s == "wptr") return typefamily_e_wptr;
  // if (s == "wcptr") return typefamily_e_wcptr;
  return typefamily_e_none;
}

std::string get_family_string(typefamily_e cat)
{
  switch (cat) {
  case typefamily_e_none: return "";
  case typefamily_e_ptr: return "ptr";
  case typefamily_e_cptr: return "cptr";
  case typefamily_e_iptr: return "iptr";
  case typefamily_e_tptr: return "tptr";
  case typefamily_e_tcptr: return "tcptr";
  case typefamily_e_tiptr: return "tiptr";
  case typefamily_e_lockobject: return "lockobject";
  case typefamily_e_clockobject: return "clockobject";
  case typefamily_e_extint: return "extint";
  case typefamily_e_extuint: return "extuint";
  case typefamily_e_extenum: return "extenum";
  case typefamily_e_extbitmask: return "extbitmask";
  case typefamily_e_extfloat: return "extfloat";
  case typefamily_e_extnumeric: return "extnumeric";
  case typefamily_e_varray: return "varray";
  case typefamily_e_darray: return "darray";
  case typefamily_e_farray: return "farray";
  case typefamily_e_slice: return "slice";
  case typefamily_e_cslice: return "cslice";
  case typefamily_e_tree_map: return "tree_map";
  case typefamily_e_tree_map_range: return "tree_map_range";
  case typefamily_e_tree_map_crange: return "tree_map_crange";
  case typefamily_e_linear: return "linear";
  case typefamily_e_noncopyable: return "noncopyable";
  case typefamily_e_nodefault: return "nodefault";
  case typefamily_e_nocascade: return "nocascade";
  }
  abort();
}

static bool is_pointer_family(const typefamily_e cat)
{
  switch (cat) {
  case typefamily_e_ptr:
  case typefamily_e_cptr:
  case typefamily_e_iptr:
  case typefamily_e_tptr:
  case typefamily_e_tcptr:
  case typefamily_e_tiptr:
  case typefamily_e_lockobject:
  case typefamily_e_clockobject:
    return true;
  default:
    return false;
  }
}

static bool is_threaded_pointer_family(const typefamily_e cat)
{
  return cat == typefamily_e_tptr || cat == typefamily_e_tcptr
    || cat == typefamily_e_tiptr;
}

static bool is_immutable_pointer_family(const typefamily_e cat)
{
  return cat == typefamily_e_iptr || cat == typefamily_e_tiptr;
}

bool pointer_conversion_allowed(const typefamily_e from,
  const typefamily_e to)
{
  switch (from) {
  case typefamily_e_ptr:
    return to == from || to == typefamily_e_cptr;
  case typefamily_e_cptr:
    return to == from;
  case typefamily_e_iptr:
    return to == from || to == typefamily_e_cptr;
  case typefamily_e_tptr:
    return to == from || to == typefamily_e_tcptr
      || to == typefamily_e_lockobject || to == typefamily_e_clockobject;
  case typefamily_e_tcptr:
    return to == from || to == typefamily_e_clockobject;
  case typefamily_e_tiptr:
    return to == from;
  case typefamily_e_lockobject:
    return false;
  case typefamily_e_clockobject:
    return false;
  default:
    return false;
  }
}

term get_pointer_target(const term& t)
{
  const typefamily_e cat = get_family(t);
  if (is_pointer_family(cat)) {
    const term_list *args = t.get_args();
    if (args != 0 && !args->empty()) {
      return *args->begin();
    }
  }
  return term();
}

#if 0
bool is_same_threading_pointer(const term& t0, const term& t1)
{
  const typefamily_e c0 = get_family(t0);
  const typefamily_e c1 = get_family(t1);
  bool thr0 = is_threaded_pointer_family(c0);
  bool thr1 = is_threaded_pointer_family(c1);
  return thr0 == thr1;
}
#endif

bool is_function(const term& t)
{
  const expr_funcdef *const tfd = dynamic_cast<const expr_funcdef *>(
    t.get_expr());
  return tfd != 0;
}

bool is_ctypedef(const term& t)
{
  const expr_typedef *const ttd = dynamic_cast<const expr_typedef *>(
    t.get_expr());
  return ttd != 0;
}

bool is_struct(const term& t)
{
  const expr_struct *const tsd = dynamic_cast<const expr_struct *>(
    t.get_expr());
  return tsd != 0;
}

bool is_dunion(const term& t)
{
  const expr_dunion *const tv = dynamic_cast<const expr_dunion *>(
    t.get_expr());
  return tv != 0;
}

bool is_interface(const term& t)
{
  const expr_interface *const ti =
    dynamic_cast<const expr_interface *>(t.get_expr());
  return ti != 0;
}

bool is_interface_or_impl(const term& t)
{
  const expr_struct *const tsd = dynamic_cast<const expr_struct *>(
    t.get_expr());
  const expr_interface *const ti =
    dynamic_cast<const expr_interface *>(t.get_expr());
  if ((tsd != 0 && tsd->block->inherit) || ti != 0) {
    return true;
  }
  return false;
}

bool is_noninterface_pointer(const term& t)
{
  term tt = get_pointer_target(t);
  if (tt.get_expr() == 0) {
    return false;
  }
  return !is_interface_or_impl(tt);
}

bool is_interface_pointer(const term& t)
{
  term tt = get_pointer_target(t);
  if (tt.get_expr() == 0) {
    return false;
  }
  return is_interface_or_impl(tt);
}

bool is_cm_pointer_family(const term& t)
{
  return is_pointer_family(get_family(t));
}

bool is_const_or_immutable_pointer_family(const term& t)
{
  const typefamily_e cat = get_family(t);
  return cat == typefamily_e_tcptr || cat == typefamily_e_cptr ||
    cat == typefamily_e_clockobject || cat == typefamily_e_tiptr ||
    cat == typefamily_e_iptr;
}

bool is_immutable_pointer_family(const term& t)
{
  const typefamily_e cat = get_family(t);
  return cat == typefamily_e_tiptr || cat == typefamily_e_iptr;
}

bool is_multithreaded_pointer_family(const term& t)
{
  const typefamily_e cat = get_family(t);
  return cat == typefamily_e_tptr || cat == typefamily_e_tcptr ||
    cat == typefamily_e_tiptr;
}

bool is_cm_lockobject_family(const term& t)
{
  const typefamily_e cat = get_family(t);
  return cat == typefamily_e_lockobject || cat == typefamily_e_clockobject;
}

static bool is_cm_lockobject_family(expr_i *e)
{
  const typefamily_e cat = get_family(e);
  return cat == typefamily_e_lockobject || cat == typefamily_e_clockobject;
}

bool is_array_family(const term& t)
{
  const typefamily_e cat = get_family(t);
  return cat == typefamily_e_varray || cat == typefamily_e_darray ||
    cat == typefamily_e_farray;
}

bool is_cm_slice_family(const term& t)
{
  const typefamily_e cat = get_family(t);
  return cat == typefamily_e_slice || cat == typefamily_e_cslice;
}

bool is_map_family(const term& t)
{
  const typefamily_e cat = get_family(t);
  return cat == typefamily_e_tree_map;
}

bool is_cm_maprange_family(const term& t)
{
  const typefamily_e cat = get_family(t);
  return cat == typefamily_e_tree_map_range ||
    cat == typefamily_e_tree_map_crange;
}

bool is_const_range_family(const term& t)
{
  const typefamily_e cat = get_family(t);
  return cat == typefamily_e_cslice || cat == typefamily_e_tree_map_crange;
}

static bool is_cm_range_family(expr_i *e)
{
  const typefamily_e cat = get_family(e);
  return cat == typefamily_e_slice || cat == typefamily_e_cslice ||
    cat == typefamily_e_tree_map_range || cat == typefamily_e_tree_map_crange;
}

bool is_cm_range_family(const term& t)
{
  return is_cm_range_family(t.get_expr());
}

static bool is_ephemeral_value_type(expr_i *e)
{
  return is_cm_range_family(e);
  // || is_cm_lockobject_family(e);
}

bool is_ephemeral_value_type(const term& t)
{
  return is_ephemeral_value_type(t.get_expr());
}

bool is_container_family(const term& t)
{
  return is_array_family(t) || is_map_family(t); // TODO
}

const bool has_userdef_constr_internal(const term& t,
  std::set<const expr_i *>& checked)
{
  const expr_i *const einst = term_get_instance_const(t);
  if (checked.find(einst) != checked.end()) {
    return false;
  }
  checked.insert(einst);
  const expr_struct *const est = dynamic_cast<const expr_struct *>(einst);
  if (est != 0) {
    if (est->has_userdefined_constr()) {
      return true;
    }
    if (est->typefamily != typefamily_e_none) {
      if (est->typefamily == typefamily_e_farray) {
	const term_list *const tl = t.get_args();
	if (tl != 0 && !tl->empty()) {
	  return has_userdef_constr_internal(tl->front(), checked);
	}
      }
      return false;
    } else {
      std::list<expr_var *> flds;
      est->get_fields(flds);
      bool f = false;
      for (std::list<expr_var *>::const_iterator i = flds.begin();
	i != flds.end(); ++i) {
	f |= has_userdef_constr_internal((*i)->get_texpr(), checked);
      }
      return f;
    }
  }
  const expr_dunion *const eva = dynamic_cast<const expr_dunion *>(einst);
  if (eva != 0) {
    std::list<expr_var *>flds;
    eva->get_fields(flds);
    if (!flds.empty()) {
      return has_userdef_constr_internal(flds.front()->get_texpr(), checked);
    }
    return false;
  }
  return false;
}

bool has_userdef_constr(const term& t)
{
  std::set<const expr_i *> checked;
  return has_userdef_constr_internal(t, checked);
}

bool type_has_invalidate_guard(const term& t)
{
  const typefamily_e cat = get_family(t);
  if (
    cat == typefamily_e_varray ||
    cat == typefamily_e_tree_map ||
    cat == typefamily_e_tptr ||
    cat == typefamily_e_tcptr) {
    return true;
  }
  return false;
}

bool type_allow_feach(const term& t)
{
  const typefamily_e cat = get_family(t);
  switch (cat) {
  case typefamily_e_varray:
  case typefamily_e_darray:
  case typefamily_e_farray:
  case typefamily_e_tree_map:
  case typefamily_e_slice:
  case typefamily_e_cslice:
  case typefamily_e_tree_map_range:
  case typefamily_e_tree_map_crange:
    return true;
  default:
    return false;
  }
}

static bool is_copyable_type_one(expr_i *e)
{
  if (e->get_esort() == expr_e_interface) {
    return false;
  }
  const typefamily_e cat = get_family(e);
  if (is_cm_lockobject_family(e)) {
    return false;
  }
  if (cat == typefamily_e_linear || cat == typefamily_e_noncopyable) {
    return false;
  }
  return true;
}

bool is_copyable(const term& t)
{
  sorted_exprs c;
  sort_dep(c, t.get_expr());
  for (std::list<expr_i *>::iterator i = c.sorted.begin();
    i != c.sorted.end(); ++i) {
    if (!is_copyable_type_one(*i)) {
      return false;
    }
  }
  return true;
#if 0
  /* return true iff copy constructor is allowed */
  if (is_interface(t)) {
    return false;
  }
  const typefamily_e cat = get_family(t);
  if (cat == typefamily_e_linear || cat == typefamily_e_noncopyable) {
    return false;
  }
  return true;
#endif
}

static bool is_assignable_type_one(expr_i *e)
{
  if (!is_copyable_type_one(e)) {
    return false;
  }
  if (is_ephemeral_value_type(e)) {
    return false;
  }
  const typefamily_e cat = get_family(e);
  if (cat == typefamily_e_darray) {
    return false;
  }
  return true;
}

bool is_assignable(const term& t)
{
  sorted_exprs c;
  sort_dep(c, t.get_expr());
  for (std::list<expr_i *>::iterator i = c.sorted.begin();
    i != c.sorted.end(); ++i) {
    if (!is_assignable_type_one(*i)) {
      return false;
    }
  }
  return true;
#if 0
  /* returns true iff 'operator =' is allowed */
  if (!is_copyable(t)) {
    return false;
  }
  if (is_ephemeral_value_type(t)) {
    return false;
  }
  const typefamily_e cat = get_family(t);
  if (cat == typefamily_e_darray) {
    return false;
  }
  return true;
#endif
}

bool is_polymorphic(const term& t)
{
  const expr_i *const einst = term_get_instance_const(t);
  if (einst->get_esort() == expr_e_interface) {
    return true;
  }
  const expr_struct *const est = dynamic_cast<const expr_struct *>(einst);
  if (est != 0 && est->block->inherit != 0) {
    return true;
  }
  return false;
}

static std::string term_tostr_tparams(const expr_tparams *tp,
  term_tostr_sort s)
{
  const size_t len = elist_length(tp);
  std::string r;
  if (s == term_tostr_sort_cname) {
    r += "< ";
  } else if (s == term_tostr_sort_humanreadable) {
    r += "{";
  } else {
    r += "$tp" + ulong_to_string(len) + "$";
  }
  while (tp != 0) {
    r += term_tostr(tp->param_def, s);
    if (tp->rest != 0) {
      if (s == term_tostr_sort_humanreadable ||
	s == term_tostr_sort_cname) {
	r += ",";
      } else {
	r += "$";
      }
    }
    tp = tp->rest;
  }
  if (s == term_tostr_sort_cname) {
    r += " >";
  } else if (s == term_tostr_sort_humanreadable) {
    r += "}";
  } else {
    r += "";
  }
  return r;
}

static std::string replace_char(const std::string& s, char from, char to)
{
  std::string r = s;
  for (std::string::iterator i = r.begin(); i != r.end(); ++i) {
    if (*i == from ) {
      *i = to;
    }
  }
  return r;
}

static std::string term_tostr_instance_chain(const expr_block *bl,
  term_tostr_sort s)
{
  const expr_i *e = bl->parent_expr;
  while (e != 0 && e->get_esort() != expr_e_block) {
    e = e->parent_expr;
  }
  std::string rstr;
  if (e != 0) {
    rstr = term_tostr_instance_chain(ptr_down_cast<const expr_block>(e), s);
  }
  if (!bl->tinfo.self_key.empty()) {
    rstr += bl->tinfo.self_key + "$";
  }
  return rstr;
}

static std::string tparam_str_long(long long v, term_tostr_sort s)
{
  if (s == term_tostr_sort_cname) {
    return long_to_string(v) + "LL";
  } else if (s == term_tostr_sort_humanreadable) {
    return long_to_string(v);
  } else {
    return escape_hex_non_alnum(long_to_string(v)) + "$li";
  }
}

static std::string tparam_str_string(const std::string& v, term_tostr_sort s)
{
  if (s == term_tostr_sort_cname) {
    return escape_c_str_literal(v);
  } else if (s == term_tostr_sort_humanreadable) {
    return v;
  }
  return escape_hex_non_alnum(v) + "$ls";
}

static std::string tparam_str_list(const term_list& tl, term_tostr_sort s)
{
  return term_tostr_list(tl, s);
}

std::string term_tostr(const term& t, term_tostr_sort s)
{
  if (t.is_null()) {
    return "";
  }
  if (t.is_long()) {
    return tparam_str_long(t.get_long(), s);
  }
  if (t.is_string()) {
    return tparam_str_string(t.get_string(), s);
  }
  if (t.is_metalist()) {
    return tparam_str_list(*t.get_metalist(), s);
  }
  const expr_i *const tdef = t.get_expr();
    /* expr_struct, expr_dunion, expr_interface, expr_typedef, expr_metafdef,
     * or expr_funcdef */
  if (tdef == 0) {
    return std::string();
  }
  const char *sym = 0;
  const char *cname = 0;
  std::string uniqns;
  const expr_block *self_block = 0;
  char esort_char = 0;
  expr_tparams *tparams = 0;
  bool append_block_id_if_local = true;
  bool is_virtual_or_member_function_flag = false;
  if (tdef == 0) {
    sym = "";
  } else {
    switch (tdef->get_esort()) {
    case expr_e_struct:
      sym = ptr_down_cast<const expr_struct>(tdef)->sym;
      self_block = ptr_down_cast<const expr_struct>(tdef)->block;
      tparams = self_block->tinfo.tparams;
      uniqns = ptr_down_cast<const expr_struct>(tdef)->uniqns;
      cname = ptr_down_cast<const expr_struct>(tdef)->cname;
      esort_char = 's';
      break;
    case expr_e_dunion:
      sym = ptr_down_cast<const expr_dunion>(tdef)->sym;
      self_block = ptr_down_cast<const expr_dunion>(tdef)->block;
      tparams = self_block->tinfo.tparams;
      uniqns = ptr_down_cast<const expr_dunion>(tdef)->uniqns;
      esort_char = 'v';
      break;
    case expr_e_interface:
      sym = ptr_down_cast<const expr_interface>(tdef)->sym;
      self_block = ptr_down_cast<const expr_interface>(tdef)->block;
      tparams = self_block->tinfo.tparams;
      uniqns = ptr_down_cast<const expr_interface>(tdef)->uniqns;
      cname = ptr_down_cast<const expr_interface>(tdef)->cname;
      esort_char = 'i';
      break;
    case expr_e_typedef:
      {
	const expr_typedef *etd = ptr_down_cast<const expr_typedef>(tdef);
	if (s == term_tostr_sort_cname) {
	  if (etd->is_enum || etd->is_bitmask) {
	    return "int";
	  }
	}
	sym = etd->sym;
	uniqns = etd->uniqns;
	cname = etd->cname;
	esort_char = 't';
	self_block = 0;
      }
      break;
    case expr_e_metafdef:
      sym = ptr_down_cast<const expr_metafdef>(tdef)->sym;
      uniqns = ptr_down_cast<const expr_metafdef>(tdef)->uniqns;
      tparams = ptr_down_cast<const expr_metafdef>(tdef)->get_tparams();
      esort_char = 0;
      self_block = 0;
      esort_char = 'a';
      break;
    case expr_e_funcdef:
      {
	const expr_funcdef *const efd = ptr_down_cast<const expr_funcdef>(
	  tdef);
	sym = efd->sym;
	self_block = efd->block;
	tparams = self_block->tinfo.tparams;
	is_virtual_or_member_function_flag
	  = efd->is_virtual_or_member_function();
	uniqns = efd->uniqns;
	if (efd->is_virtual_or_member_function()) {
	  append_block_id_if_local = false;
	}
	cname = efd->cname;
	esort_char = 'f';
      }
      break;
    case expr_e_int_literal:
      {
	const expr_int_literal *const ei =
	  ptr_down_cast<const expr_int_literal>(tdef);
	return tparam_str_long(ei->get_signed(), s);
      }
      break;
    case expr_e_str_literal:
      {
	const expr_str_literal *const es =
	  ptr_down_cast<const expr_str_literal>(tdef);
	return tparam_str_string(es->value, s);
      }
      break;
    case expr_e_tparams:
      {
	const expr_tparams *const etp = ptr_down_cast<const expr_tparams>(
	  tdef);
	sym = etp->sym;
	esort_char = 0;
	self_block = 0;
	esort_char = 'x';
      }
      break;
    case expr_e_enumval:
      {
	const expr_enumval *const eev = ptr_down_cast<const expr_enumval>(
	  tdef);
	sym = eev->sym;
	uniqns = eev->uniqns;
	tparams = 0;
	esort_char = 0;
	self_block = 0;
	esort_char = 'a';
      }
      break;
    default:
      return "[" + ulong_to_string(tdef->get_esort()) + "]";
    }
  }
  symbol_table *const st_defined = tdef->symtbl_lexical;
  if (st_defined == 0) {
    return "[nosymtbl]";
  }
  std::string rstr, rstr_post;
  switch (s) {
  case term_tostr_sort_humanreadable:
    if (!uniqns.empty() && !is_virtual_or_member_function_flag) {
      rstr += uniqns + "::";
    }
    rstr += sym ? sym : "[noname]";
    break;
  case term_tostr_sort_strict:
  case term_tostr_sort_cname:
  case term_tostr_sort_cname_tparam:
    if (is_cm_pointer_family(t)) {
      const typefamily_e cat = get_family(t);
      const std::string catstr = get_family_string(cat);
      if (s == term_tostr_sort_cname_tparam) {
	rstr += "pxcrt$$" + catstr;
      } else {
	if (is_cm_lockobject_family(t)) {
	  const std::string s = (cat == typefamily_e_lockobject)
	    ? "pxcrt::lockobject" : "pxcrt::clockobject";
	  if (is_interface_pointer(t)) {
	    rstr += s;
	  } else {
	    rstr += s + "< pxcrt::trcval";
	    rstr_post = " >";
	  }
	} else if (is_interface_pointer(t)) {
	  rstr += "pxcrt::rcptr";
	} else {
	  if (cat == typefamily_e_tcptr || cat == typefamily_e_tptr) {
	    rstr += "pxcrt::rcptr< pxcrt::trcval";
	  } else if (cat == typefamily_e_tiptr) {
	    rstr += "pxcrt::rcptr< pxcrt::tircval";
	  } else {
	    rstr += "pxcrt::rcptr< pxcrt::rcval";
	  }
	  rstr_post = " >"; // need additional '>'
	}
      }
    } else if (cname != 0 && s == term_tostr_sort_cname) {
      rstr = cname;
      #if 0
      if (s != term_tostr_sort_cname) {
	rstr = replace_char(rstr, ':', '$');
      }
      #endif
    } else {
      rstr = "";
      if (s == term_tostr_sort_cname_tparam || s == term_tostr_sort_strict) {
	rstr += replace_char(uniqns, ':', '$') + "$n$$";
      } else /* if (st_defined->get_lexical_parent() == 0) */ {
	rstr += to_c_ns(uniqns) + "::";
      }
      if (sym != 0) {
	rstr += sym;
      }
      if (esort_char) {
	rstr += "$";
	rstr.push_back(esort_char);
      }
      const expr_block *bl_def = 0;
      if (sym == 0) {
	assert(self_block != 0);
	bl_def = self_block;
	#if 0
	rstr += ulong_to_string(self_block->block_id_ns);
	#endif
      } else if (st_defined->get_lexical_parent() != 0
	&& append_block_id_if_local) {
	bl_def = st_defined->block_backref;
	#if 0
	rstr += ulong_to_string(st_defined->block_backref->block_id_ns);
	#endif
      }
      if (bl_def != 0) {
	if (bl_def->block_id_ns == 0) {
	  arena_error_throw(tdef,
	    "can not use '%s' as a template parameter "
	    "because it is generated by 'expand'", sym);
	}
	rstr += ulong_to_string(bl_def->block_id_ns);
      }
    }
    break;
  }
  if (rstr.empty()) {
    arena_error_throw(tdef, "internal error: term_tostr");
  }
  if (self_block != 0 && self_block->tinfo.template_descent && cname == 0
    && !is_virtual_or_member_function_flag) {
	/* if this is a member or virtual function, its c++ name is local to
	 * the enclosing struct and should not add chain str. */
    /* this block itself is an instance */
    const std::string chain = term_tostr_instance_chain(self_block, s);
    if (!chain.empty()) {
      rstr += "$ic" + chain;
    }
  }
  term_tostr_sort schild = s;
  if (s == term_tostr_sort_cname) {
    /* use foo<bar> only if it's a c-type */
    schild = cname != 0 ?
      term_tostr_sort_cname : term_tostr_sort_cname_tparam;
  }
  const term_list *const args = t.get_args();
  if (args != 0 && !args->empty()) {
    rstr += term_tostr_list(*args, schild);
  } else if (tparams != 0) {
    const std::string tmplargs = term_tostr_tparams(tparams, schild);
    DBG(fprintf(stderr, "cname=%s tmplargs=%s\n", cname, tmplargs.c_str()));
    rstr += tmplargs;
  }
  rstr += rstr_post;
  return rstr;
}

std::string term_tostr_list(const term_list& tl, term_tostr_sort s)
{
  std::string rstr;
  if (tl.empty()) {
    return rstr;
  }
  if (s == term_tostr_sort_cname) {
    rstr += "< ";
  } else if (s == term_tostr_sort_cname_tparam
    || s == term_tostr_sort_strict) {
    rstr += "$tp" + ulong_to_string(tl.size()) + "$";
  } else {
    rstr += "{";
  }
  for (term_list::const_iterator i = tl.begin(); i != tl.end(); ++i) {
    if (i != tl.begin()) {
      if (s == term_tostr_sort_cname_tparam) {
	rstr += "$";
      } else {
	rstr += ",";
      }
    }
    rstr += term_tostr(*i, s);
  }
  if (s == term_tostr_sort_cname) {
    rstr += " >";
  } else if (s == term_tostr_sort_cname_tparam
    || s == term_tostr_sort_strict) {
    rstr += "";
  } else {
    rstr += "}";
  }
  return rstr;
}

std::string term_tostr_cname(const term& t)
{
  return term_tostr(t, term_tostr_sort_cname);
}

std::string term_tostr_human(const term& t)
{
  return term_tostr(t, term_tostr_sort_humanreadable);
}

std::string term_tostr_list_cname(const term_list& tl)
{
  return term_tostr_list(tl, term_tostr_sort_cname);
}

std::string term_tostr_list_human(const term_list& tl)
{
  return term_tostr_list(tl, term_tostr_sort_humanreadable);
}

bool is_same_type(const term& t0, const term& t1)
{
  return t0 == t1;
}

bool implements_interface(const expr_block *blk, const expr_interface *ei)
{
  expr_block::inherit_list_type::const_iterator i;
  for (i = blk->inherit_transitive.begin();
    i != blk->inherit_transitive.end(); ++i) {
    if (*i == ei) {
      return true;
    }
  }
  return false;
}

bool is_sub_type(const term& t0, const term& t1)
{
  DBG_SUB(fprintf(stderr, "is_sub_type: arg: %s %s\n",
    term_tostr(t0, term_tostr_sort_strict).c_str(),
    term_tostr(t1, term_tostr_sort_strict).c_str()));
  if (is_same_type(t0, t1)) {
    DBG_SUB(fprintf(stderr, "is_sub_type: same\n"));
    return true;
  }
  if (t0.is_null()) {
    DBG_SUB(fprintf(stderr, "is_sub_type: null\n"));
    return false;
  }
  const expr_i *const t0inst = term_get_instance_const(t0);
  const expr_struct *const t0est = dynamic_cast<const expr_struct *>(t0inst);
  const expr_interface *const t0ei = dynamic_cast<const expr_interface *>(
    t0inst);
  const expr_block *t0blk =
    (t0est != 0) ? t0est->block : (t0ei != 0) ? t0ei->block : 0;
  if (t0blk == 0) {
    DBG_SUB(fprintf(stderr, "is_sub_type: t0blk null\n"));
    return false;
  }
  const expr_interface *const t1ei = dynamic_cast<const expr_interface *>(
    term_get_instance_const(t1));
  if (t1ei == 0) {
    return false;
  }
  return implements_interface(t0blk, t1ei);
  #if 0
  expr_telist *ih = tdef->block->inherit;
  while (ih != 0) {
    if (ih->head->get_sdef()->resolve_evaluated() == t1) {
      DBG_SUB(fprintf(stderr, "is_sub_type: inherit\n"));
      return true;
    }
    #if 0
    raise(SIGTRAP); // FIXME
    #endif
    DBG_SUB(fprintf(stderr, "is_sub_type: not inherit %s %s\n",
      term_tostr_human(ih->head->get_sdef()->resolve_evaluated()).c_str(),
      term_tostr_human(t1).c_str()));
    ih = ih->rest;
  }
  DBG_SUB(fprintf(stderr, "is_sub_type: none matched\n"));
  return false;
  #endif
}

static bool check_tvmap(tvmap_type& tvmap, const std::string& sym,
  const term& t)
{
  DBG_CONV(fprintf(stderr, "check_tvmap sym=%s t=%s\n",
    sym.c_str(), term_tostr_human(t).c_str()));
  tvmap_type::iterator i = tvmap.find(sym);
  if (i != tvmap.end()) {
    if (!is_same_type(i->second, t)) {
      return false;
    }
  } else {
    tvmap[sym] = t;
  }
  return true;
}

static bool unify_type(
  const term& t0 /* can have tparam */,
  const term& t1 /* no tparam */,
  tvmap_type& tvmap);

static bool unify_type_list(
  const term_list& tl0 /* can have tparam */,
  const term_list& tl1 /* no tparam */,
  tvmap_type& tvmap)
{
  #if 0
  DBG_CONV(fprintf(stderr, "unify_type_list: tl0=%p tl1=%p\n",
    term_tostr_list(tl0, term_tostr_sort_strict).c_str(),
    term_tostr_list(tparams, term_tostr_sort_strict).c_str()));
  #endif
  term_list::const_iterator i0= tl0.begin();
  term_list::const_iterator i1= tl1.begin();
  for (; i0 != tl0.end() && i1 != tl1.end(); ++i0, ++i1) {
    if (!unify_type(*i0, *i1, tvmap)) {
      return false;
    }
  }
  return i0 == tl0.end() && i1 == tl1.end();
}

static bool unify_type(
  const term& t0 /* can have tparam */,
  const term& t1 /* no tparam */,
  tvmap_type& tvmap)
{
  #if 0
  DBG_CONV(fprintf(stderr, "unify_template: tl0=%p tl1=%p\n",
    term_tostr_list(tl0, term_tostr_sort_strict).c_str(),
    term_tostr_list(tparams, term_tostr_sort_strict).c_str()));
  #endif
  if (t0.get_expr()->get_esort() == expr_e_tparams) {
    const expr_tparams *const t0tp = ptr_down_cast<const expr_tparams>(
      t0.get_expr()); /* outer tparam */
    return check_tvmap(tvmap, t0tp->sym, t1);
  } else if (t0.get_expr() == t1.get_expr()) {
    return unify_type_list(*t0.get_args(), *t1.get_args(), tvmap);
  }
  return false;
}

static int num_bits_min(const term& t)
{
  expr_typedef *const etd = dynamic_cast<expr_typedef *>(t.get_expr());
  if (etd == 0) {
    return 0;
  }
  return etd->tattr.significant_bits_min;
}

static int num_bits_max(const term& t)
{
  expr_typedef *const etd = dynamic_cast<expr_typedef *>(t.get_expr());
  if (etd == 0) {
    return 0;
  }
  return etd->tattr.significant_bits_max;
}

static bool numeric_convertible(expr_i *efrom, const term& tfrom,
  const term& tto)
{
  symbol_common *sdef = efrom->get_sdef();
  if (sdef != 0) {
    term& ev = sdef->resolve_evaluated();
    if (ev.is_long() && is_numeric_type(tto)) {
      return true; /* compile-time long constant to numeric */
    }
    if (ev.is_long() && ev.get_long() == 0 && is_bitmask(tto)) {
      return true; /* zero to bitmask */
    }
  }
  if (efrom->get_esort() == expr_e_int_literal) {
    if (is_integral_type(tto)) {
      return true; /* int literal to integral type */
    }
    expr_int_literal *eint = ptr_down_cast<expr_int_literal>(efrom);
    if (eint->get_signed() == 0 && is_bitmask(tto)) {
      return true; /* zero to bitmask */
    }
  }
  if (efrom->get_esort() == expr_e_float_literal && is_float_type(tto)) {
    return true; /* float literal to float type */
  }
  if (!is_numeric_type(tfrom) || !is_numeric_type(tto)) {
    return false;
  }
  if (tto == tfrom) {
    return true;
  }
  if (get_family(tfrom) != get_family(tto)) {
    return false; /* none(builtin), int, uint, enum, bitmask */
  }
  if (is_unsigned_integral_type(tfrom) != is_unsigned_integral_type(tto)) {
    return false; /* different signedness */
  }
  const typefamily_e cat = get_family(tfrom);
  if (cat != typefamily_e_none) {
    return false; /* user defined type */
  }
  /* builtin type */
  if (is_enum(tfrom) || is_bitmask(tfrom) || is_enum(tto) || is_bitmask(tto)) {
    return false;
  }
  if (is_integral_type(tfrom) != is_integral_type(tto)) {
    return false; /* integral <=> floating */
  }
  int to_min = num_bits_min(tto);
  int from_max = num_bits_max(tfrom);
  return to_min >= from_max;
}

bool convert_type(expr_i *efrom, term& tto, tvmap_type& tvmap)
{
  term& tfrom = efrom->resolve_texpr();
  DBG_CONV(fprintf(stderr, "convert efrom=%s from=%s to=%s\n",
    efrom->dump(0).c_str(),
    term_tostr_human(tfrom).c_str(),
    term_tostr_human(tto).c_str()));
  if (is_same_type(tfrom, tto)) {
    DBG_CONV(fprintf(stderr, "convert: same type\n"));
    return true;
  }
  if (unify_type(tto, tfrom, tvmap)) {
    /* type parameterized function */
    DBG_CONV(fprintf(stderr, "convert: unify\n"));
    return true;
  }
  DBG_CONV(fprintf(stderr, "P1\n"));
  term tconvto;
  expr_tparams *const tp = dynamic_cast<expr_tparams *>(tto.get_expr());
  if (tp != 0 && tvmap.find(tp->sym) != tvmap.end()) {
    /* tto is a type parameter and bound to tvmap[tto] */
    tconvto = tvmap[tp->sym];
  } else {
    tconvto = tto;
  }
  // TODO: enable this
  if (numeric_convertible(efrom, tfrom, tconvto)) {
  // if (is_numeric_type(tfrom) && is_numeric_type(tconvto)) {
    efrom->conv = conversion_e_cast;
    efrom->type_conv_to = tconvto;
    DBG_CONV(fprintf(stderr, "convert: numeric cast\n"));
    return true;
  }
  if (tfrom == builtins.type_strlit && tto == builtins.type_string) {
    DBG_CONV(fprintf(stderr, "convert: strlit to string\n"));
    efrom->conv = conversion_e_cast;
    efrom->type_conv_to = tconvto;
    return true;
  }
  #if 0
  if (is_string_type(tfrom)) {
    efrom->conv = conversion_e_from_string;
    efrom->type_conv_to = tconvto;
    DBG_CONV(fprintf(stderr, "convert: from string\n"));
    return true;
  }
  if (is_string_type(tconvto)) {
    efrom->conv = conversion_e_to_string;
    efrom->type_conv_to = tconvto;
    DBG_CONV(fprintf(stderr, "convert: to string\n"));
    return true;
  }
  #endif
  #if 0
  // TODO: disable auto boxing
  if (is_cm_pointer_family(tconvto) &&
    is_sub_type(tfrom, get_pointer_target(tconvto))) {
    /* auto boxing */
    efrom->conv = conversion_e_boxing;
    efrom->type_conv_to = tconvto;
    DBG_CONV(fprintf(stderr, "convert: auto boxing\n"));
    return true;
  }
  #endif
  if (is_cm_range_family(tconvto) && is_cm_range_family(tfrom) &&
    (is_const_range_family(tconvto) || !is_const_range_family(tfrom))) {
    /* range to crange */
    const term_list *const tta = tconvto.get_args();
    const term_list *const tfa = tfrom.get_args();
    if (tta != 0 && tfa != 0 && tta->front() == tfa->front()) {
      DBG_CONV(fprintf(stderr, "convert: range\n"));
      efrom->conv = conversion_e_subtype_ptr;
      efrom->type_conv_to = tconvto;
      return true;
    }
  }
  DBG_CONV(fprintf(stderr, "P5\n"));
  if (is_container_family(tfrom) && is_cm_range_family(tconvto)) {
    /* container to range */
    DBG_CONV(fprintf(stderr, "convert: range? %s -> %s\n",
      term_tostr_human(tfrom).c_str(), term_tostr_human(tconvto).c_str()));
    const term tra = get_array_range_texpr(0, efrom, tfrom);
    if (is_const_range_family(tra) && !is_const_range_family(tconvto)) {
      DBG_CONV(fprintf(stderr, "convert: auto range constness\n"));
      return false;
    }
    const term_list *const tta = tconvto.get_args();
    const term_list *const tfa = tra.get_args();
    if (tta != 0 && tfa != 0 && tta->front() == tfa->front()) {
      DBG_CONV(fprintf(stderr, "convert: auto container to range\n"));
      efrom->conv = conversion_e_container_range;
      efrom->type_conv_to = tconvto;
      return true;
    }
  }
  DBG_CONV(fprintf(stderr, "P6\n"));
  if (is_cm_range_family(tfrom) && is_container_family(tconvto) &&
    tconvto != builtins.type_strlit) {
    /* range to container */
    const term tra = get_array_range_texpr(0, 0, tconvto);
    const term_list *const tfa = tfrom.get_args();
    const term_list *const tta = tra.get_args();
    if (tta != 0 && tfa != 0 && tta->front() == tfa->front()) {
      DBG_CONV(fprintf(stderr, "convert: auto range to container\n"));
      // efrom->conv = conversion_e_subtype_obj; // FIXME: remove
      efrom->conv = conversion_e_cast;
      efrom->type_conv_to = tconvto;
      return true;
    }
  }
  DBG_CONV(fprintf(stderr, "P7\n"));
  if (is_cm_pointer_family(tconvto) && is_cm_pointer_family(tfrom) &&
    is_sub_type(get_pointer_target(tfrom), get_pointer_target(tconvto))
    // && is_same_threading_pointer(tfrom, tconvto)
    ) {
    /* ptr/iptr to cptr etc */
    DBG_CONV(fprintf(stderr, "convert: pointer\n"));
    const expr_struct *const es_to = dynamic_cast<const expr_struct *>(
      tconvto.get_expr());
    const expr_struct *const es_from = dynamic_cast<const expr_struct *>(
      tfrom.get_expr());
    typefamily_e cat_to = typefamily_e_none;
    typefamily_e cat_from = typefamily_e_none;
    if (es_to != 0) {
      cat_to = es_to->typefamily;
    }
    if (es_from != 0) {
      cat_from = es_from->typefamily;
    }
    if (pointer_conversion_allowed(cat_from, cat_to)) {
      DBG_CONV(fprintf(stderr, "convert: (struct) ptr\n"));
      efrom->conv = conversion_e_subtype_ptr;
      efrom->type_conv_to = tconvto;
      return true;
    }
  }
  DBG_CONV(fprintf(stderr, "P8\n"));
  if (is_sub_type(tfrom, tconvto)) {
    DBG_CONV(fprintf(stderr, "convert: sub type\n"));
    efrom->conv = conversion_e_subtype_obj;
    efrom->type_conv_to = tconvto;
    return true;
  }
  DBG_CONV(fprintf(stderr, "convert: failed\n"));
  return false;
}

static void execute_rec_incl_instances(expr_i *e, void (*f)(expr_i *))
{
  if (e == 0) {
    return;
  }
  f(e);
  expr_block *const bl = dynamic_cast<expr_block *>(e);
  if (bl != 0) {
    template_info& ti = bl->tinfo;
    for (template_info::instances_type::iterator i = ti.instances.begin();
      i != ti.instances.end(); ++i) {
      execute_rec_incl_instances(i->second->parent_expr, f);
    }
    if (ti.is_uninstantiated()) {
      /* don't check children of an uninstantiated block */
      return;
    }
  }
  const int num = e->get_num_children();
  for (int i = 0; i < num; ++i) {
    expr_i *c = e->get_child(i);
    execute_rec_incl_instances(c, f);
  }
}

#if 0
static void calc_callee_rec(expr_funcdef *efd, std::set<expr_funcdef *>& s,
  expr_funcdef::callee_vec_type& trv, expr_funcdef::callee_set_type& trs)
{
  if (s.find(efd) != s.end()) {
    return;
  }
  s.insert(efd);
  for (expr_funcdef::callee_vec_type::const_iterator i
    = efd->callee_vec.begin(); i != efd->callee_vec.end(); ++i) {
    calc_callee_rec(*i, s, trv, trs);
    if (trs.find(*i) == trs.end()) {
      trv.push_back(*i);
      trs.insert(*i);
      DBG_CALLEE(fprintf(stderr, "%s calls %s\n", efd->sym, (*i)->sym));
    }
  }
  s.erase(efd);
}

static void calc_callee_tr(expr_funcdef *efd)
{
  if (efd->callee_vec.empty() || !efd->callee_vec_tr.empty()) {
    /* already done */
  }
  /* TODO: too slow */
  std::set<expr_funcdef *> s;
  expr_funcdef::callee_vec_type trv;
  expr_funcdef::callee_set_type trs;
  calc_callee_rec(efd, s, trv, trs);
  efd->callee_vec_tr.swap(trv);
  efd->callee_set_tr.swap(trs);
}
#endif

static void check_template_upvalues_direct_one(expr_i *e)
{
  expr_funcdef *const efd = dynamic_cast<expr_funcdef *>(e);
  if (efd != 0) {
    add_tparam_upvalues_funcdef_direct(efd);
    #if 0
    calc_callee_tr(efd);
    #endif
  }
}

void fn_check_template_upvalues_direct(expr_i *e)
{
  execute_rec_incl_instances(e, check_template_upvalues_direct_one);
}

static void check_template_upvalues_tparam_one(expr_i *e)
{
  expr_funcdef *const efd = dynamic_cast<expr_funcdef *>(e);
  if (efd != 0) {
    add_tparam_upvalues_funcdef_tparam(efd);
  }
}

void fn_check_template_upvalues_tparam(expr_i *e)
{
  execute_rec_incl_instances(e, check_template_upvalues_tparam_one);
}

void fn_compile(expr_i *e, expr_i *p, bool is_template)
{
  if (e == 0) {
    return;
  }
  DBG_COMPILE(fprintf(stderr, "compile %p(%s:%d) begin\n", e, e->fname,
    e->line));
  fn_set_tree_and_define_static(e, p, p ? p->symtbl_lexical : 0, 0,
    is_template);
  arena_error_throw_pushed();
  fn_check_type(e, e->symtbl_lexical);
    /* calls fn_compile recursively if a template is instantiated */
  #if 0
  // move to expr.cpp
  fn_check_template_upvalues_direct(e);
  fn_check_template_upvalues_tparam(e);
  #endif
  arena_error_throw_pushed();
  DBG_COMPILE(fprintf(stderr, "compile %p(%s:%d) end\n", e, e->fname,
    e->line));
}

expr_i *fn_drop_non_exports(expr_i *e) {
  std::vector<expr_stmts *> sl;
  expr_stmts *st = ptr_down_cast<expr_stmts>(e);
  while (st != 0) {
    expr_i *const h = st->head;
    bool is_export = false;
    switch (h->get_esort()) {
    case expr_e_struct:
    case expr_e_dunion:
    case expr_e_interface:
    case expr_e_funcdef:
    case expr_e_typedef:
    case expr_e_metafdef:
    case expr_e_enumval:
    case expr_e_inline_c:
    case expr_e_var:
      is_export = true;
      break;
    default:
      is_export = false;
      break;
    }
    if (is_export) {
      sl.push_back(st);
    }
    st = st->rest;
  }
  if (sl.empty()) {
    return 0;
  }
  for (size_t i = 0; i + 1 < sl.size(); ++i) {
    sl[i]->rest = sl[i + 1];
  }
  sl[sl.size() - 1]->rest = 0;
  return sl[0];
}

symbol_table *get_current_frame_symtbl(symbol_table *cur)
{
  while (cur) {
    if (cur->block_esort != expr_e_block) {
      return cur; /* funcdef, struct, dunion, or interface */
    }
    cur = cur->get_lexical_parent();
  }
  return 0;
}

expr_i *get_current_frame_expr(symbol_table *cur)
{
  symbol_table *fr = get_current_frame_symtbl(cur);
  if (fr == 0) {
    return 0;
  }
  expr_i *e = fr->block_backref;
  while (e) {
    if (e->get_esort() == fr->block_esort) {
      break;
    }
    e = e->parent_expr;
  }
  return e;
}

expr_i *get_current_block_expr(symbol_table *cur)
{
  expr_i *e = cur->block_backref;
  while (e) {
    if (e->get_esort() == cur->block_esort) {
      break;
    }
    e = e->parent_expr;
  }
  return e;
}

static attribute_e get_threading_attribute_mask(expr_e e)
{
  if (is_type_esort(e)) {
    return attribute_e(attribute_threaded | attribute_multithr |
      attribute_valuetype | attribute_tsvaluetype);
  } else if (e == expr_e_funcdef) {
    return attribute_threaded;
  } else {
    return attribute_unknown;
  }
}

attribute_e get_expr_threading_attribute(const expr_i *e)
{
  if (e == 0) {
    return attribute_unknown;
  }
  const expr_i *eattr = e;
  const expr_funcdef *efd = dynamic_cast<const expr_funcdef *>(eattr);
  if (efd != 0) {
    const expr_struct *esd = efd->is_member_function();
    if (esd != 0) {
      eattr = esd;
    }
    const expr_interface *ei = efd->is_virtual_function();
    if (ei != 0) {
      eattr = ei;
    }
  }
  attribute_e attr = eattr->get_attribute();
  return attribute_e(attr & get_threading_attribute_mask(e->get_esort()));
}

attribute_e get_context_threading_attribute(symbol_table *cur)
{
  expr_i *const fe = get_current_frame_expr(cur);
  return get_expr_threading_attribute(fe);
}

bool is_threaded_context(symbol_table *cur)
{
  attribute_e attr = get_context_threading_attribute(cur);
  return (attr & attribute_threaded) != 0;
}

bool is_multithr_context(symbol_table *cur)
{
  attribute_e attr = get_context_threading_attribute(cur);
  return (attr & attribute_multithr) != 0;
}

attribute_e get_term_threading_attribute(const term& t)
{
  const expr_i *const e = t.get_expr();
  if (e == 0) {
    return attribute_unknown;
  }
  int attr = get_expr_threading_attribute(e);
  const term_list *const args = t.get_args();
  if (args == 0) {
    return attribute_e(attr);
  }
  for (term_list::const_iterator i = args->begin(); i != args->end(); ++i) {
    const expr_i *const ce = i->get_expr();
    if (ce == 0) {
      continue;
    }
    int mask = get_threading_attribute_mask(ce->get_esort());
    attr &= (get_term_threading_attribute(*i) | ~mask);
  }
  return attribute_e(attr);
}

#if 0
static bool check_term_threading_attribute(const term& t, bool req_multithr)
{
  const expr_i *const e = t.get_expr();
  if (e == 0) {
    return true;
  }
  const attribute_e attr = get_expr_threading_attribute(e);
  if ((attr & attribute_threaded) == 0) {
    DBG_THR(fprintf(stderr, "thr %s %s attr=%d 1\n",
      term_tostr_human(t).c_str(), e->dump(0).c_str(), (int)attr));
    return false;
  }
  if (req_multithr && is_type_esort(e->get_esort())) {
    if ((attr & attribute_multithr) == 0) {
      DBG_THR(fprintf(stderr, "thr %s 2\n", term_tostr_human(t).c_str()));
      return false;
    }
  }
  const term_list *const args = t.get_args();
  if (args == 0) {
    return true;
  }
  for (term_list::const_iterator i = args->begin(); i != args->end(); ++i) {
    if (!check_term_threading_attribute(*i, req_multithr)) {
      DBG_THR(fprintf(stderr, "thr %s 3\n", term_tostr_human(t).c_str()));
      return false;
    }
  }
  return true;
}

bool term_is_threaded(const term& t)
{
  return check_term_threading_attribute(t, false);
}

bool term_is_multithr(const term& t)
{
  return check_term_threading_attribute(t, true);
}
#endif

expr_funcdef *get_up_member_func(symbol_table *cur)
{
  expr_funcdef *efd = 0;
  while (cur) {
    if (cur->block_esort == expr_e_funcdef) {
      expr_i *e = cur->block_backref;
      while (e) {
	if (e->get_esort() == expr_e_funcdef) {
	  break;
	}
	e = e->parent_expr;
      }
      efd = ptr_down_cast<expr_funcdef>(e);
      if (efd->is_member_function()) { /* TMF OK */
	break;
      }
      efd = 0;
    }
    cur = cur->get_lexical_parent();
  }
  return efd;
}

#if 0
expr_i *get_up_frame(expr_funcdef *efd)
{
  symbol_table *cur = efd->block->symtbl.get_lexical_parent();
  while (cur) {
    if (cur->block_esort != expr_e_block) {
      return cur;
    }
    cur = cur->get_lexical_parent();
  }
  return 0;
}
#endif

std::string dump_expr(int indent, expr_i *e)
{
  if (e == 0) {
    return "";
  }
  return e->dump(indent);
}

void fn_append_coptions(expr_i *e, coptions& copt_apnd)
{
  if (e == 0) {
    return;
  }
  if (e->get_esort() == expr_e_inline_c) {
    /* FIXME */
    expr_inline_c *const ei = ptr_down_cast<expr_inline_c>(e);
    if (ei->posstr == "incdir") {
      copt_apnd.incdir.append_if(ei->cstr);
      DBG_COPT(fprintf(stderr, "[incdir %s]\n", ei->cstr));
    } else if (ei->posstr == "libdir") {
      copt_apnd.libdir.append_if(ei->cstr);
      DBG_COPT(fprintf(stderr, "[libdir %s]\n", ei->cstr));
    } else if (ei->posstr == "link") {
      copt_apnd.link.append_if(ei->cstr);
      DBG_COPT(fprintf(stderr, "[link %s]\n", ei->cstr));
    } else if (ei->posstr == "cflags") {
      copt_apnd.cflags.append_if(ei->cstr);
      DBG_COPT(fprintf(stderr, "[cflags %s]\n", ei->cstr));
    } else if (ei->posstr == "ldflags") {
      copt_apnd.ldflags.append_if(ei->cstr);
      DBG_COPT(fprintf(stderr, "[cflags %s]\n", ei->cstr));
    } else {
      DBG_COPT(fprintf(stderr, "[%s %s]\n", ei->posstr.c_str(), ei->cstr));
    }
  }
  const int num = e->get_num_children();
  for (int i = 0; i < num; ++i) {
    expr_i *c = e->get_child(i);
    fn_append_coptions(c, copt_apnd);
  }
}

void fn_set_namespace(expr_i *e, const std::string& uniqns,
  const std::string& injectns, int& block_id_ns)
{
  if (e == 0) {
    return;
  }
  e->set_unique_namespace_one(uniqns, injectns);
  expr_block *const bl = dynamic_cast<expr_block *>(e);
  if (bl != 0) {
    assert(compile_phase == 0);
      /* block_id_ns must be determined lexically. blocks and block_id_ns
       * values must not be generated in the middle of compilation, or it
       * would cause an inconsistency among compilation units. */
    bl->block_id_ns = ++block_id_ns;
  }
  const int num = e->get_num_children();
  for (int i = 0; i < num; ++i) {
    expr_i *c = e->get_child(i);
    fn_set_namespace(c, uniqns, injectns, block_id_ns);
  }
}

void fn_set_generated_code(expr_i *e)
{
  /* generated block has block_id_ns = 0, which is prohibited to be used
   * as a template parameter. generated variable is not allowed to be
   * used as an upvalue. */
  if (e == 0) {
    return;
  }
  e->generated_flag = true;
  expr_block *const bl = dynamic_cast<expr_block *>(e);
  if (bl != 0) {
    bl->block_id_ns = 0; /* mark as a generated block */
  }
  const int num = e->get_num_children();
  for (int i = 0; i < num; ++i) {
    expr_i *c = e->get_child(i);
    fn_set_generated_code(c);
  }
}

static void fn_check_syntax_one(expr_i *e)
{
  expr_stmts *stmts = 0;
  #if 0
  expr_struct *const est = dynamic_cast<expr_struct *>(e);
  if (est != 0) {
    stmts = est->block->stmts;
  }
  #endif
  expr_dunion *const ev = dynamic_cast<expr_dunion *>(e);
  if (ev != 0) {
    stmts = ev->block->stmts;
  }
  if (stmts != 0) {
    for ( ; stmts != 0; stmts = stmts->rest) {
      expr_i *const head = stmts->head;
      switch (head->get_esort()) {
      case expr_e_var:
      case expr_e_struct:
      case expr_e_interface:
      case expr_e_typedef:
      case expr_e_metafdef:
	break;
      case expr_e_funcdef:
	if (ev == 0) {
	  break;
	}
	/* FALLTHRU */
      default:
	arena_error_throw(head, "invalid statement");
	break;
      }
    }
  }
  expr_special *const esp = dynamic_cast<expr_special *>(e);
  if (esp != 0 && (esp->tok == TOK_CONTINUE || esp->tok == TOK_BREAK)) {
    /* must be in a loop */
    expr_i *p = esp->parent_expr;
    bool ok = false;
    while (p != 0) {
      switch (p->get_esort()) {
      case expr_e_funcdef:
	ok = false;
	p = 0;
	break;
      case expr_e_while:
      case expr_e_for:
      case expr_e_forrange:
      case expr_e_feach:
	ok = true;
	p = 0;
	break;
      default:
	break;
      }
      if (p != 0) {
	p = p->parent_expr;
      }
    }
    if (!ok) {
      arena_error_throw(esp, "%s statement not within loop",
	esp->tok == TOK_CONTINUE ? "continue" : "break");
    }
  }
}

void fn_set_tree_and_define_static(expr_i *e, expr_i *p, symbol_table *symtbl,
  expr_stmts *stmt, bool is_template_descent, bool is_expand_base)
{
  if (e == 0) {
    return;
  }
  e->parent_expr = p;
  e->symtbl_lexical = symtbl;
  DBG_SYMTBL(fprintf(stderr, "fn_set_tree_: set %p-> symtbl_lexical  = %p\n",
    e, symtbl));
  fn_check_syntax_one(e);
  if (!is_expand_base) {
    e->define_const_symbols_one();
    e->define_vars_one(stmt);
  }
  if (e->get_esort() == expr_e_block) {
    expr_block *const eb = ptr_down_cast<expr_block>(e);
    symtbl = &eb->symtbl;
    eb->tinfo.template_descent = is_template_descent;
  }
  expr_block *const ttbl = e->get_template_block();
  if (ttbl != 0 && ttbl->tinfo.has_tparams()) {
    /* has template params */
    is_template_descent = true;
  }
  if (e->get_esort() == expr_e_stmts) {
    stmt = ptr_down_cast<expr_stmts>(e);
  }
  expr_i *expand_base = 0;
  if (e->get_esort() == expr_e_expand) {
    expand_base = ptr_down_cast<expr_expand>(e)->baseexpr;
  }
  const int num = e->get_num_children();
  for (int i = 0; i < num; ++i) {
    expr_i *c = e->get_child(i);
    fn_set_tree_and_define_static(c, e, symtbl, stmt, is_template_descent,
      (is_expand_base || c == expand_base));
  }
}

void fn_update_tree(expr_i *e, expr_i *p, symbol_table *symtbl,
  const std::string& curns_u, const std::string& curns_i)
{
  if (e == 0) {
    return;
  }
  e->parent_expr = p;
  e->symtbl_lexical = symtbl;
  e->set_unique_namespace_one(curns_u, curns_i);
  DBG_SYMTBL(fprintf(stderr, "fn_update_tree: set %p-> symtbl_lexical  = %p\n",
    e, symtbl));
  if (e->get_esort() == expr_e_block) {
    expr_block *const eb = ptr_down_cast<expr_block>(e);
    symtbl = &eb->symtbl;
  }
  const int num = e->get_num_children();
  for (int i = 0; i < num; ++i) {
    expr_i *c = e->get_child(i);
    fn_update_tree(c, e, symtbl, curns_u, curns_i);
  }
}

#if 0
static void check_type_validity_common(expr_i *e, const term& t)
{
  const term_list *tl = t.get_args();
  if (tl == 0 || tl->empty())  {
    return;
  }
  for (term_list::const_iterator i = tl->begin(); i != tl->end(); ++i) {
    check_type_validity_common(e, *i);
  }
  if (!is_cm_pointer_family(t)) {
    return;
  }
  const expr_i *const tgt = tl->front().get_expr();
  if (tgt == 0) {
    arena_error_throw(e, "internal error (check_type_validity_common)");
  }
  const bool is_mtptr = is_multithreaded_pointer_family(t);
  const bool is_imptr = is_immutable_pointer_family(t);
  const attribute_e attr = tgt->get_attribute();
  #if 0
  fprintf(stderr, "ctvc %s mt=%d im=%d attr=%x\n", term_tostr_human(t).c_str(),
    (int)is_mtptr, (int)is_imptr, (int)attr); // FIXME  
  #endif
  if (is_mtptr && is_imptr && (attr & attribute_tsvaluetype) == 0) {
    arena_error_throw(e,
      "invalid type: '%s' (pointer target is not a tsvaluetype)",
      term_tostr_human(t).c_str());
  }
  if (is_imptr && (attr & attribute_valuetype) == 0) {
    arena_error_throw(e,
      "invalid type: '%s' (pointer target is not a valuetype)",
      term_tostr_human(t).c_str());
  }
  if (is_mtptr && (attr & attribute_multithr) == 0) {
    arena_error_throw(e,
      "invalid type: '%s' (pointer target is not a multithreaded type)",
      term_tostr_human(t).c_str());
  }
}

void fn_check_type(expr_i *e, symbol_table *symtbl)
{
  if (e == 0) {
    return;
  }
  DBG_CT(fprintf(stderr, "fn_check_type begin %p expr=%s\n",
    e, e->dump(0).c_str()));
  e->check_type(symtbl);
  check_type_validity_common(e, e->get_texpr());
  DBG_CT(fprintf(stderr, "fn_check_type end %p expr=%s -> type=%s\n",
    e, e->dump(0).c_str(),
    term_tostr(e->get_texpr(), term_tostr_sort_strict).c_str()));
}
#endif

template <typename T> T *find_parent_tmpl(T *e, expr_e t)
{
  while (e != 0) {
    if (e->get_esort() == t) {
      return e;
    }
    e = e->parent_expr;
  }
  return 0;
}

expr_i *find_parent(expr_i *e, expr_e t)
{
  return find_parent_tmpl(e, t);
  #if 0
  while (e != 0) {
    if (e->get_esort() == t) {
      return e;
    }
    e = e->parent_expr;
  }
  return 0;
  #endif
}

const expr_i *find_parent_const(const expr_i *e, expr_e t)
{
  return find_parent_tmpl(e, t);
}

symbol_table *find_current_symbol_table(expr_i *e)
{
  expr_i *ep = find_parent(e, expr_e_block);
  if (ep == 0) {
    return 0;
  }
  expr_block *const eb = ptr_down_cast<expr_block>(ep);
  return &eb->symtbl;
}

static void check_upvalue_funcobj(expr_i *e)
{
#if 0
  /* an upvalue function can be referred before defined only if the function
     is not a function object. */
  symbol_common *const sc = e->get_sdef();
  if (sc == 0) {
    return;
  }
  expr_funcdef *const ef = dynamic_cast<expr_funcdef *>(sc->symbol_def);
  if (ef == 0 || !ef->is_funcobj()) {
    return;
  }
  /* func is defined in the current func? */
  symbol_table *st = 0;
  for (st = sc->symtbl_defined; st != 0; st = st->get_lexical_parent()) {
    if (st->locals.find(sc->fullsym) != st->locals.end()) {
      return;
    }
    if (st->block_esort != expr_e_block) {
      break;
    }
  }
  if (st == 0) {
    return;
  }
  /* defined in the upvalue of the current frame? */
  if (st->upvalues.find(sc->fullsym) == st->upvalues.end()) {
    /* not found in the upvalues of the current frame. this means that
     * es->fullsym is defined after the current function is defined.
     * TODO: too tricky. */
    arena_error_push(e, "function closure '%s' is not yet defined",
      sc->fullsym.c_str());
  }
#endif
}

static void check_upvalue_memfunc(expr_i *e)
{
  #if 0
  /* TODO: not reached. 'calling a member function with an invalid context' */
  symbol_common *const sc = e->get_sdef();
  if (sc == 0) {
    return;
  }
  expr_funcdef *const ef = dynamic_cast<expr_funcdef *>(sc->symbol_def);
  if (ef == 0 || !ef->is_member_function()) {
    return;
  }
  symbol_table *frame_usefunc = get_current_frame_symtbl(sc->symtbl_defined);
  symbol_table *frame_usestruct = get_current_frame_symtbl(
    frame_usefunc->get_lexical_parent());
  symbol_table *frame_def = get_current_frame_symtbl(ef->symtbl_lexical);
  if (frame_usestruct != 0 && frame_def != 0 && frame_usestruct != frame_def) {
    arena_error_push(e, "cannot use a member function '%s' as an upvalue",
      sc->fullsym.c_str());
  }
  #endif
}

static bool check_function_argtypes(const expr_interface *ei,
  expr_funcdef *efd, expr_funcdef *efd_impl)
{
  if (!is_same_type(efd->get_rettype(), efd_impl->get_rettype())) {
    return false;
  }
  if (efd->is_const != efd_impl->is_const) {
    return false;
  }
  expr_argdecls *a0 = efd->block->get_argdecls();
  expr_argdecls *a1 = efd_impl->block->get_argdecls();
  while (a0 != 0 && a1 != 0) {
    if (!is_same_type(a0->resolve_texpr(), a1->resolve_texpr())) {
      return false;
    }
    if (a0->passby != a1->passby) {
      return false;
    }
    a0 = a0->get_rest();
    a1 = a1->get_rest();
  }
  return (a0 == 0 && a1 == 0);
}

static void check_interface_impl_one(expr_i *esub, expr_block *esub_block,
  expr_i *esuper)
{
  const expr_interface *const ei_super =
    dynamic_cast<const expr_interface *>(esuper);
  if (ei_super == 0) {
    arena_error_push(esub, "type '%s' is not an interface",
      term_tostr_human(esub->get_texpr()).c_str());
    return;
  }
  const symbol_table& symtbl = ei_super->block->symtbl;
  symbol_table::locals_type::const_iterator i, j;
  for (i = symtbl.locals.begin(); i != symtbl.locals.end(); ++i) {
    expr_funcdef *const efd = dynamic_cast<expr_funcdef *>(i->second.edef);
    if (efd == 0) {
      continue;
    }
    j = esub_block->symtbl.locals.find(i->first);
    if (esub->get_esort() == expr_e_struct) {
      if (j == esub_block->symtbl.locals.end() ||
	j->second.edef->get_esort() != expr_e_funcdef) {
	arena_error_push(esub, "method '%s' is not implemented",
	  i->first.c_str());
	continue;
      }
    } else if (esub->get_esort() == expr_e_interface) {
      if (j != esub_block->symtbl.locals.end()) {
	arena_error_push(esub, "can not override '%s'", i->first.c_str());
      }
    }
    if (j != esub_block->symtbl.locals.end()) {
      expr_funcdef *const efd_impl =
	ptr_down_cast<expr_funcdef>(j->second.edef);
      if (!check_function_argtypes(ei_super, efd, efd_impl)) {
	arena_error_push(efd_impl, "function type mismatch");
	continue;
      }
    }
  }
}

static void check_interface_impl(expr_i *e)
{
  expr_struct *const est = dynamic_cast<expr_struct *>(e);
  expr_interface *const ei = dynamic_cast<expr_interface *>(e);
  expr_block *block = 0;
  if (est != 0) {
    block = est->block;
  } else if (ei != 0) {
    block = ei->block;
  } else {
    return;
  }
  if (block->tinfo.is_uninstantiated()) {
    return;
  }
  expr_block::inherit_list_type::iterator i;
  for (i = block->inherit_transitive.begin();
    i != block->inherit_transitive.end(); ++i) {
    check_interface_impl_one(e, block, *i);
  }
}

static bool check_use_before_def_checkpoint(expr_i *e)
{
  expr_i *p = e->parent_expr;
  if (p == 0) {
    return false;
  }
  if (p->get_esort() == expr_e_stmts &&
    ptr_down_cast<expr_stmts>(p)->head == e) {
    return true;
  }
  /* if (...), etc. */
  switch (p->get_esort()) {
  case expr_e_if:
  case expr_e_while:
  case expr_e_for:
  case expr_e_forrange:
  case expr_e_feach:
  case expr_e_try:
    return true;
  default:
    break;
  }
  return false;
}

typedef std::map< expr_var *, std::pair<expr_i *, const expr_funcdef *> >
  varmap_type;
typedef std::set<expr_var *> vars_type;

static void check_use_before_def_one(varmap_type& uvars,
  vars_type& dvars_cur, expr_i *e)
{
  if (e == 0) {
    return;
  }
  if (e->get_esort() == expr_e_var) {
    dvars_cur.insert(ptr_down_cast<expr_var>(e));
  } else if (e->get_esort() == expr_e_funcdef ||
    e->get_esort() == expr_e_struct) {
    return;
  } else if (e->get_esort() == expr_e_funccall) {
    expr_funccall *const ef = ptr_down_cast<expr_funccall>(e);
    expr_i *fld = get_op_rhs(ef->func, '.');
      /* see expr_funccall::emit_value */
    if (fld == 0) {
      fld = get_op_rhs(ef->func, TOK_ARROW);
    }
    expr_i *const func = fld != 0 ? fld : ef->func; /* func to be called */
    symbol_common *const sdef = func->get_sdef();
    if (sdef != 0 && !sdef->get_evaluated_nochk().is_null()) {
      const expr_i *const inst = term_get_instance_const(
	sdef->get_evaluated_nochk());
      const expr_funcdef *const efd = dynamic_cast<const expr_funcdef *>(inst);
      if (efd != 0) {
	/* this funccall uses these variables as upvalues. they must be
	 * defined before the func is called. */
	expr_funcdef::tpup_vec_type::const_iterator i;
	for (i = efd->tpup_vec.begin(); i != efd->tpup_vec.end(); ++i) {
	  expr_var *const ev = dynamic_cast<expr_var *>(*i);
	  if (ev != 0 && uvars.find(ev) == uvars.end()) {
	    uvars[ev] = std::make_pair(e, efd);
	  }
	}
      }
    }
  } else {
    symbol_common *const sdef = e->get_sdef();
    if (sdef != 0) {
      expr_var *const ev = dynamic_cast<expr_var *>(sdef->get_symdef_nochk());
      if (ev != 0 && uvars.find(ev) == uvars.end()) {
	uvars[ev] = std::make_pair<expr_i *, expr_funcdef *>(e, 0);
      }
    }
  }
  const int num_children = e->get_num_children();
  for (int i = 0; i < num_children; ++i) {
    check_use_before_def_one(uvars, dvars_cur, e->get_child(i));
  }
  if (check_use_before_def_checkpoint(e)) {
    for (vars_type::const_iterator i = dvars_cur.begin(); i != dvars_cur.end();
      ++i) {
      if (uvars.find(*i) != uvars.end()) {
	expr_i *vu = uvars[*i].first;
	arena_error_push(vu, "variable '%s' used before defined", (*i)->sym);
	const expr_funcdef *ef = uvars[*i].second;
	if (ef != 0) {
	  arena_error_push(vu, "(used as an upvalue for '%s')", ef->sym);
	}
      }
    }
    dvars_cur.clear();
  }
}

// FIXME: correct?
static void check_use_before_def(expr_i *e)
{
  expr_block *const bl = dynamic_cast<expr_block *>(e);
  if (bl != 0) {
    varmap_type uvars;
    vars_type dvars_cur;
    check_use_before_def_one(uvars, dvars_cur, bl);
  }
}

static void check_type_threading_te(const term& te, expr_i *pos)
{
  /* check if tptr target is multithreaded */
  term rte;
  const expr_i *e = te.get_expr();
  const term_list *args = te.get_args();
  if (args != 0 && e != 0) {
    for (term_list::const_iterator i = args->begin(); i != args->end(); ++i) {
      check_type_threading_te(*i, pos);
    }
    const expr_struct *est = dynamic_cast<const expr_struct *>(e);
    if (est != 0 && est->typefamily != typefamily_e_none && !args->empty()) {
      const term& tgt = args->front();
      const attribute_e attr = get_term_threading_attribute(tgt);
      if (is_threaded_pointer_family(est->typefamily) &&
	(attr & attribute_multithr) == 0) {
	arena_error_throw(pos, "pointer target '%s' is not multithreaded",
	  term_tostr_human(tgt).c_str());
      }
      if (is_immutable_pointer_family(est->typefamily) &&
	(attr & attribute_valuetype) == 0) {
	arena_error_throw(pos, "pointer target '%s' is not valuetype",
	  term_tostr_human(tgt).c_str());
      }
      if (est->typefamily == typefamily_e_tiptr &&
	(attr & attribute_valuetype) == 0) {
	arena_error_throw(pos, "pointer target '%s' is not tsvaluetype",
	  term_tostr_human(tgt).c_str());
      }
    }
  }
}

static void check_type_threading(expr_i *e)
{
  term& te = e->resolve_texpr();
  check_type_threading_te(te, e);
}

void fn_check_final(expr_i *e)
{
  if (e == 0) {
    return;
  }
  check_type_threading(e);
  check_interface_impl(e);
  check_upvalue_funcobj(e);
  check_upvalue_memfunc(e);
  check_use_before_def(e);
  if (e->get_esort() == expr_e_funccall) {
    check_tpup_thisptr_constness(ptr_down_cast<expr_funccall>(e));
  }
  /* check instances */
  expr_block *const bl = dynamic_cast<expr_block *>(e);
  if (bl != 0) {
    template_info& ti = bl->tinfo;
    for (template_info::instances_type::iterator i = ti.instances.begin();
      i != ti.instances.end(); ++i) {
      fn_check_final(i->second->parent_expr);
    }
    if (ti.is_uninstantiated()) {
      /* don't check children of an uninstantiated block */
      return;
    }
  }
  expr_metafdef *const ma = dynamic_cast<expr_metafdef *>(e);
  if (ma != 0) {
    /* don't check macro body */
    return;
  }
  /* check children */
  const int num = e->get_num_children();
  for (int i = 0; i < num; ++i) {
    expr_i *c = e->get_child(i);
    fn_check_final(c);
  }
}

expr_i *get_op_lhs(expr_i *e, int op)
{
  if (e->get_esort() != expr_e_op) {
    return 0;
  }
  expr_op *const eop = ptr_down_cast<expr_op>(e);
  if (eop->op != op) {
    return 0;
  }
  return eop->arg0;
}

expr_i *get_op_rhs(expr_i *e, int op)
{
  if (e->get_esort() != expr_e_op) {
    return 0;
  }
  expr_op *const eop = ptr_down_cast<expr_op>(e);
  if (eop->op != op) {
    return 0;
  }
  return eop->arg1;
}

bool is_type_esort(expr_e e)
{
  switch (e) {
  case expr_e_typedef:
  case expr_e_metafdef:
  case expr_e_te:
  case expr_e_struct:
  case expr_e_dunion:
  case expr_e_interface:
  case expr_e_tparams:
    return true;
  default:
    break;
  }
  return false;
}

bool is_type_or_func_esort(expr_e e)
{
  switch (e) {
  case expr_e_typedef:
  case expr_e_metafdef:
  case expr_e_te:
  case expr_e_struct:
  case expr_e_dunion:
  case expr_e_interface:
  case expr_e_funcdef:
  case expr_e_tparams:
    return true;
  default:
    break;
  }
  return false;
}

bool is_global_var(const expr_i *e)
{
  const expr_var *const ev = dynamic_cast<const expr_var *>(e);
  if (ev == 0) {
    return false;
  }
  symbol_table *const stbl = ev->symtbl_lexical;
  return (stbl->get_lexical_parent() == 0);
}

bool is_ancestor_symtbl(symbol_table *st1, symbol_table *st2)
{
  while (st2 != 0) {
    if (st2 == st1) {
      DBG(fprintf(stderr, "is_ancestor_symtbl TRUE %p\n", st1));
      return true;
    }
    st2 = st2->get_lexical_parent();
  }
  DBG(fprintf(stderr, "is_ancestor_symtbl FALSE %p\n", st2));
  return false;
}

void check_inherit_threading(expr_i *einst, expr_block *block)
{
  const term& inst_te = einst->get_value_texpr();
  expr_telist *inh = block->inherit;
  for (; inh != 0; inh = inh->rest) {
    symbol_common *sd = inh->head->get_sdef();
    assert(sd);
    term& te = sd->resolve_evaluated();
    const attribute_e iattr = get_term_threading_attribute(te);
    const attribute_e eattr = einst->get_attribute();
    if ((iattr & eattr) != iattr) {
      arena_error_throw(einst,
        "type '%s' can't inherit '%s' because it has incompatible "
	"threading attribute",
        term_tostr_human(inst_te).c_str(), term_tostr_human(te).c_str());
    }
  }
}

bool is_passby_cm_value(passby_e passby)
{
  return passby == passby_e_const_value
    || passby == passby_e_mutable_value;
}

bool is_passby_cm_reference(passby_e passby)
{
  return passby == passby_e_const_reference
    || passby == passby_e_mutable_reference;
}

bool is_passby_const(passby_e passby)
{
  return passby == passby_e_const_value
    || passby == passby_e_const_reference;
}

bool is_passby_mutable(passby_e passby)
{
  return passby == passby_e_mutable_value
    || passby == passby_e_mutable_reference;
}

bool is_range_op(const expr_i *e)
{
  return (e != 0 && e->get_esort() == expr_e_op &&
    ptr_down_cast<const expr_op>(e)->op == TOK_SLICE);
}

bool expr_has_lvalue(const expr_i *epos, expr_i *a0, bool thro_flg)
{
  DBG_LV(fprintf(stderr, "expr_has_lvalue: expr=%s\n", a0->dump(0).c_str()));
  /* check if a0 is a valid lvalue expression */
  if (a0->conv != conversion_e_none && a0->conv != conversion_e_subtype_obj) {
    if (!thro_flg) {
      return false;
    }
    arena_error_push(a0, "can not be modified because of a conversion");
  }
  if (a0->get_sdef() != 0) {
    /* symbol or te */
    symbol_common *const sc = a0->get_sdef();
    const bool upvalue_flag = sc->upvalue_flag;
    term& tev = sc->resolve_evaluated();
    expr_i *const eev = term_get_instance(tev);
    expr_e astyp = eev != 0 ? eev->get_esort() : expr_e_metafdef /* dummy */;
    #if 0
    const expr_e astyp = sc->resolve_symdef()->get_esort();
    #endif
    if (astyp == expr_e_var) {
      if (upvalue_flag) {
	symbol_table *symtbl = find_current_symbol_table(eev);
	assert(symtbl);
	expr_block *bl = symtbl->block_backref;
	assert(bl);
	if (bl->parent_expr != 0 &&
	  bl->parent_expr->get_esort() == expr_e_struct) {
	  /* as->symbol_def is an instance variable */
	  expr_i *efd = a0;
	  while (efd->get_esort() != expr_e_funcdef) {
	    efd = efd->parent_expr;
	    assert(efd);
	  }
	  if (ptr_down_cast<expr_funcdef>(efd)->is_const) {
	    DBG_LV(fprintf(stderr, "expr_has_lvalue %s 1 false\n",
	      a0->dump(0).c_str()));
	    if (!thro_flg) {
	      return false;
	    }
	    arena_error_push(epos,
	      "field '%s' can not be modified from a const member function",
	      sc->fullsym.c_str());
	  }
	} else {
	  #if 0
	  arena_error_push(epos, "upvalue '%s' can not be modified",
	    sc->fullsym.c_str());
	  #endif
	}
      }
      expr_var *const ev = ptr_down_cast<expr_var>(eev);
      #if 0
      if (is_passby_const(ev->varinfo.passby)) {
      #endif
      if (!is_passby_mutable(ev->varinfo.passby)) {
	DBG_LV(fprintf(stderr, "expr_has_lvalue %s 1.1 false\n",
	  a0->dump(0).c_str()));
	if (!thro_flg) {
	  return false;
	}
	arena_error_push(epos, "variable '%s' can not be modified",
	  sc->fullsym.c_str());
      }
    } else if (astyp == expr_e_argdecls) {
      expr_argdecls *const ead = ptr_down_cast<expr_argdecls>(eev);
      if (!is_passby_mutable(ead->passby)) {
	DBG_LV(fprintf(stderr, "expr_has_lvalue %s 2 false\n",
	  a0->dump(0).c_str()));
	if (!thro_flg) {
	  return false;
	}
	arena_error_push(epos, "argument '%s' can not be modified",
	  sc->fullsym.c_str());
      }
    } else {
      DBG_LV(fprintf(stderr, "expr_has_lvalue %s 3 false\n",
	a0->dump(0).c_str()));
      if (!thro_flg) {
	return false;
      }
      arena_error_push(epos, "symbol '%s' can not be modified",
	sc->fullsym.c_str());
    }
    DBG_LV(fprintf(stderr, "expr_has_lvalue %s 4 true\n",
      a0->dump(0).c_str()));
    return true;
  }
  if (a0->get_esort() == expr_e_op) {
    const expr_op *const aop = ptr_down_cast<const expr_op>(a0);
    switch (aop->op) {
    case '.':
    case TOK_ARROW:
      /* expr 'foo.bar' has an lvalue iff foo has an lvalue */
      return expr_has_lvalue(aop, aop->arg0, thro_flg);
    case '[':
      /* expr 'foo[]' has an lvalue iff foo has an lvalue */
      DBG_LV(fprintf(stderr, "expr_has_lvalue: op[] 0\n"));
      if (is_range_op(aop->arg1)) {
	DBG_LV(fprintf(stderr, "expr_has_lvalue %s 5 false\n",
	  a0->dump(0).c_str()));
	if (!thro_flg) {
	  return false;
	}
	arena_error_push(epos, "range expression can not be modified");
      } else {
	DBG_LV(fprintf(stderr, "expr_has_lvalue: op[] 0-1\n"));
	const term tc = aop->arg0->resolve_texpr();
	if (is_const_range_family(tc)) {
	  DBG_LV(fprintf(stderr, "expr_has_lvalue %s 6 false\n",
	    a0->dump(0).c_str()));
	  if (!thro_flg) {
	    return false;
	  }
	  arena_error_push(epos, "const range element can not be modified");
	} else if (is_cm_range_family(tc)) {
	  DBG_LV(fprintf(stderr, "expr_has_lvalue %s 7 true\n",
	    a0->dump(0).c_str()));
	  return true;
	} else {
	  return expr_has_lvalue(aop, aop->arg0, thro_flg);
	}
      }
    case '(':
      /* expr '(foo)' has an lvalue iff foo has an lvalue */
      return expr_has_lvalue(aop, aop->arg0, thro_flg);
    case TOK_PTR_DEREF:
      #if 0
      /* expr '*foo' has an lvalue iff foo has an lvalue */
      /* enable this? constness should be transitive? */
      /* if so, we need to reject copying ptr{foo} if rhs has no lvalue */
      if (!expr_has_lvalue(aop, aop->arg0, thro_flg)) {
	return false;
      }
      #endif
      /* expr '*foo' has an lvalue iff foo is a non-const ptr */
      if (is_const_or_immutable_pointer_family(aop->arg0->resolve_texpr())) {
	DBG_LV(fprintf(stderr, "expr_has_lvalue %s 8 false\n",
	  a0->dump(0).c_str()));
	if (!thro_flg) {
	  return false;
	}
	arena_error_push(epos, "can not modify data via a const reference");
      }
      DBG_LV(fprintf(stderr, "expr_has_lvalue %s 9 true\n",
	a0->dump(0).c_str()));
      return true; /* ok */
    default:
      break;
    }
    DBG_LV(fprintf(stderr, "expr_has_lvalue %s 10 false\n",
      a0->dump(0).c_str()));
    if (!thro_flg) {
      return false;
    }
    arena_error_push(epos, "can not be modified"); // FIXME: error message
    return false;
  }
  if (a0->get_esort() == expr_e_var) {
    /* var definition expr */
    DBG_LV(fprintf(stderr, "expr_has_lvalue %s 11 true\n",
      a0->dump(0).c_str()));
    return true; /* ok */
  }
  if (a0->get_esort() == expr_e_funccall) {
    expr_funccall *const fc = ptr_down_cast<expr_funccall>(a0);
    bool dmy = false;
    term te = get_func_te_for_funccall(fc->func, dmy);
    expr_funcdef *const efd = dynamic_cast<expr_funcdef *>(
      term_get_instance(te));
    if (efd != 0 && efd->block->ret_passby == passby_e_mutable_reference) {
      return true;
    }
  }
  DBG_LV(fprintf(stderr, "expr_has_lvalue %s 12 false\n",
    a0->dump(0).c_str()));
  if (!thro_flg) {
    return false;
  }
  arena_error_push(epos, "can not be modified"); // FIXME: error message
  return false;
}

term get_pointer_deref_texpr(expr_op *eop, const term& t)
{
  const term tg = get_pointer_target(t);
  if (tg.is_null()) {
    arena_error_throw(eop, "invalid dereference");
    return builtins.type_void;
  }
  return tg;
}

term get_array_range_texpr(expr_op *eop, expr_i *ec /* nullable */,
  const term& ect)
{
  bool nonconst = false;
  if (ec != 0 && expr_has_lvalue(ec, ec, false)) {
    expr_has_lvalue(ec, ec, true);
    nonconst = true;
  }
  term slt = eval_local_lookup(ect,
    nonconst ? "range_type" : "crange_type", eop);
  if (slt.is_null()) {
    arena_error_throw(eop, "cannot apply '[ .. ]'");
    return builtins.type_void;
  }
  DBG_SLICE(fprintf(stderr, "range: %s %s -> %s\n",
    ec != 0 ? ec->dump(0).c_str() : "",
    term_tostr_human(ect).c_str(), term_tostr_human(slt).c_str()));
  return slt;
}

term get_array_elem_texpr(expr_op *eop, const term& t0)
{
  if (t0 == builtins.type_string || t0 == builtins.type_strlit) {
    return builtins.type_uchar;
  }
  /* extern struct? */
  const expr_i *const einst = term_get_instance_const(t0);
  const expr_struct *const est = dynamic_cast<const expr_struct *>(einst);
  if (est != 0) {
    const expr_tparams *tparams = est->block->tinfo.tparams;
    if (tparams != 0) {
      switch (est->typefamily) {
      case typefamily_e_varray:
      case typefamily_e_darray:
      case typefamily_e_farray:
      case typefamily_e_slice:
      case typefamily_e_cslice:
	return tparams->param_def;
      case typefamily_e_tree_map:
      case typefamily_e_tree_map_range:
      case typefamily_e_tree_map_crange:
	if (tparams->rest != 0) {
	  return tparams->rest->param_def;
	}
	break;
      default:
	break;
      }
      #if 0
      if (std::string(est->family) == "varray") {
	return tparams->param_def;
      } else if (std::string(est->family) == "farray") {
	return tparams->param_def;
      } else if (std::string(est->family) == "slice") {
	return tparams->param_def;
      } else if (std::string(est->family) == "cslice") {
	return tparams->param_def;
      } else if (std::string(est->family) == "tree_map") {
	if (tparams->rest != 0) {
	  return tparams->rest->param_def;
	}
      }
      #endif
    }
  }
  arena_error_throw(eop, "cannot apply '[]'");
  return builtins.type_void;
}

term get_array_index_texpr(expr_op *eop, const term& t0)
{
  if (t0 == builtins.type_string || t0 == builtins.type_strlit) {
    return builtins.type_size_t;
  }
  /* extern struct? */
  const expr_i *const einst = term_get_instance_const(t0);
  const expr_struct *const est = dynamic_cast<const expr_struct *>(einst);
  if (est != 0) {
    const expr_tparams *tparams = est->block->tinfo.tparams;
    if (tparams != 0) {
      switch (est->typefamily) {
      case typefamily_e_varray:
      case typefamily_e_darray:
      case typefamily_e_farray:
      case typefamily_e_slice:
      case typefamily_e_cslice:
	{
	  term t = eval_local_lookup(t0, "key_type", eop);
	  if (t.is_null()) {
	    arena_error_throw(eop, "key_type not found");
	    return builtins.type_void;
	  }
	  return t;
	}
      case typefamily_e_tree_map:
      case typefamily_e_tree_map_range:
	if (tparams->rest != 0) {
	  return tparams->param_def;
	}
	break;
      default:
	break;
      }
    }
  }
  arena_error_throw(eop, "cannot apply '[]'");
  return builtins.type_void;
}

bool is_vardef_constructor_or_byref(expr_i *e, bool incl_byref)
{
  expr_op *const eop = ptr_down_cast<expr_op>(e);
  if (eop == 0) {
    return false;
  }
  if (eop->op != '=' || eop->arg0->get_esort() != expr_e_var) {
    return false;
  }
  if (incl_byref) {
    expr_var *evar = ptr_down_cast<expr_var>(eop->arg0);
    if (is_passby_cm_reference(evar->varinfo.passby)) {
      return true;
    }
  }
  expr_funccall *const fc = dynamic_cast<expr_funccall *>(eop->arg1);
  if (fc == 0 || fc->conv != conversion_e_none) {
    return false;
  }
  if (fc->funccall_sort == funccall_e_struct_constructor) {
    return true;
  }
  return false;
}

static bool has_blockscope_tempvar(expr_i *e)
{
  if (e == 0) {
    return false;
  }
  if (e->tempvar_id >= 0 && e->tempvar_varinfo.scope_block) {
    return true;
  }
  const int n = e->get_num_children();
  for (int i = 0; i < n; ++i) {
    expr_i *const c = e->get_child(i);
    if (has_blockscope_tempvar(c)) {
      return true;
    }
  }
  return false;
}

static bool is_vardefset(expr_i *e)
{
  if (e->get_esort() == expr_e_op) {
    expr_op *const eop = ptr_down_cast<expr_op>(e);
    if (eop->op == '=' && eop->arg0->get_esort() == expr_e_var) {
      /* var x = ... */
      return true;
    }
  }
  return false;
}

static bool is_vardefdefault(expr_i *e)
{
  if (e->get_esort() == expr_e_var) {
    expr_op *const eop = dynamic_cast<expr_op *>(e->parent_expr);
    if (eop == 0 || eop->op != '=') {
      /* var x but not var x = ... */
      return true;
    }
  }
  return false;
}

bool is_vardef_or_vardefset(expr_i *e)
{
  return is_vardefdefault(e) || is_vardefset(e);
#if 0
  const bool r = is_vardefdefault(e) || is_vardefset(e);
  if (!r) {
    return false;
  }
  if (has_blockscope_tempvar(e)) {
    /* impossible for udcon because structs can not have ephemeral fields. */
    abort();
    return false;
  }
  return true;
#endif
}

bool is_noexec_expr(expr_i *e)
{ 
  switch (e->get_esort()) {
  case expr_e_typedef:
  case expr_e_metafdef:
  case expr_e_ns:
  case expr_e_inline_c:
  case expr_e_enumval:
  case expr_e_struct:
  case expr_e_dunion:
  case expr_e_interface:
  case expr_e_funcdef:
    return true;
  default:
    /* expr_e_if, expr_e_var, expr_e_op for example */
    return false;
  }
}

#if 0
static bool is_expand_dummy(const expr_block *bl)
{
  expr_i *p = bl->parent_expr;
  while (true) {
    if (p == 0) {
      return false;
    }
    if (p->get_esort() == expr_e_expand) {
      return true;
    }
    p = p->parent_expr;
  }
}
#endif

bool is_compiled(const expr_block *bl)
{
  // return !bl->tinfo.is_uninstantiated() && bl->symtbl_lexical != 0;
  return !bl->tinfo.is_uninstantiated() /* && !is_expand_dummy(bl) */
    && bl->compiled_flag;
}

}; 

