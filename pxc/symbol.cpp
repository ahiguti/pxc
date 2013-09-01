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

#include "symbol.hpp"
#include "expr_misc.hpp"
#include <map>
#include <vector>

namespace pxc {

typedef std::map<size_t, size_t> nsname_map_type;
struct symbol_info {
  std::string str;
  size_t ns_part;
  size_t base_part;
  nsname_map_type nsname_map;
  symbol_info() : ns_part(0), base_part(0) { }
};
typedef std::map<std::string, size_t> symbols_type;
typedef std::vector<symbol_info> symbol_strings_type;
symbols_type symbols;
symbol_strings_type symbol_strings;

static size_t intern(const std::string& s)
{
  if (symbol_strings.empty()) {
    symbol_strings.push_back(symbol_info()); /* for id == 0 */
  }
  if (s.empty()) {
    return 0;
  }
  symbols_type::iterator i = symbols.find(s);
  if (i != symbols.end()) {
    return i->second;
  }
  /* TODO: exception safety */
  size_t cur = symbol_strings.size();
  symbols.insert(std::make_pair(s, cur));
  symbol_strings.push_back(symbol_info());
  symbol_strings.back().str = s;
  if (has_namespace(s)) {
    std::string ns = get_namespace_part(s);
    size_t ns_part = intern(ns);
    std::string sh = to_short_name(s);
    size_t base_part = intern(sh);
    symbol_strings[cur].ns_part = ns_part;
    symbol_strings[cur].base_part = base_part;
    symbol_strings[base_part].nsname_map[ns_part] = cur;
  }
  return cur;
}

symbol::symbol()
  : id(intern(std::string()))
{
}

symbol::symbol(const char *s)
  : id(intern(s))
{
}

symbol::symbol(const std::string& s)
  : id(intern(s))
{
}

const std::string& symbol::to_string() const
{
  return symbol_strings[id].str;
}

const char *symbol::c_str() const
{
  return to_string().c_str();
}

void symbol::clear()
{
  symbols.clear();
  symbol_strings.clear();
}

bool symbol::has_ns() const
{
  symbol_info& si = symbol_strings[id];
  return si.ns_part != 0;
}

bool symbol::ns_defined(const symbol& ns) const
{
  symbol_info& si = symbol_strings[id];
  return si.nsname_map.find(ns.id) != si.nsname_map.end();
}

symbol symbol::add_ns_if_defined(const symbol& ns) const
{
  symbol_info& si = symbol_strings[id];
  nsname_map_type::const_iterator iter = si.nsname_map.find(ns.id);
  if (iter != si.nsname_map.end()) {
    return symbol(iter->second);
  }
  return symbol();
}

symbol symbol::get_nspart() const
{
  return symbol(symbol_strings[id].ns_part);
}

symbol symbol::get_base() const
{
  return symbol(symbol_strings[id].base_part);
}

};

