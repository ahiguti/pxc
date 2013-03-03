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

#ifndef PXC_EVALMETA_HPP
#define PXC_EVALMETA_HPP

#include "eval.hpp"

namespace pxc {

typedef std::map<std::string, term> env_type;

term eval_metafunction_lazy(const std::string& name, const term& t,
  bool targs_evaluated, env_type& env, size_t depth, expr_i *pos);
term eval_metafunction_strict(const std::string& name, term_list& tlev,
  const term& t, env_type& env, size_t depth, expr_i *pos);
term eval_local_lookup(const term& t, const std::string& name, expr_i *pos);
  /* returns t::foo */

};

#endif

