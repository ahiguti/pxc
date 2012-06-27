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

#include "expr_impl.hpp"
#include "eval.hpp"
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
    tempvar_passby(passby_e_unspecified), require_lvalue(false)
{
  expr_arena.push_back(this);
  type_of_this_expr = builtins.type_void;
}

expr_i::expr_i(const expr_i& e)
  : fname(e.fname), line(e.line), type_of_this_expr(e.type_of_this_expr),
    conv(e.conv), type_conv_to(e.type_conv_to), parent_expr(e.parent_expr),
    symtbl_lexical(e.symtbl_lexical), tempvar_id(e.tempvar_id),
    require_lvalue(e.require_lvalue)
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
    const std::string lookup_ns = arg_hidden_this ? arg_hidden_this_ns : ns;
    if (lookup == 0) {
      lookup = parent_expr->symtbl_lexical;
    }
    symbol_def = lookup->resolve_name(sym_prefix + fullsym, lookup_ns,
      parent_expr, is_global, is_upvalue);
    DBG_TE2(fprintf(stderr,
      "expr_te::resolve_symdef: this=%p symtbl=%p [%s]\n", this,
      lookup, symbol_def->dump(0).c_str()));
    symtbl_defined = lookup;
    upvalue_flag = is_upvalue;
    assert(symbol_def);
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
  symbol_def = e;
}

