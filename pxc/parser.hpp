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

#ifndef PXC_PARSER_HPP
#define PXC_PARSER_HPP

#include <string>
#include <set>
#include <deque>
#include "expr.hpp"

namespace pxc {

struct source_file_info {
  std::string filename;
  std::string filename_trim;
  std::string content;
};

typedef std::set<std::string> strset;
typedef std::deque<std::string> strlist;
typedef std::map<std::string, checksum_type> depsrc_checksum_type;

struct module_info {
  typedef std::list<source_file_info> source_files_type;
  std::string unique_namespace;
  std::string aux_filename;
  source_files_type source_files;
  imports_type imports; /* direct imports */
  strset cc_srcs;
  strlist cc_srcs_ord;
  strset link_srcs;
  strlist link_srcs_ord;
  coptions self_copts;
  depsrc_checksum_type depsrc_checksum;
  checksum_type source_checksum;
  checksum_type info_checksum;
  unsigned long long src_mask;
  bool source_loaded;
  bool source_modified;
  bool need_rebuild;
  std::string get_name() const {
    return aux_filename.empty() ? unique_namespace : aux_filename;
  }
  module_info() : src_mask(0), source_loaded(false), source_modified(false),
    need_rebuild(false) { }
};

enum compile_mode {
  compile_mode_main,
  compile_mode_import,
  compile_mode_linkonly,
};

struct source_info {
  const module_info *minfo;
  compile_mode mode;
  source_info() : minfo(0), mode(compile_mode_main) { }
};

void pxc_parse_file(const source_info& si, imports_type& imports_r);
void pxc_lex_set_buffer(const char *data, size_t len);
void pxc_lex_unset_buffer();

};

#endif

