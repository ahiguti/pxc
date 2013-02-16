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
#define DBG_LV(x)
#define DBG_SLICE(x)
#define DBG_ASGN(x)
#define DBG_STT(x)
#define DBG_CON(x)
#define DBG_ROOT(x)
#define DBG_CT(x)
#define DBG_TPUP(x)
#define DBG_DEFCON(x)

namespace pxc {

static std::string op_message(expr_op *eop)
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
  arena_error_push(eop != 0 ? eop : a0, "bool type expected (got: %s) %s",
    s0.c_str(), op_message(eop).c_str());
}

static void check_unsigned_integral_expr(expr_op *eop, expr_i *a0)
{
  const term t = a0->resolve_texpr();
  if (is_unsigned_integral_type(t)) {
    return;
  }
  const std::string s0 = term_tostr(a0->resolve_texpr(),
    term_tostr_sort_humanreadable);
  arena_error_push(eop != 0 ? eop : a0,
    "unsigned integral type expected (got: %s) %s",
    s0.c_str(), op_message(eop).c_str());
}

static void check_type_common(expr_op *eop, expr_i *a0,
  bool (*func)(const term& t), const char *mess)
{
  const term t = a0->resolve_texpr();
  if (func(t)) {
    return;
  }
  const std::string s0 = term_tostr(a0->resolve_texpr(),
    term_tostr_sort_humanreadable);
  arena_error_push(eop != 0 ? eop : a0, "%s expected (got: %s) %s",
    mess, s0.c_str(), op_message(eop).c_str());
}

static void check_integral_expr(expr_op *eop, expr_i *a0)
{
  return check_type_common(eop, a0, is_integral_type, "integral type");
}

static void check_boolean_algebra(expr_op *eop, expr_i *a0)
{
  return check_type_common(eop, a0, is_boolean_algebra, "boolean algebra");
}

static void check_equality_type(expr_op *eop, expr_i *a0)
{
  return check_type_common(eop, a0, is_equality_type, "equality type");
}

static void check_ordered_type(expr_op *eop, expr_i *a0)
{
  return check_type_common(eop, a0, is_ordered_type, "ordered type");
}

static void check_numeric_expr(expr_op *eop, expr_i *a0)
{
  return check_type_common(eop, a0, is_numeric_type, "numeric type");
}

static void check_lvalue(const expr_i *epos, expr_i *a0)
{
  expr_has_lvalue(epos, a0, true);
}

static void check_copyable(const expr_op *eop, expr_i *a0)
{
  const term t = a0->resolve_texpr();
  if (is_copyable(t)) {
    return;
  }
  const std::string s0 = term_tostr(a0->resolve_texpr(),
    term_tostr_sort_humanreadable);
  arena_error_push(eop != 0 ? eop : a0, "type '%s' is not copyable",
    s0.c_str());
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
  if ((this->get_attribute() & attribute_threaded) != 0) {
    arena_error_throw(this, "invalid attribute for an inline expression");
  }
}

void expr_var::define_vars_one(expr_stmts *stmt)
{
  symtbl_lexical->define_name(sym, injectns, this, attr, stmt);
}

static bool check_term_validity(const term& t, bool allow_nontype,
  bool allow_local_func, bool allow_ephemeral, expr_i *pos, std::string& err_mess_r)
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
  if (!allow_ephemeral && is_ephemeral_value_type(t)) {
    err_mess_r = "(template parameter '" + term_tostr_human(t) +
      "' is an ephemeral type)";
    return false;
  }
  const bool tp_allow_nontype = true;
  const bool tp_allow_local_func = !is_type(t);
  const bool tp_allow_ephemeral = false;
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
    if (est != 0 && est->typefamily != typefamily_e_none) {
      const typefamily_e cat = est->typefamily;
      if (cat == typefamily_e_farray) {
	#if 0
	if (tlarg_len != 2) {
	  DBG_CTV(fprintf(stderr, "CTV %s false 5\n",
	    term_tostr_human(t).c_str()));
	  err_mess_r = "(template parameter '" + term_tostr_human(t) +
	    "' is not an integer)";
	  return false;
	}
	#endif
	if (tlarg_len >= 1) {
	  if (!check_term_validity(tl->front(), tp_allow_nontype,
	    tp_allow_local_func, tp_allow_ephemeral, pos, err_mess_r)) {
	    return false;
	  }
	}
	if (tlarg_len >= 2) {
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
	}
	DBG_CTV(fprintf(stderr, "CTV %s true 6\n",
	  term_tostr_human(t).c_str()));
	return true;
      }
    }
  }
  if (tl != 0) {
    for (term_list::const_iterator i = tl->begin(); i != tl->end(); ++i) {
      if (!check_term_validity(*i, tp_allow_nontype, tp_allow_local_func,
	tp_allow_ephemeral, pos, err_mess_r)) {
	return false;
      }
    }
  }
  DBG_CTV(fprintf(stderr, "CTV %s true 7\n", term_tostr_human(t).c_str()));
  return true;
}

static void check_var_type(term& typ, expr_i *e, const char *sym,
  bool byref_flag, bool need_copyable)
{
  std::string err_mess;
  if (!check_term_validity(typ, false, false, true, e, err_mess)) {
    arena_error_push(e, "invalid type '%s' for variable or field '%s' %s",
      term_tostr_human(typ).c_str(), sym, err_mess.c_str());
  }
  if (is_void_type(typ)) {
    arena_error_push(e, "variable or field '%s' declared void", sym);
  }
  if (!byref_flag && need_copyable && !is_copyable(typ)) {
    arena_error_push(e, "noncopyable type '%s' for variable or field '%s'",
      term_tostr_human(typ).c_str(), sym);
  }
  if (term_has_unevaluated_expr(typ)) {
    arena_error_push(e, "variable or field '%s' has an invalid type '%s'",
      sym, term_tostr_human(typ).c_str());
  }
  if (term_has_tparam(typ)) {
    arena_error_push(e,
      "internal error: variable or field '%s' has an unbound type parameter",
      sym);
  }
  symbol_table *const cur = get_current_frame_symtbl(e->symtbl_lexical);
// FIXME: not effective?
#if 0
//  if (cur != 0 && is_threaded_context(cur) && !term_is_threaded(typ)) {
//    arena_error_push(e,
//      "type '%s' for variable or field '%s' is not threaded",
//      term_tostr_human(typ).c_str(), sym);
//  }
#endif
  if (cur != 0 && e->get_esort() == expr_e_var &&
    (cur->block_esort == expr_e_struct || cur->block_esort == expr_e_variant)) {
    expr_var *const ev = ptr_down_cast<expr_var>(e);
    if (is_ephemeral_value_type(typ)) {
      arena_error_push(e,
	"type '%s' for field '%s' is an ephemeral type",
	term_tostr_human(typ).c_str(), sym);
    }
    if (is_passby_cm_reference(ev->varinfo.passby)) {
      arena_error_push(e, "field '%s' can't be a reference", sym);
    }
// FIXME: not effective?
#if 0
//    if (is_multithr_context(cur) && !term_is_multithr(typ)) {
//      arena_error_push(e,
//	"type '%s' for field '%s' is not multithreaded",
//	term_tostr_human(typ).c_str(), sym);
//      /* note: argdecls need not to be multithreaded */
//    }
#endif
  }
}

