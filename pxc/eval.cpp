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
#define DBG_COMPILE(x)
#define DBG_TETERM(x)
#define DBG_META(x)
#define DBG_METAARGTYPE(x)
#define DBG_METASYM(x)
#define DBG_METACONCAT(x)
#define DBG_METALOCAL(x)
#define DBG_ATTR(x)

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

static expr_i *deep_clone_template(expr_i *e, expr_block *instantiate_root,
  const std::string& k, bool set_instantiated_flag)
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
      /* note: macrodef block must be instantiated even if it has a tparam */
      set_instantiated_flag = false;
    }
    ebcpy->tinfo.instantiated = set_instantiated_flag;
  }
  #if 0
  symbol_common *const sdef = ecpy->get_sdef();
  if (sdef != 0) {
    sdef->set_symdef(0); // FIXME: remove
    sdef->set_evaluated(term()); // FIXME: remove
  }
  #endif
  if (ecpy->get_esort() == expr_e_tparams) {
    DBG_CLONE3(fprintf(stderr, "deep_clone src tparams %p paramdef %p\n", e,
      ptr_down_cast<expr_tparams>(e)->param_def.get_expr()));
    DBG_CLONE3(fprintf(stderr, "deep_clone dst tparams %p paramdef %p\n", ecpy,
      ptr_down_cast<expr_tparams>(ecpy)->param_def.get_expr()));
  }
  const int num = ecpy->get_num_children();
  for (int i = 0; i < num; ++i) {
    expr_i *const c = ecpy->get_child(i);
    expr_i *const ccpy = deep_clone_template(c, instantiate_root, k,
      set_instantiated_flag);
    ecpy->set_child(i, ccpy);
  }
  return ecpy;
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
  expr_i *const rcpy = deep_clone_template(tmpl_root, tmpl_block, k, true);
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
  // FIXME: test
  attribute_e nattr = attribute_e(rcpy->get_attribute()
    & ~(attribute_threaded | attribute_multithr));
  if (term_is_multithr(rt)) {
    nattr = attribute_e(nattr | attribute_multithr);
  } else if (term_is_threaded(rt)) {
    nattr = attribute_e(nattr | attribute_threaded);
  }
  rcpy->set_attribute(nattr);
  DBG_ATTR(fprintf(stderr, "%s: attr %d\n", term_tostr_human(rt).c_str(),
    nattr));
  if (rcpy->get_esort() == expr_e_struct) {
    check_inherit_threading(ptr_down_cast<expr_struct>(rcpy));
  }
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
  /* add tparam upvalues */
  if (rcpy->get_esort() == expr_e_funcdef) {
    add_tparam_upvalues_funcdef(ptr_down_cast<expr_funcdef>(rcpy));
  }
  return rcpy;
}

expr_i *instantiate_template(expr_i *tmpl_root, term_list& args_move,
  expr_i *pos)
{
  arena_error_throw_pushed(); /* it's necessary! */
  const std::string k = term_tostr_list_human(args_move);
  try {
    expr_i *const re = instantiate_template_internal(tmpl_root, args_move);
    arena_error_throw_pushed();
    return re;
  } catch (const std::exception& e) {
    std::string s = e.what();
    s = std::string(pos->fname) + ":" + ulong_to_string(pos->line)
      + ": (instantiated from here) [" + k + "]\n" + s;
    throw std::runtime_error(s);
  }
}

typedef std::map<std::string, term> env_type;

static term eval_term_internal(const term& t, env_type& env, size_t depth,
  expr_i *pos);

static term_list eval_term_list_internal(const term_list *tl, env_type& env,
  size_t depth, expr_i *pos)
{
  if (tl == 0) {
    return term_list();
  }
  term_list r;
  for (term_list::const_iterator i = tl->begin(); i != tl->end(); ++i) {
    r.push_back(eval_term_internal(*i, env, depth, pos));
  }
  return r;
}

static bool has_unbound_tparam(const term_list& tl);

static bool has_unbound_tparam(const term& t)
{
  if (is_tpdummy_type(t)) {
    return true;
  }
  expr_tparams *const tp = dynamic_cast<expr_tparams *>(t.get_expr());
  if (tp != 0 && tp->param_def.is_null()) {
    return true;
  }
  const term_list *const cargs = t.get_args();
  if (cargs != 0 && has_unbound_tparam(*cargs)) {
    return true;
  }
  return false;
}

static bool has_unbound_tparam(const term_list& tl)
{
  for (term_list::const_iterator i = tl.begin(); i != tl.end(); ++i) {
    if (has_unbound_tparam(*i)) {
      return true;
    }
  }
  return false;
}

static bool term_truth_value(const term& t)
{
  if (t.is_null()) {
    return false;
  }
  if (t.is_long()) {
    return t.get_long() != 0;
  }
  if (t.is_string()) {
    return !t.get_string().empty();
  }
  return true;
}

static term eval_meta_local(term_list& tlev, env_type& env,
  size_t depth, expr_i *pos)
{
  if (tlev.size() < 2) {
    return term();
  }
  term& typ = tlev[0];
  const std::string name = meta_term_to_string(tlev[1], false);
  DBG_METALOCAL(fprintf(stderr, "eval_meta_local: '%s'\n", name.c_str()));
  expr_i *const einst = term_get_instance(typ);
  expr_block *const bl = einst->get_template_block();
  if (einst == 0 || bl == 0) {
    return term();
  }
  if (name.find(':') != name.npos) {
    return term(); /* invalid name */
  }
  symbol_table *const symtbl = &bl->symtbl;
  assert(symtbl);
  const std::string sym_ns;
  const bool no_private = true;
  bool is_global = false, is_upvalue = false, is_memfld = false;
  expr_i *const rsym = symtbl->resolve_name_nothrow(name, no_private, sym_ns,
    is_global, is_upvalue, is_memfld);
  DBG_METASYM(fprintf(stderr, "meta_symbol name=[%s] ns=[%s] rsym=%p\n",
    name.c_str(), sym_ns.c_str(), rsym));
  if (rsym == 0) {
    return term(); /* not found */
  }
  term_list rtargs;
  rtargs.insert(rtargs.begin(), tlev.begin() + 2, tlev.end());
  term rt(rsym, rtargs);
  return eval_term_internal(rt, env, depth + 1, pos);
}

