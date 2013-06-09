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

#ifndef PXC_TERM_HPP
#define PXC_TERM_HPP

#include <deque>
#include <vector>
#include <map>
#include "expr.hpp"

namespace pxc {

struct term;
typedef std::vector<term> term_list;

enum term_sort {
  term_sort_null,
  term_sort_long,
  term_sort_string,
  term_sort_metalist,
  term_sort_lambda,
  term_sort_bind,
  term_sort_expr,
};

struct term_i;

struct term_i {
  friend struct term;
  term_i() : refcount(1) { }
  virtual ~term_i() { }
  virtual term_sort get_sort() const = 0;
private:
  void incref() { ++refcount; }
  void decref() { if (--refcount == 0) delete this; }
  size_t refcount;
};

struct term_long;
struct term_string;
struct term_metalist;
struct term_lambda;
struct term_bind;
struct term_expr;

struct term {
  term() : ptr(0) { }
  term(const term& x) : ptr(x.ptr) { if (ptr) ptr->incref(); }
  explicit term(long long x);                            /* value */
  explicit term(const std::string& x);                   /* value */
  explicit term(const term_list& a);                     /* metalist */
  term(expr_i *tpms, const term& t);                     /* lambda */
  term(expr_i *tpm, const term& tpmv, bool is_up, const term& next); /* bind */
  explicit term(expr_i *e);                              /* value */
  term(expr_i *e, term_list& a);                         /* application */
  ~term() { if (ptr) ptr->decref(); }
  term& operator =(const term& x) {
    if (this != &x) {
      if (x.ptr) x.ptr->incref();
      if (ptr) ptr->decref();
      ptr = x.ptr;
    }
    return *this;
  }
  term_sort get_sort() const {
    return ptr == 0 ? term_sort_null : ptr->get_sort();
  }
  bool operator ==(const term& x) const;
  bool operator <(const term& x) const;
  bool operator !=(const term& x) const {
    return !((*this) == x);
  }
  bool is_null() const { return ptr == 0; }
  bool is_expr() const { return get_term_expr() != 0; }
  bool is_metalist() const { return get_term_metalist() != 0; }
  bool is_lambda() const { return get_term_lambda() != 0; }
  bool is_bind() const { return get_term_bind() != 0; }
  bool is_long() const { return get_term_long() != 0; }
  bool is_string() const { return get_term_string() != 0; }
  term_i *get() const { return ptr; }
  expr_i *get_expr() const;
  const term_list *get_args() const;
  const term_list *get_metalist() const;
  const term_list *get_args_or_metalist() const;
  expr_i *get_lambda_tparams() const;
  const term *get_lambda_body() const;
  expr_i *get_bind_tparam() const;
  const bool *get_bind_is_upvalue() const;
  const term *get_bind_tpvalue() const;
  const term *get_bind_next() const;
  long get_long() const;
  std::string get_string() const;
  void clear() {
    if (ptr) {
      ptr->decref();
    }
    ptr = 0;
  }
private:
  term_expr *get_term_expr() const;
  term_metalist *get_term_metalist() const;
  term_lambda *get_term_lambda() const;
  term_bind *get_term_bind() const;
  term_long *get_term_long() const;
  term_string *get_term_string() const;
private:
  term_i *ptr;
};

struct term_long : public term_i {
  explicit term_long(long long v) : v(v) { }
  term_sort get_sort() const { return term_sort_long; }
  const long long v;
};

struct term_string : public term_i {
  explicit term_string(const std::string& v) : v(v) { }
  term_sort get_sort() const { return term_sort_string; }
  const std::string v;
};

struct term_metalist : public term_i {
  explicit term_metalist(const term_list& a);
  term_sort get_sort() const { return term_sort_metalist; }
  virtual ssize_t find(const term& t) const;
  const term_list v;
};

struct term_metalist_idx : public term_metalist {
  explicit term_metalist_idx(const term_list& a);
  virtual ssize_t find(const term& t) const;
  typedef std::map<term, size_t> index_type;
  const index_type index; /* for fast lookup */
private:
  static index_type create_index(const term_list& a);
};

struct term_lambda : public term_i {
  explicit term_lambda(expr_i *tparams, const term& t);
  term_sort get_sort() const { return term_sort_lambda; }
  expr_i *const tparams; /* must be a expr_tparams */
  const term v;
};

struct term_bind : public term_i {
  explicit term_bind(expr_i *tp, const term& tpv, bool is_up,
    const term& next);
  term_sort get_sort() const { return term_sort_bind; }
  expr_i *const tp; /* must be a expr_tparams */
  const term tpv;
  bool is_upvalue;
  const term next; /* must be a term_lambda */
};

struct term_expr : public term_i {
  friend struct term;
  explicit term_expr(expr_i *e) : expr(e) { }
  term_expr(expr_i *e, const term_list& a) : expr(e), args(a) { }
  term_sort get_sort() const { return term_sort_expr; }
  expr_i *const expr; /* must be a template, not an instance */
  const term_list args;
};

std::string meta_term_to_string(const term& t, bool detail_flag);
long long meta_term_to_long(const term& t);

};

#endif

