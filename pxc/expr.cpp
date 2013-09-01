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

#include "expr_misc.hpp"
#include "checktype.hpp"
#include "emit.hpp"
#include "auto_free.hpp"
#include "util.hpp"

#define DBG(x)

namespace pxc {

static expr_i *arena_push(expr_i *e)
{
  expr_arena.push_back(e);
  return e;
}

expr_i *expr_te_new(const char *fn, int line, expr_i *nssym, expr_i *tlarg)
{ return arena_push(new expr_te(fn, line, nssym, tlarg)); }
expr_i *expr_telist_new(const char *fn, int line, expr_i *head, expr_i *rest)
{ return arena_push(new expr_telist(fn, line, head, rest)); }
expr_i *expr_inline_c_new(const char *fn, int line, const char *posstr,
  const char *cstr, bool declonly, expr_i *val)
{ return arena_push(new expr_inline_c(fn, line, posstr, cstr, declonly,
  val)); }
expr_i *expr_ns_new(const char *fn, int line, expr_i *nssym, bool import,
  bool pub, const char *nsalias, const char *safety)
{ return arena_push(new expr_ns(fn, line, nssym, import, pub, nsalias,
  safety)); }
expr_i *expr_int_literal_new(const char *fn, int line, const char *str,
  bool is_unsigned)
{ return arena_push(new expr_int_literal(fn, line, str, is_unsigned)); }
expr_i *expr_float_literal_new(const char *fn, int line, const char *str)
{ return arena_push(new expr_float_literal(fn, line, str)); }
expr_i *expr_bool_literal_new(const char *fn, int line, bool v)
{ return arena_push(new expr_bool_literal(fn, line, v)); }
expr_i *expr_str_literal_new(const char *fn, int line, const char *str)
{ return arena_push(new expr_str_literal(fn, line, str)); }
expr_i *expr_nssym_new(const char *fn, int line, expr_i *prefix,
  const char *sym)
{ return arena_push(new expr_nssym(fn, line, prefix, sym)); }
expr_i *expr_symbol_new(const char *fn, int line, expr_i *nssym)
{ return arena_push(new expr_symbol(fn, line, nssym)); }
expr_i *expr_var_new(const char *fn, int line, const char *sym,
  expr_i *type_uneval, passby_e passby, attribute_e attr, expr_i *rhs_ref)
{ return arena_push(new expr_var(fn, line, sym, type_uneval, passby, attr,
  rhs_ref)); }
expr_i *expr_enumval_new(const char *fn, int line, const char *sym,
  expr_i *type_uneval, const char *cname, expr_i *value, attribute_e attr,
  expr_i *rest)
{ return arena_push(new expr_enumval(fn, line, sym, type_uneval, cname, value,
  attr, rest)); }
expr_i *expr_stmts_new(const char *fn, int line, expr_i *head, expr_i *rest)
{ return arena_push(new expr_stmts(fn, line, head, rest)); }
expr_i *expr_argdecls_new(const char *fn, int line, const char *sym,
  expr_i *type_uneval, passby_e passby, expr_i *rest)
{ return arena_push(new expr_argdecls(fn, line, sym, type_uneval, passby,
  rest)); }
expr_i *expr_block_new(const char *fn, int line, expr_i *tparams,
  expr_i *inherit, expr_i *argdecls, expr_i *rettype, passby_e ret_passby,
  expr_i *stmts)
{ return arena_push(new expr_block(fn, line, tparams, inherit, argdecls,
  rettype, ret_passby, stmts)); }
expr_i *expr_op_new(const char *fn, int line, int op, expr_i *arg0,
  expr_i *arg1, const char *extop)
{ return arena_push(new expr_op(fn, line, op, extop, arg0, arg1)); }
expr_i *expr_funccall_new(const char *fn, int line, expr_i *func, expr_i *arg)
{ return arena_push(new expr_funccall(fn, line, func, arg)); }
expr_i *expr_special_new(const char *fn, int line, int tok, expr_i *arg)
{ return arena_push(new expr_special(fn, line, tok, arg)); }
expr_i *expr_if_new(const char *fn, int line, expr_i *cond, expr_i *b1,
  expr_i *b2, expr_i *rest)
{ return arena_push(new expr_if(fn, line, cond, b1, b2, rest)); }
expr_i *expr_while_new(const char *fn, int line, expr_i *cond, expr_i *block)
{ return arena_push(new expr_while(fn, line, cond, block)); }
expr_i *expr_for_new(const char *fn, int line, expr_i *e0, expr_i *e1,
  expr_i *e2, expr_i *block)
{ return arena_push(new expr_for(fn, line, e0, e1, e2, block)); }
expr_i *expr_forrange_new(const char *fn, int line, expr_i *r0, expr_i *r1,
  expr_i *block)
{ return arena_push(new expr_forrange(fn, line, r0, r1, block)); }
expr_i *expr_feach_new(const char *fn, int line, expr_i *ce, expr_i *block)
{ return arena_push(new expr_feach(fn, line, ce, block)); }
expr_i *expr_fldfe_new(const char *fn, int line, const char *namesym,
  const char *fldsym, const char *idxsym, expr_i *te, expr_i *stmts)
{ return arena_push(new expr_fldfe(fn, line, namesym, fldsym, idxsym, te,
  stmts)); }
expr_i *expr_foldfe_new(const char *fn, int line, const char *itersym,
  expr_i *valueste, const char *embedsym, expr_i *embedexpr,
  const char *foldop, expr_i *stmts)
{ return arena_push(new expr_foldfe(fn, line, itersym, valueste, embedsym,
  embedexpr, foldop, stmts)); }
expr_i *expr_expand_new(const char *fn, int line, const char *itersym,
  const char *idxsym, expr_i *valueste, expr_i *baseexpr, expand_e ex,
  expr_i *rest)
{ return arena_push(new expr_expand(fn, line, itersym, idxsym, valueste,
  baseexpr, ex, rest)); }
expr_i *expr_funcdef_new(const char *fn, int line, const char *sym,
  const char *cname, bool is_const, expr_i *block, bool ext_decl,
  bool extc_decl, attribute_e attr)
{ return arena_push(new expr_funcdef(fn, line, sym, cname, is_const, block,
  ext_decl, extc_decl, attr)); }
expr_i *expr_typedef_new(const char *fn, int line, const char *sym,
  const char *cname, const char *family, bool is_enum, bool is_bitmask,
  expr_i *enumvals, unsigned int num_tpara, attribute_e attr)
{ return arena_push(new expr_typedef(fn, line, sym, cname, family, is_enum,
  is_bitmask, enumvals, num_tpara, attr)); }
expr_i *expr_metafdef_new(const char *fn, int line, const char *sym,
  expr_i *tparams, expr_i *rhs, attribute_e attr)
{ return arena_push(new expr_metafdef(fn, line, sym, tparams, rhs, attr)); }
expr_i *expr_struct_new(const char *fn, int line, const char *sym,
  const char *cname, const char *family, expr_i *block, attribute_e attr,
  bool has_udcon, bool private_udcon)
{ return arena_push(new expr_struct(fn, line, sym, cname, family, block, attr,
  has_udcon, private_udcon)); }
expr_i *expr_dunion_new(const char *fn, int line, const char *sym,
  expr_i *block, attribute_e attr)
{ return arena_push(new expr_dunion(fn, line, sym, block, attr)); }
expr_i *expr_interface_new(const char *fn, int line, const char *sym,
  const char *cname, expr_i *block, attribute_e attr)
{ return arena_push(new expr_interface(fn, line, sym, cname, block, attr)); }
expr_i *expr_try_new(const char *fn, int line, expr_i *tblock, expr_i *cblock,
  expr_i *rest)
{ return arena_push(new expr_try(fn, line, tblock, cblock, rest)); }
expr_i *expr_tparams_new(const char *fn, int line, const char *sym,
  expr_i *rest)
{ return arena_push(new expr_tparams(fn, line, sym, rest)); }

expr_i *expr_te_local_chain_new(expr_i *te1, expr_i *te2)
{
  /* te1::te2expr{te2args, ...} => meta::local{te1, "te2expr", te2args, ...} */
  expr_te *const tte2 = ptr_down_cast<expr_te>(te2);
  std::string s = escape_c_str_literal(tte2->nssym->sym);
  char *const te2expr = arena_strdup(s.c_str());
  return expr_te_new(te1->fname, te1->line,
    expr_nssym_new(te1->fname, te1->line,
      expr_nssym_new(te1->fname, te1->line, 0, "meta"), "@local"),
      expr_telist_new(te1->fname, te1->line,
	te1,
	expr_telist_new(te2->fname, te2->line,
	  expr_str_literal_new(te2->fname, te2->line, te2expr),
	  tte2->tlarg)));
}

expr_i *expr_metalist_new(expr_i *tl)
{
  return expr_te_new(tl->fname, tl->line,
    expr_nssym_new(tl->fname, tl->line,
      expr_nssym_new(tl->fname, tl->line, 0, "meta"), "@list"),
	// TODO: type::common does not exist
    tl);
}

char *arena_strdup(const char *str)
{
  char *const p = strdup(str);
  if (p == 0) {
    abort();
  }
  str_arena.push_back(p);
  return p;
}

char *arena_dequote_strdup(const char *str)
{
  const size_t len = strlen(str);
  char *const ptr = static_cast<char *>(malloc(len + 1));
  if (ptr == 0) {
    abort();
  }
  str_arena.push_back(ptr);
  memcpy(ptr, str + 1, len - 2);
  ptr[len - 2] = '\0';
  return ptr;
}

char *arena_decode_inline_strdup(const char *str)
{
  size_t len = strlen(str);
  if (len >= 7 && memcmp(str, "inline\n", 7) == 0) {
    str += 7;
    len -= 7;
  }
  char *const p = strdup(str);
  if (p == 0) {
    abort();
  }
  if (len > 0 && p[len - 1] == '\n') {
    p[len - 1] = '\0';
  }
  str_arena.push_back(p);
  return p;
}

static std::string get_expr_info(const expr_i *e)
{
  if (e == 0) {
    return "";
  }
  const std::string fn(e->fname);
  const std::string ln = ulong_to_string(e->line);
  return fn + ":" + ln + ": ";
}

void arena_error_throw(const expr_i *e, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  char *buf = 0;
  if (vasprintf(&buf, format, ap) < 0) {
    abort();
  }
  va_end(ap);
  auto_free abuf(buf);
  std::string s(buf);
  if (!s.empty() && s[s.size() - 1] != '\n') {
    s += "\n";
  }
  throw std::runtime_error(get_expr_info(e) + s);
}

void arena_error_push(const expr_i *e, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  char *buf = 0;
  if (vasprintf(&buf, format, ap) < 0) {
    abort();
  }
  va_end(ap);
  auto_free abuf(buf);
  const std::string s(buf);
  cur_errors.push_back(get_expr_info(e) + s);
}

void arena_error_throw_pushed()
{
  if (cur_errors.empty()) {
    return;
  }
  std::string s;
  for (errors_type::const_iterator i = cur_errors.begin();
    i != cur_errors.end(); ++i) {
    s += (*i);
    s += "\n";
  }
  cur_errors.clear();
  throw std::runtime_error(s);
}

std::string arena_get_ns_main_funcname(const std::string& nsstr)
{
  std::string fnstr = "_" + nsstr;
  for (size_t i = 0; i < fnstr.size(); ++i) {
    if (fnstr[i] == ':') {
      fnstr[i] = '$';
    }
  }
  fnstr += "$$nsmain";
  return fnstr;
}

struct builtin_typedefs_type {
  const char *name;
  const char *cname;
  const char *family;
  bool is_pod;
  unsigned int num_tparams;
  type_attribute tattr;
  term *tptr;
};

static const builtin_typedefs_type builtin_typedefs[] = {
  { "bt_void", "void", 0, true, 0,
    type_attribute(0, 0, false, false, false), &builtins.type_void },
  { "bt_unit", "pxcrt::bt_unit", 0, true, 0,
    type_attribute(0, 0, false, false, false), &builtins.type_unit },
  { "bt_bool", "pxcrt::bt_bool", 0, true, 0,
    type_attribute(1, 1, false, false, false), &builtins.type_bool },
  { "bt_uchar", "pxcrt::bt_uchar", 0, true, 0,
    type_attribute(8, 8, true, true, false), &builtins.type_uchar },
  { "bt_char", "pxcrt::bt_char", 0, true, 0,
    type_attribute(7, 7, true, true, true), &builtins.type_char },
  { "bt_ushort", "pxcrt::bt_ushort", 0, true, 0,
    type_attribute(16, 16, true, true, false), &builtins.type_ushort },
  { "bt_short", "pxcrt::bt_short", 0, true, 0,
    type_attribute(15, 15, true, true, true), &builtins.type_short },
  { "bt_uint", "pxcrt::bt_uint", 0, true, 0,
    type_attribute(32, 32, true, true, false), &builtins.type_uint },
  { "bt_int", "pxcrt::bt_int", 0, true, 0,
    type_attribute(31, 31, true, true, true), &builtins.type_int },
  { "bt_ulong", "pxcrt::bt_ulong", 0, true, 0,
    type_attribute(64, 64, true, true, false), &builtins.type_ulong },
  { "bt_long", "pxcrt::bt_long", 0, true, 0,
    type_attribute(63, 63, true, true, true), &builtins.type_long },
  { "bt_size_t", "pxcrt::bt_size_t", 0, true, 0,
    type_attribute(32, 64, true, true, false), &builtins.type_size_t},
  { "bt_float", "pxcrt::bt_float", 0, true, 0,
    type_attribute(23, 23, true, false, true), &builtins.type_float },
  { "bt_double", "pxcrt::bt_double", 0, true, 0,
    type_attribute(52, 52, true, false, true), &builtins.type_double },
  { "bt_tpdummy", "pxcrt::bt_tpdummy", 0, true, 0,
    type_attribute(0, 0, false, false, false), &builtins.type_tpdummy },
};

static void define_builtins()
{
  expr_i *stmts = 0;
  const size_t num = sizeof(builtin_typedefs) / sizeof(builtin_typedefs[0]);
  for (size_t i = 0; i < num; ++i) {
    const builtin_typedefs_type *const be = builtin_typedefs + i;
    expr_typedef *const etd = ptr_down_cast<expr_typedef>(
      expr_typedef_new("BUILTIN", 0, be->name,
	be->cname, be->family, false, false, 0, be->num_tparams,
	attribute_e(attribute_public |
	  attribute_threaded | attribute_multithr | attribute_valuetype |
	  attribute_tsvaluetype)));
    etd->tattr = be->tattr;
    (*be->tptr) = etd->get_value_texpr();
    stmts = expr_stmts_new("", 0, etd, stmts);
  }
  for (expr_arena_type::iterator i = expr_arena.begin();
    i != expr_arena.end(); ++i) {
    (*i)->set_texpr(builtins.type_void);
  }
  /* set namespace */
  {
    int block_id_ns = 0;
    fn_set_namespace(stmts, "meta", block_id_ns, true);
  }
  topvals.push_front(stmts);
  /* stubs for builtin metafunctions */
  stmts = 0;
  {
    expr_i *est = 0;
    est = expr_struct_new("BUILTIN", 0,
      arena_strdup("@local"),
      arena_strdup("@local"),
      arena_strdup("@local"),
      expr_block_new("BUILTIN", 0, 0, 0, 0, 0, passby_e_mutable_value, 0),
      attribute_e(attribute_public |
	attribute_threaded | attribute_multithr | attribute_valuetype |
	attribute_tsvaluetype), false, false);
    stmts = expr_stmts_new("", 0, est, stmts);
    est = expr_struct_new("BUILTIN", 0,
      arena_strdup("@list"),
      arena_strdup("@list"),
      arena_strdup("@list"),
      expr_block_new("BUILTIN", 0, 0, 0, 0, 0, passby_e_mutable_value, 0),
      attribute_e(attribute_public |
	attribute_threaded | attribute_multithr | attribute_valuetype |
	attribute_tsvaluetype), false, false);
    stmts = expr_stmts_new("", 0, est, stmts);
  }  
  /* set namespace */
  {
    int block_id_ns = 0;
    fn_set_namespace(stmts, "meta", block_id_ns, true);
  }
  topvals.push_front(stmts);
}

static bool define_builtin_string(expr_stmts *stmts_runtime)
{
  builtins.type_strlit = builtins.type_void;
  builtins.type_slice = builtins.type_void;
  builtins.type_cslice = builtins.type_void;
  for (expr_stmts *st = stmts_runtime; st != 0; st = st->rest) {
    expr_struct *def = dynamic_cast<expr_struct *>(st->head);
    if (def == 0) {
      continue;
    }
    const std::string s(def->sym);
    const std::string ns(def->get_unique_namespace());
    if (s == "strlit" && ns == "container::array") {
      builtins.type_strlit = def->get_value_texpr();
    } else if (s == "slice" && ns == "container::array") {
      builtins.type_slice = def->get_value_texpr();
    } else if (s == "cslice" && ns == "container::array") {
      builtins.type_cslice = def->get_value_texpr();
    }
  }
  if (builtins.type_strlit == builtins.type_void ||
    builtins.type_slice == builtins.type_void ||
    builtins.type_cslice == builtins.type_void) {
    return false;
  }
  return true;
}

void arena_append_topval(const std::list<expr_i *>& tvs, bool is_main,
  imports_type& imports_r)
{
  expr_i *topval = 0;
  {
    /* concat topvals */
    expr_stmts *last = 0;
    for (std::list<expr_i *>::const_iterator i = tvs.begin();
      i != tvs.end(); ++i) {
      if ((*i) == 0) {
	continue;
      }
      if (topval == 0) {
	topval = (*i);
      }
      expr_stmts *stmts = ptr_down_cast<expr_stmts>(*i);
      if (last != 0) {
	last->set_rest(stmts);
      }
      last = stmts;
      while (last->rest != 0) {
	last = last->rest;
      }
    }
  }
  expr_i *e = topval;
  std::string uniqns;
  /* find namespace decl */
  while (e != 0) {
    expr_i *stmt = ptr_down_cast<expr_stmts>(e)->head;
    if (stmt->get_esort() == expr_e_ns) {
      expr_ns *ns = ptr_down_cast<expr_ns>(stmt);
      if (!ns->import) {
	if (!uniqns.empty()) {
	  arena_error_push(ns, "Duplicate namespace declaration");
	}
	uniqns = ns->uniq_nsstr;
	imports_r.main_unique_namespace = uniqns;
      } else {
	import_info ii;
	ii.ns = ns->uniq_nsstr;
	ii.import_public = ns->pub;
	imports_r.deps.push_back(ii);
      }
    }
    e = ptr_down_cast<expr_stmts>(e)->rest;
  }
  if (topval != 0 && uniqns.empty()) {
    arena_error_push(topval, "No namespace declaration");
  }
  if (!uniqns.empty()) {
    int block_id_ns = 0;
    fn_set_namespace(topval, uniqns, block_id_ns,
      nssafetymap[uniqns] != nssafety_e_safe);
  }
  if (is_main) {
    main_namespace = uniqns;
  } else {
    /* fn_set_namespace() must be before fn_drop_non_exports(), or it
     * can cause inconsistency of block_id_ns among compilation units. */
    expr_i *ntopval = fn_drop_non_exports(topval);
    topval = ntopval;
  }
  /* error if first or last stmt is an 'expand' expression */
  e = topval;
  while (e != 0) {
    expr_i *stmt = ptr_down_cast<expr_stmts>(e)->head;
    if (stmt->get_esort() == expr_e_expand) {
      if (e == topval) {
	arena_error_push(e,
	  "Implementation restriction: first statement of a namespace must "
	  "not be an 'expand' statement");
      }
      if (ptr_down_cast<expr_stmts>(e)->rest == 0) {
	arena_error_push(e,
	  "Implementation restriction: last statement of a namespace must "
	  "not be an 'expand' statement");
      }
    }
    e = ptr_down_cast<expr_stmts>(e)->rest;
  }
  topvals.push_back(topval);
  loaded_namespaces[imports_r.main_unique_namespace] = imports_r;
  DBG(fprintf(stderr, "namespace %s loaded\n",
    imports_r.main_unique_namespace.c_str()));
}

void arena_init()
{
  arena_clear();
  compile_phase = 0;
  define_builtins();
}

void arena_set_recursion_limit(size_t v)
{
  recursion_limit = v;
}

void arena_compile(const std::map<std::string, std::string>& prof_map,
  const std::string& dest_filename, coptions& copt_apnd,
  generate_main_e gmain)
{
  compile_phase = 1;
  cur_profile = &prof_map;
  arena_error_throw_pushed();
  /* chain topvals */
  expr_stmts *e = 0;
  topvals_type::reverse_iterator ri;
  for (ri = topvals.rbegin(); ri != topvals.rend(); ++ri) {
    expr_stmts *stmts = ptr_down_cast<expr_stmts>(*ri);
    expr_stmts *last = stmts;
    if (last == 0) {
      continue;
    }
    while (last->rest != 0) {
      last = last->rest;
    }
    last->set_rest(e);
    e = stmts;
  }
  expr_i *const global = expr_block_new("", 0, 0, 0, 0, 0,
    passby_e_mutable_value, e);
  expr_block *const gl_block = ptr_down_cast<expr_block>(global);
  global_block = gl_block;
  /* define bt_string */
  if (!define_builtin_string(e)) {
    #if 0
    return; /* compier::impl::* ? */
    #endif
  }
  /* compile */
  compile_phase = 2;
  fn_compile(global, 0, false);
  arena_error_throw_pushed();
  compile_phase = 3;
  fn_check_closure(global);
  compile_phase = 5;
  fn_check_root(global);
  compile_phase = 6;
  fn_check_template_upvalues_direct(global);
  fn_check_template_upvalues_tparam(global);
  arena_error_throw_pushed();
  // double t1 = gettimeofday_double();
  fn_check_final(global);
  // double t2 = gettimeofday_double();
  // fprintf(stderr, "%s %f\n", dest_filename.c_str(), t2 - t1);
  arena_error_throw_pushed();
  compile_phase = 7;
  fn_append_coptions(global, copt_apnd);
  arena_error_throw_pushed();
  compile_phase = 8;
  emit_code(dest_filename, gl_block, gmain);
  arena_error_throw_pushed();
}

void arena_clear()
{
  /* must initialize all global variables. see expr_misc.hpp. */
  for (expr_arena_type::iterator i = expr_arena.begin();
    i != expr_arena.end(); ++i) {
    delete *i;
  }
  expr_arena.clear();
  for (str_arena_type::iterator i = str_arena.begin();
    i != str_arena.end(); ++i) {
    free(*i);
  }
  str_arena.clear();
  topvals.clear();
  global_block = 0;
  loaded_namespaces.clear();
  expr_block_id = 1;
  builtins = builtins_type();
  cur_errors.clear();
  main_namespace = "";
  compile_phase = 0;
  cur_profile = 0;
  recursion_limit = 3000;
  nsaliases.clear();
  nsextends.clear();
  nssafetymap.clear();
}

};