static term eval_meta_symbol(term_list& tlev, env_type& env,
  size_t depth, expr_i *pos)
{
  if (tlev.size() < 2) {
    return term();
  }
  term& typ = tlev[0];
  const std::string name = meta_term_to_string(tlev[1], false);
  expr_i *const typexpr = typ.get_expr();
  if (typexpr == 0 || !is_type_or_func_esort(typexpr->get_esort())) {
    return term();
  }
  if (name.find(':') != name.npos) {
    return term(); /* invalid name */
  }
  symbol_table *const symtbl = &global_block->symtbl;
  assert(symtbl);
  std::string sym_ns = typexpr->get_ns();
  if (sym_ns.empty()) {
    sym_ns = "pxcrt";
  }
  if (loaded_namespaces.find(sym_ns) == loaded_namespaces.end()) {
    /* namespace sym_ns is not loaded. error. */
    /* note that metafunctions must be always pure functional, or generated
       template functions can be inconsistent among compilation units. we
       must generate an error here instead of returning 'notfound', in order
       to avoid such inconsistency. */
    DBG(fprintf(stderr, "not loaded: [%s] term=[%s]\n", sym_ns.c_str(),
      term_tostr_human(typ).c_str()));
    arena_error_throw(0, "namespace '%s' not imported", sym_ns.c_str());
  }
  const bool no_private = true;
  bool is_global = false, is_upvalue = false, is_memfld = false;
  expr_i *const rsym = symtbl->resolve_name_nothrow(name, no_private, sym_ns,
    is_global, is_upvalue, is_memfld);
  DBG_METASYM(fprintf(stderr, "meta_symbol name=[%s] ns=[%s] rsym=%p\n",
    name.c_str(), sym_ns.c_str(), rsym));
  if (rsym == 0 || !is_global) {
    return term(0LL);
  }
  return term(rsym);
  #if 0
  term_list rtargs;
  rtargs.insert(rtargs.begin(), tlev.begin() + 2, tlev.end());
  term rt(rsym, rtargs);
  return eval_term_internal(rt, env, depth + 1, pos);
  #endif
}

static term eval_meta_apply(term_list& tlev, env_type& env,
  size_t depth, expr_i *pos)
{
  if (tlev.size() < 1) {
    return term();
  }
  term& t0 = tlev[0];
  expr_i *const t0expr = t0.get_expr();
  if (t0expr == 0) {
    return term();
  }
  term_list rtargs;
  rtargs.insert(rtargs.begin(), tlev.begin() + 1, tlev.end());
  term rt(t0expr, rtargs);
  return eval_term_internal(rt, env, depth + 1, pos);
}

static term eval_meta_argnum(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  term& ttyp = tlev[0];
  expr_i *const ttypexpr = ttyp.get_expr();
  if (ttypexpr == 0 ||
    !is_type_or_func_esort(ttypexpr->get_esort())) {
    return term();
  }
  expr_i *const einst = term_get_instance(ttyp);
  DBG_METAARGTYPE(fprintf(stderr, "t.expr=%p inst=%p\n", t.expr, einst));
  if (einst->get_esort() == expr_e_funcdef) {
    expr_funcdef *const efd = ptr_down_cast<expr_funcdef>(einst);
    expr_argdecls *ad = efd->block->argdecls;
    const long long len = elist_length(ad);
    return term(len);
  } else {
    /* not a function */
  }
  return term();
}

static term eval_meta_rettype(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  term& ttyp = tlev[0];
  expr_i *const ttypexpr = ttyp.get_expr();
  if (ttypexpr == 0 ||
    !is_type_or_func_esort(ttypexpr->get_esort())) {
    return term();
  }
  expr_i *const einst = term_get_instance(ttyp);
  expr_funcdef *const efd = ptr_down_cast<expr_funcdef>(einst);
  DBG_METAARGTYPE(fprintf(stderr, "rettype\n"));
  if (efd != 0) {
    return efd->get_rettype();
  } else if (is_type(ttyp)) {
    return ttyp;
  }
  return term();
}

static term eval_meta_argtype(term_list& tlev)
{
  if (tlev.size() != 2) {
    return term();
  }
  term& ttyp = tlev[0];
  term& tnum = tlev[1];
  expr_i *const ttypexpr = ttyp.get_expr();
  if (ttypexpr == 0 ||
    !is_type_or_func_esort(ttypexpr->get_esort()) ||
    !tnum.is_long() || tnum.get_long() < 0) {
    return term();
  }
  const long long n = tnum.get_long();
  expr_i *const einst = term_get_instance(ttyp);
  DBG_METAARGTYPE(fprintf(stderr, "t.expr=%p inst=%p\n", t.expr, einst));
  expr_block *block = einst->get_template_block();
  DBG_METAARGTYPE(fprintf(stderr, "arg %d\n", n));
  expr_argdecls *ad = block->argdecls;
  for (int i = 0; i < n && ad != 0; ++i) {
    ad = ad->rest;
  }
  if (ad != 0) {
    return ad->resolve_texpr();
  }
  return term();
}

static term eval_meta_argbyref(term_list& tlev)
{
  if (tlev.size() != 2) {
    return term();
  }
  term& ttyp = tlev[0];
  term& tnum = tlev[1];
  expr_i *const ttypexpr = ttyp.get_expr();
  if (ttypexpr == 0 ||
    !is_type_or_func_esort(ttypexpr->get_esort()) ||
    !tnum.is_long() || tnum.get_long() < 0) {
    return term();
  }
  const long long n = tnum.get_long();
  expr_i *const einst = term_get_instance(ttyp);
  DBG_METAARGTYPE(fprintf(stderr, "t.expr=%p inst=%p\n", t.expr, einst));
  expr_block *block = einst->get_template_block();
  DBG_METAARGTYPE(fprintf(stderr, "arg %d\n", n));
  expr_argdecls *ad = block->argdecls;
  for (int i = 0; i < n && ad != 0; ++i) {
    ad = ad->rest;
  }
  if (ad != 0) {
    return term(ad->byref_flag ? 1 : 0);
  }
  return term();
}

