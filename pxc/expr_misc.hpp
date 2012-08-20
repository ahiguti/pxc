/*
 *  This file is part of PXC, the p extension compiler.
 *  Copyright (C) 2006 A.Higuchi. All rights reserved.
 *  See COPYRIGHT.txt for details.
 *
 *  This file is part of ASE, the abstract script engines.
 *  Copyright (C) 2006 A.Higuchi. All rights reserved.
 *  See COPYRIGHT.txt for details.
 */


#ifndef PXC_EXPR_MISC_HPP
#define PXC_EXPR_MISC_HPP

#include "expr_impl.hpp"

namespace pxc {

/* begin: global variables */
extern expr_arena_type expr_arena;
extern str_arena_type str_arena;
extern topvals_type topvals;
extern expr_block *global_block;
extern loaded_namespaces_type loaded_namespaces;
extern builtins_type builtins;
extern errors_type cur_errors;
extern std::string main_namespace;
extern int compile_phase;
extern nsaliases_type nsaliases;
/* end: global variables */

void print_space(int n, char c, FILE *fp = stdout);
std::string space_string(int n, char c);
std::string dump_expr(int indent, expr_i *e);
std::string get_full_name(expr_i *nssym);
std::string to_short_name(const std::string& fullname);
std::string get_namespace_part(const std::string& fullname);
bool has_namespace(const std::string& name);
std::string term_tostr(const term& t, term_tostr_sort s);
std::string term_tostr_cname(const term& t);
std::string term_tostr_human(const term& t);
std::string term_tostr_list(const term_list& tl, term_tostr_sort s);
std::string term_tostr_list_cname(const term_list& tl);
std::string term_tostr_list_human(const term_list& tl);
bool is_type(const term& t);
bool is_void_type(const term& t);
bool is_unit_type(const term& t);
bool is_bool_type(const term& t);
bool is_numeric_type(const term& t);
bool is_smallpod_type(const term& t);
bool is_string_type(const term& t);
bool is_integral_type(const term& t);
bool is_same_type(const term& t0, const term& t1);
bool is_sub_type(const term& t0, const term& t1);
bool is_tpdummy_type(const term& t);
bool is_function(const term& t);
bool is_ctypedef(const term& t);
bool is_struct(const term& t);
bool is_variant(const term& t);
bool is_interface(const term& t);
bool is_interface_or_impl(const term& t);
bool is_noninterface_pointer(const term& t);
bool is_interface_pointer(const term& t);
bool is_const_pointer_family(const term& t);
bool is_cm_pointer_family(const term& t);
bool is_array_family(const term& t);
bool is_map_family(const term& t);
bool is_const_slice_family(const term& t);
bool is_cm_slice_family(const term& t);
bool is_weak_value_type(const term& t);
bool has_userdef_constr(const term& t);
bool type_has_invalidate_guard(const term& t);
bool type_allow_feach(const term& t);
bool is_compatible_pointer(const term&t0, const term& t1);
std::string get_category(const term& t);
term get_pointer_target(const term& t);
call_trait_e get_call_trait(const term& t);
bool convert_type(expr_i *efrom, term& tto, tvmap_type& tvmap);
#if 0
std::string ulong_to_string(unsigned long long v);
std::string long_to_string(long long v);
#endif
const char *tok_string(const expr_i *e, int tok);
int count_newline(const char *str);
symbol_table *get_current_frame_symtbl(symbol_table *lookup);
expr_i *get_current_frame_expr(symbol_table *lookup);
  /* funcdef, struct, interface */
expr_i *get_current_block_expr(symbol_table *lookup);
bool is_threaded_context(symbol_table *lookup);
bool is_multithr_context(symbol_table *lookup);
bool term_is_threaded(const term& t);
bool term_is_multithr(const term& t);
expr_funcdef *get_up_member_func(symbol_table *lookup);
expr_i *find_parent(expr_i *e, expr_e t);
const expr_i *find_parent_const(const expr_i *e, expr_e t); // FIXME: remove
symbol_table *find_current_symbol_table(expr_i *e);
expr_i *get_op_rhs(expr_i *e, int op);
bool is_type_esort(expr_e es);
bool is_type_or_func_esort(expr_e es);
bool is_global_var(const expr_i *e);
bool is_ancestor_symtbl(symbol_table *st1, symbol_table *st2);
void check_inherit_threading(expr_struct *est);
bool is_passby_cm_value(passby_e passby);
bool is_passby_cm_reference(passby_e passby);
bool is_passby_const(passby_e passby);
bool is_passby_mutable(passby_e passby);

void fn_append_coptions(expr_i *e, coptions& copt_append);
void fn_set_namespace(expr_i *e, const std::string& n, int& block_id_ns);
void fn_set_tree_and_define_static(expr_i *e, expr_i *p, symbol_table *symtbl,
  expr_stmts *stmt, bool is_template);
void fn_update_tree(expr_i *e, expr_i *p, symbol_table *symtbl,
  const std::string& curns);
void fn_check_final(expr_i *e);
void fn_compile(expr_i *e, expr_i *p, bool is_template);
expr_i *fn_drop_non_exports(expr_i *e);

};

#endif
