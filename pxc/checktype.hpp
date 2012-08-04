/*
 *  This file is part of PXC, the p extension compiler.
 *  Copyright (C) 2006 A.Higuchi. All rights reserved.
 *  See COPYRIGHT.txt for details.
 *
 *  This file is part of ASE, the abstract script engines.
 *  Copyright (C) 2006 A.Higuchi. All rights reserved.
 *  See COPYRIGHT.txt for details.
 */


#ifndef PXC_CHECKTYPE_HPP
#define PXC_CHECKTYPE_HPP

#include "expr_impl.hpp"

namespace pxc {

void fn_check_type(expr_i *e, symbol_table *symtbl);
void fn_check_root(expr_i *e);
void fn_check_closure(expr_i *e);
void add_tparam_upvalues_funcdef(expr_funcdef *efd);

};

#endif