static term eval_meta_argtypes(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  term& ttyp = tlev[0];
  expr_i *const ttypexpr = ttyp.get_expr();
  if (ttypexpr == 0 ||
    !is_type_or_func_esort(ttypexpr->get_esort())) {
    return term();
  }
  expr_i *const einst = term_get_instance(ttyp);
  DBG_METAARGTYPE(fprintf(stderr, "t.expr=%p inst=%p\n", t.expr, einst));
  expr_block *block = einst->get_template_block();
  DBG_METAARGTYPE(fprintf(stderr, "arg %d\n", n));
  term_list tl;
  for (expr_argdecls *ad = block->argdecls; ad != 0; ad = ad->rest) {
    tl.push_back(ad->resolve_texpr());
  }
  return term(tl);
}

static term eval_meta_argnames(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  term& ttyp = tlev[0];
  expr_i *const ttypexpr = ttyp.get_expr();
  if (ttypexpr == 0 ||
    !is_type_or_func_esort(ttypexpr->get_esort())) {
    return term();
  }
  expr_i *const einst = term_get_instance(ttyp);
  DBG_METAARGTYPE(fprintf(stderr, "t.expr=%p inst=%p\n", t.expr, einst));
  expr_block *block = einst->get_template_block();
  DBG_METAARGTYPE(fprintf(stderr, "arg %d\n", n));
  term_list tl;
  expr_argdecls *ad = block->argdecls;
  for (; ad != 0; ad = ad->rest) {
    const std::string s(ad->sym);
    tl.push_back(term(s));
  }
  return term(tl);
}

static term eval_meta_imports(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  const std::string name = meta_term_to_string(tlev[0], false);
  loaded_namespaces_type::const_iterator iter = loaded_namespaces.find(name);
  if (iter == loaded_namespaces.end()) {
    arena_error_throw(0, "namespace '%s' not imported", name.c_str());
  }
  const imports_type::deps_type& deps = iter->second.deps;
  term_list tl;
  for (imports_type::deps_type::const_iterator i = deps.begin();
    i != deps.end(); ++i) {
    term_list e;
    e.push_back(term(i->ns));
    e.push_back(term(i->import_public ? 0LL : 1LL));
    tl.push_back(term(e));
  }
  return term(tl);
}

static void get_imports_rec(std::deque<std::string>& lst, strset& se,
  const std::string& n, bool is_main)
{
  if (se.find(n) != se.end()) {
    return;
  }
  se.insert(n);
  loaded_namespaces_type::const_iterator iter = loaded_namespaces.find(n);
  if (iter == loaded_namespaces.end()) {
    arena_error_throw(0, "internal error: namespace not found: '%s'",
      n.c_str());
  }
  const imports_type::deps_type& deps = iter->second.deps;
  term_list tl;
  for (imports_type::deps_type::const_iterator i = deps.begin();
    i != deps.end(); ++i) {
    if (i->import_public || is_main) {
      get_imports_rec(lst, se, i->ns, false);
    }
  }
  lst.push_back(n);
}

static term eval_meta_imports_tr(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  const std::string name = meta_term_to_string(tlev[0], false);
  loaded_namespaces_type::const_iterator iter = loaded_namespaces.find(name);
  if (iter == loaded_namespaces.end()) {
    arena_error_throw(0, "namespace '%s' not imported", name.c_str());
  }
  std::deque<std::string> lst;
  strset se;
  get_imports_rec(lst, se, name, true);
  term_list tl;
  for (std::deque<std::string>::iterator i = lst.begin(); i != lst.end();
    ++i) {
    tl.push_back(term(*i));
  }
  return term(tl);
}

static term eval_meta_functions(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  const std::string name = meta_term_to_string(tlev[0], false);
  if (loaded_namespaces.find(name) == loaded_namespaces.end()) {
    arena_error_throw(0, "namespace '%s' not imported", name.c_str());
  }
  symbol_table *const symtbl = &global_block->symtbl;
  symbol_table::local_names_type::const_iterator i;
  term_list tl;
  for (i = symtbl->local_names.begin(); i != symtbl->local_names.end(); ++i) {
    symbol_table::locals_type::const_iterator j = symtbl->locals.find(*i);
    assert(j != symtbl->locals.end());
    const localvar_info& lv = j->second;
    expr_i *const e = lv.edef;
    if (lv.has_attrib_private() || e->get_ns() != name) {
      continue;
    }
    if (e->get_esort() == expr_e_funcdef) {
      tl.push_back(term(e));
    }
  }
  return term(tl);
}

static term eval_meta_types(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  const std::string name = meta_term_to_string(tlev[0], false);
  if (loaded_namespaces.find(name) == loaded_namespaces.end()) {
    arena_error_throw(0, "namespace '%s' not imported", name.c_str());
  }
  symbol_table *const symtbl = &global_block->symtbl;
  symbol_table::local_names_type::const_iterator i;
  term_list tl;
  for (i = symtbl->local_names.begin(); i != symtbl->local_names.end(); ++i) {
    symbol_table::locals_type::const_iterator j = symtbl->locals.find(*i);
    assert(j != symtbl->locals.end());
    const localvar_info& lv = j->second;
    expr_i *const e = lv.edef;
    if (lv.has_attrib_private() || e->get_ns() != name) {
      continue;
    }
    bool is_type_flag = false;
    switch (e->get_esort()) {
    case expr_e_typedef:
    case expr_e_struct:
    case expr_e_variant:
    case expr_e_interface:
      is_type_flag = true;
      break;
    default:
      break;
    }
    if (is_type_flag) {
      tl.push_back(term(e));
    }
  }
  return term(tl);
}

static term eval_meta_member_functions(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  expr_i *const einst = term_get_instance(tlev[0]);
  if (!is_type_esort(einst->get_esort())) {
    return term();
  }
  term_list tl;
  expr_struct *const est = dynamic_cast<expr_struct *>(einst);
  if (est != 0) {
    symbol_table *const symtbl = &est->block->symtbl;
    symbol_table::local_names_type::const_iterator i;
    for (i = symtbl->local_names.begin(); i != symtbl->local_names.end();
      ++i) {
      symbol_table::locals_type::const_iterator j = symtbl->locals.find(*i);
      assert(j != symtbl->locals.end());
      const localvar_info lv = j->second;
      expr_i *const e = lv.edef;
      if (lv.has_attrib_private()) {
	continue;
      }
      if (e->get_esort() == expr_e_funcdef) {
	tl.push_back(term(e));
      }
    }
  }
  return term(tl);
}

