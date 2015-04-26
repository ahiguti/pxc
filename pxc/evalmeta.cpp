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
#include <algorithm>
#include <sys/time.h>
#include "checktype.hpp"
#include "expr_misc.hpp"
#include "util.hpp"
#include "emit.hpp"

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

static term eval_meta_local_internal(const term_list_range& tlev,
  bool check_only, eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 2 && tlev.size() != 3) {
    return term();
  }
  term typ = tlev[0];
  const term defval = tlev.size() > 2 ? tlev[2] : term(0LL);
  const std::string name = meta_term_to_string(tlev[1], false);
  DBG_METALOCAL(fprintf(stderr, "eval_meta_local: '%s'\n", name.c_str()));
  expr_i *const einst = term_get_instance(typ);
  expr_block *const bl = einst != 0 ? einst->get_template_block() : 0;
  if (einst == 0 || bl == 0) {
    if (einst != 0 && is_type(typ)) {
      /* builtin type */
      return defval;
    }
    arena_error_throw(pos, "meta_local: Invalid type '%s'",
      term_tostr_human(typ).c_str());
  }
  if (name.find(':') != name.npos) {
    arena_error_throw(pos, "meta_local: Invalid name '%s'", name.c_str());
  }
  if (!bl->compiled_flag) {
    /* sanity check */
    arena_error_throw(pos,
      "meta_local: Internal error: type '%s' is not compiled yet",
      term_tostr_human(typ).c_str());
  }
  symbol_table *const symtbl = &bl->symtbl;
  assert(symtbl);
  const symbol sym_ns = get_lexical_context_ns(pos);
  const bool no_private = true;
  const bool no_memfld_generated = false;
  expr_i *const rsym = symtbl->resolve_name_nothrow_memfld(name, no_private,
    no_memfld_generated, sym_ns, pos);
  DBG_METASYM(fprintf(stderr, "meta_local name=[%s] ns=[%s] rsym=%p\n",
    name.c_str(), sym_ns.c_str(), rsym));
  if (rsym == 0) {
    /* not found */
    return defval;
  }
  if (check_only) {
    return term(1LL);
  }
  term rt(rsym);
  return eval_term_internal(rt, true, ectx, pos); /* is this necessary? */
}

static void check_public_namespace(const std::string& name)
{
  nspropmap_type::const_iterator i = nspropmap.find(name);
  if (i == nspropmap.end()) {
    arena_error_throw(0, "Namespace '%s' not imported", name.c_str());
  }
  if (!i->second.is_public) {
    arena_error_throw(0, "Namespace '%s' is private", name.c_str());
  }
}

static term eval_meta_symbol_global(const std::string& sym_ns,
  const std::string& name, const term& defval, bool check_only,
  eval_context& ectx, expr_i *pos)
{
  if (name.find(':') != name.npos) {
    arena_error_throw(pos, "meta_symbol: Invalid name '%s'", name.c_str());
  }
  symbol_table *const symtbl = &global_block->symtbl;
  assert(symtbl);
  check_public_namespace(sym_ns);
    /* note that metafunctions must be always pure functional, or generated
       template functions can be inconsistent among compilation units. we
       must generate an error here instead of returning 'notfound', in order
       to avoid such inconsistency. */
  bool no_private = true;
  expr_i *const rsym = symtbl->resolve_name_nothrow_ns(name, no_private,
    sym_ns, pos);
  DBG_METASYM(fprintf(stderr, "meta_symbol name=[%s] ns=[%s] rsym=%p[%s]\n",
    name.c_str(), sym_ns.c_str(), rsym, rsym ? rsym->dump(0).c_str() : ""));
  if (rsym == 0) {
    return defval;
  }
  if (rsym->generated_flag) {
    /* hide generated symbols in order to avoid inconsistency */
    return defval;
  }
  if (check_only) {
    return term(1LL);
  }
  term rt = term(rsym);
  return eval_term_internal(rt, true, ectx, pos); /* is this necessary? */
}

static term eval_meta_symbol_internal(const term_list_range& tlev,
  bool check_only, eval_context& ectx, expr_i *pos)
{
  std::string sym_ns;
  std::string name;
  if (tlev.size() != 2 && tlev.size() != 3) {
    return term();
  }
  if (tlev[0].is_string()) {
    /* global symbol */
    if (tlev.size() > 1 && !tlev[1].is_string()) {
      arena_error_throw(pos, "meta_symbol: Invalid argument '%s'", 
	term_tostr_human(tlev[1]).c_str());
    }
    if (tlev.size() == 2) {
      /* find symbol from the specified namespace */
      sym_ns = tlev[0].get_string();
      name = tlev[1].get_string();
    } else {
      return term();
    }
    const term defval = tlev.size() > 2 ? tlev[2] : term(0LL);
    return eval_meta_symbol_global(sym_ns, name, defval, check_only, ectx, pos);
  } else {
    /* local symbol */
    return eval_meta_local_internal(tlev, check_only, ectx, pos);
  }
}

static term eval_meta_symbol(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  return eval_meta_symbol_internal(tlev, false, ectx, pos);
}

static term eval_meta_symbol_exists(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 2) {
    return term();
  }
  return eval_meta_symbol_internal(tlev, true /* check_only */, ectx, pos);
}

static term eval_meta_apply_list(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 2 || !tlev[1].is_metalist()) {
    return term();
  }
  const term_list *const targs = tlev[1].get_metalist();
  return eval_apply(tlev[0], *targs, true, ectx, pos);
}

static term eval_meta_apply(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() < 1) {
    return term();
  }
  term t0 = tlev[0];
  term_list_range rtargs(&tlev[0] + 1, tlev.size() - 1);
  return eval_apply(t0, rtargs, true, ectx, pos);
}

static term eval_meta_error(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  const std::string m = meta_term_to_string(tlev[0], true);
  std::string s = std::string(pos->fname) + ":" + ulong_to_string(pos->line)
    + ": " + m + "\n";
  throw std::runtime_error(s);
}

