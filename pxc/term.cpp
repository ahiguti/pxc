/*
 *  This file is part of PXC, the p extension compiler.
 *  Copyright (C) 2006 A.Higuchi. All rights reserved.
 *  See COPYRIGHT.txt for details.
 *
 *  This file is part of ASE, the abstract script engines.
 *  Copyright (C) 2006 A.Higuchi. All rights reserved.
 *  See COPYRIGHT.txt for details.
 */


#include "expr_impl.hpp"
#include "util.hpp"

namespace pxc {

term_metalist::term_metalist(const term_list& a)
  : v(a)
{
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
  const term_list& a)
{
  term_metalist_idx::index_type idx;
  size_t c = 0;
  for (term_list::const_iterator i = a.begin(); i != a.end(); ++i, ++c) {
    idx[*i] = c;
  }
  return idx; /* expects return value optimization */
}

term_metalist_idx::term_metalist_idx(const term_list& a)
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
      return (*get_args()) < (*x.get_args());
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

