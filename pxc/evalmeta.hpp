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

term eval_local_lookup(const term& t, const std::string& name, expr_i *pos);
  /* returns t::foo */

const term *find_builtin_typestub(const std::string& name);
builtin_strict_metafunc_t find_builtin_strict_metafunction(
  const std::string& name);
builtin_nonstrict_metafunc_t find_builtin_nonstrict_metafunction(
  const std::string& name);

};

#endif

