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
#if 0
term eval_term(const term& t, expr_i *pos);
#endif
#if 0
term eval_type_expr_symbol(expr_symbol *sym, expr_i *symdef);
term eval_type_expr_te(expr_te *te); // FIXME: impl
#endif
term eval_expr(expr_i *e);

term eval_local_lookup(const term& t, const std::string& name, expr_i *pos);
  /* returns t::foo */

bool term_has_tparam(const term& t);
bool term_has_unevaluated_expr(const term& t);
expr_i *term_get_instance(term& t);
const expr_i *term_get_instance_const(const term& t);

};

#endif

