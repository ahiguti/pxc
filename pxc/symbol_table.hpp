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

#ifndef PXC_SYMBOL_TABLE_HPP
#define PXC_SYMBOL_TABLE_HPP

#include "expr.hpp"
#include <map>
#include <string>

namespace pxc {

enum expr_e {
  expr_e_te,
  expr_e_telist,
  expr_e_inline_c,
  expr_e_ns,
  expr_e_int_literal,
  expr_e_float_literal,
  expr_e_bool_literal,
  expr_e_str_literal,
  expr_e_symbol,
  expr_e_nssym,
  expr_e_var,
  expr_e_extval,
  expr_e_stmts,
  expr_e_block,
  expr_e_op,
  expr_e_funccall,
  expr_e_if,
  expr_e_while,
  expr_e_for,
  expr_e_feach,
  expr_e_fldfe,
  expr_e_foldfe,
  expr_e_special,
  expr_e_argdecls,
  expr_e_funcdef,
  expr_e_typedef,
  expr_e_macrodef,
  expr_e_inherit, // FIXME: remove?
  expr_e_struct,
  expr_e_variant,
  expr_e_interface,
  expr_e_try,
  expr_e_throw,
  expr_e_tparams,
};

struct expr_block;
struct expr_stmts;
struct expr_funcdef;
struct expr_struct;
struct expr_variant;
struct expr_interface;

struct localvar_info {
  expr_i *edef; /* expr_var, expr_typedef, expr_macrodef, expr_struct,
		  expr_interface, expr_funcdef, or expr_argdecl */
  attribute_e attr;
  expr_stmts *stmt;
  bool has_attrib_private() const {
    return (attr & attribute_private) != 0;
  }
  bool has_attrib_threaded() const {
    return (attr & attribute_threaded) != 0;
  }
  bool has_attrib_multithr() const {
    return (attr & attribute_multithr) != 0;
  }
  localvar_info(expr_i *edef, attribute_e attr, expr_stmts *stmt)
    : edef(edef), attr(attr), stmt(stmt) { }
  localvar_info() : edef(0), attr(attribute_unknown), stmt(0) { }
};

struct symbol_table {
  symbol_table(expr_block *block_backref);
  expr_block *block_backref;
  typedef std::map<std::string, localvar_info> locals_type;
  locals_type locals;
  typedef std::list<std::string> local_names_type;
  local_names_type local_names;
  locals_type upvalues;
  bool require_thisup : 1; /* uses a member field as upvalue */
  expr_e block_esort; /* funcdef, struct, etc. */
  int tempvar_id_count;
public:
  symbol_table *get_lexical_parent() const;
  void define_name(const std::string& shortname, const std::string& ns,
    expr_i *e, attribute_e visi, expr_stmts *stmt);
  expr_i *resolve_name_nothrow(const std::string& fullname,
    bool no_private, const std::string& curns, bool& is_global_r,
    bool& is_upvalue_r, bool& is_memfld_r);
  expr_i *resolve_name(const std::string& fullname, const std::string& curns,
    expr_i *e, bool& is_global_r, bool& is_upvalue_r);
  void clear_symbols();
  int generate_tempvar();
private:
  localvar_info resolve_name_nothrow_internal(const std::string& fullname,
    const std::string& curns, bool& is_global_r, bool& is_upvalue_r,
    bool& is_memfld_r);
};

};

#endif