static bool is_default_constructible(const term& typ, expr_i *pos,
  size_t depth)
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
    const typefamily_e cat = est->typefamily;
    if (is_cm_pointer_family(typ)) {
      return false;
    }
    #if 0
    if (is_cm_lockobject_family(typ)) {
      return false;
    }
    #endif
    if (cat == typefamily_e_linear) {
      return false;
    }
    if (cat == typefamily_e_farray) {
      return (args == 0 || args->empty()) ? false
	: is_default_constructible(args->front(), pos, depth);
    }
    if (cat == typefamily_e_darray) {
      return false;
    }
    if (cat == typefamily_e_varray || cat == typefamily_e_tree_map ||
      cat == typefamily_e_nocascade) {
      return true;
    }
    if (cat != typefamily_e_none || est->cname != 0) {
      /* extern c struct */
      for (term_list::const_iterator i = args->begin(); i != args->end(); ++i) {
	if (!is_default_constructible(*i, pos, depth)) {
	  return false;
	}
      }
      return true;
    }
    if (est->has_userdefined_constr()) {
      return false;
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
  if (esort == expr_e_interface) {
    return false;
  }
  return true;
}

static void check_default_construct(term& typ, expr_var *ev, const char *sym)
{
  expr_i *const cbexpr = get_current_block_expr(ev->symtbl_lexical);
#if 0
// FIXME: remove.
  expr_struct *est = dynamic_cast<expr_struct *>(cbexpr);
  if (est != 0 && !est->has_userdefined_constr()) {
    /* if ev is a field of a struct wo userdefined constr, it's not
     * necessary to be default-constructible. */
    return;
  }
#endif
  expr_variant *eva = dynamic_cast<expr_variant *>(cbexpr);
  if (eva != 0 && eva->get_first_field() != ev) {
    /* only 1st field of variant need to be default-constructible */
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
  bool need_defcon = true;
  bool need_copyable = false;
  expr_op *const parent_eop = dynamic_cast<expr_op *>(parent_expr);
  if (parent_eop != 0 && parent_eop->op == '=') {
    need_defcon = false;
    if (!is_vardef_constructor(parent_eop)) {
      need_copyable = true;
    }
  }
  check_var_type(typ, this, sym, is_passby_cm_reference(varinfo.passby),
    need_copyable);
  if (is_global_var(this) && is_ephemeral_value_type(typ)) {
    arena_error_push(this,
      "type '%s' for global variable '%s' is an ephemeral type",
      term_tostr_human(typ).c_str(), sym);
  }
  const attribute_e tattr = get_term_threading_attribute(typ);
  if (is_global_var(this) && (tattr & attribute_threaded) == 0) {
    arena_error_push(this,
      "type '%s' for global variable '%s' is not threaded",
      term_tostr_human(typ).c_str(), sym);
  }
  if (need_defcon) {
    /* requires default constructor */
    check_default_construct(typ, this, sym);
    if (is_passby_cm_reference(varinfo.passby)) {
      arena_error_push(this, "reference variable '%s' is not initialized",
	sym);
    }
    if (is_ephemeral_value_type(typ)) {
      arena_error_push(this, "ephemeral type variable '%s' is not initialized",
	sym);
    }
  }
  /* type_of_this_expr */
}

void expr_enumval::define_vars_one(expr_stmts *stmt)
{
  symtbl_lexical->define_name(sym, injectns, this, attr, stmt);
}

void expr_enumval::check_type(symbol_table *lookup)
{
  if ((this->get_attribute() & attribute_threaded) != 0) {
    arena_error_throw(this, "invalid attribute for an external value");
  }
  if (is_void_type(resolve_texpr())) {
    arena_error_push(this, "external value '%s' declared void", sym);
  }
  if (is_interface(resolve_texpr())) {
    arena_error_push(this,
      "external value '%s' declared to be of an interface type", sym);
  }
  fn_check_type(value, lookup);
  fn_check_type(rest, lookup);
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
  const term& typ = fdef->get_rettype();
  std::string err_mess;
  const bool allow_ephemeral = fdef->no_def && fdef->is_member_function();
    /* allows ephemeral type if fdef is a extern c function */
  if (!check_term_validity(typ, false, false, allow_ephemeral, fdef, err_mess)) {
    arena_error_push(fdef, "invalid return type '%s' %s",
      term_tostr_human(typ).c_str(), err_mess.c_str());
  }
  if (is_void_type(typ)) {
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

static passby_e merge_passby(passby_e p1, passby_e p2)
{
  const bool byval = is_passby_cm_value(p1) || is_passby_cm_value(p2);
    /* byval requirement is stronger than byref */
  const bool mut = is_passby_mutable(p1) || is_passby_mutable(p2);
  return byval
    ? (mut ? passby_e_mutable_value : passby_e_const_value)
    : (mut ? passby_e_mutable_reference : passby_e_const_reference);
}

static void store_tempvar(expr_i *e, passby_e passby, bool blockscope_flag,
  bool guard_elements, const char *dbgmsg)
{
  DBG_TMPVAR(fprintf(stderr, "tempvar for %s pass=%d (%s)\n",
    e->dump(0).c_str(), (int)passby, dbgmsg));
  DBG_STT(fprintf(stderr, "store_tempvar passby=%d blocksc=%d g=%d %s\n",
    (int)passby, (int)blockscope_flag, (int)guard_elements,
    e->dump(0).c_str()));
  if (e->tempvar_id < 0) {
    symbol_table *lookup = find_current_symbol_table(e);
    assert(lookup);
    e->tempvar_id = lookup->generate_tempvar();
  } else {
    if (is_passby_cm_value(e->tempvar_varinfo.passby) !=
      is_passby_cm_value(passby)) {
      /* in this case, it will be rooted by value. test_25_slice/val.pl . */
    }
  }
  // if (guard_elements) { abort(); } // FIXME
  e->tempvar_varinfo.passby = merge_passby(e->tempvar_varinfo.passby, passby);
  e->tempvar_varinfo.guard_elements |= guard_elements;
  e->tempvar_varinfo.scope_block |= blockscope_flag;
  #if 0
  if (is_passby_cm_value(e->tempvar_varinfo.passby) &&
    !is_copyable(e->resolve_texpr())) {
    /* reaches here if expr is a lockobject constructor */
    arena_error_throw(e,
      "internal error: failed to root because it's not copyable");
  }
  #endif
}

static void add_root_requirement_container_elements(expr_i *econ,
  bool blockscope_flag)
{
  /* this function makes econ elements valid */
  DBG_CON(fprintf(stderr, "%s op[] container blockscope=%d\n",
    econ->dump(0).c_str(), (int)blockscope_flag));
  const bool con_lv = expr_has_lvalue(econ, econ, false);
  const bool guard_elements =
    type_has_invalidate_guard(econ->resolve_texpr());
  DBG_CON(fprintf(stderr, "op[] container=%s lvalue=%d\n",
    econ->dump(0).c_str(), (int)con_lv));
  if (!guard_elements) {
    /* nothing to do */
  } else {
    store_tempvar(econ,
      con_lv ? passby_e_mutable_reference : passby_e_const_reference,
      blockscope_flag, guard_elements, "arrelem");
  }
}

#if 0
static void check_ephemeral_object(expr_i *e, passby_e passby,
  bool blockscope_flag)
{
  if (passby != passby_e_const_reference) {
    return;
  }
  if (!blockscope_flag) {
    return;
  }
  store_tempvar(e, passby_e_const_value, blockscope_flag, false, "ephemeral");
}
#endif

static void add_root_requirement(expr_i *e, passby_e passby,
  bool blockscope_flag)
{
  if (e == 0) {
    return;
  }
  #if 0
  fprintf(stderr, "add_root_req %s passby=%d bl=%d\n", e->dump(0).c_str(),
    (int)passby, (int)blockscope_flag); 
  // if (e->dump(0) == "[lockobject(([int]))](sobj)") { abort(); }
  #endif
  /* this function makes e valid while the current statement (or block if
   * blockscope_flag is true) is terminated. if e is an ephemeral value (eg.,
   * range types), the object e refer to is also rooted. */
  if (e->conv == conversion_e_container_range) {
    /* convert from container to range */
    add_root_requirement_container_elements(e, blockscope_flag);
    /* e is a container. rooted c/m reference of the container is required. */
    passby = is_const_range_family(e->type_conv_to)
      ? passby_e_const_reference : passby_e_mutable_reference;
    /* thru */
  } else if (e->conv != conversion_e_none &&
    e->conv != conversion_e_subtype_obj) {
    if (passby == passby_e_mutable_reference) {
      /* cant root because of conversion-by-value */
      DBG_ROOT(fprintf(stderr, "add_root_requirement: byref conv\n"));
      /* possible to reach here? */
      arena_error_throw(e,
	"can not root by mutable reference because of conversion");
    }
    #if 0
    /* this expr has a rooted rvalue because of conversion. */
    if (blockscope_flag) {
      store_tempvar(e,
	is_passby_const(passby)
	? passby_e_const_value : passby_e_mutable_value, blockscope_flag,
	false, "conv"); /* ROOT */
    }
    return;
    #endif
    /* thru */
  }
  if (e->get_esort() == expr_e_funccall &&
    is_ephemeral_value_type(e->resolve_texpr())) {
    /* function returning ephemeral type. only allowed for extern c function. */
    expr_funccall *const efc = ptr_down_cast<expr_funccall>(e);
    expr_op *const eop = dynamic_cast<expr_op *>(efc->func);
//    if (efc->funccall_sort != funccall_e_struct_constructor) {
      if (eop == 0 || (eop->op != '.' && eop->op != TOK_ARROW)) {
	arena_error_throw(eop,
	  "internal error: non-member function returning ephemeral type");
      }
      /* 'this' object is a container. root the object and it's elements */
      add_root_requirement_container_elements(eop->arg0, blockscope_flag);
      add_root_requirement(eop->arg0,
	is_passby_const(passby)
	? passby_e_const_reference : passby_e_mutable_reference,
	blockscope_flag);
      return;
//    }
  }
  if (e->get_esort() == expr_e_op) {
    expr_op *const eop = ptr_down_cast<expr_op>(e);
    if (eop->op == '=' && eop->arg0->get_esort() == expr_e_var) {
      /* foo x = ... */
      return; /* always rooted */
    }
  }
  switch (e->get_esort()) {
  case expr_e_int_literal:
  case expr_e_float_literal:
  case expr_e_bool_literal:
  case expr_e_str_literal:
  case expr_e_symbol:
    return; /* always rooted */
  default:
    break;
  }
  expr_op *const eop = dynamic_cast<expr_op *>(e);
  if (eop == 0) {
    /* not a operator. this expr returns a value instead of reference. */
    if (blockscope_flag) {
      store_tempvar(e,
	is_passby_const(passby)
	? passby_e_const_value : passby_e_mutable_value, blockscope_flag,
	false, "nonop"); /* ROOT */
    } else {
    }
    return;
  }
  /* operator */
  switch (eop->op) {
  case ',':
    add_root_requirement(eop->arg0, passby, blockscope_flag);
    add_root_requirement(eop->arg1, passby, blockscope_flag);
    return;
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
    add_root_requirement(eop->arg0, passby_e_mutable_reference,
      blockscope_flag);
    add_root_requirement(eop->arg1, passby_e_const_value, blockscope_flag);
    return;
  #if 0
  case '?':
    if (blockscope_flag) {
      arena_error_throw(e, "can not root a conditional expression");
    }
    return add_root_requirement(eop->arg1, passby, blockscope_flag);
  case ':':
    add_root_requirement(eop->arg0, passby, blockscope_flag);
    add_root_requirement(eop->arg1, passby, blockscope_flag);
    return;
  #endif
  case '.':
  case TOK_ARROW:
    if (is_variant(eop->arg0->resolve_texpr())) {
      /* variant field */
      if (passby == passby_e_mutable_reference) {
	arena_error_throw(e, "can not root a variant field");
	DBG_ROOT(fprintf(stderr, "add_root_requirement: variant field\n"));
      } else {
	store_tempvar(eop, passby_e_const_value, blockscope_flag, false,
	  "vfld"); /* ROOT */
      }
    } else {
      /* struct field. root the struct. */
      add_root_requirement(eop->arg0, passby, blockscope_flag);
    }
    return;
  case TOK_PTR_DEREF:
    if (is_cm_range_family(eop->arg0->resolve_texpr())) {
      /* range dereference. root the range object. */
      add_root_requirement(eop->arg0, passby_e_const_value, blockscope_flag);
    } else {
      /* pointers */
      #if 0
      if (blockscope_flag &&
	is_multithreaded_pointer_family(eop->arg0->resolve_texpr())) {
	arena_error_throw(e, "block-scope lock is not implemented");
      }
      #endif
      /* copy the pointer in order to own a refcount */
      const bool guard_elements = type_has_invalidate_guard(
	eop->arg0->resolve_texpr());
      #if 0
      fprintf(stderr, "%s: guard=%d\n",
	term_tostr_human(eop->arg0->resolve_texpr()).c_str(),
	(int)guard_elements);
      #endif
      store_tempvar(eop->arg0, passby_e_const_value, blockscope_flag,
	guard_elements, "ptrderef"); /* ROOT */
    }
    return;
  case '[':
    /* root the container or range object */
    {
      passby_e container_passby = passby_e_const_value;
      if (is_cm_range_family(eop->arg0->resolve_texpr())) {
	container_passby = passby_e_const_value;
      } else {
	container_passby = is_passby_const(passby)
	  ? passby_e_const_reference : passby_e_mutable_reference;
      }
      add_root_requirement(eop->arg0, container_passby, blockscope_flag);
      #if 0
	(is_passby_const(passby) ||
	  is_cm_range_family(eop->arg0->resolve_texpr()))
	? passby_e_const_reference : passby_e_mutable_reference,
	blockscope_flag);
      #endif
    }
    if (is_passby_cm_reference(passby) ||
      (eop->arg1 != 0 && is_range_op(eop->arg1))) {
      /* v[k] is required byref, or v[..] is required. need to guard. */
      add_root_requirement_container_elements(eop->arg0, blockscope_flag);
    } else {
      /* by value */
//      if (blockscope_flag) {
	/* named variable. need to guard. */
	store_tempvar(eop, passby, blockscope_flag, false, "elemval");
//      }
    }
    return;
  case '(':
    add_root_requirement(eop->arg0, passby, blockscope_flag);
    return;
  default:
    break;
  }
  /* other operations return value */
  if (passby == passby_e_mutable_reference) {
    arena_error_throw(e, "can not root by mutable reference");
  }
  if (blockscope_flag) {
    store_tempvar(eop,
      is_passby_const(passby)
      ? passby_e_const_value : passby_e_mutable_value,
      blockscope_flag, false, "ptrderef"); /* ROOT */
  }
  if (eop->arg0 != 0) {
    add_root_requirement(eop->arg0,
      is_passby_const(passby)
      ? passby_e_const_value : passby_e_mutable_value,
      blockscope_flag);
  }
  if (eop->arg1 != 0) {
    add_root_requirement(eop->arg0,
      is_passby_const(passby)
      ? passby_e_const_value : passby_e_mutable_value,
      blockscope_flag);
  }
}

#if 0
static void check_expr_root(expr_i *e, passby_e passby, bool blockscope_flag)
{
  add_root_requirement(e, passby, blockscope_flag);
}
#endif

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

static bool is_assign_incdec_op(int op)
{
  return (is_assign_op(op) || op == TOK_INC || op == TOK_DEC);
}

// FIXME: use this to optimize
#if 0
static bool expr_is_subexpression(expr_i *e)
{
  if (e == 0) {
    return false;
  }
  expr_i *const ep = e->parent_expr;
  if (ep == 0) {
    return false;
  }
  return ep->is_expression();
}

static bool expr_can_have_side_effect(expr_i *e)
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
    if (is_assign_incdec_op(eo->op)) {
      return true;
    }
  } else if (e->get_esort() == expr_e_block) {
    /* anonymous function etc. */
    return false;
  }
  const int num_children = e->get_num_children();
  for (int i = 0; i < num_children; ++i) {
    if (expr_can_have_side_effect(e->get_child(i))) {
      return true;
    }
  }
  return false;
}
#endif

