/*
 *  This file is part of PXC, the p extension compiler.
 *  Copyright (C) 2006 A.Higuchi. All rights reserved.
 *  See COPYRIGHT.txt for details.
 *
 *  This file is part of ASE, the abstract script engines.
 *  Copyright (C) 2006 A.Higuchi. All rights reserved.
 *  See COPYRIGHT.txt for details.
 */


#ifndef PXC_EMIT_HPP
#define PXC_EMIT_HPP

#include "expr_impl.hpp"

namespace pxc {

void emit_code(const std::string& dest_filename, expr_block *gl_block,
  generate_main_e gmain);
void fn_emit_value(emit_context& em, expr_i *e, bool expand_composite = false,
  bool var_rhs = false);
std::string csymbol_var(const expr_var *ev, bool cdecl);

};

#endif

