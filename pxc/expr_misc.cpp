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
#define DBG_DEPUP(x)
#define DBG_UBD(x)

namespace pxc {

/* begin: global variables */
/* note: arena_clear() must reset these vairables */
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
expr_i *compiling_stmt = 0;
const std::map<std::string, std::string> *cur_profile = 0;
bool compile_mode_generate_single_cc = true;
size_t recursion_limit = 3000;
nsimports_type nsimports;
nsimports_rec_type nsimports_rec;
nsaliases_type nsaliases;
nschains_type nschains;
nsextends_type nsextends;
nspropmap_type nspropmap;
nsthrmap_type nsthrmap;
std::string emit_threaded_dll_func;
compiled_ns_type compiled_ns;
/* end: global variables */

static expr_i *string_to_nssym(expr_i *e, const std::string& str)
{
  expr_i *prefix = 0;
  std::string::size_type pos = str.rfind(':');
  std::string sym;
  if (pos != str.npos) {
    if (pos == 0 || str[pos - 1] != ':') {
      return 0;
    }
    prefix = string_to_nssym(e, str.substr(0, pos - 1));
    sym = str.substr(pos + 1);
  } else {
    sym = str;
  }
  for (size_t i = 0; i < sym.size(); ++i) {
    char const ch = sym[i];
    if (ch >= '0' && ch <= '9') {
      /* digit */
    } else if (ch >= 'a' && ch <= 'z') {
      /* small */
    } else if (ch >= 'A' && ch <= 'Z') {
      /* capital */
    } else if (ch == '_') {
      /* underline */
    } else {
      return 0; /* invalid char */
    }
  }
  return expr_nssym_new(e->fname, e->line, prefix, arena_strdup(sym.c_str()));
}

static expr_i *string_to_telist_internal(expr_i *e, const std::string& str,
  size_t& pos);

static expr_i *string_to_te_internal(expr_i *e, const std::string& str,
  size_t& pos)
{
  const size_t sz = str.size();
  size_t i = pos;
  char ch = 0;
  for (; i < sz; ++i) {
    ch = str[i];
    if (ch == '{' || ch == '}' || ch == ',') {
      break;
    }
  }
  if (i == pos) {
    arena_error_throw(e, "Invalid expression '%s' at offset %zu", str.c_str(),
      i);
  }
  const std::string tok = str.substr(pos, i - pos);
  expr_i *targs = 0;
  if (i != sz) {
    if (ch == '{') {
      size_t apos = i + 1;
      targs = string_to_telist_internal(e, str, apos);
      if (apos < sz && str[apos] == '}') {
	i = apos + 1;
      } else {
	arena_error_throw(e, "Invalid expression '%s' at offset %zu",
	  str.c_str(), apos);
      }
    }
  }
  expr_i *nssym = string_to_nssym(e, tok);
  if (nssym == 0) {
    arena_error_throw(e, "Invalid expression '%s' at offset %zu",
      str.c_str(), pos);
  }
  pos = i;
  return expr_te_new(e->fname, e->line, nssym, targs);
}

static expr_i *string_to_telist_internal(expr_i *e, const std::string& str,
  size_t& pos)
{
  const size_t sz = str.size();
  size_t i = pos;
  if (i < sz && str[i] == '}') {
    return 0;
  }
  expr_i *head = string_to_te_internal(e, str, i);
  expr_i *rest = 0;
  if (i < sz && str[i] == ',') {
    ++i;
    rest = string_to_telist_internal(e, str, i);
  }
  return expr_telist_new(e->fname, e->line, head, rest);
}

expr_i *string_to_te(expr_i *e, const std::string& str)
{
  size_t pos = 0;
  expr_i *r = string_to_te_internal(e, str, pos);
  if (pos != str.size()) {
    arena_error_throw(e, "Invalid expression '%s' at offset %zu",
      str.c_str(), pos);
  }
  return r;
}

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
  case TOK_SHIFTL_ASSIGN: return "<<=";
  case TOK_SHIFTR_ASSIGN: return ">>=";
  case TOK_CASE: return "case";
  case TOK_SLICE: return "::";
  case TOK_EXPAND: return "expand";
  default:
    /* arena_error_throw(e, "Internal error: unknown token %d", tok); */
    return "unknown-token";
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

symbol to_short_name(const symbol& fullname)
{
  return to_short_name(fullname.to_string()); /* TODO */
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

symbol get_namespace_part(const symbol& fullname)
{
  return get_namespace_part(fullname.to_string()); /* TODO */
}

bool has_namespace(const std::string& name)
{
  const std::string::size_type tp = name.find('<');
  const std::string::size_type pos = name.rfind(':', tp);
  return pos != name.npos;
}

bool has_namespace(const symbol& name)
{
  return has_namespace(name.to_string());
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

bool is_boolean_type(const term& t)
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
    return true;
  }
  if (is_simple_string_type(t)) {
    return true; /* memcmp-compatible */
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
  if (est != 0 && est->cnamei.cname != 0) {
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

static bool is_simple_array_type(const term& t)
{
  if (t == builtins.type_strlit) {
    return true;
  }
  const expr_struct *const est = dynamic_cast<expr_struct *>(t.get_expr());
  if (est != 0) {
    const std::string s(est->sym);
    const std::string ns(est->get_unique_namespace());
    if (ns == "container::array" && (
      s == "varray" || s == "cvarray" ||
      s == "darray" || s == "cdarray" ||
      s == "darrayst" || s == "cdarrayst" ||
      s == "farray" || s == "cfarray" ||
      s == "slice" || s == "cslice")) {
      return true;
    }
  }
  #if 0
  fprintf(stderr, "is_vector_type %s false\n", term_tostr_human(t).c_str());
  #endif
  return false;
}

bool is_simple_string_type(const term& t)
{
  if (is_simple_array_type(t)) {
    const term& et = get_array_elem_texpr(0, t);
    if (et == builtins.type_uchar) {
      return true;
    }
  }
  return false;
}

const type_attribute *get_type_attribute(const term& t)
{
  const expr_i *const e = t.get_expr();
  if (e != 0 && e->get_esort() == expr_e_typedef) {
    const expr_typedef *const etd = ptr_down_cast<const expr_typedef>(e);
    return &etd->tattr;
  }
  return 0;
}

static bool is_pod_integral_type(const term& t)
{
  const type_attribute *const tattr = get_type_attribute(t);
  if (tattr != 0 && tattr->is_integral) {
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

unsigned long long uintegral_max_value(const term& t)
{
  const type_attribute *const tattr = get_type_attribute(t);
  if (tattr != 0 && tattr->is_integral && !tattr->is_signed) {
    if (tattr->significant_bits_min == tattr->significant_bits_max) {
      unsigned long long v = 1ULL;
      v <<= static_cast<unsigned>(tattr->significant_bits_max);
      --v;
      return v;
    }
  }
  return 0;
}

long long integral_max_value(const term& t)
{
  const type_attribute *const tattr = get_type_attribute(t);
  if (tattr != 0 && tattr->is_integral && tattr->is_signed) {
    if (tattr->significant_bits_min == tattr->significant_bits_max) {
      unsigned long long v = 1ULL;
      v <<= static_cast<unsigned>(tattr->significant_bits_max);
      --v;
      return v;
    }
  }
  return 0;
}

long long integral_min_value(const term& t)
{
  long long v = integral_max_value(t);
  if (v != 0LL) {
    v = -v;
    --v;
  }
  return v;
}

bool is_unsigned_integral_type(const term& t)
{
  const type_attribute *const tattr = get_type_attribute(t);
  if (tattr != 0 && tattr->is_integral && !tattr->is_signed) {
    return true;
  }
  const typefamily_e cat = get_family(t);
  return cat == typefamily_e_extuint;
}

bool is_float_type(const term& t)
{
  if (t == builtins.type_double || t == builtins.type_float) {
    return true;
  }
  const typefamily_e cat = get_family(t);
  if (cat == typefamily_e_extfloat) {
    return true;
  }
  return false;
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
  if (s == "lock_guard") return typefamily_e_lock_guard;
  if (s == "lock_cguard") return typefamily_e_lock_cguard;
  if (s == "extint") return typefamily_e_extint;
  if (s == "extuint") return typefamily_e_extuint;
  if (s == "extenum") return typefamily_e_extenum;
  if (s == "extbitmask") return typefamily_e_extbitmask;
  if (s == "extfloat") return typefamily_e_extfloat;
  if (s == "extnumeric") return typefamily_e_extnumeric;
  if (s == "varray") return typefamily_e_varray;
  if (s == "cvarray") return typefamily_e_cvarray;
  if (s == "darray") return typefamily_e_darray;
  if (s == "cdarray") return typefamily_e_cdarray;
  if (s == "darrayst") return typefamily_e_darrayst;
  if (s == "cdarrayst") return typefamily_e_cdarrayst;
  if (s == "farray") return typefamily_e_farray;
  if (s == "cfarray") return typefamily_e_cfarray;
  if (s == "slice") return typefamily_e_slice;
  if (s == "cslice") return typefamily_e_cslice;
  if (s == "ephemeral") return typefamily_e_ephemeral;
  if (s == "map") return typefamily_e_map;
  if (s == "cmap") return typefamily_e_cmap;
  if (s == "map_range") return typefamily_e_map_range;
  if (s == "map_crange") return typefamily_e_map_crange;
  if (s == "linear") return typefamily_e_linear;
  if (s == "noncopyable") return typefamily_e_noncopyable;
  if (s == "nodefault") return typefamily_e_nodefault;
  if (s == "nocascade") return typefamily_e_nocascade;
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
  case typefamily_e_lock_guard: return "lock_guard";
  case typefamily_e_lock_cguard: return "lock_cguard";
  case typefamily_e_extint: return "extint";
  case typefamily_e_extuint: return "extuint";
  case typefamily_e_extenum: return "extenum";
  case typefamily_e_extbitmask: return "extbitmask";
  case typefamily_e_extfloat: return "extfloat";
  case typefamily_e_extnumeric: return "extnumeric";
  case typefamily_e_varray: return "varray";
  case typefamily_e_cvarray: return "cvarray";
  case typefamily_e_darray: return "darray";
  case typefamily_e_cdarray: return "cdarray";
  case typefamily_e_darrayst: return "darrayst";
  case typefamily_e_cdarrayst: return "cdarrayst";
  case typefamily_e_farray: return "farray";
  case typefamily_e_cfarray: return "cfarray";
  case typefamily_e_slice: return "slice";
  case typefamily_e_cslice: return "cslice";
  case typefamily_e_ephemeral: return "ephemeral";
  case typefamily_e_map: return "map";
  case typefamily_e_cmap: return "cmap";
  case typefamily_e_map_range: return "map_range";
  case typefamily_e_map_crange: return "map_crange";
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
  case typefamily_e_lock_guard:
  case typefamily_e_lock_cguard:
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
      || to == typefamily_e_lock_guard || to == typefamily_e_lock_cguard;
  case typefamily_e_tcptr:
    return to == from || to == typefamily_e_lock_cguard;
  case typefamily_e_tiptr:
    return to == from;
  case typefamily_e_lock_guard:
    return false;
  case typefamily_e_lock_cguard:
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
    cat == typefamily_e_tiptr || cat == typefamily_e_iptr;
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

bool is_cm_lock_guard_family(const term& t)
{
  const typefamily_e cat = get_family(t);
  return cat == typefamily_e_lock_guard || cat == typefamily_e_lock_cguard;
}

static bool is_cm_lock_guard_family(expr_i *e)
{
  const typefamily_e cat = get_family(e);
  return cat == typefamily_e_lock_guard || cat == typefamily_e_lock_cguard;
}

bool is_array_family(const term& t)
{
  const typefamily_e cat = get_family(t);
  return
    cat == typefamily_e_varray || cat == typefamily_e_cvarray ||
    cat == typefamily_e_darray || cat == typefamily_e_cdarray ||
    cat == typefamily_e_darrayst || cat == typefamily_e_cdarrayst ||
    cat == typefamily_e_farray || cat == typefamily_e_cfarray;
}

bool is_const_array_family(const term& t)
{
  const typefamily_e cat = get_family(t);
  return
    cat == typefamily_e_cvarray ||
    cat == typefamily_e_cdarray ||
    cat == typefamily_e_cdarrayst ||
    cat == typefamily_e_cfarray;
}

bool is_cm_slice_family(const term& t)
{
  const typefamily_e cat = get_family(t);
  return cat == typefamily_e_slice || cat == typefamily_e_cslice;
}

bool is_map_family(const term& t)
{
  const typefamily_e cat = get_family(t);
  return cat == typefamily_e_map || cat == typefamily_e_cmap;
}

bool is_const_map_family(const term& t)
{
  const typefamily_e cat = get_family(t);
  return cat == typefamily_e_cmap;
}

bool is_cm_maprange_family(const term& t)
{
  const typefamily_e cat = get_family(t);
  return cat == typefamily_e_map_range ||
    cat == typefamily_e_map_crange;
}

bool is_const_range_family(const term& t)
{
  const typefamily_e cat = get_family(t);
  return cat == typefamily_e_cslice || cat == typefamily_e_map_crange;
}

static bool is_cm_range_family(expr_i *e)
{
  const typefamily_e cat = get_family(e);
  return cat == typefamily_e_slice || cat == typefamily_e_cslice ||
    cat == typefamily_e_map_range || cat == typefamily_e_map_crange;
}

bool is_cm_range_family(const term& t)
{
  return is_cm_range_family(t.get_expr());
}

static bool is_range_guard_ephemeral(expr_i *e)
{
  const typefamily_e cat = get_family(e);
  return is_cm_range_family(e) || cat == typefamily_e_ephemeral
    || is_cm_lock_guard_family(e);
}

bool is_noheap_type(const term& t)
{
  expr_i *const e = t.get_expr();
  if (e == 0) {
    return false;
  }
  if (is_range_guard_ephemeral(e)) {
    return true;
  }
  const typefamily_e cat = get_family(e);
  if (cat == typefamily_e_darrayst || cat == typefamily_e_cdarrayst) {
    return true;
  }
  #if 0
  if (is_cm_range_family(e)) {
    return true;
  }
  if (is_cm_lock_guard_family(e)) {
    return true;
  }
  const typefamily_e cat = get_family(e);
  if (cat == typefamily_e_ephemeral) {
    return true;
  }
  #endif
  if (is_container_family(t)) {
    const term_list *const targs = t.get_args();
    if (targs != 0) {
      for (term_list::const_iterator i = targs->begin(); i != targs->end();
	++i) {
	if (is_noheap_type(*i)) {
	  return true;
	}
      }
    }
  }
  return false;
}

bool is_container_family(const term& t)
{
  return is_array_family(t) || is_map_family(t);
}

bool is_const_container_family(const term& t)
{
  return is_const_array_family(t) || is_const_map_family(t);
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
      if (est->typefamily == typefamily_e_farray ||
	est->typefamily == typefamily_e_cfarray) {
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

bool type_has_refguard(const term& t)
{
  const typefamily_e cat = get_family(t);
  if (
    cat == typefamily_e_varray ||
    cat == typefamily_e_cvarray ||
    cat == typefamily_e_map ||
    cat == typefamily_e_cmap ||
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
  case typefamily_e_cvarray:
  case typefamily_e_darray:
  case typefamily_e_cdarray:
  case typefamily_e_darrayst:
  case typefamily_e_cdarrayst:
  case typefamily_e_farray:
  case typefamily_e_cfarray:
  case typefamily_e_map:
  case typefamily_e_cmap:
  case typefamily_e_slice:
  case typefamily_e_cslice:
  case typefamily_e_map_range:
  case typefamily_e_map_crange:
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
  if (is_cm_lock_guard_family(e)) {
    return false;
  }
  if (cat == typefamily_e_linear || cat == typefamily_e_noncopyable) {
    return false;
  }
  /*
  if (cat == typefamily_e_darrayst || cat == typefamily_e_cdarrayst) {
    return false;
  }
  */
  return true;
}

bool is_copyable(const term& t)
{
  sorted_exprs c;
  // sort_dep(c, t.get_expr());
  term t0 = t; /* TODO: avoid copying */
  expr_i *const einst = term_get_instance(t0);
  sort_dep(c, einst);
  for (std::list<expr_i *>::iterator i = c.sorted.begin();
    i != c.sorted.end(); ++i) {
    if (!is_copyable_type_one(*i)) {
      return false;
    }
  }
  return true;
}

static bool is_assignable_type_one(expr_i *e, bool allow_unsafe)
{
  if (!is_copyable_type_one(e)) {
    return false;
  }
  if (is_range_guard_ephemeral(e)) {
    if (!allow_unsafe || !is_cm_range_family(e)) {
      return false;
    }
  }
  const typefamily_e cat = get_family(e);
  if (cat == typefamily_e_darray || cat == typefamily_e_cdarray) {
    return false;
  }
  if (cat == typefamily_e_darrayst || cat == typefamily_e_cdarrayst) {
    return false;
  }
  return true;
}

// FIXME: used?
static bool is_assignable_internal(const term& t, bool allow_unsafe)
{
  sorted_exprs c;
  sort_dep(c, t.get_expr()); // FIXME: term_get_instance()
  for (std::list<expr_i *>::iterator i = c.sorted.begin();
    i != c.sorted.end(); ++i) {
    if (!is_assignable_type_one(*i, allow_unsafe)) {
      return false;
    }
  }
  return true;
}

bool is_assignable(const term& t)
{
  return is_assignable_internal(t, false);
}

bool is_assignable_allowing_unsafe(const term& t)
{
  return is_assignable_internal(t, true);
}

bool is_constructible(const term& t)
{
  expr_i *const expr = t.get_expr();
  if (expr == 0) {
    // fprintf(stderr, "is_constructible: %s false\n", term_tostr_human(t).c_str());
    return false;
  }
  const expr_e esort = expr->get_esort();
  if (esort == expr_e_struct) {
    expr_struct *const est = ptr_down_cast<expr_struct>(expr);
    if (est->block->argdecls != 0) {
      //fprintf(stderr, "is_constructible: %s true\n", term_tostr_human(t).c_str());
      return true;
    }
  }
  const bool r = is_default_constructible(t);
  // fprintf(stderr, "is_constructible: %s %d\n", term_tostr_human(t).c_str(), (int)r);
  return r;
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

static std::string term_tostr_tparams(const expr_tparams *tp, bool show_values,
  term_tostr_sort s)
{
  if (tp == 0) {
    return "";
  }
  // const size_t len = elist_length(tp);
  std::string r;
  if (s == term_tostr_sort_cname) {
    r += "< ";
  } else if (s == term_tostr_sort_humanreadable) {
    r += "{";
  } else {
    r += "$p$";
  }
  while (tp != 0) {
    if (show_values) {
      const term& tpdef = tp->param_def;
      r += term_tostr(tpdef, s);
    } else {
      r += std::string(tp->sym);
    }
    if (tp->rest != 0) {
      if (s == term_tostr_sort_humanreadable ||
	s == term_tostr_sort_cname) {
	r += ",";
      } else {
	r += "$q$";
      }
    }
    tp = tp->rest;
  }
  if (s == term_tostr_sort_cname) {
    r += " >";
  } else if (s == term_tostr_sort_humanreadable) {
    r += "}";
  } else {
    r += "$r$";
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
    return "\"" + v + "\"";
  }
  return escape_hex_non_alnum(v) + "$ls";
}

static std::string tparam_str_metalist(const term_list& tl, term_tostr_sort s)
{
  if (s == term_tostr_sort_humanreadable) {
    return term_tostr_list(tl, s);
  }
  return "m$ll" + term_tostr_list(tl, s);
}

static std::string tparam_str_lambda(const term& t, term_tostr_sort s)
{
  const expr_tparams *tparams = static_cast<const expr_tparams *>(
    t.get_lambda_tparams());
  const term *bdy = t.get_lambda_body();
  if (s == term_tostr_sort_humanreadable) {
    const bool show_values = false;
    return "metafunction" + term_tostr_tparams(tparams, show_values, s)
      + term_tostr(*bdy, s);
  }
  const bool show_values = false;
  return "m$lm" + term_tostr_tparams(tparams, show_values, s)
    + term_tostr(*bdy, s);
}

static std::string tparam_str_bind(const term& t, term_tostr_sort s)
{
  const expr_tparams *tp = static_cast<const expr_tparams *>(
    t.get_bind_tparam());
  const term *tpval = t.get_bind_tpvalue();
  const term *next = t.get_bind_next();
  const bool is_up = *t.get_bind_is_upvalue();
  if (s == term_tostr_sort_humanreadable) {
    return std::string("{") + std::string(tp->sym) +
      (is_up ? "::=" : ":=")
      + term_tostr(*tpval, s) + "}" + term_tostr(*next, s);
  }
  return "m$lb$" + std::string(tp->sym) + "$" + term_tostr(*next, s);
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
    return tparam_str_metalist(*t.get_metalist(), s);
  }
  if (t.is_lambda()) {
    return tparam_str_lambda(t, s);
  }
  if (t.is_bind()) {
    return tparam_str_bind(t, s);
  }
  const expr_i *const tdef = t.get_expr();
    /* expr_struct, expr_dunion, expr_interface, expr_typedef, expr_metafdef,
     * or expr_funcdef */
  if (tdef == 0) {
    return std::string();
  }
  const term_list *const args = t.get_args();
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
      cname = ptr_down_cast<const expr_struct>(tdef)->cnamei.cname;
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
      cname = ptr_down_cast<const expr_interface>(tdef)->cnamei.cname;
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
	cname = etd->cnamei.cname;
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
	  uniqns = "";
	}
	cname = efd->cnamei.cname;
	if (cname != 0 && !efd->no_def && !is_virtual_or_member_function_flag
	  && args != 0 && !args->empty()) {
	  /* extern non-member pxc-defined function with tparam. ignore cname
	   * in order to generate unique c symbol. */
#if 0
fprintf(stderr, "pxc-defined %s\n", cname); // FIXME
#endif
	  cname = 0;
	}
	esort_char = 'f';
      }
      break;
    case expr_e_int_literal:
      {
	const expr_int_literal *const ei =
	  ptr_down_cast<const expr_int_literal>(tdef);
	return tparam_str_long(ei->get_value_ll(), s);
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
      return "[unknown_esort " + ulong_to_string(tdef->get_esort()) + "]";
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
      if (s == term_tostr_sort_cname_tparam || s == term_tostr_sort_strict) {
	rstr += "pxcrt$$" + catstr;
      } else {
	if (is_cm_lock_guard_family(t)) {
	  const std::string s = (cat == typefamily_e_lock_guard)
	    ? "pxcrt::lock_guard" : "pxcrt::lock_cguard";
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
      std::string::size_type pos = rstr.find('>');
      if (pos != rstr.npos && pos > 0) {
	rstr_post = rstr.substr(pos + 1);
	rstr = rstr.substr(0, pos - 1);
      }
    } else {
      rstr = "";
      if (is_virtual_or_member_function_flag) {
	/* no ns prefix */
      } else if (s == term_tostr_sort_cname_tparam ||
	s == term_tostr_sort_strict) {
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
	    "Can not use '%s' as a template parameter "
	    "because it is generated by 'expand'", sym);
	}
	rstr += ulong_to_string(bl_def->block_id_ns);
      }
    }
    break;
  }
  if (rstr.empty()) {
    arena_error_throw(tdef, "Internal error: term_tostr");
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
    schild = cname != 0
      ? term_tostr_sort_cname : term_tostr_sort_cname_tparam;
  }
  if (args != 0 && !args->empty()) {
    rstr += term_tostr_list(*args, schild);
  } else if (tparams != 0) {
    const bool show_values = true;
    const std::string tmplargs = term_tostr_tparams(tparams, show_values,
      schild);
    DBG(fprintf(stderr, "cname=%s tmplargs=%s\n", cname, tmplargs.c_str()));
    rstr += tmplargs;
  }
  rstr += rstr_post;
  return rstr;
}

std::string term_tostr_list(const term_list_range& tl, term_tostr_sort s)
{
  std::string rstr;
  if (tl.empty()) {
    return rstr;
  }
  if (s == term_tostr_sort_cname) {
    rstr += "< ";
  } else if (s == term_tostr_sort_cname_tparam
    || s == term_tostr_sort_strict) {
    rstr += "$p$";
  } else {
    rstr += "{";
  }
  for (const term *i = tl.begin(); i != tl.end(); ++i) {
    if (i != tl.begin()) {
      if (s == term_tostr_sort_cname_tparam
	|| s == term_tostr_sort_strict) {
	rstr += "$q$";
      } else {
	rstr += ",";
      }
    }
    rstr += term_tostr(*i, s);
    if (s == term_tostr_sort_cname) {
      const expr_i *const de = i->get_expr();
      if (de != 0 && de->get_esort() == expr_e_funcdef) {
	/* function as tparam. append funcobj prefix. */
	rstr += "$fo";
      }
    }
  }
  if (s == term_tostr_sort_cname) {
    rstr += " >";
  } else if (s == term_tostr_sort_cname_tparam
    || s == term_tostr_sort_strict) {
    rstr += "$r$";
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

std::string term_tostr_list_cname(const term_list_range& tl)
{
  return term_tostr_list(tl, term_tostr_sort_cname);
}

std::string term_tostr_list_human(const term_list_range& tl)
{
  return term_tostr_list(tl, term_tostr_sort_humanreadable);
}

bool is_same_type(const term& t0, const term& t1)
{
  return t0 == t1;
}

bool implements_interface(expr_block *blk, const expr_interface *ei)
{
  expr_block::inherit_list_type& inh = blk->resolve_inherit_transitive();
  expr_block::inherit_list_type::const_iterator i;
  for (i = inh.begin(); i != inh.end(); ++i) {
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
  expr_block *t0blk =
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
  if (t0.get_expr() == 0) {
    /* TODO */
  } else if (t0.get_expr()->get_esort() == expr_e_tparams) {
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

bool is_compiletime_intval(expr_i *e)
{
  if (e->get_esort() == expr_e_int_literal) {
    return true;
  }
  symbol_common *sdef = e->get_sdef();
  if (sdef != 0) {
    term& ev = sdef->resolve_evaluated();
    if (ev.is_long()) {
      return true;
    }
  }
  if (e->get_esort() == expr_e_op) {
    expr_op *const eop = ptr_down_cast<expr_op>(e);
    if (eop->op == TOK_PLUS || eop->op == TOK_MINUS || eop->op == '(') {
      /* unary + or - */
      return is_compiletime_intval(eop->arg0);
    }
  }
  return false;
}

static bool is_int_literal_or_uop(expr_i *e, bool& is_negative,
  unsigned long long& ulv)
{
  if (e->get_esort() == expr_e_int_literal) {
    expr_int_literal *const ei = ptr_down_cast<expr_int_literal>(e);
    /* int literal is always non-negative */
    #if 0
    if (ei->is_unsigned) {
    #endif
      is_negative = ei->is_negative();
      ulv = ei->get_value_nosig();
    #if 0
    } else {
      long long sv = ei->get_signed();
      if (sv >= 0) {
	is_negative = false;
	ulv = sv;
      } else {
	is_negative = true;
	sv += 1; /* in order not to overflow */
	sv = -sv;
	ulv = sv;
	ulv += 1;
      }
    }
    #endif
    return true;
  }
  if (e->get_esort() == expr_e_op) {
    expr_op *const eop = ptr_down_cast<expr_op>(e);
    if (eop->op == TOK_PLUS || eop->op == TOK_MINUS || eop->op == '(') {
      /* unary op */
      const bool r = is_int_literal_or_uop(eop->arg0, is_negative, ulv);
      if (r && eop->op == TOK_MINUS) {
	is_negative = !is_negative;
      }
      return r;
    }
  }
  return false;
}

static bool is_float_literal_or_uop(expr_i *e)
{
  if (e->get_esort() == expr_e_float_literal) {
    return true;
  }
  if (e->get_esort() == expr_e_op) {
    expr_op *const eop = ptr_down_cast<expr_op>(e);
    if (eop->op == TOK_PLUS || eop->op == TOK_MINUS || eop->op == '(') {
      /* unary op */
      return is_float_literal_or_uop(eop->arg0);
    }
  }
  return false;
}

static bool integral_in_range(const term& tto, bool is_negative,
  unsigned long long ulv)
{
  if (is_unsigned_integral_type(tto)) {
    if (is_negative) {
      return false;
    }
    unsigned long long uim = uintegral_max_value(tto);
    return uim == 0 /* no check */ || ulv <= uim;
  } else {
    if (is_negative) {
      long long lmin = integral_min_value(tto);
      if (lmin == 0) {
	return true; /* no check */
      }
      lmin += 1; /* in order not to overflow */
      lmin = -lmin;
      unsigned long long ulmin = lmin;
      ulmin += 1;
      return ulv <= ulmin;
    } else {
      long long lmax = integral_max_value(tto);
      return lmax == 0 /* no check */ ||
	ulv <= static_cast<unsigned long long>(lmax);
    }
  }
}

static bool integral_in_range_long(const term& tto, long long lv)
{
  bool is_negative = lv < 0;
  unsigned long long ulv = 0;
  if (is_negative) {
    lv += 1; /* in order not to overflow */
    lv = -lv;
    ulv = lv;
    ulv += 1;
  } else {
    ulv = lv;
  }
  return integral_in_range(tto, is_negative, ulv);
}

static bool numeric_convertible(expr_i *efrom, const term& tfrom,
  const term& tto)
{
  if (tto == tfrom) {
    return true;
  }
  /* from compile-time constant */
  symbol_common *sdef = efrom->get_sdef();
  if (sdef != 0) {
    term& ev = sdef->resolve_evaluated();
    if (ev.is_long() && is_numeric_type(tto)) {
      /* compile-time long constant to integral or float */
      return integral_in_range_long(tto, ev.get_long());
    }
    if (ev.is_long() && ev.get_long() == 0 && is_bitmask(tto)) {
      return true; /* zero to bitmask */
    }
  }
  /* from integral literal */
  unsigned long long ulv = 0;
  bool is_signed = false;
  if (is_int_literal_or_uop(efrom, is_signed, ulv)) {
    if (is_integral_type(tto)) {
      /* int literal to integral type */
      return integral_in_range(tto, is_signed, ulv);
    }
    expr_int_literal *eint = dynamic_cast<expr_int_literal *>(efrom);
    if (eint != 0 && eint->get_value_nosig() == 0 && is_bitmask(tto)) {
      return true; /* zero to bitmask */
    }
  }
  if (is_float_literal_or_uop(efrom) && is_float_type(tto)) {
    return true; /* float literal to float type */
  }
  if (!is_numeric_type(tfrom) || !is_numeric_type(tto)) {
    return false;
  }
  if (get_family(tfrom) != get_family(tto)) {
    return false; /* none(builtin), int, uint, enum, bitmask */
  }
  const typefamily_e cat = get_family(tfrom);
  if (cat != typefamily_e_none) {
    return false; /* user defined type */
  }
  /* builtin type */
  if (is_enum(tfrom) || is_bitmask(tfrom) || is_enum(tto) || is_bitmask(tto)) {
    return false;
  }
  /* builtin integral or float */
  const type_attribute *const tafrom = get_type_attribute(tfrom);
  const type_attribute *const tato = get_type_attribute(tto);
  if (tafrom == 0 || tato == 0) {
    return false;
  }
  if (!tafrom->is_integral && tato->is_integral) {
    /* float to integral */
    return false;
  }
  if (tafrom->is_signed && !tato->is_signed) {
    /* signed to unsigned */
    return false;
  }
  const int to_min = num_bits_min(tto);
  const int from_max = num_bits_max(tfrom);
  if (to_min < from_max) {
    /* possibly smaller number of significant bits */
    return false;
  }
  return true;
}

static void check_convfunc_validity(term& convfunc, const term& tfrom,
  const term& tto, expr_i *pos)
{
  expr_i *const cfe = convfunc.get_expr();
  if (cfe != 0 && cfe->get_esort() == expr_e_funcdef) {
    expr_i *const einst = term_get_instance(convfunc);
    expr_funcdef *const efd = dynamic_cast<expr_funcdef *>(einst);
    expr_block *block = einst->get_template_block();
    if (block != 0) {
      expr_argdecls *ad = block->get_argdecls();
      if (ad != 0 && ad->get_rest() == 0 && ad->resolve_texpr() == tfrom &&
	ad->passby != passby_e_mutable_reference &&
	efd->get_rettype() == tto &&
	/* implicit conversion function must not return noheap or reference
	 * because rooting is not implemented for these cases */
	is_passby_cm_value(block->ret_passby) &&
	!is_noheap_type(tto))
      {
	/* ok */
	return;
      }
    }
  }
  arena_error_push(pos, "Invalid conversion function: '%s'",
    term_tostr_human(convfunc).c_str());
}

static term get_implicit_conversion_func(const term& tfrom, const term& tto,
  expr_i *pos)
{
  const expr_i *const efrom = term_get_instance_const(tfrom);
  const expr_i *const eto = term_get_instance_const(tto);
  if (efrom == 0 || eto == 0) {
    return term(); /* tfrom or tto is not a type, for example */
  }
  const std::string tto_ns = eto->get_unique_namespace();
  const std::string tfrom_ns = efrom->get_unique_namespace();
  if (global_block == 0) {
    return term();
  }
  symbol_table *const symtbl = &global_block->symtbl;
  bool is_global_dummy = false;
  bool is_upvalue_dummy = false;
  expr_i *eic = symtbl->resolve_name("operator::implicit_conversion", "",
    global_block, is_global_dummy, is_upvalue_dummy);
  const term tic_ta[2] = { tto, tfrom };
  const term tic = term(eic, term_list_range(tic_ta, 2));
  term term_convfunc = eval_term(tic);
  check_convfunc_validity(term_convfunc, tfrom, tto, pos);
  return term_convfunc;
}

static void convert_type_internal(expr_i *efrom, term& tto, tvmap_type& tvmap)
{
  term& tfrom = efrom->resolve_texpr();
  DBG_CONV(fprintf(stderr, "convert efrom=%s from=%s to=%s\n",
    efrom->dump(0).c_str(),
    term_tostr_human(tfrom).c_str(),
    term_tostr_human(tto).c_str()));
  if (is_same_type(tfrom, tto)) {
    DBG_CONV(fprintf(stderr, "convert: same type\n"));
    return;
  }
  if (unify_type(tto, tfrom, tvmap)) {
    /* type parameterized function */
    DBG_CONV(fprintf(stderr, "convert: unify\n"));
    return;
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
  if (numeric_convertible(efrom, tfrom, tconvto)) {
    efrom->conv = conversion_e_cast;
    efrom->type_conv_to = tconvto;
    DBG_CONV(fprintf(stderr, "convert: numeric cast\n"));
    return;
  }
  if (tfrom == builtins.type_strlit && is_simple_string_type(tto)) {
    DBG_CONV(fprintf(stderr, "convert: strlit to string\n"));
    efrom->conv = conversion_e_cast;
    efrom->type_conv_to = tconvto;
    return;
  }
  if (is_cm_range_family(tconvto) && is_cm_range_family(tfrom) &&
    (is_const_range_family(tconvto) || !is_const_range_family(tfrom))) {
    /* range to crange */
    const term_list *const tta = tconvto.get_args();
    const term_list *const tfa = tfrom.get_args();
    if (tta != 0 && tfa != 0 && !tta->empty() && !tfa->empty() &&
      tta->front() == tfa->front()) {
      DBG_CONV(fprintf(stderr, "convert: range\n"));
      efrom->conv = conversion_e_subtype_ptr;
      efrom->type_conv_to = tconvto;
      return;
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
      /* no */
    } else {
      const term_list *const tta = tconvto.get_args();
      const term_list *const tfa = tra.get_args();
      if (tta != 0 && tfa != 0 && !tta->empty() && !tfa->empty() &&
	tta->front() == tfa->front()) {
	DBG_CONV(fprintf(stderr, "convert: auto container to range\n"));
	efrom->conv = conversion_e_container_range;
	efrom->type_conv_to = tconvto;
	return;
      }
    }
  }
  DBG_CONV(fprintf(stderr, "P6\n"));
  if (is_cm_range_family(tfrom) && is_container_family(tconvto) &&
    tconvto != builtins.type_strlit) {
    /* range to container */
    const term tra = get_array_range_texpr(0, 0, tconvto);
    const term_list *const tfa = tfrom.get_args();
    const term_list *const tta = tra.get_args();
    if (tta != 0 && tfa != 0 && !tta->empty() && !tfa->empty() &&
      tta->front() == tfa->front()) {
      DBG_CONV(fprintf(stderr, "convert: auto range to container\n"));
      efrom->conv = conversion_e_cast;
      efrom->type_conv_to = tconvto;
      return;
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
      return;
    }
  }
  DBG_CONV(fprintf(stderr, "P8\n"));
  if (is_sub_type(tfrom, tconvto)) {
    DBG_CONV(fprintf(stderr, "convert: sub type\n"));
    efrom->conv = conversion_e_subtype_obj;
    /* don't set type_conv_to here so that tempvar is not create as of type
     * tto, which can be an interface. */
    return;
  }
  const term cf = get_implicit_conversion_func(tfrom, tconvto, efrom);
  if (cf.is_expr()) {
    #if 0
    if (is_noheap_type(tconvto)) {
      arena_error_throw(efrom,
	"Implicit conversion to noheap type is prohibited");
    }
    #endif
    efrom->conv = conversion_e_implicit;
    efrom->type_conv_to = tconvto;
    efrom->implicit_conversion_func = cf;
    return;
  }
  DBG_CONV(fprintf(stderr, "convert: failed\n"));
  const std::string sfrom = term_tostr(efrom->resolve_texpr(),
    term_tostr_sort_humanreadable);
  const std::string sto = term_tostr(tto,
    term_tostr_sort_humanreadable);
  arena_error_throw(efrom, "Can not convert from '%s' to '%s'",
    sfrom.c_str(), sto.c_str());
}

void check_convert_type(expr_i *efrom, term& tto, tvmap_type *p)
{
  if (p == 0) {
    tvmap_type tvmap;
    convert_type_internal(efrom, tto, tvmap);
  } else {
    convert_type_internal(efrom, tto, *p);
  }
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

static void calc_dep_upvalues_direct(expr_i *e)
{
  expr_funcdef *const efd = dynamic_cast<expr_funcdef *>(e);
  if (efd == 0) {
    return;
  }
  if (!efd->block->compiled_flag) {
    return; /* uninstantiated template */
  }
  /* append upvalues used by efd itself */
  symbol_table *symt = &efd->block->symtbl;
  symbol_table::locals_type::const_iterator i;
  symbol_table::locals_type const& upvalues = symt->get_upvalues();
  for (i = upvalues.begin(); i != upvalues.end(); ++i) {
    expr_i *const eup = i->second.edef;
    bool is_constmemfunc_context = false;
    expr_i *const eframe = get_current_frame_expr(eup->symtbl_lexical);
    if (is_global_var(eup)) {
      continue; /* eup is a global variable */
    }
    if (eframe != 0 && eframe->get_esort() == expr_e_struct) {
      /* e is a struct field */
      expr_funcdef *mf = get_up_member_func(symt);
      if (mf == efd && mf->is_member_function() == eframe) {
	/* efd is a member function and eup is a field of the same struct */
	continue;
      }
      if (mf != 0 && mf->is_member_function() == eframe) {
	/* efd is a descent of a member function of eframe */
	is_constmemfunc_context = mf->is_const;
      }
    }
    expr_funcdef::dep_upvalue_type de(eup, is_constmemfunc_context);
    efd->dep_upvalues.insert_if(de);
    DBG_DEPUP(fprintf(stderr, "depup %s %s %d\n",
      term_tostr_human(efd->get_value_texpr()).c_str(), eup->dump(0).c_str(),
      (int)is_constmemfunc_context));
  }
}

static void calc_dep_upvalues_tr_rec(expr_funcdef *tgt_efd,
  std::set<expr_funcdef *>& es, expr_funcdef::dep_upvalues_type& deps)
{
  if (es.find(tgt_efd) != es.end()) {
    return;
  }
  es.insert(tgt_efd);
  /* append callee upvalues */
  expr_funcdef::calls_type::const_iterator ci;
  for (ci = tgt_efd->calls.begin(); ci != tgt_efd->calls.end(); ++ci) {
    symbol_table *const call_symt = ci->first;
    expr_funcdef *const callee = ci->second;
    expr_funcdef::dep_upvalues_type cdeps = callee->dep_upvalues;
    calc_dep_upvalues_tr_rec(callee, es, cdeps); /* recursive call */
    expr_funcdef::dep_upvalues_type::const_iterator di;
    for (di = cdeps.begin(); di != cdeps.end(); ++di) {
      expr_i *const eupdef = di->first; /* expr defining the upvalue */
      symbol_table *const eupdef_symt = eupdef->symtbl_lexical;
      symbol_table *s = call_symt; 
      for (; s != &tgt_efd->block->symtbl; s = s->get_lexical_parent()) {
	if (s == eupdef_symt) {
	  break; /* eup is defined in the tgt_efd frame */
	}
      }
      if (s == eupdef_symt) {
	/* this upvalue is defined in the frame of tgt_efd. */
	DBG_DEPUP(fprintf(stderr, "depup_found_local %s %s\n",
	  term_tostr_human(tgt_efd->get_value_texpr()).c_str(),
	  eupdef->dump(0).c_str()));
	continue;
      }
      expr_i *const eupdef_frame = get_current_frame_expr(eupdef_symt);
      if (eupdef_frame != 0 && tgt_efd->is_member_function() == eupdef_frame) {
	/* this upvalue is a field and tgt_efd is a member function of the
	 * same struct */
	DBG_DEPUP(fprintf(stderr, "depup_found_field %s %s\n",
	  term_tostr_human(tgt_efd->get_value_texpr()).c_str(),
	  eupdef->dump(0).c_str()));
	continue;
      }
      /* this upvalue is an upvalue of the caller(tgt_efd) too. */
      deps.insert_if(*di);
      DBG_DEPUP(fprintf(stderr, "depup_callee %s %s %d\n",
	term_tostr_human(tgt_efd->get_value_texpr()).c_str(),
	di->first->dump(0).c_str(), (int)(di->second)));
    }
  }
  es.erase(tgt_efd);
}

static void calc_dep_upvalues_tr(expr_i *e)
{
  expr_funcdef *const efd = dynamic_cast<expr_funcdef *>(e);
  if (efd == 0) {
    return;
  }
  std::set<expr_funcdef *> es;
  expr_funcdef::dep_upvalues_type deps = efd->dep_upvalues;
  calc_dep_upvalues_tr_rec(efd, es, deps);
  efd->dep_upvalues_tr = deps;
}

void fn_check_dep_upvalues(expr_i *e)
{
  execute_rec_incl_instances(e, calc_dep_upvalues_direct);
  execute_rec_incl_instances(e, calc_dep_upvalues_tr);
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
    /* calls fn_compile recursively if a template is instantiated or an expr
     * is generated dynamically. */
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
    case expr_e_nsmark:
    case expr_e_struct:
    case expr_e_dunion:
    case expr_e_interface:
    case expr_e_funcdef:
    case expr_e_typedef:
    case expr_e_metafdef:
    case expr_e_enumval:
    case expr_e_inline_c:
    case expr_e_var:
    case expr_e_expand:
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

bool is_global_frame(symbol_table *cur)
{
  return (get_current_frame_symtbl(cur) == 0);
}

symbol_table *get_current_frame_symtbl(symbol_table *cur)
{
  while (cur) {
    if (cur->get_block_esort() != expr_e_block) {
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
    if (e->get_esort() == fr->get_block_esort()) {
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
    if (e->get_esort() == cur->get_block_esort()) {
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
  return get_term_threading_attribute(fe->get_value_texpr());
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
  const int thr_mask_all = attribute_e(
    attribute_threaded | attribute_multithr |
    attribute_valuetype | attribute_tsvaluetype);
  int attr = 0;
  const expr_i *const e = t.get_expr();
  if (e == 0) {
    attr = attribute_unknown;
    if (t.is_metalist()) {
      attr = thr_mask_all;
    }
  } else {
    attr = get_expr_threading_attribute(e);
  }
  const term_list *const args = t.get_args_or_metalist();
  if (args == 0) {
    return attribute_e(attr);
  }
  for (term_list::const_iterator i = args->begin(); i != args->end(); ++i) {
    if (!i->is_expr() && !i->is_metalist()) {
      continue;
    }
    const expr_i *const ce = i->get_expr();
    int mask = 0;
    if (ce == 0) {
      mask = thr_mask_all;
    } else {
      mask = get_threading_attribute_mask(ce->get_esort());
    }
    attr &= (get_term_threading_attribute(*i) | ~mask);
  }
  attribute_e r = attribute_e(attr);
  return r;
}

expr_funcdef *get_up_member_func(symbol_table *cur)
{
  expr_funcdef *efd = 0;
  while (cur) {
    if (cur->get_block_esort() == expr_e_funcdef) {
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

void fn_set_namespace(expr_i *e, const std::string& uniqns, int& block_id_ns,
  bool allow_unsafe)
{
  if (e == 0) {
    return;
  }
  e->set_unique_namespace_one(uniqns, allow_unsafe);
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
    fn_set_namespace(c, uniqns, block_id_ns, allow_unsafe);
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
  #if 0
  fprintf(stderr, "set generated %p %s\n", e, e->dump(0).c_str());
  #endif
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
      case expr_e_expand:
	break;
      case expr_e_funcdef:
	if (ev == 0) {
	  break;
	}
	/* FALLTHRU */
      default:
	arena_error_throw(head, "Invalid statement");
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
	esp->tok == TOK_CONTINUE ? "Continue" : "Break");
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
  } else if (e->get_esort() == expr_e_funcdef) {
    expr_funcdef *const efd = ptr_down_cast<expr_funcdef>(e);
    if (efd->is_macro()) {
      expand_base = efd->block;
    }
  }
  const int num = e->get_num_children();
  for (int i = 0; i < num; ++i) {
    expr_i *c = e->get_child(i);
    fn_set_tree_and_define_static(c, e, symtbl, stmt, is_template_descent,
      (is_expand_base || c == expand_base));
  }
}

void fn_update_tree(expr_i *e, expr_i *p, symbol_table *symtbl,
  const std::string& curns_u)
{
  if (e == 0) {
    return;
  }
  e->parent_expr = p;
  e->symtbl_lexical = symtbl;
  e->set_unique_namespace_one(curns_u,
    nspropmap[curns_u].safety != nssafety_e_safe);
  DBG_SYMTBL(fprintf(stderr, "fn_update_tree: set %p-> symtbl_lexical  = %p\n",
    e, symtbl));
  if (e->get_esort() == expr_e_block) {
    expr_block *const eb = ptr_down_cast<expr_block>(e);
    symtbl = &eb->symtbl;
  }
  const int num = e->get_num_children();
  for (int i = 0; i < num; ++i) {
    expr_i *c = e->get_child(i);
    fn_update_tree(c, e, symtbl, curns_u);
  }
}

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

static bool check_function_argtypes(const expr_interface *ei,
  expr_funcdef *efd, expr_funcdef *efd_impl)
{
  if (!is_same_type(efd->get_rettype(), efd_impl->get_rettype())) {
    arena_error_push(efd_impl,
      "Return type mismatch (got: %s, expected: %s)",
      term_tostr_human(efd->get_rettype()).c_str(),
      term_tostr_human(efd_impl->get_rettype()).c_str());
    return false;
  }
  if (efd->is_const != efd_impl->is_const) {
    arena_error_push(efd_impl, "Wrong constness");
    return false;
  }
  expr_argdecls *a0 = efd->block->get_argdecls();
  expr_argdecls *a1 = efd_impl->block->get_argdecls();
  while (a0 != 0 && a1 != 0) {
    if (!is_same_type(a0->resolve_texpr(), a1->resolve_texpr())) {
      arena_error_push(efd_impl,
	"Invalid type for '%s' (got: %s, expected: %s)",
	a1->sym,
	term_tostr_human(a1->resolve_texpr()).c_str(),
	term_tostr_human(a0->resolve_texpr()).c_str());
      return false;
    }
    if (a0->passby != a1->passby) {
      arena_error_push(efd_impl,
	"Argument '%s' must be passed by%s%s",
	a1->sym,
	is_passby_const(a0->passby) ? " const" : " mutable",
	is_passby_cm_reference(a0->passby) ? " reference" : " value");
      return false;
    }
    a0 = a0->get_rest();
    a1 = a1->get_rest();
  }
  if (a0 != 0 || a1 != 0) {
    arena_error_push(efd_impl, "Wrong number of arguments");
    return false;
  }
  return true;
}

static void check_interface_impl_one(expr_i *esub, expr_block *esub_block,
  expr_i *esuper)
{
  const expr_interface *const ei_super =
    dynamic_cast<const expr_interface *>(esuper);
  if (ei_super == 0) {
    arena_error_push(esub, "Type '%s' is not an interface",
      term_tostr_human(esub->get_texpr()).c_str());
    return;
  }
  const symbol_table& symtbl = ei_super->block->symtbl;
  symbol_table::local_names_type const& local_names = symtbl.get_local_names();
  symbol_table::local_names_type::const_iterator k;
  symbol_table::locals_type::const_iterator i, j;
  for (k = local_names.begin(); k != local_names.end(); ++k) {
    i = symtbl.find(*k, false);
    expr_funcdef *const efd = dynamic_cast<expr_funcdef *>(i->second.edef);
    if (efd == 0) {
      continue;
    }
    j = esub_block->symtbl.find(i->first, false);
    if (esub->get_esort() == expr_e_struct) {
      if (j == esub_block->symtbl.end() ||
	j->second.edef->get_esort() != expr_e_funcdef) {
	arena_error_push(esub, "Method '%s' is not implemented",
	  i->first.c_str());
	continue;
      }
    } else if (esub->get_esort() == expr_e_interface) {
      if (j != esub_block->symtbl.end()) {
	arena_error_push(esub, "Can not override '%s'", i->first.c_str());
      }
    }
    if (j != esub_block->symtbl.end()) {
      expr_funcdef *const efd_impl =
	ptr_down_cast<expr_funcdef>(j->second.edef);
      if (!check_function_argtypes(ei_super, efd, efd_impl)) {
	#if 0
	arena_error_push(efd_impl, "Function type mismatch");
	#endif
	continue;
      }
      #if 0
      fprintf(stderr, "%p %p %d:%s %d:%s\n", efd_impl, efd,
	efd_impl->line, efd_impl->cname, efd->line, efd->cname);
      #endif
      if (efd->cnamei.cname != 0 && efd_impl->cnamei.cname == 0) {
	efd_impl->cnamei = efd->cnamei;
      }
    }
  }
}

static void check_interface_impl(expr_i *e)
{
  /* this function updates efd->cname if necessary */
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
  expr_block::inherit_list_type& inh = block->resolve_inherit_transitive();
  expr_block::inherit_list_type::iterator i;
  for (i = inh.begin(); i != inh.end(); ++i) {
    check_interface_impl_one(e, block, *i);
  }
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
    expr_var *const ev = ptr_down_cast<expr_var>(e);
    dvars_cur.insert(ev);
    DBG_UBD(fprintf(stderr, "UBD dvar=%s(%p) uvars=%zu\n", ev->sym, ev,
      uvars.size()));
    if (uvars.find(ev) != uvars.end()) {
      DBG_UBD(fprintf(stderr, "UBD!\n"));
      const std::pair<expr_i *, const expr_funcdef *>& vm = uvars[ev];
      expr_i *const eu = vm.first;
      arena_error_push(eu, "Variable '%s' used before defined",
	ev->sym);
      if (vm.second != 0) {
	arena_error_push(eu, "(used as an upvalue for '%s')", vm.second->sym);
      }
    }
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
	expr_funcdef::dep_upvalues_type::const_iterator i;
	for (i = efd->dep_upvalues_tr.begin(); i != efd->dep_upvalues_tr.end();
	  ++i) {
	  expr_var *const ev = dynamic_cast<expr_var *>(i->first);
	  if (ev != 0 && uvars.find(ev) == uvars.end()) {
	    DBG_UBD(fprintf(stderr, "UBD funccall %s var=%s(%p)\n", efd->sym,
	      ev->sym, ev));
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
  /* check recursively */
  if (e->get_esort() == expr_e_op) {
    /* special treatment for ops. variable x must not be appear in the rhs of
     * 'var x = ...'. */
    expr_op *const eop = ptr_down_cast<expr_op>(e);
    /* check rhs first */
    check_use_before_def_one(uvars, dvars_cur, eop->arg1);
    check_use_before_def_one(uvars, dvars_cur, eop->arg0);
  } else {
    const int num_children = e->get_num_children();
    for (int i = 0; i < num_children; ++i) {
      expr_i *const c = e->get_child(i);
      if (c != 0 && c->get_esort() == expr_e_block) {
	expr_block *const cbl = ptr_down_cast<expr_block>(c);
	if (cbl->symtbl.get_block_esort() != expr_e_block) {
	  /* child frame. skip. */
	  continue;
	}
      }
      check_use_before_def_one(uvars, dvars_cur, e->get_child(i));
    }
  }
}

static void check_use_before_def(expr_i *e)
{
  expr_block *const bl = dynamic_cast<expr_block *>(e);
  if (bl != 0) {
    expr_i *const fre = get_current_frame_expr(&bl->symtbl);
    if (fre == 0 || bl->symtbl.get_block_esort() != expr_e_block) {
      /* a frame block or global */
      varmap_type uvars;
      vars_type dvars_cur;
      check_use_before_def_one(uvars, dvars_cur, bl);
    }
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
	arena_error_throw(pos, "Pointer target '%s' is not multithreaded",
	  term_tostr_human(tgt).c_str());
      }
      if (is_immutable_pointer_family(est->typefamily) &&
	(attr & attribute_valuetype) == 0) {
	arena_error_throw(pos, "Pointer target '%s' is not valuetype",
	  term_tostr_human(tgt).c_str());
      }
      if (est->typefamily == typefamily_e_tiptr &&
	(attr & attribute_valuetype) == 0) {
	arena_error_throw(pos, "Pointer target '%s' is not tsvaluetype",
	  term_tostr_human(tgt).c_str());
      }
    }
  }
}

static void check_type_threading(expr_i *e)
{
  if (e->get_esort() == expr_e_te &&
    e->parent_expr != 0 && e->parent_expr->get_esort() == expr_e_telist) {
    /* this te is a sub-expression. no need to check. avoid calling
     * resolve_texpr() */
    return;
  }
  term& te = e->resolve_texpr();
  check_type_threading_te(te, e);
}

static void check_var_defcon(expr_i *e)
{
  if (e == 0) {
    return;
  }
  if (e->get_esort() == expr_e_var) {
    ptr_down_cast<expr_var>(e)->check_defcon();
  }
}

void fn_check_final(expr_i *e)
{
  if (e == 0) {
    return;
  }
  check_var_defcon(e);
  check_type_threading(e);
  check_interface_impl(e);
  check_use_before_def(e);
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
        "Type '%s' can't inherit '%s' because it has incompatible "
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
    arena_error_push(a0, "Can not be modified because of a conversion");
  }
  if (a0->get_sdef() != 0) {
    /* symbol or te */
    symbol_common *const sc = a0->get_sdef();
    const bool upvalue_flag = sc->upvalue_flag;
    term& tev = sc->resolve_evaluated();
    expr_i *const eev = term_get_instance(tev);
    expr_e astyp = eev != 0 ? eev->get_esort() : expr_e_metafdef /* dummy */;
    if (astyp == expr_e_var) {
//fprintf(stderr, "expr_has_lvalue %s var %d\n", a0->dump(0).c_str(), (int)upvalue_flag);
      if (upvalue_flag) {
//fprintf(stderr, "expr_has_lvalue %s upvar\n", a0->dump(0).c_str());
	symbol_table *symtbl = find_current_symbol_table(eev);
	assert(symtbl);
	expr_block *bl = symtbl->block_backref;
	assert(bl);
	if (bl->parent_expr != 0 &&
	  bl->parent_expr->get_esort() == expr_e_struct) {
	  /* as->symbol_def is an instance variable */
//fprintf(stderr, "expr_has_lvalue %s field\n", a0->dump(0).c_str());
	  expr_i *const a0frame = get_current_frame_expr(a0->symtbl_lexical);
	  if (a0frame->get_esort() == expr_e_funcdef) {
	    expr_funcdef *efd = ptr_down_cast<expr_funcdef>(a0frame);
	    efd = efd->is_member_function_descent();
	    if (efd->is_const) {
	      DBG_LV(fprintf(stderr, "expr_has_lvalue %s 1 false\n",
		a0->dump(0).c_str()));
	      if (!thro_flg) {
		return false;
	      }
	      arena_error_push(epos,
		"Field '%s' can not be modified from a const member function",
		sc->get_fullsym().c_str());
	    }
	  }
	}
      }
      expr_var *const ev = ptr_down_cast<expr_var>(eev);
      if (!is_passby_mutable(ev->varinfo.passby)) {
	DBG_LV(fprintf(stderr, "expr_has_lvalue %s 1.1 false\n",
	  a0->dump(0).c_str()));
	if (!thro_flg) {
	  return false;
	}
	arena_error_push(epos, "Variable '%s' can not be modified",
	  sc->get_fullsym().c_str());
      }
    } else if (astyp == expr_e_argdecls) {
      expr_argdecls *const ead = ptr_down_cast<expr_argdecls>(eev);
      if (!is_passby_mutable(ead->passby)) {
	DBG_LV(fprintf(stderr, "expr_has_lvalue %s 2 false\n",
	  a0->dump(0).c_str()));
	if (!thro_flg) {
	  return false;
	}
	arena_error_push(epos, "Argument '%s' can not be modified",
	  sc->get_fullsym().c_str());
      }
    } else {
      DBG_LV(fprintf(stderr, "expr_has_lvalue %s 3 false\n",
	a0->dump(0).c_str()));
      if (!thro_flg) {
	return false;
      }
      arena_error_push(epos, "Symbol '%s' can not be modified",
	sc->get_fullsym().c_str());
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
	arena_error_push(epos, "Range expression can not be modified");
      } else {
	DBG_LV(fprintf(stderr, "expr_has_lvalue: op[] 0-1\n"));
	const term tc = aop->arg0->resolve_texpr();
	if (is_const_range_family(tc)) {
	  /* const range */
	  DBG_LV(fprintf(stderr, "expr_has_lvalue %s 6 false\n",
	    a0->dump(0).c_str()));
	  if (!thro_flg) {
	    return false;
	  }
	  arena_error_push(epos, "Const range element can not be modified");
	} else if (is_cm_range_family(tc)) {
	  /* mutable range */
	  DBG_LV(fprintf(stderr, "expr_has_lvalue %s 7 true\n",
	    a0->dump(0).c_str()));
	  /* a[] is mutable even when a is not mutable */
	  return true;
	} else if (is_const_container_family(tc)) {
	  /* const container */
	  DBG_LV(fprintf(stderr, "expr_has_lvalue %s 7-1 false\n",
	    a0->dump(0).c_str()));
	  if (!thro_flg) {
	    return false;
	  }
	  arena_error_push(epos,
	    "Const container element can not be modified");
	} else {
	  return expr_has_lvalue(aop, aop->arg0, thro_flg);
	}
      }
    case '(':
      /* expr '(foo)' has an lvalue iff foo has an lvalue */
      return expr_has_lvalue(aop, aop->arg0, thro_flg);
    case TOK_PTR_DEREF:
      /* expr '*foo' has an lvalue iff foo is a non-const ptr */
      {
	const term& tc = aop->arg0->resolve_texpr();
	if (is_const_or_immutable_pointer_family(tc) ||
	  is_const_range_family(tc) ||
	  is_const_container_family(tc)) {
	  DBG_LV(fprintf(stderr, "expr_has_lvalue %s 8 false\n",
	    a0->dump(0).c_str()));
	  if (!thro_flg) {
	    return false;
	  }
	  arena_error_push(epos, "Can not modify data via a const reference");
	}
	DBG_LV(fprintf(stderr, "expr_has_lvalue %s 9 true\n",
	  a0->dump(0).c_str()));
	return true; /* ok */
      }
    default:
      break;
    }
    DBG_LV(fprintf(stderr, "expr_has_lvalue %s 10 false\n",
      a0->dump(0).c_str()));
    if (!thro_flg) {
      return false;
    }
    arena_error_push(epos, "Can not modify"); // FIXME: error message
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
  arena_error_push(epos, "Can not modify"); // FIXME: error message
  return false;
}

term get_pointer_deref_texpr(expr_op *eop, const term& t)
{
  const term tg = get_pointer_target(t);
  if (tg.is_null()) {
    arena_error_throw(eop, "Invalid dereference");
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
  term slt = eval_mf_symbol(ect,
    nonconst ? "range_type" : "crange_type", eop);
  if (slt.is_null()) {
    arena_error_throw(eop, "Cannot apply '[ .. ]'");
    return builtins.type_void;
  }
  DBG_SLICE(fprintf(stderr, "range: %s %s -> %s\n",
    ec != 0 ? ec->dump(0).c_str() : "",
    term_tostr_human(ect).c_str(), term_tostr_human(slt).c_str()));
  return slt;
}

bool is_dense_array(expr_op *eop, const term& t0)
{
  term t = eval_mf_symbol(t0, "is_dense_array", eop);
  if (t.is_null()) {
    arena_error_throw(eop, "Symbol 'is_dense_array' is not found for type '%s'",
      term_tostr_human(t0).c_str());
    return false;
  }
  return meta_term_to_long(t) != 0;
}

term get_array_elem_texpr(expr_op *eop, const term& t0)
{
  term t = eval_mf_symbol(t0, "mapped_type", eop);
  if (t.is_null()) {
    arena_error_throw(eop, "Symbol 'mapped_type' is not found for type '%s'",
      term_tostr_human(t0).c_str());
    return builtins.type_void;
  }
  return t;
}

term get_array_index_texpr(expr_op *eop, const term& t0)
{
  term t = eval_mf_symbol(t0, "key_type", eop);
  if (t.is_null()) {
    arena_error_throw(eop, "Symbol 'key_type' is not found for type '%s'",
      term_tostr_human(t0).c_str());
    return builtins.type_void;
  }
  return t;
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
}

bool is_noexec_expr(expr_i *e)
{ 
  switch (e->get_esort()) {
  case expr_e_typedef:
  case expr_e_metafdef:
  case expr_e_ns:
  case expr_e_nsmark:
  case expr_e_enumval:
  case expr_e_struct:
  case expr_e_dunion:
  case expr_e_interface:
  case expr_e_funcdef:
    return true;
  case expr_e_inline_c:
    return ptr_down_cast<expr_inline_c>(e)->posstr != "emit";
  default:
    /* expr_e_if, expr_e_var, expr_e_op for example */
    return false;
  }
}

bool is_compiled(const expr_block *bl)
{
  return !bl->tinfo.is_uninstantiated() && bl->compiled_flag;
}

static void add_imports_rec(const symbol& k, const symbol& v)
{
  std::pair<symbol, symbol> e(k, v);
  if (nsimports_rec.find(e) != nsimports_rec.end()) {
    return;
  }
  nsimports_rec.insert(e);
  #if 0
  fprintf(stderr, "imports_rec %s %s\n", k.c_str(), v.c_str());
  #endif
  nsimports_type::const_iterator iter = nsimports.find(v);
  if (iter == nsimports.end()) {
    return;
  }
  const std::list<std::pair<symbol, bool> >& lst = iter->second;
  for (std::list<std::pair<symbol, bool> >::const_iterator j = lst.begin();
    j != lst.end(); ++j) {
    if (j->second) {
      add_imports_rec(k, j->first);
    }
  }
}

void fn_prepare_imports()
{
  for (nsimports_type::const_iterator i = nsimports.begin();
    i != nsimports.end(); ++i) {
    #if 0
    fprintf(stderr, "imports %s\n", i->first.c_str());
    #endif
    const std::list<std::pair<symbol, bool> >& lst = i->second;
    for (std::list<std::pair<symbol, bool> >::const_iterator j = lst.begin();
      j != lst.end(); ++j) {
      add_imports_rec(i->first, j->first);
    }
  }
}

}; 