static term eval_meta_field_types(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  std::list<expr_var *> flds;
  expr_i *const einst = term_get_instance(tlev[0]);
  expr_struct *const est = dynamic_cast<expr_struct *>(einst);
  if (est != 0) {
    est->get_fields(flds);
  }
  expr_variant *const ev = dynamic_cast<expr_variant *>(einst);
  if (ev != 0) {
    ev->get_fields(flds);
  }
  term_list tl;
  for (std::list<expr_var *>::const_iterator i = flds.begin(); i != flds.end();
    ++i) {
    term& t = (*i)->resolve_texpr();
    tl.push_back(t);
  }
  return term(tl);
}

static term eval_meta_field_names(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  std::list<expr_var *> flds;
  expr_i *const einst = term_get_instance(tlev[0]);
  expr_struct *const est = dynamic_cast<expr_struct *>(einst);
  if (est != 0) {
    est->get_fields(flds);
  }
  expr_variant *const ev = dynamic_cast<expr_variant *>(einst);
  if (ev != 0) {
    ev->get_fields(flds);
  }
  term_list tl;
  for (std::list<expr_var *>::const_iterator i = flds.begin(); i != flds.end();
    ++i) {
    tl.push_back(term(std::string((*i)->sym)));
  }
  return term(tl);
}

static term eval_meta_num_tparams(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  const term& t = tlev[0];
  expr_i *const e = t.get_expr();
  if (e == 0) {
    return term();
  }
  expr_block *const ttbl = e->get_template_block();
  if (ttbl == 0) {
    return term(0LL);
  }
  const long long tparams_len = ttbl->tinfo.is_uninstantiated()
    ? elist_length(ttbl->tinfo.tparams) : 0;
  return term(tparams_len);
}

static term eval_meta_num_targs(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  const term& t = tlev[0];
  expr_i *const e = t.get_expr();
  if (e == 0) {
    return term();
  }
  const term_list *const targs = t.get_args();
  const long long v = targs == 0 ? 0 : targs->size();
  return term(v);
}

static term eval_meta_targs(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  const term& t = tlev[0];
  expr_i *const e = t.get_expr();
  if (e == 0) {
    return term();
  }
  const term_list *const targs = t.get_args();
  return term(*targs);
}

static term eval_meta_list(term_list& tlev)
{
  return term(tlev);
}

static term eval_meta_at(term_list& tlev)
{
  if (tlev.size() != 2) {
    return term();
  }
  const term& t = tlev[0];
  const term_list *const targs = t.is_metalist()
    ? t.get_metalist() : t.get_args();
  if (targs == 0) {
    return term();
  }
  const unsigned long long v = meta_term_to_long(tlev[1]);
  if (targs->size() <= v) {
    return term();
  }
  return (*targs)[v];
}

static term eval_meta_size(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  const term& t = tlev[0];
  const term_list *const targs = t.is_metalist()
    ? t.get_metalist() : t.get_args();
  if (targs == 0) {
    return term();
  }
  const long long v = targs->size();
  return term(v);
}

static term eval_meta_seq(term_list& tlev)
{
  if (tlev.size() != 1 && tlev.size() != 2) {
    return term();
  }
  long long v0 = 0, v1 = 0;
  if (tlev.size() == 2) {
    v0 = meta_term_to_long(tlev[0]);
    v1 = meta_term_to_long(tlev[1]);
  } else {
    v1 = meta_term_to_long(tlev[0]);
  }
  if (v0 < 0 || v1 < 0 || v0 > v1) {
    return term();
  }
  term_list tl;
  for (long long i = v0; i < v1; ++i) {
    tl.push_back(term(i));
  }
  return term(tl);
}

static term eval_meta_join(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  const term& t = tlev[0];
  if (!t.is_metalist()) {
    return term();
  }
  const term_list& tl = *t.get_metalist();
  term_list rtl;
  for (term_list::const_iterator i = tl.begin(); i != tl.end(); ++i) {
    const term_list *const il = i->get_metalist();
    if (il == 0) {
      return term();
    }
    rtl.insert(rtl.end(), il->begin(), il->end());
  }
  return term(rtl);
}

static term eval_meta_join_all(term_list& tlev)
{
  term_list rtl;
  for (term_list::const_iterator i = tlev.begin(); i != tlev.end(); ++i) {
    const term_list *const il = i->get_metalist();
    if (il == 0) {
      return term();
    }
    rtl.insert(rtl.end(), il->begin(), il->end());
  }
  return term(rtl);
}

static term eval_meta_join_tail(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  const term& t = tlev[0];
  const term_list *tl = t.get_metalist();
  term_list rtl;
  while (tl != 0) {
    const size_t sz = tl->size();
    if (sz <= 1) {
      break;
    }
    rtl.insert(rtl.end(), tl->begin(), tl->end() - 1);
    tl = (*tl)[sz - 1].get_metalist();
  }
  return term(rtl);
}

static term eval_meta_uniq(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  const term& t = tlev[0];
  if (!t.is_metalist()) {
    return term();
  }
  const term_list& tl = *t.get_metalist();
  std::set<term> vs;
  term_list rtl;
  for (term_list::const_iterator i = tl.begin(); i != tl.end(); ++i) {
    if (vs.find(*i) != vs.end()) {
      continue;
    }
    vs.insert(*i);
    rtl.push_back(*i);
  }
  return term(rtl);
}

static term eval_meta_is_type(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  const long long v = is_type(tlev[0]);
  return term(v);
}

static term eval_meta_is_function(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  const long long v = is_function(tlev[0]);
  return term(v);
}

static term eval_meta_is_int(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  const long long v = tlev[0].is_long();
  return term(v);
}

static term eval_meta_is_string(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  const long long v = tlev[0].is_string();
  return term(v);
}

static term eval_meta_to_int(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  const term& t = tlev[0];
  if (t.is_long()) {
    return t;
  }
  return term(meta_term_to_long(t));
}

