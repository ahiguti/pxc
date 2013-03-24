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
#include "sort_dep.hpp"
#include "checktype.hpp"
#include "term.hpp"
#include "emit.hpp"
#include "eval.hpp"
#include "util.hpp"

#define DBG_EMIT_SYM(x)
#define DBG_DECL(x)
#define DBG_FOBJ(x)
#define DBG_FOI(x)
#define DBG_IF(x)
#define DBG_ASTMT(x)
#define DBG_CEFB(x)
#define DBG_EVC(x)
#define DBG_FBC(x)

namespace pxc {

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
  case expr_e_forrange:
  case expr_e_feach:
  case expr_e_fldfe:
  case expr_e_foldfe:
  case expr_e_try:
    return true;
  default:
    return false;
  }
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
  if (is_possibly_pod(t)) {
    /* it's possible that t is a POD type. need to generate an explicit
     * initializer. */
    em.puts(" = ");
    em.puts(term_tostr(t, term_tostr_sort_cname));
    em.puts("()");
    return;
  }
}

static void emit_typestr_call_traits(emit_context& em, const term& te,
  passby_e passby)
{
  switch (passby) {
  case passby_e_mutable_value:
    emit_term(em, te);
    break;
  case passby_e_const_value:
    em.puts("const ");
    emit_term(em, te);
    break;
  case passby_e_mutable_reference:
    emit_term(em, te);
    em.puts("&");
    break;
  case passby_e_const_reference:
    em.puts("const ");
    emit_term(em, te);
    em.puts("&");
    break;
  }
}

static std::string csymbol_encode_ns(const std::string& uniqns)
{
  std::string c = uniqns;
  unsigned int cnt = 0;
  for (size_t i = 0; i < c.size(); ++i) {
    if (c[i] == ':') {
      c[i] = '$';
      cnt += 1;
    }
  }
  return c + "$ns" + ulong_to_string(cnt / 2);
}

static std::string c_namespace_str_prefix(const std::string& uniqns)
{
  std::string r;
  if (uniqns.empty()) {
    return r;
  }
  for (size_t i = 0; i < uniqns.size(); ++i) {
    r.push_back(uniqns[i]);
    if (i > 0 && uniqns[i] == ':' && uniqns[i - 1] == ':') {
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
      ? c_namespace_str_prefix(ev->uniqns) : "";
    return nsp + std::string(ev->sym) + "$"
	  + ulong_to_string(ev->symtbl_lexical->block_backref->block_id_ns)
	  + "$" + csymbol_encode_ns(ev->uniqns);
  } else {
    return std::string(ev->sym) + "$";
  }
}

static void emit_var_cdecl(emit_context& em, const expr_var *ev,
  bool is_argdecl, bool force_byref)
{
  if (is_argdecl) {
    /* used for emitting struct constructors and function upvalue args */
    passby_e passby = ev->varinfo.passby;
    if (force_byref) {
      /* used for emitting function upvalue args */
      switch (passby) {
      case passby_e_const_value:
	passby = passby_e_const_reference;
	break;
      case passby_e_mutable_value:
	passby = passby_e_mutable_reference;
	break;
      case passby_e_const_reference:
      case passby_e_mutable_reference:
	break;
      }
    }
    emit_typestr_call_traits(em, ev->type_of_this_expr, passby);
      /* passby becomes by-ref when it's passed as an upvalue */
  } else {
    emit_term(em, ev->type_of_this_expr);
  }
  em.puts(" ");
  em.puts(csymbol_var(ev, true));
}

static void emit_arg_cdecl(emit_context& em, const expr_argdecls *ad,
  bool is_argdecl, bool force_byref)
{
  if (is_argdecl) {
    emit_typestr_call_traits(em, ad->type_of_this_expr, ad->passby);
  } else {
    /* used for calling the user-defined constructor from the default
     * constructor */
    /* used for foreach iterator */
    emit_term(em, ad->type_of_this_expr);
  }
  em.puts(" ");
  ad->emit_symbol(em);
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
  em.set_ns(ei->uniqns);
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
  em.puts("virtual void incref$z() const = 0;\n");
  em.set_file_line(ei);
  em.indent('b');
  em.puts("virtual void decref$z() const = 0;\n");
  em.set_file_line(ei);
  em.indent('b');
  em.puts("virtual size_t get_count$z() const = 0;\n");
  const bool multithr = (ei->get_attribute() & attribute_multithr) != 0;
  if (multithr) {
    em.set_file_line(ei);
    em.indent('b');
    em.puts("virtual void lock$z() const = 0;\n");
    em.set_file_line(ei);
    em.indent('b');
    em.puts("virtual void unlock$z() const = 0;\n");
    em.set_file_line(ei);
    em.indent('b');
    em.puts("virtual void wait$z() const = 0;\n");
    em.set_file_line(ei);
    em.indent('b');
    em.puts("virtual void notify_one$z() const = 0;\n");
    em.set_file_line(ei);
    em.indent('b');
    em.puts("virtual void notify_all$z() const = 0;\n");
  }
  expr_block *const bl = ei->block;
  const bool is_funcbody = false;
  bl->emit_local_decl(em, is_funcbody);
  bl->emit_memberfunc_decl(em, true);
  em.add_indent(-1);
  em.puts("};\n");
}

static void emit_argdecls(emit_context& em, expr_argdecls *ads, bool is_first)
{
  for (expr_argdecls *a = ads; a; a = a->get_rest()) {
    if (is_first) {
      is_first = false;
    } else {
      em.puts(", ");
    }
    emit_typestr_call_traits(em, a->get_texpr(), a->passby);
    em.puts(" ");
    a->emit_symbol(em);
  }
}

static void emit_struct_constr_initializer(emit_context& em,
  const expr_struct *est, const std::list<expr_var *>& flds,
  bool emit_default_init, bool emit_fast_init)
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
    expr_i *const e = *i;
    e->emit_symbol(em);
    if (emit_default_init) {
      em.puts("()");         /* : v$() */
    } else if (emit_fast_init) {
      expr_op *const p = dynamic_cast<expr_op *>(e->parent_expr);
      if (p != 0 && p->op == '=') {
	em.puts("(");
	fn_emit_value(em, p->arg1);
	em.puts(")");
      } else {
	em.puts("()");
      }
    } else {
      em.puts("(");
      e->emit_symbol(em); /* : v$(v$) */
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
  em.set_ns(est->uniqns);
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
      em.puts(term_tostr_cname(ih->head->get_sdef()->get_evaluated()));
      ih = ih->rest;
    }
  }
  em.puts(" {\n");
  em.add_indent(1);
  if (est->block->inherit != 0) {
    const bool multithr = (est->get_attribute() & attribute_multithr) != 0;
    if (multithr) {
      em.set_file_line(est);
      em.indent('b');
      em.puts("void incref$z() const "
	"{ __sync_fetch_and_add(&count$z, 1); }\n");
      em.set_file_line(est);
      em.indent('b');
      em.puts("void decref$z() const "
	"{ if (__sync_fetch_and_add(&count$z, -1) == 1) delete this; }\n");
      em.set_file_line(est);
      em.indent('b');
      em.puts("void lock$z() const { monitor$z.mtx.lock(); }\n");
      em.set_file_line(est);
      em.indent('b');
      em.puts("void unlock$z() const { monitor$z.mtx.unlock(); }\n");
      em.set_file_line(est);
      em.indent('b');
      em.puts("void wait$z() const { monitor$z.cond.wait(monitor$z.mtx); }\n");
      em.set_file_line(est);
      em.indent('b');
      em.puts("void notify_one$z() const { monitor$z.cond.notify_one(); }\n");
      em.set_file_line(est);
      em.indent('b');
      em.puts("void notify_all$z() const { monitor$z.cond.notify_all(); }\n");
      em.set_file_line(est);
      em.indent('b');
      em.puts("mutable pxcrt::monitor monitor$z;\n");
    } else {
      em.set_file_line(est);
      em.indent('b');
      em.puts("void incref$z() const { ++count$z; }\n");
      em.set_file_line(est);
      em.indent('b');
      em.puts("void decref$z() const { if (--count$z == 0) delete this; }\n");
    }
    em.set_file_line(est);
    em.indent('b');
    em.puts("size_t get_count$z() const { return count$z; }\n");
    em.set_file_line(est);
    em.indent('b');
    em.puts("mutable long count$z;\n");
  }
  expr_block *const bl = est->block;
  const bool is_funcbody = false;
  /* member fields */
  bl->emit_local_decl(em, is_funcbody);
  /* member functions */
  bl->emit_memberfunc_decl(em, false);
  if (est->has_userdefined_constr()) {
    /* user defined constructor */
    em.set_file_line(est);
    em.indent('b');
    em.puts("inline ");
    em.puts(name_c);
    em.puts("(");
    emit_argdecls(em, est->block->get_argdecls(), true);
    em.puts(");\n");
  } else {
    /* plain struct */
    /* get field list */
    typedef std::list<expr_var *> flds_type;
    flds_type flds;
    est->get_fields(flds);
    flds_type::const_iterator i;
    /* default constructor */
    if (is_default_constructible(est->get_texpr())) {
      em.set_file_line(est);
      em.indent('b');
      em.puts("inline ");
      em.puts(name_c);
      em.puts("();\n");
    }
    /* struct constructor */
    if (!flds.empty()) {
      em.set_file_line(est);
      em.indent('b');
      em.puts("inline ");
      em.puts(name_c);
      em.puts("(");
      for (i = flds.begin(); i != flds.end(); ++i) {
	if (i != flds.begin()) {
	  em.puts(", ");
	}
	emit_var_cdecl(em, *i, true, false);
      }
      em.puts(");\n");
    }
  }
  em.add_indent(-1);
  em.puts("};\n");
}

