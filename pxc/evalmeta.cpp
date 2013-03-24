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

#include "evalmeta.hpp"
#include <vector>
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
#define DBG_COMPILE(x)
#define DBG_TETERM(x)
#define DBG_META(x)
#define DBG_METAARGTYPE(x)
#define DBG_METASYM(x)
#define DBG_METACONCAT(x)
#define DBG_METALOCAL(x)
#define DBG_ATTR(x)
#define DBG_TYPEOF(x)

namespace pxc {

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
    arena_error_throw(pos, "meta_local: invalid type '%s'",
      term_tostr_human(typ).c_str());
  }
  if (name.find(':') != name.npos) {
    arena_error_throw(pos, "meta_local: invalid name '%s'", name.c_str());
  }
  symbol_table *const symtbl = &bl->symtbl;
  assert(symtbl);
  const std::string sym_ns;
  const bool no_private = true;
  bool is_global = false, is_upvalue = false, is_memfld = false;
  expr_i *const rsym = symtbl->resolve_name_nothrow(name, no_private, sym_ns,
    is_global, is_upvalue, is_memfld);
  DBG_METASYM(fprintf(stderr, "meta_local name=[%s] ns=[%s] rsym=%p\n",
    name.c_str(), sym_ns.c_str(), rsym));
  if (rsym == 0) {
    return term(); /* not found */
  }
  term_list rtargs;
  rtargs.insert(rtargs.begin(), tlev.begin() + 2, tlev.end());
  term rt(rsym, rtargs);
  return eval_term_internal(rt, true, env, depth + 1, pos);
}

static term eval_meta_symbol(term_list& tlev, env_type& env,
  size_t depth, expr_i *pos)
{
  if (tlev.size() < 1) {
    return term();
  }
  const std::string name = meta_term_to_string(tlev[0], false);
  if (name.find(':') != name.npos) {
    arena_error_throw(pos, "meta_symbol: invalid name '%s'", name.c_str());
  }
  std::string sym_ns;
  bool no_private = true;
  if (tlev.size() == 2) {
    /* find symbol from the specified symtbl */
    term& typ = tlev[1];
    expr_i *const typexpr = typ.get_expr();
    if (typexpr == 0 || !is_type_or_func_esort(typexpr->get_esort())) {
      arena_error_throw(pos, "meta_symbol: invalid type '%s'",
	term_tostr_human(typ).c_str());
    }
    sym_ns = typexpr->get_unique_namespace();
  } else {
    /* find symbol from the current symtbl */
    sym_ns = pos->get_unique_namespace();
  }
  symbol_table *const symtbl = &global_block->symtbl;
  assert(symtbl);
  if (loaded_namespaces.find(sym_ns) == loaded_namespaces.end()) {
    /* namespace sym_ns is not loaded. error. */
    /* note that metafunctions must be always pure functional, or generated
       template functions can be inconsistent among compilation units. we
       must generate an error here instead of returning 'notfound', in order
       to avoid such inconsistency. */
    DBG(fprintf(stderr, "not loaded: [%s] term=[%s]\n", sym_ns.c_str(),
      term_tostr_human(typ).c_str()));
    arena_error_throw(pos, "meta_symbol: namespace '%s' not imported",
      sym_ns.c_str());
  }
  bool is_global = false, is_upvalue = false, is_memfld = false;
  expr_i *const rsym = symtbl->resolve_name_nothrow(name, no_private, sym_ns,
    is_global, is_upvalue, is_memfld);
  DBG_METASYM(fprintf(stderr, "meta_symbol name=[%s] ns=[%s] rsym=%p\n",
    name.c_str(), sym_ns.c_str(), rsym));
  if (rsym == 0 || !is_global) {
    return term(0LL);
  }
  return term(rsym);
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
  return eval_term_internal(rt, true, env, depth + 1, pos);
}

static term eval_meta_error(term_list& tlev, env_type& env,
  size_t depth, expr_i *pos)
{
  if (tlev.size() < 1) {
    return term();
  }
  const std::string m = meta_term_to_string(tlev.front(), true);
  std::string s = std::string(pos->fname) + ":" + ulong_to_string(pos->line)
    + ": " + m + "\n";
  throw std::runtime_error(s);
}

static term eval_meta_is_copyable_type(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  term& ttyp = tlev[0];
  const long long r = is_copyable(ttyp);
  return term(r);
}

