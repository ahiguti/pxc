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

#include "expr_misc.hpp"
#include "eval.hpp"
#include "evalmeta.hpp"
#include "util.hpp"

#define DBG(x)
#define DBG_TE(x)
#define DBG_TE2(x)
#define DBG_STRLIT(x)
#define DBG_SYM(x)
#define DBG_RES(x)
#define DBG_SE(x)
#define DBG_TE1(x)

namespace pxc {

expr_i::expr_i(const char *fn, int line)
  : fname(fn), line(line), type_of_this_expr(), conv(conversion_e_none),
    type_conv_to(), parent_expr(0), symtbl_lexical(0), tempvar_id(-1),
    generated_flag(false)
{
  #if 0
  // FIXME: remove. must be pushed after obj is successfully created
  expr_arena.push_back(this);
  #endif
  type_of_this_expr = builtins.type_void;
}

expr_i::expr_i(const expr_i& e)
  : fname(e.fname), line(e.line), type_of_this_expr(e.type_of_this_expr),
    conv(e.conv), type_conv_to(e.type_conv_to), parent_expr(e.parent_expr),
    symtbl_lexical(e.symtbl_lexical), tempvar_id(e.tempvar_id),
    tempvar_varinfo(e.tempvar_varinfo)
{
  expr_arena.push_back(this);
}

void expr_i::set_texpr(const term& t)
{
  DBG_SE(fprintf(stderr, "set_texpr %p -> %s\n", this,
    term_tostr_human(t).c_str()));
  type_of_this_expr = t;
}

expr_i *symbol_common::resolve_symdef(symbol_table *lookup)
{
  /* called when te is eval()ed */
  if (symbol_def == 0) {
    bool is_global = false, is_upvalue = false;
    const symbol lookup_ns =
      arg_hidden_this ? arg_hidden_this_ns : uniqns;
    if (lookup == 0) {
      lookup = parent_expr->symtbl_lexical;
    }
    #if 0
    double t0 = gettimeofday_double();
    #endif
    symbol_def = lookup->resolve_name(get_sym_prefix_fullsym(), lookup_ns,
      parent_expr, is_global, is_upvalue);
    #if 0
    double t1 = gettimeofday_double();
    #endif
    #if 0
    if (t1 - t0 > 0.00005) {
    fprintf(stderr, "slow resolve_name [%s] [%s] [%s] %f\n",
      get_sym_prefix_fullsym().c_str(), get_fullsym().c_str(),
      lookup_ns.c_str(), t1 - t0);
    }
    #endif
    DBG_TE2(fprintf(stderr,
      "expr_te::resolve_symdef: this=%p symtbl=%p [%s] up=%d\n", this,
      lookup, symbol_def->dump(0).c_str(), (int)is_upvalue));
    symtbl_defined = lookup;
    upvalue_flag = is_upvalue;
    assert(symbol_def);
    /* check private ns */
    const std::string sdns = symbol_def->get_unique_namespace();
    const std::string smns = parent_expr->get_unique_namespace();
    if (sdns != "" && sdns != smns) {
      nspropmap_type::const_iterator i = nspropmap.find(sdns);
      if (i == nspropmap.end()) {
	arena_error_push(parent_expr,
	  "Internal error: namespace '%s' not found", sdns.c_str());
      }
      assert(i != nspropmap.end());
      if (!i->second.is_public) {
	const std::string p = smns + "::";
	if (sdns.substr(0, p.size()) != p) {
	  arena_error_push(parent_expr,
	    "Symbol '%s' is defined in private namespace '%s'",
	    this->get_fullsym().c_str(), sdns.c_str());
	}
      }
    }
  } else {
    DBG_TE2(fprintf(stderr,
      "expr_te::resolve_symdef already resolved: %p [%s]\n", this,
      symbol_def->dump(0).c_str()));
  }
  return symbol_def;
}

const expr_i *symbol_common::get_symdef() const
{
  assert(symbol_def);
  return symbol_def;
}

void symbol_common::set_symdef(expr_i *e)
{
#if 0
fprintf(stderr, "set symdef %s %p\n", get_fullsym().c_str(), e); // FIXME
#endif
  symbol_def = e;
}

static bool cur_frame_uninstantiated(symbol_table *symtbl)
{
  expr_i *const fr = get_current_frame_expr(symtbl);
  if (fr != 0) {
    expr_block *const tbl = fr->get_template_block();
    if (tbl->tinfo.is_uninstantiated()) {
      return true;
    }
  }
  return false;
}

term& symbol_common::resolve_evaluated()
{
  if (evaluated.is_null()) {
    const bool need_partial_eval = cur_frame_uninstantiated(
      parent_expr->symtbl_lexical);
    evaluated = eval_expr(parent_expr, need_partial_eval);
  }
  return evaluated;
}

const term& symbol_common::get_evaluated() const
{
  assert(!evaluated.is_null());
  return evaluated;
}

const term& symbol_common::get_evaluated_nochk() const
{
  return evaluated;
}

void symbol_common::set_evaluated(const term& t)
{
  evaluated = t;
}

expr_te::expr_te(const char *fn, int line, expr_i *nssym, expr_i *tlarg)
  : expr_i(fn, line), nssym(ptr_down_cast<expr_nssym>(nssym)),
    tlarg(ptr_down_cast<expr_telist>(tlarg)), sdef(this)
{
  type_of_this_expr.clear(); /* check_type() */
  sdef.set_fullsym(get_full_name(nssym));
  DBG_TE1(fprintf(stderr, "te %p %s\n", this,
    term_tostr_human(type_of_this_expr).c_str()));
}

expr_i *expr_te::clone() const
{
  expr_te *const r = new expr_te(*this);
  r->sdef.parent_expr = r;
  if (is_term_literal()) {
    /* term literal */
  } else {
    /* argdecls have incomplete symbol_def values before instantiated */
    r->sdef.set_symdef(0);
    r->sdef.set_evaluated(term()); // HERE HERE HERE
    r->type_of_this_expr.clear();
  }
  return r;
}

expr_i *expr_te::get_child(int i)
{
  if (i == 0) { return nssym; }
  else if (i == 1) { return tlarg; }
  return 0;
}

void expr_te::set_child(int i, expr_i *e)
{
  if (i == 0) { nssym = ptr_down_cast<expr_nssym>(e); }
  else if (i == 1) { tlarg = ptr_down_cast<expr_telist>(e); }
}

bool expr_te::is_term_literal() const
{
  return nssym->sym[0] == 0; /* symbol is empty */
}

expr_i *expr_te::resolve_symdef(symbol_table *lookup)
{
  /* called when te is eval()ed */
  assert(lookup == symtbl_lexical);
  return sdef.resolve_symdef();
  #if 0
  if (sdef.symbol_def == 0) {
    bool is_global = false, is_upvalue = false;
    assert(lookup == symtbl_lexical);
    sdef.symbol_def = symtbl_lexical->resolve_name(sdef.get_fullsym(), sdef.ns,
      this, is_global, is_upvalue);
    DBG_TE2(fprintf(stderr,
      "expr_te::resolve_symdef: this=%p symtbl=%p [%s]\n", this,
      symtbl_lexical, sdef.symbol_def->dump(0).c_str()));
    assert(sdef.symbol_def);
  } else {
    DBG_TE2(fprintf(stderr,
      "expr_te::resolve_symdef already resolved: %p [%s]\n", this,
      sdef.symbol_def->dump(0).c_str()));
  }
  return sdef.symbol_def;
  #endif
}

const expr_i *expr_te::get_symdef() const
{
  return sdef.get_symdef();
  #if 0
  if (sdef.symbol_def == 0) {
    arena_error_throw(this, "Internal error: expr_te::get_symdef");
  }
  return sdef.symbol_def;
  #endif
}

static void symdef_check_threading_attr_type_or_func(expr_i *e,
  symbol_common *sc, term& t)
{
  symbol_table *const cur = get_current_frame_symtbl(e->symtbl_lexical);
  if (cur == 0) {
    return;
  }
  const attribute_e cattr = get_context_threading_attribute(cur);
  const attribute_e tattr = get_term_threading_attribute(t);
  if ((tattr & attribute_threaded) == 0 && (cattr & attribute_threaded) != 0) {
    arena_error_push(e,
      "Type or function '%s' for symbol '%s' is not threaded",
      term_tostr_human(t).c_str(), sc->get_fullsym().c_str());
  }
  #if 0
  // FIXME: remove
  if (is_type_esort(e->get_esort())) {
  #endif
  if (is_type(t)) {
    if ((tattr & attribute_multithr) == 0 &&
      (cattr & attribute_multithr) != 0) {
      arena_error_push(e,
	"Type '%s' for symbol '%s' is not multithreaded",
	term_tostr_human(t).c_str(), sc->get_fullsym().c_str());
    }
    if ((tattr & attribute_valuetype) == 0 &&
      (cattr & attribute_valuetype) != 0) {
      arena_error_push(e,
	"Type '%s' for symbol '%s' is not valuetype",
	term_tostr_human(t).c_str(), sc->get_fullsym().c_str());
    }
    if ((tattr & attribute_tsvaluetype) == 0 &&
      (cattr & attribute_tsvaluetype) != 0) {
      arena_error_push(e,
	"Type '%s' for symbol '%s' is not tsvaluetype",
	term_tostr_human(t).c_str(), sc->get_fullsym().c_str());
    }
  }
#if 0
fprintf(stderr, "%s c=%x t=%x\n", term_tostr_human(t).c_str(), (int)cattr, (int)tattr); // FIXME
#endif
  #if 0
  if (cur == 0 || !is_threaded_context(cur)) {
    return;
  }
  if (!term_is_threaded(t)) {
    arena_error_push(e,
      "Type or function '%s' for symbol '%s' is not threaded",
      term_tostr_human(t).c_str(), sc->get_fullsym().c_str());
  }
  // FIXME: term_is_valuetype
  if (is_multithr_context(cur) && !term_is_multithr(t)) {
    arena_error_push(e,
      "Type or function '%s' for symbol '%s' is not multithreaded",
      term_tostr_human(t).c_str(), sc->get_fullsym().c_str());
  }
  #endif
}

static void symdef_check_threading_attr_var(expr_i *e, symbol_common *sc,
  expr_i *def_expr)
{
  symbol_table *const cur = get_current_frame_symtbl(e->symtbl_lexical);
  if (cur == 0 || !is_threaded_context(cur)) {
    /* no need to check */
    return;
  }
  if (is_global_var(def_expr)) {
    const expr_var *const ev = ptr_down_cast<const expr_var>(def_expr);
    arena_error_push(e,
      "Global variable '%s' can't be used from a threaded context",
      ev->sym);
  }
}

static term& resolve_texpr_symbol_common(expr_i *e)
{
  symbol_common *const sc = e->get_sdef();
  assert(sc);
  // sc->resolve_symdef(); // FIXME remove?
  /* note: sc->get_symdef() can be 0 when this expr is generated by @expand */
  if (!e->type_of_this_expr.is_null()) {
    DBG_TE1(fprintf(stderr, "resolve_texpr_symbol_common cached: %p -> %s\n",
      e, term_tostr_human(e->type_of_this_expr).c_str()));
    return e->type_of_this_expr;
  }
  assert(sc->get_symdef() != 0); // FIXME?
  /* if it's a type/func symbol, evaluate it */
  term& evaluated = sc->resolve_evaluated();
  DBG_SYM(fprintf(stderr, "expr_symbol symdef=%p '%s' symdef_te=%s\n",
    sc->symbol_def, symbol_def->dump(0).c_str(),
    term_tostr_cname(evaluated).c_str()));
  DBG_TE1(fprintf(stderr, "resolve_texpr_symbol_common evaluated: %s\n",
    term_tostr_human(evaluated).c_str()));
  if (evaluated.is_long()) {
    /* int literal */
    e->type_of_this_expr = builtins.type_long;
    DBG_TE1(fprintf(stderr,
      "resolve_texpr_symbol_common tote: '%s' %p -> long\n",
      term_tostr_human(evaluated).c_str(), e));
  } else if (evaluated.is_string()) {
    /* string literal */
    e->type_of_this_expr = builtins.type_strlit;
    DBG_TE1(fprintf(stderr,
      "resolve_texpr_symbol_common tote: '%s' %p -> string\n",
    term_tostr_human(evaluated).c_str(), e));
  } else if (!evaluated.is_expr()) {
    e->type_of_this_expr = builtins.type_void;
    DBG_TE1(fprintf(stderr,
      "resolve_texpr_symbol_common tote: '%s' %p -> unknown void\n",
      term_tostr_human(evaluated).c_str(), e));
    // FIXME: metalist ???
    #if 0
    arena_error_throw(e, "Invalid metaexpression '%s'",
      term_tostr_human(evaluated).c_str());
    #endif
  } else if (is_function(evaluated)) {
    /* function symbol has the value of function */
    // e->type_of_this_expr = evaluated;
    e->type_of_this_expr = builtins.type_void;
    DBG_TE1(fprintf(stderr,
      "resolve_texpr_symbol_common tote: '%s' %p -> function\n",
      term_tostr_human(evaluated).c_str(), e));
    symdef_check_threading_attr_type_or_func(e, sc, evaluated);
  } else if (is_type(evaluated)) {
    /* type name can't have a value */
    e->type_of_this_expr = builtins.type_void;
    DBG_TE1(fprintf(stderr,
      "resolve_texpr_symbol_common tote: '%s' %p -> type\n",
      term_tostr_human(evaluated).c_str(), e));
    symdef_check_threading_attr_type_or_func(e, sc, evaluated);
  } else {
    /* not a type nor symbol */
    expr_i *const ev = evaluated.get_expr();
    assert(ev);
    // FIXME: why necessary? enable this?
    #if 0
    fn_check_type(ev, lookup);
    #endif
    switch (ev->get_esort()) {
    case expr_e_var:
    case expr_e_argdecls:
    case expr_e_enumval:
    case expr_e_int_literal:
    case expr_e_float_literal:
    case expr_e_bool_literal:
    case expr_e_str_literal:
      break;
    default:
      abort();
    }
    e->type_of_this_expr = ev->resolve_texpr();
    DBG_TE1(fprintf(stderr,
      "resolve_texpr_symbol_common tote: '%s' %p -> other\n",
      term_tostr_human(evaluated).c_str(), e));
    assert(!e->type_of_this_expr.is_null());
    symdef_check_threading_attr_var(e, sc, ev);
    #if 0
    e->type_of_this_expr = sc->symbol_def->resolve_texpr();
    #endif
  }
  #if 0
  if (!evaluated.is_null()) {
    /* type or func */
    if (is_function(evaluated)) {
      /* function symbol has the value of function */
      e->type_of_this_expr = evaluated;
    } else {
      /* type name can't have a value */
      e->type_of_this_expr = builtins.type_void;
    }
  } else {
    /* not a type symbol */
    e->type_of_this_expr = sc->symbol_def->resolve_texpr();
  }
  #endif
  DBG_RES(fprintf(stderr,
    "expr_symbol %p '%s' symdef='%s' type_of_this_expr='%s' symtbl=%p\n",
    e, sc->get_fullsym().c_str(), sc->get_symdef()->dump(0).c_str(),
    term_tostr_human(e->type_of_this_expr).c_str(), e->symtbl_lexical));
  assert(!e->type_of_this_expr.is_null());
  #if 0
  assert(!term_has_tparam(e->type_of_this_expr));
  #endif
  DBG_TE1(fprintf(stderr, "resolve_texpr_symbol_common tote: %p -> %s\n",
    e, term_tostr_human(e->type_of_this_expr).c_str()));
  return e->type_of_this_expr;
}

term& expr_te::resolve_texpr()
{
  return resolve_texpr_symbol_common(this);
}

std::string expr_te::dump(int indent) const
{
  if (tlarg == 0) {
    return "[" + sdef.get_fullsym().to_string() + "]";
  }
  return "[" + sdef.get_fullsym().to_string() +
    "(" + dump_expr(indent, tlarg) + ")]";
}

expr_telist::expr_telist(const char *fn, int line, expr_i *head, expr_i *rest)
  : expr_i(fn, line), head(head), rest(ptr_down_cast<expr_telist>(rest))
{
}

std::string expr_telist::dump(int indent) const
{
  std::string r = "(" + dump_expr(indent, head);
  if (rest) {
    r += " ";
    r += dump_expr(indent, rest);
  }
  r += ")";
  return r;
}

expr_inline_c::expr_inline_c(const char *fn, int line, const char *label,
  const char *cstr, bool declonly, expr_i *val)
  : expr_i(fn, line), posstr(label), cstr(cstr), declonly(declonly), value(val),
    te_list(0)
{
  if (
    posstr != "types" &&
    posstr != "functions" &&
    posstr != "implementation" &&
    posstr != "incdir" &&
    posstr != "link" &&
    posstr != "cflags" &&
    posstr != "ldflags" &&
    posstr != "disable-bounds-checking" &&
    posstr != "disable-guard" &&
    posstr != "disable-noheap-checking" &&
    posstr != "emit"
    ) {
    arena_error_push(this,
      "Invalid label '%s'", posstr.c_str());
  }
  const size_t sz = strlen(cstr);
  for (size_t i = 0; i + 3 < sz; ++i) {
    if (cstr[i] != '%' || cstr[i + 1] != '{') {
      continue;
    }
    size_t j = i + 2;
    for (; j + 1 < sz; ++j) {
      if (cstr[j] != '}' || cstr[j + 1] != '%') {
	continue;
      }
      break;
    }
    if (j + 1 == sz) {
      std::string mess(cstr + i);
      for (size_t k = 0; k < mess.size(); ++k) {
	if (mess[k] == '\n') mess[k] = ' ';
      }
      arena_error_push(this,
	"Invalid embedded expression '%s'", mess.c_str());
      break;
    }
    const std::string es(cstr + i + 2, j - i - 2);
    expr_i *const te = string_to_te(this, es);
    inline_c_element elem;
    elem.start = i;
    elem.finish = j + 2;
    elem.te = ptr_down_cast<expr_te>(te);
    te_list = ptr_down_cast<expr_telist>(
      expr_telist_new(this->fname, this->line, te, te_list));
    // fprintf(stderr, "embedded %s\n", te->dump(0).c_str());
    elems.push_back(elem);
    i = j + 1;
  }
}

void expr_inline_c::set_unique_namespace_one(const std::string& u,
  bool allow_unsafe)
{
  if (!allow_unsafe) {
    arena_error_throw(this,
      "Unsafe extern statement in a safe namespace");
  }
}

std::string expr_inline_c::dump(int indent) const
{
  std::string r = "inline_c\n";
  r += cstr;
  r += "end\n";
  return r;
}

expr_ns::expr_ns(const char *fn, int line, expr_i *uniq_nssym, bool import,
  bool pub, bool thr, const char *nsalias, const char *safety)
  : expr_i(fn, line), uniq_nssym(uniq_nssym),
    uniq_nsstr(get_full_name(uniq_nssym)),
    import(import), pub(pub), thr(thr), nsalias(nsalias), safety(safety)
{
  if (!import) {
    nssafety_e v = nssafety_e_safe;
    if (safety != 0) {
      std::string s(safety);
      if (s == "safe") {
	v = nssafety_e_safe;
      } else if (s == "export-unsafe") {
	v = nssafety_e_export_unsafe;
      } else if (s == "use-unsafe") {
	v = nssafety_e_use_unsafe;
      } else {
	arena_error_throw(this, "Invalid attribute '%s'", safety);
      }
    }
    nspropmap[uniq_nsstr].safety = v;
    nspropmap[uniq_nsstr].is_public = pub;
  }
}

void expr_ns::set_unique_namespace_one(const std::string& u, bool allow_unsafe)
{
  if (nsalias != 0) {
    assert(import); /* this must be a import statement */
    /* this is an 'import uniq_nsstr nsalias' statement in namespace 'u' */
    std::string astr(nsalias);
    /* astr can be a word, "-", "+", or "*" */
    if (astr == "+") {
      symbol_list& e = nsextends[uniq_nsstr];
      e.push_back(u);
      astr = "";
    } else if (astr == "*") {
      symbol_list& e = nschains[u];
      e.push_back(uniq_nsstr);
      astr = "";
    } else if (astr == "-") {
      astr = "";
    }
    /* 'u' imports 'uniq_nsstr' as prefix 'nsalias' */
    symbol_list& e = nsaliases[std::make_pair(u, astr)];
    e.push_back(uniq_nsstr);
  }
  if (import) {
    nsimports[u].push_back(std::make_pair(uniq_nsstr, pub));
  }
  src_uniq_nsstr = u;
}

std::string expr_ns::dump(int indent) const
{
  if (import) {
    if (pub) {
      return "public import " + uniq_nsstr;
    } else {
      return "private import " + uniq_nsstr;
    }
  } else {
    return "namespace " + uniq_nsstr;
  }
}

expr_nsmark::expr_nsmark(const char *fn, int line, bool end_mark)
  : expr_i(fn, line), end_mark(end_mark)
{
}

void expr_nsmark::set_unique_namespace_one(const std::string& u,
  bool allow_unsafe)
{
  uniqns = u;
}

std::string expr_nsmark::dump(int indent) const
{
  return "nsmark " + uniqns;
}

expr_int_literal::expr_int_literal(const char *fn, int line, const char *str,
  bool is_unsigned)
  : expr_i(fn, line), str(str), is_unsigned(is_unsigned)
{
  type_of_this_expr.clear();
}

term&
expr_int_literal::resolve_texpr()
{
  /* FIXME: range check */
  if (type_of_this_expr.is_null()) {
    if (is_unsigned) {
      if (str[0] == '\'') {
	type_of_this_expr = builtins.type_uchar;
      } else {
	const char *p = str;
	type_of_this_expr = builtins.type_uint;
	for (; *p != 0; ++p) {
	  if (*p == 'L' || *p == 'l') {
	    type_of_this_expr = builtins.type_ulong;
	  } else if (*p == 'S' || *p == 's') {
	    type_of_this_expr = builtins.type_ushort;
	  } else if (*p == 'O' || *p == 's') {
	    type_of_this_expr = builtins.type_uchar;
	  } else if (*p == 'Z' || *p == 'z') {
	    type_of_this_expr = builtins.type_size_t;
	  }
	}
      }
    } else {
      const char *p = str;
      type_of_this_expr = builtins.type_int;
      for (; *p != 0; ++p) {
	if (*p == 'L' || *p == 'l') {
	  type_of_this_expr = builtins.type_long;
	} else if (*p == 'S' || *p == 's') {
	  type_of_this_expr = builtins.type_short;
	} else if (*p == 'O' || *p == 'o') {
	  type_of_this_expr = builtins.type_char;
	} else if (*p == 'Z' || *p == 'o') {
	  type_of_this_expr = builtins.type_ssize_t;
	}
      }
    }
  }
  return type_of_this_expr;
}

bool
expr_int_literal::is_negative() const
{
  return str[0] == '-';
}

unsigned long long
expr_int_literal::get_value_nosig() const
{
  if (str[0] == '\'') {
    if (str[1] == '\\') {
      switch (str[2]) {
      case 'a': return '\a';
      case 'b': return '\b';
      case 'f': return '\f';
      case 'n': return '\n';
      case 'r': return '\r';
      case 't': return '\t';
      case 'v': return '\v';
      case '\'': return '\'';
      case '\"': return '\"';
      case '\\': return '\\';
      case '\?': return '\?';
      case '0': return '\0';
      default: return str[2];
      }
    }
    return static_cast<unsigned char>(str[1]); /* character */
  }
  /* decimal, hexadecimal, or octal */
  if (is_negative()) {
    return strtoull(str + 1, 0, 0);
  } else {
    return strtoull(str, 0, 0);
  }
}

long long
expr_int_literal::get_value_ll() const
{
  unsigned long long v = get_value_nosig();
  if (is_negative()) {
    v -= 1; /* in order not to overflow */
    long long sv = v;
    sv = -sv;
    sv -= 1;
    return sv;
  } else {
    return v;
  }
}

std::string expr_int_literal::dump(int indent) const
{
  return str;
}

expr_float_literal::expr_float_literal(const char *fn, int line,
  const char *str)
  : expr_i(fn, line), str(str)
{
  size_t s = strlen(str);
  if (s != 0 && (str[s - 1] == 'F' || str[s - 1] == 'f')) {
    type_of_this_expr = builtins.type_float;
  } else {
    type_of_this_expr = builtins.type_double;
  }
}

double expr_float_literal::get_value() const
{
  return strtod(str, 0);
}

std::string expr_float_literal::dump(int indent) const
{
  return str;
}

expr_bool_literal::expr_bool_literal(const char *fn, int line, bool v)
  : expr_i(fn, line), value(v)
{
  type_of_this_expr.clear();
}

term&
expr_bool_literal::resolve_texpr()
{
  if (type_of_this_expr.is_null()) {
    type_of_this_expr = builtins.type_bool;
  }
  return type_of_this_expr;
}

std::string expr_bool_literal::dump(int indent) const
{
  return value ? "true" : "false";
}

#if 0
static bool is_valid_hexadecimal_char(char ch)
{
  if (ch >= '0' && ch <= '9') {
    return true;
  }
  if (ch >= 'a' && ch <= 'f') {
    return true;
  }
  if (ch >= 'A' && ch <= 'F') {
    return true;
  }
  return false;
}

std::string unescape_c_str_literal(const char *str, bool& success_r)
{
  success_r = true;
  std::string s;
  const size_t len = strlen(str);
  for (size_t i = 0; i < len; ++i) {
    char ch = str[i];
    if (ch == '\\' && i + 1 < len) {
      ch = str[i + 1];
      ++i;
      if (ch == 'x') {
	if (i + 2 < len) {
	  const char c1 = str[i + 1];
	  const char c2 = str[i + 2];
	  if (!is_valid_hexadecimal_char(c1) ||
	    !is_valid_hexadecimal_char(c2)) {
	    success_r = false;
	  }
	  const unsigned char rch = (uchar_from_hexadecimal(c1) << 4) |
	    uchar_from_hexadecimal(c2);
	  s.push_back(rch);
	  i += 2;
	} else {
	  success_r = false;
	}
      } else if (ch == 'a' || ch == 'A') {
	s.push_back('\a');
      } else if (ch == 'b' || ch == 'B') {
	s.push_back('\b');
      } else if (ch == 'f' || ch == 'F') {
	s.push_back('\f');
      } else if (ch == 'n' || ch == 'N') {
	s.push_back('\n');
      } else if (ch == 'r' || ch == 'R') {
	s.push_back('\r');
      } else if (ch == 't' || ch == 'T') {
	s.push_back('\t');
      } else if (ch == 'v' || ch == 'V') {
	s.push_back('\v');
      } else if (ch == '\'') {
	s.push_back('\'');
      } else if (ch == '\"') {
	s.push_back('\"');
      } else if (ch == '\\') {
	s.push_back('\\');
      } else if (ch == '\?') {
	s.push_back('\?');
      } else if (ch == '0') {
	s.push_back('\0');
      } else {
	success_r = false;
      }
    }
  }
  return s;
}

std::string escape_c_str_literal(const std::string& s)
{
  std::string r;
  r.push_back('\"');
  const size_t sz = s.size();
  for (size_t i = 0; i < sz; ++i) {
    const unsigned char ch = s[i];
    if (ch == '\\' || ch == '\'' || ch == '\"' || ch < 0x20) {
      r.push_back(uchar_to_hexadecimal((ch >> 4) & 0x0f));
      r.push_back(uchar_to_hexadecimal(ch & 0x0f));
    } else {
      r.push_back(ch);
    }
  }
  r.push_back('\"');
  return r;
}
#endif

expr_str_literal::expr_str_literal(const char *fn, int line, const char *str)
  : expr_i(fn, line), raw_value(str), valid_flag(true)
{
  type_of_this_expr.clear();
  value = unescape_c_str_literal(raw_value, valid_flag);
  DBG_STRLIT(fprintf(stderr, "literal: raw=[%s] val=[%s] esc=[%s]\n",
    raw_value, value.c_str(), escape_c_str_literal(value).c_str()));
}

term&
expr_str_literal::resolve_texpr()
{
  if (type_of_this_expr.is_null()) {
    type_of_this_expr = builtins.type_strlit;
  }
  return type_of_this_expr;
}

std::string expr_str_literal::dump(int indent) const
{
  return escape_c_str_literal(value);
}

expr_nssym::expr_nssym(const char *fn, int line, expr_i *prefix,
  const char *sym)
  : expr_i(fn, line), prefix(prefix), sym(sym)
{
}

std::string expr_nssym::dump(int indent) const
{
  if (prefix != 0) {
    return dump_expr(indent, prefix) + "::" + sym;
  } else {
    return sym;
  }
}

expr_symbol::expr_symbol(const char *fn, int line, expr_i *nssym)
  : expr_i(fn, line), nssym(ptr_down_cast<expr_nssym>(nssym)), sdef(this)
{
  type_of_this_expr.clear(); /* check_type() */
  sdef.set_fullsym(get_full_name(nssym));
}

expr_i *expr_symbol::clone() const
{
  expr_symbol *const r = new expr_symbol(*this);
  r->sdef.parent_expr = r;
  r->sdef.set_symdef(0); // not necessary
  r->type_of_this_expr.clear();
  return r;
}

std::string expr_symbol::dump(int indent) const
{
  return sdef.get_fullsym().to_string();
}

expr_i *expr_symbol::resolve_symdef(symbol_table *lookup)
{
  /* called when te is eval()ed */
  assert(lookup == symtbl_lexical);
  return sdef.resolve_symdef();
}

const expr_i *expr_symbol::get_symdef() const
{
  return sdef.get_symdef();
}

term& expr_symbol::resolve_texpr()
{
  return resolve_texpr_symbol_common(this);
}

expr_var::expr_var(const char *fn, int line, const char *sym,
  expr_i *type_uneval, passby_e passby, attribute_e attr, expr_i *rhs_ref)
  : expr_i(fn, line), sym(sym),
    type_uneval(ptr_down_cast<expr_te>(type_uneval)),
    attr(attr), used_as_upvalue(false)
{
  varinfo.passby = passby;
  varinfo.scope_block = true;
  type_of_this_expr.clear(); /* resolve_texpr() */
}

expr_i *expr_var::clone() const
{
  expr_var *r = new expr_var(*this);
  r->type_of_this_expr.clear();
  return r;
}

term&
expr_var::resolve_texpr()
{
  if (type_of_this_expr.is_null()) {
    if (type_uneval != 0) {
      type_of_this_expr = eval_expr(type_uneval);
    } else {
      /* type inference */
      if (parent_expr->get_esort() == expr_e_op) {
	expr_op *eo = ptr_down_cast<expr_op>(parent_expr);
	if (eo->op == '=' && eo->arg0 == this && eo->arg1 != 0) {
	  type_of_this_expr = eo->arg1->resolve_texpr();
	}
      }
    }
    if (type_of_this_expr.is_null()) {
      arena_error_throw(this, "Type inference failed for variable '%s'", sym);
    }
  }
  return type_of_this_expr;
}

std::string expr_var::dump(int indent) const
{
  return "var " + dump_expr(indent, type_uneval);
}

expr_enumval::expr_enumval(const char *fn, int line, const char *sym,
  expr_i *type_uneval, const char *cname, expr_i *val, attribute_e attr,
  expr_i *rest)
  : expr_i(fn, line), sym(sym),
    type_uneval(ptr_down_cast<expr_te>(type_uneval)), cnamei(cname),
    value(val), attr(attr),
    rest(ptr_down_cast<expr_enumval>(rest))
{
  type_of_this_expr.clear(); /* resolve_texpr() */
}

expr_i *expr_enumval::clone() const
{
  expr_enumval *r = new expr_enumval(*this);
  r->type_of_this_expr.clear();
  return r;
}

void expr_enumval::set_unique_namespace_one(const std::string& u,
  bool allow_unsafe)
{
  if (!allow_unsafe && cnamei.has_cname()) {
    arena_error_throw(this,
      "Unsafe enum value is defined in a safe namespace");
  }
  uniqns = u;
}

term&
expr_enumval::resolve_texpr()
{
  if (type_of_this_expr.is_null()) {
    if (type_uneval == 0) {
      expr_i *p = parent_expr;
      if (p->get_esort() == expr_e_enumval) {
	type_of_this_expr = p->resolve_texpr();
      } else if (p->get_esort() == expr_e_typedef) {
	type_of_this_expr = p->get_value_texpr();
      } else {
	abort();
      }
    } else {
      type_of_this_expr = eval_expr(type_uneval);
    }
  }
  return type_of_this_expr;
}

std::string expr_enumval::dump(int indent) const
{
  return "enumval " + dump_expr(indent, type_uneval);
}

expr_stmts::expr_stmts(const char *fn, int line, expr_i *head, expr_i *rest)
  : expr_i(fn, line), head(head), rest(ptr_down_cast<expr_stmts>(rest))
{
}

std::string expr_stmts::dump(int indent) const
{
  std::string r;
  if (head != 0) {
    r += space_string(indent, 's');
    r += dump_expr(indent, head);
    switch (head->get_esort()) {
    case expr_e_block:
    case expr_e_if:
    case expr_e_while:
    case expr_e_for:
    case expr_e_forrange:
    case expr_e_feach:
    case expr_e_fldfe:
    case expr_e_funcdef:
      r += "\n";
      break;
    default:
      r += ";\n";
      break;
    }
  }
  r += dump_expr(indent, rest);
  return r;
}

static expr_tparams *convert_to_tparams(expr_i *tparams,
  expr_i *& tparams_error_r)
{
  if (tparams == 0) {
    return 0;
  }
  expr_e const es = tparams->get_esort();
  if (es == expr_e_tparams) {
    return ptr_down_cast<expr_tparams>(tparams);
  }
  if (es == expr_e_telist) {
    expr_telist *const tel = ptr_down_cast<expr_telist>(tparams);
    expr_i *const head = tel->head;
    if (head->get_esort() == expr_e_te) {
      expr_te *const te = ptr_down_cast<expr_te>(head);
      if (te->tlarg == 0 && te->nssym->prefix == 0) {
	/* ok, it's a symbol */
	return ptr_down_cast<expr_tparams>(
	  expr_tparams_new(tparams->fname, tparams->line, te->nssym->sym,
	    false, convert_to_tparams(tel->rest, tparams_error_r)));
      }
    }
  }
  tparams_error_r = tparams;
  return 0;
}

expr_block::expr_block(const char *fn, int line, expr_i *tparams,	
  expr_i *inherit, expr_i *argdecls, expr_i *rettype_uneval,
  passby_e ret_passby, expr_i *stmts)
  : expr_i(fn, line), block_id_ns(0),
    inherit(ptr_down_cast<expr_telist>(inherit)),
    argdecls(argdecls),
    rettype_uneval(ptr_down_cast<expr_te>(rettype_uneval)),
    ret_passby(ret_passby), /* NOTE: ret_passby is not implemented yet */
    stmts(ptr_down_cast<expr_stmts>(stmts)), tparams_error(0),
    symtbl(this), compiled_flag(false)
{
  /* sometimes tparams is parsed as expr_telist. we need to convert it to
   * expr_tparams. */
  tinfo.tparams = convert_to_tparams(tparams, tparams_error);
  #if 0
  tinfo.tparams = ptr_down_cast<expr_tparams>(tparams);
  #endif
}

expr_block *expr_block::clone() const
{
  expr_block *bl = new expr_block(*this);
  bl->symtbl.block_backref = bl;
  #if 0
  if (compiled_flag) {
    // template instantiation for example
    arena_error_throw(this, "Internal error: clone() for compiled block");
  }
  #endif
  bl->compiled_flag = false;
  return bl;
}

expr_argdecls *expr_block::get_argdecls() const
{
  return ptr_down_cast<expr_argdecls>(argdecls);
}

static void calc_inherit_transitive_rec(expr_i *pos,
  expr_block::inherit_list_type& lst, std::set<expr_i *>& s,
  std::set<expr_i *>& p, expr_telist *inh)
{
  for (expr_telist *i = inh; i != 0; i = i->rest) {
    term t = i->head->get_sdef()->resolve_evaluated();
    expr_i *const einst = term_get_instance(t);
    if (p.find(einst) != p.end()) {
      arena_error_throw(pos, "Inheritance loop detected: '%s'",
        term_tostr_human(t).c_str());
    } else if (s.find(einst) != s.end()) {
      /* skip */
    } else if (einst->get_esort() != expr_e_interface) {
      arena_error_throw(pos, "Not an interface: '%s'",
	term_tostr_human(t).c_str());
    } else {
      lst.push_back(einst);
      s.insert(einst);
      expr_interface *ei = ptr_down_cast<expr_interface>(einst);
      if (ei->block->inherit != 0) {
        p.insert(einst);
        calc_inherit_transitive_rec(pos, lst, s, p, ei->block->inherit);
        p.erase(einst);
      }
    }
  }
}

expr_block::inherit_list_type& expr_block::resolve_inherit_transitive()
{
  if (inherit == 0 || !inherit_transitive.empty()) {
    return inherit_transitive;
  }
  std::set<expr_i *> s;
  std::set<expr_i *> p; /* parents */
  calc_inherit_transitive_rec(this, inherit_transitive, s, p, inherit);
  return inherit_transitive;
}

std::string expr_block::dump(int indent) const
{
  std::string r;
  r += "{\n";
  r += dump_expr(indent + 1, stmts);
  r += space_string(indent, 'b');
  r += "}";
  return r;
}

expr_op::expr_op(const char *fn, int line, int op, const char *extop,
  expr_i *arg0, expr_i *arg1)
  : expr_i(fn, line), op(op), extop(extop == 0 ? "" : extop), arg0(arg0),
    arg1(arg1)
{
  type_of_this_expr.clear(); /* check_type() */
  if (this->extop != "" && this->extop != "placement-new") {
    arena_error_throw(this, "Invalid external operator '%s'",
      this->extop.c_str());
  }
}

expr_i *expr_op::clone() const
{
  expr_op *eop = new expr_op(*this);
  eop->type_of_this_expr.clear();
  return eop;
}

void expr_op::set_unique_namespace_one(const std::string& u,
  bool allow_unsafe)
{
  if (!allow_unsafe && extop != "") {
    arena_error_throw(this,
      "Unsafe operator is used in a safe namespace");
  }
}

std::string expr_op::dump(int indent) const
{
  std::string tok(tok_string(this, op));
  std::string s0 = dump_expr(indent, arg0);
  std::string s1 = dump_expr(indent, arg1);
  switch (op) {
  case ',':
  case TOK_OR_ASSIGN:
  case TOK_AND_ASSIGN:
  case TOK_XOR_ASSIGN:
  case TOK_SHIFTL_ASSIGN:
  case TOK_SHIFTR_ASSIGN:
  case TOK_ADD_ASSIGN:
  case TOK_SUB_ASSIGN:
  case TOK_MUL_ASSIGN:
  case TOK_DIV_ASSIGN:
  case TOK_MOD_ASSIGN:
  case '=':
  case '?':
  case ':':
  case TOK_LOR:
  case TOK_LAND:
  case '|':
  case '^':
  case '&':
  case TOK_EQ:
  case TOK_NE:
  case '<':
  case '>':
  case TOK_LE:
  case TOK_GE:
  case TOK_SHIFTL:
  case TOK_SHIFTR:
  case '+':
  case '-':
  case '*':
  case '/':
  case '%':
    return s0 + tok + s1;
  case TOK_INC:
  case TOK_DEC:
  case TOK_PTR_REF:
  case TOK_PTR_DEREF:
    return tok + s0;
  case '[':
    return s0 + "[" + s1 + "]";
  case '.':
  case TOK_ARROW:
    return s0 + tok + s1;
  case '(':
    return "(" + s0 + ")";
  }
  return "";
}

expr_funccall::expr_funccall(const char *fn, int line, expr_i *func,
  expr_i *arg)
  : expr_i(fn, line), func(func), arg(arg), funccall_sort(funccall_e_funccall)
{
  #if 0
  fprintf(stderr, "this=%p func=%p %d\n", this, this->func, tempvar_id);
  #endif
  type_of_this_expr.clear();
}

expr_i *expr_funccall::clone() const
{
  expr_funccall *ef = new expr_funccall(*this);
  ef->type_of_this_expr.clear();
  return ef;
}

std::string expr_funccall::dump(int indent) const
{
  std::string r = dump_expr(indent, func) + "(";
  if (arg) {
    r += dump_expr(indent, arg);
  }
  r += ")";
  return r;
}

expr_special::expr_special(const char *fn, int line, int tok, expr_i *arg)
  : expr_i(fn, line), tok(tok), arg(arg)
{
}

const char *expr_special::tok_str() const
{
  switch (tok) {
  case TOK_BREAK: return "break";
  case TOK_CONTINUE: return "continue";
  case TOK_RETURN: return "return";
  }
  abort();
  return "";
}

std::string expr_special::dump(int indent) const
{
  switch (tok) {
  case TOK_BREAK:
    return "break";
  case TOK_CONTINUE:
    return "continue";
  case TOK_RETURN:
    return arg ? "return " + dump_expr(indent, arg) : "return";
  }
  return "";
}

expr_if::expr_if(const char *fn, int line, expr_i *cond, expr_i *b1,
  expr_i *b2, expr_i *rest)
  : expr_i(fn, line), cond(cond),
    block1(ptr_down_cast<expr_block>(b1)),
    block2(ptr_down_cast<expr_block>(b2)),
    rest(ptr_down_cast<expr_if>(rest)),
    cond_static(-1)
{
}

std::string expr_if::dump(int indent) const
{
  std::string r;
  r += "if (" + dump_expr(indent, cond) + ")\n";
  r += space_string(indent, 'i');
  r += dump_expr(indent, block1);
  if (rest != 0) {
    /* else if ... */
    r += "else ";
    r += dump_expr(indent, rest);
  } else if (block2 != 0) {
    /* else */
    r += "else\n";
    r += space_string(indent, 'i');
    r += dump_expr(indent, block2);
  }
  return r;
}

expr_while::expr_while(const char *fn, int line, expr_i *cond, expr_i *block)
  : expr_i(fn, line), cond(cond),
    block(ptr_down_cast<expr_block>(block))
{
}

std::string expr_while::dump(int indent) const
{
  std::string r = "while (";
  r += dump_expr(indent, cond);
  r += ")\n";
  r += space_string(indent, 'w');
  r += dump_expr(indent, block);
  return r;
}

expr_for::expr_for(const char *fn, int line, expr_i *e0, expr_i *e1,
  expr_i *e2, expr_i *block)
  : expr_i(fn, line), e0(e0), e1(e1), e2(e2),
    block(ptr_down_cast<expr_block>(block))
{
}

std::string expr_for::dump(int indent) const
{
  std::string r = "for (";
  r += dump_expr(indent, e0);
  r += "; ";
  r += dump_expr(indent, e1);
  r += "; ";
  r += dump_expr(indent, e2);
  r += ")\n";
  r += space_string(indent, 'f');
  r += dump_expr(indent, block);
  return r;
}

expr_forrange::expr_forrange(const char *fn, int line, expr_i *r0, expr_i *r1,
  expr_i *block)
  : expr_i(fn, line), r0(r0), r1(r1), block(ptr_down_cast<expr_block>(block))
{
}

std::string expr_forrange::dump(int indent) const
{
  std::string r = "for (";
  r += dump_expr(indent, r0);
  r += " .. ";
  r += dump_expr(indent, r1);
  r += ")\n";
  r += space_string(indent, 'f');
  r += dump_expr(indent, block);
  return r;
}

expr_feach::expr_feach(const char *fn, int line, expr_i *ce, expr_i *block)
  : expr_i(fn, line), ce(ce), block(ptr_down_cast<expr_block>(block))
{
}

std::string expr_feach::dump(int indent) const
{
  std::string r = "foreach (";
  r += dump_expr(indent, ce);
  r += ")\n";
  r += space_string(indent, 'f');
  r += dump_expr(indent, block);
  return r;
}

expr_fldfe::expr_fldfe(const char *fn, int line, const char *namesym,
  const char *fldsym, const char *idxsym, expr_i *te, expr_i *stmts)
  : expr_i(fn, line), namesym(namesym), fldsym(fldsym),
    idxsym(idxsym),
    te(ptr_down_cast<expr_te>(te)),
    stmts(ptr_down_cast<expr_stmts>(stmts))
{
}

std::string expr_fldfe::dump(int indent) const
{
  std::string r = "fldfe ";
  r += dump_expr(indent, stmts);
  return r;
}

expr_foldfe::expr_foldfe(const char *fn, int line, const char *itersym,
  expr_i *valueste, const char *embedsym, expr_i *embedexpr,
  const char *foldop, expr_i *stmts)
  : expr_i(fn, line), itersym(itersym),
    valueste(ptr_down_cast<expr_te>(valueste)), embedsym(embedsym),
    embedexpr(embedexpr), foldop(foldop),
    stmts(ptr_down_cast<expr_stmts>(stmts))
{
}

std::string expr_foldfe::dump(int indent) const
{
  std::string r = "foldfe ";
  r += dump_expr(indent, stmts);
  return r;
}

expr_expand::expr_expand(const char *fn, int line, expr_i *callee,
  const char *itersym, const char *idxsym, expr_i *valueste,
  expr_i *baseexpr, expand_e ex, expr_i *rest)
  : expr_i(fn, line), itersym(itersym), idxsym(idxsym),
    valueste(ptr_down_cast<expr_te>(valueste)), baseexpr(baseexpr), ex(ex),
    rest(rest), callee(callee), generated_expr(0)
{
}

expr_i *expr_expand::clone() const
{
  expr_expand *e = new expr_expand(*this);
  e->generated_expr = 0;
  return e;
}

std::string expr_expand::dump(int indent) const
{
  std::string r = "expand ";
  r += dump_expr(indent, baseexpr);
  return r;
}



expr_argdecls::expr_argdecls(const char *fn, int line, const char *sym,
  expr_i *type_uneval, passby_e passby, expr_i *rest)
  : expr_i(fn, line), sym(sym),
    type_uneval(ptr_down_cast<expr_te>(type_uneval)), passby(passby),
    rest(rest), used_as_upvalue(false)
{
  type_of_this_expr.clear();
}

expr_i *expr_argdecls::clone() const
{
  expr_argdecls *r = new expr_argdecls(*this);
  r->type_of_this_expr.clear();
  return r;
}

expr_argdecls *expr_argdecls::get_rest() const
{
  return ptr_down_cast<expr_argdecls>(rest);
}

term&
expr_argdecls::resolve_texpr()
{
  if (type_of_this_expr.is_null()) {
    if (type_uneval != 0) {
      const bool need_partial_eval = cur_frame_uninstantiated(symtbl_lexical);
      type_of_this_expr = eval_expr(type_uneval, need_partial_eval);
    } else {
      /* type inference */
      expr_i *bl = parent_expr;
      while (bl->get_esort() != expr_e_block) {
	bl = bl->parent_expr;
      }
      expr_i *const ep = bl->parent_expr;
      if (ep->get_esort() == expr_e_feach) {
	expr_feach *const efe = ptr_down_cast<expr_feach>(ep);
	const bool is_key = parent_expr->get_esort() != expr_e_argdecls;
	if (is_key) {
	  type_of_this_expr = get_array_index_texpr(0,
	    efe->ce->resolve_texpr());
	} else {
	  type_of_this_expr = get_array_elem_texpr(0,
	    efe->ce->resolve_texpr());
	}
      } else if (ep->get_esort() == expr_e_forrange) {
	expr_forrange *const efr = ptr_down_cast<expr_forrange>(ep);
	term& t0 = efr->r0->resolve_texpr();
	term& t1 = efr->r1->resolve_texpr();
	const bool r0i = is_compiletime_intval(efr->r0);
	if (r0i) {
	  check_convert_type(efr->r0, t1);
	  type_of_this_expr = t1;
	} else {
	  check_convert_type(efr->r1, t0);
	  type_of_this_expr = t0;
	}
      } else if (ep->get_esort() == expr_e_if) {
	expr_if *const ei = ptr_down_cast<expr_if>(ep);
	type_of_this_expr = ei->cond->resolve_texpr();
      }
    }
    if (type_of_this_expr.is_null()) {
      arena_error_throw(this, "Type inference failed for variable '%s'", sym);
    }
    DBG_TE(fprintf(stderr,
      "argdecls %p %s %s:%d resolve_texpr uneval=%p type_of_this_expr=%p\n",
      this, sym, fname, line, type_uneval, type_of_this_expr.expr.expr));
  } else {
    DBG_TE(fprintf(stderr,
      "argdecls %p resolve_texpr skip uneval=%p type_of_this_expr=%p\n",
      this, type_uneval, type_of_this_expr.expr.expr));
  }
  return type_of_this_expr;
}

std::string expr_argdecls::dump(int indent) const
{
  std::string r = sym;
  r += ":" + dump_expr(indent, type_uneval);
  if (rest) {
    r += ", ";
    r += dump_expr(indent, rest);
  }
  return r;
}

expr_funcdef::expr_funcdef(const char *fn, int line, const char *sym,
  const char *cname, bool is_const, expr_i *block, bool ext_pxc, bool no_def,
  attribute_e attr)
  : expr_i(fn, line), sym(sym), cnamei(cname), is_const(is_const),
    rettype_eval(), block(ptr_down_cast<expr_block>(block)),
    ext_pxc(ext_pxc), no_def(no_def), value_texpr(), attr(attr),
    used_as_cfuncobj(false)
{
  assert(block);
  this->block->symtbl.block_esort = expr_e_funcdef;
  value_texpr = term(this);
}

expr_i *expr_funcdef::clone() const
{
  expr_funcdef *cpy = new expr_funcdef(*this);
  cpy->value_texpr = term(cpy);
  return cpy;
}

void expr_funcdef::set_unique_namespace_one(const std::string& u,
  bool allow_unsafe)
{
  if (!allow_unsafe && cnamei.has_cname()) {
    arena_error_throw(this,
      "Unsafe function is defined in a safe namespace");
  }
  uniqns = u;
}

bool expr_funcdef::is_global_function() const
{
  expr_block *const bl = block->symtbl.get_lexical_parent()->block_backref;
  return (bl->symtbl.get_lexical_parent() == 0);
}

expr_struct *expr_funcdef::is_member_function() const
{
  symbol_table *const p = block->symtbl.get_lexical_parent();
  if (p == 0) {
    return 0;
  }
  expr_i *fr = get_current_frame_expr(p);
  return dynamic_cast<expr_struct *>(fr);
}

expr_interface *expr_funcdef::is_virtual_function() const
{
  symbol_table *const p = block->symtbl.get_lexical_parent();
  if (p == 0) {
    return 0;
  }
  expr_i *fr = get_current_frame_expr(p);
  return dynamic_cast<expr_interface *>(fr);
}

expr_i *expr_funcdef::is_virtual_or_member_function() const
{
  expr_i *r = is_member_function();
  if (r != 0) {
    return r;
  }
  return is_virtual_function();
}

expr_funcdef *expr_funcdef::is_member_function_descent()
{
  expr_funcdef *efd = this;
  symbol_table *p = block->symtbl.get_lexical_parent();
  while (p != 0) {
    expr_i *fr = get_current_frame_expr(p);
    expr_struct *est = dynamic_cast<expr_struct *>(fr);
    if (est != 0) {
      return efd;
    }
    expr_funcdef *const efd_cur = dynamic_cast<expr_funcdef *>(fr);
    if (efd_cur != 0) {
      efd = efd_cur;
    }
    p = p->get_lexical_parent();
  }
  return 0;
}

const term& expr_funcdef::get_rettype()
{
  if (rettype_eval.is_null()) {
    if (block->rettype_uneval == 0) {
      rettype_eval = builtins.type_void;
    } else {
      const bool need_partial_eval = cur_frame_uninstantiated(symtbl_lexical);
      rettype_eval = eval_expr(block->rettype_uneval, need_partial_eval);
    }
  }
  return rettype_eval;
}

std::string expr_funcdef::dump(int indent) const
{
  std::string r = std::string("function ") + (sym ? sym : "[noname]") + "(";
  r += dump_expr(indent, block->argdecls);
  r += "):";
  r += dump_expr(indent, block->rettype_uneval);
  r += "\n";
  r += space_string(indent, 'w');
  r += dump_expr(indent, block);
  return r;
}

expr_typedef::expr_typedef(const char *fn, int line, const char *sym,
  const char *cname, const char *family, bool is_enum, bool is_bitmask,
  expr_i *enumvals, unsigned int num_tparams, attribute_e attr)
  : expr_i(fn, line), sym(sym), cnamei(cname), typefamily_str(family),
    is_enum(is_enum), is_bitmask(is_bitmask),
    enumvals(ptr_down_cast<expr_enumval>(enumvals)),
    num_tparams(num_tparams), attr(attr), tattr(),
    value_texpr(), typefamily(typefamily_e_none)
{
  value_texpr = term(this);
}

void expr_typedef::set_unique_namespace_one(const std::string& u,
  bool allow_unsafe)
{
  if (!allow_unsafe && cnamei.has_cname()) {
    arena_error_throw(this,
      "Unsafe type is defined in a safe namespace");
  }
  uniqns = u;
  typefamily = get_family_from_string(typefamily_str ? typefamily_str : "");
}

std::string expr_typedef::dump(int indent) const
{
  return std::string("tyepdef ") + sym + " "
    + (cnamei.cname != 0 ? cnamei.cname : sym)
    + " " + (typefamily_str != 0 ? typefamily_str : "");
}

expr_metafdef::expr_metafdef(const char *fn, int line, const char *sym,
  expr_i *tparams, expr_i *rhs, attribute_e attr)
  : expr_i(fn, line), sym(sym != 0 ? sym : ""),
    block(ptr_down_cast<expr_block>(expr_block_new(fn, line, tparams, 0, 0, 0,
      passby_e_mutable_value, expr_stmts_new(fn, line, rhs, 0)))),
    attr(attr)
{
}

expr_i *expr_metafdef::clone() const
{
  expr_metafdef *cpy = new expr_metafdef(*this);
  cpy->metafdef_term = term(); /* clear cached term */
  cpy->evaluated_term = term(); /* clear evaluated term */
  return cpy;
}

std::string expr_metafdef::dump(int indent) const
{
  return std::string("macro '") + sym + "' " + dump_expr(indent, get_rhs());
}

expr_struct::expr_struct(const char *fn, int line, const char *sym,
  const char *cname, const char *family, expr_i *block, attribute_e attr,
  bool has_udcon, bool private_udcon)
  : expr_i(fn, line), sym(sym), cnamei(cname), typefamily_str(family),
    block(ptr_down_cast<expr_block>(block)),
    attr(attr), value_texpr(), typefamily(typefamily_e_none),
    has_udcon(has_udcon), private_udcon(private_udcon), builtin_typestub(0),
    metafunc_strict(0), metafunc_nonstrict(0)
{
  assert(block);
  this->block->symtbl.block_esort = expr_e_struct;
  value_texpr = term(this);
}

expr_struct *expr_struct::clone() const
{
  expr_struct *cpy = new expr_struct(*this);
  cpy->value_texpr = term(cpy);
  return cpy;
}

void expr_struct::set_unique_namespace_one(const std::string& u,
  bool allow_unsafe)
{
  if (!allow_unsafe && cnamei.has_cname()) {
    arena_error_throw(this,
      "Unsafe type is defined in a safe namespace");
  }
  uniqns = u;
  typefamily = get_family_from_string(typefamily_str ? typefamily_str : "");
  if (typefamily_str != 0) {
    builtin_typestub = find_builtin_typestub(typefamily_str);
    metafunc_strict = find_builtin_strict_metafunction(typefamily_str);
    metafunc_nonstrict = find_builtin_nonstrict_metafunction(typefamily_str);
  }
}

void expr_struct::get_fields(std::list<expr_var *>& flds_r) const
{
  flds_r.clear();
  if (!block->compiled_flag) {
    arena_error_throw(this,
      "Failed to enumerate fields of struct '%s' which is not compiled yet",
      this->sym);
  }
  symbol_table& symtbl = block->symtbl;
  symbol_table::local_names_type::const_iterator i;
  for (i = symtbl.local_names.begin(); i != symtbl.local_names.end(); ++i) {
    const symbol_table::locals_type::const_iterator iter = symtbl.find(*i);
    assert(iter != symtbl.end());
    expr_var *const e = dynamic_cast<expr_var *>(iter->second.edef);
    if (e == 0) { continue; }
#if 0
// FIXME
    if (e->generated_flag) {
fprintf(stderr, "got generated %p %s\n", e, e->dump(0).c_str());
continue;
    }
#endif
    flds_r.push_back(e);
  }

}

bool expr_struct::has_userdefined_constr() const
{
  // return block != 0 && block->argdecls != 0;
  return has_udcon;
}

std::string expr_struct::dump(int indent) const
{
  std::string r = std::string("struct ") + sym;
  if (block->inherit != 0) {
    r += " : " + dump_expr(indent, block->inherit);
  }
  r += "\n";
  r += dump_expr(indent, block);
  return r;
}

expr_dunion::expr_dunion(const char *fn, int line, const char *sym,
  expr_i *block, attribute_e attr)
  : expr_i(fn, line), sym(sym),
    block(ptr_down_cast<expr_block>(block)), attr(attr), value_texpr()
{
  assert(block);
  this->block->symtbl.block_esort = expr_e_dunion;
  value_texpr = term(this);
}

expr_i *expr_dunion::clone() const
{
  expr_dunion *cpy = new expr_dunion(*this);
  cpy->value_texpr = term(cpy);
  return cpy;
}

void expr_dunion::set_unique_namespace_one(const std::string& u,
  bool allow_unsafe)
{
  #if 0
  // extern dunion is not implemented
  if (!allow_unsafe && cnamei.has_cname()) {
    arena_error_throw(this,
      "Unsafe type is defined in a safe namespace");
  }
  #endif
  uniqns = u;
}

void expr_dunion::get_fields(std::list<expr_var *>& flds_r) const
{
  /* copy of expr_struct::get_fields */
  flds_r.clear();
  if (!block->compiled_flag) {
    arena_error_throw(this,
      "Failed to enumerate fields of union '%s' which is not compiled yet",
      this->sym);
  }
  symbol_table& symtbl = block->symtbl;
  symbol_table::local_names_type::const_iterator i;
  for (i = symtbl.local_names.begin(); i != symtbl.local_names.end(); ++i) {
    const symbol_table::locals_type::const_iterator iter = symtbl.find(*i);
    assert(iter != symtbl.end());
    expr_var *const e = dynamic_cast<expr_var *>(iter->second.edef);
    if (e == 0) { continue; }
    flds_r.push_back(e);
  }
}
expr_var *expr_dunion::get_first_field() const
{
  if (!block->compiled_flag) {
    arena_error_throw(this,
      "Failed to enumerate fields of union '%s' which is not compiled yet",
      this->sym);
  }
  symbol_table& symtbl = block->symtbl;
  symbol_table::local_names_type::const_iterator i;
  for (i = symtbl.local_names.begin(); i != symtbl.local_names.end(); ++i) {
    const symbol_table::locals_type::const_iterator iter = symtbl.find(*i);
    assert(iter != symtbl.end());
    expr_var *const e = dynamic_cast<expr_var *>(iter->second.edef);
    if (e == 0) { continue; }
    return e;
  }
  return 0;
}

std::string expr_dunion::dump(int indent) const
{
  std::string r = std::string("dunion ") + sym + "\n";
  r += dump_expr(indent, block);
  return r;
}

expr_interface::expr_interface(const char *fn, int line, const char *sym,
  const char *cname, expr_i *block, attribute_e attr)
  : expr_i(fn, line), sym(sym), cnamei(cname),
    block(ptr_down_cast<expr_block>(block)), attr(attr), value_texpr()
{
  assert(block);
  this->block->symtbl.block_esort = expr_e_interface;
  value_texpr = term(this);
}

expr_i *expr_interface::clone() const
{
  expr_interface *cpy = new expr_interface(*this);
  cpy->value_texpr = term(cpy);
  return cpy;
}

void expr_interface::set_unique_namespace_one(const std::string& u,
  bool allow_unsafe)
{
  if (!allow_unsafe && cnamei.has_cname()) {
    arena_error_throw(this,
      "Unsafe type is defined in a safe namespace");
  }
  uniqns = u;
}

std::string expr_interface::dump(int indent) const
{
  std::string r = std::string("interface ") + sym + "\n";
  r += dump_expr(indent, block);
  return r;
}

expr_try::expr_try(const char *fn, int line, expr_i *tblock, expr_i *cblock,
  expr_i *rest)
  : expr_i(fn, line), tblock(ptr_down_cast<expr_block>(tblock)),
    cblock(ptr_down_cast<expr_block>(cblock)),
    rest(ptr_down_cast<expr_try>(rest))
{
}

std::string expr_try::dump(int indent) const
{
  std::string r;
  if (tblock != 0) {
    r += std::string("try ");
    r += dump_expr(indent, tblock);
  }
  r += std::string("catch (") + dump_expr(indent, cblock->argdecls) + ") ";
  r += dump_expr(indent, cblock);
  r += dump_expr(indent, rest);
  return r;
}

expr_tparams::expr_tparams(const char *fn, int line, const char *sym,
  bool is_variadic_metaf, expr_i *rest)
  : expr_i(fn, line), sym(sym), is_variadic_metaf(is_variadic_metaf),
    rest(ptr_down_cast<expr_tparams>(rest)), param_def()
{
}

std::string expr_tparams::dump(int indent) const
{
  std::string r;
  r += std::string(sym);
  if (rest != 0) {
    r += ", ";
    r += dump_expr(indent, rest);
  }
  return r;
}

};