static void emit_struct_constr_one(emit_context& em, expr_struct *est,
  bool emit_default_constr)
{
  if (!is_compiled(est->block)) {
    return;
  }
  const std::string name_c = get_type_cname_wo_ns(est);
  typedef std::list<expr_var *> flds_type;
  flds_type flds;
  est->get_fields(flds);
  flds_type::const_iterator i;
  if (est->has_userdefined_constr()) {
    /* user defined constructor */
    em.set_ns(est->uniqns);
    em.set_file_line(est);
    em.indent('b');
    em.puts(name_c);
    em.puts("::");
    em.puts(name_c);
    em.puts("(");
    emit_argdecls(em, est->block->get_argdecls(), true);
    em.puts(")");
    /* fast init userdef constructor */
    emit_struct_constr_initializer(em, est, flds, false, true);
    em.puts(" {\n");
    em.add_indent(1);
    expr_stmts *st = est->block->stmts;
    for (; st != 0; st = st->rest) {
      expr_i *e = st->head;
      if (!is_vardef_or_vardefset(e) && !is_noexec_expr(e)) {
	break;
      }
    }
    fn_emit_value(em, st);
    em.add_indent(-1);
    em.indent('b');
    em.puts("}\n");
  } else {
    /* no pxc usedefined */
    if (flds.empty() && !emit_default_constr) {
      return;
    }
    flds_type::const_iterator i;
    em.set_ns(est->uniqns);
    em.set_file_line(est);
    em.indent('b');
    em.puts(name_c);
    em.puts("::");
    em.puts(name_c);
    em.puts("(");
    if (emit_default_constr) {
      /* default constructor */
      em.puts(")");
      emit_struct_constr_initializer(em, est, flds, true, false);
    } else {
      /* struct fields constructor */
      for (i = flds.begin(); i != flds.end(); ++i) {
	if (i != flds.begin()) {
	  em.puts(", ");
	}
	emit_var_cdecl(em, *i, true, false);
      }
      em.puts(")");
      emit_struct_constr_initializer(em, est, flds, false, false);
    }
    fn_emit_value(em, est->block);
    em.puts("\n");
  }
}

static void emit_initialize_variant_field(emit_context& em,
  const expr_variant *ev, const expr_var *fld, bool copy_flag, bool set_flag)
{
  if (is_unit_type(fld->get_texpr())) {
    em.puts("/* unit */");
    return;
  }
  if (is_possibly_nonpod(fld->get_texpr())) {
    /* placement new */
    em.puts("new(");
    fld->emit_symbol(em);
    em.puts("$p()) ");
    emit_term(em, fld->get_texpr());
    if (copy_flag) {
      em.puts("(*(x.");
      fld->emit_symbol(em);
      em.puts("$p()))");
    } else if (set_flag) {
      em.puts("(x)");
    } else {
      em.puts("()");
    }
  } else {
    /* known to be a pod */
    em.puts("*(");
    fld->emit_symbol(em);
    em.puts("$p()) = ");
    if (copy_flag) {
      em.puts("*(x.");
      fld->emit_symbol(em);
      em.puts("$p())");
    } else if (set_flag) {
      em.puts("x");
    } else {
      em.puts(term_tostr(fld->get_texpr(), term_tostr_sort_cname));
      em.puts("()");
    }
  }
}

static std::string destructor_cstr(const term& typ)
{
  std::string s = term_tostr_cname(typ);
  s += "::~";
  const std::string shortname = to_short_name(get_term_cname(typ));
  const std::string shortname_wo_tp =
    shortname.substr(0, shortname.find('<'));
  s += shortname_wo_tp;
  return s;
}

static void emit_deinitialize_variant_field(emit_context& em,
  const expr_variant *ev, const expr_var *fld)
{
  if (is_unit_type(fld->get_texpr())) {
    em.puts("/* unit */");
    return;
  }
  if (is_possibly_nonpod(fld->get_texpr())) {
    /* destructor */
    fld->emit_symbol(em);
    em.puts("$p()->");
    em.puts(destructor_cstr(fld->get_texpr()));
    em.puts("()");
  } else {
    /* known to be a pod */
    em.puts("/* pod */");
    return;
  }
}

