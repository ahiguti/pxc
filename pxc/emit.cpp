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

#include "expr_impl.hpp"
#include "term.hpp"
#include "emit.hpp"
#include "eval.hpp"
#include "util.hpp"

#define DBG_EMIT_SYM(x)
#define DBG_DECL(x)
#define DBG_FOBJ(x)
#define DBG_FOI(x)
#define DBG_IF(x)

namespace pxc {

static bool is_compiled(const expr_block *bl)
{
  return !bl->tinfo.is_uninstantiated();
}

static bool cur_stmt_has_tempvar(expr_i *e)
{
  if (e == 0) {
    return false;
  }
  if (e->tempvar_id >= 0) {
    return true;
  }
  if (e->get_esort() == expr_e_block) {
    return false;
  }
  const int n = e->get_num_children();
  for (int i = 0; i < n; ++i) {
    if (cur_stmt_has_tempvar(e->get_child(i))) {
      return true;
    }
  }
  return false;
}

static bool is_block_stmt(expr_i *e)
{
  if (e == 0) {
    return false;
  }
  switch (e->get_esort()) {
  case expr_e_block:
  case expr_e_if:
  case expr_e_while:
  case expr_e_for:
  case expr_e_feach:
  case expr_e_fldfe:
  case expr_e_foldfe:
    return true;
  default:
    return false;
  }
}

static bool is_condblock_stmt(expr_i *e)
{
  if (e == 0) {
    return false;
  }
  switch (e->get_esort()) {
  case expr_e_if:
  case expr_e_while:
  case expr_e_for:
    return true;
  default:
  /* feach, fldfe, and foldfe don't use fn_emit_value_blockcond */
  /* case expr_e_feach: */
  /* case expr_e_fldfe: */
  /* case expr_e_foldfe: */
    return false;
  }
}

static bool inside_blockcond(expr_i *e)
{
  while (true) {
    if (e == 0) {
      break;
    }
    if (is_condblock_stmt(e)) {
      return true;
    }
    if (e->get_esort() == expr_e_stmts) {
       break;
    }
    e = e->parent_expr;
  }
  return false;
}

static std::string get_term_cname(const term& t)
{
  return term_tostr(t, term_tostr_sort_cname);
}

static void emit_term(emit_context& em, const term& t)
{
  em.puts(get_term_cname(t));
}

static void emit_explicit_init_if(emit_context& em, const term& t)
{
  const expr_i *te = term_get_instance_const(t);
  const expr_typedef *etd = dynamic_cast<const expr_typedef *>(te);
  if (etd != 0 && etd->is_pod) {
    em.puts(" = 0");
    return;
  }
  const expr_struct *est = dynamic_cast<const expr_struct *>(te);
  if (est != 0 && est->cname != 0 && est->category == 0) {
    /* it's possible that est is a POD struct. need to generate an explicit
     * initializer. */
    em.puts(" = ");
    em.puts(term_tostr(t, term_tostr_sort_cname));
    em.puts("()");
    return;
  }
}

static void emit_typestr_call_traits(emit_context& em, const term& te,
  bool byref_flag)
{
  switch (get_call_trait(te)) {
  case call_trait_e_raw_pointer:
    if (!byref_flag) {
      emit_term(em, te);
      em.puts("::rawptr const");
    } else {
      emit_term(em, te);
      em.puts("&");
    }
    break;
  case call_trait_e_const_ref_nonconst_ref:
    if (!byref_flag) {
      em.puts("const ");
    }
    emit_term(em, te);
    em.puts("&");
    break;
  case call_trait_e_value:
    emit_term(em, te);
    if (byref_flag) {
      em.puts("&");
    }
    break;
  }
}

static std::string csymbol_encode_ns(const std::string& ns)
{
  std::string c = ns;
  unsigned int cnt = 0;
  for (size_t i = 0; i < c.size(); ++i) {
    if (c[i] == ':') {
      c[i] = '$';
      cnt += 1;
    }
  }
  return c + "$ns" + ulong_to_string(cnt / 2);
}

static std::string c_namespace_str_prefix(const std::string& ns)
{
  std::string r;
  if (ns.empty()) {
    return r;
  }
  for (size_t i = 0; i < ns.size(); ++i) {
    r.push_back(ns[i]);
    if (i > 0 && ns[i] == ':' && ns[i - 1] == ':') {
      r += "$n";
    }
  }
  r += "$n::";
  return r;
}

static std::string csymbol_var(const expr_var *ev, bool cdecl)
{
  expr_i *fr = get_current_frame_expr(ev->symtbl_lexical);
  if (fr != 0 && fr->get_esort() == expr_e_struct) {
    expr_struct *const esd = ptr_down_cast<expr_struct>(fr);
    if (esd->cname != 0) {
      /* c struct field */
      return std::string(ev->sym);
    }
  }
  if (fr == 0 || fr->get_esort() == expr_e_funcdef) {
    /* global frame or a function frame */
    std::string nsp = (!cdecl && is_global_var(ev))
      ? c_namespace_str_prefix(ev->ns) : "";
    return nsp + std::string(ev->sym) + "$"
	  + ulong_to_string(ev->symtbl_lexical->block_backref->block_id_ns)
	  + "$" + csymbol_encode_ns(ev->ns);
  } else {
    return std::string(ev->sym) + "$";
  }
}

// FIXME: for expr_argdecls also?
static void emit_var_cdecl(emit_context& em, const expr_var *ev,
  bool is_argdecl, bool byref)
{
  if (is_argdecl) { /* used for emitting struct constructor */
    emit_typestr_call_traits(em, ev->type_of_this_expr, byref);
      /* byref is true when it's passed as an upvalue */
  } else {
    emit_term(em, ev->type_of_this_expr);
    #if 0
    if (byref) {
      em.puts("&");
    }
    #endif
  }
  em.puts(" ");
  em.puts(csymbol_var(ev, true));
}

static void emit_arg_cdecl(emit_context& em, const expr_argdecls *ad,
  bool is_argdecl)
{
  if (is_argdecl) {
    emit_typestr_call_traits(em, ad->type_of_this_expr, ad->byref_flag);
  } else {
    /* used for calling the user-defined constructor from the default
     * constructor */
    emit_term(em, ad->type_of_this_expr);
    #if 0
    if (byref) { // FIXME: is this possible?
      abort();
      em.puts("&");
    }
    #endif
  }
  em.puts(" ");
  ad->emit_symbol(em);
}

enum tempvars_def_e {
  tempvars_def_e_def,
  tempvars_def_e_def_set,
  tempvars_def_e_set,
};

static void emit_tempvars_def(emit_context& em, expr_i *e, tempvars_def_e td)
{
  if (e == 0 || e->get_esort() == expr_e_block) {
    return;
  }
  const int n = e->get_num_children();
  for (int i = 0; i < n; ++i) {
    emit_tempvars_def(em, e->get_child(i), td);
  }
  if (e->tempvar_id < 0) {
    return;
  }
  const term te = e->type_conv_to.is_null()
    ? e->get_texpr() : e->type_conv_to;
  const bool need_invguard = type_has_invalidate_guard(te);
  if (td == tempvars_def_e_def || td == tempvars_def_e_def_set) {
    em.set_file_line(e);
    em.indent('t');
    const std::string ts = get_term_cname(te);
    const char *const tcname =
      (td == tempvars_def_e_def_set)
	? (need_invguard ? "refvar_igrd_nn" : "refvar_nn")
	: (need_invguard ? "refvar_igrd" : "refvar");
    switch (e->tempvar_varinfo.passby) {
    case passby_e_unspecified:
      abort();
      break;
    case passby_e_mutable_value:
      em.printf("%s ", ts.c_str());
      break;
    case passby_e_const_value:
      if (td != tempvars_def_e_def) {
	em.printf("const %s ", ts.c_str());
      } else {
	em.printf("%s ", ts.c_str());
	  /* nonconst because we need to set later */
      }
      break;
    case passby_e_mutable_reference:
      if (need_invguard || td == tempvars_def_e_def) {
	em.printf("pxcrt::%s< %s > ", tcname, ts.c_str());
      } else {
	em.printf("%s& ", ts.c_str());
      }
      break;
    case passby_e_const_reference:
      if (need_invguard || td == tempvars_def_e_def) {
	em.printf("pxcrt::%s< const %s > ", tcname, ts.c_str());
      } else {
	em.printf("const %s& ", ts.c_str());
      }
      break;
    }
  }
  em.printf("t$%d", e->tempvar_id);
  if (td == tempvars_def_e_def_set || td == tempvars_def_e_set) {
    if (is_passby_cm_reference(e->tempvar_varinfo.passby)) {
      if (td == tempvars_def_e_set) {
	em.puts(".set(");
	fn_emit_value(em, e, true);
	em.puts(")");
      } else if (need_invguard) {
	em.puts("(");
	fn_emit_value(em, e, true);
	em.puts(")");
      } else {
	em.puts(" = ");
	fn_emit_value(em, e, true);
      }
    } else {
      em.puts(" = ");
      fn_emit_value(em, e, true);
    }
  } else if (is_passby_cm_value(e->tempvar_varinfo.passby)) {
    emit_explicit_init_if(em, te);
  }
  if (td == tempvars_def_e_set) {
    em.puts(", ");
  } else {
    em.puts(";\n");
  }
}

static void fn_emit_value_blockcond(emit_context& em, expr_i *e)
{
  /* expressions inside if(...), for(...), while(...) */
  if (cur_stmt_has_tempvar(e)) {
    emit_tempvars_def(em, e, tempvars_def_e_set);
  }
  fn_emit_value(em, e);
}

template <typename T> static std::string get_type_cname_w_ns(T e)
{
  return term_tostr(e->value_texpr, term_tostr_sort_cname);
}

template <typename T> static std::string get_type_cname_wo_ns(T e)
{
  return to_short_name(term_tostr(e->value_texpr, term_tostr_sort_cname));
}

static void emit_interface_def_one(emit_context& em, expr_interface *ei,
  bool proto_only)
{
  if (!is_compiled(ei->block)) {
    return;
  }
  em.set_ns(ei->ns);
  em.set_file_line(ei);
  em.puts("struct ");
  const std::string name_c = get_type_cname_wo_ns(ei);
  em.puts(name_c);
  if (proto_only) {
    em.puts(";\n");
    return;
  }
  em.puts(" {\n");
  em.add_indent(1);
  em.set_file_line(ei);
  em.indent('b');
  em.puts("virtual ~");
  em.puts(name_c);
  em.puts("() { }\n");
  em.set_file_line(ei);
  em.indent('b');
  em.puts("virtual void incref$z() const { }\n");
  em.set_file_line(ei);
  em.indent('b');
  em.puts("virtual void decref$z() const { }\n");
  em.set_file_line(ei);
  em.indent('b');
  em.puts("virtual size_t get$z() const { }\n");
  em.set_file_line(ei);
  em.indent('b');
  em.puts("virtual pxcrt::mutex& get_mutex$z() const "
    "{ pxcrt::throw_virtual_function_call(); }\n");
  expr_block *const bl = ei->block;
  const bool is_funcbody = false;
  bl->emit_local_decl(em, is_funcbody);
  bl->emit_memberfunc_decl(em, true);
  em.add_indent(-1);
  em.puts("};\n");
}

static void emit_argdecls(emit_context& em, expr_argdecls *ads, bool is_first)
{
  for (expr_argdecls *a = ads; a; a = a->rest) {
    if (is_first) {
      is_first = false;
    } else {
      em.puts(", ");
    }
    emit_typestr_call_traits(em, a->get_texpr(), a->byref_flag);
    em.puts(" ");
    a->emit_symbol(em);
  }
}

static void emit_struct_constr_initializer(emit_context& em,
  const expr_struct *est, const std::list<expr_var *>& flds,
  bool emit_default_init)
{
  if (est->block->inherit) {
    em.puts(" : count$z(1)");
  }
  std::list<expr_var *>::const_iterator i;
  for (i = flds.begin(); i != flds.end(); ++i) {
    if (i == flds.begin() && est->block->inherit == 0) {
      em.puts(" : ");
    } else {
      em.puts(", ");
    }
    (*i)->emit_symbol(em);
    if (emit_default_init) {
      em.puts("()");
    } else {
      em.puts("(");
      (*i)->emit_symbol(em);
      em.puts(")");
    }
  }
}

static void emit_struct_def_one(emit_context& em, const expr_struct *est,
  bool proto_only)
{
  if (!is_compiled(est->block)) {
    return;
  }
  em.set_ns(est->ns);
  em.set_file_line(est);
  em.puts("struct ");
  const std::string name_c = get_type_cname_wo_ns(est);
  em.puts(name_c);
  if (proto_only) {
    em.puts(";\n");
    return;
  }
  if (est->block->inherit != 0) {
    expr_telist *ih = est->block->inherit;
    while (ih != 0) {
      if (ih == est->block->inherit) {
	em.puts(" : ");
      } else {
	em.puts(", ");
      }
      #if 0
      const expr_interface *const ihdef = ptr_down_cast<const expr_interface>(
	term_get_instance(ih->head->get_texpr()));
      em.puts(get_type_cname_w_ns(ihdef));
      #endif
      em.puts(term_tostr_cname(ih->head->get_sdef()->get_evaluated()));
      ih = ih->rest;
    }
  }
  em.puts(" {\n");
  em.add_indent(1);
  if (est->block->inherit != 0) {
    em.set_file_line(est);
    em.indent('b');
    em.puts("void incref$z() const { __sync_fetch_and_add(&count$z, 1); }\n");
    em.set_file_line(est);
    em.indent('b');
    em.puts("void decref$z() const "
      "{ if (__sync_fetch_and_add(&count$z, -1) == 1) delete this; }\n");
    em.set_file_line(est);
    em.indent('b');
    em.puts("pxcrt::mutex& get_mutex$z() const { return mutex$z; }\n");
    em.set_file_line(est);
    em.indent('b');
    em.puts("size_t get$z() const { return count$z; }\n");
    em.set_file_line(est);
    em.indent('b');
    em.puts("mutable long count$z;\n");
    em.set_file_line(est);
    em.indent('b');
    em.puts("mutable pxcrt::mutex mutex$z;\n");
  }
  expr_block *const bl = est->block;
  const bool is_funcbody = false;
  /* member fields */
  bl->emit_local_decl(em, is_funcbody);
  /* member functions */
  bl->emit_memberfunc_decl(em, false);
  if (est->has_userdefined_constr()) {
    /* user defined constructor */
    /* default constructor */
    em.set_file_line(est);
    em.indent('b');
    em.puts("inline ");
    em.puts(name_c);
    em.puts("();\n");
    /* with args */
    if (elist_length(est->block->argdecls) != 0) {
      em.set_file_line(est);
      em.indent('b');
      em.puts("inline ");
      em.puts(name_c);
      em.puts("(");
      emit_argdecls(em, est->block->argdecls, true);
      em.puts(");\n");
    }
    /* initializer aux func */
    em.set_file_line(est);
    em.indent('b');
    em.puts("inline void init$z(");
    emit_argdecls(em, est->block->argdecls, true);
    em.puts(");\n");
  } else {
    /* get field list */
    typedef std::list<expr_var *> flds_type;
    flds_type flds;
    est->get_fields(flds);
    flds_type::const_iterator i;
    /* default constructor */
    em.set_file_line(est);
    em.indent('b');
    em.puts(name_c);
    em.puts("()");
    emit_struct_constr_initializer(em, est, flds, true);
    em.puts(" { }\n");
    /* struct constructor */
    if (!flds.empty()) {
      em.set_file_line(est);
      em.indent('b');
      em.puts(name_c);
      em.puts("(");
      for (i = flds.begin(); i != flds.end(); ++i) {
	if (i != flds.begin()) {
	  em.puts(", ");
	}
	emit_var_cdecl(em, *i, true, false); // (*i)->emit_cdecl(em, true, false);
      }
      em.puts(")");
      emit_struct_constr_initializer(em, est, flds, false);
      em.puts(" { }\n");
    }
  }
  em.add_indent(-1);
  em.puts("};\n");
}

static void emit_initialize_variant_field(emit_context& em,
  const expr_variant *ev, const expr_var *fld, bool copy_flag)
{
  if (is_unit_type(fld->get_texpr())) {
    em.puts("/* unit */");
    return;
  }
  if (is_smallpod_type(fld->get_texpr())) {
    em.puts("*(");
    fld->emit_symbol(em);
    em.puts("$p()) = ");
    if (copy_flag) {
      em.puts("*(x.");
      fld->emit_symbol(em);
      em.puts("$p())");
    } else {
      em.puts("0");
    }
  } else {
    /* placement new */
    em.puts("new(");
    fld->emit_symbol(em);
    em.puts("$p()) ");
    emit_term(em, fld->get_texpr());
    if (copy_flag) {
      em.puts("(*(x.");
      fld->emit_symbol(em);
      em.puts("$p()))");
    } else {
      em.puts("()");
    }
  }
}

static void emit_deinitialize_variant_field(emit_context& em,
  const expr_variant *ev, const expr_var *fld)
{
  if (is_unit_type(fld->get_texpr())) {
    em.puts("/* unit */");
    return;
  }
  if (is_smallpod_type(fld->get_texpr())) {
    em.puts("/* pod */");
    return;
  } else {
    /* destructor */
    fld->emit_symbol(em);
    em.puts("$p()->");
    emit_term(em, fld->get_texpr());
    em.puts("::~");
    em.puts(to_short_name(get_term_cname(fld->get_texpr())));
    em.puts("()");
  }
}

static void emit_variant_def_one(emit_context& em, const expr_variant *ev,
  bool proto_only)
{
  if (!is_compiled(ev->block)) {
    return;
  }
  em.set_ns(ev->ns);
  em.set_file_line(ev);
  em.puts("struct ");
  const std::string name_c = get_type_cname_wo_ns(ev);
  em.puts(name_c);
  if (proto_only) {
    em.puts(";\n");
    return;
  }
  #if 0
  if (ev->inherit != 0) {
    expr_inherit *ih = ev->inherit;
    while (ih != 0) {
      if (ih == ev->inherit) {
	em.puts(" : ");
      } else {
	em.puts(", ");
      }
      const expr_interface *const ihdef =
	ptr_down_cast<const expr_interface>(ih->get_symdef());
      ihdef->emit_symbol(em);
      ih = ih->rest;
    }
  }
  #endif
  em.puts(" {\n");
  em.add_indent(1);
  typedef std::list<expr_var *> flds_type;
  flds_type flds;
  ev->get_fields(flds);
  flds_type::const_iterator i;
  /* enum */
  em.set_file_line(ev);
  em.indent('b');
  em.puts("enum {");
  for (i = flds.begin(); i != flds.end(); ++i) {
    if (i != flds.begin()) {
      em.puts(", ");
    }
    (*i)->emit_symbol(em);
    em.puts("$e");
  }
  em.puts("} $e;\n");
  bool has_non_unit = false;
  bool has_non_smallpod = false;
  for (i = flds.begin(); i != flds.end(); ++i) {
    if (!is_unit_type((*i)->get_texpr())) {
      has_non_unit = true;
    }
    if (!is_smallpod_type((*i)->get_texpr())) {
      has_non_smallpod = true;
    }
  }
  if (has_non_unit) {
    /* union */
    em.set_file_line(ev);
    em.indent('b');
    em.puts("union {\n");
    em.add_indent(1);
    for (i = flds.begin(); i != flds.end(); ++i) {
      if (is_unit_type((*i)->get_texpr())) {
	continue;
      }
      em.set_file_line(*i);
      em.indent('b');
      em.puts("char ");
      (*i)->emit_symbol(em);
      em.puts("$u[sizeof(");
      emit_term(em, (*i)->get_texpr());
      em.puts(")];\n");
    }
    em.add_indent(-1);
    em.set_file_line(ev);
    em.indent('b');
    em.puts("} $u;\n");
    /* field pointer */
    for (i = flds.begin(); i != flds.end(); ++i) {
      if (is_unit_type((*i)->get_texpr())) {
	continue;
      }
      /* const */
      em.set_file_line(*i);
      em.indent('b');
      emit_term(em, (*i)->get_texpr());
      em.puts(" const *");
      (*i)->emit_symbol(em);
      em.puts("$p() const { return (");
      emit_term(em, (*i)->get_texpr());
      em.puts(" const *)(void *)");
      em.puts("$u.");
      (*i)->emit_symbol(em);
      em.puts("$u; }\n");
      /* non-const */
      em.set_file_line(*i);
      em.indent('b');
      emit_term(em, (*i)->get_texpr());
      em.puts(" *");
      (*i)->emit_symbol(em);
      em.puts("$p() { return (");
      emit_term(em, (*i)->get_texpr());
      em.puts(" *)(void *)");
      em.puts("$u.");
      (*i)->emit_symbol(em);
      em.puts("$u; }\n");
    }
  }
  /* default constructor */
  em.set_file_line(ev);
  em.indent('b');
  em.puts(name_c);
  em.puts("()");
  #if 0
  if (ev->inherit) {
    em.puts(" : count$z(1)");
  }
  #endif
  em.puts(" {\n");
  em.add_indent(1);
  for (i = flds.begin(); i != flds.end(); ++i) {
    em.set_file_line(*i);
    em.indent('b');
    em.puts("$e = ");
    (*i)->emit_symbol(em);
    em.puts("$e;\n");
    em.set_file_line(*i);
    em.indent('b');
    emit_initialize_variant_field(em, ev, *i, false);
    em.puts(";\n");
    break;
  }
  em.add_indent(-1);
  em.set_file_line(ev);
  em.indent('b');
  em.puts("};\n");
  /* init */
  em.set_file_line(ev);
  em.indent('b');
  em.puts("void init$(const ");
  em.puts(name_c);
  em.puts(" & x) {\n");
  em.add_indent(1);
  em.set_file_line(ev);
  em.indent('b');
  em.puts("$e = x.$e;\n");
  if (has_non_unit) {
    em.set_file_line(ev);
    em.indent('b');
    em.puts("switch ($e) {\n");
    for (i = flds.begin(); i != flds.end(); ++i) {
      em.set_file_line(*i);
      em.indent('b');
      em.puts("case ");
      (*i)->emit_symbol(em);
      em.puts("$e: ");
      emit_initialize_variant_field(em, ev, *i, true);
      em.puts("; break;\n");
    }
    em.set_file_line(ev);
    em.indent('b');
    em.puts("}\n");
  }
  em.add_indent(-1);
  em.set_file_line(ev);
  em.indent('b');
  em.puts("};\n");
  /* deinit */
  em.set_file_line(ev);
  em.indent('b');
  em.puts("void deinit$() {\n");
  em.add_indent(1);
  if (has_non_smallpod) {
    em.set_file_line(ev);
    em.indent('b');
    em.puts("switch ($e) {\n");
    for (i = flds.begin(); i != flds.end(); ++i) {
      em.set_file_line(*i);
      em.indent('b');
      em.puts("case ");
      (*i)->emit_symbol(em);
      em.puts("$e: ");
      emit_deinitialize_variant_field(em, ev, *i);
      em.puts("; break;\n");
    }
    em.set_file_line(ev);
    em.indent('b');
    em.puts("}\n");
  }
  em.add_indent(-1);
  em.set_file_line(ev);
  em.indent('b');
  em.puts("};\n");
  /* destructor */
  em.set_file_line(ev);
  em.indent('b');
  em.puts("~");
  em.puts(name_c);
  em.puts("() { deinit$(); }\n");
  /* copy constructor */
  em.set_file_line(ev);
  em.indent('b');
  em.puts(name_c);
  em.puts("(const ");
  em.puts(name_c);
  em.puts("& x) { init$(x); }\n");
  /* operator = */
  em.set_file_line(ev);
  em.indent('b');
  em.puts(name_c);
  em.puts("& operator =(const ");
  em.puts(name_c);
  em.puts("& x) { if (this != &x) { deinit$(); init$(x); } return *this; }\n");
  /* rvalue */
  for (i = flds.begin(); i != flds.end(); ++i) {
    em.set_file_line(*i);
    em.indent('b');
    em.puts("const ");
    emit_term(em, (*i)->get_texpr());
    em.puts("& ");
    (*i)->emit_symbol(em);
    em.puts("$r() const {\n");
    em.add_indent(1);
    em.set_file_line(*i);
    em.indent('b');
    em.puts("if ($e != ");
    (*i)->emit_symbol(em);
    em.puts("$e) { pxcrt::throw_invalid_field(); }\n");
    em.set_file_line(*i);
    em.indent('b');
    if (!is_unit_type((*i)->get_texpr())) {
      em.puts("return *");
      (*i)->emit_symbol(em);
      em.puts("$p();\n");
    } else {
      /* null reference */
      em.puts("return *(const ");
      emit_term(em, (*i)->get_texpr());
      em.puts(" *)(const void *)0;\n");
    }
    em.add_indent(-1);
    em.set_file_line(*i);
    em.indent('b');
    em.puts("}\n");
  }
  /* lvalue */
  for (i = flds.begin(); i != flds.end(); ++i) {
    em.set_file_line(*i);
    em.indent('b');
    emit_term(em, (*i)->get_texpr());
    em.puts("& ");
    (*i)->emit_symbol(em);
    em.puts("$l() {\n");
    em.add_indent(1);
    em.set_file_line(*i);
    em.indent('b');
    em.puts("if ($e != ");
    (*i)->emit_symbol(em);
    em.puts("$e) { deinit$(); $e = ");
    (*i)->emit_symbol(em);
    em.puts("$e; ");
    emit_initialize_variant_field(em, ev, *i, false);
    em.puts("; }\n");
    em.set_file_line(*i);
    em.indent('b');
    if (!is_unit_type((*i)->get_texpr())) {
      em.puts("return *");
      (*i)->emit_symbol(em);
      em.puts("$p();\n");
    } else {
      /* null reference */
      em.puts("return *(");
      emit_term(em, (*i)->get_texpr());
      em.puts(" *)(void *)0;\n");
    }
    em.add_indent(-1);
    em.set_file_line(*i);
    em.indent('b');
    em.puts("}\n");
  }
  /* */
  em.add_indent(-1);
  em.puts("};\n");
}

struct sorted_exprs {
  std::list<expr_i *> sorted;
  std::set<expr_i *> pset;
  std::set<expr_i *> parents;
};

static std::list<expr_i *> get_dep_tparams(expr_struct *est)
{
  std::list<expr_i *> r;
  if (est == 0) {
    return r;
  }
  const term& te = est->get_value_texpr();
  const term_list *const args = te.get_args();
  size_t argslen = args != 0 ? args->size() : 0;
  const std::string cat = est->category ? est->category : "";
  if (cat == "farray") {
    if (argslen != 0) {
      term t = (*args)[0]; /* TODO: avoid copying */
      r.push_back(term_get_instance(t));
    }
  } else if (
    cat == "ref" ||
    cat == "cref" ||
    cat == "tref" ||
    cat == "tcref" ||
    cat == "varray" ||
    cat == "map") {
    /* no dep */
  } else {
    if (args != 0) {
      for (term_list::const_iterator i = args->begin(); i != args->end();
	++i) {
	term t = *i; /* TODO: avoid copying */
	r.push_back(term_get_instance(t));
      }
    }
  }
  return r;
}

static void sort_types(sorted_exprs& c, expr_i *e)
{
  if (e == 0) {
    return;
  }
  if (c.pset.find(e) != c.pset.end()) {
    return;
  }
  if (c.parents.find(e) != c.parents.end()) {
    arena_error_throw(e, "a type dependency cycle is found");
  }
  c.parents.insert(e);
  expr_block *block = 0;
  expr_struct *const est = dynamic_cast<expr_struct *>(e);
  if (est != 0) {
    block = est->block;
  }
  expr_variant *const ev = dynamic_cast<expr_variant *>(e);
  if (ev != 0) {
    block = ev->block;
  }
  if (block != 0 && is_compiled(block)) {
    const symbol_table& st = block->symtbl;
    symbol_table::locals_type::const_iterator i;
    for (i = st.locals.begin(); i != st.locals.end(); ++i) {
      expr_var *const ev = dynamic_cast<expr_var *>(i->second.edef);
      if (ev == 0) {
	continue;
      }
      term te = ev->get_texpr(); /* TODO: eliminate copying */
      expr_i *const einst = term_get_instance(te);
      sort_types(c, einst);
    }
    typedef std::list<expr_i *> deps_type;
    deps_type deps = get_dep_tparams(est);
    for (deps_type::iterator i = deps.begin(); i != deps.end(); ++i) {
      sort_types(c, *i);
    }
    #if 0
    if (est != 0 && est->category != 0 &&
      std::string(est->category) == "farray") {
      /* fixed-size array requires its first param to be a complete type */
      const term& te = est->get_value_texpr();
      const term_list *const args = te.get_args();
      assert(args);
      assert(!args->empty());
      term tp = args->front(); /* TODO: eliminate copying */
      expr_i *const einst = term_get_instance(tp);
      sort_types(c, einst);
    }
    #endif
  }
  c.parents.erase(e);
  c.sorted.push_back(e);
  c.pset.insert(e);
}

static void emit_inline_c(emit_context& em, const std::string& posstr,
  bool is_decl)
{
  for (expr_arena_type::iterator i = expr_arena.begin();
    i != expr_arena.end(); ++i) {
    expr_inline_c *const ei = dynamic_cast<expr_inline_c *>(*i);
    if (ei == 0) { continue; }
    if (ei->posstr != posstr) { continue; }
    if (!is_decl && ei->declonly) { continue; }
    const std::string cstr(ei->cstr);
    em.set_file_line(ei);
    em.puts("/* inline */\n");
    em.puts(cstr.c_str());
    if (cstr.size() > 0 && cstr[cstr.size() - 1] != '\n') {
      em.puts("\n");
    }
  }
}

static void emit_type_definitions(emit_context& em)
{
  sorted_exprs c;
  for (expr_arena_type::iterator i = expr_arena.begin();
    i != expr_arena.end(); ++i) {
    expr_i *const e = *i;
    if (e->get_esort() == expr_e_struct || e->get_esort() == expr_e_variant) {
      sort_types(c, e);
    }
  }
  /* proto */
  for (expr_arena_type::iterator i = expr_arena.begin();
    i != expr_arena.end(); ++i) {
    expr_interface *const ei = dynamic_cast<expr_interface *>(*i);
    if (ei != 0) {
      const bool proto_only = true;
      emit_interface_def_one(em, ei, proto_only);
    }
  }
  for (std::list<expr_i *>::iterator i = c.sorted.begin();
    i != c.sorted.end(); ++i) {
    const expr_struct *const est = dynamic_cast<const expr_struct *>(*i);
    if (est != 0) {
      if (est->cname == 0) {
	const bool proto_only = true;
	emit_struct_def_one(em, est, proto_only);
      }
    }
    const expr_variant *const ev = dynamic_cast<const expr_variant *>(*i);
    if (ev != 0) {
      const bool proto_only = true;
      emit_variant_def_one(em, ev, proto_only);
    }
  }
  /* definitions */
  for (expr_arena_type::iterator i = expr_arena.begin();
    i != expr_arena.end(); ++i) {
    expr_interface *const ei = dynamic_cast<expr_interface *>(*i);
    if (ei != 0) {
      const bool proto_only = false;
      emit_interface_def_one(em, ei, proto_only);
    }
  }
  for (std::list<expr_i *>::iterator i = c.sorted.begin();
    i != c.sorted.end(); ++i) {
    const expr_struct *const est = dynamic_cast<const expr_struct *>(*i);
    if (est != 0 && est->cname == 0) {
      const bool proto_only = false;
      emit_struct_def_one(em, est, proto_only);
    }
    const expr_variant *const ev = dynamic_cast<const expr_variant *>(*i);
    if (ev != 0) {
      const bool proto_only = false;
      emit_variant_def_one(em, ev, proto_only);
    }
  }
}

static void emit_thisptr_argdecl(emit_context& em, expr_struct *est,
  bool is_const)
{
  emit_term(em, est->value_texpr);
  if (is_const) {
    em.puts(" const");
  }
  em.puts("& this$up");
}

#if 0
static bool is_global_var(expr_i *e)
{
  expr_var *const ev = dynamic_cast<expr_var *>(e);
  if (ev == 0) {
    return false;
  }
  symbol_table *const stbl = ev->symtbl_lexical;
  return (stbl->get_lexical_parent() == 0);
}
#endif

static void emit_function_argdecls(emit_context& em, expr_funcdef *efd)
{
  bool is_first = true;
  /* upvalues */
  expr_funcdef::tpup_vec_type::const_iterator i;
  for (i = efd->tpup_vec.begin(); i != efd->tpup_vec.end(); ++i) {
    if (is_global_var(*i)) {
      continue;
    }
    if (!is_first) {
      em.puts(", ");
    } else {
      is_first = false;
    }
    if ((*i)->get_esort() == expr_e_var) {
      emit_var_cdecl(em, ptr_down_cast<expr_var>(*i), true, true);
	// (*i)->emit_cdecl(em, true, true);
    } else {
      emit_arg_cdecl(em, ptr_down_cast<expr_argdecls>(*i), true);
	// (*i)->emit_cdecl(em, true, true);
    }
  }
  /* upvalue thisptr */
  if (efd->tpup_thisptr != 0) {
    if (!is_first) {
      em.puts(", ");
    } else {
      is_first = false;
    }
    emit_thisptr_argdecl(em, efd->tpup_thisptr, !efd->tpup_thisptr_nonconst);
  }
  emit_argdecls(em, efd->block->argdecls, is_first);
}

static void emit_function_decl_one(emit_context& em, expr_funcdef *efd,
  bool set_ns, bool memfunc_ext)
{
  if (efd->no_def && !efd->is_virtual_function()) {
    em.puts("/* nodef */");
    return; /* external C function */
  }
  if (set_ns) {
    em.set_ns(efd->ns);
  }
  {
    /* a simple function, a member function, or a virtual function */
    const std::string name_c = get_type_cname_wo_ns(efd);
    DBG_DECL(fprintf(stderr, "decl_one nam_c=%s\n", name_c.c_str()));
    if (efd->block->tinfo.template_descent) {
      em.puts("inline ");
    }
    expr_struct *const esd = efd->is_member_function();
    emit_term(em, efd->get_rettype());
    em.puts(" ");
    if (esd != 0 && memfunc_ext) {
      em.puts(get_type_cname_wo_ns(esd));
      em.puts("::");
    }
    em.puts(name_c);
    em.puts("(");
    emit_function_argdecls(em, efd);
    em.puts(")");
    if (efd->is_const) {
      em.puts(" const");
    }
  }
}

#if 0
static void emit_vardecl(emit_context& em, const expr_i *e)
{
  em.puts(term_tostr(e->get_texpr(), term_tostr_sort_cname));
  em.puts(" ");
  e->emit_symbol(em);
}
#endif

static void emit_struct_constr_one(emit_context& em, expr_struct *est,
  bool emit_default_constr)
{
  if (elist_length(est->block->argdecls) == 0 && !emit_default_constr) {
    return;
  }
  const std::string name_c = get_type_cname_wo_ns(est);
  typedef std::list<expr_var *> flds_type;
  flds_type flds;
  est->get_fields(flds);
  flds_type::const_iterator i;
  em.set_ns(est->ns);
  /* user defined constructor */
  em.set_file_line(est);
  em.indent('b');
  em.puts(name_c);
  em.puts("::");
  em.puts(name_c);
  em.puts("(");
  if (!emit_default_constr) {
    emit_argdecls(em, est->block->argdecls, true);
  }
  em.puts(") ");
  emit_struct_constr_initializer(em, est, flds, true);
  em.puts(" {\n");
  em.add_indent(1);
  if (emit_default_constr) {
    for (expr_argdecls *a = est->block->argdecls; a; a = a->rest) {
      em.indent('b');
      emit_arg_cdecl(em, a, false); // a->emit_cdecl(em, false, false);
      emit_explicit_init_if(em, a->get_texpr());
      em.puts(";\n");
    }
  }
  em.indent('b');
  em.puts("this->init$z(");
  bool is_first = true;
  for (expr_argdecls *a = est->block->argdecls; a; a = a->rest) {
    if (is_first) {
      is_first = false;
    } else {
      em.puts(", ");
    }
    a->emit_symbol(em);
  }
  em.puts(");\n");
  em.add_indent(-1);
  em.indent('b');
  em.puts("}\n");
}

static void emit_struct_constr_aux(emit_context& em, expr_struct *est)
{
  const std::string name_c = get_type_cname_wo_ns(est);
  typedef std::list<expr_var *> flds_type;
  flds_type flds;
  est->get_fields(flds);
  flds_type::const_iterator i;
  em.set_ns(est->ns);
  /* user defined constructor */
  em.set_file_line(est);
  em.indent('b');
  em.puts("void ");
  em.puts(name_c);
  em.puts("::init$z");
  em.puts("(");
  emit_argdecls(em, est->block->argdecls, true);
  em.puts(") ");
  fn_emit_value(em, est->block);
  em.puts("\n");
}

static void emit_function_def_one(emit_context& em, expr_funcdef *efd)
{
  if ((efd->ext_decl && !efd->block->tinfo.template_descent) || efd->no_def) {
    return;
  }
  if (efd->is_member_function()) {
    expr_struct *const est = efd->is_member_function();
    em.set_ns(est->ns);
  } else {
    em.set_ns(efd->ns);
  }
  if (efd->is_member_function()) {
    emit_function_decl_one(em, efd, false, true);
    em.puts(" ");
    fn_emit_value(em, efd->block);
    em.puts("\n");
  } else {
    emit_function_decl_one(em, efd, false, false);
    em.puts(" ");
    fn_emit_value(em, efd->block);
    em.puts("\n");
  }
}

static void emit_function_decl(emit_context& em)
{
  for (expr_arena_type::iterator i = expr_arena.begin();
    i != expr_arena.end(); ++i) {
    expr_funcdef *const efd = dynamic_cast<expr_funcdef *>(*i);
    if (efd != 0 && !efd->is_member_function() &&
      !efd->is_virtual_function() && is_compiled(efd->block)) {
      if (!efd->no_def || efd->is_virtual_function()) {
	emit_function_decl_one(em, efd, true, false);
	em.puts(";\n");
      }
    }
    expr_ns *const ens = dynamic_cast<expr_ns *>(*i);
    if (ens != 0 && ens->import) {
      const std::string fn = arena_get_ns_main_funcname(ens->nsstr);
      em.finish_ns();
      em.printf("extern \"C\" void %s$c();\n", fn.c_str());
      em.start_ns();
    }
  }
}

static void emit_function_def(emit_context& em)
{
  for (expr_arena_type::iterator i = expr_arena.begin();
    i != expr_arena.end(); ++i) {
    expr_funcdef *const efd = dynamic_cast<expr_funcdef *>(*i);
    if (efd != 0 && is_compiled(efd->block)) {
      emit_function_def_one(em, efd);
    }
    expr_struct *const est = dynamic_cast<expr_struct *>(*i);
    if (est != 0 && est->has_userdefined_constr()) {
      emit_struct_constr_one(em, est, true);
      emit_struct_constr_one(em, est, false);
      emit_struct_constr_aux(em, est);
    }
  }
}

static void emit_import_init(emit_context& em, const std::string& main_ns)
{
  /* TODO: this impl is not efficient at runtime */
  std::set<std::string> ss;
  for (expr_arena_type::iterator i = expr_arena.begin();
    i != expr_arena.end(); ++i) {
    expr_ns *const ens = dynamic_cast<expr_ns *>(*i);
    if (ens != 0 && ens->import) {
      const std::string fn = arena_get_ns_main_funcname(ens->nsstr);
      if (ss.find(fn) != ss.end()) {
	continue;
      }
      ss.insert(fn);
      em.set_file_line(ens);
      em.indent('i');
      em.printf("%s$c();\n", fn.c_str());
    }
  }
}

void expr_inline_c::emit_value(emit_context& em)
{
  em.puts("/* inline_c */");
}

void expr_ns::emit_value(emit_context& em)
{
  if (import) {
    if (pub) {
      em.printf("/* public import %s */", nsstr.c_str());
    } else {
      em.printf("/* private import %s */", nsstr.c_str());
    }
  } else {
    em.printf("/* namespace %s */", nsstr.c_str());
  }
}

void expr_int_literal::emit_value(emit_context& em)
{
  if (is_unsigned) {
    em.printf("%lluULL", get_unsigned());
  } else {
    em.printf("%lldLL", get_signed());
  }
}

void expr_float_literal::emit_value(emit_context& em)
{
  em.puts(str);
}

void expr_bool_literal::emit_value(emit_context& em)
{
  if (value) {
    em.puts("true");
  } else {
    em.puts("false");
  }
}

void expr_str_literal::emit_value(emit_context& em)
{
  em.puts(escape_c_str_literal(value));
}

static bool is_field_w_explicit_obj(const expr_i *e)
{
  const expr_op *const eop = dynamic_cast<const expr_op *>(e->parent_expr); 
  if (eop == 0) {
    return false;
  }
  return (eop->op == '.' || eop->op == TOK_ARROW);
}

static void emit_value_symdef_common(emit_context& em, symbol_common& sdef,
  const expr_i *e)
{
  if (sdef.get_evaluated().is_long() || sdef.get_evaluated().is_string()) {
    em.puts(term_tostr_cname(sdef.get_evaluated()));
    return;
  }
  if (is_type(sdef.get_evaluated()) || is_function(sdef.get_evaluated())) {
    DBG_EMIT_SYM(fprintf(stderr, "SYM TE %s %s:%d\n",
      get_term_cname(sdef.get_evaluated()).c_str(), e->fname, e->line));
    std::string s = get_term_cname(sdef.get_evaluated());
    const expr_funcdef *const efd = dynamic_cast<const expr_funcdef *>(
      term_get_instance_const(sdef.get_evaluated()));
    {
      DBG_EMIT_SYM(fprintf(stderr, "->NOT FUNCOBJ %s\n", s.c_str()));
      if (efd != 0 &&
	(efd->is_member_function() || efd->is_virtual_function())) {
	s = to_short_name(s); /* no namespace */
      }
    }
    em.puts(s);
    return;
  }
  DBG_EMIT_SYM(fprintf(stderr, "SYM NOTE %s\n",
    sdef.symbol_def->emit_symbol_str().c_str()));
  expr_i *const edef = sdef.get_evaluated().get_expr();
  if (is_field_w_explicit_obj(e)) { /* TODO: not smart? */
    fn_emit_value(em, edef);
    return;
  }
  /* check if it's an imported member field */
  expr_i *const def_fr = get_current_frame_expr(edef->symtbl_lexical);
  expr_struct *const def_est = dynamic_cast<expr_struct *>(def_fr);
  expr_i *const use_fr = get_current_frame_expr(e->symtbl_lexical);
  expr_funcdef *const use_efd = dynamic_cast<expr_funcdef *>(use_fr);
  if (def_est != 0 && use_efd != 0 &&
    def_est != use_efd->is_member_function()) {
    em.puts("(this$up.");
    fn_emit_value(em, edef);
    em.puts(")");
    return;
  }
  fn_emit_value(em, edef);
}

void expr_te::emit_value(emit_context& em)
{
  emit_value_symdef_common(em, sdef, this);
}

void expr_symbol::emit_value(emit_context& em)
{
  emit_value_symdef_common(em, sdef, this);
}

std::string expr_var::emit_symbol_str() const
{
  return csymbol_var(this, false);
}

void expr_var::emit_symbol(emit_context& em) const
{
  em.puts(emit_symbol_str());
}

#if 0
void expr_var::emit_cdecl(emit_context& em, bool is_argdecl, bool byref) const
{
  emit_var_cdecl(em, this, is_argdecl, byref);
  #if 0
  if (is_argdecl) {
    emit_typestr_call_traits(em, type_of_this_expr, byref);
  } else {
    emit_term(em, type_of_this_expr);
    if (byref) {
      em.puts("&");
    }
  }
  em.puts(" ");
  em.puts(csymbol_var(this, true));
  // this->emit_symbol(em);
  #endif
}
#endif

void expr_var::emit_value(emit_context& em)
{
  this->emit_symbol(em);
}

std::string expr_extval::emit_symbol_str() const
{
  return std::string(cname);
}

void expr_extval::emit_symbol(emit_context& em) const
{
  em.puts(emit_symbol_str());
}

void expr_extval::emit_value(emit_context& em)
{
  this->emit_symbol(em);
}

static bool cur_block_is_struct(expr_i *e)
{
  symbol_table *stbl = e->symtbl_lexical;
  return stbl->block_esort == expr_e_struct;
}

static bool cur_block_is_global(expr_i *e)
{
  symbol_table *stbl = e->symtbl_lexical;
  return stbl->get_lexical_parent() == 0;
}

static bool esort_noemit_funcbody(expr_i *e)
{
  switch (e->get_esort()) {
  case expr_e_typedef:
  case expr_e_macrodef:
  case expr_e_ns:
  case expr_e_inline_c:
  case expr_e_extval:
  case expr_e_struct:
  case expr_e_variant:
  case expr_e_interface:
  case expr_e_funcdef:
    return true;
  default:
    /* expr_e_if, expr_e_var, expr_e_op for example */
    DBG_IF(fprintf(stderr, "esort_noemit_funcbody: false %s\n",
      e->dump(0).c_str()));
    return false;
  }
}

static void emit_sep_vardef_one(emit_context& em, expr_var *ev, expr_i *etv,
  bool emit_blockscope_vars)
{
  if (ev != 0) {
  } else {
    assert(etv != 0);
  }
  // FIXME HERE HERE HERE
}

static void emit_sep_vardefs(emit_context& em, const asgnstmt_list& asts,
  bool emit_blockscope_vars)
{
  asgnstmt_e tv_to_emit = emit_blockscope_vars
    ? asgnstmt_e_blockscope_tempvar : asgnstmt_e_stmtscope_tempvar;
  asgnstmt_list::const_iterator i;
  for (i = asts.begin(); i != asts.end(); ++i) {
    const asgnstmt& ast = *i;
    if (ast.astmt == asgnstmt_e_var_def) {
      expr_var *const ev = ptr_down_cast<expr_var>(ast.top);
      emit_sep_vardef_one(em, ev, 0, emit_blockscope_vars);
    } else if (ast.astmt == asgnstmt_e_var_defset) {
      expr_op *const eop = ptr_down_cast<expr_op>(ast.top);
      expr_var *const ev = ptr_down_cast<expr_var>(eop->arg0);
      emit_sep_vardef_one(em, ev, 0, emit_blockscope_vars);
    } else if (ast.astmt == tv_to_emit) { /* tempvar */
      emit_sep_vardef_one(em, 0, ast.top, emit_blockscope_vars);
    } else {
      continue;
    }
  }
}

static void emit_asgnstmt_list(emit_context& em, const asgnstmt_list& asts,
  expr_i *eprev, expr_i *e)
{
  // FIXME HERE HERE HERE
}

static void emit_one_statement(emit_context& em, expr_stmts *stmts)
{
  const bool is_struct_constr = cur_block_is_struct(stmts);
  const bool is_global_block = cur_block_is_global(stmts);
  const bool is_condblock = is_condblock_stmt(stmts->head);
  bool has_stmt_scope_var = false;
  bool has_block_scope_var = false;
  asgnstmt_list::const_iterator i;
  for (i = stmts->asts.begin(); i != stmts->asts.end(); ++i) {
    switch (i->astmt) {
    case asgnstmt_e_stmtscope_tempvar:
      has_stmt_scope_var = true;
      break;
    case asgnstmt_e_blockscope_tempvar:
    case asgnstmt_e_var_defset:
    case asgnstmt_e_var_def:
      has_block_scope_var = true;
      break;
    case asgnstmt_e_other:
      break;
    }
  }
  const bool blockscope_defset_sep =
    is_condblock || has_stmt_scope_var || is_struct_constr || is_global_block;

  emit_context_stmt sct;
  sct.is_blockcond = is_condblock;
  sct.sep_blockscope_var = blockscope_defset_sep;
  sct.is_struct_or_global_block = is_struct_constr || is_global_block;
  stmt_context_scoped scoped(em, sct);

  if (!blockscope_defset_sep) {
    /* (1) block vars are defset-direct. no stmt var. */
    emit_asgnstmt_list(em, stmts->asts, 0, 0);
  } else if (!is_condblock) {
    /* (2) block vars are defset-sep. stmt vars are defset-direct. */
    if (!is_struct_constr || !is_global_block) {
      /* emit block var defs */
      emit_sep_vardefs(em, stmts->asts, true);
    }
    /* emit asgnstmt_list */
    em.set_file_line(stmts->head);
    em.add_indent(1);
    em.indent('s');
    em.puts("{\n");
    emit_asgnstmt_list(em, stmts->asts, 0, 0);
    em.add_indent(-1);
    em.indent('s');
    em.puts("}\n");
  } else {
    /* (3) condblock. all vars are defset-sep. */
    /* emit block var defs */
    emit_sep_vardefs(em, stmts->asts, true);
    /* emit stmt var defs */
    if (has_stmt_scope_var) {
      em.set_file_line(stmts->head);
      em.add_indent(1);
      em.indent('s');
      em.puts("{\n");
      emit_sep_vardefs(em, stmts->asts, false);
    }
    /* emit stmt */
    fn_emit_value(em, stmts->head);
    if (has_stmt_scope_var) {
      em.set_file_line(stmts->head);
      em.add_indent(-1);
      em.indent('s');
      em.puts("}\n");
    }
  }
}

void expr_stmts::emit_value(emit_context& em)
{
  if (head == 0) {
    assert(rest == 0);
    return;
  }
  const bool is_struct_block = cur_block_is_struct(this);
  const bool has_tv = cur_stmt_has_tempvar(head);
  const bool is_condblock = is_condblock_stmt(head); /* if, while, for */
  if (!is_struct_block && !has_tv && emit_local_decl_fastinit(em)) {
    /* var x:foo = ... */
  } else if (!esort_noemit_funcbody(head)) {
    if (!is_struct_block) {
      emit_local_decl(em);
    }
    if (head->get_esort() == expr_e_var ||
      head->get_esort() == expr_e_symbol) {
      /* stmt has no effect */
    } else {
      if (has_tv) {
	em.set_file_line(head);
	em.indent('s');
	em.puts("{\n");
	if (is_condblock) {
	  emit_tempvars_def(em, head, tempvars_def_e_def);
	    /* values will be set later */
	} else {
	  emit_tempvars_def(em, head, tempvars_def_e_def_set);
	}
      }
      em.set_file_line(head);
      em.indent('s');
      fn_emit_value(em, head);
      if (is_block_stmt(head)) {
	em.puts("\n");
      } else {
	em.puts(";\n");
      }
      if (has_tv) {
	em.set_file_line(head);
	em.indent('s');
	em.puts("}\n");
      }
    }
  }
  if (rest != 0) {
    fn_emit_value(em, rest);
  }
}

bool expr_stmts::emit_local_decl_fastinit(emit_context& em)
{
  if (symtbl_lexical->get_lexical_parent() == 0) {
    /* global context */
    return false;
  }
  symbol_table::locals_type::const_iterator decl_cur_stmt
    = symtbl_lexical->locals.end();
  for (symbol_table::locals_type::const_iterator i =
    symtbl_lexical->locals.begin(); i != symtbl_lexical->locals.end(); ++i) {
    if (i->second.stmt == this) {
      if (decl_cur_stmt != symtbl_lexical->locals.end()) {
	return false; /* more than one vars are declared in this stmt */
      }
      decl_cur_stmt = i;
    }
  }
  if (decl_cur_stmt == symtbl_lexical->locals.end()) {
    return false; /* no var is declared in this stmt */
  }
  if (head->get_esort() != expr_e_op) {
    return false;
  }
  expr_op *const eop = ptr_down_cast<expr_op>(head);
  if (eop->op != '=') {
    return false;
  }
  expr_i *const lhs = eop->arg0;
  if (lhs->get_esort() != expr_e_var) {
    return false;
  }
  expr_var *const ev = ptr_down_cast<expr_var>(lhs);
  if (decl_cur_stmt->second.edef != ev) {
    return false;
  }
  /* head is of the form 'var v:foo = ...' */
  em.set_file_line(ev);
  em.indent('b');
  #if 0
  /* pointer initialization? */
  expr_funccall *fc = dynamic_cast<expr_funccall *>(eop->arg1);
  if (eop->arg1->conv == conversion_e_boxing &&
    fc != 0 && fc->funccall_sort != funccall_e_funccall /* type */) {
    term& te = fc->func->resolve_texpr();
    expr_i *einst = term_get_instance(te);
    emit_vardecl(em, ev);
    em.puts("(new ");
    fn_emit_value(em, eop->arg1, false);
    em.puts(")");
  } else 
  #endif
  {
    emit_var_cdecl(em, ev, false, false); // ev->emit_cdecl(em, false, false);
    em.puts(" = ");
    fn_emit_value(em, eop->arg1);
  }
  em.puts(";\n");
  return true;
}

static void emit_global_vars(emit_context& em, expr_block *gl_blk)
{
  symbol_table *const symtbl = &gl_blk->symtbl;
  symbol_table::local_names_type::const_iterator i;
  for (i = symtbl->local_names.begin(); i != symtbl->local_names.end(); ++i) {
    const symbol_table::locals_type::const_iterator iter
      = symtbl->locals.find(*i);
    assert(iter != symtbl->locals.end());
    const expr_var *const e = dynamic_cast<const expr_var *>(
      iter->second.edef);
    if (e == 0) { continue; }
    const bool is_main_ns = (e->ns == main_namespace);
    em.set_ns(e->ns);
    em.set_file_line(e);
    em.indent('b');
    if (!is_main_ns) {
      em.puts("extern ");
    }
    emit_var_cdecl(em, e, false, false); // e->emit_cdecl(em, false, false);
    if (is_main_ns) {
      emit_explicit_init_if(em, e->get_texpr());
    }
    em.puts(";\n");
  }
}

void expr_stmts::emit_local_decl(emit_context& em)
{
  if (symtbl_lexical->get_lexical_parent() == 0) {
    /* global context */
    return;
  }
  symbol_table::local_names_type::const_iterator i;
  for (i = symtbl_lexical->local_names.begin();
    i != symtbl_lexical->local_names.end(); ++i) {
    const symbol_table::locals_type::const_iterator iter
      = symtbl_lexical->locals.find(*i);
    assert(iter != symtbl_lexical->locals.end());
    if (iter->second.stmt != this) {
      continue;
    }
    const expr_var *const e = dynamic_cast<const expr_var *>(
      iter->second.edef);
    if (e == 0) { continue; }
    em.set_file_line(e);
    em.indent('b');
    emit_var_cdecl(em, e, false, false); // e->emit_cdecl(em, false, false);
    emit_explicit_init_if(em, e->get_texpr());
    em.puts(";\n");
  }
}

void expr_block::emit_value(emit_context& em)
{
  em.puts("{\n");
  em.add_indent(1);
  emit_value_nobrace(em);
  em.add_indent(-1);
  em.indent('b');
  em.puts("}");
}

void expr_block::emit_value_nobrace(emit_context& em)
{
  this->emit_local_decl(em, true);
  if (stmts) {
    fn_emit_value(em, stmts);
  }
}

void expr_block::emit_local_decl(emit_context& em, bool is_funcbody)
{
  symbol_table::local_names_type::const_iterator i;
  for (i = symtbl.local_names.begin(); i != symtbl.local_names.end(); ++i) {
    const symbol_table::locals_type::const_iterator iter
      = symtbl.locals.find(*i);
    assert(iter != symtbl.locals.end());
    if (is_funcbody && iter->second.stmt != 0) {
      continue; /* will be defined on each stmt */
    }
    expr_var *const e = dynamic_cast<expr_var *>(iter->second.edef);
    if (e == 0) {
      continue;
    }
    em.set_file_line(e);
    em.indent('b');
    emit_var_cdecl(em, e, false, false); // e->emit_cdecl(em, false, false);
    em.puts(";\n");
  }
}

static void emit_memberfunc_decl_one(emit_context&em, expr_funcdef *efd,
  bool pure_virtual)
{
  if (efd == 0) { return; }
  template_info::instances_type::iterator i;
  for (i = efd->block->tinfo.instances.begin(); 
    i != efd->block->tinfo.instances.end(); ++i) {
    expr_i *const einst = i->second->parent_expr;
    expr_funcdef *const cefd = ptr_down_cast<expr_funcdef>(einst);
    emit_memberfunc_decl_one(em, cefd, pure_virtual);
  }
  if (!is_compiled(efd->block)) { return; }
  em.set_file_line(efd);
  em.indent('b');
  if (pure_virtual) {
    em.puts("virtual ");
  }
  emit_function_decl_one(em, efd, false, false);
  if (pure_virtual) {
    em.puts(" { pxcrt::throw_virtual_function_call(); }\n");
  } else {
    em.puts(";\n");
  }
}

void expr_block::emit_memberfunc_decl(emit_context& em, bool pure_virtual)
{
  symbol_table::local_names_type::const_iterator i;
  for (i = symtbl.local_names.begin(); i != symtbl.local_names.end(); ++i) {
    const symbol_table::locals_type::const_iterator iter
      = symtbl.locals.find(*i);
    assert(iter != symtbl.locals.end());
    expr_funcdef *const efd = dynamic_cast<expr_funcdef *>(iter->second.edef);
    emit_memberfunc_decl_one(em, efd, pure_virtual);
  }
}

void emit_array_elem_or_slice_expr(emit_context& em, expr_op *eop)
{
  if (eop->arg1 != 0 && eop->arg1->get_esort() == expr_e_op &&
    ptr_down_cast<expr_op>(eop->arg1)->op == TOK_SLICE) {
    expr_op *const sliop = ptr_down_cast<expr_op>(eop->arg1);
    std::string tc = get_term_cname(eop->get_texpr());
    em.puts(tc);
    em.puts("(");
    fn_emit_value(em, eop->arg0);
    em.puts(", ");
    fn_emit_value(em, sliop->arg0);
    em.puts(", ");
    fn_emit_value(em, sliop->arg1);
    em.puts(")");
  } else {
    fn_emit_value(em, eop->arg0);
    em.puts("[");
    fn_emit_value(em, eop->arg1);
    em.puts("]");
  }
}

void expr_op::emit_value(emit_context& em)
{
  switch (op) {
  case '(':
    em.puts("(");
    fn_emit_value(em, arg0);
    em.puts(")");
    return;
  case '[':
    emit_array_elem_or_slice_expr(em, this);
    return;
  case '!':
    em.puts("!");
    fn_emit_value(em, arg0);
    return;
  case '~':
    em.puts("~");
    fn_emit_value(em, arg0);
    return;
  case TOK_PLUS:
    em.puts("+");
    fn_emit_value(em, arg0);
    return;
  case TOK_MINUS:
    em.puts("-");
    fn_emit_value(em, arg0);
    return;
  case TOK_INC:
    em.puts("++");
    fn_emit_value(em, arg0);
    return;
  case TOK_DEC:
    em.puts("--");
    fn_emit_value(em, arg0);
    return;
  case TOK_PTR_REF:
    em.printf("%s(", term_tostr(get_texpr(),
      term_tostr_sort_cname).c_str());
    fn_emit_value(em, arg0);
    em.puts(")");
    return;
  case TOK_PTR_DEREF:
    {
      const std::string cat = get_category(arg0->get_texpr());
      if (is_noninterface_pointer(arg0->get_texpr())) {
	#if 0
	em.puts("(pxcrt::deref(");
	fn_emit_value(em, arg0);
	em.puts("),");
	#endif
	em.puts("(");
	if (cat == "tref" || cat == "tcref") {
	  em.puts("pxcrt::lockobject((");
	  fn_emit_value(em, arg0);
	  em.puts(")->get_mutex$z()),");
	}
	fn_emit_value(em, arg0);
	em.puts(")->value$z");
      } else {
	#if 0
	em.puts("*(pxcrt::deref(");
	fn_emit_value(em, arg0);
	em.puts("),");
	#endif
	em.puts("*(");
	if (cat == "tref" || cat == "tcref") {
	  em.puts("pxcrt::lockobject((");
	  fn_emit_value(em, arg0);
	  em.puts(")->get_mutex$z()),");
	}
	fn_emit_value(em, arg0);
	em.puts(")");
      }
    }
    return;
  case '.':
  case TOK_ARROW:
    if (is_variant(arg0->get_texpr())) {
      em.puts("(");
      fn_emit_value(em, arg0);
      em.puts(".");
      fn_emit_value(em, arg1);
      if (arg0->require_lvalue) {
	em.puts("$l())");
      } else {
	em.puts("$r())");
      }
    } else {
      fn_emit_value(em, arg0);
      em.puts(".");
      fn_emit_value(em, arg1);
    }
    return;
  case TOK_CASE:
    {
      expr_i *a = arg0;
      expr_op *aop = ptr_down_cast<expr_op>(a);
      while (true) {
	if (aop->op == '.') {
	  break;
	}
	assert(aop->op == '(');
	aop = ptr_down_cast<expr_op>(aop->arg0);
      }
      em.puts("(");
      fn_emit_value(em, aop->arg0);
      em.puts(".$e == ");
      em.puts(term_tostr(aop->arg0->get_texpr(),
	term_tostr_sort_cname));
      em.puts("::");
      fn_emit_value(em, aop->arg1);
      em.puts("$e)");
    }
    return;
  default:
    break;
  }
  fn_emit_value(em, arg0);
  em.puts(tok_string(this, op));
  fn_emit_value(em, arg1);
}

static bool emit_func_upvalue_args(emit_context& em, expr_i *func,
  symbol_table *caller_symtbl)
{
  symbol_common *const sdef = func->get_sdef();
  if (sdef == 0) {
    return false;
  }
  const expr_funcdef *const efd = dynamic_cast<const expr_funcdef *>(
    term_get_instance_const(sdef->get_evaluated()));
  if (efd == 0) {
    return false;
  }
  bool is_first = true;
  /* upvalues */
  expr_funcdef::tpup_vec_type::const_iterator i;
  for (i = efd->tpup_vec.begin(); i != efd->tpup_vec.end(); ++i) {
    if (is_global_var(*i)) {
      continue;
    }
    if (!is_first) {
      em.puts(", ");
    } else {
      is_first = false;
    }
    (*i)->emit_symbol(em);
  }
  /* upvalue thisptr */
  if (efd->tpup_thisptr != 0) {
    if (!is_first) {
      em.puts(", ");
    } else {
      is_first = false;
    }
    expr_i *const caller_fr_expr = get_current_frame_expr(caller_symtbl);
    if (caller_fr_expr != 0 && caller_fr_expr->get_esort() == expr_e_funcdef &&
      ptr_down_cast<expr_funcdef>(caller_fr_expr)->is_member_function()
	== efd->tpup_thisptr) {
      em.puts("*this");
    } else {
      em.puts("this$up");
    }
  }
  return !is_first;
}

static void emit_function_expr(emit_context& em, expr_i *func)
{
  const bool has_explicit_obj = (func->get_esort() == expr_e_op);
  if (has_explicit_obj) {
    /* obj.method(...) */
    fn_emit_value(em, func);
    return;
  }
  symbol_common *const sdef = func->get_sdef();
  assert(sdef != 0); /* must be a symbol */
  const expr_funcdef *const efd = ptr_down_cast<const expr_funcdef>(
    term_get_instance_const(sdef->get_evaluated()));
  if (!efd->is_member_function()) {
    /* func(...) */
    fn_emit_value(em, func);
    return;
  }
  expr_i *const caller_fr_expr = get_current_frame_expr(func->symtbl_lexical);
  if (caller_fr_expr != 0 && caller_fr_expr->get_esort() == expr_e_funcdef &&
    !ptr_down_cast<expr_funcdef>(caller_fr_expr)->is_member_function()) {
    /* calling member function from non-member function. */
    em.puts("(this$up.");
    fn_emit_value(em, func);
    em.puts(")");
    return;
  }
  fn_emit_value(em, func);
}

static size_t comma_sep_length(expr_i *e)
{
  size_t len = 0;
  expr_op *const eop = dynamic_cast<expr_op *>(e);
  if (eop == 0 || eop->op != ',') {
    if (e != 0) {
      ++len;
    }
    return len;
  }
  len += comma_sep_length(eop->arg0);
  if (eop->arg1 != 0) {
    ++len;
  }
  return len;
}

void expr_funccall::emit_value(emit_context& em)
{
  expr_i *fld = get_op_rhs(func, '.');
  if (fld == 0) {
    fld = get_op_rhs(func, TOK_ARROW);
  }
  symbol_common *const sc = fld != 0 ? fld->get_sdef() : 0;
  if (fld != 0 && sc->arg_hidden_this != 0) {
    /* calling non-member function as obj.foo(...) */
    fn_emit_value(em, fld);
    em.puts("(");
    bool is_first = !emit_func_upvalue_args(em, fld, symtbl_lexical);
    if (!is_first) {
      em.puts(",");
    }
    fn_emit_value(em, sc->arg_hidden_this);
    if (arg != 0) {
      em.puts(", ");
      fn_emit_value(em, arg);
    }
    em.puts(")");
  } else if (funccall_sort != funccall_e_funccall &&
    conv == conversion_e_boxing && comma_sep_length(arg) <= 3) {
    /* fast boxing */
    em.puts("pxcrt::boxing()");
    if (arg != 0) {
      em.puts(",");
      fn_emit_value(em, arg);
    }
  } else {
    /* function(...) */
    switch (funccall_sort) {
    case funccall_e_funccall:
      emit_function_expr(em, func);
      break;
    case funccall_e_explicit_conversion:
    case funccall_e_default_constructor:
    case funccall_e_struct_constructor:
      emit_term(em, get_texpr());
      break;
    }
    em.puts("(");
    bool is_first = !emit_func_upvalue_args(em, func, symtbl_lexical);
    if (arg != 0) {
      if (!is_first) {
	em.puts(",");
      }
      fn_emit_value(em, arg);
    }
    em.puts(")");
  }
}

void expr_special::emit_value(emit_context& em)
{
  switch (tok) {
  case TOK_BREAK:
    em.puts("break");
    break;
  case TOK_CONTINUE:
    em.puts("continue");
    break;
  case TOK_RETURN:
    em.puts("return");
    if (arg != 0) {
      em.puts(" ");
      fn_emit_value(em, arg);
    }
    break;
  case TOK_THROW:
    em.puts("throw");
    assert(arg);
    em.puts(" ");
    fn_emit_value(em, arg);
    break;
  }
}

static bool can_omit_brace(const expr_if *ei)
{
  /* returns true if ei is the only statement in the block */
  /* FIXME: TEST */
  DBG_IF(fprintf(stderr, "can_omit_brace [%s]\n", ei->dump(0).c_str()));
  if (ei->cond_static == -1) { /* not necessary to check? */
    DBG_IF(fprintf(stderr, "1: false\n"));
    return false;
  }
  if (ei->parent_expr->get_esort() == expr_e_if) {
    expr_if *const pei = ptr_down_cast<expr_if>(ei->parent_expr);
    if (pei->cond_static == 0 && pei->rest == ei) {
      const bool r = can_omit_brace(pei);
      DBG_IF(fprintf(stderr, "2: %d\n", (int)r));
      return r;
    }
    DBG_IF(fprintf(stderr, "3: false\n"));
    return false;
  }
  if (ei->parent_expr->get_esort() == expr_e_stmts) {
    /* ei is the toplevel of an if-elseif-else chain */
    expr_stmts *const pstmt = ptr_down_cast<expr_stmts>(ei->parent_expr);
    expr_i *e = pstmt;
    while (e && e->get_esort() != expr_e_block) {
      e = e->parent_expr;
    }
    assert(e != 0);
    expr_block *const pbl = ptr_down_cast<expr_block>(e);
    /* pbl is the block containing ei */
    expr_stmts *st = pbl->stmts;
    while (st != 0) {
      if (!esort_noemit_funcbody(st->head)) {
	if (st->head->get_esort() != expr_e_if ||
	  ptr_down_cast<expr_if>(st->head) != ei) {
	  /* another statement is found in the block */
	  DBG_IF(fprintf(stderr, "4: false\n"));
	  return false;
	}
      }
      st = st->rest;
    }
    DBG_IF(fprintf(stderr, "5: false\n"));
    return true;
  }
  DBG_IF(fprintf(stderr, "can_omit_brace 7: false\n"));
  return false;
}

void expr_if::emit_value(emit_context& em)
{
  if (cond_static == 0) {
    /* if (0) { ... } */
  } else if (cond_static == 1) {
    /* if (1) { block1 } */
    if (block1 != 0 && can_omit_brace(this)) {
      em.puts("/* staticif */\n");
      block1->emit_value_nobrace(em);
      em.indent('b');
      em.puts("/* staticif end */");
    } else {
      fn_emit_value(em, block1);
    }
    return;
  } else {
    /* if (cond) { block1 } [else] */
    em.puts("if (");
    fn_emit_value_blockcond(em, cond);
    em.puts(") ");
    fn_emit_value(em, block1);
    if (rest != 0) {
      em.puts(" else ");
    } else if (block2 != 0) {
      em.puts(" else ");
    }
  }
  if (rest != 0) {
    fn_emit_value(em, rest);
  } else if (block2 != 0) {
    if (cond_static == 0 && can_omit_brace(this)) {
      /* if (0) { block1 } else { block2 } */
      em.puts("/* staticif-else */\n");
      block2->emit_value_nobrace(em);
      em.indent('b');
      em.puts("/* staticif-else end */");
    } else {
      fn_emit_value(em, block2);
    }
  }
}

void expr_while::emit_value(emit_context& em)
{
  em.puts("while (");
  fn_emit_value_blockcond(em, cond);
  em.puts(") ");
  fn_emit_value(em, block);
}

void expr_for::emit_value(emit_context& em)
{
  em.puts("for (");
  if (e0) {
    fn_emit_value_blockcond(em, e0);
  }
  em.puts("; ");
  if (e1) {
    fn_emit_value_blockcond(em, e1);
  }
  em.puts("; ");
  if (e2) {
    fn_emit_value_blockcond(em, e2);
  }
  em.puts(") ");
  fn_emit_value(em, block);
}

void expr_feach::emit_value(emit_context& em)
{
  em.puts("{\n");
  em.add_indent(1);
  em.indent('f');
  const std::string cetstr = get_term_cname(ce->get_texpr());
  const bool mapped_byref_flag =
    block->argdecls != 0 && block->argdecls->rest != 0 &&
    block->argdecls->rest->byref_flag;
  if (mapped_byref_flag) {
    em.puts("");
  } else {
    em.puts("const ");
  }
  em.puts(cetstr);
  em.puts("& ag$fe = (");
  fn_emit_value(em, ce);
  em.puts(");\n");
  if (type_has_invalidate_guard(ce->get_texpr())) {
    em.indent('f');
    if (mapped_byref_flag) {
      em.puts("const pxcrt::refvar_igrd< ");
    } else {
      em.puts("const pxcrt::refvar_igrd< const ");
    }
    em.puts(cetstr);
    em.puts(" > ag$fg(ag$fe);\n");
  }
  const term& cet = ce->get_texpr();
  if (is_array_family(cet) || is_cm_slice_family(cet)) {
    em.indent('f');
    em.puts("const size_t sz$fe = ag$fe.size();\n");
    em.indent('f');
    em.puts("for (");
    expr_argdecls *const adk = block->argdecls;
    emit_arg_cdecl(em, adk, true); // adk->emit_cdecl(em, true, adk->byref_flag);
    em.puts(" = 0; ");
    adk->emit_symbol(em);
    em.puts(" != sz$fe; ++");
    adk->emit_symbol(em);
    em.puts(") {\n");
    em.add_indent(1);
    em.indent('f');
    expr_argdecls *const adm = adk->rest;
    emit_arg_cdecl(em, adm, true); // adm->emit_cdecl(em, true, adm->byref_flag);
    em.puts(" = ag$fe");
    em.puts("[");
    adk->emit_symbol(em);
    em.puts("];\n");
    em.indent('f');
    fn_emit_value(em, block);
    em.puts("\n");
    em.add_indent(-1);
    em.indent('f');
    em.puts("}\n");
  } else if (is_map_family(cet)) {
    em.indent('f');
    em.puts("for (");
    em.puts(cetstr);
    if (mapped_byref_flag) {
      em.puts("::iterator ");
    } else {
      em.puts("::const_iterator ");
    }
    em.puts("i$fe = ag$fe.begin(); i$fe != ag$fe.end(); ++i$fe) {\n");
    em.add_indent(1);
    em.indent('f');
    expr_argdecls *const adk = block->argdecls;
    emit_arg_cdecl(em, adk, true); // adk->emit_cdecl(em, true, adk->byref_flag);
    em.puts(" = i$fe->first;\n");
    expr_argdecls *const adm = adk->rest;
    em.indent('f');
    emit_arg_cdecl(em, adm, true); // adm->emit_cdecl(em, true, adm->byref_flag);
    em.puts(" = i$fe->second;\n");
    em.indent('f');
    fn_emit_value(em, block);
    em.puts("\n");
    em.add_indent(-1);
    em.indent('f');
    em.puts("}\n");
  } else {
    // FIXME: set, multiset, multimap, list
  }
  em.add_indent(-1);
  em.indent('f');
  em.puts("}");
}

void expr_fldfe::emit_value(emit_context& em)
{
  em.puts("/* foreach */\n");
  fn_emit_value(em, stmts);
  em.indent('f');
  em.puts("/* foreach end */");
}

void expr_foldfe::emit_value(emit_context& em)
{
  em.puts("/* foreach */\n");
  fn_emit_value(em, stmts);
  em.indent('f');
  em.puts("/* foreach end */");
}

std::string expr_argdecls::emit_symbol_str() const
{
  return std::string(sym) + "$"
      + ulong_to_string(symtbl_lexical->block_backref->block_id_ns);
}

void expr_argdecls::emit_symbol(emit_context& em) const
{
  em.puts(emit_symbol_str());
}

#if 0
void expr_argdecls::emit_cdecl(emit_context& em, bool is_argdecl, bool byref)
  const
{
  emit_arg_cdecl(em, this, is_argdecl);
  #if 0
  if (is_argdecl) {
    emit_typestr_call_traits(em, type_of_this_expr, byref_flag);
  } else {
    emit_term(em, type_of_this_expr);
    if (byref) {
      em.puts("&");
    }
  }
  em.puts(" ");
  this->emit_symbol(em);
  #endif
}
#endif

void expr_argdecls::emit_value(emit_context& em)
{
  this->emit_symbol(em);
}

std::string expr_funcdef::emit_symbol_str() const
{
  const std::string s = term_tostr(value_texpr, term_tostr_sort_cname);
  return s;
}

void expr_funcdef::emit_symbol(emit_context& em) const
{
  em.puts(emit_symbol_str());
}

void expr_funcdef::emit_value(emit_context& em)
{
  em.printf("/* function %s */", sym);
}

std::string expr_typedef::emit_symbol_str() const
{
  abort();
  return cname;
}

void expr_typedef::emit_symbol(emit_context& em) const
{
  em.puts(emit_symbol_str());
}

void expr_typedef::emit_value(emit_context& em)
{
  em.printf("/* typedef %s */", sym);
}

std::string expr_macrodef::emit_symbol_str() const
{
  abort();
  return std::string();
}

void expr_macrodef::emit_symbol(emit_context& em) const
{
  em.puts(emit_symbol_str());
}

void expr_macrodef::emit_value(emit_context& em)
{
  em.printf("/* macrodef %s */", sym);
}

std::string expr_struct::emit_symbol_str() const
{
  abort();
  if (symtbl_lexical->get_lexical_parent() == 0) {
    return to_c_ns(ns) + sym + "$s";
  } else {
    return std::string(sym) + "$s"
      + ulong_to_string(symtbl_lexical->block_backref->block_id_ns);
  }
}

void expr_struct::emit_symbol(emit_context& em) const
{
  em.puts(emit_symbol_str());
}

void expr_struct::emit_value(emit_context& em)
{
  em.printf("/* struct %s */", sym);
}

std::string expr_variant::emit_symbol_str() const
{
  abort();
  if (symtbl_lexical->get_lexical_parent() == 0) {
    return to_c_ns(ns) + sym + "$v";
  } else {
    return std::string(sym) + "$v"
      + ulong_to_string(symtbl_lexical->block_backref->block_id_ns);
  }
}

void expr_variant::emit_symbol(emit_context& em) const
{
  em.puts(emit_symbol_str());
}

void expr_variant::emit_value(emit_context& em)
{
  em.printf("/* variant %s */", sym);
}

std::string expr_interface::emit_symbol_str() const
{
  abort();
  if (symtbl_lexical->get_lexical_parent() == 0) {
    return to_c_ns(ns) + sym + "$i";
  } else {
    return std::string(sym) + "$i"
      + ulong_to_string(symtbl_lexical->block_backref->block_id_ns);
  }
}

void expr_interface::emit_symbol(emit_context& em) const
{
  em.puts(emit_symbol_str());
}

void expr_interface::emit_value(emit_context& em)
{
  em.printf("/* interface %s */", sym);
}

void expr_try::emit_value(emit_context& em)
{
  if (tblock != 0) {
    em.puts("try ");
    fn_emit_value(em, tblock);
  }
  em.puts(" catch (const ");
  assert(cblock->argdecls);
  /* don't use call-traits. always catch by const reference. */
  emit_term(em, cblock->argdecls->get_texpr());
  em.puts("& ");
  cblock->argdecls->emit_symbol(em);
  em.puts(") ");
  fn_emit_value(em, cblock);
  if (rest != 0) {
    fn_emit_value(em, rest);
  }
}

void fn_emit_value(emit_context& em, expr_i *e, bool expand_tempvar)
{
  if (e == 0) {
    return;
  }
  if (e->tempvar_id >= 0 && !expand_tempvar) {
    const term te = e->type_conv_to.is_null()
      ? e->get_texpr() : e->type_conv_to;
    const bool need_invguard = type_has_invalidate_guard(te);
      /* wrapped by invalid_guard<> */
    const bool inside_blc = inside_blockcond(e);
      /* wrapped by refvar<> */
    if ((need_invguard || inside_blc) &&
      is_passby_cm_reference(e->tempvar_varinfo.passby)) {
      em.printf("(t$%d.get())", e->tempvar_id);
    } else {
      em.printf("t$%d", e->tempvar_id);
    }
    return;
  }
  switch (e->conv) {
  case conversion_e_none:
    e->emit_value(em);
    break;
  case conversion_e_boxing:
    {
      /* emit ptr<te> instead of type_conv_to, so that fast boxing works. */
      const term& te = e->get_texpr();
      em.puts("(");
      if (is_interface_pointer(e->type_conv_to)) {
	em.puts("pxcrt::rcptr< ");
	emit_term(em, te);
	em.puts(" >");
      } else {
	const std::string cat = get_category(e->type_conv_to);
	if (cat == "tref" || cat == "tcref") {
	  em.puts("pxcrt::rcptr< pxcrt::trcval< ");
	} else {
	  em.puts("pxcrt::rcptr< pxcrt::rcval< ");
	}
	emit_term(em, te);
	em.puts(" > >");
      }
      em.puts("(");
      e->emit_value(em);
      em.puts("))");
    }
    break;
  case conversion_e_cast:
    if (is_smallpod_type(e->type_conv_to)) {
      em.puts("((");
      emit_term(em, e->type_conv_to);
      em.puts(")");
      em.puts("(");
      e->emit_value(em);
      em.puts("))");
    } else {
      emit_term(em, e->type_conv_to);
      em.puts("(");
      e->emit_value(em);
      em.puts(")");
    }
    break;
  }
}

void emit_code(const std::string& dest_filename, expr_block *gl_block,
  generate_main_e gmain)
{
  emit_context em(dest_filename);
  em.set_file_line(gl_block);
  em.puts("/* type definitions */\n");
  emit_inline_c(em, "type", true);
  em.start_ns();
  emit_type_definitions(em);
  em.finish_ns();
  em.puts("/* inline c hdr */\n");
  emit_inline_c(em, "fdecl", true);
  em.puts("/* function prototype decls */\n");
  em.start_ns();
  emit_function_decl(em);
  em.finish_ns();
  em.puts("/* inline c */\n");
  emit_inline_c(em, "fdef", false);
  em.start_ns();
  em.puts("/* global variables */\n");
  emit_global_vars(em, gl_block);
  em.puts("/* function definitions */\n");
  emit_function_def(em);
  /* main */
  em.puts("/* package main */\n");
  em.set_file_line(gl_block);
  const std::string mainfn = arena_get_ns_main_funcname(main_namespace);
  em.set_ns(main_namespace);
  em.set_file_line(gl_block);
  em.printf("void %s()\n", mainfn.c_str());
  fn_emit_value(em, gl_block);
  em.puts("\n");
  em.finish_ns();
  /* extern c no namespace */
  em.set_file_line(gl_block);
  em.puts("/* package main c */\n");
  em.set_file_line(gl_block);
  em.printf("static int i$%s$init = 0;\n", mainfn.c_str());
  em.set_file_line(gl_block);
  em.puts("extern \"C\" {\n");
  em.set_file_line(gl_block);
  em.printf("void %s$c()\n{\n", mainfn.c_str());
  em.set_file_line(gl_block);
  em.add_indent(1);
  em.indent('m');
  em.printf("if (i$%s$init == 0) {\n", mainfn.c_str());
  em.add_indent(1);
  emit_import_init(em, main_namespace);
  em.set_file_line(gl_block);
  em.indent('m');
  em.printf("%s::%s();\n", to_c_ns(main_namespace).c_str(), mainfn.c_str());
  em.set_file_line(gl_block);
  em.indent('m');
  em.printf("i$%s$init = 1;\n", mainfn.c_str());
  em.add_indent(-1);
  em.indent('m');
  em.puts("}\n");
  em.add_indent(-1);
  em.puts("}\n");
  /* TODO: don't emit $cm if not necessary */
  if (gmain != generate_main_none) {
    if (gmain == generate_main_dl) {
      em.printf("int %s$cm()\n{\n", mainfn.c_str());
    } else if (gmain == generate_main_exe) {
      em.puts("int main()\n{\n");
    }
    em.printf(" try { %s$c(); } catch (const std::exception& e) {\n",
      mainfn.c_str());
    em.puts("  std::string mess(e.what());\n");
    em.puts("  if (!mess.empty() && mess[mess.size() - 1] != \'\\n\') {\n");
    em.puts("    mess += \"\\n\";\n");
    em.puts("  }\n");
    em.puts("  write(2, mess.data(), mess.size());\n");
    em.puts("  return 1;\n");
    em.puts(" }\n");
    em.puts(" return 0;\n");
    em.puts("}\n");
  }
  em.puts("}; /* extern \"C\" */\n");
}

};