static term eval_meta_to_string(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  const term& t = tlev[0];
  if (t.is_string()) {
    return t;
  }
  return term(meta_term_to_string(t, false));
}

static term eval_meta_full_string(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  const term& t = tlev[0];
  if (t.is_string()) {
    return t;
  }
  return term(meta_term_to_string(t, true));
}

static term eval_meta_concat(term_list& tlev)
{
  std::string s;
  for (term_list::iterator i = tlev.begin(); i != tlev.end(); ++i) {
    s += meta_term_to_string(*i, false);
  }
  DBG_METACONCAT(fprintf(stderr, "meta_concat r=[%s]\n", s.c_str()));
  return term(s);
}

static term eval_meta_category(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  expr_i *const einst = term_get_instance(tlev[0]);
  expr_struct *const est = dynamic_cast<expr_struct *>(einst);
  if (est != 0 && est->category != 0) {
    const std::string s(est->category);
    if (!s.empty() && s[0] == '@') {
      return term("metafunction");
    }
    /* farray, varray, map, ref, cref, ... */
    return term(s);
  }
  if (est != 0) {
    return term("struct");
  }
  switch (einst->get_esort()) {
  case expr_e_variant:
    return term("union");
  case expr_e_interface:
    return term("interface");
  case expr_e_typedef:
    return term("builtin");
  case expr_e_macrodef:
    return term("macro");
  case expr_e_funcdef:
    return term("function");
  case expr_e_var:
    return term("variable");
  case expr_e_argdecls:
    return term("argument");
  case expr_e_extval:
    return term("extvalue");
  default:
    break;
  }
  return term("unknown");
}

static term eval_meta_nsname(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  expr_i *const e = tlev[0].get_expr();
  if (e == 0) {
    return term();
  }
  const term t(e); /* drop params */
  const std::string s(term_tostr_human(t));
  return term(s);
}

static term eval_meta_not(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  const bool c = term_truth_value(tlev[0]);
  return term(c ? 0LL : 1LL);
}

static term eval_meta_eq(term_list& tlev)
{
  /* TODO: lazy? */
  int v = 1;
  if (tlev.size() < 1) {
    v = 1;
  } else {
    const term& t = tlev[0];
    for (term_list::const_iterator i = ++tlev.begin(); i != tlev.end(); ++i) {
      if (t != *i) {
	v = 0;
	break;
      }
    }
  }
  return term(v);
}

static term eval_meta_add(term_list& tlev)
{
  long long v = 0;
  for (term_list::iterator i = tlev.begin(); i != tlev.end(); ++i) {
    v += meta_term_to_long(*i);
  }
  return term(v);
}

static term eval_meta_sub(term_list& tlev)
{
  if (tlev.size() != 2) {
    return term();
  }
  return term(meta_term_to_long(tlev[0]) - meta_term_to_long(tlev[1]));
}

static term eval_meta_mul(term_list& tlev)
{
  long long v = 1;
  for (term_list::iterator i = tlev.begin(); i != tlev.end(); ++i) {
    v *= meta_term_to_long(*i);
  }
  return term(v);
}

static term eval_meta_div(term_list& tlev)
{
  if (tlev.size() != 2) {
    return term();
  }
  const long long v0 = meta_term_to_long(tlev[0]);
  const long long v1 = meta_term_to_long(tlev[1]);
  if (v1 == 0) {
    return term();
  }
  return term(v0 / v1);
}

static term eval_meta_mod(term_list& tlev)
{
  if (tlev.size() != 2) {
    return term();
  }
  const long long v0 = meta_term_to_long(tlev[0]);
  const long long v1 = meta_term_to_long(tlev[1]);
  if (v1 == 0) {
    return term();
  }
  return term(v0 % v1);
}

static term eval_meta_gt(term_list& tlev)
{
  if (tlev.size() != 2) {
    return term();
  }
  return term(meta_term_to_long(tlev[0]) > meta_term_to_long(tlev[1]));
}

static term eval_meta_lt(term_list& tlev)
{
  if (tlev.size() != 2) {
    return term();
  }
  return term(meta_term_to_long(tlev[0]) < meta_term_to_long(tlev[1]));
}

static term eval_meta_map(term_list& tlev, env_type& env, size_t depth,
  expr_i *pos)
{
  if (tlev.size() != 2) {
    return term();
  }
  const term& t0 = tlev[0]; /* list */
  const term& t1 = tlev[1]; /* function */
  if (!t0.is_metalist()) {
    arena_error_throw(0, "not a list: '%s'", term_tostr_human(t0).c_str());
  }
  if (!t1.is_expr()) {
    arena_error_throw(0, "not a symbol: '%s'", term_tostr_human(t1).c_str());
  }
  if (t1.get_args() != 0 && !t1.get_args()->empty()) {
    arena_error_throw(0, "not a symbol: '%s'", term_tostr_human(t1).c_str());
  }
  const term_list& tl = *t0.get_metalist();
  term_list rtl;
  for (term_list::const_iterator i = tl.begin(); i != tl.end(); ++i) {
    term_list itl;
    itl.push_back(*i);
    term it(t1.get_expr(), itl);
    term tc = eval_term_internal(it, env, depth + 1, pos);
    rtl.push_back(tc);
  }
  return term(rtl);
}

static term eval_meta_filter(term_list& tlev, env_type& env, size_t depth,
  expr_i *pos)
{
  if (tlev.size() != 2) {
    return term();
  }
  const term& t0 = tlev[0]; /* list */
  const term& t1 = tlev[1]; /* function */
  if (!t0.is_metalist()) {
    arena_error_throw(0, "not a list: '%s'", term_tostr_human(t0).c_str());
  }
  if (!t1.is_expr()) {
    arena_error_throw(0, "not a symbol: '%s'", term_tostr_human(t1).c_str());
  }
  if (t1.get_args() != 0 && !t1.get_args()->empty()) {
    arena_error_throw(0, "not a symbol: '%s'", term_tostr_human(t1).c_str());
  }
  const term_list& tl = *t0.get_metalist();
  term_list rtl;
  for (term_list::const_iterator i = tl.begin(); i != tl.end(); ++i) {
    term_list itl;
    itl.push_back(*i);
    term it(t1.get_expr(), itl);
    term tc = eval_term_internal(it, env, depth + 1, pos);
    const long long  v = meta_term_to_long(tc);
    if (v != 0) {
      rtl.push_back(*i);
    }
  }
  return term(rtl);
}

