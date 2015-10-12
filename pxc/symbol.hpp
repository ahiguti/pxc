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

#ifndef PXC_SYMBOL_HPP
#define PXC_SYMBOL_HPP

#include "expr.hpp"
#include <map>
#include <string>

namespace pxc {

struct symbol {
  symbol();
  symbol(const char *str);
  symbol(const std::string& str);
  const std::string& to_string() const;
  const char *c_str() const;
  bool operator ==(const symbol& x) const {
    return id == x.id;
  }
  bool operator !=(const symbol& x) const {
    return id != x.id;
  }
  bool operator <(const symbol& x) const {
    return id < x.id;
  }
  bool empty() const { return id == 0; }
  bool has_ns() const;
  bool ns_defined(const symbol& ns) const;
  symbol add_ns_if_defined(const symbol& ns) const;
  symbol get_nspart() const;
  symbol get_base() const;
  static void clear();
  size_t get_id() const { return id; }
private:
  symbol(size_t id0) : id(id0) { }
  size_t id;
};

#if 0
struct symbol_eq {
  bool operator ()(const symbol& x, const symbol& y) const {
    return x.get_id() == y.get_id();
  }
};
struct symbol_hash {
  size_t operator ()(const symbol& x) const {
    return x.get_id();
  }
};
#endif

};

#endif

