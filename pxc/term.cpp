/*
 *  This file is part of PXC, the p extension compiler.
 *  Copyright (C) 2006 A.Higuchi. All rights reserved.
 *  See COPYRIGHT.txt for details.
 *
 *  This file is part of ASE, the abstract script engines.
 *  Copyright (C) 2006 A.Higuchi. All rights reserved.
 *  See COPYRIGHT.txt for details.
 */


#include "expr_misc.hpp"
#include "util.hpp"

namespace pxc {

term::term(long long x)
  : ptr(new term_long(x))
{
}

term::term(const std::string& x)
  : ptr(new term_string(x))
{
}

term::term(const term_list_range& x)
  : ptr(new term_metalist(x))
{
}

term::term(const std::vector<expr_tparams *>& upvalues, expr_tparams *tpms,
  const term& t)
  : ptr(new term_lambda(upvalues, tpms, t))
{
}

term::term(expr_tparams *tpm, const term& tpmv, const term& tpmv_lexctx,
  bool is_up, const term& next)
  : ptr(new term_bind(tpm, tpmv, tpmv_lexctx, is_up, next))
{
}

term::term(expr_i *e)
  : ptr(new term_expr(e))
{
}

term::term(expr_i *e, const term_list_range& a)
  : ptr(new term_expr(e, a))
{
}

expr_i *term::get_expr() const
{
  term_expr *const te = get_term_expr();
  return te != 0 ? te->expr : 0;
}

const term_list *term::get_args() const
{
  term_expr *const te = get_term_expr();
  return te != 0 ? &te->args : 0;
}

const term_list *term::get_metalist() const
{
  term_metalist *const tm = get_term_metalist();
  return tm != 0 ? &tm->v : 0;
}

const term_list *term::get_args_or_metalist() const
{
  const term_list *r = get_args();
  if (r == 0) {
    r = get_metalist();
  }
  return r;
}

expr_tparams *term::get_lambda_tparams() const
{
  term_lambda *const tl = get_term_lambda();
  return tl != 0 ? tl->tparams : 0;
}

const term *term::get_lambda_body() const
{
  term_lambda *const tl = get_term_lambda();
  return tl != 0 ? &tl->v : 0;
}

expr_tparams *term::get_bind_tparam() const
{
  term_bind *const tb = get_term_bind();
  return tb != 0 ? tb->tp : 0;
}

const term *term::get_bind_tpvalue() const
{
  term_bind *const tb = get_term_bind();
  return tb != 0 ? &tb->tpv : 0;
}

const bool *term::get_bind_is_upvalue() const
{
  term_bind *const tb = get_term_bind();
  return tb != 0 ? &tb->is_upvalue : 0;
}

const term *term::get_bind_next() const
{
  term_bind *const tb = get_term_bind();
  return tb != 0 ? &tb->next : 0;
}

long term::get_long() const
{
  term_long *const ti = get_term_long();
  return ti != 0 ? ti->v : 0;
}

std::string term::get_string() const
{
  term_string *const ts = get_term_string();
  return ts != 0 ? ts->v : std::string();
}

term_metalist::term_metalist(const term_list_range& a)
  : v(a.begin(), a.end())
{
}

term_expr *term::get_term_expr() const
{
  return dynamic_cast<term_expr *>(ptr);
}
term_metalist *term::get_term_metalist() const
{
  return dynamic_cast<term_metalist *>(ptr);
}

term_lambda *term::get_term_lambda() const
{
  return dynamic_cast<term_lambda *>(ptr);
}

term_bind *term::get_term_bind() const
{
  return dynamic_cast<term_bind *>(ptr);
}

term_long *term::get_term_long() const
{
  return dynamic_cast<term_long *>(ptr);
}

term_string *term::get_term_string() const
{
  return dynamic_cast<term_string *>(ptr);
}

ssize_t term_metalist::find(const term& x) const
{
  ssize_t c = 0;
  for (term_list::const_iterator i = v.begin(); i != v.end();
    ++i, ++c) {
    if (*i == x) {
      return c;
    }
  }
  return -1;
}

term_metalist_idx::index_type term_metalist_idx::create_index(
  const term_list_range& a)
{
  term_metalist_idx::index_type idx;
  for (size_t i = 0; i < a.size(); ++i) {
    idx[a[i]] = i;
  }
  #if 0
  size_t c = 0;
  for (term_list::const_iterator i = a.begin(); i != a.end(); ++i, ++c) {
    idx[*i] = c;
  }
  #endif
  return idx; /* expects return value optimization */
}

term_metalist_idx::term_metalist_idx(const term_list_range& a)
  : term_metalist(a), index(create_index(a))
{
}

ssize_t term_metalist_idx::find(const term& x) const
{
  index_type::const_iterator iter = index.find(x);
  if (iter != index.end()) {
    return iter->second;
  }
  return -1;
}

term_lambda::term_lambda(const std::vector<expr_tparams *>& upvalues,
  expr_tparams *tpms, const term& t)
  : upvalues(upvalues), tparams(tpms), v(t)
{
}

term_bind::term_bind(expr_tparams *tp, const term& tpv, const term& tpv_lexctx,
  bool is_up, const term& next)
  : tp(tp), tpv(tpv), tpv_lexctx(tpv_lexctx), is_upvalue(is_up), next(next)
{
}

template <typename T> bool term_eq(const term_i *x, const term_i *y)
{
  const T *const px = static_cast<const T *>(x);
  const T *const py = static_cast<const T *>(y);
  return px->v == py->v;
}

bool term::operator ==(const term& x) const
{
  const term_sort ts = get_sort();
  const term_sort tsx = x.get_sort();
  if (ts != tsx) {
    return false;
  }
  if (ptr == x.ptr) {
    return true;
  }
  switch (ts) {
  case term_sort_null: return true;
  case term_sort_long: return term_eq<term_long>(ptr, x.ptr);
  case term_sort_string: return term_eq<term_string>(ptr, x.ptr);
  case term_sort_metalist: return term_eq<term_metalist>(ptr, x.ptr);
  case term_sort_lambda:
    {
      const term_lambda& tl0 = *static_cast<const term_lambda *>(ptr);
      const term_lambda& tl1 = *static_cast<const term_lambda *>(x.ptr);
      return tl0.tparams == tl1.tparams && tl0.v == tl1.v;
    }
  case term_sort_bind:
    {
      const term_bind& tb0 = *static_cast<const term_bind *>(ptr);
      const term_bind& tb1 = *static_cast<const term_bind *>(x.ptr);
      return tb0.tp == tb1.tp && tb0.tpv == tb1.tpv &&
	tb0.tpv_lexctx == tb1.tpv_lexctx &&
	tb0.is_upvalue == tb1.is_upvalue && tb0.next == tb1.next;
    }
  case term_sort_expr:
    {
      const term_expr& te0 = *static_cast<const term_expr *>(ptr);
      const term_expr& te1 = *static_cast<const term_expr *>(x.ptr);
      return te0.expr == te1.expr && te0.args == te1.args;
    }
  }
  return false; /* not reached */
}

template <typename T> bool term_lt(const term_i *x, const term_i *y)
{
  const T *const px = static_cast<const T *>(x);
  const T *const py = static_cast<const T *>(y);
  return px->v < py->v;
}

bool term::operator <(const term& x) const
{
  const term_sort ts = get_sort();
  const term_sort tsx = x.get_sort();
  if (ts < tsx) {
    return true;
  }
  if (ts > tsx) {
    return false;
  }
  if (ptr == x.ptr) {
    return false;
  }
  switch (ts) {
  case term_sort_null: return false;
  case term_sort_long: return term_lt<term_long>(ptr, x.ptr);
  case term_sort_string: return term_lt<term_string>(ptr, x.ptr);
  case term_sort_metalist: return term_lt<term_metalist>(ptr, x.ptr);
  case term_sort_lambda:
    {
      const term_lambda& tl0 = *static_cast<const term_lambda *>(ptr);
      const term_lambda& tl1 = *static_cast<const term_lambda *>(x.ptr);
      expr_tparams *tp0 = dynamic_cast<expr_tparams *>(tl0.tparams);
      expr_tparams *tp1 = dynamic_cast<expr_tparams *>(tl1.tparams);
      while (tp0 != 0 || tp1 != 0) {
	const std::string s0 = tp0 != 0 ? tp0->sym : "";
	const std::string s1 = tp1 != 0 ? tp1->sym : "";
	if (s0 < s1) { return true; }
	if (s1 < s0) { return false; }
	tp0 = tp0 != 0 ? tp0->rest : 0;
	tp1 = tp1 != 0 ? tp1->rest : 0;
      }
      return tl0.v < tl1.v;
    }
  case term_sort_bind:
    {
      const term_bind& tb0 = *static_cast<const term_bind *>(ptr);
      const term_bind& tb1 = *static_cast<const term_bind *>(x.ptr);
      expr_tparams *tp0 = dynamic_cast<expr_tparams *>(tb0.tp);
      expr_tparams *tp1 = dynamic_cast<expr_tparams *>(tb1.tp);
      const std::string s0 = tp0 != 0 ? tp0->sym : "";
      const std::string s1 = tp1 != 0 ? tp1->sym : "";
      if (s0 < s1) { return true; }
      if (s1 < s0) { return false; }
      if (tb0.tpv < tb1.tpv) { return true; }
      if (tb1.tpv < tb0.tpv) { return false; }
      if (tb0.tpv_lexctx < tb1.tpv_lexctx) { return true; }
      if (tb1.tpv_lexctx < tb0.tpv_lexctx) { return false; }
      if (tb0.is_upvalue < tb1.is_upvalue) { return true; }
      if (tb1.is_upvalue < tb0.is_upvalue) { return false; }
      return tb0.next < tb1.next;
    }
  case term_sort_expr:
    {
      /* TODO: slow */
      const std::string s0 = term_tostr(term(get_expr()),
	term_tostr_sort_strict);
      const std::string s1 = term_tostr(term(x.get_expr()),
	term_tostr_sort_strict);
      if (s0 < s1) {
	return true;
      }
      if (s0 > s1) {
	return false;
      }
      term_list emp;
      const term_list *a0 = get_args();
      const term_list *a1 = x.get_args();
      a0 = a0 != 0 ? a0 : &emp;
      a1 = a1 != 0 ? a1 : &emp;
      return (*a0) < (*a1);
    }
  }
  return false; /* not reached */
}

std::string meta_term_to_string(const term& t, bool detail_flag)
{
  if (t.is_string()) {
    return t.get_string();
  } else if (t.is_long()) {
    return long_to_string(t.get_long());
  } else if (t.is_metalist()) {
    std::string s = "{";
    const term_list& tl = *t.get_metalist();
    for (term_list::const_iterator i = tl.begin(); i != tl.end(); ++i) {
      if (i != tl.begin()) {
	s += ",";
      }
      s += meta_term_to_string(*i, detail_flag);
    }
    s += "}";
    return s;
  } else {
    expr_i *const e = t.get_expr();
    if (e == 0) {
      return std::string();
    }
    if (detail_flag) {
      return term_tostr_human(t);
    }
    std::string s = term_tostr_human(term(e)); /* drop params */
    size_t c = s.rfind(':');
    if (c != s.npos) {
      s = s.substr(c + 1); /* drop ns */
    }
    return s;
  }
}

long long meta_term_to_long(const term& t)
{
  if (t.is_string()) {
    return string_to_long(t.get_string());
  } else if (t.is_long()) {
    return t.get_long();
  } else {
    return 1LL;
  }
}

};

