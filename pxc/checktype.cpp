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
#include <signal.h>

#define DBG(x)
#define DBG_SYM(x)
#define DBG_CT_BLOCK(x)
#define DBG_CALLEE(x)
#define DBG_CTV(x)
#define DBG_UP(x)
#define DBG_FLDFE(x)
#define DBG_CHECKROOT(x)
#define DBG_TMPVAR(x)

namespace pxc {

std::string op_message(expr_op *eop)
{
  if (eop == 0) {
    return std::string();
  }
  const std::string opstr = std::string("(near operator '")
    + tok_string(eop, eop->op) + "')";
  return opstr;
}

static void check_type_convert_to_lhs(expr_op *eop, expr_i *efrom, term& tto)
{
  tvmap_type tvmap;
  if (!convert_type(efrom, tto, tvmap)) {
    const std::string sfrom = term_tostr(efrom->resolve_texpr(),
      term_tostr_sort_humanreadable);
    const std::string sto = term_tostr(tto,
      term_tostr_sort_humanreadable);
    arena_error_push(efrom, "cannot convert from '%s' to '%s' %s",
      sfrom.c_str(), sto.c_str(), op_message(eop).c_str());
  }
}

static void check_bool_expr(expr_op *eop, expr_i *a0)
{
  const term& t = a0->resolve_texpr();
  if (t == builtins.type_bool) {
    return;
  }
  /* template expression? */
  symbol_common *const sdef = a0->get_sdef();
  if (sdef != 0 && sdef->resolve_evaluated().is_long()) {
    const long long v = sdef->resolve_evaluated().get_long();
    if (v == 0 || v == 1) {
      return;
    }
  }
  const std::string s0 = term_tostr(a0->resolve_texpr(),
    term_tostr_sort_humanreadable);
  arena_error_push(eop, "bool type expected (got: %s) %s",
    s0.c_str(), op_message(eop).c_str());
}

static void check_integral_expr(expr_op *eop, expr_i *a0)
{
  const term t = a0->resolve_texpr();
  if (is_integral_type(t)) {
    return;
  }
  const std::string s0 = term_tostr(a0->resolve_texpr(),
    term_tostr_sort_humanreadable);
  arena_error_push(eop, "integral type expected (got: %s) %s",
    s0.c_str(), op_message(eop).c_str());
}

static void check_numeric_expr(expr_op *eop, expr_i *a0)
{
  const term t = a0->resolve_texpr();
  if (is_numeric_type(t)) {
    return;
  }
  const std::string s0 = term_tostr(a0->resolve_texpr(),
    term_tostr_sort_humanreadable);
  arena_error_push(eop, "numeric type expected (got: %s) %s",
    s0.c_str(), op_message(eop).c_str());
}

static void check_lvalue(const expr_i *epos, expr_i *a0)
{
  /* check if a0 is a valid lvalue expression */
  a0->require_lvalue = true;
  if (a0->get_sdef() != 0) {
    /* symbol or te */
    symbol_common *const sc = a0->get_sdef();
    const bool upvalue_flag = sc->upvalue_flag;
    const expr_e astyp = sc->resolve_symdef()->get_esort();
    if (astyp == expr_e_var) {
      if (upvalue_flag) {
	symbol_table *symtbl = find_current_symbol_table(sc->resolve_symdef());
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
    } else if (astyp == expr_e_argdecls) {
      expr_argdecls *const ead = ptr_down_cast<expr_argdecls>(
	sc->resolve_symdef());
      if (!ead->byref_flag) {
	arena_error_push(epos, "argument '%s' can not be modified",
	  sc->fullsym.c_str());
      }
    } else {
      arena_error_push(epos, "symbol '%s' can not be modified",
	sc->fullsym.c_str());
    }
    return;
  }
  if (a0->get_esort() == expr_e_op) {
    const expr_op *const aop = ptr_down_cast<const expr_op>(a0);
    switch (aop->op) {
    case '.':
    case TOK_ARROW:
      /* expr 'foo.bar' has an lvalue iff foo has an lvalue */
      return check_lvalue(aop, aop->arg0);
    case '[':
      /* expr 'foo[]' has an lvalue iff foo has an lvalue */
      return check_lvalue(aop, aop->arg0);
    case '(':
      /* expr '(foo)' has an lvalue iff foo has an lvalue */
      return check_lvalue(aop, aop->arg0);
    case TOK_PTR_DEREF:
      #if 0
      /* expr '*foo' has an lvalue iff foo has an lvalue */
      /* enable this? constness should be transitive? */
      /* if so, we need to reject copying ref{foo} if rhs has no lvalue */
      check_lvalue(aop, aop->arg0);
      #endif
      /* expr '*foo' has an lvalue iff foo is a non-const ref */
      if (is_const_pointer(aop->arg0->resolve_texpr())) {
	arena_error_push(epos, "can not modify data via a const reference");
      }
      return; /* ok */
    default:
      break;
    }
    arena_error_push(epos, "can not be modified"); // FIXME: error message
    return;
  }
  if (a0->get_esort() == expr_e_var) {
    return; /* ok */
  }
  arena_error_push(epos, "can not be modified"); // FIXME: error message
}

static term get_pointer_deref_texpr(expr_op *eop, const term& t)
{
  const term tg = get_pointer_target(t);
  if (tg.is_null()) {
    arena_error_throw(eop, "invalid dereference");
    return builtins.type_void;
  }
  return tg;
}

static term get_array_slice_texpr(expr_op *eop, term& t0)
{
  term slt = eval_local_lookup(t0, "range_type", eop);
  if (slt.is_null()) {
    arena_error_throw(eop, "cannot apply '[ .. ]'");
    return builtins.type_void;
  }
fprintf(stderr, "slice: %s -> %s\n", term_tostr_human(t0).c_str(),
  term_tostr_human(slt).c_str());
  return slt;
}

static term get_array_elem_texpr(expr_op *eop, term& t0)
{
  /* extern struct? */
  expr_i *const einst = term_get_instance(t0);
  const expr_struct *const est = dynamic_cast<const expr_struct *>(einst);
  if (est != 0) {
    const expr_tparams *tparams = est->block->tinfo.tparams;
    if (tparams != 0) {
      if (std::string(est->category) == "varray") {
	return tparams->param_def;
      } else if (std::string(est->category) == "farray") {
	return tparams->param_def;
      } else if (std::string(est->category) == "slice") {
	return tparams->param_def;
      } else if (std::string(est->category) == "map") {
	if (tparams->rest != 0) {
	  return tparams->rest->param_def;
	}
      }
    }
  }
  arena_error_throw(eop, "cannot apply '[]'");
  return builtins.type_void;
}

static term get_array_index_texpr(expr_op *eop, term& t0)
{
  /* extern struct? */
  expr_i *const einst = term_get_instance(t0);
  const expr_struct *const est = dynamic_cast<const expr_struct *>(einst);
  if (est != 0) {
    const expr_tparams *tparams = est->block->tinfo.tparams;
    if (tparams != 0) {
      if (std::string(est->category) == "varray") {
	return builtins.type_size_t;
      } else if (std::string(est->category) == "farray") {
	return builtins.type_size_t;
      } else if (std::string(est->category) == "slice") {
	return builtins.type_size_t;
      } else if (std::string(est->category) == "map") {
	if (tparams->rest != 0) {
	  return tparams->param_def;
	}
      }
    }
  }
  arena_error_throw(eop, "cannot apply '[]'");
  return builtins.type_void;
}

static void append_hidden_this(expr_i *e, std::list<expr_i *>& lst)
{
  expr_i *rhs = get_op_rhs(e, '.');
  if (rhs == 0) {
    rhs = get_op_rhs(e, TOK_ARROW);
    if (rhs == 0) {
      return;
    }
  }
  symbol_common *const sc = rhs->get_sdef();
  assert(sc);
  if (sc->arg_hidden_this) {
    lst.push_back(sc->arg_hidden_this);
  }
}

static void append_comma_sep_list(expr_i *e, std::list<expr_i *>& lst)
{
  expr_op *const eop = dynamic_cast<expr_op *>(e);
  if (eop == 0 || eop->op != ',') {
    if (e != 0) {
      lst.push_back(e);
    }
    return;
  }
  append_comma_sep_list(eop->arg0, lst);
  if (eop->arg1 != 0) {
    lst.push_back(eop->arg1);
  }
}

#if 0
static expr_i **get_comma_sep_first_ptr(expr_i **e)
{
  while (true) {
    DBG_FLDFE(fprintf(stderr, "g %s\n", (*e)->dump(0).c_str()));
    expr_op *const eop = dynamic_cast<expr_op *>(*e);
    if (eop == 0 || eop->op != ',') {
      break;
    }
    e = &eop->arg0;
  }
  return e;
}
#endif

static void check_type_symbol_common(expr_i *e, symbol_table *lookup)
{
  symbol_common *const sc = e->get_sdef();
  assert(sc);
  sc->resolve_symdef(lookup);
  /* don't evaluate here, so that macro rhs can be evaluated lazily */
}

void expr_telist::check_type(symbol_table *lookup)
{
  fn_check_type(head, lookup);
  fn_check_type(rest, lookup);
}

void expr_te::check_type(symbol_table *lookup)
{
  fn_check_type(tlarg, symtbl_lexical);
    /* tlarg always uses lexical context */
  check_type_symbol_common(this, lookup);
}

void expr_symbol::check_type(symbol_table *lookup)
{
  check_type_symbol_common(this, lookup);
}

void expr_inline_c::check_type(symbol_table *lookup)
{
  if ((this->get_attribute() & (attribute_threaded | attribute_multithr))
    != 0) {
    arena_error_throw(this, "invalid attribute for an inline expression");
  }
}

void expr_var::define_vars_one(expr_stmts *stmt)
{
  symtbl_lexical->define_name(sym, ns, this, attr, stmt);
}

static bool check_term_validity(const term& t, bool allow_nontype,
  bool allow_local_func, expr_i *pos, std::string& err_mess_r)
{
  DBG_CTV(fprintf(stderr, "CTV %s\n", term_tostr_human(t).c_str()));
  expr_i *const e = t.get_expr();
  #if 0
  if (e == 0) {
    DBG_CTV(fprintf(stderr, "CTV %s false 1\n", term_tostr_human(t).c_str()));
    return false;
  }
  #endif
  if (!allow_nontype && !is_type(t)) {
    DBG_CTV(fprintf(stderr, "CTV %s false 2\n", term_tostr_human(t).c_str()));
    err_mess_r = "(template parameter '" + term_tostr_human(t) +
      "' is not a type)";
    return false;
  }
  if (!allow_local_func) {
    expr_funcdef *const efd = dynamic_cast<expr_funcdef *>(e);
    if (efd != 0 && !efd->is_global_function()) {
      DBG_CTV(fprintf(stderr, "CTV %s false 2x\n",
	term_tostr_human(t).c_str()));
      err_mess_r = "(template parameter '" + term_tostr_human(t) +
	"' is a local function)";
      return false;
    }
  }
  const bool tp_allow_nontype = true;
  const bool tp_allow_local_func = !is_type(t);
  const term_list *tl = t.get_args();
  const size_t tlarg_len = tl != 0 ? tl->size() : 0;
  expr_block *const ttbl = e ? e->get_template_block() : 0;
  if (ttbl == 0 && tlarg_len != 0) {
    DBG_CTV(fprintf(stderr, "CTV %s false 3\n", term_tostr_human(t).c_str()));
    err_mess_r = "(template expression '" + term_tostr_human(t) +
      "' is incomplete)";
    return false;
  }
  if (ttbl != 0) {
    const size_t tparams_len = elist_length(ttbl->tinfo.tparams);
    #if 0
    const size_t tparams_len = ttbl->tinfo.is_uninstantiated()
      ? elist_length(ttbl->tinfo.tparams) : 0;
    #endif
    if (tparams_len != tlarg_len) {
      // abort(); // FIXME
      DBG_CTV(fprintf(stderr, "CTV %s false 4\n",
	term_tostr_human(t).c_str()));
      err_mess_r = "(wrong number of template parameters: '" +
	term_tostr_human(t) + "')";
      return false;
    }
    expr_struct *const est = dynamic_cast<expr_struct *>(e);
    if (est != 0 && est->category) {
      const std::string cat(est->category);
      if (cat == "farray") {
	if (tlarg_len != 2) {
	  DBG_CTV(fprintf(stderr, "CTV %s false 5\n",
	    term_tostr_human(t).c_str()));
	  err_mess_r = "(template parameter '" + term_tostr_human(t) +
	    "' is not an integer)";
	  return false;
	}
	if (!check_term_validity(tl->front(), tp_allow_nontype,
	  tp_allow_local_func, pos, err_mess_r)) {
	  return false;
	}
	const term& t2nd = *(++tl->begin());
	if (!t2nd.is_long()) {
	  err_mess_r = "(2nd template parameter must be an integer literal)";
	  return false;
	}
	#if 0
	const expr_i *const e2nd = t2nd.get_expr();
	if (e2nd == 0 || e2nd->get_esort() != expr_e_int_literal) {
	  err_mess_r = "(2nd template parameter must be an integer literal)";
	  return false;
	}
	#endif
	DBG_CTV(fprintf(stderr, "CTV %s true 6\n",
	  term_tostr_human(t).c_str()));
	return true;
      }
    }
  }
  if (tl != 0) {
    for (term_list::const_iterator i = tl->begin(); i != tl->end(); ++i) {
      if (!check_term_validity(*i, tp_allow_nontype, tp_allow_local_func, pos,
	err_mess_r)) {
	return false;
      }
    }
  }
  DBG_CTV(fprintf(stderr, "CTV %s true 7\n", term_tostr_human(t).c_str()));
  return true;
}

static void check_var_type(term& typ, expr_i *e, const char *sym,
  bool allow_interface)
{
  std::string err_mess;
  if (!check_term_validity(typ, false, false, e, err_mess)) {
    arena_error_push(e, "invalid type '%s' for variable or field '%s' %s",
      term_tostr_human(typ).c_str(), sym, err_mess.c_str());
  }
  if (is_void_type(typ)) {
    arena_error_push(e, "variable or field '%s' declared void", sym);
  }
  if (!allow_interface && is_interface(typ)) {
    arena_error_push(e, "interface type '%s' for variable or field '%s'",
      term_tostr_human(typ).c_str(), sym);
  }
  if (term_has_tparam(typ)) {
    arena_error_push(e,
      "internal error: variable or field '%s' has an unbound type parameter",
      sym);
  }
  symbol_table *const cur = get_current_frame_symtbl(e->symtbl_lexical);
  if (cur != 0 && is_threaded_context(cur) && !term_is_threaded(typ)) {
    arena_error_push(e,
      "type '%s' for variable or field '%s' is not threaded",
      term_tostr_human(typ).c_str(), sym);
  }
  if (cur != 0 && e->get_esort() == expr_e_var &&
    (cur->block_esort == expr_e_struct || cur->block_esort == expr_e_variant)
    && is_multithr_context(cur) && !term_is_multithr(typ)) {
    arena_error_push(e,
      "type '%s' for field '%s' is not multithreaded",
      term_tostr_human(typ).c_str(), sym);
    /* note: argdecls need not to be multithreaded */
  }
}

static bool is_default_constructible(const term& typ, expr_i *pos, size_t depth)
{
  expr_i *const expr = typ.get_expr();
  const term_list *const args = typ.get_args();
  if (expr == 0) {
    return true;
  }
  if (depth > 1000) {
    arena_error_push(pos, "a type dependency cycle is found: '%s'",
      term_tostr_human(typ).c_str());
    return true;
  }
  ++depth;
  const expr_e esort = expr->get_esort();
  if (esort == expr_e_interface) {
    return false;
  }
  if (esort == expr_e_struct) {
    expr_struct *const est = ptr_down_cast<expr_struct>(expr);
    const std::string cat(est->category ? est->category : "");
    if (is_pointer(typ)) {
      return (args == 0 || args->empty()) ? false
	: is_default_constructible(args->front(), pos, depth);
    }
    if (cat == "linear") {
      return false;
    }
    if (cat == "farray") {
      return (args == 0 || args->empty()) ? false
	: is_default_constructible(args->front(), pos, depth);
    }
    if (cat == "varray" || cat == "map" || cat == "nocascade") {
      return true;
    }
    if (cat != "" || est->cname != 0) {
      /* extern c struct */
      for (term_list::const_iterator i = args->begin(); i != args->end(); ++i) {
	if (!is_default_constructible(*i, pos, depth)) {
	  return false;
	}
      }
      return true;
    }
    if (est->has_userdefined_constr()) {
      expr_argdecls *ads = est->block->argdecls;
      for (; ads != 0; ads = ads->rest) {
	term& ctyp = ads->resolve_texpr();
	if (!is_default_constructible(ctyp, pos, depth)) {
	  return false;
	}
      }
    } else {
      typedef std::list<expr_var *> fields_type;
      fields_type flds;
      est->get_fields(flds);
      for (fields_type::iterator i = flds.begin(); i != flds.end(); ++i) {
	term& ctyp = (*i)->resolve_texpr();
	if (!is_default_constructible(ctyp, pos, depth)) {
	  return false;
	}
      }
    }
    return true;
  }
  if (esort == expr_e_variant) {
    expr_variant *const ev = ptr_down_cast<expr_variant>(expr);
    typedef std::list<expr_var *> fields_type;
    fields_type flds;
    ev->get_fields(flds);
    if (flds.empty()) {
      return true;
    }
    term& ctyp = flds.front()->resolve_texpr();
    return is_default_constructible(ctyp, pos, depth);
  }
  return true;
}

static void check_default_construct(term& typ, expr_var *ev, const char *sym)
{
  expr_struct *est = dynamic_cast<expr_struct *>(get_current_block_expr(ev->symtbl_lexical));
  if (est != 0 && !est->has_userdefined_constr()) {
    /* if ev is a field of a struct wo userdefined constr, it's not
     * necessary to be default-constructible. */
    return;
  }
  if (!is_default_constructible(typ, ev, 0)) {
    arena_error_push(ev, "type '%s' is not default-constructible",
      term_tostr_human(typ).c_str());
  }
}

void expr_var::check_type(symbol_table *lookup)
{
  term& typ = resolve_texpr();
  check_var_type(typ, this, sym, false);
  if (is_global_var(this) && has_userdef_constr(typ)) {
    arena_error_push(this, "user-defined constructor is not allowed for a "
      "global variable '%s'", sym);
  }
  if (parent_expr->get_esort() != expr_e_op ||
    ptr_down_cast<expr_op>(parent_expr)->op != '=') {
    /* requires default constructor */
    check_default_construct(typ, this, sym);
  }
  /* type_of_this_expr */
}

void expr_extval::define_vars_one(expr_stmts *stmt)
{
  symtbl_lexical->define_name(sym, ns, this, attr, stmt);
}

void expr_extval::check_type(symbol_table *lookup)
{
  if ((this->get_attribute() & (attribute_threaded | attribute_multithr))
    != 0) {
    arena_error_throw(this, "invalid attribute for an external value");
  }
  if (is_void_type(resolve_texpr())) {
    arena_error_push(this, "external value '%s' declared void", sym);
  }
  if (is_interface(resolve_texpr())) {
    arena_error_push(this,
      "external value '%s' declared to be of an interface type", sym);
  }
  /* type_of_this_expr */
}

void expr_stmts::check_type(symbol_table *lookup)
{
  fn_check_type(head, lookup);
  fn_check_type(rest, lookup);
  switch (head->get_esort()) {
  case expr_e_int_literal:
  case expr_e_float_literal:
  case expr_e_bool_literal:
  case expr_e_str_literal:
  case expr_e_symbol:
    if (parent_expr != 0 && parent_expr->parent_expr != 0 &&
      parent_expr->parent_expr->get_esort() == expr_e_macrodef) {
      /* ok for macro rhs */
    } else {
      arena_error_push(this, "invalid statement");
    }
    break;
  default:
    break;
  }
}

static void check_return_expr_block(expr_funcdef *fdef, expr_block *block)
{
  expr_stmts *est = block->stmts;
  if (est != 0) {
    while (est->rest != 0) {
      est = est->rest;
    }
  }
  expr_i *last_stmt = 0;
  if (est != 0) {
    last_stmt = est->head;
  }
  expr_i *error_pos = last_stmt != 0 ? last_stmt : block;
  if (last_stmt == 0) {
    /* error */
  } else if (last_stmt->get_esort() == expr_e_if) {
    expr_if *ei = ptr_down_cast<expr_if>(last_stmt);
    while (true) {
      if (ei->cond_static != 0) {
	check_return_expr_block(fdef, ei->block1);
      }
      if (ei->cond_static == 1) {
	return;
      }
      if (ei->block2 != 0) {
	/* has 'else' clause */
	if (ei->cond_static != 1) {
	  check_return_expr_block(fdef, ei->block2);
	}
	return;
      } else if (ei->rest != 0) {
	/* has 'else if' clause */
	ei = ei->rest;
	continue;
      } else {
	/* no 'else if' nor 'else' clause. error. */
	error_pos = ei->block1;
	break;
      }
    }
  } else if (last_stmt->get_esort() == expr_e_block) {
    check_return_expr_block(fdef, ptr_down_cast<expr_block>(last_stmt));
    return;
  } else if (last_stmt->get_esort() == expr_e_special) {
    expr_special *const esp = ptr_down_cast<expr_special>(last_stmt);
    if (esp->tok == TOK_RETURN) {
      return; /* ok */
    }
  }
  // FIXME: try - catch
  arena_error_push(error_pos, "control reaches end of non-void function");
}

static void check_return_expr(expr_funcdef *fdef)
{
  if (is_void_type(fdef->get_rettype())) {
    return; /* returning void */
  }
  if (fdef->no_def) {
    return; /* no definition (extern c) */
  }
  if (fdef->ext_decl && fdef->block->tinfo.tparams == 0) {
    DBG_CT_BLOCK(fprintf(stderr, "extdecl %s\n", fdef->dump(0).c_str()));
    return; /* decl only */
  }
  check_return_expr_block(fdef, fdef->block);
}

void expr_block::check_type(symbol_table *lookup)
{
  DBG_CT_BLOCK(fprintf(stderr, "%s: %p\n", __PRETTY_FUNCTION__, this));
  /* compiling argdecls and rettype is necessary for function type
   * automatching. */
  fn_check_type(argdecls, &symtbl);
  fn_check_type(rettype_uneval, &symtbl);
  /* stmts are compiled only if it's instantiated. */
  if (!tinfo.is_uninstantiated()) {
    DBG_CT_BLOCK(fprintf(stderr, "%s: %p: instantiated\n",
      __PRETTY_FUNCTION__, this));
    fn_check_type(inherit, &symtbl);
    fn_check_type(stmts, &symtbl);
    if (parent_expr != 0 && parent_expr->get_esort() == expr_e_funcdef) {
      expr_funcdef *efd = ptr_down_cast<expr_funcdef>(parent_expr);
      check_return_expr(efd);
    } // FIXME: check_return_expr for struct constructor?
  }
}

static void check_variant_field(const expr_op *eop, expr_i *a0)
{
  expr_i *val = 0;
  expr_i *fld = 0;
  while (true) {
    if (a0->get_esort() != expr_e_op) {
      break;
    }
    expr_op *const aop = ptr_down_cast<expr_op>(a0);
    a0 = aop->arg0;
    if (aop->op == '.') {
      val = aop->arg0;
      fld = aop->arg1;
      break;
    }
    if (aop->op == '(') {
      continue;
    } else {
      break;
    }
  }
  do {
    if (val == 0 || !is_variant(val->resolve_texpr())) {
      break;
    }
    expr_symbol *const fldsym = dynamic_cast<expr_symbol *>(fld);
    if (fldsym == 0) {
      break;
    }
    expr_var *const symdef = dynamic_cast<expr_var *>(
      fldsym->sdef.resolve_symdef());
    if (symdef == 0) {
      break;
    }
    expr_block *const bl = dynamic_cast<expr_block *>(
      find_parent(symdef, expr_e_block));
    if (bl == 0) {
      break;
    }
    expr_variant *const vari = dynamic_cast<expr_variant *>(bl->parent_expr);
    if (vari == 0) {
      break;
    }
    /* ok */
    return;
  } while (0);
  arena_error_push(eop, "union field expected");
  return;
}

void expr_op::check_type(symbol_table *lookup)
{
  fn_check_type(arg0, lookup);
  if (op == '.' || op == TOK_ARROW) {
    term t = arg0->resolve_texpr();
    // expr_i *tedef = te->resolve_symdef();
    symbol_table *symtbl = 0;
    symbol_table *parent_symtbl = 0;
    std::string arg0_ns;
    symbol_common *const sc = arg1->get_sdef();
      /* always a symbol */
    std::string arg1_sym_prefix;
    expr_i *const einst = term_get_instance(t);
    if (einst->get_esort() == expr_e_struct) {
      expr_struct *const es = ptr_down_cast<expr_struct>(einst);
      /* resolve using the context of the struct */
      symtbl = &es->block->symtbl;
      parent_symtbl = symtbl->get_lexical_parent();
      arg0_ns = es->ns;
      if (arg0_ns.empty()) {
	arg0_ns = "pxcrt";
      }
      /* vector_size, map_size etc */
      arg1_sym_prefix = es->sym + std::string("_");
      if (is_pointer(t)) {
	/* if t is a ref{foo}, find 'foo_funcname' from foo's namespace */
	term t1 = get_pointer_target(t);
	expr_i *const einst1 = term_get_instance(t1);
	expr_struct *const es1 = dynamic_cast<expr_struct *>(einst1);
	if (es1 != 0) {
	  arg0_ns = es1->ns;
	  if (arg0_ns.empty()) {
	    arg0_ns = "pxcrt";
	  }
	  arg1_sym_prefix = es1->sym + std::string("_");
	}
	DBG(fprintf(stderr, "ptr target = %s", einst1->dump(0).c_str()));
      }
    } else if (einst->get_esort() == expr_e_variant) {
      expr_variant *const ev = ptr_down_cast<expr_variant>(einst);
      /* resolve using the context of the variant */
      symtbl = &ev->block->symtbl;
      parent_symtbl = symtbl->get_lexical_parent();
      arg0_ns = ev->ns;
    } else if (einst->get_esort() == expr_e_interface) {
      expr_interface *const ei = ptr_down_cast<expr_interface>(einst);
      /* resolve using the context of the interface */
      symtbl = &ei->block->symtbl;
      parent_symtbl = symtbl->get_lexical_parent();
      arg0_ns = ei->ns;
    } else if (einst->get_esort() == expr_e_typedef) {
      expr_typedef *const etd = ptr_down_cast<expr_typedef>(einst);
      if (is_pointer(t)) {
	t = get_pointer_target(t);
	// tedef = te->resolve_symdef();
	arg0_ns = einst->get_ns();
	if (arg0_ns.empty()) {
	  arg0_ns = "pxcrt";
	}
	parent_symtbl = einst->symtbl_lexical;
	DBG(fprintf(stderr, "ptr target = %s", einst->dump(0).c_str()));
      } else {
	symtbl = 0;
	arg0_ns = etd->ns;
	if (arg0_ns.empty()) {
	  arg0_ns = "pxcrt";
	}
	/* size_vector, size_map etc */
	// TODO: UNUSED. remove.
	arg1_sym_prefix = etd->sym + std::string("_");
	parent_symtbl = einst->symtbl_lexical;
      }
    }
    bool is_global_dummy = false;
    bool is_upvalue_dummy = false;
    bool is_memfld_dummy = false;
    DBG(fprintf(stderr,
      "sym=%s rhs_sym_ns=%s symtbl=%p parent_symtbl=%p\n",
      sc->fullsym.c_str(), sc->ns.c_str(), symtbl, parent_symtbl));
    /* lookup member field */
    bool no_private = true;
    if (is_ancestor_symtbl(symtbl, symtbl_lexical)) {
      no_private = false; /* allow private field */
    }
    if (symtbl != 0 && symtbl->resolve_name_nothrow(sc->fullsym, no_private,
      sc->ns, is_global_dummy, is_upvalue_dummy, is_memfld_dummy) != 0) {
      /* symbol is defined as a field */
      DBG(fprintf(stderr, "found fld '%s' ns=%s\n", sc->fullsym.c_str(),
	sc->ns.c_str()));
      fn_check_type(arg1, symtbl);
      type_of_this_expr = arg1->resolve_texpr();
      expr_i *const fo = type_of_this_expr.get_expr();
      expr_funcdef *const fo_efd = dynamic_cast<expr_funcdef *>(fo);
      if (fo_efd != 0 && !fo_efd->is_virtual_or_member_function()) {
	arena_error_throw(this, "can not apply '%s'", tok_string(this, op));
	return;
      }
      assert(!type_of_this_expr.is_null());
      return;
    } else if (parent_symtbl != 0) {
      /* find non-member function (vector_size, map_size etc.) */
      const std::string funcname_w_prefix = arg1_sym_prefix + sc->fullsym;
      symtbl = parent_symtbl;
      const std::string ns = arg0_ns;
      DBG(fprintf(stderr,
	"trying non-member arg0=%s fullsym=%s ns=%s symtbl=%p\n",
	arg0->dump(0).c_str(), sc->fullsym.c_str(), ns.c_str(),
	symtbl));
      no_private = false;
      expr_i *const fo = symtbl->resolve_name_nothrow(funcname_w_prefix,
	no_private, ns.c_str(), is_global_dummy, is_upvalue_dummy,
	is_memfld_dummy);
      if (fo != 0) {
	// expr_funcdef *efd = dynamic_cast<expr_funcdef *>(fo);
	// if (efd != 0 && !efd->is_virtual_or_member_function()) { }
	DBG(fprintf(stderr, "found %s\n", sc->fullsym.c_str()));
	sc->arg_hidden_this = arg0;
	sc->arg_hidden_this_ns = arg0_ns;
	sc->sym_prefix = arg1_sym_prefix;
	fn_check_type(arg1, symtbl); /* expr_symbol::check_type */
	type_of_this_expr = arg1->resolve_texpr();
	assert(!type_of_this_expr.is_null());
	return;
      } else {
	DBG(fprintf(stderr, "func %s notfound [%s] [%s])",
	  funcname_w_prefix.c_str(), ns.c_str(), arg0_ns.c_str()));
	arena_error_throw(this, "can not apply '%s'", tok_string(this, op));
	return;
      }
    } else {
      arena_error_throw(this, "can not apply '%s'", tok_string(this, op));
      return;
    }
  } else {
    fn_check_type(arg1, lookup);
    /* type_of_this_expr */
  }
  switch (op) {
  case ',':
    type_of_this_expr = arg1 ?  arg1->resolve_texpr() : arg0->resolve_texpr();
    break;
  case TOK_OR_ASSIGN:
  case TOK_AND_ASSIGN:
  case TOK_XOR_ASSIGN:
    check_integral_expr(this, arg0);
    /* FALLTHRU */
  case TOK_ADD_ASSIGN:
  case TOK_SUB_ASSIGN:
  case TOK_MUL_ASSIGN:
  case TOK_DIV_ASSIGN:
  case TOK_MOD_ASSIGN:
    check_numeric_expr(this, arg0);
    check_numeric_expr(this, arg1);
    /* FALLTHRU */
  case '=':
    check_type_convert_to_lhs(this, arg1, arg0->resolve_texpr());
    check_lvalue(this, arg0);
    type_of_this_expr = arg0->resolve_texpr();
    break;
  case '?':
    check_bool_expr(this, arg0);
    type_of_this_expr = arg0->resolve_texpr();
    break;
  case ':':
    check_type_convert_to_lhs(this, arg1, arg0->resolve_texpr());
    type_of_this_expr = arg0->resolve_texpr();
    break;
  case TOK_LOR:
  case TOK_LAND:
    check_bool_expr(this, arg0);
    check_bool_expr(this, arg1);
    type_of_this_expr = builtins.type_bool;
    break;
  case '|':
  case '^':
  case '&':
    check_integral_expr(this, arg0);
    check_integral_expr(this, arg1);
    type_of_this_expr = arg0->resolve_texpr();
    break;
  case TOK_EQ:
  case TOK_NE:
    check_type_convert_to_lhs(this, arg1, arg0->resolve_texpr());
    type_of_this_expr = builtins.type_bool;
    break;
  case '<':
  case '>':
  case TOK_LE:
  case TOK_GE:
    check_type_convert_to_lhs(this, arg1, arg0->resolve_texpr());
    type_of_this_expr = builtins.type_bool;
    break;
  case TOK_SHIFTL:
  case TOK_SHIFTR:
    check_integral_expr(this, arg0);
    check_integral_expr(this, arg1);
    type_of_this_expr = arg0->resolve_texpr();
    break;
  case '+':
  case '-':
  case '*':
  case '/':
  case '%':
    check_numeric_expr(this, arg0);
    check_numeric_expr(this, arg1);
    check_type_convert_to_lhs(this, arg1, arg0->resolve_texpr());
    type_of_this_expr = arg0->resolve_texpr();
    break;
  case TOK_PLUS:
  case TOK_MINUS:
    check_numeric_expr(this, arg0);
    type_of_this_expr = arg0->resolve_texpr();
    break;
  case TOK_INC:
  case TOK_DEC:
    check_numeric_expr(this, arg0);
    check_lvalue(this, arg0);
    type_of_this_expr = arg0->resolve_texpr();
    break;
  // TODO: remove
  #if 0
  case TOK_PTR_REF:
    type_of_this_expr = get_pointer_ref_texpr(this, arg0->resolve_texpr());
    break;
  #endif
  case TOK_PTR_DEREF:
    type_of_this_expr = get_pointer_deref_texpr(this, arg0->resolve_texpr());
    break;
  case TOK_CASE:
    check_variant_field(this, arg0);
    type_of_this_expr = builtins.type_bool;
    break;
  case '[':
    {
      term idxt = get_array_index_texpr(this, arg0->resolve_texpr());
      if (arg1 != 0 && arg1->get_esort() == expr_e_op &&
	ptr_down_cast<expr_op>(arg1)->op == TOK_SLICE) {
	/* array slice */
	expr_op *const eopslice = ptr_down_cast<expr_op>(arg1);
	expr_i *const slice_begin = eopslice->arg0;
	expr_i *const slice_end = eopslice->arg1;
	if (is_integral_type(idxt) &&
	  is_integral_type(slice_begin->resolve_texpr())) {
	  /* need not to convert */
	} else {
	  check_type_convert_to_lhs(this, slice_begin, idxt);
	}
	if (is_integral_type(idxt) &&
	  is_integral_type(slice_end->resolve_texpr())) {
	  /* need not to convert */
	} else {
	  check_type_convert_to_lhs(this, slice_end, idxt);
	}
	type_of_this_expr = get_array_slice_texpr(this, arg0->resolve_texpr());
      } else {
	/* array element */
	if (is_integral_type(idxt) &&
	  is_integral_type(arg1->resolve_texpr())) {
	  /* need not to convert */
	} else {
	  check_type_convert_to_lhs(this, arg1, idxt);
	}
	type_of_this_expr = get_array_elem_texpr(this, arg0->resolve_texpr());
      }
    }
    break;
  case TOK_SLICE:
    /* only valid inside [] */
    type_of_this_expr = builtins.type_unit;
    break;
  case '.':
  case TOK_ARROW:
    type_of_this_expr = arg1->resolve_texpr();
    break;
  case '(':
    type_of_this_expr = arg0->resolve_texpr();
    break;
  }
  if (arg0 != 0 && is_void_type(arg0->resolve_texpr())) {
    arena_error_throw(arg0, "invalid expression (lhs is void)");
  }
  if (arg1 != 0 && is_void_type(arg1->resolve_texpr())) {
    arena_error_throw(arg1, "invalid expression (rhs is void)");
  }
  assert(!type_of_this_expr.is_null());
}

static term_list tparams_apply_tvmap(expr_i *e, expr_tparams *tp,
  const tvmap_type& tvm)
{
  term_list tl;
  for (; tp != 0; tp = tp->rest) {
    tvmap_type::const_iterator iter = tvm.find(tp->sym);
    if (iter == tvm.end()) {
      arena_error_push(e, "template parameter '%s' is unbound", tp->sym);
      return term_list();
    }
    tl.push_back(iter->second);
  }
  return tl;
}

static expr_i *create_tpfunc_texpr(expr_i *e, const tvmap_type& tvm,
  expr_i *inst_pos)
{
  expr_block *const bl = e->get_template_block();
  assert(bl);
  term_list telist = tparams_apply_tvmap(e, bl->tinfo.tparams, tvm);
  return instantiate_template(e, telist, inst_pos);
}

void expr_funccall::check_type(symbol_table *lookup)
{
  fn_check_type(func, lookup);
  fn_check_type(arg, lookup);
  if (arg != 0 && is_void_type(arg->resolve_texpr())) {
    arena_error_throw(arg, "expression '%s' is of type void",
      arg->dump(0).c_str());
  }
  term& func_te = func->resolve_texpr();
  symbol_common *const sc = func->get_sdef();
  term *const evaluated_ptr =  sc ? (&sc->resolve_evaluated()) : 0;
  if (is_void_type(func_te) && evaluated_ptr != 0) {
    /* type name */
    func_te = *evaluated_ptr;
  }
  if (term_has_tparam(func_te)) {
    arena_error_throw(func,
      "termplate expression '%s' has an unbound type parameter",
      term_tostr_human(func_te).c_str());
  }
  const term_list *const func_te_args = func_te.get_args();
  expr_i *const func_inst = term_get_instance(func_te);
  if (is_function(func_te)) {
    /* function call */
    expr_funcdef *efd = ptr_down_cast<expr_funcdef>(func_inst);
    expr_argdecls *ad = efd->block->argdecls;
    typedef std::list<expr_i *> arglist_type;
    arglist_type arglist;
    append_hidden_this(func, arglist);
    append_comma_sep_list(arg, arglist);
    unsigned int argcnt = 1;
    tvmap_type tvmap;
    arglist_type::iterator j = arglist.begin();
    while (j != arglist.end() && ad != 0) {
      DBG(fprintf(stderr, "convert_type(%s -> %s)\n",
	term_tostr((*j)->resolve_texpr(), term_tostr_sort_humanreadable)
	  .c_str(),
	term_tostr(ad->resolve_texpr(), term_tostr_sort_humanreadable)
	  .c_str()));
      if (!convert_type(*j, ad->resolve_texpr(), tvmap)) {
	const std::string s0 = term_tostr_human((*j)->resolve_texpr());
	const std::string s1 = term_tostr_human(ad->resolve_texpr());
	arena_error_push(this, "invalid conversion from %s to %s",
	  s0.c_str(), s1.c_str());
	arena_error_push(this, "  initializing argument %u of '%s'",
	  argcnt, efd->sym);
      }
      ++j;
      ad = ad->rest;
      ++argcnt;
    }
    if (j != arglist.end()) {
      arena_error_push(this, "too many argument for '%s'", efd->sym);
    } else if (ad != 0) {
      arena_error_push(this, "too few argument for '%s'", efd->sym);
    }
    if (efd->block->tinfo.has_tparams() &&
      (func_te_args == 0 || func_te_args->empty())) {
      /* instantiate template function automatically */
      DBG(fprintf(stderr, "func is uninstantiated\n"));
      expr_i *const einst = create_tpfunc_texpr(efd, tvmap, this);
      expr_i *real_func_expr = 0;
      if (func->get_esort() == expr_e_op) {
	/* foo.bar(...) */
	expr_op *const func_op = ptr_down_cast<expr_op>(func);
	if (func_op->arg1 != 0) {
	  real_func_expr = func_op->arg1;
	}
      } else {
	/* foo(...) */
	real_func_expr = func;
      }
      if (real_func_expr != 0 && real_func_expr->get_sdef() != 0) {
	std::string err_mess;
	if (!check_term_validity(einst->get_value_texpr(), true, true, this,
	  err_mess)) {
	  arena_error_push(this, "incomplete template expression: '%s' %s",
	    term_tostr_human(einst->get_value_texpr()).c_str(),
	    err_mess.c_str());
	}
	real_func_expr->get_sdef()->set_evaluated(einst->get_value_texpr());
	  /* set auto-matching result */
	expr_funcdef *efd_inst = ptr_down_cast<expr_funcdef>(einst);
	type_of_this_expr = efd_inst->get_rettype();
      } else {
	arena_error_throw(this, "not a template function");
      }
    } else {
      std::string err_mess;
      if (!check_term_validity(efd->get_value_texpr(), true, true, this,
	err_mess)) {
	// abort();
	arena_error_push(this, "incomplete template expression: '%s' %s",
	  term_tostr_human(efd->get_value_texpr()).c_str(), err_mess.c_str());
      }
      type_of_this_expr = apply_tvmap(efd->get_rettype(), tvmap);
    }
    funccall_sort = funccall_e_funccall;
    expr_funcdef *const caller_memfunc = get_up_member_func(symtbl_lexical);
    if (caller_memfunc != 0 && caller_memfunc->is_const) {
      expr_struct *const caller_memfunc_struct =
	caller_memfunc->is_member_function(); /* IMF OK */
      const bool call_wo_obj = func->get_esort() != expr_e_op;
      if (call_wo_obj && efd->is_virtual_or_member_function() &&
	!efd->is_const) {
	arena_error_throw(this,
	  "calling a non-const member function from a const member function");
      }
      if (efd->tpup_thisptr == caller_memfunc_struct &&
	!efd->tpup_thisptr_nonconst) {
	arena_error_throw(this,
	  "calling a non-const member function '%s' from a const "
	  "member function", term_tostr_human(efd->value_texpr).c_str());
      }
    }
    DBG(fprintf(stderr,
      "expr=[%s] type_of_this_expr=[%s] tvmap.size()=%d "
      "arglist.size()=%d funccall\n",
      this->dump(0).c_str(),
      term_tostr(type_of_this_expr, term_tostr_sort_cname).c_str(),
      (int)tvmap.size(), (int)arglist.size()));
    assert(!type_of_this_expr.is_null());
    return;
  }
  const bool is_type_flag = is_type(func_te);
  if (is_type_flag && arg == 0) {
    /* default constructor */
    type_of_this_expr = func_te;
    funccall_sort = funccall_e_default_constructor;
    DBG(fprintf(stderr, "expr=[%s] type_of_this_expr=[%s] defcon\n",
      this->dump(0).c_str(),
      term_tostr(type_of_this_expr, term_tostr_sort_cname).c_str()));
    if (!is_default_constructible(func_te, this, 0)) {
      /* TODO: more user friendly error message */
      arena_error_push(this, "type '%s' is not default-constructible",
	term_tostr_human(func_te).c_str());
    }
    return;
  }
  if (func_inst->get_esort() == expr_e_struct) {
    /* struct constructor */
    expr_struct *est = ptr_down_cast<expr_struct>(func_inst);
    if (est->cname != 0) {
      arena_error_push(this,
	"can't call a constructor for an extern struct '%s'", est->sym);
    }
    tvmap_type tvmap;
    if (est->has_userdefined_constr()) {
      /* user defined constructor */
      expr_argdecls *ad = est->block->argdecls;
      typedef std::list<expr_i *> arglist_type;
      arglist_type arglist;
      append_comma_sep_list(arg, arglist);
      unsigned int argcnt = 1;
      arglist_type::iterator j = arglist.begin();
      while (j != arglist.end() && ad != 0) {
	DBG(fprintf(stderr, "convert_type(%s -> %s)\n",
	  term_tostr((*j)->resolve_texpr(), term_tostr_sort_humanreadable)
	    .c_str(),
	  term_tostr(ad->resolve_texpr(), term_tostr_sort_humanreadable)
	    .c_str()));
	if (!convert_type(*j, ad->resolve_texpr(), tvmap)) {
	  const std::string s0 = term_tostr_human((*j)->resolve_texpr());
	  const std::string s1 = term_tostr_human(ad->resolve_texpr());
	  arena_error_push(this, "invalid conversion from %s to %s",
	    s0.c_str(), s1.c_str());
	  arena_error_push(this, "  initializing argument %u of '%s'",
	    argcnt, est->sym);
	}
	++j;
	ad = ad->rest;
	++argcnt;
      }
      if (j != arglist.end()) {
	arena_error_push(this, "too many argument for '%s'", est->sym);
      } else if (ad != 0) {
	arena_error_push(this, "too few argument for '%s'", est->sym);
      }
    } else {
      /* plain constructor */
      typedef std::list<expr_var *> flds_type;
      flds_type flds;
      est->get_fields(flds);
      typedef std::list<expr_i *> arglist_type;
      arglist_type arglist;
      append_comma_sep_list(arg, arglist);
      unsigned int argcnt = 1;
      flds_type::iterator i = flds.begin();
      arglist_type::iterator j = arglist.begin();
      while (i != flds.end() && j != arglist.end()) {
	if (!convert_type(*j, (*i)->resolve_texpr(), tvmap)) {
	  const std::string s0 = term_tostr_human((*j)->resolve_texpr());
	  const std::string s1 = term_tostr_human((*i)->resolve_texpr());
	  arena_error_push(this, "invalid conversion from %s to %s",
	    s0.c_str(), s1.c_str());
	  arena_error_push(this, "  initializing argument %u of '%s'",
	    argcnt, est->sym);
	}
	++i;
	++j;
	++argcnt;
      }
      if (j != arglist.end()) {
	arena_error_push(this, "too many argument for '%s'", est->sym);
      } else if (i != flds.end() && !arglist.empty()) {
	arena_error_push(this, "too few argument for '%s'", est->sym);
      }
    }
    if (est->block->tinfo.has_tparams() &&
      (func_te_args == 0 || func_te_args->empty())) {
      /* instantiate template struct automatically */
      DBG(fprintf(stderr, "type is uninstantiated\n"));
      expr_i *const einst = create_tpfunc_texpr(est, tvmap, this);
      expr_i *real_func_expr = func;
      if (real_func_expr != 0 && real_func_expr->get_sdef() != 0) {
	real_func_expr->get_sdef()->set_evaluated(einst->get_value_texpr());
	  /* set auto-matching result */
	expr_struct *est_inst = ptr_down_cast<expr_struct>(einst);
	type_of_this_expr = est_inst->get_value_texpr();
      } else {
	arena_error_throw(this, "not a template type");
      }
    } else {
      type_of_this_expr = apply_tvmap(est->value_texpr, tvmap);
    }
    funccall_sort = funccall_e_struct_constructor;
    DBG(fprintf(stderr, "expr=[%s] type_of_this_expr=[%s] strcon\n",
      this->dump(0).c_str(),
      term_tostr(type_of_this_expr, term_tostr_sort_strict).c_str()));
    assert(!type_of_this_expr.is_null());
    return;
  }
  if (is_type_flag) {
    /* explicit conversion */
    term& convto = func_te;
    DBG(fprintf(stderr, "explicit conv to %s te=%s\n", func->dump(0).c_str(),
      term_tostr(convto, term_tostr_sort_strict).c_str()));
    typedef std::list<expr_i *> arglist_type;
    arglist_type arglist;
    append_comma_sep_list(arg, arglist);
    if (arglist.size() > 1) {
      arena_error_throw(this, "too many argument for '%s'",
	term_tostr(convto, term_tostr_sort_strict).c_str());
    } else if (arglist.size() < 1) {
      arena_error_throw(this, "too few argument for '%s'",
	term_tostr(convto, term_tostr_sort_strict).c_str());
    }
    tvmap_type tvmap;
    const arglist_type::iterator j = arglist.begin();
    if (!convert_type(*j, convto, tvmap)) {
      const std::string s0 = term_tostr_human((*j)->resolve_texpr());
      const std::string s1 = term_tostr_human(convto);
      arena_error_push(this, "invalid conversion from %s to %s",
	s0.c_str(), s1.c_str());
      arena_error_push(this, "  initializing argument %u of '%s'",
	1, s1.c_str());
    }
    type_of_this_expr = convto;
    funccall_sort = funccall_e_explicit_conversion;
    DBG(fprintf(stderr, "expr=[%s] type_of_this_expr=[%s] explicit conv\n",
      this->dump(0).c_str(),
      term_tostr(type_of_this_expr, term_tostr_sort_strict).c_str()));
    return;
  }
  arena_error_throw(func, "not a function: %s", func->dump(0).c_str());
}

void expr_special::check_type(symbol_table *lookup)
{
  fn_check_type(arg, lookup);
  if (arg != 0) {
    arg->resolve_texpr();
  }
  if (tok == TOK_RETURN) {
    expr_funcdef *const fdef = dynamic_cast<expr_funcdef *>(
      get_current_frame_expr(lookup));
    if (fdef == 0) {
      arena_error_push(this, "invalid return statement");
      return;
    }
    term rettyp = fdef->get_rettype();
    if (rettyp.is_null()) {
      rettyp = builtins.type_void;
    }
    if (arg == 0) {
      if (!is_void_type(fdef->get_rettype())) {
	arena_error_push(this, "'return' with no value");
      }
      return;
    }
    if (is_void_type(rettyp)) {
      arena_error_push(this, "'return' with a value");
      return;
    }
    tvmap_type tvmap;
    if (!convert_type(arg, rettyp, tvmap)) {
      const std::string s0 = term_tostr(rettyp,
	term_tostr_sort_humanreadable);
      const std::string s1 = term_tostr(arg->resolve_texpr(),
	term_tostr_sort_humanreadable);
      arena_error_push(this, "cannot convert from '%s' to '%s'",
	s1.c_str(), s0.c_str());
    }
  }
}

void expr_if::check_type(symbol_table *lookup)
{
  fn_check_type(cond, lookup);
  check_bool_expr(0, cond);
  /* if cond is a metafunction and it evaluates to false, drop the 'then'
   * clause. */
  symbol_common *const sdef = cond->get_sdef();
  if (sdef != 0) {
    const term& tev = sdef->resolve_evaluated();
    if (tev == term(0LL)) {
      cond_static = 0;
      block1->stmts = 0;
    } else if (tev == term(1LL)) {
      cond_static = 1;
      block2 = 0;
      rest = 0;
    }
  }
  fn_check_type(block1, lookup);
  fn_check_type(block2, lookup);
  fn_check_type(rest, lookup);
}

void expr_while::check_type(symbol_table *lookup)
{
  fn_check_type(cond, lookup);
  check_bool_expr(0, cond);
  fn_check_type(block, lookup);
}

void expr_for::check_type(symbol_table *lookup)
{
  fn_check_type(e0, lookup);
  fn_check_type(e1, lookup);
  fn_check_type(e2, lookup);
  fn_check_type(block, lookup);
}

void expr_feach::check_type(symbol_table *lookup)
{
  fn_check_type(ce, lookup);
  fn_check_type(block, lookup);
  term& tce = ce->resolve_texpr();
  if (!type_allow_feach(tce)) {
    const std::string s0 = term_tostr(ce->resolve_texpr(),
      term_tostr_sort_humanreadable);
    arena_error_push(this, "container type expected (got: %s)", s0.c_str());
    return;
  }
  term tidx = get_array_index_texpr(0, tce);
  term telm = get_array_elem_texpr(0, tce);
  assert(elist_length(block->argdecls) == 2);
  term& ta0 = block->argdecls->resolve_texpr();
  term& ta1 = block->argdecls->rest->resolve_texpr();
  if (!is_same_type(tidx, ta0)) {
    arena_error_push(this, "invalid type for '%s' (got: %s, expected: %s)",
      block->argdecls->sym,
      term_tostr_human(ta0).c_str(),
      term_tostr_human(tidx).c_str());
  }
  if (!is_same_type(telm, ta1)) {
    arena_error_push(this, "invalid type for '%s' (got: %s, expected: %s)",
      block->argdecls->rest->sym,
      term_tostr_human(ta1).c_str(),
      term_tostr_human(telm).c_str());
  }
}

static expr_i *deep_clone_expr(expr_i *e)
{
  if (e == 0) {
    return 0;
  }
  expr_i *const ecpy = e->clone();
  const int num = ecpy->get_num_children();
  for (int i = 0; i < num; ++i) {
    expr_i *const c = ecpy->get_child(i);
    expr_i *const ccpy = deep_clone_expr(c);
    ecpy->set_child(i, ccpy);
  }
  return ecpy;
}

static expr_i *create_symbol_from_string(const std::string& s, expr_i *pos)
{
  /* FIXME: check if s is valid for a symbol */
  expr_i *nsy = expr_nssym_new(pos->fname, pos->line, 0,
    arena_strdup(s.c_str()));
  expr_i *sy = expr_symbol_new(pos->fname, pos->line, nsy);
  return sy;
}

static expr_i *create_te_from_string(const std::string& s, expr_i *pos);

static expr_i *create_telist_from_string(const std::string& s, expr_i *pos)
{
  std::string sym = s;
  size_t i = s.find(','); 
  expr_i *rest = 0;
  if (i != s.npos) {
    const std::string reststr = s.substr(i + 1);
    rest = create_telist_from_string(reststr, pos);
    sym = s.substr(0, i);
  }
  expr_i *head = create_te_from_string(sym, pos);
  expr_i *telist = expr_telist_new(pos->fname, pos->line, head, rest);
  return telist;
}

static expr_i *create_te_from_string(const std::string& s, expr_i *pos)
{
  // fprintf(stderr, "create_te_from_string %s <==\n", s.c_str()); // FIXME
  std::string sym = s;
  expr_i *tl = 0;
  const size_t i = s.find('{');
  if (i != s.npos && s[s.size() - 1] == '}') {
    std::string tlstr = s.substr(i + 1, s.size() - i - 2);
    tl = create_telist_from_string(tlstr, pos);
    sym = s.substr(0, i);
  }
  /* FIXME: check if s is valid for a symbol */
  expr_i *nsy = expr_nssym_new(pos->fname, pos->line, 0,
    arena_strdup(sym.c_str()));
  expr_i *te = expr_te_new(pos->fname, pos->line, nsy, tl);
  // fprintf(stderr, "create_te_from_string %s ==>\n", s.c_str()); // FIXME
  return te;
}

static void subst_symbol_name(expr_i *e, const std::string& src,
  const term& dst, bool to_symbol)
{
  if (e == 0) {
    return;
  }
  // fprintf(stderr, "subst_symbol_name: e=%s\n", e->dump(0).c_str()); // FIXME
  const int num = e->get_num_children();
  for (int i = 0; i < num; ++i) {
    bool subst_node = false;
    expr_i *const c = e->get_child(i);
    if (c == 0) {
      continue;
    }
    #if 0
    // fprintf(stderr, "subst_symbol_name: c=%s\n", c->dump(0).c_str());
    // // FIXME
    #endif
    expr_symbol *sy = dynamic_cast<expr_symbol *>(c);
    expr_te *te = dynamic_cast<expr_te *>(c);
    if (sy != 0) {
      expr_nssym *nsy = sy->nssym;
      assert(nsy != 0);
      if (nsy->prefix == 0 && std::string(nsy->sym) == src) {
	if (dst.is_long()) {
	  expr_i *lit = expr_int_literal_new(nsy->fname, nsy->line,
	    arena_strdup(long_to_string(dst.get_long()).c_str()),
	    dst.get_long() < 0);
	  e->set_child(i, lit);
	} else if (to_symbol) {
	  #if 0
	  expr_i *nsycpy = expr_nssym_new(nsy->fname, nsy->line, 0,
	    arena_strdup(dst.get_string().c_str()));
	  expr_i *sycpy = expr_symbol_new(sy->fname, sy->line, nsycpy);
	  #endif
	  expr_i *sycpy = create_symbol_from_string(dst.get_string(), sy);
	  e->set_child(i, sycpy);
	} else {
	  expr_i *lit = expr_str_literal_new(nsy->fname, nsy->line,
	    arena_strdup(escape_c_str_literal(dst.get_string()).c_str()));
	  DBG_FLDFE(fprintf(stderr, "lit: %s\n", lit->dump(0).c_str()));
	  e->set_child(i, lit);
	}
	subst_node = true;
      }
    } else if (te != 0) { // TODO: fix copipe
      expr_nssym *nsy = te->nssym;
      assert(nsy != 0);
      if (nsy->prefix == 0 && std::string(nsy->sym) == src) {
	if (dst.is_long()) {
	  expr_i *lit = expr_int_literal_new(nsy->fname, nsy->line,
	    arena_strdup(long_to_string(dst.get_long()).c_str()),
	    dst.get_long() < 0);
	  e->set_child(i, lit);
	} else if (to_symbol) {
	  #if 0
	  expr_i *nsycpy = expr_nssym_new(nsy->fname, nsy->line, 0,
	    arena_strdup(dst.get_string().c_str()));
	  expr_i *tecpy = expr_te_new(te->fname, te->line, nsycpy, 0);
	  #endif
	  expr_i *tecpy = create_te_from_string(dst.get_string(), te);
	  e->set_child(i, tecpy);
	} else {
	  expr_i *lit = expr_str_literal_new(nsy->fname, nsy->line,
	    arena_strdup(escape_c_str_literal(dst.get_string()).c_str()));
	  DBG_FLDFE(fprintf(stderr, "lit: %s\n", lit->dump(0).c_str()));
	  e->set_child(i, lit);
	}
	subst_node = true;
      }
    }
    if (!subst_node) {
      subst_symbol_name(c, src, dst, to_symbol);
    }
  }
}


void check_genericfe_syntax(expr_i *e)
{
  if (e == 0) {
    return;
  }
  switch (e->get_esort()) {
  case expr_e_inline_c:
  case expr_e_ns:
  case expr_e_var:
  case expr_e_extval:
  // case expr_e_block:
  case expr_e_feach:
  case expr_e_fldfe:
  case expr_e_foldfe:
  case expr_e_argdecls:
  case expr_e_funcdef:
  case expr_e_typedef:
  case expr_e_macrodef:
  case expr_e_inherit:
  case expr_e_struct:
  case expr_e_variant:
  case expr_e_interface:
  case expr_e_try:
    arena_error_push(e, "not allowed in a generic foreach block: '%s'",
      e->dump(0).c_str());
    break;
  default:
    break;
  }
  #if 0
  expr_var *const ev = dynamic_cast<expr_var *>(e);
  if (ev != 0) {
    arena_error_push(e, "variable definition is not allowed here: '%s'",
      ev->sym);
  }
  #endif
  const int num = e->get_num_children();
  for (int i = 0; i < num; ++i) {
    expr_i *const c = e->get_child(i);
    check_genericfe_syntax(c);
  }
}

#if 0
static bool valid_for_symbol(const std::string& s)
{
  const size_t ln = s.size();
  if (ln == 0) {
    return false;
  }
  for (size_t i = 0; i < ln; ++i) {
    const char c = s[i];
    if (c >= 'a' && c <= 'z') {
    } else if (c >= 'A' && c <= 'Z') {
    } else if (i != 0 && c >= '0' && c <= 'Z') {
    } else if (c == '_') {
    } else {
      return false;
    }
  }
  return true;
}
#endif

void expr_fldfe::check_type(symbol_table *lookup)
{
  check_genericfe_syntax(stmts);
  fn_check_type(te, lookup);
  const term& typ = te->sdef.resolve_evaluated();
  expr_stmts *const storig = ptr_down_cast<expr_stmts>(stmts);
  expr_stmts *const st0 = ptr_down_cast<expr_stmts>(deep_clone_expr(storig));
  strlist syms;
  if (typ.is_metalist()) {
    const term_list& tl = *typ.get_metalist();
    for (term_list::const_iterator i = tl.begin(); i != tl.end(); ++i) {
      const std::string s = meta_term_to_string(*i, true);
      #if 0
      if (!valid_for_symbol(s)) {
	arena_error_push(this, "invalid argument for 'foreach': '%s'",
	  term_tostr_human(typ).c_str());
	arena_error_push(this, "(string '%s' is invalid for a symbol)",
	  s.c_str());
	break;
      }
      #endif
      syms.push_back(s);
    }
  } else if (typ.is_expr()) {
    /* get fields */
    typedef std::list<expr_var *> fields_type;
    fields_type flds;
    const expr_struct *const est = dynamic_cast<const expr_struct *>(
      term_get_instance_const(typ));
    const expr_variant *const ev = dynamic_cast<const expr_variant *>(
      term_get_instance_const(typ));
    if (est != 0) {
      est->get_fields(flds);
    } else if (ev != 0) {
      est->get_fields(flds);
    } else {
      arena_error_push(this, "invalid argument for 'foreach': '%s'",
	term_tostr_human(typ).c_str());
      arena_error_push(this, "(not a struct nor an union)");
    }
    for (fields_type::const_iterator i = flds.begin(); i != flds.end(); ++i) {
      syms.push_back((*i)->sym);
    }
  } else {
    arena_error_push(this, "invalid argument for 'foreach': '%s'",
      term_tostr_human(typ).c_str());
      arena_error_push(this, "(not a type nor a metalist)");
  }
  expr_stmts *cstmts = 0;
  for (strlist::reverse_iterator i = syms.rbegin(); i != syms.rend(); ++i) {
    expr_stmts *const st = ptr_down_cast<expr_stmts>(deep_clone_expr(st0));
    DBG_FLDFE(fprintf(stderr, "replace n=%s fld=%s dst=%s\n",
      (this->namesym ? this->namesym : ""), this->fldsym, i->c_str()));
    const std::string dststr(*i);
    subst_symbol_name(st, this->fldsym, term(dststr), true);
    if (this->namesym != 0) {
      subst_symbol_name(st, this->namesym, term(dststr), false);
    }
    expr_stmts *s = st;
    while (s != 0 && s->rest != 0) {
      s = s->rest;
    }
    if (s != 0) {
      s->rest = cstmts;
      cstmts = st;
    }
  }
  this->stmts = cstmts;
  fn_update_tree(this->stmts, this, lookup, ns);
  DBG_FLDFE(fprintf(stderr, "%s\n", this->stmts->dump(0).c_str()));
  fn_check_type(stmts, lookup);
}

int check_foldop(const std::string& s)
{
  /* only left-to-right ops are supported */
  if (s == ",") {
    return ',';
  }
  if (s == "||") {
    return TOK_LOR;
  }
  if (s == "&&") {
    return TOK_LAND;
  }
  if (s == "|" || s == "^" || s == "&" || s == "+" || s == "*") {
    return s[0];
  }
  return 0;
}

typedef std::deque<expr_i *> exprlist_type;

static void subst_foldfe_one(expr_foldfe *ffe, expr_i *e,
  const exprlist_type& ees_emb, const int fop)
{
  expr_i *seqtop = e;
  while (seqtop->parent_expr != 0 &&
    seqtop->parent_expr->get_esort() == expr_e_op &&
    ptr_down_cast<expr_op>(seqtop->parent_expr)->op == fop) {
    seqtop = seqtop->parent_expr;
  }
  exprlist_type ees;
  expr_i *cur = seqtop;
  while (true) {
    if (cur->get_esort() == expr_e_op &&
      ptr_down_cast<expr_op>(cur)->op == fop) {
      expr_op *const curop = ptr_down_cast<expr_op>(cur);
      expr_i *const arg1 = curop->arg1;
      if (arg1->get_esort() == expr_e_symbol &&
	ptr_down_cast<expr_symbol>(arg1)->sdef.fullsym == ffe->embedsym) {
	for (exprlist_type::const_reverse_iterator i = ees_emb.rbegin();
	  i != ees_emb.rend(); ++i) {
	  ees.push_front(deep_clone_expr(*i));
	}
      } else {
	ees.push_front(curop->arg1);
      }
      cur = curop->arg0;
      continue;
    } else if (cur->get_esort() == expr_e_symbol &&
      ptr_down_cast<expr_symbol>(cur)->sdef.fullsym == ffe->embedsym) {
      for (exprlist_type::const_reverse_iterator i = ees_emb.rbegin();
	i != ees_emb.rend(); ++i) {
	ees.push_front(deep_clone_expr(*i));
      }
    } else {
      ees.push_front(cur);
    }
    break;
  }
  /* tree */
  expr_i *ee = 0;
  for (exprlist_type::const_iterator i = ees.begin(); i != ees.end(); ++i) {
    if (ee != 0) {
      ee = expr_op_new(ffe->fname, ffe->line, fop, ee, *i);
    } else {
      ee = *i;
    }
  }
  if (seqtop->parent_expr == 0) {
    return;
  }
  expr_i *const seqtop_parent = seqtop->parent_expr;
  /* need paren? */
  if (seqtop_parent->get_esort() == expr_e_op) {
    expr_op *const pop = ptr_down_cast<expr_op>(seqtop_parent);
    if (pop->op != '[' && pop->op != '(') {
      ee = expr_op_new(ffe->fname, ffe->line, '(', ee, 0);
    }
  }
  /* replace seqtop with ee */
  for (int i = 0; i < seqtop_parent->get_num_children(); ++i) {
    if (seqtop_parent->get_child(i) == seqtop) {
      seqtop_parent->set_child(i, ee);
      break;
    }
  }
}

static void subst_foldfe(expr_foldfe *ffe, expr_i *e, const exprlist_type& ees,
  const int fop)
{
  if (e == 0) {
    return;
  }
  if (e->get_esort() == expr_e_symbol &&
    ptr_down_cast<expr_symbol>(e)->sdef.fullsym == ffe->embedsym) {
    subst_foldfe_one(ffe, e, ees, fop);
    return;
  }
  int n = e->get_num_children();
  for (int i = 0; i < n; ++i) {
    expr_i *const c = e->get_child(i);
    subst_foldfe(ffe, c, ees, fop);
  }
}

void expr_foldfe::check_type(symbol_table *lookup)
{
  check_genericfe_syntax(stmts);
  fn_check_type(valueste, lookup);
  const term& vtyp = valueste->sdef.resolve_evaluated();
  const term_list *const targs = vtyp.is_metalist() ? vtyp.get_metalist()
    : vtyp.get_args();
  if (targs == 0) {
    arena_error_throw(valueste, "invalid parameter for 'foreach' : '%s'", 
      term_tostr_human(vtyp).c_str());
  }
  exprlist_type ees;
  for (term_list::const_iterator i = targs->begin(); i != targs->end(); ++i) {
    if (i->is_string() && i->get_string().empty()) {
      arena_error_throw(valueste, "invalid parameter for 'foreach' : '%s'", 
	term_tostr_human(vtyp).c_str());
    }
    expr_i *const se = deep_clone_expr(embedexpr);
    // fprintf(stderr, "subst pre=%s\n", se->dump(0).c_str()); // FIXME
    subst_symbol_name(se, itersym, *i, true);
    // fprintf(stderr, "subst post=%s\n", se->dump(0).c_str()); // FIXME
    ees.push_back(se);
  }
  const int fop = check_foldop(foldop);
  if (fop == 0) {
    arena_error_throw(this, "unsupported fold operator for 'foreach' : '%s'", 
      foldop);
  }
  subst_foldfe(this, stmts, ees, fop);
  fn_update_tree(this->stmts, this, lookup, ns);
  fn_check_type(stmts, lookup);
}

void expr_argdecls::define_vars_one(expr_stmts *stmt)
{
  symtbl_lexical->define_name(sym, ns, this, attribute_private, stmt);
}

void expr_argdecls::check_type(symbol_table *lookup)
{
  term& typ = resolve_texpr();
  expr_i *const fr = get_current_frame_expr(lookup);
  if (fr != 0) {
    expr_block *const tbl = fr->get_template_block();
    if (!tbl->tinfo.is_uninstantiated()) {
      check_var_type(typ, this, sym, true);
    }
  }
  fn_check_type(rest, lookup);
  /* type_of_this_expr */
}

void expr_funcdef::check_type(symbol_table *lookup)
{
  if ((this->get_attribute() & attribute_multithr) != 0) {
    arena_error_throw(this,
      "attribute 'multithreaded' is invalid for a function");
  }
  if (this->is_virtual_or_member_function() &&
    (this->get_attribute() & (attribute_threaded | attribute_multithr)) != 0) {
    arena_error_throw(this,
      "invalid attribute for a member/virtual function");
  }
  if (ext_decl && !block->tinfo.template_descent) {
    /* external and not template. stmts are not necessary. */
    block->stmts = 0;
  }
  fn_check_type(block, lookup);
  add_tparam_upvalues_funcdef(this);
}

void expr_typedef::check_type(symbol_table *lookup)
{
}

void expr_macrodef::check_type(symbol_table *lookup)
{
  if ((this->get_attribute() & (attribute_threaded | attribute_multithr))
    != 0) {
    arena_error_throw(this, "invalid attribute for a macro");
  }
  fn_check_type(block, lookup);
}

#if 0
void expr_inherit::check_type(symbol_table *lookup)
{
  this->resolve_symdef();
}
#endif

void expr_struct::check_type(symbol_table *lookup)
{
  fn_check_type(block, lookup);
  check_inherit_threading(this);
  #if 0
  expr_telist *inh = block->inherit;
  for (; inh != 0; inh = inh->rest) {
    symbol_common *sd = inh->head->get_sdef();
    assert(sd);
    term& te = sd->resolve_evaluated();
    if (term_is_threaded(te) &&
      (this->get_attribute() & (attribute_threaded | attribute_multithr))
	== 0) {
      arena_error_throw(this,
	"struct '%s' can't implement '%s' because it's not threaded",
	this->sym, term_tostr_human(te).c_str());
    }
    if (term_is_multithr(te) &&
      (this->get_attribute() & attribute_multithr) == 0) {
      arena_error_throw(this,
	"struct '%s' can't implement '%s' because it's not multithreaded",
	this->sym, term_tostr_human(te).c_str());
    }
  }
  #endif
}

void expr_variant::check_type(symbol_table *lookup)
{
  fn_check_type(block, lookup);
}

void expr_interface::check_type(symbol_table *lookup)
{
  fn_check_type(block, lookup);
}

void expr_try::check_type(symbol_table *lookup)
{
  fn_check_type(tblock, lookup);
  fn_check_type(cblock, lookup);
  fn_check_type(rest, lookup);
  /* type_of_this_expr */
}

static bool is_struct_member(expr_i *edef)
{
  expr_i *const efr = get_current_frame_expr(edef->symtbl_lexical);
  return efr != 0 && efr->get_esort() == expr_e_struct;
}

static void add_tparam_upvalues_tp(expr_funcdef *efd, const term& t)
{
  expr_i *const texpr = t.get_expr();
  if (texpr == 0) {
    return;
  }
  if (texpr->get_esort() == expr_e_funcdef) {
    const expr_i *const einst = term_get_instance_const(t);
    const expr_funcdef *tefd = ptr_down_cast<const expr_funcdef>(einst);
    symbol_table& symtbl = tefd->block->symtbl;
    symbol_table::locals_type::const_iterator j;
    /* upvalues required by this tparam function */
    for (j = symtbl.upvalues.begin(); j != symtbl.upvalues.end(); ++j) {
      expr_i *const e = j->second.edef;
      if (is_struct_member(e)) {
	continue;
      }
      if (efd->tpup_set.find(e) == efd->tpup_set.end()) {
	efd->tpup_set.insert(e);
	efd->tpup_vec.push_back(e);
      }
    }
    /* is this tparam function memberfunc descent? */
    if (tefd->block->symtbl.require_thisup) {
      expr_funcdef *const up_memfunc = get_up_member_func(
	&tefd->block->symtbl);
      if (up_memfunc) {
	/* yes, it's a memberfunc descent */
	expr_struct *const up_est = up_memfunc->is_member_function();
	  /* IMF ?? */
	assert(up_est);
	if (efd->is_member_function() != up_est) { /* IMF ?? */
	  if (efd->tpup_thisptr != 0 && efd->tpup_thisptr != up_est) {
	    arena_error_throw(tefd, "internal error: up_memfunc (indirect)");
	  }
	  efd->tpup_thisptr = up_est;
	  efd->tpup_thisptr_nonconst |= (!up_memfunc->is_const);
	}
      }
    }
  }
  const term_list *const targs = t.get_args();
  if (targs != 0) {
    term_list::const_iterator j;
    for (j = targs->begin(); j != targs->end(); ++j) {
      add_tparam_upvalues_tp(efd, *j);
    }
  }
}

void add_tparam_upvalues_funcdef(expr_funcdef *efd)
{
  /* direct upvalues */
  efd->tpup_set.clear();
  efd->tpup_vec.clear();
  const symbol_table::locals_type& upvalues = efd->block->symtbl.upvalues;
  symbol_table::locals_type::const_iterator i;
  for (i = upvalues.begin(); i != upvalues.end(); ++i) {
    expr_i *const e = i->second.edef;
    if (is_struct_member(e)) {
      continue;
    }
    if (efd->tpup_set.find(e) == efd->tpup_set.end()) {
      efd->tpup_set.insert(e);
      efd->tpup_vec.push_back(e);
    }
  }
  if (efd->block->symtbl.require_thisup) {
    expr_funcdef *const up_memfunc = get_up_member_func(&efd->block->symtbl);
    if (up_memfunc) {
      expr_struct *const up_est = up_memfunc->is_member_function();
	/* IMF ?? */
      assert(up_est);
      if (efd->is_member_function() != up_est) { /* IMF ?? */
	if (efd->tpup_thisptr != 0 && efd->tpup_thisptr != up_est) {
	  arena_error_throw(efd, "internal error: up_memfunc (indirect)");
	}
	efd->tpup_thisptr = up_est;
	efd->tpup_thisptr_nonconst |= (!up_memfunc->is_const);
      }
    }
  }
  /* tparam upvalues */
  term& t = efd->value_texpr;
  const term_list *const targs = t.get_args();
  if (targs == 0 || targs->empty()) {
    return;
  }
  term_list::const_iterator j;
  for (j = targs->begin(); j != targs->end(); ++j) {
    add_tparam_upvalues_tp(efd, *j);
  }
}

void fn_check_closure(expr_i *e)
{
#if 0
  add_tparam_upvalues(e);
#endif
}

static bool is_assign_op(int op)
{
  switch (op) {
  case '=':
  case TOK_ADD_ASSIGN:
  case TOK_SUB_ASSIGN:
  case TOK_MUL_ASSIGN:
  case TOK_DIV_ASSIGN:
  case TOK_MOD_ASSIGN:
  case TOK_OR_ASSIGN:
  case TOK_AND_ASSIGN:
  case TOK_XOR_ASSIGN:
    return true;
  }
  return false;
}

static void rvalue_store_tempvar(expr_i *e, const char *dbgmsg)
{
  if (e->tempvar_id >= 0) {
    return;
  }
  symbol_table *lookup = find_current_symbol_table(e);
  assert(lookup);
  e->tempvar_id = lookup->generate_tempvar();
  e->tempvar_passby = passby_e_const_value; // FIXME: implement
  DBG_TMPVAR(fprintf(stderr, "tempvar for %s (%s)\n", e->dump(0).c_str(),
    dbgmsg));
}

static bool check_need_guard(expr_i *earr, expr_i *eelem, bool lvalue)
{
  const std::string cat = get_category(earr->resolve_texpr());
  if (cat == "farray") {
    return false;
  }
  const call_trait_e ct = get_call_trait(eelem->resolve_texpr());
  if (!lvalue && ct == call_trait_e_value) {
    return false;
  }
  return true;
}

static bool root_nothrow(expr_i *e, bool lvalue_flag, bool checkonly_flag)
{
  expr_op *const eop = dynamic_cast<expr_op *>(e);
  if (eop == 0 || e->tempvar_id >= 0) {
    return true;
  }
  const call_trait_e ect = get_call_trait(e->resolve_texpr());
  if (!lvalue_flag &&
    (ect == call_trait_e_value || ect == call_trait_e_raw_pointer)) {
    if (!checkonly_flag) {
      rvalue_store_tempvar(eop, "value/rawptr rvalue");
	/* ROOT */
    }
    return true;
  }
  switch (eop->op) {
  case ',':
    return root_nothrow(eop->arg1, lvalue_flag, checkonly_flag);
  case '=':
  case TOK_ADD_ASSIGN:
  case TOK_SUB_ASSIGN:
  case TOK_MUL_ASSIGN:
  case TOK_DIV_ASSIGN:
  case TOK_MOD_ASSIGN:
  case TOK_OR_ASSIGN:
  case TOK_AND_ASSIGN:
  case TOK_XOR_ASSIGN:
  case TOK_INC:
  case TOK_DEC:
  case TOK_PTR_REF:
    return root_nothrow(eop->arg0, lvalue_flag, checkonly_flag);
  case '?':
    return root_nothrow(eop->arg1, lvalue_flag, checkonly_flag);
  case ':':
    if (
      root_nothrow(eop->arg0, lvalue_flag, true) &&
      root_nothrow(eop->arg1, lvalue_flag, true)) {
      root_nothrow(eop->arg0, lvalue_flag, checkonly_flag);
      root_nothrow(eop->arg1, lvalue_flag, checkonly_flag);
      return true;
    }
    return false;
  case '.':
  case TOK_ARROW:
    if (is_variant(eop->arg0->resolve_texpr())) {
      /* variant field cant be rooted here. this is the only case
       * root_nothrow cant root an expression */
      return false;
    } else {
      return root_nothrow(eop->arg0, lvalue_flag, checkonly_flag);
    }
  case TOK_PTR_DEREF:
    if (!checkonly_flag) {
      /* copy the pointer and own a refcount */
      rvalue_store_tempvar(eop->arg0, "ptrderef");
	/* ROOT */
    }
    return true;
  case '[':
    if (root_nothrow(eop->arg0, lvalue_flag, true)) {
      root_nothrow(eop->arg0, lvalue_flag, checkonly_flag);
      if (!checkonly_flag) {
	eop->need_guard = check_need_guard(eop->arg0, e, lvalue_flag);
	  /* ROOT */
      }
      return true;
    }
    return false;
  case '(':
    return root_nothrow(eop->arg0, lvalue_flag, checkonly_flag);
  default:
    return true;
  }
}

static void root_lvalue(expr_i *e)
{
  if (!root_nothrow(e, true, false)) {
    arena_error_push(e, "can not root an lvalue");
  }
}

static void root_rvalue(expr_i *e)
{
  /* root_nothrow can generate better code than simple rvalue_store_tempvar,
   * because adding refcount is cheaper than copying. */
  if (root_nothrow(e, false, false)) {
    return;
  }
  call_trait_e ct = get_call_trait(e->resolve_texpr());
  switch (ct) {
  case call_trait_e_raw_pointer:
  case call_trait_e_const_ref_nonconst_ref:
    rvalue_store_tempvar(e, "root_rvalue");
      /* only variant field can reach here */
    break;
  case call_trait_e_value:
    break;
  }
}

static symbol_common *root_funcobj(expr_i *fobj)
{
  if (fobj->get_sdef() != 0) {
    /* 'foo(...)' */
    return fobj->get_sdef();
  }
  if (fobj->get_esort() != expr_e_op) {
    arena_error_push(fobj, "unrecognized function call expression");
    return 0;
  }
  expr_op *const eop = ptr_down_cast<expr_op>(fobj);
  if (eop->op != '.' && eop->op != TOK_ARROW) {
    arena_error_push(fobj, "unrecognized function call expression");
    return 0 ;
  }
  /* 'obj->foo(...)' or 'obj.foo(...)' */
  symbol_common *const right_sc = eop->arg1->get_sdef();
    /* arg1 of these ops must be a symbol or a te */
  expr_funcdef *const fdef = dynamic_cast<expr_funcdef *>(
    right_sc->resolve_symdef());
  if (fdef == 0 || !fdef->is_virtual_or_member_function()) {
    if (fdef != 0 && right_sc->arg_hidden_this == 0) {
      root_rvalue(fobj);
    }
    return right_sc;
  }
  if (fdef->is_const) {
    root_rvalue(eop->arg0); /* obj part of obj.method(...) */
  } else {
    check_lvalue(eop->arg0, eop->arg0);
    root_lvalue(eop->arg0); /* obj part of obj.method(...) */
  }
  return right_sc;
}

static bool expr_would_invalidate_other_expr(expr_i *e)
{
  /* returns true if e contains an assignment op or it calls an impure
   * function */
  if (e == 0) {
    return false;
  }
  if (e->get_esort() == expr_e_funccall) {
    return true;
  } else if (e->get_esort() == expr_e_op) {
    expr_op *const eo = ptr_down_cast<expr_op>(e);
    if (is_assign_op(eo->op)) {
      return true;
    }
  } else if (e->get_esort() == expr_e_block) {
    /* anonymous function etc. */
    return false;
  }
  const int num_children = e->get_num_children();
  for (int i = 0; i < num_children; ++i) {
    if (expr_would_invalidate_other_expr(e->get_child(i))) {
      return true;
    }
  }
  return false;
}

void fn_check_root(expr_i *e)
{
  if (e == 0) {
    return;
  }
  DBG_CHECKROOT(fprintf(stderr, "fn_check_root: check %s\n",
    e->dump(0).c_str()));
  if (e->get_esort() == expr_e_block) {
    expr_block *const bl = ptr_down_cast<expr_block>(e);
    const template_info& ti = bl->tinfo;
    if (ti.tparams != 0) {
      template_info::instances_type::const_iterator i;
      for (i = ti.instances.begin(); i != ti.instances.end(); ++i) {
	fn_check_root(i->second);
      }
      /* e itself must be checked */
      if (ti.is_uninstantiated()) {
	return;
      }
    }
  }
  const int num_children = e->get_num_children();
  for (int i = 0; i < num_children; ++i) {
    fn_check_root(e->get_child(i));
  }
  if (e->get_esort() == expr_e_funccall) {
    DBG_CHECKROOT(fprintf(stderr, "fn_check_root: funccall %s\n",
      le->dump(0).c_str()));
    expr_funccall *const ef = ptr_down_cast<expr_funccall>(e);
    /* root funcobj and '*this' */
    symbol_common *const func_sc = root_funcobj(ef->func);
    if (func_sc == 0) {
      return;
    }
    term& func_t = func_sc->resolve_evaluated();
    expr_funcdef *const efd = dynamic_cast<expr_funcdef *>(
      term_get_instance(func_t));
    DBG_CHECKROOT(fprintf(stderr,
      "fn_check_root: funccall %s symdef=%s efd=%p\n", e->dump(0).c_str(),
      func_sc->resolve_symdef()->dump(0).c_str(), efd));
    /* byref flags */
    std::vector<bool> byref_flags;
    if (efd != 0) {
      for (expr_argdecls *ads = efd->block->argdecls; ads != 0;
	ads = ads->rest) {
	byref_flags.push_back(ads->byref_flag);
      }
    }
    /* root args */
    expr_i *arg = ef->arg;
    typedef std::list<expr_i *> arglist_type;
    arglist_type arglist;
    append_hidden_this(ef->func, arglist);
    append_comma_sep_list(arg, arglist);
    size_t c = 0;
    for (arglist_type::iterator i = arglist.begin(); i != arglist.end(); ++i) {
      if ((*i) == 0) {
	arena_error_throw(e, "internal error in check_root");
      }
      if (c < byref_flags.size() && byref_flags[c]) {
	check_lvalue(*i, *i);
	root_lvalue(*i);
      } else {
	root_rvalue(*i);
      }
      ++c;
    }
  } else if (e->get_esort() == expr_e_op) {
    expr_op *const eo = ptr_down_cast<expr_op>(e);
    if (is_assign_op(eo->op)) {
      /* lhs ?= rhs */
      if (expr_would_invalidate_other_expr(eo->arg0)) {
	/* lhs can invalidate rhs expression */
	root_rvalue(eo->arg1);
      }
      if (expr_would_invalidate_other_expr(eo->arg1)) {
	/* rhs can invalidate lhs expression */
	if (!root_nothrow(eo->arg0, true, true)) {
	  /* lhs is a variant field. make tempvar for rhs, so that rhs is
	   * evaluated before lhs.  */
	  rvalue_store_tempvar(eo->arg1, "variant assign");
	}
      }
    }
  } else if (e->get_esort() == expr_e_feach) {
    expr_feach *const fe = ptr_down_cast<expr_feach>(e);
    if (fe->block->argdecls->rest->byref_flag) {
      check_lvalue(fe, fe->ce);
      root_lvalue(fe->ce); // FIXME: test
    } else {
      root_rvalue(fe->ce); // FIXME: test
    }
  }
}

};

