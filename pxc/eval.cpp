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
#define DBG_METACACHE(x)
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
  if (t.get_expr() == 0) {
    return t;
  }
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

static void downgrade_threading_attribute_rec(expr_i *e, attribute_e mask)
{
  if (e == 0) {
    return;
  }
  expr_e es = e->get_esort();
  if (es == expr_e_funcdef || es == expr_e_struct || es == expr_e_typedef ||
    es == expr_e_dunion || es == expr_e_interface) {
    const attribute_e oattr = e->get_attribute();
    const int dmask = attribute_threaded | attribute_multithr |
      attribute_valuetype | attribute_tsvaluetype;
    const int om = dmask & oattr & mask;
    attribute_e nattr = attribute_e((oattr & ~dmask) | om);
    e->set_attribute(nattr);
  }
  const int num = e->get_num_children();
  for (int i = 0; i < num; ++i) {
    downgrade_threading_attribute_rec(e->get_child(i), mask);
  }
}

static void check_closure_cfuncobj(const term& t, const expr_i *einst)
{
  if (einst->get_esort() != expr_e_funcdef) {
    return;
  }
#if 0
//fprintf(stderr, "check_closure_cfuncobj %s\n", term_tostr_human(t).c_str());
#endif
  const expr_funcdef *const efd_inst =
    ptr_down_cast<const expr_funcdef>(einst);
  if (!efd_inst->is_c_defined()) {
    return;
  }
  const term_list *targs = t.get_args();
  if (targs == 0) {
    return;
  }
  for (term_list::const_iterator i = targs->begin(); i != targs->end(); ++i) {
    expr_i *const c = i->get_expr();
    if (c == 0 || c->get_esort() != expr_e_funcdef) {
      continue;
    }
    term cte = *i;
    expr_funcdef *const efd = ptr_down_cast<expr_funcdef>(
      term_get_instance(cte));
      efd->used_as_cfuncobj = true;
#if 0
fprintf(stderr, "used_as_funcobj %s\n", term_tostr_human(*i).c_str());
#endif
  }
}

static expr_i *instantiate_template_internal(expr_i *tmpl_root,
  const term_list_range& targs)
{
  expr_block *const tmpl_block = tmpl_root->get_template_block();
  assert(tmpl_root != 0);
  /* check if it's already instantiated */
  const std::string k = term_tostr_list(targs, term_tostr_sort_strict);
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
  term_list_range::const_iterator argi = targs.begin();
  for (; tp != 0 && argi != targs.end(); tp = tp->rest, ++argi) {
    DBG_TMPL(fprintf(stderr, "set param_def '%s' (%p) = %p(%s) (root=%p)\n",
      tp->dump(0).c_str(),
      tp, argi->get_expr(), term_tostr(*argi, term_tostr_sort_cname).c_str(),
      tmpl_root));
    tp->param_def = *argi;
  }
  DBG_TMPL(fprintf(stderr, "instantiate 2\n"));
  /* update value_texpr for the instance */
  const term rt(tmpl_root, targs);
  rcpy->set_value_texpr(rt);
  /* downgrade threading attribute if necessary */
  downgrade_threading_attribute_rec(rcpy, get_term_threading_attribute(rt));
  DBG_ATTR(fprintf(stderr, "%s: attr %x\n", term_tostr_human(rt).c_str(),
    get_term_threading_attribute(rt)));
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
  /* closure is used as a c++ function object? */
  check_closure_cfuncobj(rt, rcpy);
  return rcpy;
}

expr_i *instantiate_template(expr_i *tmpl_root, const term_list_range& targs,
  expr_i *pos)
{
  arena_error_throw_pushed(); /* it's necessary! */
  try {
    expr_i *const re = instantiate_template_internal(tmpl_root, targs);
    arena_error_throw_pushed();
    return re;
  } catch (const std::exception& e) {
    const term t(tmpl_root, targs);
    const std::string k = term_tostr_human(t);
    std::string m = e.what();
    std::string s = std::string(pos->fname) + ":" + ulong_to_string(pos->line)
      + ": (instantiated from here) instantiating " + k + "\n" + m;
    throw std::runtime_error(s);
  }
}

static void eval_term_list_internal(const term_list_range& tl,
  bool targs_evaluated, term_list& tlev, eval_context& ectx, expr_i *pos)
{
  for (size_t i = 0; i < tl.size(); ++i) {
    if (targs_evaluated) {
      tlev[i] = tl[i];
    } else {
      tlev[i] = eval_term_internal(tl[i], false, ectx, pos);
    }
  }
}

