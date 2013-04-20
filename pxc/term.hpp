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

struct term_expr : public term_i {
  friend struct term;
  explicit term_expr(expr_i *e) : expr(e) { }
  term_expr(expr_i *e, const term_list& a) : expr(e), args(a) { }
  term_sort get_sort() const { return term_sort_expr; }
  expr_i *const expr; /* must be a template, not an instance */
  const term_list args;
};

struct term {
  term() : ptr(0) { }
  term(const term& x) : ptr(x.ptr) { if (ptr) ptr->incref(); }
  explicit term(long long x) : ptr(new term_long(x)) { }
  explicit term(const std::string& x) : ptr(new term_string(x)) { }
  explicit term(const term_list& a) : ptr(new term_metalist(a)) { }
  explicit term(expr_i *e) : ptr(new term_expr(e)) { assert(e); }
  term(expr_i *e, term_list& a) : ptr(new term_expr(e, a)) { }
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
  bool is_long() const { return get_term_long() != 0; }
  bool is_string() const { return get_term_string() != 0; }
  term_i *get() const { return ptr; }
  expr_i *get_expr() const {
    term_expr *const te = get_term_expr();
    return te != 0 ? te->expr : 0;
  }
  const term_list *get_args() const {
    term_expr *const te = get_term_expr();
    return te != 0 ? &te->args : 0;
  }
  const term_list *get_metalist() const {
    term_metalist *const tm = get_term_metalist();
    return tm != 0 ? &tm->v : 0;
  }
  const term_list *get_args_or_metalist() const {
    const term_list *r = get_args();
    if (r == 0) {
      r = get_metalist();
    }
    return r;
  }
  long get_long() const {
    term_long *const ti = get_term_long();
    return ti != 0 ? ti->v : 0;
  }
  std::string get_string() const {
    term_string *const ts = get_term_string();
    return ts != 0 ? ts->v : std::string();
  }
  void clear() {
    if (ptr) {
      ptr->decref();
    }
    ptr = 0;
  }
private:
  term_expr *get_term_expr() const { return dynamic_cast<term_expr *>(ptr); }
  term_metalist *get_term_metalist() const {
    return dynamic_cast<term_metalist *>(ptr); }
  term_long *get_term_long() const { return dynamic_cast<term_long *>(ptr); }
  term_string *get_term_string() const {
    return dynamic_cast<term_string *>(ptr); }
private:
  term_i *ptr;
};

std::string meta_term_to_string(const term& t, bool detail_flag);
long long meta_term_to_long(const term& t);

};

#endif