static term eval_meta_is_copyable_type(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  const term& ttyp = tlev[0];
  const long long r = is_copyable(ttyp);
  return term(r);
}

static term eval_meta_is_movable_type(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  const term& ttyp = tlev[0];
  const long long r = is_movable(ttyp);
  return term(r);
}

static term eval_meta_is_assignable_type(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  const term& ttyp = tlev[0];
  const long long r = is_assignable(ttyp);
  return term(r);
}

static term eval_meta_is_constructible_type(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  const term& ttyp = tlev[0];
  const long long r = is_constructible(ttyp);
  return term(r);
}

static term eval_meta_is_default_constructible_type(
  const term_list_range& tlev, eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  const term& ttyp = tlev[0];
  const long long r = is_default_constructible(ttyp);
  return term(r);
}

static term eval_meta_is_polymorphic_type(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  const term& ttyp = tlev[0];
  const long long r = is_polymorphic(ttyp);
  return term(r);
}

static term eval_meta_inherits(const term_list_range& tlev, eval_context& ectx, 
  expr_i *pos)
{
  if (tlev.size() != 2) {
    return term();
  }
  const term& tderived = tlev[0];
  const term& tbase = tlev[1];
  const long long r = is_sub_type(tderived, tbase);
  return term(r);
}

static term eval_meta_base_types(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  term tbase = tlev[0];
  expr_i *const einst = term_get_instance(tbase);
  if (einst == 0) {
    return term();
  }
  expr_block *block = einst->get_template_block();
  if (block == 0) {
    return term();
  }
  expr_block::inherit_list_type& il = block->resolve_inherit_transitive();
  term_list tl;
  for (expr_block::inherit_list_type::const_iterator i = il.begin();
    i != il.end(); ++i) {
    term te = (*i)->get_value_texpr();
    tl.push_back(te);
  }
  return term(tl);
}

static term eval_meta_arg_size(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  term ttyp = tlev[0];
  expr_i *const ttypexpr = ttyp.get_expr();
  if (ttypexpr == 0 ||
    !is_type_or_func_esort(ttypexpr->get_esort())) {
    return term();
  }
  expr_i *const einst = term_get_instance(ttyp);
  DBG_METAARGTYPE(fprintf(stderr, "t.expr=%p inst=%p\n", t.expr, einst));
  expr_block *block = einst->get_template_block();
  if (block == 0) {
    return term(0LL);
  }
  expr_argdecls *ad = block->get_argdecls();
  const long long len = argdecls_length(ad);
  return term(len);
}

static term eval_meta_ret_common(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos, int idx)
{
  if (tlev.size() != 1) {
    return term();
  }
  term ttyp = tlev[0];
  expr_i *const ttypexpr = ttyp.get_expr();
  if (ttypexpr == 0 ||
    !is_type_or_func_esort(ttypexpr->get_esort())) {
    return term();
  }
  expr_i *const einst = term_get_instance(ttyp);
  expr_funcdef *const efd = dynamic_cast<expr_funcdef *>(einst);
  if (efd != 0) {
    switch (idx) {
    case -1: break;
    case 1: return efd->get_rettype();
    case 2: return term(is_passby_cm_reference(efd->block->ret_passby)
      ? 1 : 0);
    case 3: return term(is_passby_mutable(efd->block->ret_passby) ? 1 : 0);
    default: abort();
    }
    term r[4];
    r[0] = term("");
    r[1] = efd->get_rettype();
    r[2] = term(is_passby_cm_reference(efd->block->ret_passby) ? 1 : 0);
    r[3] = term(is_passby_mutable(efd->block->ret_passby) ? 1 : 0);
    return term(term_list_range(r, 4));
  } else if (is_type(ttyp)) {
    switch (idx) {
    case -1: break;
    case 1: return ttyp;
    case 2: return term(0LL);
    case 3: return term(0LL);
    default: abort();
    }
    term r[4];
    r[0] = term("");
    r[1] = ttyp;
    r[2] = term(0LL);
    r[3] = term(0LL);
    return term(term_list_range(r, 4));
  }
  return term();
}

static term eval_meta_ret_type(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  return eval_meta_ret_common(tlev, ectx, pos, 1);
}

static term eval_meta_ret_byref(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  return eval_meta_ret_common(tlev, ectx, pos, 2);
}

static term eval_meta_ret_mutable(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  return eval_meta_ret_common(tlev, ectx, pos, 3);
}

static term eval_meta_ret(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  return eval_meta_ret_common(tlev, ectx, pos, -1);
}

static term eval_meta_arg_common(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos, int idx)
{
  if (tlev.size() != 2) {
    return term();
  }
  term ttyp = tlev[0];
  const term& tnum = tlev[1];
  expr_i *const ttypexpr = ttyp.get_expr();
  if (ttypexpr == 0 ||
    !is_type_or_func_esort(ttypexpr->get_esort())) {
    return term();
  }
  const long long n = meta_term_to_long(tnum);
  expr_i *const einst = term_get_instance(ttyp);
  DBG_METAARGTYPE(fprintf(stderr, "t.expr=%p inst=%p\n", t.expr, einst));
  expr_block *block = einst->get_template_block();
  if (block == 0) {
    return term();
  }
  DBG_METAARGTYPE(fprintf(stderr, "arg %d\n", n));
  expr_argdecls *ad = block->get_argdecls();
  for (int i = 0; i < n && ad != 0; ++i) {
    ad = ad->get_rest();
  }
  if (ad == 0) {
    return term();
  }
  switch (idx) {
  case -1: break;
  case 0: return term(std::string(ad->sym));
  case 1: return ad->resolve_texpr();
  case 2: return term(is_passby_cm_reference(ad->passby) ? 1 : 0);
  case 3: return term(is_passby_mutable(ad->passby) ? 1 : 0);
  default: abort();
  }
  term r[4];
  r[0] = term(std::string(ad->sym));
  r[1] = ad->resolve_texpr();
  r[2] = term(is_passby_cm_reference(ad->passby) ? 1 : 0);
  r[3] = term(is_passby_mutable(ad->passby) ? 1 : 0);
  return term(term_list_range(r, 4));
}