#if 0
// TODO: remove
struct evaluated_entry {
  evaluated_entry() : uneval_tpb(0) { }
  term_bind *uneval_tpb;
  term evaluated;
};

// TODO: remove
struct evvals_guard {
  evvals_guard(eval_context& ec, evaluated_entry *evvals, size_t evvals_num)
    : ec(ec), old_evvals(ec.evvals), old_evvals_num(ec.evvals_num)
  {
    ec.evvals = evvals;
    ec.evvals_num = evvals_num;
  }
  ~evvals_guard() {
    ec.evvals = old_evvals;
    ec.evvals_num = old_evvals_num;
  }
private:
  eval_context& ec;
  evaluated_entry *const old_evvals;
  size_t old_evvals_num;
  evvals_guard(const evvals_guard&);
  evvals_guard& operator =(const evvals_guard&);
};
#endif

struct tpbind_guard {
  tpbind_guard(eval_context& ec, const term *tpbind)
    : ec(ec), oldbind(ec.tpbind) {
    ec.tpbind = tpbind;
  }
  ~tpbind_guard() {
    ec.tpbind = oldbind;
  }
private:
  eval_context& ec;
  const term *oldbind;
private:
  tpbind_guard(const tpbind_guard&);
  tpbind_guard& operator =(const tpbind_guard&);
};

static term eval_term_with_bind(const term& t, bool targs_evaluated,
  const term& tb, eval_context& ectx, expr_i *pos)
{
  // TODO: remove
  #if 0
  const term *tbp = &tb;
  size_t evvals_len = 0;
  while (true) {
    term_bind *b = tbp->get_term_bind();
    if (b == 0) {
      break;
    }
    ++evvals_len;
    tbp = &b->next;
  }
  evaluated_entry evvals[evvals_len];
  size_t i = 0;
  for (i = 0, tbp = &tb; i < evvals_len; ++i) {
    evaluated_entry& e = evvals[i];
    term_bind *const b = tbp->get_term_bind();
    e.uneval_tpb = b;
    tbp = &b->next;
  }
  evvals_guard bg(ectx, evvals, evvals_len);
  #endif
  tpbind_guard g(ectx, tb.is_null() ? 0 : &tb);
  return eval_term_internal(t, targs_evaluated, ectx, pos);
}

static term get_term_bind_evaluated(const term& t, eval_context& ectx)
{
  term_bind *tb = t.get_term_bind();
  return tb->tpv;
}

static term find_tparam_bind(expr_tparams *tp, eval_context& ectx)
{
  const term *tpbind = ectx.tpbind;
  while (tpbind != 0) {
    if (!tpbind->is_bind()) {
      break;
    }
    expr_tparams *p = tpbind->get_bind_tparam();
    if (p == tp) {
      /* found */
      #if 0
      return *tpbind->get_bind_tpvalue();
      #endif
      return get_term_bind_evaluated(*tpbind, ectx);
    }
    tpbind = tpbind->get_bind_next();
  }
  /* not found */
  return term();
}

bool is_partial_eval_uneval(const term& t, eval_context& ectx)
{
  return ectx.need_partial_eval && has_unbound_tparam(t, ectx);
}

bool is_partial_eval_uneval(const term_list_range& tl, eval_context& ectx)
{
  return ectx.need_partial_eval && has_unbound_tparam(tl, ectx);
}

bool has_unbound_tparam(const term& t, eval_context& ectx)
{
  if (is_tpdummy_type(t)) {
    return true;
  }
  expr_tparams *const tp = dynamic_cast<expr_tparams *>(t.get_expr());
  if (tp != 0 && tp->param_def.is_null()) {
    const term p = find_tparam_bind(tp, ectx);
    if (p.is_null()) {
      return true;
    }
    return has_unbound_tparam(p, ectx);
  }
  const term_list *const cargs = t.get_args_or_metalist();
  if (cargs != 0 && has_unbound_tparam(*cargs, ectx)) {
    return true;
  }
  return false;
}

