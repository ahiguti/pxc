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
#define DBG_EP(x)
#define DBG_RECHAIN(x)
#define DBG_SETCHILD(x)
#define DBG_VARIADIC(x)
#define DBG_TIMING(x)
#define DBG_TIMING3(x)
#define DBG_EXPAND_TIMING(x)
#define DBG_DYNFLD(x)

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
  check_convert_type(efrom, tto, &tvmap);
  #if 0
    const std::string sfrom = term_tostr(efrom->resolve_texpr(),
      term_tostr_sort_humanreadable);
    const std::string sto = term_tostr(tto,
      term_tostr_sort_humanreadable);
    arena_error_push(efrom, "Can not convert from '%s' to '%s' %s",
      sfrom.c_str(), sto.c_str(), op_message(eop).c_str());
  #endif
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
  arena_error_push(eop != 0 ? eop : a0, "Bool type expected (got: %s) %s",
    s0.c_str(), op_message(eop).c_str());
}

#if 0
static void check_unsigned_integral_expr(expr_op *eop, expr_i *a0)
{
  const term t = a0->resolve_texpr();
  if (is_unsigned_integral_type(t)) {
    return;
  }
  const std::string s0 = term_tostr(a0->resolve_texpr(),
    term_tostr_sort_humanreadable);
  arena_error_push(eop != 0 ? eop : a0,
    "Unsigned integral type expected (got: %s) %s",
    s0.c_str(), op_message(eop).c_str());
}
#endif

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
  return check_type_common(eop, a0, is_integral_type, "Integral type");
}

static void check_boolean_type(expr_op *eop, expr_i *a0)
{
  return check_type_common(eop, a0, is_boolean_type, "Boolean type");
}

static void check_equality_type(expr_op *eop, expr_i *a0)
{
  return check_type_common(eop, a0, is_equality_type, "Equality type");
}

static void check_ordered_type(expr_op *eop, expr_i *a0)
{
  return check_type_common(eop, a0, is_ordered_type, "Ordered type");
}

static void check_numeric_expr(expr_op *eop, expr_i *a0)
{
  return check_type_common(eop, a0, is_numeric_type, "Numeric type");
}

static void check_lvalue(const expr_i *epos, expr_i *a0)
{
  #if 0
  fprintf(stderr, "check_lvalue: %s %d\n", a0->dump(0).c_str(), (int)a0->conv);
  #endif
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
  arena_error_push(eop != 0 ? eop : a0, "Type '%s' is not copyable",
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
  // double t0 = gettimeofday_double();
  fn_check_type(head, lookup);
  // double t1 = gettimeofday_double();
  // if (t1 - t0 > 0.0001) {
  // fprintf(stderr, "slow expr_telist check_type %f %s\n", t1 - t0,
  //   this->dump(0).c_str());
  // }
  fn_check_type(rest, lookup);
}

void expr_te::check_type(symbol_table *lookup)
{
  // double t0 = gettimeofday_double();
  fn_check_type(tlarg, symtbl_lexical);
    /* tlarg always uses lexical context */
  // double t1 = gettimeofday_double();
  check_type_symbol_common(this, lookup);
  // double t2 = gettimeofday_double();
  #if 0
  if (t2 - t0 > 0.0001) {
  fprintf(stderr, "slow expr_te check_type %f %f %s\n", t1 - t0, t2 - t1,
    this->dump(0).c_str());
  }
  #endif
}

void expr_ns::check_type(symbol_table *lookup)
{
  if (import) {
    nspropmap_type::const_iterator i = nspropmap.find(src_uniq_nsstr);
    if (i == nspropmap.end()) {
      arena_error_throw(this, "Internal error: namespace '%s' not found",
	src_uniq_nsstr.c_str());
    }
    nspropmap_type::const_iterator j = nspropmap.find(uniq_nsstr);
    if (j == nspropmap.end()) {
      arena_error_throw(this, "Internal error: namespace '%s' not found",
	uniq_nsstr.c_str());
    }
    if (i->second.safety == nssafety_e_safe &&
      j->second.safety == nssafety_e_export_unsafe) {
      arena_error_throw(this, "Can not import unsafe namespace '%s'",
	uniq_nsstr.c_str());
    }
    if (i->second.safety == nssafety_e_use_unsafe &&
      j->second.safety == nssafety_e_export_unsafe && pub) {
      arena_error_throw(this,
	"Importing unsafe namespace '%s' must be private",
	uniq_nsstr.c_str());
    }
    if (!j->second.is_public) {
      std::string p = src_uniq_nsstr + "::";
      if (uniq_nsstr.substr(0, p.size()) != p) {
	arena_error_throw(this, "Can not import private namespace '%s'",
	  uniq_nsstr.c_str());
      }
    }
  }
}

void expr_symbol::check_type(symbol_table *lookup)
{
  check_type_symbol_common(this, lookup);
}

void expr_inline_c::check_type(symbol_table *lookup)
{
  fn_check_type(value, symtbl_lexical);
  fn_check_type(te_list, symtbl_lexical);
  for (size_t i = 0; i < elems.size(); ++i) {
    elems[i].te->sdef.resolve_evaluated();
  }
  if ((this->get_attribute() & attribute_threaded) != 0) {
    arena_error_throw(this, "Invalid attribute for an inline expression");
  }
  if (value != 0) {
    value_evaluated = eval_expr(value);
    const long long v = meta_term_to_long(value_evaluated);
    if (posstr == "disable-bounds-checking") {
      symtbl_lexical->pragma.disable_bounds_checking = v;
    } else if (posstr == "disable-noheap-checking") {
      symtbl_lexical->pragma.disable_noheap_checking = v;
    } else if (posstr == "disable-guard") {
      symtbl_lexical->pragma.disable_guard = v; // not implemented yet
    } else if (posstr == "trace-meta") {
      symtbl_lexical->pragma.trace_meta = v;
    } else if (posstr == "emit") {
      /* nothing to do */
    } else {
      arena_error_throw(this, "Invalid inline expression");
    }
  }
}

void expr_var::define_vars_one(expr_stmts *stmt)
{
  symtbl_lexical->define_name(sym, uniqns, this, attr, stmt);
}

static bool check_term_validity(const term& t, bool allow_nontype,
  bool allow_local_func, bool allow_noheap, bool allow_noheap_container,
  expr_i *pos, std::string& err_mess_r)
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
  const bool is_noheap_flag = is_noheap_type(t);
  if (is_noheap_flag) {
    if (!allow_noheap) {
      err_mess_r = "(template parameter '" + term_tostr_human(t) +
	"' is a noheap type)";
      return false;
    }
    if (!allow_noheap_container && is_container_family(t)) {
      err_mess_r = "(template parameter '" + term_tostr_human(t) +
	"' is a noheap container type)";
      return false;
    }
  }
  const bool tp_allow_nontype = true;
  const bool tp_allow_local_func = !is_type(t);
  const bool tp_allow_noheap = !is_type(t) || is_noheap_flag;
  const bool tp_allow_noheap_container = !is_type(t) || is_noheap_flag;
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
    if (tparams_len != tlarg_len && (tlarg_len != 0 || !allow_nontype)) {
      /* allows template type or function wo tlarg if allow_nontype is true,
       * so that template type/frunction can be used as a metafunction. */
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
      if (cat == typefamily_e_farray || cat == typefamily_e_cfarray) {
	if (tlarg_len >= 1) {
	  if (!check_term_validity(tl->front(), tp_allow_nontype,
	    tp_allow_local_func, tp_allow_noheap, tp_allow_noheap_container,
	    pos, err_mess_r)) {
	    return false;
	  }
	}
	if (tlarg_len >= 2) {
	  const term& t2nd = *(++tl->begin());
	  if (!t2nd.is_long()) {
	    err_mess_r = "(2nd template parameter must be an integer literal)";
	    return false;
	  }
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
	tp_allow_noheap, tp_allow_noheap_container, pos, err_mess_r)) {
	return false;
      }
    }
  }
  DBG_CTV(fprintf(stderr, "CTV %s true 7\n", term_tostr_human(t).c_str()));
  return true;
}

static void check_var_type(term& typ, expr_i *e, const char *sym,
  bool byref_flag, bool need_copyable, bool allow_noheap_container)
{
  std::string err_mess;
  if (!check_term_validity(typ, false, false, true, allow_noheap_container,
    e, err_mess)) {
    arena_error_push(e, "Invalid type '%s' for variable or field '%s' %s",
      term_tostr_human(typ).c_str(), sym, err_mess.c_str());
  }
  if (is_void_type(typ)) {
    arena_error_push(e, "Variable or field '%s' declared void", sym);
  }
  if (!byref_flag && need_copyable && !is_copyable(typ)) {
    arena_error_push(e, "Noncopyable type '%s' for variable or field '%s'",
      term_tostr_human(typ).c_str(), sym);
  }
  if (term_has_unevaluated_expr(typ)) {
    arena_error_push(e, "Variable or field '%s' has an invalid type '%s'",
      sym, term_tostr_human(typ).c_str());
  }
  if (term_has_tparam(typ)) {
    arena_error_push(e,
      "Internal error: variable or field '%s' has an unbound type parameter",
      sym);
  }
  symbol_table *const cur = get_current_frame_symtbl(e->symtbl_lexical);
// FIXME: not effective?
#if 0
//  if (cur != 0 && is_threaded_context(cur) && !term_is_threaded(typ)) {
//    arena_error_push(e,
//      "Type '%s' for variable or field '%s' is not threaded",
//      term_tostr_human(typ).c_str(), sym);
//  }
#endif
  if (cur != 0 && e->get_esort() == expr_e_var &&
    (cur->block_esort == expr_e_struct || cur->block_esort == expr_e_dunion)) {
    expr_var *const ev = ptr_down_cast<expr_var>(e);
    if (is_noheap_type(typ)) {
      arena_error_push(e,
	"Type '%s' for field '%s' is a noheap type",
	term_tostr_human(typ).c_str(), sym);
    }
    if (is_passby_cm_reference(ev->varinfo.passby)) {
      arena_error_push(e, "Field '%s' can't be a reference", sym);
    }
// FIXME: not effective?
#if 0
//    if (is_multithr_context(cur) && !term_is_multithr(typ)) {
//      arena_error_push(e,
//	"Type '%s' for field '%s' is not multithreaded",
//	term_tostr_human(typ).c_str(), sym);
//      /* note: argdecls need not to be multithreaded */
//    }
#endif
  }
}