static term compose_term(const term& t, term_list& tlev)
{
  if (!t.is_expr()) {
    return t;
  }
  expr_i *const e = t.get_expr();
  return term(e, tlev);
}

static term eval_metafunction(const std::string& name, term_list& tlev,
  const term& t, env_type& env, size_t depth, expr_i *pos)
{
  DBG_META(fprintf(stderr, "eval_metafunction: %s\n", name.c_str()));
  term r;
  try {
    if (name == "@list") {
      r = eval_meta_list(tlev);
    } else if (name == "@at") {
      r = eval_meta_at(tlev);
    } else if (name == "@size") {
      r = eval_meta_size(tlev);
    } else if (name == "@seq") {
      r = eval_meta_seq(tlev);
    } else if (name == "@join") {
      r = eval_meta_join(tlev);
    } else if (name == "@join_all") {
      r = eval_meta_join_all(tlev);
    } else if (name == "@join_tail") {
      r = eval_meta_join_tail(tlev);
    } else if (name == "@uniq") {
      r = eval_meta_uniq(tlev);
    } else if (name == "@rettype") {
      r = eval_meta_rettype(tlev);
    } else if (name == "@argnum") {
      r = eval_meta_argnum(tlev);
    } else if (name == "@argtype") {
      r = eval_meta_argtype(tlev);
    } else if (name == "@argbyref") {
      r = eval_meta_argbyref(tlev);
    } else if (name == "@argtypes") {
      r = eval_meta_argtypes(tlev);
    } else if (name == "@argnames") {
      r = eval_meta_argnames(tlev);
    } else if (name == "@imports") {
      r = eval_meta_imports(tlev);
    } else if (name == "@imports_tr") {
      r = eval_meta_imports_tr(tlev);
    } else if (name == "@functions") {
      r = eval_meta_functions(tlev);
    } else if (name == "@types") {
      r = eval_meta_types(tlev);
    } else if (name == "@member_functions") {
      r = eval_meta_member_functions(tlev);
    } else if (name == "@field_types") {
      r = eval_meta_field_types(tlev);
    } else if (name == "@field_names") {
      r = eval_meta_field_names(tlev);
    } else if (name == "@num_tparams") {
      r = eval_meta_num_tparams(tlev);
    } else if (name == "@num_targs") {
      r = eval_meta_num_targs(tlev);
    } else if (name == "@targs") {
      r = eval_meta_targs(tlev);
    } else if (name == "@is_type") {
      r = eval_meta_is_type(tlev);
    } else if (name == "@is_function") {
      r = eval_meta_is_function(tlev);
    } else if (name == "@is_int") {
      r = eval_meta_is_int(tlev);
    } else if (name == "@is_string") {
      r = eval_meta_is_string(tlev);
    } else if (name == "@to_int") {
      r = eval_meta_to_int(tlev);
    } else if (name == "@to_string") {
      r = eval_meta_to_string(tlev);
    } else if (name == "@full_string") {
      r = eval_meta_full_string(tlev);
    } else if (name == "@concat") {
      r = eval_meta_concat(tlev);
    } else if (name == "@category") {
      r = eval_meta_category(tlev);
    } else if (name == "@nsname") {
      r = eval_meta_nsname(tlev);
    } else if (name == "@not") {
      r = eval_meta_not(tlev);
    } else if (name == "@eq") {
      r = eval_meta_eq(tlev);
    } else if (name == "@add") {
      r = eval_meta_add(tlev);
    } else if (name == "@sub") {
      r = eval_meta_sub(tlev);
    } else if (name == "@mul") {
      r = eval_meta_mul(tlev);
    } else if (name == "@div") {
      r = eval_meta_div(tlev);
    } else if (name == "@mod") {
      r = eval_meta_mod(tlev);
    } else if (name == "@gt") {
      r = eval_meta_gt(tlev);
    } else if (name == "@lt") {
      r = eval_meta_lt(tlev);
    } else if (name == "@map") {
      r = eval_meta_map(tlev, env, depth, pos);
    } else if (name == "@filter") {
      r = eval_meta_filter(tlev, env, depth, pos);
    } else if (name == "@local") {
      r = eval_meta_local(tlev, env, depth, pos);
    } else if (name == "@symbol") {
      r = eval_meta_symbol(tlev, env, depth, pos);
    } else if (name == "@apply") {
      r = eval_meta_apply(tlev, env, depth, pos);
    }
  } catch (const std::exception& e) {
    std::string s = e.what();
    s += std::string(pos->fname) + ":" + ulong_to_string(pos->line)
      + ": (while evaluating expression: "
      + term_tostr(compose_term(t, tlev), term_tostr_sort_humanreadable)
      + ")\n";
    throw std::runtime_error(s);
  }
  if (r.is_null()) {
    arena_error_throw(pos, "invalid template expression: %s",
      term_tostr(compose_term(t, tlev),
      term_tostr_sort_humanreadable).c_str());
  }
  return r;
}

static term eval_meta_metaif(const term& t, env_type& env, size_t depth,
  expr_i *pos)
{
  const term_list *args = t.get_args();
  if (args == 0) {
    return term();
  }
  const size_t num_args = args->size();
  if (num_args != 3) {
    return term();
  }
  term tc = eval_term_internal((*args)[0], env, depth + 1, pos);
  if (has_unbound_tparam(tc)) {
    return t; /* incomplete expr */
  }
  const bool c = term_truth_value(tc);
  if (c) {
    return eval_term_internal((*args)[1], env, depth + 1, pos);
  } else {
    return eval_term_internal((*args)[2], env, depth + 1, pos);
  }
}

static term eval_meta_or(const term& t, env_type& env,
  size_t depth, expr_i *pos)
{
  const term_list *args = t.get_args();
  if (args == 0) {
    return term(0LL);
  }
  for (term_list::const_iterator i = args->begin(); i != args->end(); ++i) {
    term tc = eval_term_internal(*i, env, depth + 1, pos);
    if (has_unbound_tparam(tc)) {
      return t;
    }
    if (term_truth_value(tc)) {
      return term(1LL);
    }
  }
  return term(0LL);
}