bool has_unbound_tparam(const term_list_range& tl, eval_context& ectx)
{
  for (term_list_range::const_iterator i = tl.begin(); i != tl.end(); ++i) {
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

static term expr_to_term(expr_i *expr, eval_context& ectx, expr_i *pos)
{
  expr_i *rexpr = 0;
  term_list rargs;
  switch (expr->get_esort()) {
  case expr_e_te:
    {
      expr_te *const tete = ptr_down_cast<expr_te>(expr);
      rexpr = tete->resolve_symdef(tete->symtbl_lexical);
      for (expr_telist *tl = tete->tlarg; tl != 0; tl = tl->rest) {
	rargs.push_back(expr_to_term(tl->head, ectx, pos));
      }
      DBG_TETERM(fprintf(stderr, "expr_to_term: tete!=0 %s rargslen=%d\n",
	tete->dump(0).c_str(), (int)rargs.size()));
    }
    break;
  case expr_e_metafdef:
    return metafdef_to_term(ptr_down_cast<expr_metafdef>(expr), ectx, pos);
    break;
  case expr_e_int_literal:
    return term(ptr_down_cast<expr_int_literal>(expr)->get_value_ll());
    break;
  case expr_e_str_literal:
    {
      const std::string s = ptr_down_cast<expr_str_literal>(expr)->value;
      return term(s);
    }
    break;
  default:
    rexpr = expr;
    break;
  }
  term r(rexpr, rargs);
  DBG_TETERM(fprintf(stderr, "expr_to_term: [%s](%s:%d) -> [%s]\n",
    expr->dump(0).c_str(), expr->fname, expr->line,
    term_tostr_human(r).c_str()));
  return r;
}

static void get_metafdef_upvalues(expr_i *e, std::vector<expr_tparams *>& r)
{
  while (e != 0) {
    switch (e->get_esort()) {
    case expr_e_metafdef:
      {
	expr_metafdef *const mf = ptr_down_cast<expr_metafdef>(e);
	for (expr_tparams *tp = mf->get_tparams(); tp != 0; tp = tp->rest) {
	  r.push_back(tp);
	}
      }
      e = e->parent_expr;
      break;
    case expr_e_telist:
    case expr_e_te:
      e = e->parent_expr;
      break;
    case expr_e_stmts:
      if (e->parent_expr != 0) {
	expr_i *p = e->parent_expr;
	if (p->get_esort() == expr_e_block) {
	  expr_i *pp = p->parent_expr;
	  if (pp->get_esort() == expr_e_metafdef) {
	    e = pp;
	    break;
	  }
	}
      }
      e = 0;
      break;
    default:
      e = 0;
      break;
    }
  }
}

static term metafdef_to_term(expr_metafdef *mf, eval_context& ectx,
  expr_i *pos)
{
  if (!mf->metafdef_term.is_null()) {
    return mf->metafdef_term; /* cached */
  }
  term rhs_te = expr_to_term(mf->get_rhs(), ectx, pos);
  expr_tparams *tpms = mf->get_tparams();
  term r;
  if (tpms != 0) {
    std::vector<expr_tparams *> up;
    get_metafdef_upvalues(mf->parent_expr, up);
    #if 0
    fprintf(stderr, "%s: %zu\n", mf->dump(0).c_str(), up.size());
    #endif
    r = term(up, tpms, rhs_te); /* term_lambda */
  } else {
    r = rhs_te; /* no lambda abstraction */
  }
  DBG_LAMBDA(fprintf(stderr, "metafdef_to_term: %s -> %s\n",
    mf->dump(0).c_str(), term_tostr_human(r).c_str()));
  mf->metafdef_term = r; /* cache */
  return r;
}

struct eval_depth_inc {
  eval_depth_inc(eval_context& ec) : ec(ec) {
    if (recursion_limit != 0 && ++ec.depth > recursion_limit) {
      arena_error_throw(0, "Recursion depth limit is exceeded");
    };
  }
  ~eval_depth_inc() { --ec.depth; }
private:
  eval_context& ec;
  eval_depth_inc(const eval_depth_inc&);
  eval_depth_inc& operator =(const eval_depth_inc&);
};

static term eval_te(expr_i *te, eval_context& ectx, expr_i *pos)
{
  eval_depth_inc di(ectx);
  term nt = expr_to_term(te, ectx, pos);
  DBG_EVAL(fprintf(stderr, "EVALI eval_te sort te=%s %s\n",
    te->dump(0).c_str(), term_tostr_human(nt).c_str()));
  return eval_term_internal(nt, false, ectx, pos);
}

static term save_upvalue_bind(term tm,
  const std::vector<expr_tparams *>& upvalues, eval_context& ectx, expr_i *pos)
{
  /* save current binds as upvalue binds */
  const term *tpbind = ectx.tpbind;
  while (tpbind != 0) {
    if (!tpbind->is_bind()) {
      break;
    }
    expr_tparams *p = tpbind->get_bind_tparam();
    for (size_t i = 0; i < upvalues.size(); ++i) {
      if (upvalues[i] == p) {
	term v = *tpbind->get_bind_tpvalue();
	tm = term(p, v, term(), true, tm); /* term_bind upvalue */
	break;
      }
    }
    /* TODO: use evaluated value? */
    tpbind = tpbind->get_bind_next();
  }
  return tm;
}

static term eval_apply_internal(const term& tm, const term_list_range& args,
  bool targs_evaluated, eval_context& ectx, expr_i *pos)
{
  /* tm is already evaluated */
  const term_sort s = tm.get_sort();
  if (s == term_sort_lambda || s == term_sort_bind) {
    /* calc nparam */
    size_t nbind = 0;
    const term *cur = &tm;
    /* chain of term_bind */
    expr_tparams *tp = cur->get_bind_tparam();
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
    expr_tparams *ltpms = cur->get_lambda_tparams();
    while (ltpms != 0) {
      while (ltpms != 0 && nbind > 0) {
	/* this param is already bound */
	ltpms = ltpms->rest;
	--nbind;
      }
      while (ltpms != 0 && args_bindcnt < args.size()) {
	/* bind this param */
	if (ltpms->is_variadic_metaf) {
	  /* ltpms is a parameter for a variadic metafunction */
	  term_list al;
	  while (args_bindcnt < args.size()) {
	    term arg = eval_if_unevaluated(args[args_bindcnt], targs_evaluated,
	      ectx, pos); /* eager evaluation */
	    al.push_back(arg);
	    ++args_bindcnt;
	  }
	  term vas(al); /* term_metalist */
	  ntm = term(ltpms, vas, term(), false, ntm); /* term_bind */
	  ltpms = 0;
	} else {
	  /* not a variadic metafunction */
	  term arg = eval_if_unevaluated(args[args_bindcnt], targs_evaluated,
	    ectx, pos); /* eager evaluation */
	  ntm = term(ltpms, arg, term(), false, ntm); /* term_bind */
	  ltpms = ltpms->rest;
	  ++args_bindcnt;
	}
      }
      if (ltpms != 0) {
	/* not enough args supplied yet */
	DBG_EVAL_TRACE(fprintf(stderr, "eval_apply (pushbind) %s . %s => %s\n",
	  term_tostr_human(tm).c_str(), term_tostr_list_human(args).c_str(),
	  term_tostr_human(ntm).c_str()));
	return ntm;
      }
      cur = cur->get_lambda_body();
      ltpms = cur->get_lambda_tparams();
    }
    /* enough args are supplied */
    /* cur is not term_lambda nor term_bind */
    term et = eval_term_with_bind(*cur, false, ntm, ectx, pos);
    if (args_bindcnt == args.size()) {
      /* no more args to apply */
      DBG_EVAL_TRACE(fprintf(stderr, "eval_apply (reduce) %s . %s => %s\n",
	term_tostr_human(tm).c_str(), term_tostr_list_human(args).c_str(),
	term_tostr_human(et).c_str()));
      return et;
    }
    term_list_range remargs(args.begin() + args_bindcnt,
      args.size() - args_bindcnt);
    term r = eval_apply(et, remargs, targs_evaluated, ectx, pos);
    DBG_EVAL_TRACE(fprintf(stderr, "eval_apply (remarg) %s . %s => %s\n",
      term_tostr_human(tm).c_str(), term_tostr_list_human(args).c_str(),
      term_tostr_human(r).c_str()));
    return r;
  } else if (s == term_sort_expr && tm.get_args()->size() == 0) {
    term ntm(tm.get_expr(), args);
    term r = eval_term_internal(ntm, targs_evaluated, ectx, pos);
    DBG_EVAL_TRACE(fprintf(stderr, "eval_apply (expr) %s . %s => %s\n",
      term_tostr_human(tm).c_str(), term_tostr_list_human(args).c_str(),
      term_tostr_human(r).c_str()));
    return r;
  }
  arena_error_throw(pos, "Can not apply: %s . %s",
    term_tostr_human(tm).c_str(), term_tostr_list_human(args).c_str());
  return term();
}

term eval_apply(const term& tm, const term_list_range& args,
  bool targs_evaluated, eval_context& ectx, expr_i *pos)
{
  #if 0
  try {
  #endif
    return eval_apply_internal(tm, args, targs_evaluated, ectx, pos);
  #if 0
  } catch (const std::exception& e) {
    std::string s = e.what();
    if (s.size() > 0 && s[s.size() - 1] != '\n') {
      s += "\n";
    }
    s += std::string(pos->fname) + ":" + ulong_to_string(pos->line)
      + ": (while applying "
      + term_tostr_human(tm)
      + " . "
      + term_tostr_human(term(args))
      + ")\n";
    if (ectx.tpbind != 0) {
      s += std::string(pos->fname) + ":" + ulong_to_string(pos->line)
	+ ": (where "
	+ term_tostr_human(*ectx.tpbind)
	+ ")\n";
    }
    throw std::runtime_error(s);
  }
  #endif
}

static term rebuild_term(const term *tptr, expr_i *texpr,
  const term_list_range& targs)
{
  if (tptr != 0) {
    return *tptr;
  }
  return term(texpr, targs);
}

static std::string get_ectx_bind_str(eval_context const& ectx)
{
  if (ectx.tpbind == 0) {
    return "";
  }
  std::string r;
  term_bind *tb = ectx.tpbind->get_term_bind();
  while (tb != 0) {
    if (r.empty()) { r += " where"; }
    r += " ";
    r += std::string(tb->tp->sym) + "=" + term_tostr_human(tb->tpv);
    tb = tb->next.get_term_bind();
  }
  return r;
}

static std::string incomplete_term_to_str(expr_i *texpr,
  const term_list_range& targs1, const term_list_range& targs2)
{
  size_t sz = targs1.size();
  term_list targs(sz);
  for (size_t i = 0; i < sz; ++i) {
    targs[i] = targs2[i].is_null() ? targs1[i] : targs2[i];
  }
  return term_tostr_human(
    term(texpr, term_list_range(sz != 0 ? &targs[0] : 0, sz)));
}

static term eval_apply_expr(expr_i *texpr, const term_list_range& targs,
  bool targs_evaluated, const term *tptr, eval_context& ectx, expr_i *pos)
{
  /* if tptr is supplied, it must be equiv to term(texpr, targs) */
  eval_depth_inc di(ectx);
  switch (texpr->get_esort()) {
  case expr_e_te:
    {
      if (targs.size() > 0) {
	arena_error_throw(pos, "Too many template arguments: '%s'",
	  term_tostr_human(rebuild_term(tptr, texpr, targs)).c_str());
      }
      expr_te *const te = ptr_down_cast<expr_te>(texpr);
      if (te->is_term_literal()) {
	return te->sdef.get_evaluated();
      }
      return eval_te(te, ectx, pos);
    }
    break;
  case expr_e_symbol:
    {
      if (targs.size() > 0) {
	arena_error_throw(pos, "Too many template arguments: '%s'",
	  term_tostr_human(rebuild_term(tptr, texpr, targs)).c_str());
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
	const term p = find_tparam_bind(tp, ectx);
	if (p.is_null()) {
	  return rebuild_term(tptr, texpr, targs); /* unbound tparam */
	}
	tbase = p;
      }
      if (targs.size() > 0) {
	return eval_apply(tbase, targs, targs_evaluated, ectx, pos);
      } else {
	return tbase;
      }
    }
    break;
  case expr_e_metafdef:
    {
      expr_metafdef *const ta = ptr_down_cast<expr_metafdef>(texpr);
      const term tmf = metafdef_to_term(ta, ectx, pos);
      term tmfev;
      if (!ta->evaluated_term.is_null()) {
	tmfev = ta->evaluated_term;
      } else {
	tmfev = eval_term_internal(tmf, false, ectx, pos);
	DBG_METACACHE(fprintf(stderr, "ev %s\n", ta->sym));
	if (ta->has_name() && ta->block->tinfo.tparams == 0) {
	  /* named metafunction without params. cache the evaluated result. */
	  ta->evaluated_term = tmfev;
	}
      }
      if (targs.size() != 0) {
	#if 0
	if (ta->is_variadic) {
	  term_list vargs(targs.size());
	  for (size_t i = 0; i < targs.size(); ++i) {
	    vargs[i] = eval_if_unevaluated(targs[i], targs_evaluated, ectx,
	      pos);
	  }
	  const term larg(vargs); /* metalist */
	  return eval_apply(tmfev, term_list_range(&larg, 1), true, ectx, pos);
	} else {
	#endif
	  return eval_apply(tmfev, targs, targs_evaluated, ectx, pos);
	#if 0
	}
	#endif
      } else {
	return tmfev;
      }
    }
    break;
  case expr_e_typedef:
    if (!targs.empty()) {
      arena_error_throw(pos, "Too many template arguments");
    }
    DBG_EVAL(fprintf(stderr, "EVALI2 sort typedef\n"));
    return rebuild_term(tptr, texpr, targs);
    break;
  #if 0
  case expr_e_int_literal:
    abort();
    if (!targs.empty()) {
      arena_error_throw(pos, "Too many template arguments");
    }
    DBG_EVAL(fprintf(stderr, "EVALI2 int literal\n"));
    // FIXME: unsigned?
    return term(ptr_down_cast<expr_int_literal>(texpr)->get_signed());
    break;
  case expr_e_str_literal:
    abort();
    if (!targs.empty()) {
      arena_error_throw(pos, "Too many template arguments");
    }
    DBG_EVAL(fprintf(stderr, "EVALI2 str literal\n"));
    {
      const std::string s = ptr_down_cast<expr_str_literal>(texpr)->value;
      return term(s);
    }
    break;
  #endif
  default:
    DBG_EVAL(fprintf(stderr, "EVALI2 others %d\n", (int)texpr->get_esort()));
    break;
  }
  /* builtin template metafunction? */
  expr_struct *const es = dynamic_cast<expr_struct *>(texpr);
  if (es != 0 && es->builtin_typestub != 0) {
    return *es->builtin_typestub;
  }
  if (es != 0 && (es->metafunc_strict != 0 || es->metafunc_nonstrict != 0)) {
    if (targs.size() == 0) {
      return rebuild_term(tptr, texpr, targs); /* no argument is supplied yet */
    }
    term r;
    if (es->metafunc_strict != 0) {
      term_list tlev(targs.size());
      term_list_range tlev_range(tlev);
      try {
	eval_term_list_internal(targs, targs_evaluated, tlev, ectx, pos);
	if (is_partial_eval_uneval(tlev_range, ectx)) {
	  DBG_EVAL(fprintf(stderr, "EVALI2 : found ubound tparam\n"));
	  return rebuild_term(tptr, texpr, targs); /* incomplete expr */
	}
	r = (*es->metafunc_strict)(tlev_range, ectx, pos);
      } catch (const std::exception& e) {
	std::string s = e.what();
	if (s.size() > 0 && s[s.size() - 1] != '\n') {
	  s += "\n";
	}
	s += std::string(pos->fname) + ":" + ulong_to_string(pos->line)
	  + ": (while evaluating expression: "
	  + incomplete_term_to_str(texpr, targs, tlev_range)
	  + "";
	s += get_ectx_bind_str(ectx);
	s += ")\n";
	throw std::runtime_error(s);
      }
      if (r.is_null()) {
	arena_error_throw(pos, "Invalid template expression: %s",
	  term_tostr_human(term(texpr, tlev_range)).c_str());
      }
    } else {
      try {
	r = (*es->metafunc_nonstrict)(texpr, targs, targs_evaluated, ectx,
	  pos);
      } catch (const std::exception& e) {
	std::string s = e.what();
	if (s.size() > 0 && s[s.size() - 1] != '\n') {
	  s += "\n";
	}
	s += std::string(pos->fname) + ":" + ulong_to_string(pos->line)
	  + ": (while evaluating expression: "
	  + term_tostr_human(term(texpr, targs))
	  + "";
	s += get_ectx_bind_str(ectx);
	s += ")\n";
	throw std::runtime_error(s);
      }
      if (r.is_null()) {
	arena_error_throw(pos, "Invalid template expression: %s",
	  term_tostr_human(term(texpr, targs)).c_str());
      }
    }
    return r;
  }
  /* type or func without targ (no instantiaton) */
  if (is_type_or_func_esort(texpr->get_esort()) && targs.empty()) {
    return texpr->get_value_texpr();
  }
  /* template instantiation? */
  expr_block *const ttbl = texpr != 0 ? texpr->get_template_block() : 0;
  if (ttbl != 0) {
    const size_t tparams_len = ttbl->tinfo.is_uninstantiated()
      ? elist_length(ttbl->tinfo.tparams) : 0;
    if (tparams_len != 0 && targs.size() == 0) {
      return rebuild_term(tptr, texpr, targs); /* no argument is supplied yet */
    }
    if (targs.size() > tparams_len) {
      arena_error_throw(pos, "Too many template arguments: '%s'",
	term_tostr_human(rebuild_term(tptr, texpr, targs)).c_str());
    } else if (targs.size() < tparams_len) {
      return rebuild_term(tptr, texpr, targs); /* not filled yet */ 
      #if 0
      arena_error_throw(pos, "Too few template arguments: '%s'",
	term_tostr(tm, term_tostr_sort_humanreadable).c_str());
      #endif
    }
    if (tparams_len != 0) {
      term_list tlev(targs.size());
      eval_term_list_internal(targs, targs_evaluated, tlev, ectx, pos);
      DBG_INST(fprintf(stderr, "instantiate tm=%s evaluated_args=%s\n",
	term_tostr_human(rebuild_term(tptr, texpr, targs)).c_str(),
	term_tostr_list_human(term_list_range(tlev, targs.size())).c_str()));
      if (!is_partial_eval_uneval(term_list_range(tlev), ectx)) {
	DBG_INST(fprintf(stderr, "instantiate!\n"));
	expr_i *const einst = instantiate_template(texpr,
	  term_list_range(tlev), pos);
	DBG_EVAL(fprintf(stderr, "EVALI2 sort tmpl instantiated %s\n",
	  term_tostr(einst->get_value_texpr(), term_tostr_sort_strict)
	    .c_str()
	  ));
	return einst->get_value_texpr();
      }
      assert(ectx.need_partial_eval);
      DBG_INST(fprintf(stderr, "not instantiated\n"));
      /* not instantiate yet */
    }
    DBG_EVAL(fprintf(stderr, "EVALI2 sort tmpl not-instantiated\n"));
    return rebuild_term(tptr, texpr, targs);
  }
  return rebuild_term(tptr, texpr, targs);
}

term eval_term_internal(const term& tm, bool targs_evaluated,
  eval_context& ectx, expr_i *pos)
{
  DBG_EVAL(fprintf(stderr, "EVALI %s BEGIN\n",
    term_tostr_human(tm).c_str()));
  expr_i *const texpr = tm.get_expr();
  if (texpr == 0) {
    const term_sort s = tm.get_sort();
    if (s == term_sort_lambda) {
      const term_lambda *lmd = tm.get_term_lambda();
      if (!lmd->upvalues.empty()) {
	/* is it possible to fix that targs are evaluated again when
	 * targs_evaluated is true? */
	return save_upvalue_bind(tm, lmd->upvalues, ectx, pos);
      }
    }
    return tm;
  }
  const term r = eval_apply_expr(texpr, *tm.get_args(), targs_evaluated,
    &tm, ectx, pos);
  DBG_EVAL_TRACE(fprintf(stderr, "eval_term_internal %s => %s\n",
    term_tostr_human(tm).c_str(),
    term_tostr_human(r).c_str()));
  return r;
}

term eval_expr(expr_i *e, bool need_partial_eval)
{
  DBG_EVAL(fprintf(stderr, "eval %s\n", e->dump(0).c_str()));
  eval_context ectx;
  const term tm = expr_to_term(e, ectx, e);
  ectx.need_partial_eval = need_partial_eval;
  term t = eval_term_internal(tm, false, ectx, e);
  return t;
}

term eval_term(const term& t, bool need_partial_eval)
{
  eval_context ectx;
  ectx.need_partial_eval = need_partial_eval;
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

static expr_i *term_get_instance_internal(const term& t, bool throw_if_noinst)
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
    if (throw_if_noinst) {
      arena_error_throw(texpr, "Template expression '%s' is invalid",
	term_tostr_human(t).c_str());
    } else {
      return texpr;
    }
  }
  expr_i *const r = i->second->parent_expr;
  return r;
}

expr_i *term_get_instance(term& t)
{
  return term_get_instance_internal(t, true);
}

expr_i *term_get_instance_if(term& t)
{
  return term_get_instance_internal(t, false);
}

const expr_i *term_get_instance_const(const term& t)
{
  return term_get_instance_internal(t, true);
}

}; 

