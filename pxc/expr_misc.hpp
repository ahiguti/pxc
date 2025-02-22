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
extern unsigned int expr_block_id;
extern builtins_type builtins;
extern errors_type cur_errors;
extern std::string main_namespace;
extern int compile_phase;
extern expr_i *compiling_stmt;
extern const std::map<std::string, std::string> *cur_profile;
extern bool compile_mode_generate_single_cc;
extern size_t recursion_limit;
extern nsimports_type nsimports;
extern nsimports_rec_type nsimports_rec;
extern nsaliases_type nsaliases;
extern nschains_type nschains;
extern nsextends_type nsextends;
typedef std::map<std::string, nsprop> nspropmap_type;
extern nspropmap_type nspropmap;
typedef std::map<std::string, bool> nsthrmap_type;
extern nsthrmap_type nsthrmap;
extern std::string emit_threaded_dll_func;
typedef std::map<symbol, bool> compiled_ns_type;
extern compiled_ns_type compiled_ns;
extern bool detail_error;
extern compiling_expr_stack_type compiling_expr_stack;
/* end: global variables */

expr_i *string_to_te(expr_i *epos, const std::string& str);

void print_space(int n, char c, FILE *fp = stdout);
std::string space_string(int n, char c);
std::string dump_expr(int indent, expr_i *e);
std::string get_full_name(expr_i *nssym);
std::string to_short_name(const std::string& fullname);
symbol to_short_name(const symbol& fullname);
std::string get_namespace_part(const std::string& fullname);
symbol get_namespace_part(const symbol& fullname);
bool has_namespace(const std::string& name);
bool has_namespace(const symbol& name);
std::string term_tostr(const term& t, term_tostr_sort s);
std::string term_tostr_cname(const term& t);
std::string term_tostr_human(const term& t);
std::string term_tostr_list(const term_list_range& tl, term_tostr_sort s);
std::string term_tostr_list_cname(const term_list_range& tl);
std::string term_tostr_list_human(const term_list_range& tl);
bool is_type(const term& t);
bool is_void_type(const term& t);
bool is_unit_type(const term& t);
bool is_bool_type(const term& t);
bool is_enum(const term& t);
bool is_bitmask(const term& t);
bool is_numeric_type(const term& t);
bool is_boolean_type(const term& t);
bool is_equality_type(const term& t);
bool is_ordered_type(const term& t);
// bool is_smallpod_type(const term& t);
bool is_possibly_pod(const term& t);
bool is_possibly_nonpod(const term& t);
bool is_simple_string_type(const term& t);
bool is_integral_type(const term& t);
unsigned long long uint_max_value(const term& t);
const type_attribute *get_type_attribute(const term& t);
long long integral_max_value(const term& t);
long long integral_min_value(const term& t);
bool is_unsigned_integral_type(const term& t);
bool is_float_type(const term& t);
bool is_same_type(const term& t0, const term& t1);
bool implements_interface(const expr_struct *est, const expr_interface *ei);
bool is_sub_type(const term& t0, const term& t1);
bool is_tpdummy_type(const term& t);
bool is_function(const term& t);
bool is_ctypedef(const term& t);
bool is_struct(const term& t);
bool is_dunion(const term& t);
bool is_interface(const term& t);
bool is_intrusive(const term& t);
bool is_dereferencable(const term& t);
bool is_nonintrusive_pointer(const term& t);
bool is_intrusive_pointer(const term& t);
bool is_const_or_immutable_pointer_family(const term& t);
bool is_cm_pointer_family(const term& t);
bool is_const_or_immutable_pointer_family(const term& t);
bool is_immutable_pointer_family(const term& t);
bool is_cm_lock_guard_family(const term& t);
bool is_multithreaded_pointer_family(const term& t);
bool is_array_family(const term& t);
bool is_cm_slice_family(const term& t);
bool is_container_family(const term& t);
bool is_const_container_family(const term& t);
bool is_map_family(const term& t);
bool is_cm_maprange_family(const term& t);
bool is_const_range_family(const term& t);
bool is_cm_range_family(const term& t);
bool is_ephemeral_type(const term& t);
bool has_userdef_constr(const term& t);
bool type_has_refguard(const term& t);
bool type_allow_feach(const term& t);
bool is_compatible_pointer(const term&t0, const term& t1);
bool is_copyable(const term& t);
bool is_movable(const term& t);
bool is_assignable(const term& t);
bool is_assignable_allowing_unsafe(const term& t);
bool is_constructible(const term& t);
bool is_polymorphic(const term& t);
typefamily_e get_family(const term& t);
typefamily_e get_family_from_string(const std::string& s);
std::string get_family_string(typefamily_e cat);
term get_pointer_target(const term& t);
bool pointer_conversion_allowed(const typefamily_e f, const typefamily_e t);
void check_convert_type(expr_i *efrom, term& tto, tvmap_type *tvmap = 0);
bool is_compiletime_intval(expr_i *e);
#if 0
bool convert_type(expr_i *efrom, term& tto, tvmap_type& tvmap);
bool convert_type(expr_i *efrom, term& tto);
#endif
#if 0
std::string ulong_to_string(unsigned long long v);
std::string long_to_string(long long v);
#endif
const char *tok_string(const expr_i *e, int tok);
int count_newline(const char *str);
bool is_global_frame(symbol_table *lookup);
symbol_table *get_current_frame_symtbl(symbol_table *lookup);
expr_i *get_current_frame_expr(symbol_table *lookup);
  /* funcdef, struct, interface */