static expr_i *get_variant_object_if_vfld(expr_i *e)
{
  expr_op *const eop = dynamic_cast<expr_op *>(e);
  if (eop != 0 && (eop->op == '.' || eop->op == TOK_ARROW) &&
    is_variant(eop->arg0->resolve_texpr())) {
    return eop->arg0;
  }
  return 0;
}

static void passing_root_requirement(passby_e passby, expr_i *epos,
  expr_i *e, bool blockscope_flag)
{
  if (is_passby_cm_reference(passby) && is_passby_mutable(passby)) {
    check_lvalue(epos, e);
  }
  add_root_requirement(e, passby, blockscope_flag);
}

static bool is_copying_preferred(const term& t)
{
  if (!is_type(t)) {
    return false;
  }
  if (!is_copyable(t)) {
    return false;
  }
  if (is_ctypedef(t) || is_enum(t) || is_bitmask(t) || is_numeric_type(t)) {
    return true;
  }
  return false;
}

static void passing_root_requirement_fast(expr_i *epos, expr_i *e,
  bool blockscope_flag)
{
  /* passby const_value or const_reference */
  term t = e->resolve_texpr();
  passby_e const passby = is_copying_preferred(t)
    ? passby_e_const_value : passby_e_const_reference;
  add_root_requirement(e, passby, blockscope_flag);
}