static bool is_default_constructible(const term& typ, expr_i *pos,
  size_t depth)
{
  // expr_i *const expr = typ.get_expr();
  const expr_i *const expr = term_get_instance_const(typ);
  const term_list *const args = typ.get_args();
  if (expr == 0) {
    return true; /* TODO: false? */
  }
  if (depth > 1000) {
    arena_error_push(pos, "A type dependency cycle is found: '%s'",
      term_tostr_human(typ).c_str());
    return true;
  }
  ++depth;
  const expr_e esort = expr->get_esort();
  if (esort == expr_e_interface) {
    return false;
  }
  if (esort == expr_e_struct) {
    const expr_struct *const est = ptr_down_cast<const expr_struct>(expr);
    if (est->block->argdecls != 0) {
      /* note that it's defcon even when it has empty udcon */
      return false;
    }
    const typefamily_e cat = est->typefamily;
    if (is_cm_pointer_family(typ)) {
      return false;
    }
    if (cat == typefamily_e_linear || cat == typefamily_e_nodefault) {
      return false;
    }
    if (cat == typefamily_e_farray || cat == typefamily_e_cfarray) {
      return (args == 0 || args->empty()) ? false
	: is_default_constructible(args->front(), pos, depth);
    }
    if (cat == typefamily_e_darray || cat == typefamily_e_cdarray) {
      return false;
    }
    if (cat == typefamily_e_varray || cat == typefamily_e_cvarray ||
      cat == typefamily_e_map || cat == typefamily_e_cmap ||
      cat == typefamily_e_nocascade) {
      return true;
    }
    if (cat != typefamily_e_none || est->cnamei.cname != 0) {
      /* extern c struct */
      for (term_list::const_iterator i = args->begin(); i != args->end(); ++i) {
	if (!is_default_constructible(*i, pos, depth)) {
	  return false;
	}
      }
      return true;
    }
    return true;
  }
  if (esort == expr_e_dunion) {
    const expr_dunion *const ev = ptr_down_cast<const expr_dunion>(expr);
    #if 0
    if (!ev->block->tinfo.is_uninstantiated()) {
      return true;
    }
    #endif
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

// TODO: move to another file
bool is_default_constructible(const term& typ)
{
  return is_default_constructible(typ, typ.get_expr(), 0);
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
  expr_dunion *eva = dynamic_cast<expr_dunion *>(cbexpr);
  if (eva != 0 && eva->get_first_field() != ev) {
    /* only 1st field of dunion need to be default-constructible */
    return;
  }
  if (!is_default_constructible(typ, ev, 0)) {
    arena_error_push(ev, "Type '%s' is not default-constructible",
      term_tostr_human(typ).c_str());
  }
}

void expr_var::check_type(symbol_table *lookup)
{
  resolve_texpr();
}

void expr_var::check_defcon()
{
  term& typ = resolve_texpr();
  bool need_defcon = true;
  bool need_copyable = false;
  expr_op *const parent_eop = dynamic_cast<expr_op *>(parent_expr);
  if (parent_eop != 0 && parent_eop->op == '=') {
    need_defcon = false;
    const bool incl_byref = true;
    if (!is_vardef_constructor_or_byref(parent_eop, incl_byref)) {
      need_copyable = true;
    }
  }
  bool allow_noheap_container = !is_passby_mutable(varinfo.passby);
  if (symtbl_lexical->pragma.disable_noheap_checking) {
    allow_noheap_container = true;
  }
  check_var_type(typ, this, sym, is_passby_cm_reference(varinfo.passby),
    need_copyable, allow_noheap_container);
  if (is_global_var(this)) {
    if (is_noheap_type(typ)) {
      arena_error_push(this,
	"Type '%s' for global variable '%s' is a noheap type",
	term_tostr_human(typ).c_str(), sym);
    }
    const attribute_e tattr = get_term_threading_attribute(typ);
    if ((tattr & attribute_threaded) == 0) {
      arena_error_push(this,
	"Type '%s' for global variable '%s' is not threaded",
	term_tostr_human(typ).c_str(), sym);
    }
    need_defcon = true;
  }
  if (need_defcon) {
    /* requires default constructor */
    check_default_construct(typ, this, sym);
    if (is_passby_cm_reference(varinfo.passby)) {
      arena_error_push(this, "Reference variable '%s' is not initialized",
	sym);
    }
    #if 0
    // TODO: remove. not necessary.
    if (is_noheap_type(typ)) {
      arena_error_push(this, "Ephemeral type variable '%s' is not initialized",
	sym);
    }
    #endif
  }
  /* type_of_this_expr */
}

static void eval_cname_info(cname_info& cnamei, expr_i *pos, const char *sym)
{
  const char *const c = cnamei.cname;
  if (c == 0 || strchr(c, '%') == 0) {
    return;
  }
  const size_t len = strlen(c);
  std::string s;
  for (size_t i = 0; i < len; ++i) {
    const char ch = c[i];
    if (ch != '%') {
      s.push_back(ch);
    } else {
      s.insert(s.end(), sym, sym + strlen(sym));
    }
  }
  cnamei.cname = arena_strdup(s.c_str());
}

void expr_enumval::define_vars_one(expr_stmts *stmt)
{
  eval_cname_info(cnamei, this, sym);
  symtbl_lexical->define_name(sym, uniqns, this, attr, stmt);
}

void expr_enumval::check_type(symbol_table *lookup)
{
  if ((this->get_attribute() & attribute_threaded) != 0) {
    arena_error_throw(this, "Invalid attribute for an external value");
  }
  if (is_void_type(resolve_texpr())) {
    arena_error_push(this, "External value '%s' declared void", sym);
  }
  if (is_interface(resolve_texpr())) {
    arena_error_push(this,
      "External value '%s' declared to be of an interface type", sym);
  }
  fn_check_type(value, lookup);
  fn_check_type(rest, lookup);
  /* type_of_this_expr */
}

void expr_stmts::check_type(symbol_table *lookup)
{
  #if 0
  double t1 = gettimeofday_double();
  #endif
  fn_check_type(head, lookup);
  if (rest != 0 && rest->parent_expr != this) {
    /* this happenes when head is a expr_expand. in this case, rest has
     * moved to the rest of the generated expr. */
    DBG_RECHAIN(fprintf(stderr, "RE-CHAINED\n"));
    return;
  }
  #if 0
  double t2 = gettimeofday_double();
  fprintf(stderr, "stmt %d %f\n", (int)head->get_esort(), t2 - t1); // FIXME
  #endif
  fn_check_type(rest, lookup);
  switch (head->get_esort()) {
  case expr_e_int_literal:
  case expr_e_float_literal:
  case expr_e_bool_literal:
  case expr_e_str_literal:
  case expr_e_symbol:
    if (parent_expr != 0 && parent_expr->parent_expr != 0 &&
      parent_expr->parent_expr->get_esort() == expr_e_metafdef) {
      /* ok for macro rhs */
    } else {
      arena_error_push(this, "Invalid statement");
    }
    break;
  default:
    break;
  }
}

static void check_return_expr_block(expr_funcdef *fdef, expr_block *block)
{
  expr_i *last_stmt = 0;
  for (expr_stmts *st = block->stmts; st != 0; st = st->rest) {
    expr_i *e = st->head;
    if (e->get_esort() == expr_e_expand) {
      abort(); // already extracted
    }
    if (!is_noexec_expr(e)) {
      last_stmt = e;
    }
  }
  #if 0
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
  #endif
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
    if (esp->tok == TOK_RETURN || esp->tok == TOK_THROW) {
      return; /* ok */
    }
  }
  arena_error_push(error_pos, "Control reaches end of non-void function");
}

static bool is_ext_struct_memfunc(expr_funcdef *fdef)
{
  expr_struct *est = fdef->is_member_function();
  return (est != 0 && est->cnamei.has_cname());
}

static void check_return_expr(expr_funcdef *fdef)
{
  const term& typ = fdef->get_rettype();
  std::string err_mess;
  bool allow_noheap = fdef->cnamei.has_cname()
    || is_ext_struct_memfunc(fdef);
    /* allows noheap type if fdef is an external c function or a member
     * function of an external struct */
  if (fdef->block != 0 && fdef->block->symtbl.pragma.disable_noheap_checking) {
    allow_noheap = true;
  }
  if (!check_term_validity(typ, false, false, allow_noheap,
    allow_noheap, fdef, err_mess)) {
    arena_error_push(fdef, "Invalid return type '%s' %s",
      term_tostr_human(typ).c_str(), err_mess.c_str());
  }
  if (is_void_type(typ)) {
    return; /* returning void */
  }
  if (fdef->no_def) {
    return;
  }
  if (fdef->ext_pxc && !fdef->block->tinfo.template_descent) {
    /* non-template function defined in non-main module. body is dropped. */
    DBG_CT_BLOCK(fprintf(stderr, "ext_pxc %s\n", fdef->dump(0).c_str()));
    return; /* decl only */
  }
  check_return_expr_block(fdef, fdef->block);
}

void expr_block::check_type(symbol_table *lookup)
{
  if (tparams_error != 0) {
    arena_error_throw(tparams_error, "Syntax error near '%s'",
      tparams_error->dump(0).c_str());
  }
  DBG_CT_BLOCK(fprintf(stderr, "%s: %p\n", __PRETTY_FUNCTION__, this));
  #if 0
  double t1 = gettimeofday_double();
  #endif
  /* compiling argdecls and rettype is necessary for function type
   * automatching. */
  fn_check_type(argdecls, &symtbl);
  #if 0
  double t2 = gettimeofday_double();
  #endif
  fn_check_type(rettype_uneval, &symtbl);
  #if 0
  double t3 = gettimeofday_double();
  #endif
  /* stmts are compiled only if it's instantiated. */
  if (!tinfo.is_uninstantiated()) {
    DBG_CT_BLOCK(fprintf(stderr, "%s: %s: instantiated\n",
      __PRETTY_FUNCTION__, this->dump(0).c_str()));
    fn_check_type(inherit, &symtbl);
    fn_check_type(stmts, &symtbl);
    if (parent_expr != 0 && parent_expr->get_esort() == expr_e_funcdef) {
      expr_funcdef *efd = ptr_down_cast<expr_funcdef>(parent_expr);
      check_return_expr(efd);
    } // FIXME: check_return_expr for struct constructor?
    /* evaluate __static_assert__ */
    bool is_global = false;
    bool is_upvalue = false;
    bool is_memfld = false;
    const bool no_private = false;
    expr_i *sa = symtbl.resolve_name_nothrow("__static_assert__",
      no_private, "", is_global, is_upvalue, is_memfld);
    if (sa != 0 && !is_upvalue && !is_global &&
      sa->get_esort() == expr_e_metafdef) {
      const term t = eval_term(
	term(ptr_down_cast<expr_metafdef>(sa)->get_rhs()));
      if (t != term(1LL)) {
	arena_error_throw(sa, "Static assertion failed");
      }
    }
    compiled_flag = true;
  }
  #if 0
  double t4 = gettimeofday_double();
  fprintf(stderr, "block %f %f %f %s %s\n", t2 - t1, t3 - t2, t4 - t3,
    argdecls ? argdecls->dump(0).c_str() : "",
    rettype_uneval ? rettype_uneval->dump(0).c_str() : ""); // FIXME
  #endif
}

static void check_dunion_field(const expr_op *eop, expr_i *a0)
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
    if (val == 0 || !is_dunion(val->resolve_texpr())) {
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
    expr_dunion *const vari = dynamic_cast<expr_dunion *>(bl->parent_expr);
    if (vari == 0) {
      break;
    }
    /* ok */
    return;
  } while (0);
  arena_error_push(eop, "Union field expected");
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
  e->tempvar_varinfo.passby = merge_passby(e->tempvar_varinfo.passby, passby);
  e->tempvar_varinfo.guard_elements |= guard_elements;
  e->tempvar_varinfo.scope_block |= blockscope_flag;
  /* note: temp variable is created as of type e->resolve_texpr() even when
   * it is implicitly converted to another type. */
  if (is_passby_cm_value(passby) && !is_copyable(e->resolve_texpr())) {
    arena_error_throw(e,
      "Can not create a temp copy of type '%s' expr '%s'",
      term_tostr_human(e->resolve_texpr()).c_str(), e->dump(0).c_str());
  }
  if (e->conv == conversion_e_implicit) {
    /* no idea how to reach here, but check for safety */
    arena_error_throw(e,
      "Can not create a temp variable for expr '%s' because of conversion",
      e->dump(0).c_str());
  }
}

static void add_root_requirement_container_elements(expr_i *econ,
  bool blockscope_flag)
{
  /* this function makes econ elements valid */
  DBG_CON(fprintf(stderr, "%s op[] container blockscope=%d\n",
    econ->dump(0).c_str(), (int)blockscope_flag));
  const bool con_lv = expr_has_lvalue(econ, econ, false);
  const bool guard_elements =
    type_has_refguard(econ->resolve_texpr());
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

symbol_common *get_func_symbol_common_for_funccall(expr_i *func,
  bool& memfunc_w_explicit_obj_r)
{
  memfunc_w_explicit_obj_r = false;
  if (func->get_sdef() != 0) {
    symbol_common *const sc = func->get_sdef();
    return sc;
  } else if (func->get_esort() == expr_e_op) {
    expr_op *const eop = ptr_down_cast<expr_op>(func);
    symbol_common *const sc = eop->arg1 ? eop->arg1->get_sdef() : 0;
    if (sc != 0 && (eop->op == '.' || eop->op == TOK_ARROW)) {
      memfunc_w_explicit_obj_r = true;
      return sc;
    }
  }
  return 0;
}

term get_func_te_for_funccall(expr_i *func,
  bool& memfunc_w_explicit_obj_r)
{
  symbol_common *sc = get_func_symbol_common_for_funccall(func,
    memfunc_w_explicit_obj_r);
  return sc != 0 ? sc->resolve_evaluated() : term();
#if 0
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
#endif
}

static bool is_vararg_function_or_struct(expr_i *e)
{
  if (e == 0) {
    return false;
  }
  expr_block *bl = 0;
  if (e->get_esort() == expr_e_funcdef) {
    bl = ptr_down_cast<expr_funcdef>(e)->block;
  } else if (e->get_esort() == expr_e_struct) {
    bl = ptr_down_cast<expr_struct>(e)->block;
  }
  return bl != 0 && bl->is_expand_argdecls();
}

static void add_root_requirement(expr_i *e, passby_e passby,
  bool blockscope_flag)
{
  /* this function makes e valid until the current statement (or block if
   * blockscope_flag is true) is terminated. if e is a noheap value (eg.,
   * range types), the object e refer to is also rooted. */
  if (e == 0) {
    return;
  }
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
	"Can not root by mutable reference because of conversion");
    }
    /* thru */
  }
  if (e->get_esort() == expr_e_funccall) {
    expr_funccall *const efc = ptr_down_cast<expr_funccall>(e);
    if (is_noheap_type(e->resolve_texpr())) {
      /* function returning noheap type, which is only allowed for extern c
       * function. it must be a member function (or a member-like function) of
       * a (possibly) container type, and the returned reference must be valid 
       * while the container is valid and it is not resized. */
      expr_op *const eop = dynamic_cast<expr_op *>(efc->func);
      if (eop != 0 && (eop->op == '.' || eop->op == TOK_ARROW)) {
	/* 'this' object is possibly a container. root the object and it's
	 * elements */
	add_root_requirement_container_elements(eop->arg0, blockscope_flag);
	add_root_requirement(eop->arg0,
	  is_passby_const(passby)
	  ? passby_e_const_reference : passby_e_mutable_reference,
	  blockscope_flag);
	return;
      }
      /* non-member function. it must be an 'extern' function */
      {
	bool memfunc_w_explicit_obj = false;
	term te = get_func_te_for_funccall(efc->func, memfunc_w_explicit_obj);
	expr_funcdef *const efd = dynamic_cast<expr_funcdef *>(
	  term_get_instance(te));
	if (efd != 0) {
	  if (!efd->cnamei.has_cname()) {
	    if (efd->block == 0 ||
	      !efd->block->symtbl.pragma.disable_noheap_checking) {
	      arena_error_throw(efc,
		"Internal error: Non-member, non-extern function returning "
		"a noheap type");
	    }
	  }
	}
      }
      /* when a non-member function returns a noheap type, its arguments must
       * be rooted. */
      typedef std::list<expr_i *> arglist_type;
      arglist_type arglist;
      append_hidden_this(efc->func, arglist); /* noop */
      append_comma_sep_list(efc->arg, arglist);
      for (arglist_type::const_iterator j = arglist.begin();
	j != arglist.end(); ++j) {
	if (is_container_family(e->resolve_texpr())) {
	  /* this funccall returns a container of noheap type. root container
	   * elements also, because it may be converted to .
	   * ( 'const r = make_vector{cslice{int}}(iv0, iv1)' for example ) */
	  add_root_requirement_container_elements(*j, blockscope_flag);
	}
	add_root_requirement(*j,
	  is_passby_const(passby)
	  ? passby_e_const_reference : passby_e_mutable_reference,
	  blockscope_flag);
      }
    }
    /* function returning reference? */
    bool memfunc_w_explicit_obj = false;
    term te = get_func_te_for_funccall(efc->func, memfunc_w_explicit_obj);
    expr_funcdef *const efd = dynamic_cast<expr_funcdef *>(
      term_get_instance(te));
    if (efd != 0 && is_passby_cm_reference(efd->block->ret_passby)) {
      if (efd->is_virtual_or_member_function()) {
	/* member function */
	if (memfunc_w_explicit_obj) {
	  arena_error_throw(efc,
	    "Invalid function call (explicit object, returning reference)");
	}
	/* member function call with implicit object. no need to root the
	 * object. */
      } else {
	/* non-member function */
	expr_i *efcarg = efc->arg;
	if (efcarg == 0) {
	  arena_error_throw(efc,
	    "Invalid function call (no argument, returning reference)");
	}
	if (efcarg->get_esort() == expr_e_op &&
	  ptr_down_cast<expr_op>(efcarg)->op == ',') {
	  arena_error_throw(efc,
	    "Invalid function call (multiple arguments, returning reference)");
	    /* TODO: remove this restriction (root the first arg) */
	}
	if (efd->is_virtual_or_member_function()) {
	  arena_error_throw(efc,
	    "Invalid function call (member function, returning reference)");
	}
	add_root_requirement(efcarg, efd->block->ret_passby, blockscope_flag);
      }
      return;
    }
    if (is_passby_cm_reference(passby) && blockscope_flag) {
      /* expr returns a value, and require blockscope passby-reference.
       * create a tempvar to root the value. 025/blockscope_val?.px */
      store_tempvar(e,
	passby == passby_e_const_reference
	  ? passby_e_const_value : passby_e_mutable_value,
	blockscope_flag, false, "blpr"); /* ROOT */
    }
    return; /* function returning value */
  }
  if (e->get_esort() != expr_e_op) {
    /* function calls, symbols, literals etc. */
    return;
  }
  /* operator */
  expr_op *const eop = ptr_down_cast<expr_op>(e);
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
  case TOK_SHIFTL_ASSIGN:
  case TOK_SHIFTR_ASSIGN:
  case TOK_INC:
  case TOK_DEC:
  case TOK_PTR_REF:
    if (eop->op == '=' && eop->arg0->get_esort() == expr_e_var) {
      /* foo x = ... */
      return; /* always rooted */
    } else {
      add_root_requirement(eop->arg0, passby_e_mutable_reference,
	blockscope_flag);
      add_root_requirement(eop->arg1, passby_e_const_value, blockscope_flag);
    }
    return;
  case '.':
  case TOK_ARROW:
    if (is_dunion(eop->arg0->resolve_texpr())) {
      /* dunion field */
      if (passby == passby_e_mutable_reference) {
	arena_error_throw(e, "Can not root an union field");
	DBG_ROOT(fprintf(stderr, "add_root_requirement: dunion field\n"));
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
    } else if (is_cm_lock_guard_family(eop->arg0->resolve_texpr())) {
      /* lock guard object. root the object. */
      add_root_requirement(eop->arg0, passby_e_const_value, blockscope_flag);
    } else {
      /* pointers */
      /* copy the pointer in order to own a refcount */
      const bool guard_elements = type_has_refguard(
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
    }
    if (is_passby_cm_reference(passby) ||
      (eop->arg1 != 0 && is_range_op(eop->arg1))) {
      /* v[k] is required byref, or v[..] is required. need to guard. */
      add_root_requirement_container_elements(eop->arg0, blockscope_flag);
    } else {
      /* passing container element by value */
      store_tempvar(eop, passby, blockscope_flag, false, "elemval");
    }
    return;
  case '(':
    add_root_requirement(eop->arg0, passby, blockscope_flag);
    return;
  default:
    break;
  }
  /* reaches here if eop is an operation returning byval */
  if (passby == passby_e_mutable_reference) {
    arena_error_throw(e, "Can not root by mutable reference");
  }
  bool child_blockscope_flag = blockscope_flag;
  if (blockscope_flag) {
    store_tempvar(eop,
      is_passby_const(passby)
      ? passby_e_const_value : passby_e_mutable_value,
      blockscope_flag, false, "bscpval"); /* ROOT */
    if (eop->op == '?' && is_noheap_type(e->resolve_texpr())) {
      /* can not store values of 2nd and 3rd exprs to a named temp variable
       * because they must be lazily evaluated. */
      arena_error_throw(e,
	"Can not blockscope-root operator '?:' returning a noheap type");
      /* no need to worry about '||' and '&&' because they always return
       * bool. */
    }
    /* need not to blockscope-root child expressions */
    child_blockscope_flag = false;
  }
  if (eop->arg0 != 0) {
    add_root_requirement(eop->arg0,
      is_passby_const(passby)
      ? passby_e_const_value : passby_e_mutable_value,
      child_blockscope_flag);
  }
  if (eop->op == '?') {
    expr_op *const arg1_eop = ptr_down_cast<expr_op>(eop->arg1);
    add_root_requirement(arg1_eop->arg0,
      is_passby_const(passby)
      ? passby_e_const_value : passby_e_mutable_value,
      child_blockscope_flag);
    add_root_requirement(arg1_eop->arg1,
      is_passby_const(passby)
      ? passby_e_const_value : passby_e_mutable_value,
      child_blockscope_flag);
  } else {
    if (eop->arg1 != 0) {
      add_root_requirement(eop->arg1,
	is_passby_const(passby)
	? passby_e_const_value : passby_e_mutable_value,
	child_blockscope_flag);
    }
  }
}

