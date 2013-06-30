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
expr_i *instantiate_template(expr_i *tmpl_root, const term_list_range& targs,
  expr_i *pos);

term eval_expr(expr_i *e, bool need_partial_eval = false);
term eval_term(const term& t, bool need_partial_eval = false);
term eval_local_lookup(const term& t, const std::string& name, expr_i *pos);
  /* returns t::foo */

bool term_has_tparam(const term& t);
bool term_has_unevaluated_expr(const term& t);
expr_i *term_get_instance(term& t);
expr_i *term_get_instance_if(term& t);
  /* nothrow even when no instance is found */
const expr_i *term_get_instance_const(const term& t);

/* internal */
struct evaluated_entry;
struct eval_context {
  eval_context() : depth(0), tpbind(0), evvals(0), evvals_num(0),
    need_partial_eval(false) { }
  size_t depth;
  const term *tpbind; /* argument variable bindings. must be term_bind. */
  evaluated_entry *evvals; /* evalated values for argument variables */
    // TODO: remove
  size_t evvals_num;
    // TODO: remove
  bool need_partial_eval;
};
#if 0
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
#endif
term eval_term_internal(const term& t, bool targs_evaluated,
  eval_context& ectx, expr_i *pos);
term eval_apply(const term& tm, const term_list_range& args,
  bool targs_evaluated, eval_context& ectx, expr_i *pos);
bool has_unbound_tparam(const term& t, eval_context& ectx);
bool has_unbound_tparam(const term_list_range& tl, eval_context& ectx);
bool is_partial_eval_uneval(const term& t, eval_context& ectx);
bool is_partial_eval_uneval(const term_list_range& tl, eval_context& ectx);
term eval_if_unevaluated(const term& t, bool evaluated_flag,
  eval_context& ectx, expr_i *pos);
expr_i *deep_clone_template(expr_i *e, expr_block *instantiate_root);


};

#endif

