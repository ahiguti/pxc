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
typedef std::map<std::string, term> env_type;
term eval_term_internal(const term& t, bool targs_evaluated, env_type& env,
  size_t depth, expr_i *pos);
bool has_unbound_tparam(const term& t);
bool has_unbound_tparam(const term_list& tl);
term eval_if_unevaluated(const term& t, bool evaluated_flag, env_type& env,
  size_t depth, expr_i *pos);
expr_i *deep_clone_template(expr_i *e, expr_block *instantiate_root);


};

#endif