static term eval_meta_arg_type(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  return eval_meta_arg_common(tlev, ectx, pos, 1);
}

static term eval_meta_arg_byref(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  return eval_meta_arg_common(tlev, ectx, pos, 2);
}

static term eval_meta_arg_mutable(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  return eval_meta_arg_common(tlev, ectx, pos, 3);
}

static term eval_meta_arg_types(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  term ttyp = tlev[0];
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
  if (block != 0) {
    for (expr_argdecls *ad = block->get_argdecls(); ad != 0;
      ad = ad->get_rest()) {
      tl.push_back(ad->resolve_texpr());
    }
  }
  return term(tl);
}

static term eval_meta_arg_names(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  term ttyp = tlev[0];
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

static term eval_meta_args(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  term ttyp = tlev[0];
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

static term eval_meta_imports(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  const std::string name = meta_term_to_string(tlev[0], false);
  check_public_namespace(name);
  loaded_namespaces_type::const_iterator iter = loaded_namespaces.find(name);
  if (iter == loaded_namespaces.end()) {
    /* not reached */
    arena_error_throw(0, "Namespace '%s' not imported", name.c_str());
  }
  const imports_type::deps_type& deps = iter->second.deps;
  term_list tl;
  for (imports_type::deps_type::const_iterator i = deps.begin();
    i != deps.end(); ++i) {
    const std::string tns = i->ns;
    nspropmap_type::const_iterator j = nspropmap.find(tns);
    if (j == nspropmap.end()) {
      arena_error_throw(pos, "Internal error: namespace '%s' not imported",
	tns.c_str());
    }
    if (!j->second.is_public) {
      continue;
    }
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
    arena_error_throw(0, "Internal error: namespace not found: '%s'",
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

static term eval_meta_functions(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  const std::string name = meta_term_to_string(tlev[0], false);
  check_public_namespace(name);
  symbol_table *const symtbl = &global_block->symtbl;
  std::list< std::pair<symbol, localvar_info> > syms;
  symtbl->get_ns_symbols(name, syms);
  term_list tl;
  for (std::list< std::pair<symbol, localvar_info> >::const_iterator i
    = syms.begin(); i != syms.end(); ++i)
  {
    const localvar_info& lv = i->second;
    expr_i *const e = lv.edef;
    if (lv.has_attrib_private()) {
      continue;
    }
    if (e->get_esort() == expr_e_funcdef) {
      tl.push_back(term(e));
    }
  }
  return term(tl);
}

static term eval_meta_types(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  const std::string name = meta_term_to_string(tlev[0], false);
  check_public_namespace(name);
  symbol_table *const symtbl = &global_block->symtbl;
  std::list< std::pair<symbol, localvar_info> > syms;
  symtbl->get_ns_symbols(name, syms);
  term_list tl;
  for (std::list< std::pair<symbol, localvar_info> >::const_iterator i
    = syms.begin(); i != syms.end(); ++i)
  {
    const localvar_info& lv = i->second;
    expr_i *const e = lv.edef;
    if (lv.has_attrib_private()) {
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
    case expr_e_dunion:
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

static term eval_meta_global_variables(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  const std::string name = meta_term_to_string(tlev[0], false);
  check_public_namespace(name);
  symbol_table *const symtbl = &global_block->symtbl;
  term_list tl;
  long long idx = 0;
  std::list< std::pair<symbol, localvar_info> > syms;
  symtbl->get_ns_symbols(name, syms);
  for (std::list< std::pair<symbol, localvar_info> >::const_iterator i
    = syms.begin(); i != syms.end(); ++i)
  {
    const localvar_info& lv = i->second;
    expr_i *const e = lv.edef;
    term_list rec;
    std::string name;
    bool is_mutable = false;
    bool is_byref = false;
    if (e->get_esort() == expr_e_var) {
      expr_var *ev = ptr_down_cast<expr_var>(e);
      name = std::string(ev->sym);
      is_mutable = is_passby_mutable(ev->varinfo.passby);
    } else if (e->get_esort() == expr_e_enumval) {
      expr_enumval *ee = ptr_down_cast<expr_enumval>(e);
      name = std::string(ee->sym);
      is_mutable = false;
    } else {
      continue;
    }
    rec.push_back(term(name)); /* name */
    rec.push_back(e->resolve_texpr()); /* type */
    rec.push_back(term(is_byref)); /* byref(=0) */
    rec.push_back(term(is_mutable)); /* mutable */
    term re(rec);
    tl.push_back(re);
    ++idx;
  }
  return term(tl);
}

static term eval_meta_member_functions(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  term ttyp = tlev[0];
  expr_i *const einst = term_get_instance(ttyp);
  if (!is_type_esort(einst->get_esort())) {
    return term();
  }
  term_list tl;
  expr_block *blk = 0;
  expr_struct *const est = dynamic_cast<expr_struct *>(einst);
  if (est != 0) {
    blk = est->block;
  }
  expr_interface *const ei = dynamic_cast<expr_interface *>(einst);
  if (ei != 0) {
    blk = ei->block;
  }
  if (blk != 0) {
    symbol_table *symtbl = &blk->symtbl;
    symbol_table::local_names_type::const_iterator i;
    symbol_table::local_names_type const& local_names
      = symtbl->get_local_names();
    for (i = local_names.begin(); i != local_names.end(); ++i) {
      symbol_table::locals_type::const_iterator j = symtbl->find(*i, false);
	/* incl generated symbols */
      assert(j != symtbl->end());
      const localvar_info lv = j->second;
      expr_i *const e = lv.edef;
      /* no need to skip generated symbols because this metafunctions is
       * allowed only for a compiled block. */
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

template <int mask> static term
eval_meta_fields_internal(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  expr_block *blk = 0;
  std::list<expr_var *> flds;
  term ttyp = tlev[0];
  expr_i *const einst = term_get_instance(ttyp);
  expr_struct *const est = dynamic_cast<expr_struct *>(einst);
  if (est != 0) {
    est->get_fields(flds);
    blk = est->block;
  }
  expr_dunion *const ev = dynamic_cast<expr_dunion *>(einst);
  if (ev != 0) {
    ev->get_fields(flds);
    blk = ev->block;
  }
  if (blk != 0 && !blk->compiled_flag) {
    /* necessary for referential transparency */
    const char *sym = est != 0 ? est->sym : ev->sym;
    arena_error_throw(blk,
      "Failed to enumerate member fields of '%s' "
      "which is not compiled yet", sym);
  }
  term_list tl;
  for (std::list<expr_var *>::const_iterator i = flds.begin(); i != flds.end();
    ++i) {
    if (((*i)->get_attribute() & attribute_public) == 0) {
      continue;
    }
    if (mask == 0x3) {
      term_list tl1;
      tl1.push_back(term(std::string((*i)->sym))); /* name */
      tl1.push_back((*i)->resolve_texpr()); /* type */
      tl1.push_back(term(0LL)); /* byref(=0) */
      tl1.push_back(term(is_passby_mutable((*i)->varinfo.passby)));
	/* mutable */
      term t1(tl1);
      tl.push_back(t1);
    } else if (mask == 0x1) {
      tl.push_back(term(std::string((*i)->sym))); /* name */
    } else if (mask == 0x2) {
      tl.push_back((*i)->resolve_texpr()); /* type */
    }
  }
  return term(tl);
}

static term eval_meta_field_names(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  return eval_meta_fields_internal<0x1>(tlev, ectx, pos);
}

static term eval_meta_field_types(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  return eval_meta_fields_internal<0x2>(tlev, ectx, pos);
}

static term eval_meta_fields(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  return eval_meta_fields_internal<0x3>(tlev, ectx, pos);
}

static term eval_meta_tparam_size(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
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

static term eval_meta_targ_size(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
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

static term eval_meta_targs(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
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

static term eval_meta_values(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  term ttyp = tlev[0];
  expr_i *const einst = term_get_instance(ttyp);
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

static term eval_meta_typeof(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() != 1) {
    arena_error_throw(pos, "typeof: Invalid argument");
    return term();
  }
  term ttyp = tlev[0];
  expr_i *const e0 = term_get_instance(ttyp);
  if (e0 == 0) { /* TODO: integer etc */
    arena_error_throw(pos, "typeof: Invalid argument");
    return term();
  }
  term rt = e0->resolve_texpr();
  DBG_TYPEOF(fprintf(stderr, "eval_meta_typeof: %s -> %s\n",
    term_tostr_human(t).c_str(), term_tostr_human(rt).c_str()));
  return rt;
}

static term eval_meta_list(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  return term(tlev);
}

static term eval_meta_at(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
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

static term eval_meta_size(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
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

static term eval_meta_slice(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() != 2 && tlev.size() != 3) {
    return term();
  }
  const term& t = tlev[0];
  const term_list *const targs = t.is_metalist()
    ? t.get_metalist() : t.get_args();
  if (targs == 0) {
    return term();
  }
  unsigned long long v0 = meta_term_to_long(tlev[1]);
  if (targs->size() <= v0) {
    v0 = targs->size();
  }
  unsigned long long v1 =
    (tlev.size() == 3) ? meta_term_to_long(tlev[2]) : targs->size();
  if (targs->size() <= v1) {
    v1 = targs->size();
  }
  if (v0 >= v1) {
    v1 = v0;
  }
  term_list tl(targs->begin() + v0, targs->begin() + v1);
  return term(tl);
}

static term eval_meta_seq(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
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

static term eval_meta_join_list(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 1 && tlev.size() != 2) {
    arena_error_throw(pos, "join_list: Invalid number of arguments");
    return term();
  }
  const term& t = tlev[0];
  if (!t.is_metalist()) {
    arena_error_throw(pos, "join_list: Invalid argument");
    return term();
  }
  const term_list& tl = *t.get_metalist();
  term_list rtl;
  for (term_list::const_iterator i = tl.begin(); i != tl.end(); ++i) {
    if (tlev.size() == 2 && i != tl.begin()) {
      rtl.push_back(tlev[1]);
    }
    const term_list *const il = i->get_metalist();
    if (il == 0) {
      arena_error_throw(pos, "join: Invalid argument: %s",
	term_tostr_human(*i).c_str());
      return term();
    }
    rtl.insert(rtl.end(), il->begin(), il->end());
  }
  return term(rtl);
}

static term eval_meta_join(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  term_list rtl;
  for (term_list_range::const_iterator i = tlev.begin(); i != tlev.end();
    ++i) {
    const term_list *const il = i->get_metalist();
    if (il == 0) {
      return term();
    }
    rtl.insert(rtl.end(), il->begin(), il->end());
  }
  return term(rtl);
}

static term eval_meta_transposev(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  std::vector<term_list> mat;
  for (size_t i = 0; i < tlev.size(); ++i) {
    if (!tlev[i].is_metalist()) {
      return term();
    }
    const term_list& tl1 = *tlev[i].get_metalist();
    for (size_t j = 0; j < tl1.size(); ++j) {
      if (mat.size() <= j) {
	mat.push_back(term_list());
      }
      term_list& rtl1 = mat[j];
      rtl1.push_back(tl1[j]);
    }
  }
  term_list rtl;
  for (size_t i = 0; i < mat.size(); ++i) {
    const term e(mat[i]);
    rtl.push_back(e);
  }
  return term(rtl);
}

static term eval_meta_transpose(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 1 || !tlev[0].is_metalist()) {
    return term();
  }
  const term_list& tl = *tlev[0].get_metalist();
  return eval_meta_transposev(tl, ectx, pos);
}

static term eval_meta_sort(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() != 1 || !tlev[0].is_metalist()) {
    return term();
  }
  const term_list& tl = *tlev[0].get_metalist();
  term_list rtl = tl;
  std::stable_sort(rtl.begin(), rtl.end());
  return term(rtl);
}

static term eval_meta_unique(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
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

static term eval_meta_list_index(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  const term& t = tlev[0];
  if (!t.is_metalist()) {
    return term();
  }
  if (t.has_index()) {
    return t;
  }
  const term_list& tl = *t.get_metalist();
  return term(tl, true); /* list with index */
}

static term eval_meta_list_find(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 2) {
    return term();
  }
  const term& t = tlev[0];
  if (!t.is_metalist()) {
    return term();
  }
  if (!tlev[1].is_long() && !tlev[1].is_string()) {
    return term();
  }
  long v = t.assoc_find(tlev[1]);
  return term(v);
}

static term eval_meta_base(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  const term& t = tlev[0];
  if (!t.is_expr()) {
    return term();
  }
  expr_i *const e = t.get_expr();
  return term(e);
}

static term eval_meta_is_type(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  const long long v = is_type(tlev[0]);
  return term(v);
}

static term eval_meta_is_function(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  const long long v = is_function(tlev[0]);
  return term(v);
}

static term eval_meta_is_member_function(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  const expr_funcdef *const efd = dynamic_cast<const expr_funcdef *>(
    tlev[0].get_expr());
  long long v = 0;
  if (efd != 0) {
    v = (efd->is_virtual_or_member_function() != 0);
  }
  return term(v);
}

static term eval_meta_is_mutable_member_function(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  const expr_funcdef *const efd = dynamic_cast<const expr_funcdef *>(
    tlev[0].get_expr());
  long long v = 0;
  if (efd != 0) {
    v = (efd->is_virtual_or_member_function() != 0) && !(efd->is_const);
  }
  return term(v);
}

static term eval_meta_is_const_member_function(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  const expr_funcdef *const efd = dynamic_cast<const expr_funcdef *>(
    tlev[0].get_expr());
  long long v = 0;
  if (efd != 0) {
    v = (efd->is_virtual_or_member_function() != 0) && (efd->is_const);
  }
  return term(v);
}

static term eval_meta_is_metafunction(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
 const expr_metafdef *const em  = dynamic_cast<const expr_metafdef *>(
    tlev[0].get_expr());
  return em != 0 ? term(1LL) : term(0LL);
}

static term eval_meta_is_int(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  const long long v = tlev[0].is_long();
  return term(v);
}

static term eval_meta_is_string(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  const long long v = tlev[0].is_string();
  return term(v);
}

static term eval_meta_is_list(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  const long long v = tlev[0].is_metalist();
  return term(v);
}

static term eval_meta_to_int(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
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

static term eval_meta_to_string(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
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

static term eval_meta_full_string(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
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

static term eval_meta_concat(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  std::string s;
  for (term_list_range::const_iterator i = tlev.begin(); i != tlev.end();
    ++i) {
    s += meta_term_to_string(*i, false);
  }
  DBG_METACONCAT(fprintf(stderr, "meta_concat r=[%s]\n", s.c_str()));
  return term(s);
}

static term eval_meta_substring(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
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

static term eval_meta_subst(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() > 3) {
    return term();
  }
  const std::string str = meta_term_to_string(tlev[0], false);
  const std::string sfrom = meta_term_to_string(tlev[1], false);
  const std::string sto = meta_term_to_string(tlev[2], false);
  if (sfrom.size() == 0) {
    return tlev[0];
  }
  const char *const str_ptr = str.data();
  size_t str_size = str.size();
  const char *const from_ptr = sfrom.data();
  size_t from_size = sfrom.size();
  size_t const midx = str_size - from_size;
  std::string r;
  for (size_t i = 0; i < str_size; ++i) {
    if (i < midx && memcmp(str_ptr + i, from_ptr, from_size) == 0) {
      r += sto;
      i += from_size - 1;
    } else {
      r.push_back(str_ptr[i]);
    }
  }
  return term(r);
}

static term eval_meta_strlen(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  const std::string s = meta_term_to_string(tlev[0], false);
  const long long v = s.size();
  return term(v);
}

static term eval_meta_character(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  std::string str;
  const term& te = tlev[0];
  if (te.is_metalist()) {
    const term_list& tl = *te.get_metalist();
    for (size_t i = 0; i < tl.size(); ++i) {
      if (!tl[i].is_long()) {
	return term();
      }
      long long v = meta_term_to_long(tl[i]);
      if (v < 0 || v > 255) {
	return term();
      }
      unsigned char cv = v;
      str.push_back(cv);
    }
  } else if (te.is_long()) {
    long long v = meta_term_to_long(te);
    if (v < 0 || v > 255) {
      return term();
    }
    unsigned char cv = v;
    str.push_back(cv);
  } else {
    return term();
  }
  return term(str);
}

static term eval_meta_code_at(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() != 2 || !tlev[0].is_string() || !tlev[1].is_long()) {
    return term();
  }
  const std::string s = tlev[0].get_string();
  const long long idx = tlev[1].get_long();
  if (idx < 0 || static_cast<unsigned long long>(idx) >= s.size()) {
    return term();
  }
  const unsigned char ch = s[idx];
  const long long lch = ch;
  return term(lch);
}

template <bool right> static term eval_meta_strchr(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 2) {
    return term();
  }
  const std::string s = meta_term_to_string(tlev[0], false);
  const long long ch = meta_term_to_long(tlev[1]);
  std::string::size_type p = 0;
  if (right) {
    p = s.rfind(ch);
  } else {
    p = s.find(ch);
  }
  if (p == s.npos) {
    return term(-1LL);
  } else {
    return term(static_cast<long long>(p));
  }
}

static term eval_meta_characteristic(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 2) {
    return term();
  }
  std::string s = meta_term_to_string(tlev[1], false);
  if (s == "noheap") {
    return term(is_noheap_type(tlev[0]));
  } else if (s == "defcon") {
    return term(is_default_constructible(tlev[0]));
  } else if (s == "copyable") {
    return term(is_copyable(tlev[0]));
  } else if (s == "threaded") {
    attribute_e attr = get_term_threading_attribute(tlev[0]);
    return term((attr & attribute_threaded) != 0);
  }
  return term();
}

static term eval_meta_family(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  term ttyp = tlev[0];
  expr_i *const einst = term_get_instance(ttyp);
  if (einst == 0) {
    return term();
  }
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
  case expr_e_dunion:
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
  case expr_e_metafdef:
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

static term eval_meta_nameof(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  expr_i *const e = tlev[0].get_expr();
  if (e == 0) {
    return term();
  }
  const term t(e); /* drop params */
  std::string s(term_tostr_human(t));
  std::string::size_type p = s.find('{');
  if (p != s.npos) {
    s = s.substr(0, p); /* drop { ... } */
  }
  return term(s);
}

static term eval_meta_nsof(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  expr_i *const e = tlev[0].get_expr();
  if (e == 0) {
    return term();
  }
  const term t(e); /* drop params */
  std::string s(term_tostr_human(t));
  std::string::size_type p = s.rfind(':');
  if (p != s.npos && p > 1) {
    s = s.substr(0, p - 1);
  }
  return term(s);
}

static term eval_meta_is_safe_ns(const term_list_range& tlev,
  eval_context& ectx, expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  const std::string s = meta_term_to_string(tlev[0], false);
  bool v = is_safe_namespace(s);
  return term(v);
}

static term eval_meta_not(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  const bool c = term_truth_value(tlev[0]);
  return term(c ? 0LL : 1LL);
}

static term eval_meta_eq(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  /* TODO: lazy? */
  int v = 1;
  if (tlev.size() < 1) {
    v = 1;
  } else {
    const term& t = tlev[0];
    for (term_list_range::const_iterator i = tlev.begin() + 1; i != tlev.end();
      ++i) {
      if (t != *i) {
	v = 0;
	break;
      }
    }
  }
  return term(v);
}

static term eval_meta_add(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  long long v = 0;
  for (term_list_range::const_iterator i = tlev.begin(); i != tlev.end();
    ++i) {
    v += meta_term_to_long(*i);
  }
  return term(v);
}

static term eval_meta_sub(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() != 2) {
    return term();
  }
  return term(meta_term_to_long(tlev[0]) - meta_term_to_long(tlev[1]));
}

static term eval_meta_mul(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  long long v = 1;
  for (term_list_range::const_iterator i = tlev.begin(); i != tlev.end();
    ++i) {
    v *= meta_term_to_long(*i);
  }
  return term(v);
}

static term eval_meta_div(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() != 2) {
    return term();
  }
  const long long v0 = meta_term_to_long(tlev[0]);
  const long long v1 = meta_term_to_long(tlev[1]);
  if (v1 == 0) {
    arena_error_throw(pos, "Division by zero: div{%lld, 0}", v0);
  }
  return term(v0 / v1);
}

static term eval_meta_mod(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() != 2) {
    return term();
  }
  const long long v0 = meta_term_to_long(tlev[0]);
  const long long v1 = meta_term_to_long(tlev[1]);
  if (v1 == 0) {
    arena_error_throw(pos, "Division by zero: mod{%lld, 0}", v0);
    return term();
  }
  return term(v0 % v1);
}

static term eval_meta_gt(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() != 2) {
    return term();
  }
  return term(meta_term_to_long(tlev[0]) > meta_term_to_long(tlev[1]));
}

static term eval_meta_lt(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() != 2) {
    return term();
  }
  return term(meta_term_to_long(tlev[0]) < meta_term_to_long(tlev[1]));
}

static term eval_meta_csymbol(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() == 1) {
    const std::string s = term_tostr(tlev[0], term_tostr_sort_cname);
    return term(s);
  } else if (tlev.size() == 2) {
    long fldnum = meta_term_to_long(tlev[1]);
    const expr_i *const einst = term_get_instance_const(tlev[0]);
    typedef std::list<expr_var *> flds_type;
    flds_type flds;
    einst->get_fields(flds);
    long j = 0;
    expr_var *v = 0;
    for (flds_type::const_iterator i = flds.begin(); i != flds.end();
      ++i, ++j) {
      if (j == fldnum) {
	v = *i;
	break;
      }
    }
    if (v != 0) {
      return term(csymbol_var(v, false));
    }
  }
  return term();
}

static term eval_meta_profile(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() != 1) {
    return term();
  }
  std::string k = meta_term_to_string(tlev[0], false);
  if (cur_profile == 0) {
    arena_error_throw(pos, "internal error: cur_profile");
  }
  std::map<std::string, std::string>::const_iterator i = cur_profile->find(k);
  if (i != cur_profile->end()) {
    return term(i->second);
  }
  return term("");
}

static term eval_meta_map(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() != 2 || !tlev[0].is_metalist()) {
    return term();
  }
  const term_list& tl = *tlev[0].get_metalist();
  const term& tfunc = tlev[1];
  term_list rtl(tl.size());
  for (size_t i = 0; i < tl.size(); ++i) {
    const term& arg = tl[i];
    term tc = eval_apply(tfunc, term_list_range(&arg, 1), true, ectx, pos);
    rtl[i] = tc;
  }
  return term(rtl);
}

static term eval_meta_fold(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() != 3) {
    return term();
  }
  if (!tlev[0].is_metalist()) {
    return term();
  }
  const term& mf = tlev[1];
  term val = tlev[2];
  const term_list& tl = *tlev[0].get_metalist();
  for (term_list::const_iterator i = tl.begin(); i != tl.end(); ++i) {
    term ta[2];
    ta[0] = val;
    ta[1] = *i;
    val = eval_apply(mf, term_list_range(ta, 2), true, ectx, pos);
  }
  return val;
}

static term eval_meta_filter(const term_list_range& tlev, eval_context& ectx,
  expr_i *pos)
{
  if (tlev.size() != 2) {
    return term();
  }
  const term& t0 = tlev[0]; /* list */
  const term& t1 = tlev[1]; /* function */
  if (!t0.is_metalist()) {
    arena_error_throw(0, "Not a list: '%s'", term_tostr_human(t0).c_str());
  }
  #if 0
  if (!t1.is_expr()) {
    arena_error_throw(0, "Not a symbol: '%s'", term_tostr_human(t1).c_str());
  }
  if (t1.get_args() != 0 && !t1.get_args()->empty()) {
    arena_error_throw(0, "Not a symbol: '%s'", term_tostr_human(t1).c_str());
  }
  #endif
  const term_list& tl = *t0.get_metalist();
  term_list rtl;
  for (term_list::const_iterator i = tl.begin(); i != tl.end(); ++i) {
    term_list itl;
    itl.push_back(*i);
    term tc = eval_apply(t1, itl, true, ectx, pos);
    #if 0
    term it(t1.get_expr(), itl);
    term tc = eval_term_internal(it, true, ectx, pos);
    #endif
    const long long  v = meta_term_to_long(tc);
    if (v != 0) {
      rtl.push_back(*i);
    }
  }
  return term(rtl);
}

static term eval_meta_cond(expr_i *texpr, const term_list_range& targs,
  bool targs_evaluated, eval_context& ectx, expr_i *pos)
{
  const size_t num_args = targs.size();
  if (num_args % 2 != 1) {
    arena_error_throw(pos, "cond: Invalid number of arguments (got: %zu)",
      num_args);
    return term();
  }
  for (size_t i = 0; i + 1 < num_args; i += 2) {
    term tc = eval_if_unevaluated((targs)[i], targs_evaluated, ectx, pos);
    if (is_partial_eval_uneval(tc, ectx)) {
      return term(texpr, targs); /* incomplete expr */
    }
    const bool c = term_truth_value(tc);
    if (c) {
      return eval_if_unevaluated(targs[i + 1], targs_evaluated, ectx, pos);
    }
  }
  return eval_if_unevaluated(targs[num_args - 1], targs_evaluated, ectx, pos);
}

static term eval_meta_or(expr_i *texpr, const term_list_range& targs,
  bool targs_evaluated, eval_context& ectx, expr_i *pos)
{
  for (term_list_range::const_iterator i = targs.begin(); i != targs.end();
    ++i) {
    term tc = eval_if_unevaluated(*i, targs_evaluated, ectx, pos);
    if (is_partial_eval_uneval(tc, ectx)) {
      return term(texpr, targs);
    }
    if (term_truth_value(tc)) {
      return term(1LL);
    }
  }
  return term(0LL);
}

static term eval_meta_and(expr_i *texpr, const term_list_range& targs,
  bool targs_evaluated, eval_context& ectx, expr_i *pos)
{
  for (term_list_range::const_iterator i = targs.begin(); i != targs.end();
    ++i) {
    term tc = eval_if_unevaluated(*i, targs_evaluated, ectx, pos);
    if (is_partial_eval_uneval(tc, ectx)) {
      return term(texpr, targs);
    }
    if (!term_truth_value(tc)) {
      return term(0LL);
    }
  }
  return term(1LL);
}

static term eval_meta_trace(expr_i *texpr, const term_list_range& targs,
  bool targs_evaluated, eval_context& ectx, expr_i *pos)
{
  term tc;
  for (term_list_range::const_iterator i = targs.begin(); i != targs.end();
    ++i) {
    tc = eval_if_unevaluated(*i, targs_evaluated, ectx, pos);
    fprintf(stderr, "%s\t=>\t%s\n",
      term_tostr_human(*i).c_str(), term_tostr_human(tc).c_str());
  }
  return tc;
}

static term eval_meta_timing(expr_i *texpr, const term_list_range& targs,
  bool targs_evaluated, eval_context& ectx, expr_i *pos)
{
  term tc;
  for (term_list_range::const_iterator i = targs.begin(); i != targs.end();
    ++i) {
    double const t1 = gettimeofday_double();
    tc = eval_if_unevaluated(*i, targs_evaluated, ectx, pos);
    double const t2 = gettimeofday_double();
    fprintf(stderr, "%.06f\t%s\t=>\t%s\n",
      t2 - t1, term_tostr_human(*i).c_str(), term_tostr_human(tc).c_str());
  }
  return tc;
}

struct builtin_typestub_entry {
  const char *name;
  const term *ptr;
};

static const builtin_typestub_entry builtin_typestub_entries[] = {
  { "@0void", &builtins.type_void },
  { "@0unit", &builtins.type_unit },
  { "@0bool", &builtins.type_bool },
  { "@0uchar", &builtins.type_uchar },
  { "@0char", &builtins.type_char },
  { "@0schar", &builtins.type_schar },
  { "@0ushort", &builtins.type_ushort },
  { "@0short", &builtins.type_short },
  { "@0uint", &builtins.type_uint },
  { "@0int", &builtins.type_int },
  { "@0ulong", &builtins.type_ulong },
  { "@0long", &builtins.type_long },
  { "@0ulonglong", &builtins.type_ulonglong },
  { "@0longlong", &builtins.type_longlong },
  { "@0size_t", &builtins.type_size_t },
  { "@0ssize_t", &builtins.type_ssize_t },
  { "@0float", &builtins.type_float },
  { "@0double", &builtins.type_double },
  { "@0strlit", &builtins.type_strlit },
};

struct strict_metafunc_entry {
  const char *name;
  builtin_strict_metafunc_t func;
};

static const strict_metafunc_entry strict_metafunc_entries[] = {
  { "@list", &eval_meta_list },
  { "@at", &eval_meta_at },
  { "@size", &eval_meta_size },
  { "@slice", &eval_meta_slice },
  { "@seq", &eval_meta_seq },
  { "@join_list", &eval_meta_join_list },
  { "@join", &eval_meta_join },
  { "@transpose", &eval_meta_transpose },
  { "@sort", &eval_meta_sort },
  { "@unique", &eval_meta_unique },
  { "@list_index", &eval_meta_list_index },
  { "@list_find", &eval_meta_list_find },
  { "@base", &eval_meta_base },
  { "@ret_type", &eval_meta_ret_type },
  { "@ret_byref", &eval_meta_ret_byref },
  { "@ret_mutable", &eval_meta_ret_mutable },
  { "@ret", &eval_meta_ret },
  { "@arg_size", &eval_meta_arg_size },
  { "@arg_type", &eval_meta_arg_type },
  { "@arg_byref", &eval_meta_arg_byref },
  { "@arg_mutable", &eval_meta_arg_mutable },
  { "@arg_types", &eval_meta_arg_types },
  { "@arg_names", &eval_meta_arg_names },
  { "@args", &eval_meta_args },
  { "@imports", &eval_meta_imports },
  { "@functions", &eval_meta_functions },
  { "@types", &eval_meta_types },
  { "@global_variables", &eval_meta_global_variables },
  { "@member_functions", &eval_meta_member_functions },
  { "@is_copyable_type", &eval_meta_is_copyable_type },
  { "@is_movable_type", &eval_meta_is_movable_type },
  { "@is_assignable_type", &eval_meta_is_assignable_type },
  { "@is_constructible_type", &eval_meta_is_constructible_type },
  { "@is_default_constructible_type",
    &eval_meta_is_default_constructible_type },
  { "@is_polymorphic_type", &eval_meta_is_polymorphic_type },
  { "@inherits", &eval_meta_inherits },
  { "@base_types", &eval_meta_base_types },
  { "@field_types", &eval_meta_field_types },
  { "@field_names", &eval_meta_field_names },
  { "@fields", &eval_meta_fields },
  { "@tparam_size", &eval_meta_tparam_size },
  { "@targ_size", &eval_meta_targ_size },
  { "@targs", &eval_meta_targs },
  { "@values", &eval_meta_values },
  { "@typeof", &eval_meta_typeof },
  { "@is_type", &eval_meta_is_type },
  { "@is_function", &eval_meta_is_function },
  { "@is_member_function", &eval_meta_is_member_function },
  { "@is_mutable_member_function", &eval_meta_is_mutable_member_function },
  { "@is_const_member_function", &eval_meta_is_const_member_function },
  { "@is_metafunction", &eval_meta_is_metafunction },
  { "@is_int", &eval_meta_is_int },
  { "@is_string", &eval_meta_is_string },
  { "@is_list", &eval_meta_is_list},
  { "@to_int", &eval_meta_to_int },
  { "@to_string", &eval_meta_to_string },
  { "@full_string", &eval_meta_full_string },
  { "@concat", &eval_meta_concat },
  { "@substring", &eval_meta_substring },
  { "@subst", &eval_meta_subst },
  { "@strlen", &eval_meta_strlen },
  { "@character", &eval_meta_character },
  { "@code_at", &eval_meta_code_at },
  { "@strchr", &eval_meta_strchr<false> },
  { "@strrchr", &eval_meta_strchr<true> },
  { "@family", &eval_meta_family },
  { "@characteristic", &eval_meta_characteristic },
  { "@nsof", &eval_meta_nsof },
  { "@is_safe_ns", &eval_meta_is_safe_ns },
  { "@nameof", &eval_meta_nameof },
  { "@not", &eval_meta_not },
  { "@eq", &eval_meta_eq },
  { "@add", &eval_meta_add },
  { "@sub", &eval_meta_sub },
  { "@mul", &eval_meta_mul },
  { "@div", &eval_meta_div },
  { "@mod", &eval_meta_mod },
  { "@gt", &eval_meta_gt },
  { "@lt", &eval_meta_lt },
  { "@csymbol", &eval_meta_csymbol },
  { "@profile", &eval_meta_profile },
  { "@map", &eval_meta_map },
  { "@fold", &eval_meta_fold },
  { "@filter", &eval_meta_filter },
  { "@symbol", &eval_meta_symbol },
  { "@symbol_exists", &eval_meta_symbol_exists },
  { "@apply_list", &eval_meta_apply_list },
  { "@apply", &eval_meta_apply },
  { "@error", &eval_meta_error },
};

struct nonstrict_metafunc_entry {
  const char *name;
  builtin_nonstrict_metafunc_t func;
};

static const nonstrict_metafunc_entry nonstrict_metafunc_entries[] = {
  { "@@cond", &eval_meta_cond },
  { "@@or", &eval_meta_or },
  { "@@and", &eval_meta_and },
  { "@@trace", &eval_meta_trace },
  { "@@timing", &eval_meta_timing },
};

const term *find_builtin_typestub(const std::string& name)
{
  for (size_t i = 0;
    i < sizeof(builtin_typestub_entries) / sizeof(builtin_typestub_entries[0]);
    ++i) {
    if (name == std::string(builtin_typestub_entries[i].name)) {
      return builtin_typestub_entries[i].ptr;
    }
  }
  return 0;
}

builtin_strict_metafunc_t find_builtin_strict_metafunction(
  const std::string& name)
{
  for (size_t i = 0;
    i < sizeof(strict_metafunc_entries) / sizeof(strict_metafunc_entries[0]);
    ++i) {
    if (name == std::string(strict_metafunc_entries[i].name)) {
      return strict_metafunc_entries[i].func;
    }
  }
  return 0;
}

builtin_nonstrict_metafunc_t find_builtin_nonstrict_metafunction(
  const std::string& name)
{
  for (size_t i = 0;
    i < sizeof(nonstrict_metafunc_entries)
      / sizeof(nonstrict_metafunc_entries[0]);
    ++i) {
    if (name == std::string(nonstrict_metafunc_entries[i].name)) {
      return nonstrict_metafunc_entries[i].func;
    }
  }
  return 0;
}

term eval_mf_symbol(const term& t, const std::string& name, expr_i *pos)
{
  eval_context ectx;
  term_list tl;
  tl.push_back(t);
  tl.push_back(term(name));
  term r = eval_meta_symbol(tl, ectx, pos);
  return r;
}

term eval_mf_args(const term& t, expr_i *pos)
{
  eval_context ectx;
  term_list_range tr(&t, 1);
  term r = eval_meta_args(tr, ectx, pos);
  return r;
}

term eval_mf_ret(const term& t, expr_i *pos)
{
  eval_context ectx;
  term_list_range tr(&t, 1);
  term r = eval_meta_ret(tr, ectx, pos);
  return r;
}

}; 

