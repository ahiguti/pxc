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
void fn_check_misc(expr_i *e, generate_main_e gmain);
void add_tparam_upvalues_funcdef_direct(expr_funcdef *efd);
void add_tparam_upvalues_funcdef_tparam(expr_funcdef *efd);
#if 0
void check_tpup_thisptr_constness(expr_funccall *fc);
#endif
bool is_default_constructible(const term& typ);
term get_func_te_for_funccall(expr_i *func, expr_i*& memfunc_w_explicit_obj_r);

};

#endif

