/*
 *  This file is part of PXC, the p extension compiler.
 *  Copyright (C) 2006 A.Higuchi. All rights reserved.
 *  See COPYRIGHT.txt for details.
 *
 *  This file is part of ASE, the abstract script engines.
 *  Copyright (C) 2006 A.Higuchi. All rights reserved.
 *  See COPYRIGHT.txt for details.
 */


#ifndef PXC_SORT_DEP_HPP
#define PXC_SORT_DEP_HPP

#include <list>
#include <set>
#include "expr_impl.hpp"

namespace pxc {

struct sorted_exprs {
  std::list<expr_i *> sorted;
  std::set<expr_i *> pset;
  std::set<expr_i *> parents;
};

void sort_dep(sorted_exprs& s, expr_i *e);

};

#endif