static bool is_assign_op(int op, const std::string& extop)
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
  case TOK_SHIFTL_ASSIGN:
  case TOK_SHIFTR_ASSIGN:
    return true;
  }
  if (op == TOK_EXTERN) {
    if (extop == "placement-new") {
      return true;
    }
  }
  return false;
}

static bool is_assign_incdec_op(int op, const std::string& extop)
{
  return (is_assign_op(op, extop) || op == TOK_INC || op == TOK_DEC);
}

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
    if (is_assign_incdec_op(eo->op, eo->extop)) {
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

static expr_i *get_dunion_object_if_vfld(expr_i *e)
{
  expr_op *const eop = dynamic_cast<expr_op *>(e);
  if (eop != 0 && (eop->op == '.' || eop->op == TOK_ARROW) &&
    is_dunion(eop->arg0->resolve_texpr())) {
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

static bool is_ifdef_cond_expr(expr_op *eop)
{
  expr_if *const ei = dynamic_cast<expr_if *>(eop->parent_expr);
  if (ei == 0) {
    return false;
  }
  if (ei->block1->argdecls != 0 && ei->cond == eop) {
    return true;
  }
  return false;
}

static void subst_user_defined_op(expr_op *eop)
{
  const char *func = 0;
  bool (*tc0)(const term& t) = 0;
  bool (*tc1)(const term& t) = 0;
  switch (eop->op) {
  /* binary op wo side effect */
  case '+': func = "add"; tc0 = tc1 = is_numeric_type; break;
  case '-': func = "sub"; tc0 = tc1 = is_numeric_type; break;
  case '*': func = "mul"; tc0 = tc1 = is_numeric_type; break;
  case '/': func = "div"; tc0 = tc1 = is_numeric_type; break;
  case '%': func = "mod"; tc0 = tc1 = is_integral_type; break;
  case '|': func = "or"; tc0 = tc1 = is_boolean_type; break;
  case '&': func = "and"; tc0 = tc1 = is_boolean_type; break;
  case '^': func = "xor"; tc0 = tc1 = is_boolean_type; break;
  case TOK_EQ: func = "eq"; tc0 = tc1 = is_equality_type; break;
  case TOK_NE: func = "ne"; tc0 = tc1 = is_equality_type; break;
  case '<': func = "lt"; tc0 = tc1 = is_ordered_type; break;
  case '>': func = "gt"; tc0 = tc1 = is_ordered_type; break;
  case TOK_LE: func = "le"; tc0 = tc1 = is_ordered_type; break;
  case TOK_GE: func = "ge"; tc0 = tc1 = is_ordered_type; break;
  case TOK_SHIFTL: func = "shiftl"; tc0 = tc1 = is_integral_type; break;
  case TOK_SHIFTR: func = "shiftr"; tc0 = tc1 = is_integral_type; break;
  /* assignment op */
  case TOK_ADD_ASSIGN: func = "adda"; tc0 = tc1 = is_numeric_type; break;
  case TOK_SUB_ASSIGN: func = "suba"; tc0 = tc1 = is_numeric_type; break;
  case TOK_MUL_ASSIGN: func = "mula"; tc0 = tc1 = is_numeric_type; break;
  case TOK_DIV_ASSIGN: func = "diva"; tc0 = tc1 = is_numeric_type; break;
  case TOK_MOD_ASSIGN: func = "moda"; tc0 = tc1 = is_integral_type; break;
  case TOK_OR_ASSIGN: func = "ora"; tc0 = tc1 = is_boolean_type; break;
  case TOK_AND_ASSIGN: func = "anda"; tc0 = tc1 = is_boolean_type; break;
  case TOK_XOR_ASSIGN: func = "xora"; tc0 = tc1 = is_boolean_type; break;
  case TOK_SHIFTL_ASSIGN: func = "shiftla"; tc0 = tc1 = is_boolean_type; break;
  case TOK_SHIFTR_ASSIGN: func = "shiftra"; tc0 = tc1 = is_boolean_type; break;
  /* unary op wo side effect */
  case TOK_PLUS: func = "plus"; tc0 = is_numeric_type; break;
  case TOK_MINUS: func = "minus"; tc0 = is_numeric_type; break;
  case '~': func = "neg"; tc0 = is_boolean_type; break;
  case '!': func = "not"; tc0 = is_boolean_type; break;
  default: break;
  }
  if (func == 0) {
    return;
  }
  bool tc0f = true;
  bool tc1f = true;
  if (tc0 != 0) {
    fn_check_type(eop->arg0, eop->symtbl_lexical);
    tc0f = tc0(eop->arg0->resolve_texpr());
  }
  if (tc1 != 0) {
    fn_check_type(eop->arg1, eop->symtbl_lexical);
    tc1f = tc1(eop->arg1->resolve_texpr());
  }
  if (tc0f && tc1f) {
    return; /* builtin op */
  }
  expr_i *fc = expr_funccall_new(eop->fname, eop->line,
    expr_symbol_new(eop->fname, eop->line,
      expr_nssym_new(eop->fname, eop->line,
	expr_nssym_new(eop->fname, eop->line, 0, "operator"), func)),
    expr_op_new(eop->fname, eop->line, ',', eop->arg0, eop->arg1));
  eop->arg0 = fc;
  eop->arg1 = 0;
  eop->op = '(';
  fn_set_tree_and_define_static(fc, eop, eop->symtbl_lexical, 0, 
    eop->symtbl_lexical->block_backref->tinfo.template_descent);
}

static void subst_user_defined_fldop(expr_op *eop)
{
  if (eop->op == '.' && eop->arg1->get_esort() == expr_e_symbol &&
    eop->parent_expr != 0 &&
    eop->parent_expr->get_esort() != expr_e_funccall &&
    (eop->parent_expr->get_esort() != expr_e_op ||
      ptr_down_cast<expr_op>(eop->parent_expr)->op != '=' ||
      ptr_down_cast<expr_op>(eop->parent_expr)->arg0 != eop))
  {
    fn_check_type(eop->arg0, eop->symtbl_lexical);
    term ta0 = eop->arg0->resolve_texpr();
    expr_i *const einst0 = term_get_instance(ta0);
    expr_block *const tblk = einst0->get_template_block();
    if (tblk != 0) {
      bool is_global = false;
      bool is_upvalue = false;
      bool is_memfld = false;
      expr_i *hf = tblk->symtbl.resolve_name_nothrow("__has_fldop__", true,
	"", is_global, is_upvalue, is_memfld);
      if (hf != 0 && !hf->generated_flag) {
	DBG_DYNFLD(fprintf(stderr, "hasfldop!!! %s\n", eop->dump(0).c_str()));
	expr_symbol *sym = ptr_down_cast<expr_symbol>(eop->arg1);
	expr_i *fc = expr_funccall_new(eop->fname, eop->line,
	  expr_symbol_new(eop->fname, eop->line,
	    expr_nssym_new(eop->fname, eop->line,
	      expr_nssym_new(eop->fname, eop->line, 0, "operator"), "getfld")),
	  expr_op_new(eop->fname, eop->line, ',', eop->arg0,
	    expr_str_literal_new(eop->fname, eop->line,
	      arena_strdup(escape_c_str_literal(sym->nssym->sym).c_str()))));
	eop->arg0 = fc;
	eop->arg1 = 0;
	eop->op = '(';
	fn_set_tree_and_define_static(fc, eop, eop->symtbl_lexical, 0, 
	  eop->symtbl_lexical->block_backref->tinfo.template_descent);
	DBG_DYNFLD(fprintf(stderr, "hasfldop!!! => %s\n",
	  eop->dump(0).c_str()));
      }
    }
  }
  if (eop->op == '=') {
    expr_op *lhsop = dynamic_cast<expr_op *>(eop->arg0);
    if (lhsop != 0 && lhsop->op == '.' &&
      lhsop->arg1->get_esort() == expr_e_symbol)
    {
      fn_check_type(eop->arg0, eop->symtbl_lexical);
      term ta0 = lhsop->arg0->resolve_texpr();
      expr_i *const einst0 = term_get_instance(ta0);
      expr_block *const tblk = einst0->get_template_block();
      if (tblk != 0) {
	bool is_global = false;
	bool is_upvalue = false;
	bool is_memfld = false;
	expr_i *hf = tblk->symtbl.resolve_name_nothrow("__has_fldop__", true,
	  "", is_global, is_upvalue, is_memfld);
	if (hf != 0 && !hf->generated_flag) {
	  DBG_DYNFLD(fprintf(stderr, "hasfldop!!! set %s\n",
	    eop->dump(0).c_str()));
	  expr_symbol *sym = ptr_down_cast<expr_symbol>(lhsop->arg1);
	  expr_i *fc = expr_funccall_new(eop->fname, eop->line,
	    expr_symbol_new(eop->fname, eop->line,
	      expr_nssym_new(eop->fname, eop->line,
		expr_nssym_new(eop->fname, eop->line, 0, "operator"),
		  "setfld")),
	    expr_op_new(eop->fname, eop->line, ',',
	      expr_op_new(eop->fname, eop->line, ',',
		lhsop->arg0,
		expr_str_literal_new(eop->fname, eop->line,
		  arena_strdup(
		    escape_c_str_literal(sym->nssym->sym).c_str()))),
	      eop->arg1));
	  eop->arg0 = fc;
	  eop->arg1 = 0;
	  eop->op = '(';
	  fn_set_tree_and_define_static(fc, eop, eop->symtbl_lexical, 0, 
	    eop->symtbl_lexical->block_backref->tinfo.template_descent);
	  DBG_DYNFLD(fprintf(stderr, "hasfldop!!! set => %s\n",
	    eop->dump(0).c_str()));
	}
      }
    }
  }
}

static void subst_user_defined_elemop(expr_op *eop)
{
  if (eop->op == '[') {
    fn_check_type(eop->arg0, eop->symtbl_lexical);
    const term& ta0 = eop->arg0->resolve_texpr();
    if (!is_container_family(ta0) && !is_cm_range_family(ta0)) {
      expr_i *fc = expr_funccall_new(eop->fname, eop->line,
	expr_symbol_new(eop->fname, eop->line,
	  expr_nssym_new(eop->fname, eop->line,
	    expr_nssym_new(eop->fname, eop->line, 0, "operator"), "getelem")),
	expr_op_new(eop->fname, eop->line, ',', eop->arg0, eop->arg1));
      eop->arg0 = fc;
      eop->arg1 = 0;
      eop->op = '(';
      fn_set_tree_and_define_static(fc, eop, eop->symtbl_lexical, 0, 
	eop->symtbl_lexical->block_backref->tinfo.template_descent);
    }
  }
  if (eop->op == '=') {
    expr_op *lhsop = dynamic_cast<expr_op *>(eop->arg0);
    if (lhsop != 0 && lhsop->op == '[') {
      fn_check_type(lhsop->arg0, lhsop->symtbl_lexical);
      const term& ta0 = lhsop->arg0->resolve_texpr();
      if (!is_container_family(ta0) && !is_cm_range_family(ta0)) {
	expr_i *fc = expr_funccall_new(eop->fname, eop->line,
	  expr_symbol_new(eop->fname, eop->line,
	    expr_nssym_new(eop->fname, eop->line,
	      expr_nssym_new(eop->fname, eop->line, 0, "operator"),
	      "setelem")),
	  expr_op_new(eop->fname, eop->line, ',',
	    expr_op_new(eop->fname, eop->line, ',',
	      lhsop->arg0, lhsop->arg1), eop->arg1));
	eop->arg0 = fc;
	eop->arg1 = 0;
	eop->op = '(';
	fn_set_tree_and_define_static(fc, eop, eop->symtbl_lexical, 0, 
	  eop->symtbl_lexical->block_backref->tinfo.template_descent);
      }
    }
  }
}

struct additional_lookup {
  std::string ns;
  std::string sym_prefix;
};
typedef std::vector<additional_lookup> additional_lookups;

static void append_additional_lookups(expr_i *e, additional_lookups& als,
  bool recursive_flag = true)
{
  expr_e const sort = e->get_esort();
  additional_lookup al;
  if (sort == expr_e_struct) {
    expr_struct *est = ptr_down_cast<expr_struct>(e);
    al.ns = est->uniqns;
    al.sym_prefix = est->sym + std::string("_");
    als.push_back(al);
    term t = e->get_value_texpr();
    if (is_cm_pointer_family(t)) {
      /* if t is a ptr{foo}, find 'foo_funcname' from foo's namespace */
      term t1 = get_pointer_target(t);
      expr_i *const einst1 = term_get_instance(t1);
      append_additional_lookups(einst1, als, true);
    }
  } else if (sort == expr_e_dunion) {
    expr_dunion *eu = ptr_down_cast<expr_dunion>(e);
    al.ns = eu->uniqns;
    al.sym_prefix = eu->sym + std::string("_");
    als.push_back(al);
  } else if (sort == expr_e_interface) {
    expr_interface *ei = ptr_down_cast<expr_interface>(e);
    al.ns = ei->uniqns;
    al.sym_prefix = ei->sym + std::string("_");
    als.push_back(al);
  } else if (sort == expr_e_typedef) {
    expr_typedef *et = ptr_down_cast<expr_typedef>(e);
    al.ns = et->uniqns;
    al.sym_prefix = et->sym + std::string("_");
    als.push_back(al);
  }
  if (recursive_flag) {
    expr_block *const blk = e->get_template_block();
    if (blk != 0) {
      expr_block::inherit_list_type& ih = blk->resolve_inherit_transitive();
      for (expr_block::inherit_list_type::const_iterator i = ih.begin();
	i != ih.end(); ++i) {
	append_additional_lookups(*i, als, false);
      }
    }
  }
}

void expr_op::check_type(symbol_table *lookup)
{
  subst_user_defined_op(this);
  subst_user_defined_elemop(this);
  subst_user_defined_fldop(this);
  if (op == '.' || op == TOK_ARROW) {
    fn_check_type(arg0, lookup);
    term t = arg0->resolve_texpr();
    symbol_common *const sc = arg1->get_sdef();
      /* always a symbol */
    expr_i *const einst = term_get_instance(t);
    if (einst == 0) {
      arena_error_throw(arg0, "Invalid type: '%s'",
	term_tostr_human(t).c_str());
    }
    expr_block *const blk = einst->get_template_block();
    symbol_table *const symtbl = blk != 0 ? &blk->symtbl : 0;
    bool is_global_dummy = false;
    bool is_upvalue_dummy = false;
    bool is_memfld_dummy = false;
    DBG(fprintf(stderr,
      "sym=%s rhs_sym_ns=%s symtbl=%p parent_symtbl=%p\n",
      sc->fullsym.c_str(), sc->uniqns.c_str(), symtbl, parent_symtbl));
    bool no_private = true;
    if (is_ancestor_symtbl(symtbl, symtbl_lexical)) {
      no_private = false; /* allow private field */
    }
    /* find member field or function */
    if (symtbl != 0) {
      symbol symstr = sc->get_fullsym();
      expr_i *fmem = symtbl->resolve_name_nothrow_memfld(symstr, no_private,
	sc->uniqns);
      bool is_generic_invoke = false;
      if (fmem == 0) {
	/* "__invoke" member function */
	symstr = "__invoke"; /* TODO: intern once */
	fmem = symtbl->resolve_name_nothrow_memfld(symstr, no_private,
	  sc->uniqns);
	is_generic_invoke = true;
      }
      if (fmem != 0) {
	/* symbol is defined as a field/mfunc */
	DBG(fprintf(stderr, "found fld '%s' ns=%s\n",
	  symstr.c_str(), sc->uniqns.c_str()));
	if (is_generic_invoke) {
	  sc->set_sym_prefix_fullsym(symstr, is_generic_invoke);
	}
	fn_check_type(arg1, symtbl); /* expr_symbol::check_type */
	type_of_this_expr = arg1->resolve_texpr();
	const expr_i *const fo = sc->get_symdef();
	const expr_funcdef *const fo_efd =
	  dynamic_cast<const expr_funcdef *>(fo);
	if (fo_efd != 0 && !fo_efd->is_virtual_or_member_function()) {
	  arena_error_throw(this, "Can not apply '%s' "
	    "('%s' is not a member function)",
	    tok_string(this, op), sc->get_fullsym().c_str());
	  return;
	}
	assert(!type_of_this_expr.is_null());
	return;
      }
    }
    /* find member-like non-member function (vector_size, map_size etc.) */
    symbol_table *const parent_symtbl =
      symtbl != 0 ? symtbl->get_lexical_parent() : 0;
    if (parent_symtbl != 0) {
      additional_lookups als;
      append_additional_lookups(einst, als);
      for (additional_lookups::const_iterator i = als.begin(); i != als.end();
	++i) {
	/* lookup with arg1_sym_prefix */
	std::string funcname_w_prefix = i->sym_prefix
	  + sc->get_fullsym().to_string(); /* TODO: don't use std::string */
	const std::string uniqns = i->ns;
	DBG(fprintf(stderr,
	  "trying non-member arg0=%s fullsym=%s ns=%s parent_symtbl=%p\n",
	  arg0->dump(0).c_str(), sc->get_fullsym().c_str(), uniqns.c_str(),
	  parent_symtbl));
	no_private = false;
	expr_i *fo = parent_symtbl->resolve_name_nothrow(funcname_w_prefix,
	  no_private, uniqns, is_global_dummy, is_upvalue_dummy,
	  is_memfld_dummy);
	bool is_generic_invoke = false;
	if (fo == 0) {
	  /* "foo__invoke" function */
	  funcname_w_prefix = i->sym_prefix + "__invoke";
	    /* TODO: don't use std::string */
	  fo = parent_symtbl->resolve_name_nothrow(funcname_w_prefix,
	    no_private, uniqns, is_global_dummy, is_upvalue_dummy,
	    is_memfld_dummy);
	  is_generic_invoke = true;
	}
	if (fo != 0) {
	  DBG(fprintf(stderr, "found %s\n", sc->get_fullsym().c_str()));
	  sc->arg_hidden_this = arg0;
	  sc->arg_hidden_this_ns = i->ns;
	  sc->set_sym_prefix_fullsym(funcname_w_prefix, is_generic_invoke);
	  fn_check_type(arg1, parent_symtbl); /* expr_symbol::check_type */
	  type_of_this_expr = arg1->resolve_texpr();
	  assert(!type_of_this_expr.is_null());
	  return;
	}
      }
      /* not found */
      std::string s;
      for (additional_lookups::const_iterator i = als.begin(); i != als.end();
	++i) {
	if (!s.empty()) {
	  s += ", ";
	}
	s += i->sym_prefix + sc->get_fullsym().to_string();
      }
      DBG(fprintf(stderr, "func %s notfound [%s])",
	funcname_w_prefix.c_str(), uniqns.c_str()));
      arena_error_throw(this,
	"Can not apply '%s' "
	"(member function '%s' nor non-member function '%s' not found)",
	tok_string(this, op),
	sc->get_fullsym().c_str(),
	s.c_str());
      return;
    }
    arena_error_throw(this, "Can not apply '%s' (field not found)",
      tok_string(this, op));
    return;
  } else {
    /* other ops */
    fn_check_type(arg1, lookup); /* arg1 first. must for type inference */
    fn_check_type(arg0, lookup);
    /* type_of_this_expr */
  }
  switch (op) {
  case ',':
    type_of_this_expr = arg1 ?  arg1->resolve_texpr() : arg0->resolve_texpr();
    break;
  case TOK_OR_ASSIGN:
  case TOK_AND_ASSIGN:
  case TOK_XOR_ASSIGN:
  case TOK_SHIFTL_ASSIGN:
  case TOK_SHIFTR_ASSIGN:
  case TOK_MOD_ASSIGN:
  case TOK_ADD_ASSIGN:
  case TOK_SUB_ASSIGN:
  case TOK_MUL_ASSIGN:
  case TOK_DIV_ASSIGN:
  case '=':
    /* assignment ops */
    switch (op) {
    case TOK_OR_ASSIGN:
    case TOK_AND_ASSIGN:
    case TOK_XOR_ASSIGN:
    case TOK_SHIFTL_ASSIGN:
    case TOK_SHIFTR_ASSIGN:
      check_boolean_type(this, arg0);
      check_boolean_type(this, arg1);
      break;
    case TOK_MOD_ASSIGN:
      check_integral_expr(this, arg0);
      check_integral_expr(this, arg1);
      break;
    case TOK_ADD_ASSIGN:
    case TOK_SUB_ASSIGN:
    case TOK_MUL_ASSIGN:
    case TOK_DIV_ASSIGN:
      check_numeric_expr(this, arg0);
      check_numeric_expr(this, arg1);
      break;
    case '=':
      break;
    default:
      abort();
    }
    /* assignment ops */
    check_type_convert_to_lhs(this, arg1, arg0->resolve_texpr());
    check_lvalue(this, arg0);
    {
      const bool incl_byref = true;
      if (!is_vardef_constructor_or_byref(this, incl_byref)) {
	check_copyable(this, arg1);
      }
    }
    /* assignment expressions don't have value */
    type_of_this_expr = builtins.type_void;
    break;
  case '?':
    check_bool_expr(this, arg0);
    type_of_this_expr = arg1->resolve_texpr();
    break;
  case ':':
    check_type_convert_to_lhs(this, arg1, arg0->resolve_texpr());
    type_of_this_expr = arg0->resolve_texpr();
    break;
  case TOK_LOR:
  case TOK_LAND:
    check_bool_expr(this, arg0);
    check_type_convert_to_lhs(this, arg1, arg0->resolve_texpr());
    type_of_this_expr = builtins.type_bool;
    break;
  case '|':
  case '^':
  case '&':
    check_boolean_type(this, arg0);
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
    check_dunion_field(this, arg0);
    type_of_this_expr = builtins.type_bool;
    break;
  case '!':
    check_bool_expr(this, arg0);
    type_of_this_expr = builtins.type_bool;
    break;
  case '~':
    check_boolean_type(this, arg0);
    type_of_this_expr = arg0->resolve_texpr();
    break;
  case TOK_PTR_DEREF:
  case '[':
    if (op == TOK_PTR_DEREF && is_cm_pointer_family(arg0->resolve_texpr())) {
      /* pointer types */
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
	#if 0
	if (is_integral_type(idxt) &&
	  is_integral_type(range_begin->resolve_texpr())) {
	  /* need not to convert */
	} else {
	#endif
	  check_type_convert_to_lhs(this, range_begin, idxt);
	#if 0
	}
	#endif
	#if 0
	if (is_integral_type(idxt) &&
	  is_integral_type(range_end->resolve_texpr())) {
	  /* need not to convert */
	} else {
	#endif
	  check_type_convert_to_lhs(this, range_end, idxt);
	#if 0
	}
	#endif
	type_of_this_expr = get_array_range_texpr(this, arg0,
	  arg0->resolve_texpr());
	if (type_of_this_expr.is_null()) {
	  arena_error_throw(this, "Using an array without type parameter");
	}
      } else {
	/* array or map element */
	if (is_map_family(arg0->resolve_texpr())) {
	  if (!is_ifdef_cond_expr(this)) {
	    /* getting map element can cause implicit inserting */
	    check_lvalue(this, arg0);
	    /* operator [] for map requires mapped type to be defcon */
	    term telem = get_array_elem_texpr(this, arg0->resolve_texpr());
	    if (!is_default_constructible(telem)) {
	      arena_error_push(this, "Type '%s' is not default-constructible",
		term_tostr_human(telem).c_str());
	    }
	  } else {
	    /* if (x : m[i]) { ... } */
	    /* m need not to have lvalue */
	  }
	}
	if (op == TOK_PTR_DEREF) {
	  if (!is_cm_range_family(arg0->resolve_texpr())) {
	    arena_error_throw(this, "Can not apply '%s'",
	      tok_string(this, op));
	  }
	#if 0
	} else if (is_integral_type(idxt) &&
	  is_integral_type(arg1->resolve_texpr())) {
	  /* need not to convert */
	#endif
	} else {
	  check_type_convert_to_lhs(this, arg1, idxt);
	}
	type_of_this_expr = get_array_elem_texpr(this, arg0->resolve_texpr());
	if (type_of_this_expr.is_null()) {
	  arena_error_throw(this, "Using an array without type parameter");
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
  default:
    type_of_this_expr = builtins.type_void;
    break;
  }
  /* rooting */
  if (op == TOK_PTR_DEREF && is_cm_pointer_family(arg0->resolve_texpr()) &&
    type_has_refguard(arg0->resolve_texpr())) {
    /* threaded pointer dereference. copy the ptr and lock it */
    passing_root_requirement(passby_e_const_value, this, this, false);
  }
  if (is_assign_incdec_op(op, extop)) {
    /* arg0 is an lvalue */
    DBG_CON(fprintf(stderr, "arg0: %s\n", arg0->dump(0).c_str()));
    if (arg0->get_esort() == expr_e_var) {
      /* var x = ... */
      if (op != '=') {
	arena_error_throw(this, "Invalid expression (invalid assignment)");
      }
      expr_var *const ev = ptr_down_cast<expr_var>(arg0);
      if (ev->varinfo.passby == passby_e_mutable_reference) {
	/* foo mutable & x = ... requires rhs to have lvalue */
	check_lvalue(this, arg1);
      }
      if (is_passby_cm_reference(ev->varinfo.passby) ||
	(is_noheap_type(ev->resolve_texpr()) &&
	  !is_cm_lock_guard_family(ev->resolve_texpr()))) {
	/* require block scope root on rhs because the variable is a noheap
	 * variable and is required to be valid while the block is finished.
	 * this is the only case a block scope tempvar is required. */
	/* note: no need (and should not) to create a copy of a lock guard. */
	DBG_CON(fprintf(stderr, "block scope root rhs %s\n",
	  arg1->dump(0).c_str()));
	add_root_requirement(arg1, ev->varinfo.passby, true);
      } else {
	/* no need to root arg1 because lhs is a variable */
      }
    } else {
      /* asign op, lhs is not a vardef */
      term& a0t = arg0->resolve_texpr();
      if (!symtbl_lexical->pragma.disable_noheap_checking &&
	is_noheap_type(a0t)) {
	/* 'w1 = w2' is not allowed for noheap types, because these
	 * variables may depend different objects */
	arena_error_throw(this,
	  "Invalid assignment ('%s' is a noheap type)",
	  term_tostr_human(arg0->resolve_texpr()).c_str());
      }
      if (!is_assignable_allowing_unsafe(a0t)) {
	/* 'v1 = v2' is not allowed. darray for example. */
	arena_error_throw(this,
	  "Invalid assignment ('%s' is not an assignable type)",
	  term_tostr_human(arg0->resolve_texpr()).c_str());
      }
      if (arg1 != 0) {
	if (expr_is_subexpression(this) ||
	  (arg0 != 0 && expr_can_have_side_effect(arg0))) {
	  /* arg1 can be invalidated by arg0 or other expr */
	  passing_root_requirement_fast(this, arg1, false);
	}
      }
      if (expr_is_subexpression(this) ||
	(arg1 != 0 && expr_can_have_side_effect(arg1))) {
	/* arg0 can be invalidated by arg1 or other expr */
	expr_i *const evo = get_dunion_object_if_vfld(arg0);
	if (evo != 0) {
	  /* lhs is a dunion field. root the dunion object. */
	  add_root_requirement(evo, passby_e_mutable_reference, false);
	} else {
	  /* lhs is not a dunion field. root the lhs. */
	  add_root_requirement(arg0, passby_e_mutable_reference, false);
	}
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
  if (arg0 != 0 && op != '(' && is_void_type(arg0->resolve_texpr())) {
    arena_error_throw(arg0, "Invalid expression (lhs is void)");
  }
  if (arg1 != 0 && is_void_type(arg1->resolve_texpr())) {
    arena_error_throw(arg1, "Invalid expression (rhs is void)");
  }
  assert(!type_of_this_expr.is_null());
}

static term_list tparams_apply_tvmap(expr_i *e, expr_tparams *tp,
  const term_list *partial_targs, const tvmap_type& tvm)
{
  term_list tl;
  if (partial_targs != 0) {
    for (term_list::const_iterator i = partial_targs->begin();
      i != partial_targs->end() && tp != 0; ++i, tp = tp->rest) {
      term iev = eval_term(*i);
      tl.push_back(iev);
    }
  }
  for (; tp != 0; tp = tp->rest) {
    tvmap_type::const_iterator iter = tvm.find(tp->sym);
    if (iter == tvm.end()) {
      arena_error_push(e, "Template parameter '%s' is unbound", tp->sym);
      return term_list();
    }
    tl.push_back(iter->second);
  }
  return tl;
}

static expr_i *create_tpfunc_texpr(expr_i *e, const term_list *partial_targs,
  const tvmap_type& tvm, expr_i *inst_pos)
{
  expr_block *const bl = e->get_template_block();
  assert(bl);
  term_list telist = tparams_apply_tvmap(e, bl->tinfo.tparams, partial_targs,
    tvm);
  return instantiate_template(e, telist, inst_pos);
}

static void set_type_inference_result_for_funccall(expr_funccall *efc,
  expr_i *einst)
{
  expr_i *func = efc->func;
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
    if (!check_term_validity(einst->get_value_texpr(), true, true, true, true,
      efc, err_mess)) {
      arena_error_push(efc, "Incomplete template expression: '%s' %s",
	term_tostr_human(einst->get_value_texpr()).c_str(),
	err_mess.c_str());
    }
    real_func_expr->get_sdef()->set_evaluated(einst->get_value_texpr());
      /* set auto-matching result */
  } else {
    arena_error_throw(efc, "Not a template function");
  }
}

static size_t get_tparam_length(expr_i *e)
{
  expr_block *const ttbl = e != 0 ? e->get_template_block() : 0;
  if (ttbl == 0) {
    return 0;
  }
  return elist_length(ttbl->tinfo.tparams);
}

static void check_passing_memfunc_rec(expr_funccall *fc,
  expr_struct *caller_up_est, const term& te)
{
  // const expr_i *const einst = term_get_instance_const(te);
  /* te can be an incomplete term. dont use term_get_instance(). */
  const expr_i *const ee = te.get_expr();
  if (ee != 0 && ee->get_esort() == expr_e_funcdef) {
    const expr_i *const estv = ptr_down_cast<const expr_funcdef>(ee)
      ->is_virtual_or_member_function();
    if (estv != 0) {
      if (caller_up_est == 0) {
	arena_error_throw(fc, "Passing invalid member function '%s'",
	  term_tostr_human(te).c_str());
      }
      if (estv->get_esort() == expr_e_struct) {
	if (caller_up_est != estv) {
	  arena_error_throw(fc, "Passing invalid member function '%s'",
	    term_tostr_human(te).c_str());
	}
      #if 0
      } else {
	const expr_interface *const ei =
	  ptr_down_cast<const expr_interface>(estv);
	if (!implements_interface(caller_up_est, ei)) {
	  arena_error_throw(fc, "Passing invalid member function");
	}
      #endif
      }
    }
  }
  const term_list *tl = te.get_args_or_metalist();
  if (tl != 0) {
    for (term_list::const_iterator i = tl->begin(); i != tl->end(); ++i) {
      check_passing_memfunc_rec(fc, caller_up_est, *i);
    }
  }
}

static void check_passing_memfunc(expr_funccall *fc, const term& te)
{
  /* issue an error if a foreign member function is passed as a template
   * parameter, which is caused by meta::symbol{t, name} . */
  expr_funcdef *const efd = get_up_member_func(fc->symtbl_lexical);
  expr_struct *est = 0;
  if (efd != 0) {
    est = efd->is_member_function();
  } else {
    est = dynamic_cast<expr_struct *>(
      get_current_frame_expr(fc->symtbl_lexical));
      /* nonzero if fc is inside a constr */
  }
  check_passing_memfunc_rec(fc, est, te);
}

static void check_ptr_constructor_syntax(expr_funccall *fc, const term& te)
{
  expr_i *curfr_expr = get_current_frame_expr(fc->symtbl_lexical);
  if (curfr_expr == 0 || curfr_expr->get_unique_namespace() != "pointer") {
    const std::string s0 = term_tostr_human(te);
    arena_error_push(fc,
      "Can not call a pointer constructor. Use make_ptr family instead.");
  }
  #if 0
  if (fc->parent_expr != 0 && fc->parent_expr->get_esort() == expr_e_op) {
    expr_op *const eop = ptr_down_cast<expr_op>(fc->parent_expr);
    if (eop->op == '=' && eop->arg0->get_esort() == expr_e_var) {
      return; /* ok */
    }
  }
  #if 0
  /* replace 'ptr(foo(...)) with box{foo}(...) */
  symbol_table *symtbl = &global_block->symtbl;
  bool is_global = false, is_upvalue = false;
  expr_i *const rsym = symtbl->resolve_name("pointer::box::box", "", fc,
    is_global, is_upvalue);
  term_list targs;
  targs.push_back(func_te);
  ;;
  // FIXME: here
  #endif
  const term& tgt_type = te.get_args()->front();
  if (!is_copyable(tgt_type)) {
    const std::string s0 = term_tostr_human(tgt_type);
    arena_error_push(fc, "Pointer taget '%s' must be copyable "
      "because of an implementation restriction. Use box{%s}(...) instead.",
      s0.c_str(), s0.c_str());
  }
  #if 0
  arena_error_push(fc,
    "Pointer construction must be a right-hand-side of a variable assignment");
  #endif
  #endif
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
    arena_error_throw(arg, "Expression '%s' is of type void",
      arg->dump(0).c_str());
  }
  bool memfunc_w_explicit_obj = false;
  symbol_common *func_sc = get_func_symbol_common_for_funccall(func,
    memfunc_w_explicit_obj);
  term func_te = func_sc != 0 ? func_sc->resolve_evaluated() : term();
  if (func_te.is_null()) {
    arena_error_throw(func, "Expression '%s' is not a function",
      func->dump(0).c_str());
  }
  {
    eval_context ectx;
    if (has_unbound_tparam(func_te, ectx)) {
      arena_error_throw(func,
	"Template expression '%s' has an unbound type parameter",
	term_tostr_human(func_te).c_str());
    }
  }
  if (is_vararg_function_or_struct(func_te.get_expr()) &&
    func_te.get_args()->size() < get_tparam_length(func_te.get_expr())) {
    /* type inference for generic vararg function */
    typedef std::list<expr_i *> arglist_type;
    arglist_type arglist;
    append_hidden_this(func, arglist);
    append_comma_sep_list(arg, arglist);
    term_list tis;
    for (arglist_type::const_iterator i = arglist.begin(); i != arglist.end();
      ++i) {
      const bool has_lv = expr_has_lvalue(*i, *i, false);
      term_list ent;
      ent.push_back((*i)->resolve_texpr());
      ent.push_back(term(has_lv));
      tis.push_back(term(ent));
    }
    term tis_ml(tis); /* metalist */
    const size_t tplen = get_tparam_length(func_te.get_expr());
    term_list tas; /* resolved targs */
    if (func_sc != 0 && func_sc->is_generic_invoke()) {
      /* "__invoke" or "foo__invoke" */
      const std::string method_name(func_sc->get_fullsym().to_string());
      tas.push_back(term(method_name));
      tas.push_back(tis_ml);
    } else if (func_te.get_args()->size() + 1 == tplen) {
      /* leading targs are explicitly specified */
      tas = *func_te.get_args();
      tas.push_back(tis_ml);
    } else if (func_te.get_args()->size() == 0 && tplen == 2
      && tis.size() == 1) {
      /* no explicit targ is specified */
      /* make_ptr(...) uses this case when it boxes an already-constructed
       * value */
      tas.push_back(arglist.front()->resolve_texpr());
      tas.push_back(tis_ml);
    } else {
      arena_error_throw(this,
	"Invalid template arguments for variadic template: '%s'",
	  term_tostr_human(func_te).c_str());
    }
    const term t_un(func_te.get_expr(), tas);
    func_te = eval_term(t_un);
    DBG_VARIADIC(fprintf(stderr, "variadic %s -> %s\n",
      term_tostr_human(t_un).c_str(), term_tostr_human(func_te).c_str()));
    set_type_inference_result_for_funccall(this, term_get_instance(func_te));
  }
  if (!memfunc_w_explicit_obj) {
    symbol_common *esym = func->get_sdef();
    if (esym != 0 && esym->get_symdef()->get_esort() == expr_e_tparams) {
      /* func is passed as a template parameter. ok even when func is a
       * foreign member function. */
      /* noop */
    } else {
      check_passing_memfunc(this, func_te);
    }
  }
  const term_list *const func_te_args = func_te.get_args();
  expr_i *const func_p_inst = term_get_instance_if(func_te);
    /* possibly instantiated. not instantiated if it's an incomplete te. */
  /* TODO: fix the copy-paste of passing_root_requirement calls */
  if (is_function(func_te)) {
    /* function call */
    expr_funcdef *efd_p_inst = ptr_down_cast<expr_funcdef>(func_p_inst);
    expr_argdecls *ad = efd_p_inst->block->get_argdecls();
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
      check_convert_type(*j, ad->resolve_texpr(), &tvmap);
      #if 0
	const std::string s0 = term_tostr_human((*j)->resolve_texpr());
	const std::string s1 = term_tostr_human(ad->resolve_texpr());
	arena_error_push(this, "Invalid conversion from %s to %s",
	  s0.c_str(), s1.c_str());
	arena_error_push(this, "  initializing argument %u of '%s'",
	  argcnt, efd_p_inst->sym);
      #endif
      /* check lvalue and root argument expressions */
      passing_root_requirement(ad->passby, this, *j, false);
      ++j;
      ad = ad->get_rest();
      ++argcnt;
    }
    if (func->get_esort() == expr_e_op &&
      efd_p_inst->is_virtual_or_member_function()) {
      /* check lvalue and root the foo of foo.bar(...) */
      expr_i *const thisexpr = ptr_down_cast<expr_op>(func)->arg0;
      passing_root_requirement(
	efd_p_inst->is_const
	  ? passby_e_const_reference : passby_e_mutable_reference,
	this, thisexpr, false);
    }
    if (j != arglist.end()) {
      arena_error_push(this, "Too many argument for '%s'", efd_p_inst->sym);
    } else if (ad != 0) {
      arena_error_push(this, "Too few argument for '%s'", efd_p_inst->sym);
    }
    if (efd_p_inst->is_macro()) {
      arena_error_throw(this,
	"Invalid function call: target function is for 'expand' statement");
    }
    expr_i *einst = 0;
    size_t const efd_num_tparams = elist_length(
      efd_p_inst->block->tinfo.tparams);
    size_t const num_targs = (func_te_args == 0) ? 0 : func_te_args->size();
    /*
    if (efd_p_inst->block->tinfo.has_tparams() &&
      (func_te_args == 0 || func_te_args->empty())) {
    */
    if (efd_num_tparams > num_targs) {
      /* instantiate template function automatically */
      DBG(fprintf(stderr, "func is uninstantiated\n"));
      einst = create_tpfunc_texpr(efd_p_inst, func_te_args, tvmap, this);
      set_type_inference_result_for_funccall(this, einst);
      expr_funcdef *efd_inst = ptr_down_cast<expr_funcdef>(einst);
      type_of_this_expr = efd_inst->get_rettype();
    } else {
      einst = efd_p_inst;
      std::string err_mess;
      if (!check_term_validity(efd_p_inst->get_value_texpr(), true, true, true,
	true, this, err_mess)) {
	arena_error_push(this, "Incomplete template expression: '%s' %s",
	  term_tostr_human(efd_p_inst->get_value_texpr()).c_str(),
	  err_mess.c_str());
      }
      type_of_this_expr = apply_tvmap(efd_p_inst->get_rettype(), tvmap);
    }
    funccall_sort = funccall_e_funccall;
    const bool call_wo_obj = func->get_esort() != expr_e_op;
    expr_funcdef *const caller_memfunc = get_up_member_func(symtbl_lexical);
    if (call_wo_obj && caller_memfunc != 0 && caller_memfunc->is_const) {
      if (efd_p_inst->is_virtual_or_member_function() &&
	!efd_p_inst->is_const) {
	arena_error_throw(this,
	  "Calling a non-const member function from a const member function");
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
	= efd_p_inst->is_virtual_or_member_function();
      if (caller_memfunc_parent == 0 ||
	caller_memfunc_parent->get_esort() != expr_e_struct) {
	#if 0
	/* if caller is not inside a struct context, it's a function which
	 * takes callee as a template parameter. if so, callee should be
	 * called without an object. */
	/* noop */
	#endif
	if (callee_memfunc_parent != 0) {
	  arena_error_throw(this,
	    "Calling a member function from a non-member function");
	}
      } else if (callee_memfunc_parent != 0 &&
	caller_memfunc_parent != callee_memfunc_parent) {
	arena_error_throw(this,
	  "Calling a member function without an object");
      }
      /* calling a member function from a nested function? */
      if (caller_memfunc != 0 &&
	caller_memfunc != get_current_frame_expr(symtbl_lexical) &&
	callee_memfunc_parent != 0) {
	arena_error_throw(this,
	  "Calling a member function from a nested function");
      }
    }
    /* add callee_funcs */
    expr_i *const fr = get_current_frame_expr(symtbl_lexical);
    if (fr != 0 && fr->get_esort() == expr_e_funcdef) {
      expr_funcdef *const efd_inst = ptr_down_cast<expr_funcdef>(einst);
      expr_funcdef *const curfd = ptr_down_cast<expr_funcdef>(fr);
      expr_funcdef::call_entry ce(symtbl_lexical, efd_inst);
      curfd->calls.insert_if(ce);
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
    DBG_DEFCON(fprintf(stderr, "expr=[%s] type_of_this_expr=[%s] defcon\n",
      this->dump(0).c_str(),
      term_tostr(type_of_this_expr, term_tostr_sort_cname).c_str()));
    if (!is_default_constructible(func_te, this, 0)) {
      /* TODO: more user friendly error message */
      arena_error_push(this, "Type '%s' is not default-constructible",
	term_tostr_human(func_te).c_str());
    }
    return;
  }
  if (func_p_inst != 0 && func_p_inst->get_esort() == expr_e_struct) {
    /* struct constructor */
    tvmap_type tvmap;
    expr_struct *est_p_inst = ptr_down_cast<expr_struct>(func_p_inst);
    if (est_p_inst->has_userdefined_constr()) {
      /* user defined constructor */
      expr_argdecls *ad = est_p_inst->block->get_argdecls();
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
	check_convert_type(*j, ad->resolve_texpr(), &tvmap);
	#if 0
	  const std::string s0 = term_tostr_human((*j)->resolve_texpr());
	  const std::string s1 = term_tostr_human(ad->resolve_texpr());
	  arena_error_push(this, "Invalid conversion from %s to %s",
	    s0.c_str(), s1.c_str());
	  arena_error_push(this, "  initializing argument %u of '%s'",
	    argcnt, est_p_inst->sym);
	#endif
	passing_root_requirement(ad->passby, this, *j, false);
	++j;
	ad = ad->get_rest();
	++argcnt;
      }
      if (j != arglist.end()) {
	arena_error_push(this, "Too many argument for '%s'", est_p_inst->sym);
      } else if (ad != 0) {
	arena_error_push(this, "Too few argument for '%s'", est_p_inst->sym);
      }
    } else if (est_p_inst->cnamei.cname != 0) {
      typedef std::list<expr_i *> arglist_type;
      arglist_type arglist;
      append_hidden_this(func, arglist);
      append_comma_sep_list(arg, arglist);
      if (is_cm_pointer_family(func_te)) {
	if (arglist.size() != 1) {
	  arena_error_push(this, "Too many argument for '%s'",
	    est_p_inst->sym);
	}
	expr_i *const j = arglist.front();
	if (is_cm_pointer_family(j->resolve_texpr())) {
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
	    arena_error_push(this, "Pointer target is an interface: %s",
	      term_tostr_human(func_te).c_str());
	  }
	  check_convert_type(j, tg, &tvmap);
	  #if 0
	    const std::string s0 = term_tostr_human(j->resolve_texpr());
	    const std::string s1 = term_tostr_human(tg);
	    arena_error_push(this, "Invalid conversion from %s to %s",
	      s0.c_str(), s1.c_str());
	  #endif
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
	check_ptr_constructor_syntax(this, type_of_this_expr);
	  /* must be of the form var x = ptr(foo(...)) */
	funccall_sort = funccall_e_struct_constructor;
	return;
      } else if (is_numeric_type(func_te)) {
	/* explicit conversion to external numeric type */
	if (arglist.size() < 1) {
	  arena_error_throw(this, "Too few argument for '%s'",
	    est_p_inst->sym);
	} else if (arglist.size() > 1) {
	  arena_error_throw(this, "Too many argument for '%s'",
	    est_p_inst->sym);
	}
	expr_i *const j = arglist.front();
	check_convert_type(j, func_te, &tvmap);
	#if 0
	  const std::string s0 = term_tostr_human(j->resolve_texpr());
	  const std::string s1 = term_tostr_human(func_te);
	  arena_error_push(this, "Invalid conversion from %s to %s",
	    s0.c_str(), s1.c_str());
	#endif
	passing_root_requirement_fast(this, j, false);
	  /* root the arg. */
	type_of_this_expr = func_te;
	funccall_sort = funccall_e_struct_constructor;
      } else {
	arena_error_push(this,
	  "Can not call a constructor for an extern struct '%s'",
	    est_p_inst->sym);
      }
    } else {
      /* plain constructor */
      if (est_p_inst->block->tinfo.is_uninstantiated()) {
	arena_error_throw(this,
	  "Can not call a plain constructor for an uninstantiated struct '%s'",
	  est_p_inst->sym);
      }
      typedef std::list<expr_var *> flds_type;
      flds_type flds;
      est_p_inst->get_fields(flds);
      typedef std::list<expr_i *> arglist_type;
      arglist_type arglist;
      append_comma_sep_list(arg, arglist);
      unsigned int argcnt = 1;
      flds_type::iterator i = flds.begin();
      arglist_type::iterator j = arglist.begin();
      while (i != flds.end() && j != arglist.end()) {
	check_convert_type(*j, (*i)->resolve_texpr(), &tvmap);
	#if 0
	  const std::string s0 = term_tostr_human((*j)->resolve_texpr());
	  const std::string s1 = term_tostr_human((*i)->resolve_texpr());
	  arena_error_push(this, "Invalid conversion from %s to %s",
	    s0.c_str(), s1.c_str());
	  arena_error_push(this, "  initializing argument %u of '%s'",
	    argcnt, est_p_inst->sym);
	#endif
	passing_root_requirement(passby_e_const_reference, this, *j, false);
	++i;
	++j;
	++argcnt;
      }
      if (j != arglist.end()) {
	arena_error_push(this, "Too many argument for '%s'", est_p_inst->sym);
      } else if (i != flds.end() && !arglist.empty()) {
	arena_error_push(this, "Too few argument for '%s'", est_p_inst->sym);
      }
    }
    size_t const est_num_tparams = elist_length(
      est_p_inst->block->tinfo.tparams);
    size_t const num_targs = (func_te_args == 0) ? 0 : func_te_args->size();
    if (est_num_tparams > num_targs) {
    /*
    if (est_p_inst->block->tinfo.has_tparams() &&
      (func_te_args == 0 || func_te_args->empty())) {
    */
      /* instantiate template struct automatically */
      DBG(fprintf(stderr, "type is uninstantiated\n"));
      expr_i *const einst = create_tpfunc_texpr(est_p_inst, func_te_args,
	tvmap, this);
      expr_i *real_func_expr = func;
      if (real_func_expr != 0 && real_func_expr->get_sdef() != 0) {
	real_func_expr->get_sdef()->set_evaluated(einst->get_value_texpr());
	  /* set auto-matching result */
	expr_struct *est_inst = ptr_down_cast<expr_struct>(einst);
	type_of_this_expr = est_inst->get_value_texpr();
      } else {
	arena_error_throw(this, "Not a template type");
      }
    } else {
      type_of_this_expr = apply_tvmap(est_p_inst->value_texpr, tvmap);
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
      arena_error_throw(this, "Too many argument for '%s'",
	term_tostr(convto, term_tostr_sort_strict).c_str());
    } else if (arglist.size() < 1) {
      arena_error_throw(this, "Too few argument for '%s'",
	term_tostr(convto, term_tostr_sort_strict).c_str());
    }
    tvmap_type tvmap;
    const arglist_type::iterator j = arglist.begin();
    check_convert_type(*j, convto, &tvmap);
    #if 0
      const std::string s0 = term_tostr_human((*j)->resolve_texpr());
      const std::string s1 = term_tostr_human(convto);
      arena_error_push(this, "Invalid conversion from %s to %s",
	s0.c_str(), s1.c_str());
      arena_error_push(this, "  initializing argument %u of '%s'",
	1, s1.c_str());
    #endif
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
  arena_error_throw(func, "Not a function: %s", func->dump(0).c_str());
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
      arena_error_push(this, "Invalid return statement");
      return;
    }
    term rettyp = fdef->get_rettype();
    if (rettyp.is_null()) {
      rettyp = builtins.type_void;
    }
    if (arg == 0) {
      if (!is_void_type(fdef->get_rettype())) {
	arena_error_push(this, "Return with no value");
      }
      return;
    }
    #if 0
    if (is_void_type(rettyp)) {
      arena_error_push(this, "'return' with a value");
      return;
    }
    #endif
    tvmap_type tvmap;
    check_convert_type(arg, rettyp, &tvmap);
    #if 0
      const std::string s0 = term_tostr(rettyp,
	term_tostr_sort_humanreadable);
      const std::string s1 = term_tostr(arg->resolve_texpr(),
	term_tostr_sort_humanreadable);
      arena_error_push(this, "Can not convert from '%s' to '%s'",
	s1.c_str(), s0.c_str());
    #endif
  } else if (tok == TOK_THROW) {
    if (is_void_type(arg->resolve_texpr())) {
      arena_error_push(this, "Can not throw a void value");
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
    /* if (var x : ...) */
    term tce;
    if (cond->get_esort() == expr_e_op &&
      ptr_down_cast<expr_op>(cond)->op == '[') {
      /* if (var x : m[key]) */
      tce = ptr_down_cast<expr_op>(cond)->arg0->resolve_texpr();
    } else if (cond->get_esort() == expr_e_op &&
      (ptr_down_cast<expr_op>(cond)->op == '.' ||
	ptr_down_cast<expr_op>(cond)->op == TOK_ARROW)) {
      /* if (var x : u.fld) */
      tce = ptr_down_cast<expr_op>(cond)->arg0->resolve_texpr();
    } else {
      const std::string s0 = term_tostr(cond->resolve_texpr(),
	term_tostr_sort_humanreadable);
      arena_error_push(this, "Map element or union field expected (got: %s)",
	s0.c_str());
      return;
    }
    term telm = cond->resolve_texpr();
    assert(argdecls_length(block1->get_argdecls()) == 1);
    term& ta = block1->argdecls->resolve_texpr();
    if (!is_same_type(telm, ta)) {
      arena_error_push(this, "Invalid type for '%s' (got: %s, expected: %s)",
	block1->get_argdecls()->sym,
	term_tostr_human(ta).c_str(),
	term_tostr_human(telm).c_str());
    }
    passing_root_requirement(block1->get_argdecls()->passby, this, cond, true);
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
    arena_error_push(block->argdecls, "Integral type expected");
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
    arena_error_push(this, "Container type expected (got: %s)", s0.c_str());
    return;
  }
  term tidx = get_array_index_texpr(0, tce);
  term telm = get_array_elem_texpr(0, tce);
  assert(argdecls_length(block->get_argdecls()) == 2);
  term& ta0 = block->get_argdecls()->resolve_texpr();
  term& ta1 = block->get_argdecls()->rest->resolve_texpr();
  if (!is_same_type(tidx, ta0)) {
    arena_error_push(this, "Invalid type for '%s' (got: %s, expected: %s)",
      block->get_argdecls()->sym,
      term_tostr_human(ta0).c_str(),
      term_tostr_human(tidx).c_str());
  }
  if (!is_same_type(telm, ta1)) {
    arena_error_push(this, "Invalid type for '%s' (got: %s, expected: %s)",
      block->get_argdecls()->get_rest()->sym,
      term_tostr_human(ta1).c_str(),
      term_tostr_human(telm).c_str());
  }
  passing_root_requirement(block->get_argdecls()->get_rest()->passby, this,
    ce, true);
}

static expr_i *deep_clone_expr(expr_i *e)
{
  return deep_clone_template(e, 0);
}

static bool is_pxc_alpha(int ch)
{
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}

static bool is_pxc_alnum(int ch)
{
  return (ch >= '0' && ch <= '9') || is_pxc_alpha(ch);
}

static bool is_valid_for_symbol(const std::string& s)
{
  if (s.empty()) {
    return false;
  }
  if (!is_pxc_alpha(s[0])) {
    return false;
  }
  for (size_t i = 0; i < s.size(); ++i) {
    if (!is_pxc_alnum(s[i])) {
      return false;
    }
  }
  return true;
}

static void check_validity_for_symbol(const std::string& s, expr_i *pos)
{
  if (!is_valid_for_symbol(s)) {
    arena_error_throw(pos,
      "Expand: name '%s' is invalid for a symbol", s.c_str());
  }
}

static expr_i *create_nssym_from_string(const std::string& s, expr_i *pos)
{
  std::string name = s;
  expr_i *prefix = 0;
  std::string::size_type p = name.rfind("::");
  if (p != name.npos) {
    std::string pre = name.substr(0, p);
    name = name.substr(p + 2);
    prefix = create_nssym_from_string(pre, pos);
  }
  check_validity_for_symbol(name, pos);
  expr_i *nsy = expr_nssym_new(pos->fname, pos->line, prefix,
    arena_strdup(name.c_str()));
  return nsy;
}

static expr_i *create_symbol_from_string(const std::string& s, expr_i *pos)
{
  #if 0
  expr_i *nsy = expr_nssym_new(pos->fname, pos->line, 0,
    arena_strdup(s.c_str()));
  #endif
  expr_i *nsy = create_nssym_from_string(s, pos);
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

static void set_child_parent(expr_i *parent, int pos, expr_i *child)
{
  DBG_SETCHILD(fprintf(stderr, "scp p=%p pos=%d oc=%p nc=%p\n", parent, pos,
    parent->get_child(pos), child));
  parent->set_child(pos, child);
  if (child != 0) {
    child->parent_expr = parent;
  }
}

static expr_i *subst_symbol_name_rec(expr_i *e, expr_i *parent, int parent_pos,
  const std::string& src, const term& dst, bool to_symbol /* true */)
{
  if (e == 0) {
    return 0;
  }
  DBG_EP(fprintf(stderr, "subst_symbol_name_rec: %s %d\n",
    e->dump(0).c_str(), (int)e->get_esort()));
  expr_symbol *sy = dynamic_cast<expr_symbol *>(e);
  expr_te *te = dynamic_cast<expr_te *>(e);
  expr_funcdef *fd = dynamic_cast<expr_funcdef *>(e);
  expr_var *ev = dynamic_cast<expr_var *>(e);
  expr_enumval *en = dynamic_cast<expr_enumval *>(e);
  expr_struct *es = dynamic_cast<expr_struct *>(e);
  expr_dunion *eu = dynamic_cast<expr_dunion *>(e);
  if (sy != 0) {
    expr_nssym *nsy = sy->nssym;
    assert(nsy != 0);
    if (nsy->prefix == 0 && std::string(nsy->sym) == src) {
      DBG_EP(fprintf(stderr, "found '%s' expr_symbol\n", src.c_str()));
      if (dst.is_long()) {
	expr_i *lit = expr_int_literal_new(nsy->fname, nsy->line,
	  arena_strdup(long_to_string(dst.get_long()).c_str()),
	  dst.get_long() < 0);
	if (parent != 0) {
	  parent->set_child(parent_pos, lit);
	}
	return lit;
      } else if (dst.is_string() && to_symbol) {
	expr_i *sycpy = create_symbol_from_string(dst.get_string(), sy);
	if (parent != 0) {
	  parent->set_child(parent_pos, sycpy);
	}
	return sycpy;
      } else {
	expr_i *lit = expr_str_literal_new(nsy->fname, nsy->line,
	  arena_strdup(escape_c_str_literal(dst.get_string()).c_str()));
	DBG_FLDFE(fprintf(stderr, "lit: %s\n", lit->dump(0).c_str()));
	if (parent != 0) {
	  parent->set_child(parent_pos, lit);
	}
	return lit;
      }
    }
  } else if (te != 0) { // TODO: fix copipe
    expr_nssym *nsy = te->nssym;
    assert(nsy != 0);
    if (nsy->prefix == 0 && std::string(nsy->sym) == src) {
      DBG_EP(fprintf(stderr, "found '%s' expr_te\n", src.c_str()));
      if (dst.is_long()) {
	expr_i *lit = expr_int_literal_new(nsy->fname, nsy->line,
	  arena_strdup(long_to_string(dst.get_long()).c_str()),
	  dst.get_long() < 0);
	if (parent != 0) {
	  parent->set_child(parent_pos, lit);
	}
	return lit;
      #if 0 /* not reached */
      } else if (to_symbol) {
	expr_i *tecpy = create_te_from_string(dst.get_string(), te);
	if (parent != 0) {
	  kparent->set_child(parent_pos, tecpy);
	}
	return tecpy;
      #endif
      } else if (dst.is_string()) {
	expr_i *lit = expr_str_literal_new(nsy->fname, nsy->line,
	  arena_strdup(escape_c_str_literal(dst.get_string()).c_str()));
	DBG_FLDFE(fprintf(stderr, "lit: %s\n", lit->dump(0).c_str()));
	if (parent != 0) {
	  parent->set_child(parent_pos, lit);
	}
	return lit;
      } else if (dst.is_metalist()) {
	expr_te *nte = ptr_down_cast<expr_te>(
	  expr_te_new(te->fname, te->line,
	    expr_nssym_new(te->fname, te->line, 0, arena_strdup("")), 0));
	nte->set_symdef(nte); /* resolves to itself */
	nte->sdef.set_evaluated(dst);
	if (parent != 0) {
	  parent->set_child(parent_pos, nte);
	}
	return nte;
      }
    }
  } else if (fd != 0) {
    if (std::string(fd->sym) == src) {
      check_validity_for_symbol(dst.get_string(), e);
      fd->sym = arena_strdup(dst.get_string().c_str());
    }
  } else if (ev != 0) {
    if (std::string(ev->sym) == src) {
      check_validity_for_symbol(dst.get_string(), e);
      ev->sym = arena_strdup(dst.get_string().c_str());
    }
  } else if (en != 0) {
    if (std::string(en->sym) == src) {
      check_validity_for_symbol(dst.get_string(), e);
      en->sym = arena_strdup(dst.get_string().c_str());
    }
  } else if (es != 0) {
    if (std::string(es->sym) == src) {
      check_validity_for_symbol(dst.get_string(), e);
      es->sym = arena_strdup(dst.get_string().c_str());
    }
  } else if (eu != 0) {
    if (std::string(eu->sym) == src) {
      check_validity_for_symbol(dst.get_string(), e);
      eu->sym = arena_strdup(dst.get_string().c_str());
    }
  }
  int num = e->get_num_children();
  for (int i = 0; i < num; ++i) {
    subst_symbol_name_rec(e->get_child(i), e, i, src, dst, to_symbol);
  }
  return e;
}

// FIXME: remove. unused.
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
	} else if (dst.is_string() && to_symbol) {
	  #if 0
	  expr_i *nsycpy = expr_nssym_new(nsy->fname, nsy->line, 0,
	    arena_strdup(dst.get_string().c_str()));
	  expr_i *sycpy = expr_symbol_new(sy->fname, sy->line, nsycpy);
	  #endif
	  expr_i *sycpy = create_symbol_from_string(dst.get_string(), sy);
	  e->set_child(i, sycpy);
	#if 0
	} else {
	  expr_i *lit = expr_str_literal_new(nsy->fname, nsy->line,
	    arena_strdup(escape_c_str_literal(dst.get_string())));
	  DBG_FLDFE(fprintf(stderr, "lit: %s\n", lit->dump(0).c_str()));
	  e->set_child(i, lit);
	#endif
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
	} else if (dst.is_string() && to_symbol) {
	  #if 0
	  expr_i *nsycpy = expr_nssym_new(nsy->fname, nsy->line, 0,
	    arena_strdup(dst.get_string().c_str()));
	  expr_i *tecpy = expr_te_new(te->fname, te->line, nsycpy, 0);
	  #endif
	  expr_i *tecpy = create_te_from_string(dst.get_string(), te);
	  e->set_child(i, tecpy);
	#if 0
	} else {
	  expr_i *lit = expr_str_literal_new(nsy->fname, nsy->line,
	    arena_strdup(escape_c_str_literal(dst.get_string())));
	  DBG_FLDFE(fprintf(stderr, "lit: %s\n", lit->dump(0).c_str()));
	  e->set_child(i, lit);
	#endif
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
    /* TODO: allow expr_e_var when it is not used as a upvalue */
  case expr_e_enumval:
  // case expr_e_block:
  case expr_e_feach:
  case expr_e_fldfe:
  case expr_e_foldfe:
  case expr_e_argdecls:
  case expr_e_funcdef:
  case expr_e_typedef:
  case expr_e_metafdef:
  case expr_e_struct:
  case expr_e_dunion:
  case expr_e_interface:
  case expr_e_try:
    arena_error_push(e, "Not allowed in a generic foreach block: '%s'",
      e->dump(0).c_str());
    break;
  default:
    break;
  }
  #if 0
  expr_var *const ev = dynamic_cast<expr_var *>(e);
  if (ev != 0) {
    arena_error_push(e, "Variable definition is not allowed here: '%s'",
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

// FIXME: remove. unsed.
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
	arena_error_push(this, "Invalid argument for 'foreach': '%s'",
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
    const expr_dunion *const ev = dynamic_cast<const expr_dunion *>(
      term_get_instance_const(typ));
    if (est != 0) {
      est->get_fields(flds);
    } else if (ev != 0) {
      ev->get_fields(flds);
    } else {
      arena_error_push(this, "Invalid argument for 'foreach': '%s'",
	term_tostr_human(typ).c_str());
      arena_error_push(this, "(not a struct nor an union)");
    }
    for (fields_type::const_iterator i = flds.begin(); i != flds.end(); ++i) {
      syms.push_back((*i)->sym);
    }
  } else {
    arena_error_push(this, "Invalid argument for 'foreach': '%s'",
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
  fn_set_generated_code(cstmts); /* mark block_id_ns = 0 */
  fn_update_tree(this->stmts, this, lookup, uniqns);
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
	ptr_down_cast<expr_symbol>(arg1)->sdef.get_fullsym() == ffe->embedsym) {
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
      ptr_down_cast<expr_symbol>(cur)->sdef.get_fullsym() == ffe->embedsym) {
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
    ptr_down_cast<expr_symbol>(e)->sdef.get_fullsym() == ffe->embedsym) {
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
    arena_error_throw(valueste, "Invalid parameter for 'foreach' : '%s'", 
      term_tostr_human(vtyp).c_str());
  }
  exprlist_type ees;
  for (term_list::const_iterator i = targs->begin(); i != targs->end(); ++i) {
    if (i->is_string() && i->get_string().empty()) {
      arena_error_throw(valueste, "Invalid parameter for 'foreach' : '%s'", 
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
    arena_error_throw(this, "Unsupported fold operator for 'foreach' : '%s'", 
      foldop);
  }
  subst_foldfe(this, stmts, ees, fop);
  fn_set_generated_code(this->stmts); /* mark block_id_ns = 0 */
  fn_update_tree(this->stmts, this, lookup, uniqns);
  fn_check_type(stmts, lookup);
}

static expr_i *chain_exprlist(exprlist_type const& ees, expand_e ex)
{
  if (ex == expand_e_argdecls) {
    expr_i *rest = 0;
    for (exprlist_type::const_reverse_iterator i = ees.rbegin();
      i != ees.rend(); ++i) {
      expr_i *const cur = *i;
      ptr_down_cast<expr_argdecls>(cur)->rest
	= ptr_down_cast<expr_argdecls>(rest);
      rest = cur;
    }
    return rest;
  } else if (ex == expand_e_statement) {
    expr_i *rest = 0;
    for (exprlist_type::const_reverse_iterator i = ees.rbegin();
      i != ees.rend(); ++i) {
      expr_i *const cur = *i;
      rest = expr_stmts_new(cur->fname, cur->line, cur, rest);
    }
    return rest;
  } else if (ex == expand_e_comma) {
    expr_i *e = 0;
    for (exprlist_type::const_iterator i = ees.begin(); i != ees.end(); ++i) {
      expr_i *const cur = *i;
      if (e != 0) {
	e = expr_op_new(cur->fname, cur->line, ',', e, cur);
      } else {
	e = cur;
      }
    }
    return e;
  } else {
    abort();
  }
}

static void assert_valid_tree(expr_i *e)
{
  #ifndef NDEBUG
  if (e == 0) {
    return;
  }
  int n = e->get_num_children();
  for (int i = 0; i < n; ++i) {
    expr_i *c = e->get_child(i);
    if (c == 0) {
      continue;
    }
    assert(c->parent_expr == e);
    assert_valid_tree(c);
  }
  #endif
}

static void assert_is_child(expr_i *e, expr_i *p)
{
  #ifndef NDEBUG
  int n = p->get_num_children();
  for (int i = 0; i < n; ++i) {
    expr_i *c = p->get_child(i);
    if (e == c) {
      return;
    }
  }
  abort();
  #endif
}

static term fill_stmt_selector(const term& te, expr_i *const baseexpr,
  expr_expand *const epnd)
{
  expr_stmts *const stmts = ptr_down_cast<expr_stmts>(baseexpr);
  size_t num_stmts = elist_length(stmts);
  const term_list *const targs = te.get_metalist();
  if (targs == 0) {
    return te;
  }
  term_list tl;
  size_t idx = 0;
  for (term_list::const_iterator i = targs->begin(); i != targs->end();
    ++i, ++idx) {
    const term_list *const ent = i->get_metalist();
    if (ent == 0 || ent->size() == 1) {
      for (size_t j = 0; j < num_stmts; ++j) {
	term nent[3];
	nent[0] = (ent == 0 ? (*i) : (*ent)[0]);
	nent[1] = term(static_cast<long long>(j));
	nent[2] = term(static_cast<long long>(idx));
	tl.push_back(term(term_list_range(nent, 3)));
      }
    } else if (ent->size() == 2) {
      const term_list *const sels = (*ent)[1].get_metalist();
      if (sels != 0) {
	/* selector is a list of ints */
	for (term_list::const_iterator j = sels->begin(); j != sels->end();
	  ++j) {
	  term nent[3];
	  nent[0] = (*ent)[0];
	  nent[1] = *j;
	  nent[2] = term(static_cast<long long>(idx));
	  tl.push_back(term(term_list_range(nent, 3)));
	}
      } else {
	tl.push_back(*i);
      }
    }
  }
  return term(tl);
}

static void check_type_expr_expand(expr_expand *const epnd,
  symbol_table *lookup)
{
  DBG_EXPAND_TIMING(double t0 = gettimeofday_double());
  assert_is_child(epnd, epnd->parent_expr);
  assert_is_child(epnd->parent_expr, epnd->parent_expr->parent_expr);
  term vtyp;
  typedef std::map<std::string, term> expfunc_subst_type;
  expfunc_subst_type expfunc_subst;
  if (epnd->callee != 0) {
    /* expand foo{...} */
    expr_te *const te = ptr_down_cast<expr_te>(epnd->callee);
    expr_i *const sd = te->resolve_symdef(lookup);
    if (sd->get_esort() != expr_e_funcdef) {
      arena_error_throw(te,
	"Invalid parameter for 'expand' : '%s' (function expected)",
	te->nssym->sym);
    }
    expr_funcdef *const efd = ptr_down_cast<expr_funcdef>(sd);
    if (!efd->is_macro()) {
      arena_error_throw(te,
	"Invalid function for 'expand' : '%s' "
	"(function is not for 'expand' statement)",
	te->nssym->sym);
    }
    expr_stmts *const stmts = efd->block->stmts;
    expr_tparams *callee_tps = efd->block->tinfo.tparams;
    expr_telist *tlarg = te->tlarg;
    if (elist_length(tlarg) != elist_length(callee_tps)) {
      arena_error_throw(epnd,
	"Wrong number of template parameters: '%s' (%d expected)",
	te->dump(0).c_str(), static_cast<int>(elist_length(callee_tps)));
    }
    term_list tal;
    while (tlarg != 0) {
      const term t = eval_term(term(tlarg->head));
      const std::string n(callee_tps->sym);
      expfunc_subst[n] = term_litexpr_to_literal(t);
      tlarg = tlarg->rest;
      callee_tps = callee_tps->rest;
    }
    term_list vals;
    {
      long long i = 0;
      for (expr_stmts *s = stmts; s != 0; s = s->rest, ++i) {
	term ent[2];
	ent[0] = term(std::string(""));
	ent[1] = term(i);
	const term val(term_list_range(ent, 2));
	vals.push_back(val);
      }
    }
    vtyp = term(vals);
    epnd->valueste = ptr_down_cast<expr_te>(te); /* dummy */
    epnd->baseexpr = stmts;
    epnd->itersym = 0;
  } else {
    /* expand ( ) */
    fn_check_type(epnd->valueste, lookup);
    vtyp = epnd->valueste->sdef.resolve_evaluated();
    if (epnd->ex == expand_e_statement) {
      /* if statement selector is not specified, fill it. */
      vtyp = fill_stmt_selector(vtyp, epnd->baseexpr, epnd);
    }
  }
  DBG_EXPAND_TIMING(double t01 = gettimeofday_double());
  bool slow_flag = false;
  DBG_EXPAND_TIMING(slow_flag = (t01 - t0 > 0.0003));
  if (slow_flag) {
    DBG_EXPAND_TIMING(fprintf(stderr, "slow expand valueste = %s %d\n",
      epnd->valueste->dump(0).c_str(), (int)epnd->valueste->get_esort()));
  }
  DBG_EXPAND_TIMING(double t02 = gettimeofday_double());
  eval_context ectx;
  if (has_unbound_tparam(vtyp, ectx)) {
    return; /* not instantiated */
  }
  const term_list *const targs = vtyp.get_metalist();
  if (targs == 0) {
    arena_error_throw(epnd->valueste,
      "Invalid parameter for 'expand' : '%s' (metalist expected)",
      term_tostr_human(vtyp).c_str());
  }
  if (epnd->ex != expand_e_argdecls && epnd->baseexpr == 0) {
    arena_error_throw(epnd, "Empty base expression for 'expand'");
  }
  exprlist_type ees;
  long long idx = 0;
  for (term_list::const_iterator i = targs->begin(); i != targs->end();
    ++i, ++idx) {
    const term& te = *i;
    expr_i *se = 0;
    if (epnd->ex == expand_e_argdecls) {
      const term_list *const ent = te.get_metalist();
      if (ent == 0 || ent->size() != 4) {
	arena_error_throw(epnd->valueste,
	  "Invalid parameter for 'expand' : '%s' "
	  "(must be a metalist of size 4)",
	  term_tostr_human(te).c_str());
      }
      const std::string name = (*ent)[0].get_string();
      const term typ = (*ent)[1];
      const long long pass_byref = (*ent)[2].get_long();
      const long long arg_mutable = (*ent)[3].get_long();
      expr_argdecls *ad = ptr_down_cast<expr_argdecls>(
	expr_argdecls_new(epnd->fname, epnd->line, "dummy",
	expr_te_new(epnd->fname, epnd->line,
	  expr_nssym_new(epnd->fname, epnd->line, 0, "dummy"), 0),
	passby_e_const_value, 0));
      /* dirty hack ... */
      ad->sym = arena_strdup(name.c_str());
      ad->type_uneval->type_of_this_expr = typ;
      ad->type_of_this_expr = typ;
      ad->passby = pass_byref
	? (arg_mutable ? passby_e_mutable_reference : passby_e_const_reference)
	: (arg_mutable ? passby_e_mutable_value : passby_e_const_value);
      se = ad;
    } else if (epnd->ex == expand_e_statement) {
      const term_list *const ent = te.get_metalist();
      term symte;
      expr_i *baseexpr_cur = 0;
      if (ent != 0) {
	/* te is of the form {{"foo", 3, ...}, {"bar", 2, ...}} and baseexpr
	 * is of type expr_stmts */
	if (ent->size() >= 2) {
	  symte = (*ent)[0];
	  if ((*ent)[1].is_long()) {
	    const int stnum = (*ent)[1].get_long();
	    if (stnum >= 0 && epnd->baseexpr->get_esort() == expr_e_stmts) {
	      DBG_EP(fprintf(stderr, "metalist 2 long stmts ent = %p\n", ent));
	      expr_stmts *cur = ptr_down_cast<expr_stmts>(epnd->baseexpr);
	      for (int i = 0; i < stnum && cur != 0; ++i, cur = cur->rest) { }
	      if (cur == 0) {
		arena_error_throw(epnd->valueste,
		  "Expand: statement number %d not found", stnum);
	      }
	      baseexpr_cur = cur->head;
	      DBG_EP(fprintf(stderr, "baseexpr_cur = %p\n", baseexpr_cur));
	    }
	  }
	}
      } else {
	/* te is of the form {"foo", "bar"} and baseexpr is a single stmt */
	symte = te; /* TODO: convert to string */
	if (epnd->baseexpr->get_esort() == expr_e_stmts) {
	  baseexpr_cur = ptr_down_cast<expr_stmts>(epnd->baseexpr)->head;
	} else {
	  baseexpr_cur = epnd->baseexpr;
	}
      }
      se = deep_clone_expr(baseexpr_cur);
      if (epnd->itersym != 0) {
	se = subst_symbol_name_rec(se, 0, 0, epnd->itersym, symte, true);
      }
      if (epnd->idxsym != 0) {
	long long idxv = idx;
	if (ent->size() >= 3) {
	  idxv = (*ent)[2].get_long();
	}
	const term tidx = term(idxv);
	se = subst_symbol_name_rec(se, 0, 0, epnd->idxsym, tidx, true);
      }
      for (expfunc_subst_type::const_iterator i = expfunc_subst.begin();
	i != expfunc_subst.end(); ++i) {
	se = subst_symbol_name_rec(se, 0, 0, i->first, i->second, true);
      }
      DBG_EP(fprintf(stderr, "subst = %s\n", se->dump(0).c_str()));
    } else {
      assert(epnd->ex == expand_e_comma);
      se = deep_clone_expr(epnd->baseexpr);
      if (epnd->itersym != 0) {
	se = subst_symbol_name_rec(se, 0, 0, epnd->itersym, te, true);
      }
      for (expfunc_subst_type::const_iterator i = expfunc_subst.begin();
	i != expfunc_subst.end(); ++i) {
	se = subst_symbol_name_rec(se, 0, 0, i->first, i->second, true);
      }
    }
    if (se == 0) {
      arena_error_throw(epnd->valueste, "Expanded to a null expression");
    }
    ees.push_back(se);
  }
  DBG_EXPAND_TIMING(double t1 = gettimeofday_double());
  epnd->generated_expr = chain_exprlist(ees, epnd->ex);
  DBG_EXPAND_TIMING(double t2 = gettimeofday_double());
  expr_i *gparent = 0;
  int gparent_pos = 0;
  expr_i *rest_expr = 0;
  if (epnd->ex == expand_e_statement) {
    /* expand expr is placed as if it is an element of the list. */
    expr_stmts *pstmts = ptr_down_cast<expr_stmts>(epnd->parent_expr);
    /* rest expression is the parent stmt's rest instead of this->rest */
    rest_expr = pstmts->rest;
    /* generated_expr is created as stmts */
    assert(epnd->generated_expr == 0 ||
      epnd->generated_expr->get_esort() == expr_e_stmts);
    expr_i *p = pstmts->parent_expr;
    int const n = p->get_num_children();
    int i = 0;
    for (i = 0; i < n; ++i) {
      if (p->get_child(i) == pstmts) {
	break;
      }
    }
    if (i == n) {
      arena_error_throw(epnd,
	"Expand stmts: internal error: not a child expr");
    }
    set_child_parent(p, i, epnd->generated_expr);
    gparent_pos = i;
    gparent = p;
  } else {
    if (epnd->ex == expand_e_argdecls) {
      /* expand expr is placed on the 'rest' part of the list. */
      rest_expr = epnd->rest;
    } else {
      assert(ex == expand_e_comma);
      expr_op *p_op = dynamic_cast<expr_op *>(epnd->parent_expr);
      if (p_op != 0 && p_op->op == ',' && p_op->arg1 == epnd) {
	rest_expr = p_op->arg0;
      }
    }
    expr_i *p = epnd->parent_expr;
    int const n = p->get_num_children();
    int i = 0;
    for (i = 0; i < n; ++i) {
      if (p->get_child(i) == epnd) {
	break;
      }
    }
    if (i == n) {
      arena_error_throw(epnd,
	"Expand argdecls: internal error: not a child expr");
    }
    set_child_parent(p, i, epnd->generated_expr);
    gparent = p;
    gparent_pos = i;
  }
  /* re-chain */
  if (epnd->generated_expr != 0) {
    fn_set_generated_code(epnd->generated_expr); /* mark block_id_ns = 0 */
    fn_update_tree(epnd->generated_expr, gparent, lookup, epnd->uniqns);
    assert(lookup != 0);
    const bool is_template_descent
      = lookup->block_backref->tinfo.template_descent;
    fn_set_tree_and_define_static(epnd->generated_expr, gparent, lookup, 0,
      is_template_descent);
    /* chain rest */
    if (rest_expr != 0) {
      if (epnd->ex == expand_e_comma) {
	expr_i *cur = epnd->generated_expr;
	expr_i *cur_parent = gparent;
	int cur_parent_pos = gparent_pos;
	while (true) {
	  expr_op *cur_op = dynamic_cast<expr_op *>(cur);
	  if (cur_op == 0 || cur_op->op != ',') {
	    break;
	  }
	  cur_parent = cur;
	  cur_parent_pos = 0;
	  cur = cur_op->arg0;
	}
	/* cur is the tail (left-most) non-comma expr */
	expr_i *ne = expr_op_new(cur->fname, cur->line, ',', rest_expr, cur);
	rest_expr->parent_expr = ne;
	cur->parent_expr = ne;
	set_child_parent(cur_parent, cur_parent_pos, ne);
	expr_i *ntop = gparent->get_child(gparent_pos); /* combined expr */
	expr_i *gpp = gparent->parent_expr;
	int n = gpp->get_num_children();
	int i = 0;
	for (; i < n; ++i) {
	  if (gpp->get_child(i) == gparent) {
	    break;
	  }
	}
	if (i == n) {
	  arena_error_throw(epnd, "Expand: internal error: not a child expr");
	}
	set_child_parent(gpp, i, ntop);
	assert_valid_tree(gpp);
	fn_check_type(ntop, lookup);
      } else if (epnd->ex == expand_e_statement) {
	expr_stmts *st = ptr_down_cast<expr_stmts>(epnd->generated_expr);
	assert(st != 0);
	while (st->rest != 0) {
	  st = st->rest;
	}
	st->rest = ptr_down_cast<expr_stmts>(rest_expr);
	rest_expr->parent_expr = st;
	DBG_RECHAIN(fprintf(stderr, "RE-CHAIN genrest rest_expr=%p\n",
	  rest_expr));
	assert_valid_tree(epnd->generated_expr);
	DBG_TIMING(fprintf(stderr, "expand ch %s:%d begin\n", epnd->fname,
	  epnd->line));
	DBG_TIMING(double timing_x1 = gettimeofday_double());
	fn_check_type(epnd->generated_expr, lookup);
	DBG_TIMING(double timing_x2 = gettimeofday_double());
	DBG_TIMING(fprintf(stderr, "expand ch %s:%d end %f\n", epnd->fname,
	  epnd->line, timing_x2 - timing_x1));
      } else if (epnd->ex == expand_e_argdecls) {
	expr_argdecls *ad = ptr_down_cast<expr_argdecls>(epnd->generated_expr);
	assert(ad != 0);
	while (ad->rest != 0) {
	  ad = ad->get_rest();
	}
	ad->rest = ptr_down_cast<expr_argdecls>(rest_expr);
	rest_expr->parent_expr = ad;
	assert_valid_tree(epnd->generated_expr);
	fn_check_type(epnd->generated_expr, lookup);
      } else {
	abort();
      }
    } else {
      /* generated_expr != 0 && rest_expr == 0 */
      assert_is_child(epnd->generated_expr, epnd->generated_expr->parent_expr);
      assert_is_child(epnd->generated_expr->parent_expr,
	epnd->generated_expr->parent_expr->parent_expr);
      assert_valid_tree(epnd->generated_expr);
      fn_check_type(epnd->generated_expr, lookup);
    }
  } else {
    /* generated_expr == 0 */
    if (epnd->ex == expand_e_comma) {
      if (rest_expr != 0) {
	expr_i *gpp = gparent->parent_expr;
	int n = gpp->get_num_children();
	int i = 0;
	for (; i < n; ++i) {
	  if (gpp->get_child(i) == gparent) {
	    break;
	  }
	}
	if (i == n) {
	  arena_error_throw(epnd, "Expand: internal error: not a child expr");
	}
	set_child_parent(gpp, i, rest_expr);
	assert_valid_tree(rest_expr);
	fn_check_type(rest_expr, lookup);
	#if 0
	fprintf(stderr, "rest_expr = %s\n", gpp->dump(0).c_str());
	#endif
      } else {
	/* generated_expr == 0 && rest_expr == 0 */
	expr_op *const gp_op = dynamic_cast<expr_op *>(gparent);
	if (gp_op != 0 && gp_op->op == ',' && gparent_pos == 0) {
	  /* 'this' is the tail (left-most) of the comma list */
	  expr_i *gpp = gparent->parent_expr;
	  #if 0
	  fprintf(stderr, "empty empty gpp = %s\n", gpp->dump(0).c_str());
	  #endif
	  int n = gpp->get_num_children();
	  int i = 0;
	  for (; i < n; ++i) {
	    if (gpp->get_child(i) == gparent) {
	      break;
	    }
	  }
	  if (i == n) {
	    arena_error_throw(epnd, "Internal error: replace_child");
	  }
	  set_child_parent(gpp, i, gp_op->arg1);
	} else {
	  set_child_parent(gparent, gparent_pos, 0);
	}
      }
    } else {
      set_child_parent(gparent, gparent_pos, rest_expr);
      if (epnd->ex == expand_e_argdecls) {
	/* need to check rest_expr here because it is a child of this expr */
	assert_valid_tree(rest_expr);
	fn_check_type(rest_expr, lookup);
      } else {
	/* rest_expr is removed from the original parent. we need to call
	 * check_type here. */
	DBG_RECHAIN(fprintf(stderr, "RE-CHAIN emptygen rest_expr=%p\n",
	  rest_expr));
	assert_valid_tree(rest_expr);
	fn_check_type(rest_expr, lookup);
      }
    }
  }
  DBG_EXPAND_TIMING(double t3 = gettimeofday_double());
  DBG_EXPAND_TIMING(fprintf(stderr, "expand x %f %f %f %f %f\n", t01-t0,
    t02-t01, t1-t02, t2-t1, t3-t2));
}

void expr_expand::check_type(symbol_table *lookup)
{
  check_type_expr_expand(this, lookup);
}

void expr_argdecls::define_vars_one(expr_stmts *stmt)
{
  #if 0
  fprintf(stderr, "define %s\n", sym); // FIXME
  #endif
  symtbl_lexical->define_name(sym, uniqns, this, attribute_private, stmt);
}

void expr_argdecls::check_type(symbol_table *lookup)
{
  DBG_TIMING3(double timing_x1 = gettimeofday_double());
  term& typ = resolve_texpr();
  expr_i *const fr = get_current_frame_expr(lookup);
  if (fr != 0) {
    expr_block *const tbl = fr->get_template_block();
    if (!tbl->tinfo.is_uninstantiated()) {
      check_var_type(typ, this, sym, is_passby_cm_reference(passby), true,
	!is_passby_mutable(passby));
    }
  }
  fn_check_type(rest, lookup);
  /* type_of_this_expr */
  DBG_TIMING3(double timing_x2 = gettimeofday_double());
  DBG_TIMING3(fprintf(stderr, "argdecls ch %s:%d end %f\n", this->fname,
    this->line, timing_x2 - timing_x1));
}

void expr_funcdef::check_type(symbol_table *lookup)
{
  eval_cname_info(cnamei, this, sym);
  if (this->is_const && !this->is_virtual_or_member_function()) {
    arena_error_throw(this, "Non-member/virtual function can not be const");
  }
  if ((this->get_attribute() & attribute_multithr) != 0) {
    // FIXME: valuetype and tsvaluetype are invalid also
    arena_error_throw(this, "Invalid attribute for a function");
  }
  if (this->is_virtual_or_member_function() &&
    (this->get_attribute() & attribute_threaded) != 0) {
    arena_error_throw(this,
      "Invalid attribute for a member/virtual function");
  }
  if (!this->is_virtual_or_member_function() &&
    (this->get_attribute() & attribute_threaded) != 0) {
    symbol_table *parent = this->block->symtbl.get_lexical_parent();
    expr_i *fr = get_current_frame_expr(parent);
    if (fr != 0 && fr->get_esort() == expr_e_funcdef) {
      if ((get_expr_threading_attribute(fr) & attribute_threaded) == 0) {
	arena_error_throw(this, "Parent function is not threaded");
      }
    } else if (fr != 0) {
      arena_error_throw(this,
	"Internal error: threaded function in an invalid context");
    }
  }
  if (ext_pxc && !block->tinfo.template_descent) {
    /* external and not template. stmts are not necessary. */
    block->stmts = 0;
  }
  fn_check_type(block, lookup);
}

void expr_typedef::check_type(symbol_table *lookup)
{
  eval_cname_info(cnamei, this, sym);
  fn_check_type(enumvals, lookup);
}

void expr_metafdef::check_type(symbol_table *lookup)
{
  if ((this->get_attribute() & attribute_threaded) != 0) {
    arena_error_throw(this, "Invalid attribute for a macro");
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
	"Calling member function before field initialization");
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
  const bool has_udcon = est->has_userdefined_constr();
  expr_stmts *st = est->block->stmts;
  expr_i *exec_found = 0;
  for (; st != 0; st = st->rest) {
    expr_i *e = st->head;
    if (e->get_esort() == expr_e_expand) {
      abort(); // already extracted
    }
    if (is_vardef_or_vardefset(e)) {
      if (!has_udcon && e->get_esort() != expr_e_var) {
	/* plain struct allows vardef only */
	arena_error_push(e, "Invalid statement (field definition expected)");
      } else if (exec_found != 0) {
	arena_error_push(e, "Invalid statement before field initialization");
      }
      check_udcon_vardef_calling_memfunc(est, e);
    } else if (!is_noexec_expr(e)) {
      if (exec_found == 0) {
	exec_found = e;
      }
      if (!has_udcon) {
	/* plain struct allows vardef only */
	arena_error_push(e, "Invalid statement for a plain struct body");
      }
    }
  }
}

void expr_struct::check_type(symbol_table *lookup)
{
  eval_cname_info(cnamei, this, sym);
  fn_check_type(block, lookup);
  check_inherit_threading(this, this->get_template_block());
  if (!block->tinfo.is_uninstantiated()) {
    check_constr_restrictions(this);
  }
  if (typefamily_str != 0 && typefamily_str[0] != '@'
    && typefamily_str[0] != 0 && typefamily == typefamily_e_none) {
    arena_error_throw(this, "invalid type family: \'%s\'", typefamily_str);
  }
}

void expr_dunion::check_type(symbol_table *lookup)
{
  fn_check_type(block, lookup);
}

void expr_interface::check_type(symbol_table *lookup)
{
  eval_cname_info(cnamei, this, sym);
  fn_check_type(block, lookup);
  check_inherit_threading(this, this->get_template_block());
}

void expr_try::check_type(symbol_table *lookup)
{
  fn_check_type(tblock, lookup);
  fn_check_type(cblock, lookup);
  fn_check_type(rest, lookup);
  /* type_of_this_expr */
}

void fn_check_misc(expr_i *glo, generate_main_e gmain)
{
#if 0
  typedef std::map<std::string, bool> nsthr_map_type;
  nsthr_map_type nsthr_map;
#endif
  expr_block *const gb = ptr_down_cast<expr_block>(glo);
  bool cur_ns_thr = false;
  for (expr_stmts *st = gb->stmts; st != 0; st = st->rest) {
    expr_i *const head = st->head;
    if (head->get_esort() == expr_e_ns) {
      expr_ns *const ens = ptr_down_cast<expr_ns>(head);
      if (!ens->import) {
	cur_ns_thr = ens->thr;
#if 0
fprintf(stderr, "ns %s %d\n", ens->uniq_nsstr.c_str(), int(ens->thr));
#endif
      } else {
	/* import */
	const std::string n = ens->uniq_nsstr;
	nsthrmap_type::iterator i = nsthrmap.find(n);
	if (i == nsthrmap.end()) {
	  #if 0
	  arena_error_push(head, "Internal error: nsthr_map.find %s",
	    n.c_str());
	  #endif
	} else {
	  if (cur_ns_thr && !i->second) {
	    arena_error_push(head, "Importing a non-threaded namespace '%s' "
	      "from a threaded namespace", n.c_str());
	  }
	}
      }
    }
    if (cur_ns_thr && !is_noexec_expr(head)) {
      arena_error_push(head, "Global statement is not allowed in a "
	"threaded namespace");
    }
  }
  /* emit_threaded_dll */
  if (gmain == generate_main_dl) {
    std::map<std::string, std::string>::const_iterator iter
      = cur_profile->find("emit_threaded_dll");
    if (iter != cur_profile->end()) {
      expr_block *const glo_block = ptr_down_cast<expr_block>(glo);
      const std::string etdstr = iter->second;
      bool is_global_dummy = false;
      bool is_upvalue_dummy = false;
      expr_i *const etd = glo_block->symtbl.resolve_name(etdstr, "", glo,
	is_global_dummy, is_upvalue_dummy);
      if (etd->get_esort() != expr_e_struct) {
	arena_error_throw(glo, "Invalid configuration for emit_threaded_dll: "
	  "symbol '%s' is not a struct", etdstr.c_str());
      }
      const term& te = ptr_down_cast<expr_struct>(etd)->get_value_texpr();
      term exp_args = eval_mf_symbol(te, "args", glo);
      term exp_ret = eval_mf_symbol(te, "ret", glo);
      term efname = eval_mf_symbol(te, "name", glo);
      std::string efnstr = meta_term_to_string(efname, false);
      if (efnstr.empty()) {
	arena_error_throw(glo, "Invalid configuration for emit_threaded_dll: "
	  "symbol '%s:name' is not a string",
	  etdstr.c_str());
      }
      std::string emfname = main_namespace + "::" + efnstr;
      is_global_dummy = false;
      is_upvalue_dummy = false;
      expr_i *emf = glo_block->symtbl.resolve_name(emfname, "", glo,
	is_global_dummy, is_upvalue_dummy);
      if (emf->get_esort() != expr_e_funcdef) {
	arena_error_throw(glo, "Invalid configuration for emit_threaded_dll: "
	  "symbol '%s' is not a function", emfname.c_str());
      }
      const term& fte = emf->get_value_texpr();
      term fte_args = eval_mf_args(fte, glo);
      term fte_ret = eval_mf_ret(fte, glo);
      if (exp_args != fte_args) {
	arena_error_throw(glo, "Precondition failed for emit_threaded_dll: "
	  "invalid arguments '%s' ('%s' expected)",
	  term_tostr_human(fte_args).c_str(),
	  term_tostr_human(exp_args).c_str());
      }
      if (exp_ret != fte_ret) {
	arena_error_throw(glo, "Precondition failed for emit_threaded_dll: "
	  "invalid return type '%s' ('%s' expected)",
	  term_tostr_human(fte_ret).c_str(),
	  term_tostr_human(exp_ret).c_str());
      }
      const bool ns_thr = nsthrmap[main_namespace];
      if (!ns_thr) {
	arena_error_throw(glo, "Precondition failed for emit_threaded_dll: "
	  "namespace '%s' is not threaded", main_namespace.c_str());
      }
      emit_threaded_dll_func = term_tostr_cname(fte);
    }
  }
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
      arena_error_throw(e, "Internal error (check_type_validity_common)");
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
	"Invalid type: '%s' (pointer target is not a tsvaluetype)",
	term_tostr_human(t).c_str());
    }
    if (is_imptr && (attr & attribute_valuetype) == 0) {
      arena_error_throw(e,
	"Invalid type: '%s' (pointer target is not a valuetype)",
	term_tostr_human(t).c_str());
    }
    if (is_mtptr && (attr & attribute_multithr) == 0) {
      arena_error_throw(e,
	"Invalid type: '%s' (pointer target is not a multithreaded type)",
	term_tostr_human(t).c_str());
    }
  } else if (is_container_family(t) || is_cm_range_family(t)) {
    if (tl == 0 || tl->empty()) {
      return;
    }
    const term& te = tl->front();
    if (!is_copyable(te)) {
      arena_error_push(e, "Type '%s' is not copyable",
	term_tostr_human(te).c_str());
    }
  }
}

bool expr_is_instantiated(expr_i *e)
{
  if (e == 0) {
    return false;
  }
  if (e->symtbl_lexical == 0) {
    return false;
  }
  if (e->symtbl_lexical->block_backref == 0) {
    return false;
  }
  if (e->symtbl_lexical->block_backref->tinfo.is_uninstantiated()) {
    return false;
  }
  return true;
}

void fn_check_type(expr_i *e, symbol_table *symtbl)
{
  /* note: symtbl is different from symtbl_lexical if e is a field symbol.
   * see expr_op::check_type for details. */
  if (e == 0) {
    return;
  }
  DBG_CT(fprintf(stderr, "fn_check_type begin %p expr=%s\n",
    e, e->dump(0).c_str()));
  e->check_type(symtbl);
  if (expr_is_instantiated(e)) {
    check_type_validity_common(e, e->get_texpr());
  }
  DBG_CT(fprintf(stderr, "fn_check_type end %p expr=%s -> type=%s\n",
    e, e->dump(0).c_str(),
    term_tostr(e->get_texpr(), term_tostr_sort_strict).c_str()));
}

};

