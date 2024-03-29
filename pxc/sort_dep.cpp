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

#include "sort_dep.hpp"
#include "expr_misc.hpp"
#include "eval.hpp"

namespace pxc {

static std::list<expr_i *> get_dep_tparams(expr_struct *est)
{
  std::list<expr_i *> r;
  if (est == 0) {
    return r;
  }
  const term& te = est->get_value_texpr();
  const term_list *const args = te.get_args();
  size_t argslen = args != 0 ? args->size() : 0;
  const typefamily_e cat = est->typefamily;
  if (
    cat == typefamily_e_farray ||
    cat == typefamily_e_cfarray) {
    // cat == typefamily_e_darray ||
    // cat == typefamily_e_cdarray ||
    // cat == typefamily_e_darrayst ||
    // cat == typefamily_e_cdarrayst ||
    // cat == typefamily_e_varray ||
    // cat == typefamily_e_cvarray) {
    if (argslen != 0) {
      term t = (*args)[0]; /* elemnt type */
      r.push_back(term_get_instance(t));
    }
  } else if (cat == typefamily_e_map || cat == typefamily_e_cmap) {
    if (argslen > 1) {
      term t = (*args)[argslen - 1]; /* compare funcobject */
      r.push_back(term_get_instance(t));
    }
  } else if (
    cat == typefamily_e_nocascade ||
    cat == typefamily_e_ptr ||
    cat == typefamily_e_cptr ||
    cat == typefamily_e_iptr ||
    cat == typefamily_e_tptr ||
    cat == typefamily_e_tcptr ||
    cat == typefamily_e_tiptr ||
    cat == typefamily_e_darray ||
    cat == typefamily_e_cdarray ||
    cat == typefamily_e_darrayst ||
    cat == typefamily_e_cdarrayst ||
    cat == typefamily_e_varray ||
    cat == typefamily_e_cvarray) {
    /* no dep */
  } else {
    if (args != 0) {
      for (term_list::const_iterator i = args->begin(); i != args->end();
        ++i) {
        term t = *i; /* TODO: avoid copying */
        r.push_back(term_get_instance(t));
      }
    }
  }
  return r;
}

void sort_dep(sorted_exprs& c, expr_i *e)
{
  if (e == 0) {
    return;
  }
  if (c.pset.find(e) != c.pset.end()) {
    return;
  }
  if (c.parents.find(e) != c.parents.end()) {
    arena_error_throw(e, "A type dependency cycle is found");
  }
  c.parents.insert(e);
  expr_block *block = 0;
  expr_struct *const est = dynamic_cast<expr_struct *>(e);
  if (est != 0) {
    block = est->block;
  }
  expr_dunion *const ev = dynamic_cast<expr_dunion *>(e);
  if (ev != 0) {
    block = ev->block;
  }
  if (block != 0 && is_compiled(block)) {
    const symbol_table& st = block->symtbl;
    symbol_table::local_names_type const& local_names = st.get_local_names();
    symbol_table::local_names_type::const_iterator k;
    symbol_table::locals_type::const_iterator i;
    for (k = local_names.begin(); k != local_names.end(); ++k) {
      i = st.find(*k, false);
      expr_var *const ev = dynamic_cast<expr_var *>(i->second.edef);
      if (ev == 0) {
        continue;
      }
      term te = ev->get_texpr(); /* TODO: eliminate copying */
      expr_i *const einst = term_get_instance(te);
      sort_dep(c, einst);
    }
    typedef std::list<expr_i *> deps_type;
    deps_type deps = get_dep_tparams(est);
    for (deps_type::iterator i = deps.begin(); i != deps.end(); ++i) {
      sort_dep(c, *i);
    }
  }
  c.parents.erase(e);
  c.sorted.push_back(e);
  c.pset.insert(e);
}

};