static void emit_variant_aux_functions(emit_context& em,
  const expr_variant *ev, bool declonly)
{
  typedef std::list<expr_var *> flds_type;
  flds_type flds;
  ev->get_fields(flds);
  flds_type::const_iterator i;
  bool has_non_unit = false;
  bool has_non_smallpod = false;
  for (i = flds.begin(); i != flds.end(); ++i) {
    if (!is_unit_type((*i)->get_texpr())) {
      has_non_unit = true;
    }
    if (is_possibly_nonpod((*i)->get_texpr())) {
      has_non_smallpod = true;
    }
  }
  const std::string name_c = get_type_cname_wo_ns(ev);
  em.set_ns(ev->uniqns);
  /* init */
  em.set_file_line(ev);
  em.indent('b');
  if (!declonly) {
    em.puts("void ");
    em.puts(name_c);
    em.puts("::");
  } else {
    em.puts("inline void ");
  }
  em.puts("init$(const ");
  em.puts(name_c);
  em.puts(" & x)");
  if (declonly) {
    em.puts(";");
  } else {
    em.puts(" {\n");
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
	emit_initialize_variant_field(em, ev, *i, true, false);
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
  }
  /* deinit */
  em.set_file_line(ev);
  em.indent('b');
  if (!declonly) {
    em.puts("void ");
    em.puts(name_c);
    em.puts("::");
  } else {
    em.puts("inline void ");
  }
  em.puts("deinit$()");
  if (declonly) {
    em.puts(";\n");
  } else {
    em.puts(" {\n");
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
  }
  /* rvalue */
  for (i = flds.begin(); i != flds.end(); ++i) {
    em.set_file_line(*i);
    em.indent('b');
    if (declonly) {
      em.puts("inline ");
    }
    emit_term(em, (*i)->get_texpr());
    em.puts(" ");
    if (!declonly) {
      em.puts(name_c);
      em.puts("::");
    }
    (*i)->emit_symbol(em);
    em.puts("$r() const");
    if (declonly) {
      em.puts(";\n");
    } else {
      em.puts(" {\n");
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
	em.puts("return pxcrt::unit_value;\n");
      }
      em.add_indent(-1);
      em.set_file_line(*i);
      em.indent('b');
      em.puts("}\n");
    }
  }
  /* lvalue */
  for (i = flds.begin(); i != flds.end(); ++i) {
    em.set_file_line(*i);
    em.indent('b');
    if (declonly) {
      em.puts("inline ");
    }
    emit_term(em, (*i)->get_texpr());
    em.puts(" ");
    if (!declonly) {
      em.puts(name_c);
      em.puts("::");
    }
    (*i)->emit_symbol(em);
    em.puts("$l(");
    emit_term(em, (*i)->get_texpr());
    em.puts(" x)"); /* by value, in order not to be invalidated. */
    if (declonly) {
      em.puts(";\n");
    } else {
      em.puts(" {\n");
      em.add_indent(1);
      em.set_file_line(*i);
      em.indent('b');
      em.puts("{ deinit$(); $e = ");
      (*i)->emit_symbol(em);
      em.puts("$e; ");
      emit_initialize_variant_field(em, ev, *i, false, true);
      em.puts("; }\n");
      em.set_file_line(*i);
      em.indent('b');
      if (!is_unit_type((*i)->get_texpr())) {
	em.puts("return (*");
	(*i)->emit_symbol(em);
	em.puts("$p());\n");
      } else {
	em.puts("return pxcrt::unit_value;\n");
      }
      em.add_indent(-1);
      em.set_file_line(*i);
      em.indent('b');
      em.puts("}\n");
    }
  }
}