expr_i *get_current_block_expr(symbol_table *lookup);
bool is_threaded_context(symbol_table *lookup); // FIXME get_context_threading
bool is_multithr_context(symbol_table *lookup); // FIXME get_context_threading
attribute_e get_expr_threading_attribute(const expr_i *e,
  std::string *pusherr = nullptr);
attribute_e get_context_threading_attribute(symbol_table *lookup,
  std::string *pusherr = nullptr);
/*
bool term_is_threaded(const term& t); // FIXME: get_term_threading
bool term_is_multithr(const term& t); // FIXME: get_term_threading
*/
attribute_e get_term_threading_attribute(const term& t,
  std::string *pusherr = nullptr);
expr_funcdef *get_up_member_func(symbol_table *lookup);
// expr_i *get_up_struct(expr_funcdef *efd); // struct, funcdef etc. skips block.
expr_i *find_parent(expr_i *e, expr_e t);
const expr_i *find_parent_const(const expr_i *e, expr_e t); // FIXME: remove
symbol_table *find_current_symbol_table(expr_i *e);
expr_i *get_op_rhs(expr_i *e, int op);
bool is_type_esort(expr_e es);
bool is_type_or_func_esort(expr_e es);
bool is_global_var(const expr_i *e);
bool is_ancestor_symtbl(symbol_table *st1, symbol_table *st2);
void check_inherit_threading(expr_i *einst, expr_block *block);
bool is_passby_cm_value(passby_e passby);
bool is_passby_cm_reference(passby_e passby);
bool is_passby_const(passby_e passby);
bool is_passby_mutable(passby_e passby);

bool is_range_op(const expr_i *e);
bool expr_has_lvalue(const expr_i *epos, expr_i *a0, bool thro_flg);
term get_pointer_deref_texpr(expr_op *eop, const term& t);
term get_array_range_texpr(expr_op *eop, expr_i *ec, const term& ect);
bool is_dense_array(expr_op *eop, const term& t0);
term get_array_elem_texpr(expr_op *eop, const term& t0);
term get_array_index_texpr(expr_op *eop, const term& t0);
bool is_vardef_constructor_or_byref(expr_i *e, bool incl_byref);
bool is_vardef_or_vardefset(expr_i *e);
bool is_noexec_expr(expr_i *e);

bool is_safe_namespace(const std::string& ns);
bool is_sibling_namespace(const std::string& ns0, const std::string& ns1);

symbol get_ns(expr_i *pos);
bool is_visible_from_ns(const symbol& ns, const symbol& nsfrom);
bool is_visible_from_pos(const symbol& ns, expr_i *pos);
bool is_visible_from_pos_or_inst_context(const symbol& ns, expr_i *pos);

bool is_compiled(const expr_block *bl);

void fn_append_coptions(expr_i *e, coptions& copt_append);
void fn_set_namespace(expr_i *e, const std::string& uniqns, int& block_id_ns,
  bool allow_unsafe);
void fn_set_generated_code(expr_i *e);
void fn_set_tree_and_define_static(expr_i *e, expr_i *p, symbol_table *symtbl,
  expr_stmts *stmt, bool is_template_descent, bool is_expand_base = false);
void fn_update_tree(expr_i *e, expr_i *p, symbol_table *symtbl,
  const std::string& curns_u);
void fn_check_template_upvalues_direct(expr_i *e);
void fn_check_template_upvalues_tparam(expr_i *e);
void fn_check_dep_upvalues(expr_i *e);
void fn_check_final(expr_i *e);
void fn_prepare_imports();
void fn_compile(expr_i *e, expr_i *p, bool is_template);
expr_i *fn_drop_non_exports(expr_i *e);
void fn_set_implicit_threading_attr(expr_i *e, bool is_inside_funcdef = false,
  attribute_e parent_tattr = attribute_unknown);

bool fn_str_equals(const char *x, const char *y);
bool fn_expr_equals(expr_i *x, expr_i *y);

bool is_ephemeral_assignable(symbol_table *lhs_scope, symbol_table *rhs_scope);
symbol_table * get_smaller_ephemeral_scope(symbol_table *a, symbol_table *b);

std::string conversion_string(conversion_e c);


#if 0
template <typename T> fn_equals(T *x, T *y) {
  if (x == nullptr && y == nullptr) {
    return true;
  }
  if (x == nullptr || y == nullptr) {
    return false;
  }
  return (*x) == (*y);
}
#endif

struct compiling_expr_push {
  compiling_expr_push(const std::string& msg, expr_i *expr, term const& t,
    const char *func, const char *fname, int line)
  {
    if (!detail_error) {
      return;
    }
    compiling_expr e;
    e.message = msg;
    e.expr = expr;
    e.tm = t;
    e.func = func;
    e.fname = fname;
    e.line = line;
    compiling_expr_stack.push_back(std::move(e));
  }
  ~compiling_expr_push() {
    if (!detail_error) {
      return;
    }
    compiling_expr_stack.pop_back();
  }
  compiling_expr_push(compiling_expr_push const&) = delete;
  compiling_expr_push& operator =(compiling_expr_push const&) = delete;
};

#define PUSH_EXPR(v, m, e, t) \
  compiling_expr_push \
    v(m, e, t, __PRETTY_FUNCTION__, __FILE__, __LINE__);

};

#endif

