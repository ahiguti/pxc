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
#include "eval.hpp"
#include "evalmeta.hpp"
#include "checktype.hpp"
#include "expr_misc.hpp"
#include "util.hpp"

#define DBG_UP(x)
#define DBG(x)
#define DBG_TMPL(x)
#define DBG_TMPL2(x)
#define DBG_CLONE(x)
#define DBG_CLONE2(x)
#define DBG_CLONE3(x)
#define DBG_SUB(x)
#define DBG_CONV(x)
#define DBG_NEWTE(x)
#define DBG_INST(x)
#define DBG_EVAL(x)
#define DBG_MACRO(x)
#define DBG_COMPILE(x)
#define DBG_TETERM(x)
#define DBG_META(x)
#define DBG_METAARGTYPE(x)
#define DBG_METASYM(x)
#define DBG_METACONCAT(x)
#define DBG_METALOCAL(x)
#define DBG_ATTR(x)
#define DBG_TYPEOF(x)
#define DBG_LAMBDA(x)
#define DBG_EVAL_TRACE(x)

namespace pxc {

static void apply_tvmap_list(const term_list& src, term_list& dst,
  const tvmap_type& tvmap)
{
  for (term_list::const_iterator i = src.begin(); i != src.end(); ++i) {
    dst.push_back(apply_tvmap(*i, tvmap));
  }
}

term apply_tvmap(const term& t, const tvmap_type& tvmap)
{
  const expr_tparams *const et = dynamic_cast<const expr_tparams *>(
    t.get_expr());
  const term_list *const targs = t.get_args();
  if (et != 0 && (targs == 0 || targs->empty())) {
    tvmap_type::const_iterator i = tvmap.find(et->sym);
    if (i != tvmap.end()) {
      return i->second;
    }
  }
  term_list args;
  if (targs != 0) {
    apply_tvmap_list(*targs, args, tvmap);
  }
  return term(t.get_expr(), args);
}

static expr_i *deep_clone_rec(expr_i *e, expr_block *instantiate_root,
  bool set_instantiated_flag)
{
  if (e == 0) {
    return 0;
  }
  expr_i *const ecpy = e->clone();
  //DBG_CLONE2(fprintf(stderr, "clone (%s) %p -> %p\n", e->dump(0).c_str(), e,
  //  ecpy));
  DBG_CLONE2(fprintf(stderr, "clone %p -> %p\n", e, ecpy));
  if (ecpy->get_esort() == expr_e_block) {
    expr_block *const ebcpy = dynamic_cast<expr_block *>(ecpy);
    /* local symbols are defined even when it's a template. we need to
     * clear them. */
    ebcpy->symtbl.clear_symbols();
    if (ebcpy->tinfo.tparams != 0 &&
      dynamic_cast<expr_block *>(e) != instantiate_root) {
      /* found a nested template */
      /* note: metafdef block must be instantiated even if it has a tparam */
      set_instantiated_flag = false;
    }
    ebcpy->tinfo.instantiated = set_instantiated_flag;
  }
  if (ecpy->get_esort() == expr_e_tparams) {
    DBG_CLONE3(fprintf(stderr, "deep_clone src tparams %p paramdef %p\n", e,
      ptr_down_cast<expr_tparams>(e)->param_def.get_expr()));
    DBG_CLONE3(fprintf(stderr, "deep_clone dst tparams %p paramdef %p\n", ecpy,
      ptr_down_cast<expr_tparams>(ecpy)->param_def.get_expr()));
  }
  const int num = ecpy->get_num_children();
  for (int i = 0; i < num; ++i) {
    expr_i *const c = ecpy->get_child(i);
    expr_i *const ccpy = deep_clone_rec(c, instantiate_root,
      set_instantiated_flag);
    ecpy->set_child(i, ccpy);
  }
  return ecpy;
}

expr_i *deep_clone_template(expr_i *e, expr_block *instantiate_root)
{
  return deep_clone_rec(e, instantiate_root, true);
}

static expr_i *instantiate_template_internal(expr_i *tmpl_root,
  term_list& args_move)
{
  expr_block *const tmpl_block = tmpl_root->get_template_block();
  assert(tmpl_root != 0);
  /* check if it's already instantiated */
  const std::string k = term_tostr_list(args_move, term_tostr_sort_strict);
  DBG_TMPL(fprintf(stderr, "instantiate %s k=%s\n",
    tmpl_root->dump(0).c_str(), k.c_str()));
  const template_info::instances_type::const_iterator i =
    tmpl_block->tinfo.instances.find(k);
  if (i != tmpl_block->tinfo.instances.end()) {
    expr_i *const r = i->second->parent_expr;
    DBG_TMPL(fprintf(stderr, "instantiate found %s %p %s\n", k.c_str(), r,
      term_tostr(r->get_value_texpr(), term_tostr_sort_cname).c_str()));
    return r;
  }
  /* deep clone */
  expr_i *const rcpy = deep_clone_template(tmpl_root, tmpl_block);
  DBG_TMPL(fprintf(stderr, "instantiate %s %p => %p\n", k.c_str(), tmpl_root,
    rcpy));
  /* set tmpl <-> inst mapping */
  expr_block *const rcpy_block = rcpy->get_template_block();
  rcpy_block->tinfo.instances_backref = tmpl_block;
  rcpy_block->tinfo.instances.clear();
  rcpy_block->tinfo.self_key = k;
  tmpl_block->tinfo.instances[k] = rcpy_block;
  DBG_TMPL(fprintf(stderr, "instantiate 1\n"));
  assert(tmpl_block->tinfo.instances_backref == 0);
  /* set param_def */
  expr_tparams *tp = rcpy_block->tinfo.tparams;
  term_list::const_iterator argi = args_move.begin();
  for (; tp != 0 && argi != args_move.end(); tp = tp->rest, ++argi) {
    DBG_TMPL(fprintf(stderr, "set param_def '%s' (%p) = %p(%s) (root=%p)\n",
      tp->dump(0).c_str(),
      tp, argi->get_expr(), term_tostr(*argi, term_tostr_sort_cname).c_str(),
      tmpl_root));
    tp->param_def = *argi;
  }
  DBG_TMPL(fprintf(stderr, "instantiate 2\n"));
  /* update value_texpr for the instance */
  const term rt(tmpl_root, args_move);
  rcpy->set_value_texpr(rt);
  /* downgrade threading attribute if necessary */
  attribute_e nattr = attribute_e(rcpy->get_attribute()
    & ~(attribute_threaded | attribute_multithr |
      attribute_valuetype | attribute_tsvaluetype));
  nattr = attribute_e(nattr | get_term_threading_attribute(rt));
  rcpy->set_attribute(nattr);
  DBG_ATTR(fprintf(stderr, "%s: attr %d\n", term_tostr_human(rt).c_str(),
    nattr));
  /* compile instantiated part */
  assert(rcpy->symtbl_lexical != 0);
  const int num = rcpy->get_num_children();
  DBG_COMPILE(fprintf(stderr, "inst compile (%s:%d) k=%s begin\n", rcpy->fname,
    rcpy->line, k.c_str()));
  for (int j = 0; j < num; ++j) {
    expr_i *const c = rcpy->get_child(j);
    DBG_TMPL(fprintf(stderr, "instantiate 3\n"));
    fn_compile(c, rcpy, true);
    DBG_TMPL(fprintf(stderr, "instantiate 4\n"));
  }
  DBG_COMPILE(fprintf(stderr, "inst compile (%s:%d) k=%s end\n", rcpy->fname,
    rcpy->line, k.c_str()));
  DBG_TMPL(fprintf(stderr, "instantiate rcpy=%p rt=%s %s\n", rcpy,
    term_tostr(rt, term_tostr_sort_cname).c_str(),
    term_tostr(rcpy->get_value_texpr(), term_tostr_sort_cname).c_str()
    ));
  /* check the rcpy itself */
  /* tree must be updated before check_inherit_threading because inherit
   * base may use tparam. */
  check_inherit_threading(rcpy, rcpy->get_template_block());
  return rcpy;
}

expr_i *instantiate_template(expr_i *tmpl_root, term_list& args_move,
  expr_i *pos)
{
  arena_error_throw_pushed(); /* it's necessary! */
  try {
    expr_i *const re = instantiate_template_internal(tmpl_root, args_move);
    arena_error_throw_pushed();
    return re;
  } catch (const std::exception& e) {
    term_list tl = args_move;
    term t(tmpl_root, tl);
    const std::string k = term_tostr_human(t);
    std::string m = e.what();
    std::string s = std::string(pos->fname) + ":" + ulong_to_string(pos->line)
      + ": (instantiated from here) instantiating " + k + "\n" + m;
    throw std::runtime_error(s);
  }
}

static term_list eval_term_list_internal(const term_list *tl,
  bool targs_evaluated, eval_context& ectx, expr_i *pos)
{
  if (tl == 0) {
    return term_list();
  }
  if (targs_evaluated) {
    return *tl;
  }
  term_list r;
  for (term_list::const_iterator i = tl->begin(); i != tl->end(); ++i) {
    r.push_back(eval_term_internal(*i, false, ectx, pos));
  }
  return r;
}

static term find_tparam_bind(expr_tparams *tp, const term& tm,
  eval_context& ectx, bool& found)
{
  const term *tpbind = ectx.tpbind;
  while (tpbind != 0) {
    if (!tpbind->is_bind()) {
      break;
    }
    expr_tparams *p = dynamic_cast<expr_tparams *>(tpbind->get_bind_tparam());
    if (p == tp) {
      /* found */
      found = true;
      return *tpbind->get_bind_tpvalue();
    }
    tpbind = tpbind->get_bind_next();
  }
  /* not found */
  ectx.found_unbound_tparam = true;
  return tm;
}

bool has_unbound_tparam(const term& t, eval_context& ectx)
{
  if (is_tpdummy_type(t)) {
    return true;
  }
  expr_tparams *const tp = dynamic_cast<expr_tparams *>(t.get_expr());
  if (tp != 0 && tp->param_def.is_null()) {
    bool found = false;
    find_tparam_bind(tp, term(), ectx, found);
    return !found;
  }
  const term_list *const cargs = t.get_args_or_metalist();
  if (cargs != 0 && has_unbound_tparam(*cargs, ectx)) {
    return true;
  }
  return false;
}

bool has_unbound_tparam(const term_list& tl, eval_context& ectx)
{
  for (term_list::const_iterator i = tl.begin(); i != tl.end(); ++i) {
    if (has_unbound_tparam(*i, ectx)) {
      return true;
    }
  }
  return false;
}

term eval_if_unevaluated(const term& t, bool evaluated_flag,
  eval_context& ectx, expr_i *pos)
{
  if (evaluated_flag) {
    return t;
  } else {
    return eval_term_internal(t, false, ectx, pos);
  }
}


struct depth_error { };

static term metafdef_to_term(expr_metafdef *mf, eval_context& ectx,
  expr_i *pos);

static term te_to_term(expr_i *te, eval_context& ectx, expr_i *pos)
{
  expr_i *texpr = 0;
  term_list targs;
  expr_te *const tete = dynamic_cast<expr_te *>(te);
  expr_metafdef *const temfd = dynamic_cast<expr_metafdef *>(te);
  if (tete != 0) {
    texpr = tete->resolve_symdef(tete->symtbl_lexical);
    for (expr_telist *tl = tete->tlarg; tl != 0; tl = tl->rest) {
      targs.push_back(te_to_term(tl->head, ectx, pos));
    }
    DBG_TETERM(fprintf(stderr, "te_to_term: tete!=0 %s targslen=%d\n",
      tete->dump(0).c_str(), (int)targs.size()));
  #if 1
  } else if (temfd != 0) {
    return metafdef_to_term(temfd, ectx, pos);
  #endif
  } else {
    DBG_TETERM(fprintf(stderr, "te_to_term: tete=0\n"));
    texpr = te;
  }
  term r(texpr, targs);
  DBG_TETERM(fprintf(stderr, "te_to_term: [%s](%s:%d) -> [%s]\n",
    te->dump(0).c_str(), te->fname, te->line,
    term_tostr_human(r).c_str()));
  return r;
}

static term metafdef_to_term(expr_metafdef *mf, eval_context& ectx,
  expr_i *pos)
{
  term rhs_te = te_to_term(mf->get_rhs(), ectx, pos);
  expr_tparams *tpms = mf->get_tparams();
  term r;
  if (tpms != 0) {
    r = term(tpms, rhs_te); /* term_lambda */
  } else {
    r = rhs_te; /* no lambda abstraction */
  }
  DBG_LAMBDA(fprintf(stderr, "metafdef_to_term: %s -> %s\n",
    mf->dump(0).c_str(), term_tostr_human(r).c_str()));
  return r;
}

static term eval_te(expr_i *te, eval_context& ectx, expr_i *pos)
{
  eval_depth_inc di(ectx);
  term nt = te_to_term(te, ectx, pos);
  DBG_EVAL(fprintf(stderr, "EVALI eval_te sort te=%s %s\n",
    te->dump(0).c_str(), term_tostr_human(nt).c_str()));
  return eval_term_internal(nt, false, ectx, pos);
}

struct local_tparams_binding {
  local_tparams_binding(eval_context& ec, expr_tparams *tpms)
    : ec(ec), tpms(tpms), len(0) {
    for (expr_tparams *tp = tpms; tp != 0; tp = tp->rest) {
      ec.saved_bindings.push_back(tp->param_def);
      ++len;
      #if 0
      fprintf(stderr, "save %p %s\n", tp,
	term_tostr_human(tp->param_def).c_str());
      #endif
    }
  }
  ~local_tparams_binding() {
    size_t i = 0;
    size_t blen = ec.saved_bindings.size();
    for (expr_tparams *tp = tpms; tp != 0; tp = tp->rest, ++i) {
      tp->param_def = ec.saved_bindings[blen - len + i];
      #if 0
      fprintf(stderr, "restore %p %s\n", tp,
	term_tostr_human(tp->param_def).c_str());
      #endif
    }
    ec.saved_bindings.resize(blen - len);
  }
private:
  eval_context& ec;
  expr_tparams *tpms;
  size_t len;
private:
  local_tparams_binding(const local_tparams_binding&);
  local_tparams_binding& operator =(const local_tparams_binding&);
};

struct tpbind_guard {
  tpbind_guard(eval_context& ec, term *nt)
    : ec(ec), oldbind(ec.tpbind) {
    ec.tpbind = nt;
  }
  ~tpbind_guard() {
    ec.tpbind = oldbind;
  }
private:
  eval_context& ec;
  term *oldbind;
private:
  tpbind_guard(const tpbind_guard&);
  tpbind_guard& operator =(const tpbind_guard&);
};

static term save_upvalue_bind(term tm, eval_context& ectx, expr_i *pos)
{
  /* save current binds as upvalue binds */
#if 0
fprintf(stderr, "upvalue binds\n");
#endif
  const term *tpbind = ectx.tpbind;
  while (tpbind != 0) {
    if (!tpbind->is_bind()) {
      break;
    }
    expr_tparams *p = dynamic_cast<expr_tparams *>(
      tpbind->get_bind_tparam());
    term v = *tpbind->get_bind_tpvalue();
#if 0
fprintf(stderr, "upvalue bind: %s %s\n", p->sym, term_tostr_human(v).c_str());
#endif
    tm = term(p, v, true, tm); /* term_bind upvalue */
    tpbind = tpbind->get_bind_next();
  }
  return tm;
}

term eval_apply(const term& tm, const term_list& args,
  bool targs_evaluated, eval_context& ectx, expr_i *pos)
{
  /* tm is already evaluated */
  const term_sort s = tm.get_sort();
  if (s == term_sort_lambda || s == term_sort_bind) {
    /* calc nparam */
    size_t nbind = 0;
    const term *cur = &tm;
    /* chain of term_bind */
    expr_i *tp = cur->get_bind_tparam();
    while (tp != 0) {
      if (!*cur->get_bind_is_upvalue()) {
	++nbind;
      }
      cur = cur->get_bind_next();
      tp = cur->get_bind_tparam();
    }
    term ntm = tm;
    size_t args_bindcnt = 0;
    /* chain of term_lambda */
    expr_tparams *ltpms = dynamic_cast<expr_tparams *>(
      cur->get_lambda_tparams());
    while (ltpms != 0) {
      while (ltpms != 0 && nbind > 0) {
	/* this param is already bound */
	ltpms = ltpms->rest;
	--nbind;
      }
      while (ltpms != 0 && args_bindcnt < args.size()) {
	/* bind this param */
	term arg = eval_if_unevaluated(args[args_bindcnt], targs_evaluated,
	  ectx, pos); /* eager evaluation */
	ntm = term(ltpms, arg, false, ntm); /* term_bind */
	ltpms = ltpms->rest;
	++args_bindcnt;
      }
      if (ltpms != 0) {
	/* not enough args supplied yet */
	DBG_EVAL_TRACE(fprintf(stderr, "eval_apply (pushbind) %s . %s => %s\n",
	  term_tostr_human(tm).c_str(), term_tostr_list_human(args).c_str(),
	  term_tostr_human(ntm).c_str()));
#if 0
	ntm = save_upvalue_binds(ntm, ectx, pos);
#endif
#if 0
	/* save current binds as upvalue binds */
fprintf(stderr, "upvalue binds\n");
	const term *tpbind = ectx.tpbind;
	while (tpbind != 0) {
	  if (!tpbind->is_bind()) {
	    break;
	  }
	  expr_tparams *p = dynamic_cast<expr_tparams *>(
	    tpbind->get_bind_tparam());
	  term v = *tpbind->get_bind_tpvalue();
fprintf(stderr, "upvalue bind: %s %s\n", p->sym, term_tostr_human(v).c_str());
	  ntm = term(p, v, true, ntm); /* term_bind upvalue */
	  tpbind = tpbind->get_bind_next();
	}
#endif
	return ntm;
      }
      cur = cur->get_lambda_body();
      ltpms = dynamic_cast<expr_tparams *>(cur->get_lambda_tparams());
    }
    /* enough args are supplied */
    /* cur is not term_lambda nor term_bind */
    term et;
    {
      tpbind_guard g(ectx, &ntm);
      et = eval_term_internal(*cur, false, ectx, pos);
    }
    if (args_bindcnt == args.size()) {
      /* no more args to apply */
      DBG_EVAL_TRACE(fprintf(stderr, "eval_apply (reduce) %s . %s => %s\n",
	term_tostr_human(tm).c_str(), term_tostr_list_human(args).c_str(),
	term_tostr_human(et).c_str()));
      return et;
    }
    term_list remargs(args.begin() + args_bindcnt, args.end());
    term r = eval_apply(et, remargs, targs_evaluated, ectx, pos);
    DBG_EVAL_TRACE(fprintf(stderr, "eval_apply (remarg) %s . %s => %s\n",
      term_tostr_human(tm).c_str(), term_tostr_list_human(args).c_str(),
      term_tostr_human(r).c_str()));
    return r;
  } else if (s == term_sort_expr && tm.get_args()->size() == 0) {
    term_list args_cpy = args;
    term ntm(tm.get_expr(), args_cpy);
    term r = eval_term_internal(ntm, targs_evaluated, ectx, pos);
    DBG_EVAL_TRACE(fprintf(stderr, "eval_apply (expr) %s . %s => %s\n",
      term_tostr_human(tm).c_str(), term_tostr_list_human(args).c_str(),
      term_tostr_human(r).c_str()));
    return r;
  }
  arena_error_throw(pos, "can not apply: %s . %s",
    term_tostr_human(tm).c_str(), term_tostr_list_human(args).c_str());
  return term();
}

static term eval_term_internal2(const term& tm, bool targs_evaluated,
  eval_context& ectx, expr_i *pos)
{
  DBG_INST(fprintf(stderr, "eval_te [%s]\n",
    term_tostr_human(tm).c_str()));
  eval_depth_inc di(ectx);
  expr_i *const texpr = tm.get_expr();
  if (texpr == 0) {
    const term_sort s = tm.get_sort();
    if (s == term_sort_lambda || s == term_sort_bind) {
      return save_upvalue_bind(tm, ectx, pos);
    }
    return tm;
  }
  const term_list *const targs = tm.get_args();
  const size_t tlarg_len = targs != 0 ? targs->size() : 0;
//fprintf(stderr, "ei2 te=%s texpr=%s targlen=%zu sort=%d\n", term_tostr_human(tm).c_str(), texpr->dump(0).c_str(), tlarg_len, (int)texpr->get_esort()); // FIXME
  switch (texpr->get_esort()) {
  case expr_e_te:
    {
      if (tlarg_len > 0) {
	arena_error_throw(pos, "too many template arguments: '%s'",
	  term_tostr(tm, term_tostr_sort_humanreadable).c_str());
      }
      expr_te *const te = ptr_down_cast<expr_te>(texpr);
//fprintf(stderr, "ei2 TE %s\n", te->dump(0).c_str()); // FIXME
      return eval_te(te, ectx, pos);
    }
    break;
  case expr_e_symbol:
    {
      if (tlarg_len > 0) {
	arena_error_throw(pos, "too many template arguments: '%s'",
	  term_tostr(tm, term_tostr_sort_humanreadable).c_str());
      }
      expr_symbol *const esym = ptr_down_cast<expr_symbol>(texpr);
      expr_i *const symdef = esym->sdef.resolve_symdef();
      const term nt(symdef);
      return eval_term_internal(nt, false, ectx, pos);
    }
    break;
  case expr_e_tparams:
    {
      expr_tparams *const tp = ptr_down_cast<expr_tparams>(texpr);
      term tbase;
      if (!tp->param_def.is_null()) {
	DBG_NEWTE(fprintf(stderr, "tparam %p concrete\n", tp));
	/* TODO: need to eval? */
	DBG_EVAL(fprintf(stderr, "EVALI2 sort tparams pdef\n"));
	tbase = tp->param_def;
      } else {
	DBG_EVAL(fprintf(stderr, "EVALI2 sort tparams nopdef (tp=%p sym=%s)\n",
	  tp, tp->sym));
	bool found = false;
	tbase = find_tparam_bind(tp, tm, ectx, found);
	if (!found) {
	  return tm; /* unbound tparam */
	}
      }
      if (tlarg_len > 0) {
	return eval_apply(tbase, *targs, targs_evaluated, ectx, pos);
      } else {
	return tbase;
      }
    }
    break;
  case expr_e_metafdef:
    {
      expr_metafdef *const ta = ptr_down_cast<expr_metafdef>(texpr);
      const term tmf = metafdef_to_term(ta, ectx, pos);
      const term tmfev = eval_term_internal(tmf, false, ectx, pos);
      if (tlarg_len != 0) {
	return eval_apply(tmfev, *targs, targs_evaluated, ectx, pos);
      } else {
	return tmfev;
      }
    }
    break;
  case expr_e_typedef:
    if (targs != 0 && !targs->empty()) {
      arena_error_throw(pos, "too many template arguments");
    }
    DBG_EVAL(fprintf(stderr, "EVALI2 sort typedef\n"));
    return tm;
    break;
  case expr_e_int_literal:
    if (targs != 0 && !targs->empty()) {
      arena_error_throw(pos, "too many template arguments");
    }
    DBG_EVAL(fprintf(stderr, "EVALI2 int literal\n"));
    // FIXME: unsigned?
    return term(ptr_down_cast<expr_int_literal>(texpr)->get_signed());
    break;
  case expr_e_str_literal:
    if (targs != 0 && !targs->empty()) {
      arena_error_throw(pos, "too many template arguments");
    }
    DBG_EVAL(fprintf(stderr, "EVALI2 str literal\n"));
    {
      const std::string s = ptr_down_cast<expr_str_literal>(texpr)->value;
      return term(s);
    }
    break;
  default:
    DBG_EVAL(fprintf(stderr, "EVALI2 others %d\n", (int)texpr->get_esort()));
    break;
  }
  /* builtin template metafunction? */
  expr_struct *const es = dynamic_cast<expr_struct *>(texpr);
  if (es != 0 && es->typefamily_str != 0 &&
    std::string(es->typefamily_str).substr(0, 1) == "@") {
    if (std::string(es->typefamily_str).substr(0, 2) == "@@") {
      return eval_metafunction_lazy(es->typefamily_str, tm, targs_evaluated,
	ectx, pos);
    }
    if (tlarg_len == 0 &&
      std::string(es->typefamily_str).substr(0, 2) != "@0") {
      return tm; /* no argument is supplied yet */
    }
    term_list tlev;
    tlev = eval_term_list_internal(targs, targs_evaluated, ectx, pos);
    if (has_unbound_tparam(tlev, ectx)) {
      DBG_EVAL(fprintf(stderr, "EVALI2 : found ubound tparam\n"));
      return tm; /* incomplete expr */
    }
    return eval_metafunction_strict(es->typefamily_str, tlev, tm, ectx, pos);
  }
  /* type or func without targ (no instantiaton) */
  if (is_type_or_func_esort(texpr->get_esort()) && tlarg_len == 0) {
    return texpr->get_value_texpr();
  }
  /* template instantiation? */
  expr_block *const ttbl = texpr != 0 ? texpr->get_template_block() : 0;
  if (ttbl != 0) {
    const size_t tparams_len = ttbl->tinfo.is_uninstantiated()
      ? elist_length(ttbl->tinfo.tparams) : 0;
    if (tparams_len != 0 && tlarg_len == 0) {
      return tm; /* no argument is supplied yet */
    }
    if (tlarg_len > tparams_len) {
      arena_error_throw(pos, "too many template arguments: '%s'",
	term_tostr(tm, term_tostr_sort_humanreadable).c_str());
    } else if (tlarg_len < tparams_len) {
      return tm; /* not filled yet */ 
#if 0
      arena_error_throw(pos, "too few template arguments: '%s'",
	term_tostr(tm, term_tostr_sort_humanreadable).c_str());
#endif
    }
    if (tparams_len != 0) {
      term_list tlev = eval_term_list_internal(targs, targs_evaluated, ectx,
	pos);
      DBG_INST(fprintf(stderr, "instantiate tm=%s evaluated_args=%s\n",
	term_tostr_human(tm).c_str(),
	term_tostr_list_human(tlev).c_str()));
      if (!has_unbound_tparam(tlev, ectx)) {
	DBG_INST(fprintf(stderr, "instantiate!\n"));
	expr_i *const einst = instantiate_template(texpr, tlev, pos);
	DBG_EVAL(fprintf(stderr, "EVALI2 sort tmpl instantiated %s\n",
	  term_tostr(einst->get_value_texpr(), term_tostr_sort_strict)
	    .c_str()
	  ));
	return einst->get_value_texpr();
      }
      DBG_INST(fprintf(stderr, "not instantiated\n"));
      /* not instantiate yet */
    }
    DBG_EVAL(fprintf(stderr, "EVALI2 sort tmpl not-instantiated\n"));
    return tm;
  }
  return tm;
}

term eval_term_internal(const term& tm, bool targs_evaluated,
  eval_context& ectx, expr_i *pos)
{
  DBG_EVAL(fprintf(stderr, "EVALI %s BEGIN\n",
    term_tostr_human(tm).c_str()));
  try {
    const term r = eval_term_internal2(tm, targs_evaluated, ectx, pos);
    DBG_EVAL_TRACE(fprintf(stderr, "eval_term_internal %s => %s\n",
      term_tostr_human(tm).c_str(),
      term_tostr_human(r).c_str()));
    return r;
#if 0
  } catch (const std::exception& e) {
    std::string s = e.what();
    s += std::string(pos->fname) + ":" + ulong_to_string(pos->line)
      + ": (while evaluating expression: "
      + term_tostr(tm, term_tostr_sort_humanreadable) + ")\n";
    throw std::runtime_error(s);
#endif
  } catch (...) {
    throw;
  }
}

term eval_expr(expr_i *e)
{
  DBG_EVAL(fprintf(stderr, "eval %s\n", e->dump(0).c_str()));
  eval_context ectx;
  const term tm(e);
  term t = eval_term_internal(tm, false, ectx, e);
  return t;
}

term eval_term(const term& t)
{
  eval_context ectx;
  term tr = eval_term_internal(t, false, ectx, t.get_expr());
  return tr;
}

bool term_has_tparam(const term& t)
{
  expr_i *const texpr = t.get_expr();
  const term_list *const targs = t.get_args();
  if (texpr == 0) {
    return false;
  }
  if (texpr->get_esort() == expr_e_tparams) {
    return true;
  }
  if (targs == 0) {
    return false;
  }
  for (term_list::const_iterator i = targs->begin(); i != targs->end(); ++i) {
    if (term_has_tparam(*i)) {
      return true;
    }
  }
  return false;
}

bool term_has_unevaluated_expr(const term& t)
{
  expr_i *const texpr = t.get_expr();
  const term_list *const targs = t.get_args();
  if (texpr == 0) {
    return false;
  }
  if (texpr->get_esort() == expr_e_metafdef) {
    return true;
  }
  if (targs == 0) {
    return false;
  }
  for (term_list::const_iterator i = targs->begin(); i != targs->end(); ++i) {
    if (term_has_unevaluated_expr(*i)) {
      return true;
    }
  }
  return false;
}

static expr_i *term_get_instance_internal(const term& t)
{
  expr_i *const texpr = t.get_expr();
  const term_list *const targs = t.get_args();
  if (texpr == 0 || targs == 0) {
    return texpr;
  }
  expr_block *const bl = texpr->get_template_block();
  if (bl == 0) {
    return texpr;
  }
  if (targs->empty()) {
    return texpr;
  }
  const std::string k = term_tostr_list(*targs, term_tostr_sort_strict);
  const template_info::instances_type::const_iterator i =
    bl->tinfo.instances.find(k);
  if (i == bl->tinfo.instances.end()) {
    arena_error_throw(0, "internal error: instance '%s' not found",
      term_tostr_human(t).c_str());
  }
  expr_i *const r = i->second->parent_expr;
  return r;
}

expr_i *term_get_instance(term& t)
{
  return term_get_instance_internal(t);
}

const expr_i *term_get_instance_const(const term& t)
{
  return term_get_instance_internal(t);
}

}; 

