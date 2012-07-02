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
#include "expr_impl.hpp"
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
#define DBG_CT(x)
#define DBG_COMPILE(x)
#define DBG_COPT(x)
#define DBG_SYMTBL(x)
#define DBG_THR(x)

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
  return is_integral_type(t) ||
    t == builtins.type_double ||
    t == builtins.type_float;
}

bool is_smallpod_type(const term& t)
{
  const expr_typedef *const td = dynamic_cast<const expr_typedef *>(
    t.get_expr());
  return td != 0 && td->is_pod;
}

bool is_string_type(const term& t)
{
  return t == builtins.type_string;
}

bool is_integral_type(const term& t)
{
  /* TODO: slow */
  return
    t == builtins.type_bool ||
    t == builtins.type_uchar ||
    t == builtins.type_char ||
    t == builtins.type_ushort ||
    t == builtins.type_short ||
    t == builtins.type_uint ||
    t == builtins.type_int ||
    t == builtins.type_ulong ||
    t == builtins.type_long ||
    t == builtins.type_size_t;
}

std::string get_category(const term& t)
{
  const expr_struct *est = dynamic_cast<const expr_struct *>(
    t.get_expr());
  std::string cat;
  if (est != 0 && est->category != 0) {
    return est->category;
  }
  return std::string();
}

static bool is_pointer_category(const std::string& cat)
{
  return (cat == "cref" || cat == "ref" || cat == "tcref" || cat == "tref");
}

static bool is_threaded_pointer_category(const std::string& cat)
{
  return (cat == "tcref" || cat == "tref");
}

term get_pointer_target(const term& t)
{
  std::string cat = get_category(t);
  if (is_pointer_category(cat)) {
    const term_list *args = t.get_args();
    if (args != 0 && !args->empty()) {
      return *args->begin();
    }
  }
  return term();
}

bool is_compatible_pointer(const term& t0, const term& t1)
{
  const std::string c0 = get_category(t0);
  const std::string c1 = get_category(t1);
  bool thr0 = is_threaded_pointer_category(c0);
  bool thr1 = is_threaded_pointer_category(c1);
  return thr0 == thr1;
}