static term eval_meta_and(const term& t, env_type& env, size_t depth,
  expr_i *pos)
{
  const term_list *args = t.get_args();
  if (args == 0) {
    return term(1LL);
  }
  for (term_list::const_iterator i = args->begin(); i != args->end(); ++i) {
    term tc = eval_term_internal(*i, env, depth + 1, pos);
    if (has_unbound_tparam(tc)) {
      return t;
    }
    if (!term_truth_value(tc)) {
      return term(0LL);
    }
  }
  return term(1LL);
}

static term eval_metafunction_lazy(const std::string& name, const term& t,
  env_type& env, size_t depth, expr_i *pos)
{
  DBG_META(fprintf(stderr, "eval_metafunction_lazy: %s\n", name.c_str()));
  term r;
  try {
    if (name == "@@metaif") {
      r = eval_meta_metaif(t, env, depth, pos);
    } else if (name == "@@or") {
      r = eval_meta_or(t, env, depth, pos);
    } else if (name == "@@and") {
      r = eval_meta_and(t, env, depth, pos);
    } else if (name == "@@local") {
      abort(); // FIXME
    }
  } catch (const std::exception& e) {
    std::string s = e.what();
    s = std::string(pos->fname) + ":" + ulong_to_string(pos->line)
      + ": (while evaluating expression: "
      + term_tostr(t, term_tostr_sort_humanreadable) + ")\n";
    throw std::runtime_error(s);
  }
  if (r.is_null()) {
    arena_error_throw(pos, "invalid template expression: %s",
      term_tostr(t, term_tostr_sort_humanreadable).c_str());
  }
  return r;
}

struct depth_error { };

static term eval_term_internal(const term& t, env_type& env, size_t depth,
  expr_i *pos);

#if 0
static term eval_te(expr_i *te, env_type& env, size_t depth, expr_i *pos);
static term te_to_term(expr_i *te, env_type& env, size_t depth, expr_i *pos);
#endif

#if 0
static term te_to_term_local_lookup(expr_te *te, env_type& env, size_t depth,
  expr_i *pos)
{
  /* operator ::{te1, te2} */
  assert(te->tlarg);
  assert(te->tlarg->head);
  term te1 = eval_te(te->tlarg->head, env, depth, pos);
  expr_i *const teinst = term_get_instance(te1);
  // FIXME: check attribute
  // check_attribute_public(teinst, this);
  expr_block *const bl = teinst->get_template_block();
  if (bl == 0) {
    arena_error_throw(0, "can not apply '::' for '%s'",
      term_tostr_human(te1).c_str());
  }
  symbol_table *const lookup = &bl->symtbl;
  assert(te->tlarg->rest);
  expr_te *te2 = dynamic_cast<expr_te *>(te->tlarg->rest->head);
  assert(te2);
  expr_i *texpr = te2->resolve_symdef(lookup);
  term_list targs;
  for (expr_telist *tl = te2->tlarg; tl != 0; tl = tl->rest) {
    targs.push_back(te_to_term(tl->head, env, depth, pos));
  }
  term nt(texpr, targs);
  return eval_term_internal(nt, env, depth + 1, pos);
}
#endif

static term te_to_term(expr_i *te, env_type& env, size_t depth, expr_i *pos)
{
  expr_i *texpr = 0;
  term_list targs;
  expr_te *const tete = dynamic_cast<expr_te *>(te);
  if (tete != 0) {
#if 0
    if (tete->nssym->prefix == 0 && tete->nssym->sym[0] == ':') {
// FIXME: lazy evaluation
      return te_to_term_local_lookup(tete, env, depth, pos);
    }
#endif
#if 0
fprintf(stderr, "te_to_term tete->resolve_symdef %p begin\n", tete);
#endif
    texpr = tete->resolve_symdef(tete->symtbl_lexical);
#if 0
fprintf(stderr, "te_to_term tete->resolve_symdef %p end\n", tete);
#endif
    for (expr_telist *tl = tete->tlarg; tl != 0; tl = tl->rest) {
      targs.push_back(te_to_term(tl->head, env, depth, pos));
    }
  } else {
    texpr = te;
#if 0
fprintf(stderr, "te p=%p e=%d dump='%s'\n", texpr, texpr->get_esort(), texpr->dump(0).c_str()); // FIXME
if (texpr->dump(0).empty()) abort();
#endif
  }
  term r(texpr, targs);
#if 0
if (tete){
fprintf(stderr, "te_to_term tete=%p '%s' symdef=%p term='%s'\n", tete, tete->dump(0).c_str(), texpr, term_tostr_human(r).c_str()); //FIXME
if (texpr->get_esort() == expr_e_tparams) {
expr_tparams *tp = ptr_down_cast<expr_tparams>(texpr);
fprintf(stderr, "symdef(tparams)=%p param_def='%s'\n", texpr, term_tostr_human(tp->param_def).c_str());
}
}
#endif
  DBG_TETERM(fprintf(stderr, "te_to_term: [%s](%s:%d) -> [%s]\n",
    te->dump(0).c_str(), te->fname, te->line,
    term_tostr_human(r).c_str()));
  return r;
}

static term eval_te(expr_i *te, env_type& env, size_t depth, expr_i *pos)
{
  term nt = te_to_term(te, env, depth + 1, pos);
  DBG_EVAL(fprintf(stderr, "EVALI2 sort te\n"));
  return eval_term_internal(nt, env, depth + 1, pos);
}

