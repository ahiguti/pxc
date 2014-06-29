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
#include "util.hpp"

#define DBG_DEF(x)
#define DBG(x)
#define DBG_TIMING(x)
#define DBG_EXT(x)
#define DBG_FIND(x)

namespace pxc {

symbol_table::symbol_table(expr_block *block_backref)
  : block_backref(block_backref), require_thisup(false),
    block_esort(expr_e_block), tempvar_id_count(0)
{
}

symbol_table *symbol_table::get_lexical_parent() const
{
  return block_backref->symtbl_lexical;
}

symbol append_namespace(const symbol& ns, const symbol& name)
{
  return ns.to_string() + "::" + name.to_string(); /* TODO */
}

void symbol_table::define_name(const symbol& shortname,
  const symbol& curns, expr_i *e, attribute_e attr, expr_stmts *stmt)
  /* stmt must be set if e is a local variable */
{
  bool is_global = false;
  symbol name = shortname;
  if (get_lexical_parent() == 0 && !curns.empty()) {
    name = append_namespace(curns, shortname);
    is_global = true;
  }
  DBG_DEF(fprintf(stderr,
    "symtbl=%p define[%s] curns=[%s] (%s:%d) s=%d e=%p lparent=%p\n",
    this, name.c_str(), curns.c_str(), e->fname, e->line, (int)block_esort, e,
    get_lexical_parent()));
  if (block_backref->compiled_flag) {
    arena_error_throw(block_backref,
      "Internal error: symbol_table::define_name('%s'): "
      "block is compiled already", shortname.c_str());
  }
  locals_type::iterator i = locals.find(name);
  DBG_FIND(fprintf(stderr, "symbol_table::find0 %s %d\n", name.c_str(),
    int(i != locals.end())));
  if (i != locals.end()) {
    arena_error_throw(e, "Duplicated name %s (defined at %s:%d)",
      name.c_str(), i->second.edef->fname, i->second.edef->line);
  }
  locals[name] = localvar_info(e, attr, stmt);
  local_names.push_back(name);
  if (is_global) {
    nsmap_entry& nsent = nsmap[curns];
    nsent[shortname] = name; /* short to long */
  }
}

expr_i *symbol_table::resolve_name_nothrow_ns(const symbol& fullname,
  bool no_private_curns, const symbol& curns, expr_i *pos)
{
  bool is_global_dmy = false;
  bool is_upvalue_dmy = false;
  bool is_memfld_dmy = false;
  symbol_table *symtbl = this;
  while (true) {
    symbol_table *p = symtbl->get_lexical_parent();
    if (p == 0) {
      break;
    }
    symtbl = p;
  }
  localvar_info v = symtbl->resolve_name_nothrow_internal(fullname, curns, pos,
    false, false, is_global_dmy, is_upvalue_dmy, is_memfld_dmy);
  if (v.has_attrib_private()) {
    if (no_private_curns) {
      return 0;
    }
    const symbol nspart = get_namespace_part(fullname);
    if (!nspart.empty() && nspart != curns) {
      return 0;
    }
  }
  return v.edef;
}

expr_i *symbol_table::resolve_name_nothrow_memfld(const symbol& fullname,
  bool no_private, bool no_generated, const symbol& curns, expr_i *pos)
{
  bool is_global_dmy = false;
  bool is_upvalue_dmy = false;
  bool is_memfld_dmy = false;
  localvar_info v = resolve_name_nothrow_internal(fullname, curns, pos, true,
    no_generated, is_global_dmy, is_upvalue_dmy, is_memfld_dmy);
  if (v.has_attrib_private()) {
    if (no_private) {
      return 0;
    }
    const symbol nspart = get_namespace_part(fullname);
    if (!nspart.empty()) {
      return 0;
    }
  }
  return v.edef;
}

static void check_compiled_ns(const symbol& fullname, const symbol& nspart,
  const symbol& curns, expr_i *pos)
{
  compiled_ns_type::const_iterator iter = compiled_ns.find(nspart);
  if (iter != compiled_ns.end() && !iter->second) {
    /* ns defined, but not compiled yet */
    if (fullname.empty()) {
      arena_error_throw(pos,
	"Can not get symbols: namespace '%s' is not compiled yet",
	nspart.c_str());
    } else {
      arena_error_throw(pos,
	"Undefined symbol '%s' (default namespace: '%s')"
	"(namespace '%s' is not compiled yet)",
	fullname.c_str(), curns.c_str(), nspart.to_string().c_str());
    }
  }
}

static bool is_nsalias(const symbol& fullname, const symbol& curns)
{
  const symbol nspart = get_namespace_part(fullname);
  nsaliases_type::const_iterator iter = nsaliases.find(
    std::make_pair(curns, nspart));
  return iter != nsaliases.end();
}

localvar_info symbol_table::resolve_global_internal(const symbol& shortname,
  const symbol& ns, const symbol& curns, bool no_generated, expr_i *pos)
{
  localvar_info v;
  const symbol pname = shortname.add_ns_if_defined(ns);
  if (pname.empty()) {
    return v;
  }
  DBG_EXT(fprintf(stderr, "lookup global %s\n", pname.c_str()));
  locals_type::const_iterator i = locals.find(pname);
  DBG_FIND(fprintf(stderr, "symbol_table::find2 %s %d\n", pname.c_str(),
    int(i != locals.end())));
  if (i != locals.end() &&
    (!no_generated || !i->second.edef->generated_flag)) {
    v = i->second;
    return v;
  }
  /* not found */
  if (!no_generated) {
    /* if no_generated is not set, target ns must be compiled to avoid
     * returning inconsistent result */
    check_compiled_ns(pname, ns, curns, pos);
  }
  return v;
}

symbol get_lexical_context_ns(expr_i *pos)
{
  symbol_table *stbl = pos->symtbl_lexical;
  while (stbl != 0) {
    expr_i *e = stbl->block_backref;
    std::string s = e->get_unique_namespace();
    if (!s.empty()) {
      return symbol(s);
    }
    stbl = stbl->get_lexical_parent();
  }
  return symbol();
}

static bool is_visible_from_pos(const symbol& ns, expr_i *pos)
{
  const symbol posns = get_lexical_context_ns(pos);
  if (ns == posns) {
    return true;
  }
  if (nsimports_rec.find(std::make_pair(posns, ns)) != nsimports_rec.end()) {
    #if 0
    fprintf(stderr, "visible %s from %s [%s]\n", ns.c_str(), posns.c_str(),
      pos->dump(0).c_str());
    #endif
    return true;
  }
  #if 0
  fprintf(stderr, "invisible %s from %s [%s]\n", ns.c_str(), posns.c_str(),
    pos->dump(0).c_str());
  #endif
  return false;
}

symbol_table::locals_type::const_iterator symbol_table::find(const symbol& k,
  bool no_generated) const
{
  if (!no_generated && !block_backref->compiled_flag) {
    arena_error_throw(0,
      "Internal error: symbol_table::find('%s'): "
      "block '%s' is not compiled yet", k.c_str(),
      block_backref->dump(0).c_str());
  }
  locals_type::const_iterator i = locals.find(k);
  DBG_FIND(fprintf(stderr, "symbol_table::find3 %s %d\n", k.c_str(),
    int(i != locals.end())));
  if (no_generated && i->second.edef->generated_flag) {
    return end();
  }
  return i;
}

symbol_table::locals_type::const_iterator symbol_table::end() const
{
  return locals.end();
}

void symbol_table::get_fields(std::list<expr_var *>& flds_r) const
{
  flds_r.clear();
  if (!block_backref->compiled_flag) {
    arena_error_throw(block_backref,
      "Internal error: symbol_table::get_fields(): "
      "block is not compiled yet");
  }
  local_names_type::const_iterator i;
  for (i = local_names.begin(); i != local_names.end(); ++i) {
    const locals_type::const_iterator iter = find(*i, false);
      /* incl generated */
    assert(iter != end());
    if (iter->second.edef && iter->second.edef->get_esort() == expr_e_var) {
      flds_r.push_back(ptr_down_cast<expr_var>(iter->second.edef));
    }
  }
}

symbol_table::local_names_type const& symbol_table::get_local_names() const
{
  if (!block_backref->compiled_flag) {
    arena_error_throw(block_backref,
      "Internal error: symbol_table::get_local_names(): "
      "block is not compiled yet");
  }
  return local_names;
}

symbol_table::locals_type const& symbol_table::get_upvalues() const
{
  if (!block_backref->compiled_flag) {
    arena_error_throw(block_backref,
      "Internal error: symbol_table::get_upvalues(): "
      "block is not compiled yet");
  }
  return upvalues;
}

void symbol_table::get_ns_symbols(const symbol& ns,
  std::list< std::pair<symbol, localvar_info> >& locals_r) const
{
  check_compiled_ns(symbol(), ns, ns, block_backref);
  local_names_type::const_iterator i;
  for (i = local_names.begin(); i != local_names.end(); ++i) {
    locals_type::const_iterator j = locals.find(*i);
    assert(j != locals.end());
    const localvar_info& lv = j->second;
    expr_i *const e = lv.edef;
    if (lv.has_attrib_private() ||
      e->get_unique_namespace() != ns.to_string()) {
      continue;
    }
    locals_r.push_back(std::make_pair(*i, lv));
  }
}

localvar_info symbol_table::resolve_name_nothrow_internal(
  const symbol& fullname, const symbol& curns, expr_i *pos,
  bool memfld_only, bool memfld_nogen, bool& is_global_r, bool& is_upvalue_r,
  bool& is_memfld_r)
{
  localvar_info v;
  DBG_TIMING(double t[16] = { 0 });
  DBG_TIMING(t[0] = gettimeofday_double());
  do {
    is_upvalue_r = false;
    is_memfld_r = block_esort == expr_e_struct;
    is_global_r = (get_lexical_parent() == 0);
    DBG_TIMING(t[1] = gettimeofday_double());
    if (is_global_r && fullname.has_ns() && is_nsalias(fullname, curns)) {
      /* skip */
      /* fullname has a prefix which is an alias. we must ignore non-alias
       * namespace of the same name as the prefix.*/
    } else {
      locals_type::const_iterator i = locals.find(fullname);
      DBG_FIND(fprintf(stderr, "symbol_table::find1 %s %d\n", fullname.c_str(),
	int(i != locals.end())));
      if (i != locals.end() &&
	(!memfld_nogen || !i->second.edef->generated_flag)) {
	v = i->second;
	return v;
      }
      /* missed */
      {
	const symbol nspart = get_namespace_part(fullname);
	if (is_global_r && !nspart.empty()) {
	  check_compiled_ns(fullname, nspart, curns, pos);
	}
      }
    }
    if (memfld_only) {
      if (!block_backref->compiled_flag && !memfld_nogen) {
	/* this block is not compiled yet */
	arena_error_throw(pos,
	  "Undefined symbol '%s' (default namespace: '%s')"
	  "(target type is not compiled yet)",
	  fullname.c_str(), curns.c_str());
      }
      return v;
    }
    DBG_TIMING(t[2] = gettimeofday_double());
    symbol_table *psymtbl = get_lexical_parent();
    if (psymtbl != 0) {
      /* non-global */
      v = psymtbl->resolve_name_nothrow_internal(fullname, curns, pos,
	false, false, is_global_r, is_upvalue_r, is_memfld_r);
      if (v.edef != 0 && block_esort != expr_e_block) {
	bool is_const = false;
	switch (v.edef->get_esort()) {
	case expr_e_funcdef:
	case expr_e_typedef:
	case expr_e_metafdef:
	case expr_e_struct:
	case expr_e_dunion:
	case expr_e_interface:
	case expr_e_enumval:
	case expr_e_tparams:
	  is_const = true;
	  break;
	default:
	  is_const = false;
	  break;
	}
	if (is_memfld_r) {
	  require_thisup = true;
	}
	if (!is_const) {
	  is_upvalue_r = true;
	  if (block_esort != expr_e_funcdef && !is_global_r) {
	    /* structs and interfaces cant have an upvalue ex globals */
	    v = localvar_info();
	  } else {
	    if (!is_global_r && v.edef != 0) {
	      /* need to generate more verbose variable symbol */
	      if (v.edef->get_esort() == expr_e_var) {
		ptr_down_cast<expr_var>(v.edef)->used_as_upvalue = true;
	      } else if (v.edef->get_esort() == expr_e_argdecls) {
		ptr_down_cast<expr_argdecls>(v.edef)->used_as_upvalue = true;
	      }
	    }
	    upvalues[fullname] = v;
	    DBG(fprintf(stderr, "%p upvalue %s\n", this, fullname.c_str()));
	  }
	}
	if (v.edef != 0 && v.edef->get_esort() == expr_e_funcdef &&
	  ptr_down_cast<expr_funcdef>(v.edef)->is_member_function() &&
	  block_esort != expr_e_funcdef) {
	  /* calling member function from an internal struct */
	  v = localvar_info();
	}
	if (v.edef != 0 && v.edef->get_esort() == expr_e_argdecls &&
	  v.edef->symtbl_lexical->block_esort == expr_e_struct) {
	  /* struct argument is not visible from member functions */
	  v = localvar_info();
	}
      }
      DBG_TIMING(t[3] = gettimeofday_double());
      return v;
    } else {
      /* global */
      DBG_EXT(fprintf(stderr, "global %s\n", fullname.c_str()));
      DBG_TIMING(t[4] = gettimeofday_double());
      if (!fullname.has_ns()) {
	/* try the current namespace */
	v = resolve_global_internal(fullname, curns, curns, true, pos);
	  /* no-generated symbol only */
	if (v.edef != 0) {
	  return v;
	}
	/* try nsextends of the curns */
	DBG_TIMING(t[5] = gettimeofday_double());
	{
	  nsextends_type::const_iterator j = nsextends.find(curns);
	  if (j != nsextends.end()) {
	    for (symbol_list::const_iterator k = j->second.begin();
	      k != j->second.end(); ++k) {
	      /* *k is imported from pos? */
	      if (!is_visible_from_pos(*k, pos)) {
		continue;
	      }
	      v = resolve_global_internal(fullname, *k, curns, false, pos);
	      if (v.edef != 0) {
		return v;
	      }
	    }
	  }
	  DBG_TIMING(t[6] = gettimeofday_double());
	}
	DBG_TIMING(t[7] = gettimeofday_double());
	/* try noprefix imports of curns */
	nsaliases_type::const_iterator iter = nsaliases.find(
	  std::make_pair(curns, ""));
	if (iter != nsaliases.end()) {
	  for (symbol_list::const_iterator i = iter->second.begin();
	    i != iter->second.end(); ++i) {
	    v = resolve_global_internal(fullname, *i, curns, false, pos);
	    if (v.edef != 0) {
	      return v;
	    }
	    /* try nschains of a noprefix import of curns */
	    nschains_type::const_iterator j = nschains.find(*i);
	    if (j != nschains.end()) {
	      for (symbol_list::const_iterator k = j->second.begin();
		k != j->second.end(); ++k) {
		v = resolve_global_internal(fullname, *k, curns, false, pos);
		if (v.edef != 0) {
		  return v;
		}
	      }
	    }
	  }
	  DBG_TIMING(t[8] = gettimeofday_double());
	}
	/* try the current namespace again, incl generated syms */
	v = resolve_global_internal(fullname, curns, curns, false, pos);
	if (v.edef != 0) {
	  return v;
	}
      } else {
	/* has_namespace(fullname) */
	const symbol nspart = get_namespace_part(fullname);
	const symbol shortname = to_short_name(fullname);
	DBG_EXT(fprintf(stderr, "has_nspart %s\n", fullname.c_str()));
	/* is nspart an alias? */
	nsaliases_type::const_iterator iter = nsaliases.find(
	  std::make_pair(curns, nspart));
	DBG_TIMING(t[9] = gettimeofday_double());
	if (iter != nsaliases.end()) {
	  /* try namespace aliases */
	  DBG_EXT(fprintf(stderr, "nsalias %s -> %s\n", curns.c_str(),
	    nspart.c_str()));
	  for (symbol_list::const_iterator i = iter->second.begin();
	    i != iter->second.end(); ++i) {
	    v = resolve_global_internal(shortname, *i, curns, false, pos);
	    if (v.edef != 0) {
	      return v;
	    }
	    /* try nsextends of an nsalias of curns */
	    DBG_EXT(fprintf(stderr, "nsalias try nsextends %s\n", i->c_str()));
	    nsextends_type::const_iterator j = nsextends.find(*i);
	    if (j != nsextends.end()) {
	      for (symbol_list::const_iterator k = j->second.begin();
		k != j->second.end(); ++k) {
		/* *k is imported from pos? */
		if (!is_visible_from_pos(*k, pos)) {
		  continue;
		}
		v = resolve_global_internal(shortname, *k, curns, false,
		  pos);
		if (v.edef != 0) {
		  return v;
		}
	      }
	    }
	    DBG_TIMING(t[10] = gettimeofday_double());
	  }
	  DBG_TIMING(t[11] = gettimeofday_double());
	} else {
	  /* nspart is not an alias */
	  DBG_EXT(fprintf(stderr, "real try nsextends %s\n", nspart.c_str()));
	  nsextends_type::const_iterator j = nsextends.find(nspart);
	  if (j != nsextends.end()) {
	    DBG_EXT(fprintf(stderr, "real nsextends found %s, %zu\n",
	      nspart.c_str(), j->second.size()));
	    for (symbol_list::const_iterator k = j->second.begin();
	      k != j->second.end(); ++k) {
	      /* *k is imported from pos? */
	      if (!is_visible_from_pos(*k, pos)) {
		continue;
	      }
	      v = resolve_global_internal(shortname, *k, curns, false, pos);
	      if (v.edef != 0) {
		return v;
	      }
	    }
	  }
	}
      }
    }
    v = localvar_info();
  } while (0);
  DBG(fprintf(stderr,
    "symtbl=%p (%s:%d) resolve fullname=%s curns=%s %s v=%p\n", this,
    block_backref->fname,
    block_backref->line, fullname.c_str(), curns.c_str(),
    is_upvalue_r ? "up" : "-", v.edef));
  DBG_TIMING(double tcur = gettimeofday_double());
  #if 0
  if (tcur - t[0] > 0.00002) {
    fprintf(stderr, "slow reso [%s] [%s]", fullname.c_str(), curns.c_str());
    for (int i = 1; i < 16; ++i) {
      if (t[i] != 0) fprintf(stderr, " (%d) %f", i, t[i] - t[0]);
    }
    fprintf(stderr, "\n");
  }
  #endif
  return v;
}

expr_i *symbol_table::resolve_name(const symbol& fullname,
  const symbol& curns, expr_i *e, bool& is_global_r, bool& is_upvalue_r)
{
  bool is_memfld_r = false;
  localvar_info v = resolve_name_nothrow_internal(fullname, curns, e, false,
    false, is_global_r, is_upvalue_r, is_memfld_r);
  if (v.edef == 0) {
    if (is_upvalue_r) {
      arena_error_throw(e, "Undefined symbol '%s' (default namespace: '%s')"
	"(can not use '%s' as an upvalue)", fullname.c_str(),
	curns.c_str(), fullname.c_str());
    } else {
      arena_error_throw(e, "Undefined symbol '%s' (default namespace: '%s')",
	fullname.c_str(), curns.c_str());
    }
  }
  if (v.has_attrib_private()) {
    const symbol nspart = get_namespace_part(fullname);
    if (!nspart.empty()) {
      arena_error_throw(v.edef, "Symbol '%s' is private", fullname.c_str());
    }
  }
  return v.edef;
}

int
symbol_table::generate_tempvar()
{
  return tempvar_id_count++;
}

void
symbol_table::clear_symbols()
{
  /* called when a template is instantiated */
  tempvar_id_count = 0;
  locals.clear();
  local_names.clear();
  upvalues.clear();
  nsmap.clear(); /* not necessary */
}

};