call_trait_e get_call_trait(const term& t)
{
  if (is_cm_pointer_family(t)) { 
    return call_trait_e_raw_pointer;
  } else if (is_smallpod_type(t)) {
    return call_trait_e_value;
  } else {
    return call_trait_e_const_ref_nonconst_ref;
  }
  // FIXME: slice{t}
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

bool is_variant(const term& t)
{
  const expr_variant *const tv = dynamic_cast<const expr_variant *>(
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
  return is_pointer_category(get_category(t));
}

bool is_const_pointer_family(const term& t)
{
  const std::string cat = get_category(t);
  if (cat == "tcref" || cat == "cref") {
    return true;
  }
  return false;
}

bool is_array_family(const term& t)
{
  const std::string cat = get_category(t);
  return cat == "varray" || cat == "farray";
}

bool is_map_family(const term& t)
{
  const std::string cat = get_category(t);
  return cat == "map";
}

bool is_const_slice_family(const term& t)
{
  const std::string cat = get_category(t);
  return cat == "cslice";
}

bool is_cm_slice_family(const term& t)
{
  const std::string cat = get_category(t);
  return cat == "slice" || cat == "cslice";
}

bool is_weak_value_type(const term& t)
{
  return is_cm_slice_family(t); /* slices are weak */
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
    if (est->category != 0) {
      if (std::string(est->category) == "farray") {
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
  const expr_variant *const eva = dynamic_cast<const expr_variant *>(einst);
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
  const std::string cat = get_category(t);
  if (cat == "varray" || cat == "map") {
    return true;
  }
  return false;
}

bool type_allow_feach(const term& t)
{
  const std::string cat = get_category(t);
  if (cat == "varray" || cat == "farray" || cat == "map" || cat == "slice") {
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
  if (s == term_tostr_sort_cname || s == term_tostr_sort_humanreadable) {
    return long_to_string(v);
  }
  return escape_hex_non_alnum(long_to_string(v)) + "$li";
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
    /* expr_struct, expr_variant, expr_interface, expr_typedef, expr_macrodef,
     * or expr_funcdef */
  if (tdef == 0) {
    return std::string();
  }
  const char *sym = 0;
  const char *cname = 0;
  std::string ns;
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
      ns = ptr_down_cast<const expr_struct>(tdef)->ns;
      cname = ptr_down_cast<const expr_struct>(tdef)->cname;
      esort_char = 's';
      break;
    case expr_e_variant:
      sym = ptr_down_cast<const expr_variant>(tdef)->sym;
      self_block = ptr_down_cast<const expr_variant>(tdef)->block;
      tparams = self_block->tinfo.tparams;
      ns = ptr_down_cast<const expr_variant>(tdef)->ns;
      esort_char = 'v';
      break;
    case expr_e_interface:
      sym = ptr_down_cast<const expr_interface>(tdef)->sym;
      self_block = ptr_down_cast<const expr_interface>(tdef)->block;
      tparams = self_block->tinfo.tparams;
      ns = ptr_down_cast<const expr_interface>(tdef)->ns;
      esort_char = 'i';
      break;
    case expr_e_typedef:
      sym = ptr_down_cast<const expr_typedef>(tdef)->sym;
      ns = ptr_down_cast<const expr_typedef>(tdef)->ns;
      cname = ptr_down_cast<const expr_typedef>(tdef)->cname;
      esort_char = 0;
      self_block = 0;
      break;
    case expr_e_macrodef:
      sym = ptr_down_cast<const expr_macrodef>(tdef)->sym;
      ns = ptr_down_cast<const expr_macrodef>(tdef)->ns;
      tparams = ptr_down_cast<const expr_macrodef>(tdef)->get_tparams();
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
	ns = efd->ns;
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
	return "[param " + std::string(etp->sym) + "]";
      }
      break;
    default:
      return "[unknown " + ulong_to_string(tdef->get_esort()) + "]";
    }
  }
  symbol_table *const st_defined = tdef->symtbl_lexical;
  if (st_defined == 0) {
    return "[nosymtbl]";
  }
  std::string rstr, rstr_post;
  switch (s) {
  case term_tostr_sort_humanreadable:
    if (!ns.empty()) {
      rstr += ns + "::";
    }
    rstr += sym ? sym : "[noname]";
    break;
  case term_tostr_sort_strict:
  case term_tostr_sort_cname:
  case term_tostr_sort_cname_tparam:
    if (is_cm_pointer_family(t)) {
      const std::string cat = get_category(t);
      if (s == term_tostr_sort_cname_tparam) {
	rstr += "pxcrt$$" + cat;
      } else {
	if (is_interface_pointer(t)) {
	  rstr += "pxcrt::rcptr";
	} else {
	  if (cat == "tcref" || cat == "tref") {
	    rstr += "pxcrt::rcptr<pxcrt::trcval";
	  } else {
	    rstr += "pxcrt::rcptr<pxcrt::rcval";
	  }
	  rstr_post = " >";
	}
      }
    } else if (cname != 0) {
      rstr = cname;
      if (s != term_tostr_sort_cname) {
	rstr = replace_char(rstr, ':', '$');
      }
    } else {
      rstr = "";
      if (s == term_tostr_sort_cname_tparam || s == term_tostr_sort_strict) {
	rstr += replace_char(ns, ':', '$') + "$n$$";
      } else /* if (st_defined->get_lexical_parent() == 0) */ {
	rstr += to_c_ns(ns) + "::";
      }
      if (sym != 0) {
	rstr += sym;
      }
      if (esort_char) {
	rstr += "$";
	rstr.push_back(esort_char);
      }
      if (sym == 0) {
	assert(self_block != 0);
	rstr += ulong_to_string(self_block->block_id_ns);
      } else if (st_defined->get_lexical_parent() != 0
	&& append_block_id_if_local) {
	rstr += ulong_to_string(st_defined->block_backref->block_id_ns);
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
  const expr_struct *const tdef = dynamic_cast<const expr_struct *>(
    t0.get_expr());
  if (tdef == 0) {
    DBG_SUB(fprintf(stderr, "is_sub_type: tdef null\n"));
    return false;
  }
  #if 0
  const expr_i *const t1inst = term_get_instance_const(t1);
  #endif
  expr_telist *ih = tdef->block->inherit;
  while (ih != 0) {
    if (ih->head->get_sdef()->resolve_evaluated() == t1) {
      DBG_SUB(fprintf(stderr, "is_sub_type: inherit\n"));
      return true;
    }
    DBG_SUB(fprintf(stderr, "is_sub_type: not inherit %s %s\n",
      term_tostr_human(ih->head->get_sdef()->resolve_evaluated()).c_str(),
      term_tostr_human(t1).c_str()));
    ih = ih->rest;
  }
  DBG_SUB(fprintf(stderr, "is_sub_type: none matched\n"));
  return false;
}

static bool check_tvmap(tvmap_type& tvmap, const std::string& sym,
  const term& t)
{
  DBG_CONV(fprintf(stderr, "check_tvmap sym=%s t=%s\n",
    sym, term_tostr(t, term_tostr_strict).c_str()));
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
  DBG_CONV(fprintf(stderr, "unify_type_list: tl0=%p tl1=%p\n",
    term_tostr_list(tl0, term_tostr_sort_strict).c_str(),
    term_tostr_list(tparams, term_tostr_sort_strict).c_str()));
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
  DBG_CONV(fprintf(stderr, "unify_template: tl0=%p tl1=%p\n",
    term_tostr_list(tl0, term_tostr_sort_strict).c_str(),
    term_tostr_list(tparams, term_tostr_sort_strict).c_str()));
  if (t0.get_expr()->get_esort() == expr_e_tparams) {
    const expr_tparams *const t0tp = ptr_down_cast<const expr_tparams>(
      t0.get_expr()); /* outer tparam */
    return check_tvmap(tvmap, t0tp->sym, t1);
  } else if (t0.get_expr() == t1.get_expr()) {
    return unify_type_list(*t0.get_args(), *t1.get_args(), tvmap);
  }
  return false;
}

bool convert_type(expr_i *efrom, term& tto, tvmap_type& tvmap)
{
  term& tfrom = efrom->resolve_texpr();
  DBG_CONV(fprintf(stderr, "convert efrom=%s from=%s to=%s\n",
    efrom->dump(0).c_str(),
    term_tostr(tfrom, term_tostr_sort_strict).c_str(),
    term_tostr(tto, term_tostr_sort_strict).c_str()));
  if (is_same_type(tfrom, tto)) {
    DBG_CONV(fprintf(stderr, "convert: same type\n"));
    return true;
  }
  if (unify_type(tto, tfrom, tvmap)) {
    /* type parameterized function */
    DBG_CONV(fprintf(stderr, "convert: unify\n"));
    return true;
  }
  term tconvto;
  expr_tparams *const tp = dynamic_cast<expr_tparams *>(tto.get_expr());
  if (tp != 0 && tvmap.find(tp->sym) != tvmap.end()) {
    /* tto is a type parameter and bound to tvmap[tto] */
    tconvto = tvmap[tp->sym];
  } else {
    tconvto = tto;
  }
  if (is_numeric_type(tfrom) && is_numeric_type(tconvto)) {
    efrom->conv = conversion_e_cast;
    efrom->type_conv_to = tconvto;
    DBG_CONV(fprintf(stderr, "convert: numeric cast\n"));
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
  if (is_cm_slice_family(tconvto) && is_cm_slice_family(tfrom) &&
    (is_const_slice_family(tconvto) || !is_const_slice_family(tfrom))) {
    /* slice to cslice */
    const term_list *const tta = tconvto.get_args();
    const term_list *const tfa = tfrom.get_args();
    if (tta != 0 && tfa != 0 && tta->front() == tfa->front()) {
      DBG_CONV(fprintf(stderr, "convert: slice\n"));
      return true;
    }
  }
  #if 0
  #endif
  if (is_cm_pointer_family(tconvto) &&
    is_sub_type(get_pointer_target(tfrom), get_pointer_target(tconvto)) &&
    is_compatible_pointer(tfrom, tconvto)) {
    /* ref to cref or tref to tcref */
    const expr_struct *const es_to = dynamic_cast<const expr_struct *>(
      tconvto.get_expr());
    const expr_struct *const es_from = dynamic_cast<const expr_struct *>(
      tfrom.get_expr());
    std::string cat_to, cat_from;
    if (es_to != 0 && es_to->category != 0) {
      cat_to = es_to->category;
    }
    if (es_from != 0 && es_from->category != 0) {
      cat_from = es_from->category;
    }
    if (!cat_to.empty() && !cat_from.empty() &&
      (cat_to == "cref" || cat_from == "ref")) {
      DBG_CONV(fprintf(stderr, "convert: (struct) ref to cref\n"));
      return true;
    }
    if (!cat_to.empty() && !cat_from.empty() &&
      (cat_to == "tcref" || cat_from == "tref")) {
      DBG_CONV(fprintf(stderr, "convert: (struct) tref to tcref\n"));
      return true;
    }
  }
  if (is_sub_type(tfrom, tconvto)) {
    DBG_CONV(fprintf(stderr, "convert: sub type\n"));
    return true;
  }
  DBG_CONV(fprintf(stderr, "convert: failed\n"));
  return false;
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
    case expr_e_variant:
    case expr_e_interface:
    case expr_e_funcdef:
    case expr_e_typedef:
    case expr_e_macrodef:
    case expr_e_extval:
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
  return e;
}

symbol_table *get_current_frame_symtbl(symbol_table *cur)
{
  while (cur) {
    if (cur->block_esort != expr_e_block) {
      return cur; /* funcdef, struct, variant, or interface */
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

static attribute_e get_expr_threading_attribute(const expr_i *e)
{
  if (e == 0) {
    return attribute_unknown;
  }
  const expr_funcdef *efd = dynamic_cast<const expr_funcdef *>(e);
  if (efd != 0) {
    const expr_struct *esd = efd->is_member_function();
    if (esd != 0) {
      e = esd;
    }
  }
  return e->get_attribute();
}

bool is_threaded_context(symbol_table *cur)
{
  attribute_e attr = get_expr_threading_attribute(get_current_frame_expr(cur));
  return (attr & (attribute_threaded | attribute_multithr)) != 0;
}

bool is_multithr_context(symbol_table *cur)
{
  expr_i *const fe = get_current_frame_expr(cur);
  if (fe != 0 && fe->get_esort() == expr_e_funcdef) {
    return false;
  }
  attribute_e attr = get_expr_threading_attribute(fe);
  return (attr & attribute_multithr) != 0;
}

static bool check_term_threading_attribute(const term& t, bool req_multithr)
{
  const expr_i *const e = t.get_expr();
  if (e == 0) {
    return true;
  }
  // const attribute_e attr = e->get_attribute();
  const attribute_e attr = get_expr_threading_attribute(e);
  if ((attr & (attribute_threaded | attribute_multithr)) == 0) {
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

void fn_set_namespace(expr_i *e, const std::string& n, int& block_id_ns)
{
  if (e == 0) {
    return;
  }
  e->set_namespace_one(n);
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
    fn_set_namespace(c, n, block_id_ns);
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
  expr_variant *const ev = dynamic_cast<expr_variant *>(e);
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
      case expr_e_macrodef:
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
  expr_stmts *stmt, bool is_template_descent)
{
  if (e == 0) {
    return;
  }
  e->parent_expr = p;
  e->symtbl_lexical = symtbl;
  DBG_SYMTBL(fprintf(stderr, "fn_set_tree_: set %p-> symtbl_lexical  = %p\n",
    e, symtbl));
  fn_check_syntax_one(e);
  e->define_const_symbols_one();
  e->define_vars_one(stmt);
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
  const int num = e->get_num_children();
  for (int i = 0; i < num; ++i) {
    expr_i *c = e->get_child(i);
    fn_set_tree_and_define_static(c, e, symtbl, stmt, is_template_descent);
  }
}

void fn_update_tree(expr_i *e, expr_i *p, symbol_table *symtbl,
  const std::string& curns)
{
  if (e == 0) {
    return;
  }
  e->parent_expr = p;
  e->symtbl_lexical = symtbl;
  e->set_namespace_one(curns);
  DBG_SYMTBL(fprintf(stderr, "fn_update_tree: set %p-> symtbl_lexical  = %p\n",
    e, symtbl));
  if (e->get_esort() == expr_e_block) {
    expr_block *const eb = ptr_down_cast<expr_block>(e);
    symtbl = &eb->symtbl;
  }
  const int num = e->get_num_children();
  for (int i = 0; i < num; ++i) {
    expr_i *c = e->get_child(i);
    fn_update_tree(c, e, symtbl, curns);
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
  DBG_CT(fprintf(stderr, "fn_check_type end %p expr=%s -> type=%s\n",
    e, e->dump(0).c_str(),
    term_tostr(e->get_texpr(), term_tostr_sort_strict).c_str()));
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

static bool check_function_arity(const expr_interface *ei, expr_funcdef *efd,
  expr_struct *est, expr_funcdef *efd_impl)
{
  if (!is_same_type(efd->get_rettype(), efd_impl->get_rettype())) {
    return false;
  }
  if (efd->is_const != efd_impl->is_const) {
    return false;
  }
  expr_argdecls *a0 = efd->block->argdecls;
  expr_argdecls *a1 = efd_impl->block->argdecls;
  while (a0 != 0 && a1 != 0) {
    if (!is_same_type(a0->resolve_texpr(), a1->resolve_texpr())) {
      return false;
    }
    a0 = a0->rest;
    a1 = a1->rest;
  }
  return (a0 == 0 && a1 == 0);
}

static void check_interface_impl_one(expr_struct *est, expr_telist *ih)
{
  const expr_interface *const ei = dynamic_cast<const expr_interface *>(
    term_get_instance_const(ih->head->get_sdef()->resolve_evaluated()));
  if (ei == 0) {
    arena_error_push(ih, "type '%s' is not an interface",
      term_tostr_human(ih->head->get_texpr()).c_str());
    return;
  }
  const symbol_table& symtbl = ei->block->symtbl;
  symbol_table::locals_type::const_iterator i, j;
  for (i = symtbl.locals.begin(); i != symtbl.locals.end(); ++i) {
    expr_funcdef *const efd = dynamic_cast<expr_funcdef *>(i->second.edef);
    if (efd == 0) {
      continue;
    }
    j = est->block->symtbl.locals.find(i->first);
    if (j == est->block->symtbl.locals.end() ||
      j->second.edef->get_esort() != expr_e_funcdef) {
      arena_error_push(est, "method '%s' is not implemented",
	i->first.c_str());
      continue;
    }
    expr_funcdef *const efd_impl = ptr_down_cast<expr_funcdef>(j->second.edef);
    if (!check_function_arity(ei, efd, est, efd_impl)) {
      arena_error_push(efd_impl, "function type mismatch");
      continue;
    }
  }
}

static void check_interface_impl(expr_i *e)
{
  expr_struct *const est = dynamic_cast<expr_struct *>(e);
  if (est == 0) {
    return;
  }
  expr_telist *i = est->block->inherit;
  while (i != 0) {
    check_interface_impl_one(est, i);
    i = i->rest;
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
  /* check if tref target is multithreaded */
  term rte;
  const expr_i *e = te.get_expr();
  const term_list *args = te.get_args();
  if (args != 0 && e != 0) {
    for (term_list::const_iterator i = args->begin(); i != args->end(); ++i) {
      check_type_threading_te(*i, pos);
    }
    const expr_struct *est = dynamic_cast<const expr_struct *>(e);
    if (est != 0 && est->category != 0 && !args->empty()) {
      const std::string cat = est->category;
      if (is_threaded_pointer_category(cat)) {
	const term& tgt = args->front();
	if (!term_is_multithr(tgt)) {
	  arena_error_throw(pos, "'%s' is not a multithreaded type",
	    term_tostr_human(tgt).c_str());
	}
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
  const int num = e->get_num_children();
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
  /* check children */
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
  case expr_e_macrodef:
  case expr_e_te:
  case expr_e_struct:
  case expr_e_variant:
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
  case expr_e_macrodef:
  case expr_e_te:
  case expr_e_struct:
  case expr_e_variant:
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

void check_inherit_threading(expr_struct *est)
{
  const term& est_te = est->get_value_texpr();
  expr_telist *inh = est->block->inherit;
  for (; inh != 0; inh = inh->rest) {
    symbol_common *sd = inh->head->get_sdef();
    assert(sd);
    term& te = sd->resolve_evaluated();
    if (term_is_multithr(te) &&
      (est->get_attribute() & attribute_multithr) == 0) {
      arena_error_throw(est,
        "struct '%s' can't implement '%s' because it's not multithreaded",
        term_tostr_human(est_te).c_str(), term_tostr_human(te).c_str());
    }
    if (term_is_threaded(te) &&
      (est->get_attribute() & (attribute_threaded | attribute_multithr))
        == 0) {
      arena_error_throw(est,
        "struct '%s' can't implement '%s' because it's not threaded",
        term_tostr_human(est_te).c_str(), term_tostr_human(te).c_str());
    }
  }
}

}; 