term& symbol_common::resolve_evaluated()
{
  if (evaluated.is_null()) {
    evaluated = eval_expr(parent_expr);
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
  sdef.fullsym = get_full_name(nssym);
  DBG_TE1(fprintf(stderr, "te %p %s\n", this,
    term_tostr_human(type_of_this_expr).c_str()));
}

expr_i *expr_te::clone() const
{
  expr_te *const r = new expr_te(*this);
  /* argdecls have incomplete symbol_def values before instantiated */
  r->sdef.parent_expr = r;
  r->sdef.set_symdef(0);
  r->type_of_this_expr.clear();
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

expr_i *expr_te::resolve_symdef(symbol_table *lookup)
{
  /* called when te is eval()ed */
  assert(lookup == symtbl_lexical);
  return sdef.resolve_symdef();
  #if 0
  if (sdef.symbol_def == 0) {
    bool is_global = false, is_upvalue = false;
    assert(lookup == symtbl_lexical);
    sdef.symbol_def = symtbl_lexical->resolve_name(sdef.fullsym, sdef.ns, this,
      is_global, is_upvalue);
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
    arena_error_throw(this, "internal error: expr_te::get_symdef");
  }
  return sdef.symbol_def;
  #endif
}

static void symdef_check_threading_attr_type_or_func(expr_i *e,
  symbol_common *sc, term& t)
{
  symbol_table *const cur = get_current_frame_symtbl(e->symtbl_lexical);
  if (cur == 0 || !is_threaded_context(cur)) {
    return;
  }
  if (!term_is_threaded(t)) {
    arena_error_push(e,
      "type or function '%s' for symbol '%s' is not threaded",
      term_tostr_human(t).c_str(), sc->fullsym.c_str());
  }
  if (is_multithr_context(cur) && !term_is_multithr(t)) {
    arena_error_push(e,
      "type or function '%s' for symbol '%s' is not multithreaded",
      term_tostr_human(t).c_str(), sc->fullsym.c_str());
  }
}

static void symdef_check_threading_attr_var(expr_i *e, symbol_common *sc,
  expr_i *def_expr)
{
  symbol_table *const cur = get_current_frame_symtbl(e->symtbl_lexical);
  if (cur == 0 || !is_threaded_context(cur)) {
#if 0
fprintf(stderr, "symdef_check_threading_attr_var: %s : not threaded\n",
  sc->fullsym.c_str());
#endif
    return;
  }
#if 0
fprintf(stderr, "symdef_check_threading_attr_var: %s : threaded def_expr=%s\n",
  sc->fullsym.c_str(), def_expr->dump(0).c_str());
#endif
  if (is_global_var(def_expr)) {
#if 0
fprintf(stderr, "global var!\n");
#endif
    const expr_var *const ev = ptr_down_cast<const expr_var>(def_expr);
    arena_error_push(e,
      "global variable '%s' can't be used from a threaded context",
      ev->sym);
  }
}

static term& resolve_texpr_symbol_common(expr_i *e)
{
  symbol_common *const sc = e->get_sdef();
  assert(sc);
  // sc->resolve_symdef(); // FIXME remove?
  assert(sc->get_symdef() != 0); // FIXME?
  if (!e->type_of_this_expr.is_null()) {
    DBG_TE1(fprintf(stderr, "resolve_texpr_symbol_common cached: %p -> %s\n",
      e, term_tostr_human(e->type_of_this_expr).c_str()));
    return e->type_of_this_expr;
  }
  /* if it's a type/func symbol, evaluate it */
  term& evaluated = sc->resolve_evaluated();
  DBG_SYM(fprintf(stderr, "expr_symbol symdef=%p '%s' symdef_te=%s\n",
    sc->symbol_def, symbol_def->dump(0).c_str(),
    term_tostr_cname(evaluated).c_str()));
  DBG_TE1(fprintf(stderr, "resolve_texpr_symbol_common evaluated: %s\n",
    term_tostr_human(evaluated).c_str()));
  if (evaluated.is_long()) {
    /* int literal */
    e->type_of_this_expr = builtins.type_int;
    DBG_TE1(fprintf(stderr,
      "resolve_texpr_symbol_common tote: '%s' %p -> int\n",
      term_tostr_human(evaluated).c_str(), e));
  } else if (evaluated.is_string()) {
    /* string literal */
    e->type_of_this_expr = builtins.type_string;
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
    arena_error_throw(e, "invalid metaexpression '%s'",
      term_tostr_human(evaluated).c_str());
    #endif
  } else if (is_function(evaluated)) {
    /* function symbol has the value of function */
    e->type_of_this_expr = evaluated;
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
    e, sc->fullsym.c_str(), sc->get_symdef()->dump(0).c_str(),
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
    return "[" + sdef.fullsym + "]";
  }
  return "[" + sdef.fullsym + "(" + dump_expr(indent, tlarg) + ")]";
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

expr_inline_c::expr_inline_c(const char *fn, int line, const char *posstr,
  const char *cstr, bool declonly)
  : expr_i(fn, line), posstr(posstr), cstr(cstr), declonly(declonly)
{
}

std::string expr_inline_c::dump(int indent) const
{
  std::string r = "inline_c\n";
  r += cstr;
  r += "end\n";
  return r;
}

expr_ns::expr_ns(const char *fn, int line, expr_i *nssym, bool import,
  bool pub, const char *nsalias)
  : expr_i(fn, line), nssym(nssym), nsstr(get_full_name(nssym)),
    import(import), pub(pub), nsalias(nsalias)
{
}

void expr_ns::set_namespace_one(const std::string& n)
{
  if (nsalias != 0) {
    const std::string astr(nsalias);
    nsalias_entries& e = nsaliases[std::make_pair(n, astr)];
    e.push_back(nsstr);
  }
}

std::string expr_ns::dump(int indent) const
{
  if (import) {
    if (pub) {
      return "public import " + nsstr;
    } else {
      return "private import " + nsstr;
    }
  } else {
    return "namespace " + nsstr;
  }
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
  if (type_of_this_expr.is_null()) {
    if (is_unsigned) {
      type_of_this_expr = builtins.type_ulong;
    } else {
      type_of_this_expr = builtins.type_long;
    }
  }
  return type_of_this_expr;
}

unsigned long long
expr_int_literal::get_unsigned() const
{
  return strtoull(str, 0, 10);
}

long long
expr_int_literal::get_signed() const
{
  return strtoll(str, 0, 10);
}

std::string expr_int_literal::dump(int indent) const
{
  return str;
}

expr_float_literal::expr_float_literal(const char *fn, int line,
  const char *str)
  : expr_i(fn, line), str(str)
{
  type_of_this_expr = builtins.type_double;
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
    type_of_this_expr = builtins.type_string;
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
  sdef.fullsym = get_full_name(nssym);
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
  return sdef.fullsym;
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
  expr_i *type_uneval, attribute_e attr)
  : expr_i(fn, line), sym(sym),
    type_uneval(ptr_down_cast<expr_te>(type_uneval)), attr(attr)
{
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
    type_of_this_expr = eval_expr(type_uneval);
  }
  return type_of_this_expr;
}

std::string expr_var::dump(int indent) const
{
  return "var " + dump_expr(indent, type_uneval);
}

expr_extval::expr_extval(const char *fn, int line, const char *sym,
  expr_i *type_uneval, const char *cname, attribute_e attr)
  : expr_i(fn, line), sym(sym),
    type_uneval(ptr_down_cast<expr_te>(type_uneval)), cname(cname),
    attr(attr)
{
  type_of_this_expr.clear(); /* resolve_texpr() */
}

expr_i *expr_extval::clone() const
{
  expr_extval *r = new expr_extval(*this);
  r->type_of_this_expr.clear();
  return r;
}

term&
expr_extval::resolve_texpr()
{
  if (type_of_this_expr.is_null()) {
    type_of_this_expr = eval_expr(type_uneval);
  }
  return type_of_this_expr;
}

std::string expr_extval::dump(int indent) const
{
  return "extern " + dump_expr(indent, type_uneval);
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

expr_block::expr_block(const char *fn, int line, expr_i *tparams,	
  expr_i *inherit, expr_i *argdecls, expr_i *rettype_uneval, expr_i *stmts)
  : expr_i(fn, line), block_id_ns(0),
    inherit(ptr_down_cast<expr_telist>(inherit)),
    argdecls(ptr_down_cast<expr_argdecls>(argdecls)),
    rettype_uneval(ptr_down_cast<expr_te>(rettype_uneval)),
    stmts(ptr_down_cast<expr_stmts>(stmts)),
    symtbl(this)
{
  tinfo.tparams = ptr_down_cast<expr_tparams>(tparams);
}

expr_block *expr_block::clone() const
{
  expr_block *bl = new expr_block(*this);
  bl->symtbl.block_backref = bl;
  return bl;
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

expr_op::expr_op(const char *fn, int line, int op, expr_i *arg0, expr_i *arg1)
  : expr_i(fn, line), op(op), arg0(arg0), arg1(arg1), need_guard(false)
{
  type_of_this_expr.clear(); /* check_type() */
}

expr_i *expr_op::clone() const
{
  expr_op *eop = new expr_op(*this);
  eop->type_of_this_expr.clear();
  return eop;
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
  const char *fldsym, expr_i *te, expr_i *stmts)
  : expr_i(fn, line), namesym(namesym), fldsym(fldsym),
    te(ptr_down_cast<expr_te>(te)), stmts(ptr_down_cast<expr_stmts>(stmts))
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



expr_argdecls::expr_argdecls(const char *fn, int line, const char *sym,
  expr_i *type_uneval, bool byref_flag, expr_i *rest)
  : expr_i(fn, line), sym(sym),
    type_uneval(ptr_down_cast<expr_te>(type_uneval)),
    byref_flag(byref_flag), rest(ptr_down_cast<expr_argdecls>(rest))
{
  type_of_this_expr.clear();
}

expr_i *expr_argdecls::clone() const
{
  expr_argdecls *r = new expr_argdecls(*this);
  r->type_of_this_expr.clear();
  return r;
}

term&
expr_argdecls::resolve_texpr()
{
  if (type_of_this_expr.is_null()) {
    type_of_this_expr = eval_expr(type_uneval);
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
  const char *cname, bool is_const, expr_i *block, bool ext_decl, bool no_def,
  attribute_e attr)
  : expr_i(fn, line), sym(sym), cname(cname), is_const(is_const),
    rettype_eval(), block(ptr_down_cast<expr_block>(block)),
    ext_decl(ext_decl), no_def(no_def), value_texpr(), tpup_thisptr(0),
    tpup_thisptr_nonconst(false), attr(attr)
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

bool expr_funcdef::is_virtual_or_member_function() const
{
  return is_member_function() || is_virtual_function();
}

#if 0
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
#endif

const term& expr_funcdef::get_rettype()
{
  if (rettype_eval.is_null()) {
    rettype_eval = eval_expr(block->rettype_uneval);
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
  const char *cname, const char *category, bool is_pod,
  unsigned int num_tparams, attribute_e attr)
  : expr_i(fn, line), sym(sym), cname(cname), category(category),
    is_pod(is_pod), num_tparams(num_tparams), attr(attr), value_texpr()
{
  value_texpr = term(this);
}

std::string expr_typedef::dump(int indent) const
{
  return std::string("tyepdef ") + sym + " " + (cname != 0 ? cname : sym)
    + " " + (category != 0 ? category : "");
}

expr_macrodef::expr_macrodef(const char *fn, int line, const char *sym,
  expr_i *tparams, expr_i *rhs, attribute_e attr)
  : expr_i(fn, line), sym(sym),
    block(ptr_down_cast<expr_block>(expr_block_new(fn, line, tparams, 0, 0, 0,
      expr_stmts_new(fn, line, rhs, 0)))),
    attr(attr)
{
}

std::string expr_macrodef::dump(int indent) const
{
  return std::string("macro ") + sym + " " + dump_expr(indent, get_rhs());
}

#if 0
expr_inherit::expr_inherit(const char *fn, int line, expr_i *nssym,
  expr_i *rest)
  : expr_i(fn, line), nssym(ptr_down_cast<expr_nssym>(nssym)),
    rest(ptr_down_cast<expr_inherit>(rest)), fullsym(get_full_name(nssym)),
    symbol_def(0)
{
}

expr_i *expr_inherit::resolve_symdef()
{
  if (symbol_def == 0) {
    bool is_global = false, is_upvalue = false;
    symbol_def = symtbl_lexical->resolve_name(fullsym, ns, this, is_global,
      is_upvalue);
    if (symbol_def->get_esort() != expr_e_interface) {
      arena_error_throw(this, "'%s' is not an interface", fullsym.c_str());
    }
  }
  return symbol_def;
}

const expr_i *expr_inherit::get_symdef() const
{
  if (symbol_def == 0) {
    arena_error_throw(this, "internal error: expr_inherit::get_symdef");
  }
  return symbol_def;
}

std::string expr_inherit::dump(int indent) const
{
  std::string r = fullsym;
  if (rest != 0) {
    r += " ";
    r += dump_expr(indent, rest);
  }
  return r;
}
#endif

expr_struct::expr_struct(const char *fn, int line, const char *sym,
  const char *cname, const char *category, expr_i *block, attribute_e attr)
  : expr_i(fn, line), sym(sym), cname(cname), category(category),
    block(ptr_down_cast<expr_block>(block)),
    attr(attr), value_texpr()
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

void expr_struct::get_fields(std::list<expr_var *>& flds_r) const
{
  flds_r.clear();
  symbol_table& symtbl = block->symtbl;
  symbol_table::local_names_type::const_iterator i;
  for (i = symtbl.local_names.begin(); i != symtbl.local_names.end(); ++i) {
    const symbol_table::locals_type::const_iterator iter
      = symtbl.locals.find(*i);
    assert(iter != symtbl.locals.end());
    expr_var *const e = dynamic_cast<expr_var *>(iter->second.edef);
    if (e == 0) { continue; }
    flds_r.push_back(e);
  }

}

bool expr_struct::has_userdefined_constr() const
{
  return block != 0 && block->argdecls != 0;
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

expr_variant::expr_variant(const char *fn, int line, const char *sym,
  expr_i *block, attribute_e attr)
  : expr_i(fn, line), sym(sym),
    block(ptr_down_cast<expr_block>(block)), attr(attr), value_texpr()
{
  assert(block);
  this->block->symtbl.block_esort = expr_e_variant;
  value_texpr = term(this);
}

expr_i *expr_variant::clone() const
{
  expr_variant *cpy = new expr_variant(*this);
  cpy->value_texpr = term(cpy);
  return cpy;
}

void expr_variant::get_fields(std::list<expr_var *>& flds_r) const
{
  /* copy of expr_struct::get_fields */
  flds_r.clear();
  symbol_table& symtbl = block->symtbl;
  symbol_table::local_names_type::const_iterator i;
  for (i = symtbl.local_names.begin(); i != symtbl.local_names.end(); ++i) {
    const symbol_table::locals_type::const_iterator iter
      = symtbl.locals.find(*i);
    assert(iter != symtbl.locals.end());
    expr_var *const e = dynamic_cast<expr_var *>(iter->second.edef);
    if (e == 0) { continue; }
    flds_r.push_back(e);
  }
}

std::string expr_variant::dump(int indent) const
{
  std::string r = std::string("variant ") + sym + "\n";
  r += dump_expr(indent, block);
  return r;
}

expr_interface::expr_interface(const char *fn, int line, const char *sym,
  expr_i *block, attribute_e attr)
  : expr_i(fn, line), sym(sym), block(ptr_down_cast<expr_block>(block)),
    attr(attr), value_texpr()
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
  expr_i *rest)
  : expr_i(fn, line), sym(sym), rest(ptr_down_cast<expr_tparams>(rest)),
    param_def()
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