static term eval_meta_is_assignable_type(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  term& ttyp = tlev[0];
  const long long r = is_assignable(ttyp);
  return term(r);
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
    expr_argdecls *ad = efd->block->get_argdecls();
    const long long len = argdecls_length(ad);
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
    !is_type_or_func_esort(ttypexpr->get_esort())) {
    return term();
  }
  const long long n = meta_term_to_long(tnum);
  expr_i *const einst = term_get_instance(ttyp);
  DBG_METAARGTYPE(fprintf(stderr, "t.expr=%p inst=%p\n", t.expr, einst));
  expr_block *block = einst->get_template_block();
  DBG_METAARGTYPE(fprintf(stderr, "arg %d\n", n));
  expr_argdecls *ad = block->get_argdecls();
  for (int i = 0; i < n && ad != 0; ++i) {
    ad = ad->get_rest();
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
  expr_argdecls *ad = block->get_argdecls();
  for (int i = 0; i < n && ad != 0; ++i) {
    ad = ad->get_rest();
  }
  if (ad != 0) {
    return term(is_passby_cm_reference(ad->passby) ? 1 : 0);
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
  for (expr_argdecls *ad = block->get_argdecls(); ad != 0;
    ad = ad->get_rest()) {
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
  term_list tl;
  expr_block *block = einst->get_template_block();
  if (block != 0) {
    DBG_METAARGTYPE(fprintf(stderr, "arg %d\n", n));
    expr_argdecls *ad = block->get_argdecls();
    for (; ad != 0; ad = ad->get_rest()) {
      const std::string s(ad->sym);
      tl.push_back(term(s));
    }
  }
  return term(tl);
}

static term eval_meta_args(term_list& tlev)
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
  term_list tl;
  expr_block *block = einst->get_template_block();
  if (block != 0) {
    DBG_METAARGTYPE(fprintf(stderr, "arg %d\n", n));
    expr_argdecls *ad = block->get_argdecls();
    long long idx = 0;
    for (; ad != 0; ad = ad->get_rest(), ++idx) {
      term_list rec;
      const std::string s(ad->sym);
      rec.push_back(term(idx)); /* index */
      rec.push_back(term(s)); /* name */
      rec.push_back(ad->resolve_texpr()); /* type */
      rec.push_back(term(is_passby_cm_reference(ad->passby))); /* byref */
      rec.push_back(term(is_passby_mutable(ad->passby))); /* mutable */
      term e(rec);
      tl.push_back(e);
    }
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
    if (lv.has_attrib_private() || e->get_unique_namespace() != name) {
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
    if (lv.has_attrib_private() || e->get_unique_namespace() != name) {
      continue;
    }
    if (e == builtins.type_tpdummy.get_expr()) {
      continue;
    }
    expr_struct *const est = dynamic_cast<expr_struct *>(e);
    if (est != 0 && est->typefamily_str != 0) {
      const std::string s(est->typefamily_str);
      if (!s.empty() && s[0] == '@') {
	/* builtin metafunction */
	continue;
      }
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
  symbol_table *symtbl = 0;
  expr_struct *const est = dynamic_cast<expr_struct *>(einst);
  if (est != 0) {
    symtbl = &est->block->symtbl;
  }
  expr_interface *const ei = dynamic_cast<expr_interface *>(einst);
  if (ei != 0) {
    symtbl = &ei->block->symtbl;
  }
  if (symtbl != 0) {
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

static term eval_meta_values(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  expr_i *const einst = term_get_instance(tlev[0]);
  if (einst == 0) {
    return term();
  }
  if (einst->get_esort() != expr_e_typedef) {
    return term();
  }
  expr_typedef *const etd = ptr_down_cast<expr_typedef>(einst);
  if (!etd->is_enum && !etd->is_bitmask) {
    return term();
  }
  expr_enumval *vals = etd->enumvals;
  term_list tl;
  while (vals != 0) {
    tl.push_back(term(vals));
    vals = vals->rest;
  }
  return term(tl);
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

static term eval_meta_join(term_list& tlev, expr_i *pos)
{
  if (tlev.size() != 1) {
    arena_error_throw(pos, "join: invalid number of arguments");
    return term();
  }
  const term& t = tlev[0];
  if (!t.is_metalist()) {
    arena_error_throw(pos, "join: invalid argument");
    return term();
  }
  const term_list& tl = *t.get_metalist();
  term_list rtl;
  for (term_list::const_iterator i = tl.begin(); i != tl.end(); ++i) {
    const term_list *const il = i->get_metalist();
    if (il == 0) {
      arena_error_throw(pos, "join: invalid argument: %s",
	term_tostr_human(*i).c_str());
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

static term eval_meta_substring(term_list& tlev)
{
  if (tlev.size() < 2 || tlev.size() > 3) {
    return term();
  }
  const std::string s = meta_term_to_string(tlev[0], false);
  long i0 = meta_term_to_long(tlev[1]);
  if (i0 < 0) {
    i0 = 0;
  }
  if (tlev.size() == 3) {
    const long i1 = meta_term_to_long(tlev[2]);
    if (i1 <= i0) {
      return term(std::string());
    }
    return term(s.substr(i0, i1 - i0));
  } else {
    return term(s.substr(i0));
  }
}

static term eval_meta_attribute(term_list& tlev)
{
  if (tlev.size() != 2) {
    return term();
  }
  std::string s = meta_term_to_string(tlev[1], false);
  if (s == "ephemeral") {
    return term(is_ephemeral_value_type(tlev[0]));
  }
  return term();
}

static term eval_meta_family(term_list& tlev)
{
  if (tlev.size() != 1) {
    return term();
  }
  expr_i *const einst = term_get_instance(tlev[0]);
  expr_struct *const est = dynamic_cast<expr_struct *>(einst);
  if (est != 0 && est->typefamily_str != 0) {
    const std::string s(est->typefamily_str);
    if (!s.empty() && s[0] == '@') {
      return term("metafunction");
    }
    /* farray, varray, map, ptr, cptr, ... */
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
    {
      expr_typedef *const etd = ptr_down_cast<expr_typedef>(einst);
      if (etd->is_enum) {
	return term("enum");
      } else if (etd->is_bitmask) {
	return term("bitmask");
      }
    }
    return term("builtin");
  case expr_e_macrodef:
    return term("macro");
  case expr_e_funcdef:
    return term("function");
  case expr_e_var:
    return term("variable");
  case expr_e_argdecls:
    return term("argument");
  case expr_e_enumval:
    return term("enumvalue");
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

static term eval_meta_metamap(term_list& tlev, env_type& env, size_t depth,
  expr_i *pos)
{
  if (tlev.size() < 2) {
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
    itl.insert(itl.end(), tlev.begin() + 2, tlev.end()); /* opt_binds */
    itl.push_back(*i);
    term it(t1.get_expr(), itl);
    term tc = eval_term_internal(it, true, env, depth + 1, pos);
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
    term tc = eval_term_internal(it, true, env, depth + 1, pos);
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

term eval_metafunction_strict(const std::string& name, term_list& tlev,
  const term& t, env_type& env, size_t depth, expr_i *pos)
{
  DBG_META(fprintf(stderr, "eval_metafunction_strict: %s\n", name.c_str()));
  term r;
  try {
    /* TODO: use std::map */
    if (name == "@0void") {
      r = builtins.type_void;
    } else if (name == "@0unit") {
      r = builtins.type_unit;
    } else if (name == "@0bool") {
      r = builtins.type_bool;
    } else if (name == "@0uchar") {
      r = builtins.type_uchar;
    } else if (name == "@0char") {
      r = builtins.type_char;
    } else if (name == "@0ushort") {
      r = builtins.type_ushort;
    } else if (name == "@0short") {
      r = builtins.type_short;
    } else if (name == "@0uint") {
      r = builtins.type_uint;
    } else if (name == "@0int") {
      r = builtins.type_int;
    } else if (name == "@0ulong") {
      r = builtins.type_ulong;
    } else if (name == "@0long") {
      r = builtins.type_long;
    } else if (name == "@0size_t") {
      r = builtins.type_size_t;
    } else if (name == "@0float") {
      r = builtins.type_float;
    } else if (name == "@0double") {
      r = builtins.type_double;
    } else if (name == "@0string") {
      r = builtins.type_string;
    } else if (name == "@0strlit") {
      r = builtins.type_strlit;
    } else if (name == "@list") {
      r = eval_meta_list(tlev);
    } else if (name == "@at") {
      r = eval_meta_at(tlev);
    } else if (name == "@size") {
      r = eval_meta_size(tlev);
    } else if (name == "@seq") {
      r = eval_meta_seq(tlev);
    } else if (name == "@join") {
      r = eval_meta_join(tlev, pos);
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
    } else if (name == "@args") {
      r = eval_meta_args(tlev);
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
    } else if (name == "@is_copyable_type") {
      r = eval_meta_is_copyable_type(tlev);
    } else if (name == "@is_assignable_type") {
      r = eval_meta_is_assignable_type(tlev);
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
    } else if (name == "@values") {
      r = eval_meta_values(tlev);
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
    } else if (name == "@substring") {
      r = eval_meta_substring(tlev);
    } else if (name == "@family") {
      r = eval_meta_family(tlev);
    } else if (name == "@attribute") {
      r = eval_meta_attribute(tlev);
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
    } else if (name == "@metamap") {
      r = eval_meta_metamap(tlev, env, depth, pos);
    } else if (name == "@filter") {
      r = eval_meta_filter(tlev, env, depth, pos);
    } else if (name == "@local") {
      r = eval_meta_local(tlev, env, depth, pos);
    } else if (name == "@symbol") {
      r = eval_meta_symbol(tlev, env, depth, pos);
    } else if (name == "@apply") {
      r = eval_meta_apply(tlev, env, depth, pos);
    } else if (name == "@error") {
      r = eval_meta_error(tlev, env, depth, pos);
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

#if 0
static term eval_if_unevaluated(const term& t, bool evaluated_flag,
  env_type& env, size_t depth, expr_i *pos)
{
  if (evaluated_flag) {
    return t;
  } else {
    return eval_term_internal(t, false, env, depth, pos);
  }
}
#endif

static term eval_meta_metaif(const term& t, bool targs_evaluated,
  env_type& env, size_t depth, expr_i *pos)
{
  const term_list *args = t.get_args();
  if (args == 0) {
    arena_error_throw(pos, "metaif: invalid argument");
    return term();
  }
  const size_t num_args = args->size();
  if (num_args != 3) {
    arena_error_throw(pos, "metaif: invalid number of arguments (got: %zu)",
      num_args);
    return term();
  }
  term tc = eval_if_unevaluated((*args)[0], targs_evaluated, env, depth + 1,
    pos);
  if (has_unbound_tparam(tc)) {
    return t; /* incomplete expr */
  }
  const bool c = term_truth_value(tc);
  if (c) {
    return eval_if_unevaluated((*args)[1], targs_evaluated, env, depth + 1,
      pos);
  } else {
    return eval_if_unevaluated((*args)[2], targs_evaluated, env, depth + 1,
      pos);
  }
}

static term eval_meta_or(const term& t, bool targs_evaluated, env_type& env,
  size_t depth, expr_i *pos)
{
  const term_list *args = t.get_args();
  if (args == 0) {
    return term(0LL);
  }
  for (term_list::const_iterator i = args->begin(); i != args->end(); ++i) {
    term tc = eval_if_unevaluated(*i, targs_evaluated, env, depth + 1, pos);
    if (has_unbound_tparam(tc)) {
      return t;
    }
    if (term_truth_value(tc)) {
      return term(1LL);
    }
  }
  return term(0LL);
}

static term eval_meta_and(const term& t, bool targs_evaluated, env_type& env,
  size_t depth, expr_i *pos)
{
  const term_list *args = t.get_args();
  if (args == 0) {
    return term(1LL);
  }
  for (term_list::const_iterator i = args->begin(); i != args->end(); ++i) {
    term tc = eval_if_unevaluated(*i, targs_evaluated, env, depth + 1, pos);
    if (has_unbound_tparam(tc)) {
      return t;
    }
    if (!term_truth_value(tc)) {
      return term(0LL);
    }
  }
  return term(1LL);
}

static term eval_meta_typeof(const term& t, bool targs_evaluated,
  env_type& env, size_t depth, expr_i *pos)
{
  const term_list *args = t.get_args();
  if (args == 0 || args->size() != 1) {
    arena_error_throw(pos, "typeof: invalid argument");
    return term();
  }
  const term& t0 = args->front();
  expr_i *const e0 = t0.get_expr();
  if (e0 == 0) { /* TODO: integer etc */
    arena_error_throw(pos, "typeof: invalid argument");
    return term();
  }
  term rt = e0->resolve_texpr();
  DBG_TYPEOF(fprintf(stderr, "eval_meta_typeof: %s -> %s\n",
    term_tostr_human(t).c_str(), term_tostr_human(rt).c_str()));
  return rt;
}

term eval_metafunction_lazy(const std::string& name, const term& t,
  bool targs_evaluated, env_type& env, size_t depth, expr_i *pos)
{
  DBG_META(fprintf(stderr, "eval_metafunction_lazy: %s\n", name.c_str()));
  term r;
  try {
    if (name == "@@metaif") {
      r = eval_meta_metaif(t, targs_evaluated, env, depth, pos);
    } else if (name == "@@or") {
      r = eval_meta_or(t, targs_evaluated, env, depth, pos);
    } else if (name == "@@and") {
      r = eval_meta_and(t, targs_evaluated, env, depth, pos);
    } else if (name == "@@typeof") {
      r = eval_meta_typeof(t, targs_evaluated, env, depth, pos);
    }
  } catch (const std::exception& e) {
    std::string s = e.what();
    s += std::string(pos->fname) + ":" + ulong_to_string(pos->line)
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

term eval_local_lookup(const term& t, const std::string& name, expr_i *pos)
{
  env_type env;
  term_list tl;
  tl.push_back(t);
  tl.push_back(term(name));
  term r = eval_meta_local(tl, env, 0, pos);
  return r;
}

}; 

