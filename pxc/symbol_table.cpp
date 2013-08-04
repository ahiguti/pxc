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

#define DBG_DEF(x)
#define DBG(x)
#define DBG_EXT(x)

namespace pxc {

symbol_table::symbol_table(expr_block *block_backref)
  : block_backref(block_backref), require_thisup(false),
    block_esort(expr_e_block), tempvar_id_count(0)
{
}

symbol_table *symbol_table::get_lexical_parent() const
{
  expr_i *e = block_backref->parent_expr;
  while (e != 0) {
    expr_block *bl = dynamic_cast<expr_block *>(e);
    if (bl != 0) {
      return &bl->symtbl;
    }
    e = e->parent_expr;
  }
  return 0;
}

void symbol_table::define_name(const std::string& shortname,
  const std::string& curns, expr_i *e, attribute_e attr, expr_stmts *stmt)
  /* stmt must be set if e is a local variable */
{
  std::string name = shortname;
  if (get_lexical_parent() == 0 && !curns.empty()) {
    name = curns + "::" + shortname;
  }
  DBG_DEF(fprintf(stderr,
    "symtbl=%p define[%s] curns=[%s] (%s:%d) s=%d e=%p lparent=%p\n",
    this, name.c_str(), curns.c_str(), e->fname, e->line, (int)block_esort, e,
    get_lexical_parent()));
  locals_type::iterator i = locals.find(name);
  if (i == locals.end()) {
    locals[name] = localvar_info(e, attr, stmt);
    local_names.push_back(name);
    return;
  }
  arena_error_throw(e, "Duplicated name %s (defined at %s:%d)",
    name.c_str(), i->second.edef->fname, i->second.edef->line);
}

expr_i *symbol_table::resolve_name_nothrow(const std::string& fullname,
  bool no_private, const std::string& curns, bool& is_global_r,
  bool& is_upvalue_r, bool& is_memfld_r)
{
  localvar_info v = resolve_name_nothrow_internal(fullname, curns, false,
    is_global_r, is_upvalue_r, is_memfld_r);
  if (v.has_attrib_private()) {
    if (no_private) {
      return 0;
    }
    const std::string nspart = get_namespace_part(fullname);
    if (!nspart.empty()) {
      return 0;
    }
  }
  return v.edef;
}

expr_i *symbol_table::resolve_name_nothrow_memfld(const std::string& fullname,
  bool no_private, const std::string& curns)
{
  bool is_global_dmy = false;
  bool is_upvalue_dmy = false;
  bool is_memfld_dmy = false;
  localvar_info v = resolve_name_nothrow_internal(fullname, curns, true,
    is_global_dmy, is_upvalue_dmy, is_memfld_dmy);
  if (v.has_attrib_private()) {
    if (no_private) {
      return 0;
    }
    const std::string nspart = get_namespace_part(fullname);
    if (!nspart.empty()) {
      return 0;
    }
  }
  return v.edef;
}

localvar_info symbol_table::resolve_name_nothrow_internal(
  const std::string& fullname, const std::string& curns,
  bool memfld_only, bool& is_global_r, bool& is_upvalue_r,
  bool& is_memfld_r)
{
  localvar_info v;
  do {
    is_upvalue_r = false;
    is_memfld_r = block_esort == expr_e_struct;
    is_global_r = (get_lexical_parent() == 0);
    locals_type::const_iterator i = locals.find(fullname);
    if (i != locals.end()) {
      v = i->second;
      break;
    }
    if (memfld_only) {
      break;
    }
    symbol_table *psymtbl = get_lexical_parent();
    if (psymtbl != 0) {
      /* non-global */
      bool is_upvalue_dummy = false;
      v = psymtbl->resolve_name_nothrow_internal(fullname, curns, false,
	is_global_r, is_upvalue_dummy, is_memfld_r);
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
      if (v.edef != 0) {
	break;
      }
    } else {
      /* global */
      if (!has_namespace(fullname)) {
	/* try the current namespace */
	std::string pname = curns + "::" + fullname;
	locals_type::const_iterator j = locals.find(pname);
	if (j != locals.end()) {
	  v = j->second;
	  DBG_EXT(fprintf(stderr, "found curns %s %s %s\n", fullname.c_str(),
	    curns.c_str(), pname.c_str()));
	  break;
	}
	/* try nsextends of the curns */
	{
	  nsextends_type::const_iterator j = nsextends.find(curns);
	  if (j != nsextends.end()) {
	    for (nsextend_entries::const_iterator k = j->second.begin();
	      k != j->second.end(); ++k) {
	      pname = *k + "::" + fullname;
#if 0
fprintf(stderr, "try %s\n", pname.c_str());
#endif
	      locals_type::const_iterator jp = locals.find(pname);
	      if (jp != locals.end()) {
		v = jp->second;
		DBG_EXT(fprintf(stderr, "found nsext curns %s %s %s\n",
		  fullname.c_str(), curns.c_str(), pname.c_str()));
		break;
	      }
	    }
	  }
	  if (v.edef != 0) {
	    break;
	  }
	}
	/* try noprefix imports of curns */
	nsaliases_type::const_iterator iter = nsaliases.find(
	  std::make_pair(curns, ""));
	if (iter != nsaliases.end()) {
	  for (nsalias_entries::const_iterator i = iter->second.begin();
	    i != iter->second.end(); ++i) {
	    pname = *i + "::" + fullname;
#if 0
fprintf(stderr, "try %s\n", pname.c_str());
#endif
	    locals_type::const_iterator ip = locals.find(pname);
	    if (ip != locals.end()) {
	      v = ip->second;
	      DBG_EXT(fprintf(stderr, "found noprefix curns %s %s %s\n",
		fullname.c_str(), curns.c_str(), pname.c_str()));
	      break;
	    }
	    /* try nsextends of a noprefix import of curns */
	    nsextends_type::const_iterator j = nsextends.find(*i);
	    if (j != nsextends.end()) {
	      for (nsextend_entries::const_iterator k = j->second.begin();
		k != j->second.end(); ++k) {
		pname = *k + "::" + fullname;
		locals_type::const_iterator jp = locals.find(pname);
		if (jp != locals.end()) {
		  v = jp->second;
		  DBG_EXT(fprintf(stderr,
		    "found nsext noprefix curns %s %s %s\n",
		    fullname.c_str(), curns.c_str(), pname.c_str()));
		  break;
		}
	      }
	    }
	    if (v.edef != 0) {
	      break;
	    }
	  }
	  if (v.edef != 0) {
	    break;
	  }
	}
      } else {
	/* has_namespace(fullname) */
	const std::string nspart = get_namespace_part(fullname);
	const std::string shortname = to_short_name(fullname);
	/* try namespace aliases */
	nsaliases_type::const_iterator iter = nsaliases.find(
	  std::make_pair(curns, nspart));
	if (iter != nsaliases.end()) {
	  DBG_EXT(fprintf(stderr, "nsalias found %s -> %s\n", curns.c_str(),
	    nspart.c_str()));
	  for (nsalias_entries::const_iterator i = iter->second.begin();
	    i != iter->second.end(); ++i) {
	    std::string pname = *i + "::" + shortname;
	    DBG_EXT(fprintf(stderr, "nsalias found %s -> %s %s\n",
	      curns.c_str(), nspart.c_str(), pname.c_str()));
	    locals_type::const_iterator ip = locals.find(pname);
	    if (ip != locals.end()) {
	      v = ip->second;
	      break;
	    }
	    /* try nsextends of a nsalias of curns */
	    DBG_EXT(fprintf(stderr, "nsalias try nsextends %s\n", i->c_str()));
	    nsextends_type::const_iterator j = nsextends.find(*i);
	    if (j != nsextends.end()) {
	      DBG_EXT(fprintf(stderr, "nsalias nsextends found %s\n",
		i->c_str()));
	      for (nsextend_entries::const_iterator k = j->second.begin();
		k != j->second.end(); ++k) {
		pname = *k + "::" + shortname;
		DBG_EXT(fprintf(stderr, "nsalias nsextends lookup %s\n",
		  pname.c_str()));
		locals_type::const_iterator jp = locals.find(pname);
		if (jp != locals.end()) {
		  v = jp->second;
		  break;
		}
	      }
	    }
	    if (v.edef != 0) {
	      break;
	    }
	  }
	  if (v.edef != 0) {
	    break;
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
  return v;
}

expr_i *symbol_table::resolve_name(const std::string& fullname,
  const std::string& curns, expr_i *e, bool& is_global_r, bool& is_upvalue_r)
{
  bool is_memfld_r = false;
  localvar_info v = resolve_name_nothrow_internal(fullname, curns, false,
    is_global_r, is_upvalue_r, is_memfld_r);
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
    const std::string nspart = get_namespace_part(fullname);
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
}

};

