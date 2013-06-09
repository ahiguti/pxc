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

#ifndef PXC_EVAL_HPP
#define PXC_EVAL_HPP

#include "expr_impl.hpp"

namespace pxc {

term apply_tvmap(const term& t, const tvmap_type& tvmap);
expr_i *instantiate_template(expr_i *tmpl_root, term_list& args_move,
  expr_i *pos);

term eval_expr(expr_i *e);
term eval_term(const term& t);
term eval_local_lookup(const term& t, const std::string& name, expr_i *pos);
  /* returns t::foo */

bool term_has_tparam(const term& t);
bool term_has_unevaluated_expr(const term& t);
expr_i *term_get_instance(term& t);
const expr_i *term_get_instance_const(const term& t);

/* internal */
struct eval_context {
  eval_context() : found_unbound_tparam(false), depth(0), tpbind(0) { }
  bool found_unbound_tparam : 1;
  size_t depth;
  std::vector<term> saved_bindings;
  term *tpbind; /* term_bind */
};
struct eval_depth_inc {
  eval_depth_inc(eval_context& ec) : ec(ec) {
    if (++ec.depth > 10000) {
      arena_error_throw(0, "recursion depth limit is exceeded");
    };
  }
  ~eval_depth_inc() { --ec.depth; }
private:
  eval_context& ec;
  eval_depth_inc(const eval_depth_inc&);
  eval_depth_inc& operator =(const eval_depth_inc&);
};
struct eval_found_unbound_save {
  eval_found_unbound_save(eval_context& ec)
    : ec(ec), found_unbound_saved(ec.found_unbound_tparam) {
    ec.found_unbound_tparam = false;
  }
  ~eval_found_unbound_save() {
    ec.found_unbound_tparam = found_unbound_saved;
  }
private:
  eval_context& ec;
  bool found_unbound_saved : 1;
  eval_found_unbound_save(const eval_found_unbound_save&);
  eval_found_unbound_save& operator =(const eval_found_unbound_save&);
};
term eval_term_internal(const term& t, bool targs_evaluated,
  eval_context& ectx, expr_i *pos);
term eval_apply(const term& tm, const term_list& args, bool targs_evaluated,
  eval_context& ectx, expr_i *pos);
bool has_unbound_tparam(const term& t, eval_context& ectx);
bool has_unbound_tparam(const term_list& tl, eval_context& ectx);
term eval_if_unevaluated(const term& t, bool evaluated_flag,
  eval_context& ectx, expr_i *pos);
expr_i *deep_clone_template(expr_i *e, expr_block *instantiate_root);


};

#endif