static term eval_term_internal2(const term& t, env_type& env, size_t depth,
  expr_i *pos)
{
  DBG_INST(fprintf(stderr, "eval_te [%s]\n",
    term_tostr(t, term_tostr_sort_strict).c_str()));
  if (depth >= 10000) { /* TODO */
    arena_error_throw(pos, "recursion depth limit is exceeded");
  }
  expr_i *const texpr = t.get_expr();
  if (texpr == 0) {
    return t;
  }
  const term_list *const targs = t.get_args();
  const size_t tlarg_len = targs != 0 ? targs->size() : 0;
  switch (texpr->get_esort()) {
  case expr_e_te:
    {
      if (tlarg_len > 0) {
	arena_error_throw(pos, "too may template arguments: '%s'",
	  term_tostr(t, term_tostr_sort_humanreadable).c_str());
      }
      expr_te *const te = ptr_down_cast<expr_te>(texpr);
      return eval_te(te, env, depth + 1, pos);
      #if 0
      const term nt = te_to_term(te);
      DBG_EVAL(fprintf(stderr, "EVALI2 sort te\n"));
      return eval_term_internal(nt, env, depth + 1, pos);
      #endif
    }
    break;
  case expr_e_symbol:
    {
      if (tlarg_len > 0) {
	arena_error_throw(pos, "too may template arguments: '%s'",
	  term_tostr(t, term_tostr_sort_humanreadable).c_str());
      }
      expr_symbol *const esym = ptr_down_cast<expr_symbol>(texpr);
      expr_i *const symdef = esym->sdef.resolve_symdef();
      const term nt(symdef);
      return eval_term_internal(nt, env, depth + 1, pos);
    }
    break;
  case expr_e_tparams:
    {
      expr_tparams *const tp = ptr_down_cast<expr_tparams>(texpr);
      env_type::const_iterator iter = env.find(tp->sym);
      if (iter != env.end()) {
	return iter->second;
      }
      if (!tp->param_def.is_null()) {
	DBG_NEWTE(fprintf(stderr, "tparam %p concrete\n", tp));
	/* TODO: need to eval? */
	DBG_EVAL(fprintf(stderr, "EVALI2 sort tparams pdef\n"));
	return eval_term_internal(tp->param_def, env, depth + 1, pos);
      }
      DBG_EVAL(fprintf(stderr, "EVALI2 sort tparams nopdef (tp=%p)\n",
	tp));
      return t;
    }
    break;
  case expr_e_macrodef:
    {
      expr_macrodef *const ta = ptr_down_cast<expr_macrodef>(texpr);
      const size_t aplen = elist_length(ta->get_tparams());
      if (aplen != 0 && tlarg_len == 0) {
	return t; /* no argument is supplied yet */
      }
      if (aplen < tlarg_len) {
	arena_error_throw(pos, "too many template arguments");
      } else if (aplen > tlarg_len) {
	arena_error_throw(pos, "too few template arguments");
      }
      symbol_common *const sc = ta->get_rhs()->get_sdef();
      if (aplen == 0) {
	/* this macro has no param. if it's evaluated already, we use
	 * the evaluated value. */
	if (sc != 0 && !sc->resolve_evaluated().is_null()) {
	  return sc->resolve_evaluated();
	}
      }
      env_type nenv = env;
      expr_tparams *tpms = ta->get_tparams();
      for (term_list::const_iterator i = targs->begin(); i != targs->end();
	++i, tpms = tpms->rest) {
	nenv[std::string(tpms->sym)] =
	  eval_term_internal(*i, env, depth + 1, pos);
      }
      return eval_te(ta->get_rhs(), nenv, depth + 1, pos);
      #if 0
      term rhs = ta->rhs_term;
      if (rhs.is_null()) {
	rhs = ta->rhs_term = te_to_term(ta->get_rhs());
      }
      const term rt = eval_term_internal(rhs, nenv, depth + 1, pos);
      return rt;
      #endif
    }
    break;
  case expr_e_typedef:
    if (targs != 0 && !targs->empty()) {
      arena_error_throw(pos, "too many template arguments");
    }
    DBG_EVAL(fprintf(stderr, "EVALI2 sort typedef\n"));
    return t;
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
    break;
  }
  /* builtin template metafunction? */
  expr_struct *const es = dynamic_cast<expr_struct *>(texpr);
  if (es != 0 && es->category != 0 &&
    std::string(es->category).substr(0, 1) == "@") {
    if (tlarg_len == 0) {
      return t; /* no argument is supplied yet */
    }
    if (std::string(es->category).substr(0, 2) == "@@") {
      return eval_metafunction_lazy(es->category, t, env, depth, pos);
    } else {
      term_list tlev = eval_term_list_internal(targs, env, depth + 1, pos);
      if (has_unbound_tparam(tlev)) {
	DBG_EVAL(fprintf(stderr, "EVALI2 : has ubound tparam\n"));
	return t; /* incomplete expr */
      }
      return eval_metafunction(es->category, tlev, t, env, depth, pos);
    }
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
      return t; /* no argument is supplied yet */
    }
    if (tlarg_len > tparams_len) {
      arena_error_throw(pos, "too many template arguments: '%s'",
	term_tostr(t, term_tostr_sort_humanreadable).c_str());
    } else if (tlarg_len < tparams_len) {
      arena_error_throw(pos, "too few template arguments: '%s'",
	term_tostr(t, term_tostr_sort_humanreadable).c_str());
    }
    if (tparams_len != 0) {
      term_list tlev = eval_term_list_internal(targs, env, depth + 1, pos);
      DBG_INST(fprintf(stderr, "instantiate t=%s evaluated_args=%s\n",
	term_tostr_human(t).c_str(),
	term_tostr_list_human(tlev).c_str()));
      if (!has_unbound_tparam(tlev)) {
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
    return t;
  }
  return t;
}

static term eval_term_internal(const term& t, env_type& env, size_t depth,
  expr_i *pos)
{
  DBG_EVAL(fprintf(stderr, "EVALI %s BEGIN\n",
    term_tostr_human(t).c_str()));
  const term r = eval_term_internal2(t, env, depth, pos);
  DBG_EVAL(fprintf(stderr, "EVALI %s => %s\n",
    term_tostr_human(t).c_str(),
    term_tostr_human(r).c_str()));
  return r;
}

#if 0
term eval_term(const term& t, expr_i *pos)
{
  env_type env;
  return eval_term_internal(t, env, 0, pos);
}
#endif

term eval_expr(expr_i *e)
{
  const term t(e);
  env_type env;
  return eval_term_internal(t, env, 0, e);
}

term eval_local_lookup(const term& t, const std::string& name, expr_i *pos)
{
  env_type env;
  term_list tl;
  tl.push_back(t);
  tl.push_back(term(name));
  term r = eval_meta_local(tl, env, 0, pos);
  return r;
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