static void emit_variant_def_one(emit_context& em, const expr_variant *ev,
  bool proto_only)
{
  if (!is_compiled(ev->block)) {
    return;
  }
  em.set_ns(ev->uniqns);
  em.set_file_line(ev);
  em.puts("struct ");
  const std::string name_c = get_type_cname_wo_ns(ev);
  em.puts(name_c);
  if (proto_only) {
    em.puts(";\n");
    return;
  }
  em.puts(" {\n");
  em.add_indent(1);
  typedef std::list<expr_var *> flds_type;
  flds_type flds;
  ev->get_fields(flds);
  flds_type::const_iterator i;
  /* enum part */
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
  #if 0
  bool has_non_smallpod = false;
  #endif
  for (i = flds.begin(); i != flds.end(); ++i) {
    if (!is_unit_type((*i)->get_texpr())) {
      has_non_unit = true;
    }
    #if 0
    if (is_possibly_nonpod((*i)->get_texpr())) {
      has_non_smallpod = true;
    }
    #endif
  }
  if (has_non_unit) {
    /* field may_alias typedefs */
    for (i = flds.begin(); i != flds.end(); ++i) {
      if (is_unit_type((*i)->get_texpr())) {
	continue;
      }
      /* we need to add 'may_alias' attribute to let gcc know about the
       * potential aliasing. note that -Wno-attribute should be set in
       * order to avoid 'ignoreing attribute to foo after definition'
       * warnings. */
      em.set_file_line(*i);
      em.indent('b');
      em.puts("typedef ");
      emit_term(em, (*i)->get_texpr());
      em.puts(" __attribute__((may_alias)) ");
      (*i)->emit_symbol(em);
      em.puts("$ut;\n");
    }
    /* union part */
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
      #if 0
      // C++11 unrestricted union
      emit_term(em, (*i)->get_texpr());
      em.puts(" ");
      (*i)->emit_symbol(em);
      em.puts(";\n");
      #endif
      #if 1
      em.puts("char ");
      (*i)->emit_symbol(em);
      em.puts("$u[sizeof(");
      (*i)->emit_symbol(em);
      // emit_term(em, (*i)->get_texpr());
      em.puts("$ut)];\n");
      #endif
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
      // emit_term(em, (*i)->get_texpr());
      (*i)->emit_symbol(em);
      em.puts("$ut const *");
      (*i)->emit_symbol(em);
      em.puts("$p() const { return (");
      // emit_term(em, (*i)->get_texpr());
      (*i)->emit_symbol(em);
      em.puts("$ut const *)");
      em.puts("$u.");
      (*i)->emit_symbol(em);
      em.puts("$u; }\n");
      /* non-const */
      em.set_file_line(*i);
      em.indent('b');
      // emit_term(em, (*i)->get_texpr());
      (*i)->emit_symbol(em);
      em.puts("$ut *");
      (*i)->emit_symbol(em);
      em.puts("$p() { return (");
      // emit_term(em, (*i)->get_texpr());
      (*i)->emit_symbol(em);
      em.puts("$ut *)");
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
    emit_initialize_variant_field(em, ev, *i, false, false);
    em.puts(";\n");
    break; /* first field only */
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
  /* init/deinit and getter functions (declonly) */
  emit_variant_aux_functions(em, ev, true);
  /* */
  em.add_indent(-1);
  em.puts("};\n");
}

static void emit_variant_aux_defs(emit_context& em, expr_variant *ev)
{
  if (!is_compiled(ev->block)) {
    return;
  }
  /* init/deinit and getter functions (definitions) */
  emit_variant_aux_functions(em, ev, false);
}

static void emit_type_definitions(emit_context& em)
{
  sorted_exprs c;
  for (expr_arena_type::iterator i = expr_arena.begin();
    i != expr_arena.end(); ++i) {
    expr_i *const e = *i;
    if (e->get_esort() == expr_e_struct || e->get_esort() == expr_e_variant) {
      sort_dep(c, e);
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
    } else {
      emit_arg_cdecl(em, ptr_down_cast<expr_argdecls>(*i), true, true);
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
  emit_argdecls(em, efd->block->get_argdecls(), is_first);
}

static void emit_function_decl_one(emit_context& em, expr_funcdef *efd,
  bool set_ns, bool memfunc_ext)
{
  if (set_ns) {
    if (efd->cname != 0) {
      std::string cname = efd->cname;
      size_t pos = cname.find(':');
      if (pos != cname.npos) {
	const bool ns_extc = true;
	em.set_ns(cname.substr(0, pos), ns_extc);
      } else {
	em.set_ns(efd->uniqns);
      }
    } else {
      em.set_ns(efd->uniqns);
    }
  }
  {
    /* a simple function, a member function, or a virtual function */
    const std::string name_c = get_type_cname_wo_ns(efd);
    DBG_DECL(fprintf(stderr, "decl_one nam_c=%s\n", name_c.c_str()));
    if (efd->block->tinfo.template_descent && !efd->is_virtual_function()) {
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

static void emit_function_def_one(emit_context& em, expr_funcdef *efd)
{
  if ((efd->ext_decl && !efd->block->tinfo.template_descent) || efd->no_def) {
    return;
  }
  if (efd->is_member_function()) {
    expr_struct *const est = efd->is_member_function();
    em.set_ns(est->uniqns);
  } else {
    em.set_ns(efd->uniqns);
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
	if (efd->no_def && efd->block->tinfo.has_tparams()) {
	  continue;
	}
//      if (!efd->no_def || efd->is_virtual_function()) {
	emit_function_decl_one(em, efd, true, false);
	em.puts(";\n");
//      }
    }
    expr_ns *const ens = dynamic_cast<expr_ns *>(*i);
    if (ens != 0 && ens->import) {
      const std::string fn = arena_get_ns_main_funcname(ens->uniq_nsstr);
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
    if (est != 0 && est->cname == 0) {
      if (!est->has_userdefined_constr()) {
	/* default constr for plain struct */
	emit_struct_constr_one(em, est, true);
      }
      /* udcon or struct field constr */
      emit_struct_constr_one(em, est, false);
    }
    expr_variant *const ev = dynamic_cast<expr_variant *>(*i);
    if (ev != 0) {
      emit_variant_aux_defs(em, ev);
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
      const std::string fn = arena_get_ns_main_funcname(ens->uniq_nsstr);
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
      em.printf("/* public import %s */", uniq_nsstr.c_str());
    } else {
      em.printf("/* private import %s */", uniq_nsstr.c_str());
    }
  } else {
    em.printf("/* namespace %s */", uniq_nsstr.c_str());
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
  em.puts("pxcrt::bt_strlit(");
  em.puts(escape_c_str_literal(value));
  em.puts(")");
}

static bool is_field_w_explicit_obj(const expr_i *e)
{
  const expr_op *const eop = dynamic_cast<const expr_op *>(e->parent_expr); 
  if (eop == 0) {
    return false;
  }
  return (eop->op == '.' || eop->op == TOK_ARROW);
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

static std::string csymbol_tempvar(int tempvar_id)
{
  return std::string("t") + long_to_string(tempvar_id);
}

static void emit_value_symdef_common(emit_context& em, symbol_common& sdef,
  const expr_i *e)
{
  if (sdef.get_evaluated().is_long()) {
    em.puts(term_tostr_cname(sdef.get_evaluated()));
    return;
  }
  if (sdef.get_evaluated().is_string()) {
    em.puts("pxcrt::bt_strlit(");
    em.puts(term_tostr_cname(sdef.get_evaluated()));
    em.puts(")");
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
  expr_i *const edef = sdef.get_evaluated().get_expr();
  // FIXME: when necessary?
  if (is_field_w_explicit_obj(e)) { /* TODO: not smart? */
    edef->emit_symbol(em);
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
    edef->emit_symbol(em);
    em.puts(")");
    return;
  }
  edef->emit_symbol(em);
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

void expr_var::emit_value(emit_context& em)
{
  this->emit_symbol(em);
}

std::string expr_enumval::emit_symbol_str() const
{
  abort();
  return std::string(cname);
}

void expr_enumval::emit_symbol(emit_context& em) const
{
  if (cname == 0) {
    assert(value != 0);
    fn_emit_value(em, value);
  } else {
    em.puts(std::string(cname));
  }
}

void expr_enumval::emit_value(emit_context& em)
{
  this->emit_symbol(em);
}

static bool is_split_point(expr_i *e)
{
  if (e->tempvar_id >= 0 && e->tempvar_varinfo.scope_block) {
    return true;
  } else if (is_vardef_or_vardefset(e)) {
    return true;
  }
  return false;
}

static void split_expr_rec(expr_i *e, std::vector<expr_i *>& es)
{
  const int n = e->get_num_children();
  for (int i = 0; i < n; ++i) {
    expr_i *const c = e->get_child(i);
    if (c == 0) {
      continue;
    }
    split_expr_rec(c, es);
  }
  if (is_split_point(e)) {
    es.push_back(e);
  }
}

static void split_expr(expr_i *e, std::vector<expr_i *>& es)
{
  split_expr_rec(e, es);
  if (es.empty() || es.back() != e) {
    es.push_back(e);
  }
}

static void emit_split_expr(emit_context& em, expr_i *e, bool noemit_last)
{
  /* line */
  typedef std::vector<expr_i *> es_type;
  es_type es;
  split_expr(e, es);
  for (es_type::iterator i = es.begin(); i != es.end(); ++i) {
    expr_i *ie = *i;
    if (noemit_last && ie == e) {
      break;
    }
    em.set_file_line(e);
    em.indent('x');
    fn_emit_value(em, ie, true);
    em.puts(";\n");
  }
}

void expr_stmts::emit_value(emit_context& em)
{
  /* line */
  if (head == 0) {
    assert(rest == 0);
    return;
  }
  if (is_noexec_expr(head)) {
    /* nothing to emit */
  } else if (!is_block_stmt(head)) {
    emit_split_expr(em, head, false); /* line */
  } else {
    /* block statement */
    em.indent('S');
    fn_emit_value(em, head); /* noline  */
    em.puts("\n");
  }
  if (rest != 0) {
    fn_emit_value(em, rest);
  }
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
    const bool is_main_ns = (e->uniqns == main_namespace);
    em.set_ns(e->uniqns);
    em.set_file_line(e);
    em.indent('b');
    if (!is_main_ns) {
      em.puts("extern ");
    }
    emit_var_cdecl(em, e, false, false);
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
    em.indent('S');
    emit_var_cdecl(em, e, false, false);
    emit_explicit_init_if(em, e->get_texpr());
    em.puts(";\n");
  }
}

void expr_block::emit_value(emit_context& em)
{
  /* noline */
  em.puts("{\n");
  em.add_indent(1);
  emit_value_nobrace(em); /* line */
  em.add_indent(-1);
  em.indent('B');
  em.puts("}");
}

void expr_block::emit_value_nobrace(emit_context& em)
{
  /* line */
  this->emit_local_decl(em, true); /* line */
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
    emit_var_cdecl(em, e, false, false);
    em.puts("; // localdecl\n");
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
  em.indent('D');
  if (pure_virtual) {
    em.puts("virtual ");
  }
  emit_function_decl_one(em, efd, false, false);
  if (pure_virtual) {
    em.puts(" = 0;\n");
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

static bool need_to_emit_expr_returning_value(expr_i *e)
{
  /* need to emit function returning a value rather than a reference, in order
   * not to be invalidated. for example, vec[idx] returns an reference and it
   * can be invalidated when another expression is evaluated before it is 
   * used. in this case, we need to emit vec.value_at(idx) which returns a
   * value. */
  return (e->tempvar_id >= 0 && is_passby_cm_value(e->tempvar_varinfo.passby));
}

void emit_array_elem_or_range_expr(emit_context& em, expr_op *eop)
{
  const term& ct = eop->arg0->get_texpr();
  if (eop->arg1 != 0 && eop->arg1->get_esort() == expr_e_op &&
    ptr_down_cast<expr_op>(eop->arg1)->op == TOK_SLICE) {
    expr_op *const rangeop = ptr_down_cast<expr_op>(eop->arg1);
    std::string tc = get_term_cname(eop->get_texpr());
    em.puts(tc);
    em.puts("(");
    fn_emit_value(em, eop->arg0);
    em.puts(", ");
    fn_emit_value(em, rangeop->arg0);
    em.puts(", ");
    fn_emit_value(em, rangeop->arg1);
    em.puts(")");
  } else if (need_to_emit_expr_returning_value(eop)) {
    if (is_array_family(ct) || is_cm_slice_family(ct)) {
      if (eop->symtbl_lexical->pragma.disable_bounds_checking) {
	em.puts("pxcrt::get_elem_value_nocheck(");
      } else {
	em.puts("pxcrt::get_elem_value(");
      }
      fn_emit_value(em, eop->arg0);
      em.puts(",");
      fn_emit_value(em, eop->arg1);
      em.puts(")");
    } else { /* map and map_range */
      em.puts("(");
      fn_emit_value(em, eop->arg0);
      em.puts(".value_at(");
      fn_emit_value(em, eop->arg1);
      em.puts("))");
    }
  } else {
    if ((is_array_family(ct) || is_cm_slice_family(ct)) &&
      eop->symtbl_lexical->pragma.disable_bounds_checking) {
      em.puts("(");
      fn_emit_value(em, eop->arg0);
      em.puts(".rawarr()[");
      fn_emit_value(em, eop->arg1);
      em.puts("])");
    } else {
      fn_emit_value(em, eop->arg0);
      em.puts("[");
      fn_emit_value(em, eop->arg1);
      em.puts("]");
    }
  }
}

bool emit_op_memcmp(emit_context& em, expr_op *eop)
{
  const term& te = eop->arg0->get_texpr();
  if (is_array_family(te) || is_cm_slice_family(te)) {
    const char *fn = "";
    switch (eop->op) {
    case TOK_EQ: fn = "eq_memcmp"; break;
    case TOK_NE: fn = "ne_memcmp"; break;
    case '>':    fn = "gt_memcmp"; break;
    case '<':    fn = "lt_memcmp"; break;
    case TOK_GE: fn = "ge_memcmp"; break;
    case TOK_LE: fn = "le_memcmp"; break;
    default:
      abort();
    }
    em.puts(fn);
    em.puts("(");
    fn_emit_value(em, eop->arg0);
    em.puts(",");
    fn_emit_value(em, eop->arg1);
    em.puts(")");
    return true;
  }
  return false;
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
    emit_array_elem_or_range_expr(em, this);
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
    if (is_noninterface_pointer(arg0->get_texpr())) {
      /* non-interface pointer dereference */
      if (need_to_emit_expr_returning_value(this)) {
	em.puts("(pxcrt::deref_value$z(");
	fn_emit_value(em, arg0);
	em.puts("))");
      } else {
	em.puts("((");
	fn_emit_value(em, arg0);
	em.puts(")->value$z)");
      }
    } else if (is_cm_pointer_family(arg0->get_texpr())) {
      /* interface pointer dereference */
      if (need_to_emit_expr_returning_value(this)) {
	em.puts("(pxcrt::deref(");
	fn_emit_value(em, arg0);
	em.puts("))");
      } else {
	em.puts("(*(");
	fn_emit_value(em, arg0);
	em.puts("))");
      }
    } else {
      /* range dereference */
      assert(is_cm_range_family(arg0->get_texpr()));
      /* safe to return ref because range object is always rooted. */
      em.puts("(*(");
      fn_emit_value(em, arg0);
      em.puts("))");
    }
    return;
  case '.':
  case TOK_ARROW:
    if (is_variant(arg0->get_texpr())) {
      em.puts("(");
      fn_emit_value(em, arg0);
      em.puts(".");
      fn_emit_value(em, arg1);
      /* note: we don't reach here if expr is part of foo.fld = ... */
      if (need_to_emit_expr_returning_value(this)) {
	em.puts("$r())"); /* $r() always returns a value rather than a ref */
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
  case '=':
    {
      expr_i *a = arg0;
      expr_op *aop = dynamic_cast<expr_op *>(a);
      if (aop != 0 && (aop->op == '.' || aop->op == TOK_ARROW)) {
	if (is_variant(aop->arg0->get_texpr())) {
	  /* setting variant field */
	  em.puts("(");
	  fn_emit_value(em, aop->arg0); /* variant object */
	  em.puts(".");
	  fn_emit_value(em, aop->arg1); /* field name */
	  em.puts("$l(");
	  fn_emit_value(em, arg1);
	  em.puts("))");
	  return;
	}
      }
    }
    break;
  case TOK_EQ:
  case TOK_NE:
  case '<':
  case '>':
  case TOK_GE:
  case TOK_LE:
    if (emit_op_memcmp(em, this)) {
      return;
    }
    break;
  default:
    break;
  }
  fn_emit_value(em, arg0);
  em.puts(" ");
  em.puts(tok_string(this, op));
  em.puts(" ");
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
      em.puts(" , ");
    }
    fn_emit_value(em, sc->arg_hidden_this);
    if (arg != 0) {
      em.puts(" , ");
      fn_emit_value(em, arg);
    }
    em.puts(")");
  } else {
    /* function(...) */
    /* function or constructor */
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
	em.puts(" , ");
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
      if (!is_noexec_expr(st->head)) {
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
  bool has_own_block = false;
  if (cond_static == 0) {
    /* if (0) { ... } [else] */
  } else if (cond_static == 1) {
    /* if (1) { block1 } */
    if (block1 != 0 && can_omit_brace(this)) {
      em.puts("/* staticif */\n");
      block1->emit_value_nobrace(em);
      em.indent('I');
      em.puts("/* staticif end */");
    } else {
      fn_emit_value(em, block1);
    }
    return;
  } else {
    /* if (cond) { block1 } [else] */
    if (block1->argdecls != 0) {
      expr_op *const eop = ptr_down_cast<expr_op>(cond);
      assert(eop->op == '[');
      expr_i *const econ = eop->arg0;
      const term& tcon = econ->get_texpr();
      assert(type_allow_feach(tcon));
      const std::string cetstr = get_term_cname(tcon);
      expr_argdecls *const mapped = block1->get_argdecls();
      assert(mapped != 0);
      has_own_block = true;
      em.puts("{\n");
      em.add_indent(1);
      em.indent('I');
      const bool mapped_mutable_flag =
	(mapped->passby == passby_e_mutable_reference);
      if (mapped_mutable_flag) {
	em.puts("");
      } else {
	em.puts("const ");
      }
      em.puts(cetstr);
      em.puts("& ag$fe = (");
      fn_emit_value(em, econ);
      em.puts(");\n");
      if (type_has_invalidate_guard(tcon) &&
	is_passby_cm_reference(mapped->passby)) {
	em.indent('I');
	if (mapped_mutable_flag) {
	  em.puts("const pxcrt::guard_ref< ");
	} else {
	  em.puts("const pxcrt::guard_ref< const ");
	}
	em.puts(cetstr);
	em.puts(" > ag$fg(ag$fe);\n");
      }
      em.indent('I');
      em.puts(cetstr);
      if (mapped_mutable_flag) {
	em.puts("::iterator");
      } else {
	em.puts("::const_iterator");
      }
      em.puts(" i$it = ag$fe.find(");
      fn_emit_value(em, eop->arg1);
      em.puts(");\n");
      em.indent('I');
      em.puts("if (i$it != ag$fe.end()) {\n");
      em.add_indent(1);
      em.indent('I');
      emit_arg_cdecl(em, mapped, true, false);
      em.puts(" = i$it->second;\n");
      em.indent('I');
      fn_emit_value(em, block1);
      em.puts("\n");
      em.add_indent(-1);
      em.indent('I');
      em.puts("}");
    } else {
      /* note: blockscope var is not allowed in cond */
      em.puts("if (");
      fn_emit_value(em, cond);
      em.puts(") ");
      fn_emit_value(em, block1);
    }
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
      em.indent('I');
      em.puts("/* staticif-else end */");
    } else {
      fn_emit_value(em, block2);
    }
  }
  if (has_own_block) {
    em.puts("\n");
    em.add_indent(-1);
    em.indent('I');
    em.puts("}");
  }
}

void expr_while::emit_value(emit_context& em)
{
  emit_split_expr(em, cond, true);
  em.puts("while (");
  fn_emit_value(em, cond);
  em.puts(") ");
  fn_emit_value(em, block);
}

void expr_for::emit_value(emit_context& em)
{
  if (e0) {
    emit_split_expr(em, e0, true);
  }
  em.puts("for (");
  if (e0) {
    fn_emit_value(em, e0);
  }
  em.puts("; ");
  if (e1) {
    fn_emit_value(em, e1);
  }
  em.puts("; ");
  if (e2) {
    fn_emit_value(em, e2);
  }
  em.puts(") ");
  fn_emit_value(em, block);
}

void expr_forrange::emit_value(emit_context& em)
{
  expr_argdecls *arg = block->get_argdecls();
  em.puts("for (");
  emit_arg_cdecl(em, arg, false, false); /* always mutable */
  em.puts(" = ");
  fn_emit_value(em, r0);
  em.puts("; ");
  arg->emit_symbol(em);
  em.puts(" < ");
  fn_emit_value(em, r1);
  em.puts("; ++");
  arg->emit_symbol(em);
  em.puts(") ");
  fn_emit_value(em, block);
}

// FIXME: remove
static bool arg_passby_mutable(expr_argdecls *ad)
{
  const passby_e passby = ad->passby;
  switch (passby) {
  case passby_e_const_value:
  case passby_e_const_reference:
    return false;
  case passby_e_mutable_value:
  case passby_e_mutable_reference:
    return true;
  }
  abort();
  return false;
}

void expr_feach::emit_value(emit_context& em)
{
  em.puts("{\n");
  em.add_indent(1);
  em.indent('f');
  assert(block->get_argdecls() != 0);
  assert(block->get_argdecls()->rest != 0);
  expr_argdecls *const mapped = block->get_argdecls()->get_rest();
  const std::string cetstr = get_term_cname(ce->get_texpr());
  emit_split_expr(em, ce, true);
  const bool mapped_mutable_flag = arg_passby_mutable(mapped);
  if (mapped_mutable_flag) {
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
    if (mapped_mutable_flag) {
      em.puts("const pxcrt::guard_ref< ");
    } else {
      em.puts("const pxcrt::guard_ref< const ");
    }
    em.puts(cetstr);
    em.puts(" > ag$fg(ag$fe);\n");
  }
  const term& cet = ce->get_texpr();
  if (is_array_family(cet) || is_cm_slice_family(cet)) {
    em.indent('f');
    em.puts("const size_t sz$fe = ag$fe.size();\n");
    em.indent('f');
    if (!mapped_mutable_flag) {
      em.puts("const ");
    }
    expr_argdecls *const adk = block->get_argdecls();
    expr_argdecls *const adm = adk->get_rest();
    em.puts(get_term_cname(adm->get_texpr()));
    em.puts(" *const ar$fe = ag$fe.rawarr();\n");
    em.indent('f');
    em.puts("for (");
    emit_arg_cdecl(em, adk, false, false); /* always mutable */
    em.puts(" = 0; ");
    adk->emit_symbol(em);
    em.puts(" != sz$fe; ++");
    adk->emit_symbol(em);
    em.puts(") {\n");
    em.add_indent(1);
    em.indent('f');
    emit_arg_cdecl(em, adm, true, false);
    em.puts(" = ar$fe"); /* raw ponter. no need to perform range check. */
    em.puts("[");
    adk->emit_symbol(em);
    em.puts("];\n");
    em.indent('f');
    fn_emit_value(em, block);
    em.puts("\n");
    em.add_indent(-1);
    em.indent('f');
    em.puts("}\n");
  } else if (is_map_family(cet) || is_cm_maprange_family(cet)) {
    em.indent('f');
    em.puts("for (");
    em.puts(cetstr);
    if (mapped_mutable_flag) {
      em.puts("::iterator ");
    } else {
      em.puts("::const_iterator ");
    }
    em.puts("i$fe = ag$fe.begin(); i$fe != ag$fe.end(); ++i$fe) {\n");
    em.add_indent(1);
    em.indent('f');
    expr_argdecls *const adk = block->get_argdecls();
    emit_arg_cdecl(em, adk, true, false);
    em.puts(" = i$fe->first;\n");
    expr_argdecls *const adm = adk->get_rest();
    em.indent('f');
    emit_arg_cdecl(em, adm, true, false);
    em.puts(" = i$fe->second;\n");
    em.indent('f');
    fn_emit_value(em, block);
    em.puts("\n");
    em.add_indent(-1);
    em.indent('f');
    em.puts("}\n");
  } else {
    abort();
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
  abort();
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
  abort();
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
    return to_c_ns(uniqns) + sym + "$s";
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
    return to_c_ns(uniqns) + sym + "$v";
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
    return to_c_ns(uniqns) + sym + "$i";
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

static void emit_comma_sep_list_with_paren(emit_context& em, expr_i *e)
{
  assert(e != 0);
  expr_op *const eop = dynamic_cast<expr_op *>(e);
  if (eop == 0 || eop->op != ',') {
    em.puts("(");
    fn_emit_value(em, e);
    em.puts(")");
    return;
  }
  /* eop is comma */
  emit_comma_sep_list_with_paren(em, eop->arg0);
  em.puts(", ");
  em.puts("(");
  fn_emit_value(em, eop->arg1);
  em.puts(")");
}

static void emit_vardef_constructor_fast_boxing(emit_context& em,
  expr_i *e, const term& typ, const std::string& cs0,
  const std::string& var_csymbol, expr_funccall *fc,
  expr_funccall *fast_boxing_cfunc)
{
  /* NOTE: this function depends on the internals of rcval/trcval/tircval. */
  /* var x = ptr(type(args, ....)) */
  const term& otyp = (*typ.get_args())[0];
  const std::string otyp_cstr = term_tostr_cname(otyp); /* foo */
  DBG_FBC(fprintf(stderr, "type=%s atyp=%s\n", term_tostr_human(typ).c_str(),
    term_tostr_human(otyp).c_str()));
  if (!is_interface_or_impl(otyp)) {
    const std::string varp0 = var_csymbol + "p0";
    const std::string varp1 = var_csymbol + "p1";
    const typefamily_e cat = get_family(typ);
    std::string rcval_cstr;
    std::string count_type_cstr;
    if (cat == typefamily_e_tptr || cat == typefamily_e_tcptr) {
      rcval_cstr = "pxcrt::trcval";
      count_type_cstr = "pxcrt::mtcount";
    } else if (cat == typefamily_e_tiptr) {
      rcval_cstr = "pxcrt::tircval";
      count_type_cstr = "pxcrt::mtcount";
    } else { /* ptr cptr iptr */
      rcval_cstr = "pxcrt::rcval";
      count_type_cstr = "pxcrt::stcount";
    }
    const std::string otypcnt_cstr = rcval_cstr + "< " + otyp_cstr + " >";
      /* rcval<foo> */
    em.puts("pxcrt::uninit_mem< " + otypcnt_cstr + " > *const " + varp0
      + " = new pxcrt::uninit_mem< " + otypcnt_cstr + " >;\n");
    em.indent('x');
    em.puts(otypcnt_cstr + "*const " + varp1 + " = reinterpret_cast< "
      + otypcnt_cstr + " * >(" + varp0 + ");\n");
    em.indent('x');
    em.puts("try {\n");
    em.indent('x');
    em.puts("new (&" + varp1 + "->value$z) " + otyp_cstr + "(");
    fn_emit_value(em, fast_boxing_cfunc->arg);
    em.puts(");\n");
    em.indent('x');
    em.puts("new (&" + varp1 + "->count$z) " + count_type_cstr
      + "(); /* nothrow */\n");
    if (is_multithreaded_pointer_family(typ)) {
      em.indent('x');
      em.puts("try {\n");
      em.indent('x');
      em.puts("new (&" + varp1 + "->monitor$z) pxcrt::monitor();\n");
      em.indent('x');
      em.puts("} catch (...) {\n");
      em.indent('x');
      em.puts(varp1 + "->value$z." + destructor_cstr(otyp) + "();\n");
      em.indent('x');
      em.puts("throw;\n");
      em.indent('x');
      em.puts("}\n");
    }
    em.indent('x');
    em.puts("} catch (...) {\n");
    em.indent('x');
    em.puts("delete " + varp0 + ";\n");
    em.indent('x');
    em.puts("throw;\n");
    em.indent('x');
    em.puts("}\n");
    em.indent('x');
    em.puts(cs0 + " " + var_csymbol);
    em.puts("((" + varp1 + "))");
  } else {
    em.puts(cs0 + " " + var_csymbol);
    em.puts("(new " + otyp_cstr + "(");
    fn_emit_value(em, fast_boxing_cfunc->arg);
    em.puts("))");
  }
}

static expr_funccall *can_emit_fast_boxing(expr_funccall *func)
{
  if (func->funccall_sort != funccall_e_struct_constructor) {
    DBG_CEFB(fprintf(stderr, "cefb %s 1\n", func->dump(0).c_str()));
    return 0;
  }
  if (!is_cm_pointer_family(func->get_texpr())) {
    DBG_CEFB(fprintf(stderr, "cefb %s 2 %s\n", func->dump(0).c_str(),
      term_tostr_human(func->get_texpr()).c_str()));
    return 0;
  }
  if (comma_sep_length(func->arg) != 1) {
    DBG_CEFB(fprintf(stderr, "cefb %s 3\n", func->dump(0).c_str()));
    return 0;
  }
  expr_funccall *cfunc = dynamic_cast<expr_funccall *>(func->arg);
  if (cfunc == 0 || cfunc->conv != conversion_e_none) {
    DBG_CEFB(fprintf(stderr, "cefb %s 4\n", func->dump(0).c_str()));
    return 0;
  }
  if (cfunc->funccall_sort != funccall_e_struct_constructor) {
    DBG_CEFB(fprintf(stderr, "cefb %s 5\n", func->dump(0).c_str()));
    return 0;
  }
  return cfunc;
}

static void emit_vardef_constructor(emit_context& em, expr_i *e,
  const term& typ, const std::string& cs0, const std::string& var_csymbol)
{
  /* var x = type(args, ....) */
  DBG_EVC(fprintf(stderr, "evc %s\n", e->dump(0).c_str()));
  expr_op *const eop = ptr_down_cast<expr_op>(e);
  expr_funccall *const fc = ptr_down_cast<expr_funccall>(eop->arg1);
  expr_funccall *const fast_boxing_cfunc = can_emit_fast_boxing(fc);
  if (fast_boxing_cfunc) {
    return emit_vardef_constructor_fast_boxing(em, e, typ, cs0, var_csymbol,
      fc, fast_boxing_cfunc);
  }
  em.puts(cs0 + " " + var_csymbol);
  if (fc->arg == 0) {
    /* foo x */
    emit_explicit_init_if(em, typ);
  } else {
    /* foo x((a0), (a1), ...) */
    em.puts("(");
    /* to avoid ambiguity with function declaration, we need to append
     * parens for each argument. */
    emit_comma_sep_list_with_paren(em, fc->arg);
    em.puts(")");
  }
}

enum emit_var_e {
  emit_var_defset,
  emit_var_set,
  emit_var_get,
  emit_var_tempobj,
};

static void emit_var_or_tempvar(emit_context& em, expr_i *e, const term& tbase,
  const term& tconvto, const variable_info& vi, const std::string& var_csymbol,
  expr_i *rhs, emit_var_e emv, bool is_unnamed)
{
  const std::string s0 = term_tostr_cname(tconvto.is_null() ? tbase : tconvto);
  const std::string cs0 =
    std::string(is_passby_const(vi.passby) ? "const " : "") + s0;
  if (vi.guard_elements) {
    const std::string base_s0 = term_tostr_cname(tbase);
    const std::string base_cs0 =
      std::string(is_cm_range_family(tconvto)
	? (is_const_range_family(tconvto) ? "const " : "")
	: (is_passby_const(vi.passby) ? "const " : ""))
      + base_s0;
    const std::string wr = is_passby_cm_reference(vi.passby)
      ? "pxcrt::guard_ref" : "pxcrt::guard_val";
    const std::string tstr = wr + "< " + base_cs0 + " > ";
    if (emv == emit_var_get || emv == emit_var_tempobj) {
      em.puts("(");
      if (emv == emit_var_tempobj) {
	em.puts(tstr);
	em.puts("(");
	fn_emit_value(em, rhs, false, is_unnamed); /* tempobj */
	em.puts(")");
      } else {
	em.puts(var_csymbol); /* get */
      }
      if (is_const_range_family(tconvto)) {
	em.puts(".get_crange())");
      } else if (is_cm_range_family(tconvto)) {
	em.puts(".get_range())");
      } else {
	em.puts(".get())");
      }
    } else {
      assert(emv == emit_var_defset);
      em.puts(tstr + var_csymbol);
      em.puts("((");
      fn_emit_value(em, rhs, false, is_unnamed);
      em.puts("))");
    }
  } else if (!is_passby_cm_reference(vi.passby)) {
    /* no guard, pass by value */
    if (emv == emit_var_tempobj) {
      assert(rhs != 0);
      {
	em.puts(s0 + "(");
	fn_emit_value(em, rhs, false, is_unnamed);
	em.puts(")");
      }
    } else if (emv == emit_var_get) {
      assert(!var_csymbol.empty());
      em.puts(var_csymbol);
    } else { /* set or defset */
      if (emv == emit_var_set) {
	/* set */
	if (rhs != 0) {
	  em.puts(var_csymbol + " = ");
	  fn_emit_value(em, rhs, false, is_unnamed);
	} else {
	  em.puts("/* ");
	  em.puts(var_csymbol);
	  em.puts(" */");
	}
      } else if (rhs == 0) {
	/* defset wo rhs */
	em.puts(cs0 + " " + var_csymbol);
	emit_explicit_init_if(em, tconvto.is_null() ? tbase : tconvto);
      } else {
	/* defset */
#if 0
fprintf(stderr, "defset %s\n", e->dump(0).c_str()); // FIXME
#endif
	if (!is_unnamed && is_vardef_constructor(e)) {
	  /* foo x((a0), (a1), ...) */
	  emit_vardef_constructor(em, e, tbase, cs0, var_csymbol);
	} else {
	  em.puts(cs0 + " " + var_csymbol + " = ");
	  fn_emit_value(em, rhs, false, is_unnamed);
	}
      }
    }
  } else {
    /* no guard, pass by reference */
    const std::string tstr = cs0 + "&";
    if (emv == emit_var_get) {
      em.puts(var_csymbol);
    } else if (emv == emit_var_defset) {
      em.puts(tstr + " " + var_csymbol + " = ");
      fn_emit_value(em, rhs, false, is_unnamed);
    } else {
      abort();
    }
  }
}

void emit_value_internal(emit_context& em, expr_i *e)
{
  e->emit_value(em);
}

void fn_emit_value(emit_context& em, expr_i *e, bool expand_composite,
  bool var_rhs)
  /* if var_rhs is true, e is a tempvar and fn_emit_value emits the rhs
   * of it instead of the variable name. if expand_composite is true
   * and e is a split point, fn_emit_value emits var (or tempvar)
   * defnintion instead of the variable name. */
{
  if (e == 0) {
    return;
  }
  const bool split_flag = is_split_point(e);
  if (!var_rhs && (split_flag || e->tempvar_id >= 0)) {
    expr_var *ev = 0;
    const variable_info *vi = 0;
    std::string var_csymbol;
    expr_i *rhs = 0;
    emit_var_e emv = emit_var_get;
    if (e->tempvar_id >= 0 && e->tempvar_varinfo.scope_block) {
      ev = 0;
      rhs = e;
      vi = &e->tempvar_varinfo;
      var_csymbol = csymbol_tempvar(e->tempvar_id);
      emv = expand_composite ? emit_var_defset : emit_var_get;
    } else if (e->tempvar_id >= 0) {
      ev = 0;
      rhs = e;
      vi = &e->tempvar_varinfo;
      var_csymbol = "";
      emv = emit_var_tempobj;
    } else if (split_flag && e->get_esort() == expr_e_op) {
      /* vardefset */
      ev = ptr_down_cast<expr_var>(ptr_down_cast<expr_op>(e)->arg0);
      rhs = ptr_down_cast<expr_op>(e)->arg1;
      vi = &ev->varinfo;
      var_csymbol = csymbol_var(ev, true);
      emv = expand_composite ? emit_var_defset: emit_var_get;
    } else if (split_flag && e->get_esort() == expr_e_var) {
      /* vardef */
      ev = ptr_down_cast<expr_var>(e);
      rhs = 0;
      vi = &ev->varinfo;
      var_csymbol = csymbol_var(ev, true);
      emv = expand_composite ? emit_var_defset: emit_var_get;
    } else {
      abort();
    }
    if (emv == emit_var_defset && ev != 0 &&
      (cur_block_is_struct(ev) || cur_block_is_global(ev))) {
      /* this is a global variable or a struct field, 
       * emit set instead of defset */
      emv = emit_var_set;
    }
    emit_var_or_tempvar(em, e, e->get_texpr(), e->type_conv_to, *vi,
      var_csymbol, rhs, emv, ev == 0);
    return;
  }
  switch (e->conv) {
  case conversion_e_none:
  case conversion_e_subtype_ptr:
  case conversion_e_subtype_obj:
    emit_value_internal(em, e);
    break;
  case conversion_e_container_range:
    emit_value_internal(em, e);
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
	const typefamily_e cat = get_family(e->type_conv_to);
	if (cat == typefamily_e_tptr || cat == typefamily_e_tcptr) {
	  em.puts("pxcrt::rcptr< pxcrt::trcval<");
	} else if (cat == typefamily_e_tiptr) {
	  em.puts("pxcrt::rcptr< pxcrt::tircval<");
	} else {
	  em.puts("pxcrt::rcptr< pxcrt::rcval<");
	}
	emit_term(em, te);
	em.puts("> >");
      }
      em.puts("(");
      emit_value_internal(em, e);
      em.puts("))");
    }
    break;
  case conversion_e_cast:
    {
      emit_term(em, e->type_conv_to);
      em.puts("(");
      emit_value_internal(em, e);
      em.puts(")");
    }
    break;
  }
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
  em.finish_ns();
  /* main */
  em.puts("/* package main */\n");
  em.start_ns();
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
    em.printf(" return pxcrt::main_nothrow(& %s$c);\n", mainfn.c_str());
    em.puts("}\n");
  }
  em.puts("}; /* extern \"C\" */\n");
}

};