void expr_op::check_type(symbol_table *lookup)
{
  if (op == '.' || op == TOK_ARROW) {
    fn_check_type(arg0, lookup);
    term t = arg0->resolve_texpr();
    symbol_table *symtbl = 0;
    symbol_table *parent_symtbl = 0;
    std::string arg0_uniqns;
    symbol_common *const sc = arg1->get_sdef();
      /* always a symbol */
    std::string arg1_sym_prefix;
    expr_i *const einst = term_get_instance(t);
    if (einst->get_esort() == expr_e_struct) {
      expr_struct *const es = ptr_down_cast<expr_struct>(einst);
      /* resolve using the context of the struct */
      symtbl = &es->block->symtbl;
      parent_symtbl = symtbl->get_lexical_parent();
      arg0_uniqns = es->uniqns;
      /* vector_size, map_size etc */
      arg1_sym_prefix = es->sym + std::string("_");
      if (is_cm_pointer_family(t)) {
	/* if t is a ptr{foo}, find 'foo_funcname' from foo's namespace */
	term t1 = get_pointer_target(t);
	expr_i *const einst1 = term_get_instance(t1);
	expr_struct *const es1 = dynamic_cast<expr_struct *>(einst1);
	if (es1 != 0) {
	  arg0_uniqns = es1->uniqns;
	  arg1_sym_prefix = es1->sym + std::string("_");
	}
	DBG(fprintf(stderr, "ptr target = %s", einst1->dump(0).c_str()));
      }
    } else if (einst->get_esort() == expr_e_variant) {
      expr_variant *const ev = ptr_down_cast<expr_variant>(einst);
      /* resolve using the context of the variant */
      symtbl = &ev->block->symtbl;
      parent_symtbl = symtbl->get_lexical_parent();
      arg0_uniqns = ev->uniqns;
    } else if (einst->get_esort() == expr_e_interface) {
      expr_interface *const ei = ptr_down_cast<expr_interface>(einst);
      /* resolve using the context of the interface */
      symtbl = &ei->block->symtbl;
      parent_symtbl = symtbl->get_lexical_parent();
      arg0_uniqns = ei->uniqns;
    } else if (einst->get_esort() == expr_e_typedef) {
      expr_typedef *const etd = ptr_down_cast<expr_typedef>(einst);
      if (is_cm_pointer_family(t)) {
	t = get_pointer_target(t);
	arg0_uniqns = einst->get_unique_namespace();
	parent_symtbl = einst->symtbl_lexical;
	DBG(fprintf(stderr, "ptr target = %s", einst->dump(0).c_str()));
      } else {
	symtbl = 0;
	arg0_uniqns = etd->uniqns;
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
    bool no_private = true;
    if (is_ancestor_symtbl(symtbl, symtbl_lexical)) {
      no_private = false; /* allow private field */
    }
    /* lookup without arg1_sym_prefix */
    /* lookup member field */
    if (symtbl != 0 && symtbl->resolve_name_nothrow(sc->fullsym, no_private,
      sc->uniqns, is_global_dummy, is_upvalue_dummy, is_memfld_dummy) != 0) {
      /* symbol is defined as a field */
      DBG(fprintf(stderr, "found fld '%s' ns=%s\n", sc->fullsym.c_str(),
	sc->ns.c_str()));
      fn_check_type(arg1, symtbl);
      type_of_this_expr = arg1->resolve_texpr();
      // expr_i *const fo = type_of_this_expr.get_expr();
      const expr_i *const fo = sc->get_symdef();
      const expr_funcdef *const fo_efd =
	dynamic_cast<const expr_funcdef *>(fo);
      if (fo_efd != 0 && !fo_efd->is_virtual_or_member_function()) {
	arena_error_throw(this, "can not apply '%s'", tok_string(this, op));
	return;
      }
      assert(!type_of_this_expr.is_null());
      return;
    } else if (parent_symtbl != 0) {
      /* lookup with arg1_sym_prefix */
      /* find non-member function (vector_size, map_size etc.) */
      const std::string funcname_w_prefix = arg1_sym_prefix + sc->fullsym;
      symtbl = parent_symtbl;
      const std::string uniqns = arg0_uniqns;
      DBG(fprintf(stderr,
	"trying non-member arg0=%s fullsym=%s ns=%s symtbl=%p\n",
	arg0->dump(0).c_str(), sc->fullsym.c_str(), uniqns.c_str(),
	symtbl));
      no_private = false;
      expr_i *const fo = symtbl->resolve_name_nothrow(funcname_w_prefix,
	no_private, uniqns.c_str(), is_global_dummy, is_upvalue_dummy,
	is_memfld_dummy);
      if (fo != 0) {
	DBG(fprintf(stderr, "found %s\n", sc->fullsym.c_str()));
	sc->arg_hidden_this = arg0;
	sc->arg_hidden_this_ns = arg0_uniqns;
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
    /* other ops */
    fn_check_type(arg1, lookup); /* arg1 first. must for type inference */
    fn_check_type(arg0, lookup);
    /* type_of_this_expr */
  }
  bool unknown_eval_order = true; // FIXME: remove. unused.
  switch (op) {
  case ',':
    type_of_this_expr = arg1 ?  arg1->resolve_texpr() : arg0->resolve_texpr();
    unknown_eval_order = false;
    break;
  case TOK_OR_ASSIGN:
  case TOK_AND_ASSIGN:
  case TOK_XOR_ASSIGN:
    check_boolean_algebra(this, arg0);
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
    if (!is_vardef_constructor(this)) {
      check_copyable(this, arg1);
    }
    type_of_this_expr = arg0->resolve_texpr();
    break;
  case '?':
    check_bool_expr(this, arg0);
    type_of_this_expr = arg1->resolve_texpr();
    unknown_eval_order = false;
    break;
  case ':':
    check_type_convert_to_lhs(this, arg1, arg0->resolve_texpr());
    type_of_this_expr = arg0->resolve_texpr();
    unknown_eval_order = false;
    break;
  case TOK_LOR:
  case TOK_LAND:
    check_bool_expr(this, arg0);
    check_type_convert_to_lhs(this, arg1, arg0->resolve_texpr());
    type_of_this_expr = builtins.type_bool;
    unknown_eval_order = false;
    break;
  case '|':
  case '^':
  case '&':
    check_boolean_algebra(this, arg0);
    check_type_convert_to_lhs(this, arg1, arg0->resolve_texpr());
    type_of_this_expr = arg0->resolve_texpr();
    break;
  case TOK_EQ:
  case TOK_NE:
    check_equality_type(this, arg0);
    check_type_convert_to_lhs(this, arg1, arg0->resolve_texpr());
    type_of_this_expr = builtins.type_bool;
    break;
  case '<':
  case '>':
  case TOK_LE:
  case TOK_GE:
    check_ordered_type(this, arg0);
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
    check_integral_expr(this, arg0);
    check_lvalue(this, arg0);
    type_of_this_expr = arg0->resolve_texpr();
    break;
  case TOK_CASE:
    check_variant_field(this, arg0);
    type_of_this_expr = builtins.type_bool;
    break;
  case '!':
    check_bool_expr(this, arg0);
    type_of_this_expr = builtins.type_bool;
    break;
  case '~':
    check_boolean_algebra(this, arg0);
    type_of_this_expr = arg0->resolve_texpr();
    break;
  case TOK_PTR_DEREF:
  case '[':
    if (op == TOK_PTR_DEREF && is_cm_pointer_family(arg0->resolve_texpr())) {
      /* pointer types */
      #if 0
      if (type_has_invalidate_guard(arg0->resolve_texpr())) {
	/* threaded pointer dereference. copy the ptr and lock it */
	passing_root_requirement(passby_e_const_value, this, this, false);
      }
      #endif
      type_of_this_expr = get_pointer_deref_texpr(this, arg0->resolve_texpr());
    } else {
      /* containers or ranges */
      term idxt = (op == '[')
	? get_array_index_texpr(this, arg0->resolve_texpr())
	: term();
      if (arg1 != 0 && is_range_op(arg1)) {
	/* array slice */
	expr_op *const eoprange = ptr_down_cast<expr_op>(arg1);
	expr_i *const range_begin = eoprange->arg0;
	expr_i *const range_end = eoprange->arg1;
	if (is_integral_type(idxt) &&
	  is_integral_type(range_begin->resolve_texpr())) {
	  /* need not to convert */
	} else {
	  check_type_convert_to_lhs(this, range_begin, idxt);
	}
	if (is_integral_type(idxt) &&
	  is_integral_type(range_end->resolve_texpr())) {
	  /* need not to convert */
	} else {
	  check_type_convert_to_lhs(this, range_end, idxt);
	}
	type_of_this_expr = get_array_range_texpr(this, arg0,
	  arg0->resolve_texpr());
	if (type_of_this_expr.is_null()) {
	  arena_error_throw(this, "using an array without type parameter");
	}
      } else {
	/* array element */
	if (is_map_family(arg0->resolve_texpr())) {
	  /* getting map element can cause implicit inserting */
	  check_lvalue(this, arg0);
	}
	if (op == TOK_PTR_DEREF) {
	  if (!is_cm_range_family(arg0->resolve_texpr())) {
	    arena_error_throw(this, "can not apply '%s'",
	      tok_string(this, op));
	  }
	} else if (is_integral_type(idxt) &&
	  is_integral_type(arg1->resolve_texpr())) {
	  /* need not to convert */
	} else {
	  check_type_convert_to_lhs(this, arg1, idxt);
	}
	type_of_this_expr = get_array_elem_texpr(this, arg0->resolve_texpr());
	if (type_of_this_expr.is_null()) {
	  arena_error_throw(this, "using an array without type parameter");
	}
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
  /* rooting */
  if (op == TOK_PTR_DEREF && is_cm_pointer_family(arg0->resolve_texpr()) &&
    type_has_invalidate_guard(arg0->resolve_texpr())) {
    /* threaded pointer dereference. copy the ptr and lock it */
    passing_root_requirement(passby_e_const_value, this, this, false);
  }
  if (is_assign_incdec_op(op)) {
    /* arg0 is an lvalue */
    DBG_CON(fprintf(stderr, "arg0: %s\n", arg0->dump(0).c_str()));
    if (arg0->get_esort() == expr_e_var) {
      /* var x = ... */
      if (op != '=') {
	arena_error_throw(this, "invalid expression (invalid assignment)");
      }
      expr_var *const ev = ptr_down_cast<expr_var>(arg0);
      if (ev->varinfo.passby == passby_e_mutable_reference) {
	/* foo mutable & x = ... requires rhs to have lvalue */
	check_lvalue(this, arg1);
      }
      if (is_passby_cm_reference(ev->varinfo.passby) ||
	is_ephemeral_value_type(ev->resolve_texpr())) {
	/* require block scope root on rhs because the variable is an ephemeral
	 * variable and is required to be valid while the block is finished.
	 * this is the only case a block scope tempvar is required. */
	DBG_CON(fprintf(stderr, "block scope root rhs %s\n",
	  arg1->dump(0).c_str()));
	add_root_requirement(arg1, ev->varinfo.passby, true);
      } else {
	/* no need to root arg1 because lhs is a variable */
      }
    } else {
      /* asign op, lhs is not a vardef */
      if (is_ephemeral_value_type(arg0->resolve_texpr())) {
	/* 'w1 = w2' is not allowed for ephemeral types, because these variables
	 * may depend different objects */
	arena_error_throw(this,
	  "invalid assignment ('%s' is an ephemeral type)",
	  term_tostr_human(arg0->resolve_texpr()).c_str());
      }
      if (!is_assignable(arg0->resolve_texpr())) {
	/* 'v1 = v2' is not allowed. darray for example. */
	arena_error_throw(this,
	  "invalid assignment ('%s' is not an assignable type)",
	  term_tostr_human(arg0->resolve_texpr()).c_str());
      }
      if (arg1 != 0) {
	passing_root_requirement_fast(this, arg1, false);
      }
      expr_i *const evo = get_variant_object_if_vfld(arg0);
      if (evo != 0) {
	/* lhs is a variant field. root the variant object. */
	add_root_requirement(evo, passby_e_mutable_reference, false);
      } else {
	/* lhs is not a variant field. root the lhs. */
	add_root_requirement(arg0, passby_e_mutable_reference, false);
      }
    }
  } else {
    /* not an assign op */
    if (arg0 != 0) {
      passing_root_requirement_fast(this, arg0, false);
    }
    if (arg1 != 0) {
      passing_root_requirement_fast(this, arg1, false);
    }
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

term get_func_te_for_funccall(expr_i *func, bool& memfunc_w_explicit_obj_r)
{
  memfunc_w_explicit_obj_r = false;
  if (func->get_sdef() != 0) {
    symbol_common *const sc = func->get_sdef();
    return sc->resolve_evaluated();
  } else if (func->get_esort() == expr_e_op) {
    expr_op *const eop = ptr_down_cast<expr_op>(func);
    symbol_common *const sc = eop->arg1->get_sdef();
    if (sc != 0 && (eop->op == '.' || eop->op == TOK_ARROW)) {
      memfunc_w_explicit_obj_r = true;
      return sc->resolve_evaluated();
    }
  }
  return term();
}

void expr_funccall::check_type(symbol_table *lookup)
{
  fn_check_type(func, lookup);
  /* check args */
  {
    /* can't use fn_check_type(arg, lookup) because it assumes arg as
     * comma ops */
    typedef std::list<expr_i *> arglist_type;
    arglist_type arglist;
    append_comma_sep_list(arg, arglist);
    for (arglist_type::const_iterator i = arglist.begin(); i != arglist.end();
      ++i) {
      fn_check_type(*i, lookup);
    }
  }
  DBG_DEFCON(fprintf(stderr, "expr=[%s] expr_funccall::check_type\n",
    this->dump(0).c_str()));
  if (arg != 0 && is_void_type(arg->resolve_texpr())) {
    arena_error_throw(arg, "expression '%s' is of type void",
      arg->dump(0).c_str());
  }
  bool memfunc_w_explicit_obj = false;
  term func_te = get_func_te_for_funccall(func, memfunc_w_explicit_obj);
  if (func_te.is_null()) {
    arena_error_throw(func, "expression '%s' is not a function",
      func->dump(0).c_str());
  }
  if (term_has_tparam(func_te)) {
    arena_error_throw(func,
      "termplate expression '%s' has an unbound type parameter",
      term_tostr_human(func_te).c_str());
  }
  const term_list *const func_te_args = func_te.get_args();
  expr_i *const func_inst = term_get_instance(func_te);
  /* TODO: fix the copy-paste of passing_root_requirement calls */
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
      /* check argument types */
      if (!convert_type(*j, ad->resolve_texpr(), tvmap)) {
	const std::string s0 = term_tostr_human((*j)->resolve_texpr());
	const std::string s1 = term_tostr_human(ad->resolve_texpr());
	arena_error_push(this, "invalid conversion from %s to %s",
	  s0.c_str(), s1.c_str());
	arena_error_push(this, "  initializing argument %u of '%s'",
	  argcnt, efd->sym);
      }
      /* check lvalue and root argument expressions */
      passing_root_requirement(ad->passby, this, *j, false);
      ++j;
      ad = ad->rest;
      ++argcnt;
    }
    if (func->get_esort() == expr_e_op &&
      efd->is_virtual_or_member_function()) {
      /* check lvalue and root the foo of foo.bar(...) */
      expr_i *const thisexpr = ptr_down_cast<expr_op>(func)->arg0;
      passing_root_requirement(
	efd->is_const ? passby_e_const_reference : passby_e_mutable_reference,
	this, thisexpr, false);
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
	if (!check_term_validity(einst->get_value_texpr(), true, true, true,
	  this, err_mess)) {
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
      if (!check_term_validity(efd->get_value_texpr(), true, true, true,
	this, err_mess)) {
	// abort();
	arena_error_push(this, "incomplete template expression: '%s' %s",
	  term_tostr_human(efd->get_value_texpr()).c_str(), err_mess.c_str());
      }
      type_of_this_expr = apply_tvmap(efd->get_rettype(), tvmap);
    }
    funccall_sort = funccall_e_funccall;
    const bool call_wo_obj = func->get_esort() != expr_e_op;
    expr_funcdef *const caller_memfunc = get_up_member_func(symtbl_lexical);
    if (call_wo_obj && caller_memfunc != 0 && caller_memfunc->is_const) {
      if (efd->is_virtual_or_member_function() && !efd->is_const) {
	arena_error_throw(this,
	  "calling a non-const member function from a const member function");
      }
    }
    if (call_wo_obj) {
      /* caller's struct context */
      expr_i *const caller_memfunc_parent
	= caller_memfunc != 0
	  ? caller_memfunc->is_virtual_or_member_function()
	    /* caller is in a member function or its local function. */
	  : get_current_frame_expr(symtbl_lexical);
	    /* caller is not a member function. can be a struct constr. */
      /* callee's struct context */
      expr_i *const callee_memfunc_parent
	= efd->is_virtual_or_member_function();
      if (callee_memfunc_parent != 0 &&
	caller_memfunc_parent != callee_memfunc_parent) {
	arena_error_throw(this,
	  "calling a member function without an object");
      }
    }
      #if 0
      expr_struct *const caller_memfunc_struct =
	caller_memfunc->is_member_function(); /* IMF OK */
      /* moved to check_tpup_thisptr_constness() */
      if (efd->tpup_thisptr == caller_memfunc_struct &&
	!efd->tpup_thisptr_nonconst) {
	arena_error_throw(this,
	  "calling a non-const member function '%s' from a const "
	  "member function", term_tostr_human(efd->value_texpr).c_str());
      }
      #endif
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
    DBG_DEFCON(fprintf(stderr, "expr=[%s] type_of_this_expr=[%s] defcon\n",
      this->dump(0).c_str(),
      term_tostr(type_of_this_expr, term_tostr_sort_cname).c_str()));
    if (!is_default_constructible(func_te, this, 0)) {
      /* TODO: more user friendly error message */
      arena_error_push(this, "type '%s' is not default-constructible",
	term_tostr_human(func_te).c_str());
    }
    return;
  }
  // if (func_inst->get_esort() == expr_e_struct) {
  if (func_inst != 0 && func_inst->get_esort() == expr_e_struct) {
    /* struct constructor */
    tvmap_type tvmap;
    expr_struct *est = ptr_down_cast<expr_struct>(func_inst);
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
	passing_root_requirement(ad->passby, this, *j, false);
	++j;
	ad = ad->rest;
	++argcnt;
      }
      if (j != arglist.end()) {
	arena_error_push(this, "too many argument for '%s'", est->sym);
      } else if (ad != 0) {
	arena_error_push(this, "too few argument for '%s'", est->sym);
      }
    } else if (est->cname != 0) {
      typedef std::list<expr_i *> arglist_type;
      arglist_type arglist;
      append_hidden_this(func, arglist);
      append_comma_sep_list(arg, arglist);
      if (is_cm_pointer_family(func_te)) {
	if (arglist.size() != 1) {
	  arena_error_push(this, "too many argument for '%s'", est->sym);
	}
	expr_i *const j = arglist.front();
	if (is_cm_pointer_family(j->resolve_texpr())) {
#if 0
fprintf(stderr, "ptr to ptr? %s\n", dump(0).c_str());
#endif
	  term tgto = get_pointer_target(func_te);
	  term tgfrom = get_pointer_target(j->resolve_texpr());
	  if (tgto == tgfrom &&
	    pointer_conversion_allowed(
	      get_family(j->resolve_texpr()), get_family(func_te))) {
	    /* pointer from another pointer */
	    passing_root_requirement(passby_e_const_reference, this, j, false);
	      /* root the arg */
	    type_of_this_expr = func_te;
	    funccall_sort = funccall_e_struct_constructor;
	    return;
	  }
	}
	/* value to pointer */
	const term_list *const al = func_te.get_args();
	if (al != 0 && !al->empty()) {
	  /* func_te is concrete */
	  term tg = get_pointer_target(func_te);
	  if (is_interface(tg)) {
	    arena_error_push(this, "pointer target is an interface: %s",
	      term_tostr_human(func_te).c_str());
	  }
	  if (!convert_type(j, tg, tvmap)) {
	    const std::string s0 = term_tostr_human(j->resolve_texpr());
	    const std::string s1 = term_tostr_human(tg);
	    arena_error_push(this, "invalid conversion from %s to %s",
	      s0.c_str(), s1.c_str());
	  }
	  passing_root_requirement(passby_e_const_reference, this, j, false);
	    /* root the arg */
	  type_of_this_expr = func_te;
	} else {
	  /* func_te has a tparam. expr is of the form ptr(x). */
	  term_list tl;
	  tl.push_back(j->resolve_texpr());
	  term rt(func_te.get_expr(), tl);
	  passing_root_requirement(passby_e_const_reference, this, j, false);
	    /* root the arg */
	  /* need not to eval rt, because it' always irreducible. */
	  type_of_this_expr = rt;
	}
	funccall_sort = funccall_e_struct_constructor;
	return;
      } else if (get_family(func_te) == typefamily_e_darray) {
	/* darray constructor */
	if (arglist.size() < 2) {
	  arena_error_throw(this, "too few argument for '%s'", est->sym);
	} else if (arglist.size() > 2) {
	  arena_error_throw(this, "too many argument for '%s'", est->sym);
	}
	check_unsigned_integral_expr(0, arglist.front());
	expr_i *const j = *(++arglist.begin());
	const term_list *const arr_te_targs = func_te.get_args();
	if (arr_te_targs == 0 || arr_te_targs->size() != 1) {
	  arena_error_throw(this, "using an array without type parameter");
	}
	term tto = (*arr_te_targs)[0];
	if (!convert_type(j, tto, tvmap)) {
	  const std::string s0 = term_tostr_human(j->resolve_texpr());
	  const std::string s1 = term_tostr_human(tto);
	  arena_error_push(this, "invalid conversion from %s to %s",
	    s0.c_str(), s1.c_str());
	  arena_error_push(this, "  initializing argument %u of '%s'",
	    0, est->sym);
	}
	passing_root_requirement_fast(this, j, false);
	  /* root the arg. */
	type_of_this_expr = func_te;
	funccall_sort = funccall_e_struct_constructor;
	return;
      } else if (get_family(func_te) == typefamily_e_varray) {
	/* varray constructor */
	const term_list *const arr_te_targs = func_te.get_args();
	if (arr_te_targs == 0 || arr_te_targs->size() != 1) {
	  arena_error_throw(this, "using an array without type parameter");
	}
	if (arglist.size() != 1) {
	  arena_error_throw(this, "too many argument for '%s'", est->sym);
	}
	expr_i *const j = arglist.front();
	check_unsigned_integral_expr(0, j);
	passing_root_requirement_fast(this, j, false);
	  /* root the arg. */
	type_of_this_expr = func_te;
	funccall_sort = funccall_e_struct_constructor;
	return;
      } else if (is_numeric_type(func_te)) {
	/* explicit conversion to external numeric type */
	if (arglist.size() < 1) {
	  arena_error_throw(this, "too few argument for '%s'", est->sym);
	} else if (arglist.size() > 1) {
	  arena_error_throw(this, "too many argument for '%s'", est->sym);
	}
	expr_i *const j = arglist.front();
	if (!convert_type(j, func_te, tvmap)) {
	  const std::string s0 = term_tostr_human(j->resolve_texpr());
	  const std::string s1 = term_tostr_human(func_te);
	  arena_error_push(this, "invalid conversion from %s to %s",
	    s0.c_str(), s1.c_str());
	}
	passing_root_requirement_fast(this, j, false);
	  /* root the arg. */
	type_of_this_expr = func_te;
	funccall_sort = funccall_e_struct_constructor;
      } else {
	arena_error_push(this,
	  "can't call a constructor for an extern struct '%s'", est->sym);
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
	passing_root_requirement(passby_e_const_reference, this, *j, false);
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
    /* need not to root (*j) */ /* <---- WHY? */
    passing_root_requirement_fast(this, *j, false);
      /* root the arg. */
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
  if (block1->argdecls == 0) {
    /* if (..) */
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
  }
  fn_check_type(block1, lookup);
  if (block1->argdecls != 0) {
    /* if (var x : m[key]) */
    term tce;
    if (cond->get_esort() == expr_e_op &&
      ptr_down_cast<expr_op>(cond)->op == '[') {
      tce = ptr_down_cast<expr_op>(cond)->arg0->resolve_texpr();
    }
    if (!is_map_family(tce)) {
      const std::string s0 = term_tostr(cond->resolve_texpr(),
	term_tostr_sort_humanreadable);
      arena_error_push(this, "map type expected (got: %s)", s0.c_str());
      return;
    }
    term telm = get_array_elem_texpr(0, tce);
    assert(elist_length(block1->argdecls) == 1);
    term& ta = block1->argdecls->resolve_texpr();
    if (!is_same_type(telm, ta)) {
      arena_error_push(this, "invalid type for '%s' (got: %s, expected: %s)",
	block1->argdecls->sym,
	term_tostr_human(ta).c_str(),
	term_tostr_human(telm).c_str());
    }
    passing_root_requirement(block1->argdecls->passby, this, cond, true);
  }
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

void expr_forrange::check_type(symbol_table *lookup)
{
  fn_check_type(r0, lookup);
  fn_check_type(r1, lookup);
  fn_check_type(block, lookup);
  term& ta0 = block->argdecls->resolve_texpr();
  if (!is_integral_type(ta0)) {
    arena_error_push(block->argdecls, "integral type expected");
  }
  check_type_convert_to_lhs(0, r0, ta0);
  check_type_convert_to_lhs(0, r1, ta0);
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
  passing_root_requirement(block->argdecls->rest->passby, this, ce, true);
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
    /* var is not allowed because it requires block_id_ns which is not
     * generated inside a generic foreach block */
  case expr_e_enumval:
  // case expr_e_block:
  case expr_e_feach:
  case expr_e_fldfe:
  case expr_e_foldfe:
  case expr_e_argdecls:
  case expr_e_funcdef:
  case expr_e_typedef:
  case expr_e_macrodef:
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
      ev->get_fields(flds);
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
  size_t idx = syms.size() - 1;
  for (strlist::reverse_iterator i = syms.rbegin(); i != syms.rend();
    ++i, --idx) {
    expr_stmts *const st = ptr_down_cast<expr_stmts>(deep_clone_expr(st0));
    DBG_FLDFE(fprintf(stderr, "replace n=%s fld=%s dst=%s\n",
      (this->namesym ? this->namesym : ""), this->fldsym, i->c_str()));
    const std::string dststr(*i);
    subst_symbol_name(st, this->fldsym, term(dststr), true);
    if (this->namesym != 0) {
      subst_symbol_name(st, this->namesym, term(dststr), false);
    }
    if (this->idxsym != 0) {
      term idxt(idx);
      subst_symbol_name(st, this->idxsym, term(idx), false);
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
  fn_update_tree(this->stmts, this, lookup, uniqns, injectns);
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
  fn_update_tree(this->stmts, this, lookup, uniqns, injectns);
  fn_check_type(stmts, lookup);
}

void expr_argdecls::define_vars_one(expr_stmts *stmt)
{
  symtbl_lexical->define_name(sym, injectns, this, attribute_private, stmt);
}

void expr_argdecls::check_type(symbol_table *lookup)
{
  term& typ = resolve_texpr();
  expr_i *const fr = get_current_frame_expr(lookup);
  if (fr != 0) {
    expr_block *const tbl = fr->get_template_block();
    if (!tbl->tinfo.is_uninstantiated()) {
      check_var_type(typ, this, sym, is_passby_cm_reference(passby), true);
    }
  }
  fn_check_type(rest, lookup);
  /* type_of_this_expr */
}

void expr_funcdef::check_type(symbol_table *lookup)
{
  if (this->is_const && !this->is_virtual_or_member_function()) {
    arena_error_throw(this, "non-member/virtual function can not be const");
  }
  if ((this->get_attribute() & attribute_multithr) != 0) {
    // FIXME: valuetype, tsvaluetype
    arena_error_throw(this, "invalid attribute for a function");
  }
  if (this->is_virtual_or_member_function() &&
    (this->get_attribute() & attribute_threaded) != 0) {
    arena_error_throw(this,
      "invalid attribute for a member/virtual function");
  }
  if (!this->is_virtual_or_member_function() &&
    (this->get_attribute() & attribute_threaded) != 0) {
    symbol_table *parent = this->block->symtbl.get_lexical_parent();
    expr_i *fr = get_current_frame_expr(parent);
    if (fr != 0 && fr->get_esort() == expr_e_funcdef) {
      if ((get_expr_threading_attribute(fr) & attribute_threaded) == 0) {
	arena_error_throw(this, "parent function is not threaded");
      }
    } else if (fr != 0) {
      arena_error_throw(this,
	"internal error: threaded function in an invalid context");
    }
  }
  if (ext_decl && !block->tinfo.template_descent) {
    /* external and not template. stmts are not necessary. */
    block->stmts = 0;
  }
  fn_check_type(block, lookup);
  #if 0
  // moved to fn_compile. need to be executed after all functions are compiled.
  add_tparam_upvalues_funcdef(this);
  #endif
}

void expr_typedef::check_type(symbol_table *lookup)
{
  fn_check_type(enumvals, lookup);
}

void expr_macrodef::check_type(symbol_table *lookup)
{
  if ((this->get_attribute() & attribute_threaded) != 0) {
    arena_error_throw(this, "invalid attribute for a macro");
  }
  fn_check_type(block, lookup);
}

static void check_udcon_vardef_calling_memfunc(expr_struct *est, expr_i *e)
{
  if (e == 0) {
    return;
  }
  int n = e->get_num_children();
  if (e->get_esort() == expr_e_funccall) {
    expr_funccall *fc = ptr_down_cast<expr_funccall>(e);
    bool w_explicit_obj = false;
    term func_te = get_func_te_for_funccall(fc->func, w_explicit_obj);
    expr_i *const func_inst = term_get_instance(func_te);
    expr_funcdef *const efd = dynamic_cast<expr_funcdef *>(func_inst);
    if (efd != 0 && efd->is_virtual_or_member_function() && !w_explicit_obj) {
      arena_error_push(e,
	"calling member function before field initialization");
    }
  }
  for (int i = 0; i < n; ++i) {
    check_udcon_vardef_calling_memfunc(est, e->get_child(i));
  }
}

static void check_constr_restrictions(expr_struct *est)
{
  if (est->block == 0 || est->block->stmts == 0) {
    return;
  }
  expr_stmts *st = est->block->stmts;
  expr_i *exec_found = 0;
  for (; st != 0; st = st->rest) {
    expr_i *const e = st->head;
    if (is_vardef_or_vardefset(e)) {
      if (exec_found != 0) {
	arena_error_push(e, "invalid statement before field initialization");
      }
      check_udcon_vardef_calling_memfunc(est, e);
    } else if (!is_noexec_expr(e)) {
      if (exec_found == 0) {
	exec_found = e;
      }
    }
    #if 0
    bool ok = false;
    switch (e->get_esort()) {
    case expr_e_var:
      ok = true;
      break;
    case expr_e_op:
      {
	expr_op *const eop = ptr_down_cast<expr_op>(e);
	if (eop->op == '=' && eop->arg0->get_esort() == expr_e_var) {
	  ok = true;
	}
      }
      break;
    case expr_e_funcdef:
    case expr_e_struct:
    case expr_e_variant:
    case expr_e_macrodef:
      ok = true;
      break;
    default:
      break;
    }
    if (!ok) {
      arena_error_push(e, "invalid statement");
    }
    #endif
    #if 0
    check_constr_calling_memfunc(est, e);
    #endif
  }
}

void expr_struct::check_type(symbol_table *lookup)
{
  fn_check_type(block, lookup);
  check_inherit_threading(this);
  check_constr_restrictions(this);
  if (typefamily_str != 0 && typefamily_str[0] != '@' && typefamily_str[0] != 0 &&
    typefamily == typefamily_e_none) {
    arena_error_throw(this, "invalid type family: \'%s\'", typefamily_str);
  }
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

static void add_tparam_upvalues_tp_internal(expr_funcdef *efd, const term& t)
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
	DBG_TPUP(fprintf(stderr, "TPUP %s tp %s\n", efd->dump(0).c_str(),
	  e->dump(0).c_str()));
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
      add_tparam_upvalues_tp_internal(efd, *j);
    }
  }
}

void add_tparam_upvalues_funcdef_tparam(expr_funcdef *efd)
{
  /* tparam upvalues */
  term& t = efd->value_texpr;
  const term_list *const targs = t.get_args();
  if (targs == 0 || targs->empty()) {
    return;
  }
  term_list::const_iterator j;
  for (j = targs->begin(); j != targs->end(); ++j) {
    add_tparam_upvalues_tp_internal(efd, *j);
  }
}

void add_tparam_upvalues_funcdef_direct(expr_funcdef *efd)
{
  /* direct upvalues */
  DBG_TPUP(fprintf(stderr, "TPUP %s\n", efd->dump(0).c_str()));
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
      DBG_TPUP(fprintf(stderr, "TPUP %s direct %s\n", efd->dump(0).c_str(),
	e->dump(0).c_str()));
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
}

void check_tpup_thisptr_constness(expr_funccall *fc)
{
  bool memfunc_w_explicit_obj = false;
  term func_te = get_func_te_for_funccall(fc->func, memfunc_w_explicit_obj);
  expr_i *const func_inst = term_get_instance(func_te);
  if (is_function(func_te)) {
    expr_funcdef *efd = ptr_down_cast<expr_funcdef>(func_inst);
    expr_funcdef *const caller_memfunc = get_up_member_func(fc->symtbl_lexical);
    if (caller_memfunc != 0 && caller_memfunc->is_const) {
      expr_struct *const caller_memfunc_struct =
        caller_memfunc->is_member_function(); /* IMF OK */
      if (efd->tpup_thisptr == caller_memfunc_struct &&
        !efd->tpup_thisptr_nonconst) {
        arena_error_throw(fc,
          "calling a non-const member function '%s' from a const "
          "member function", term_tostr_human(efd->value_texpr).c_str());
      }
    }
  }
}

void fn_check_closure(expr_i *e)
{
// TODO: remove this func
#if 0
#endif
}

void fn_check_root(expr_i *e)
{
// TODO: remove this func
#if 0
#endif
}

static void check_type_validity_common(expr_i *e, const term& t)
{
  const term_list *tl = t.get_args();
  if (tl != 0 && !tl->empty()) {
    for (term_list::const_iterator i = tl->begin(); i != tl->end(); ++i) {
      check_type_validity_common(e, *i);
    }
  }
  if (is_cm_pointer_family(t)) {
    if (tl == 0 || tl->empty()) {
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
  } else if (is_container_family(t) || is_cm_range_family(t)) {
    if (tl == 0 || tl->empty()) {
      return;
    }
    const term& te = tl->front();
    if (!is_copyable(te)) {
      arena_error_push(e, "type '%s' is not copyable",
	term_tostr_human(te).c_str());
    }
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

};

