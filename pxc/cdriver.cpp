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

#include <stdio.h>
#include <set>
#include <list>
#include <deque>
#include "expr.hpp"
#include "util.hpp"
#include "parser.hpp"
#include "pxc_md5.hpp"

#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <assert.h>
#include <stdexcept>

#define DBG(x)
#define DBG_SRCLIST(x)
#define DBG_SUM(x)
#define DBG_MOD(x)
#define DBG_INF(x)
#define DBG_CHK(x)
#define DBG_LINK(x)

#define PXC_TIMESTAMP_MARGIN 0

namespace pxc {

typedef std::set<std::string> strset;
typedef std::map<std::string, std::string> strmap;

struct profile_settings {
  std::string md5sum;
  strmap mapval;
  strlist incdir;
  std::string cxx;
  std::string cflags;
  std::string ldflags;
  bool generate_dynamic; /* TODO */
  profile_settings() : generate_dynamic(false) { }
};

typedef std::list<source_info> sources_type;

struct parser_options {
  std::string profile_name;
  std::string src_filename;
  profile_settings profile;
  std::string work_dir;
  int verbose;
  bool clean_flag;
  bool no_build;
  bool no_update;
  bool no_execute;
  bool no_realpath;
  bool show_out;
  std::string argv0;
  time_t argv0_ctime;
  parser_options() : verbose(0), clean_flag(false), no_build(false),
    no_update(false), no_execute(false), no_realpath(false), show_out(false),
    argv0_ctime(0) { }
};

struct all_modules_info {
  typedef std::map<std::string, module_info> modules_type;
  modules_type modules;
};

static std::string get_filename_for_ns(const std::string& ns)
{
  std::string r;
  size_t pos = 0;
  while (true) {
    if (!r.empty()) {
      r += "/";
    }
    size_t delim = ns.find("::", pos);
    if (delim == ns.npos) {
      r += ns.substr(pos);
      break;
    }
    r += ns.substr(pos, delim - pos);
    pos = delim + 2;
  }
  r += ".px";
  return r;
}

static std::string get_work_filename(const parser_options& po,
  const module_info& mi, bool get_dirname)
{
  std::string fn;
  if (!mi.aux_filename.empty()) {
    fn = mi.aux_filename;
    fn = escape_hex_non_alnum(fn);
    fn = "fn/" + fn;
  } else {
    assert(!mi.unique_namespace.empty());
    fn = "ns/" + mi.unique_namespace;
  }
  std::string dn, bn, r;
  size_t delim = fn.rfind('/');
  if (delim != fn.npos) {
    dn = fn.substr(0, delim + 1);
    bn = fn.substr(delim + 1);
  } else {
    dn = "";
    bn = fn;
  }
  if (get_dirname) {
    r = po.work_dir + "/" + dn;
  } else {
    r = po.work_dir + "/" + dn + bn;
  }
  DBG(fprintf(stderr, "fn=%s r=%s\n", fn.c_str(), r.c_str()));
  return r;
}

static std::string get_cc_filename(const parser_options& po,
  const module_info& mi)
{
  return get_work_filename(po, mi, false) + ".cc";
}

static std::string get_o_filename(const parser_options& po,
  const module_info& mi)
{
  return get_work_filename(po, mi, false) + ".o";
}

static std::string get_so_filename(const parser_options& po,
  const module_info& mi)
{
  return get_work_filename(po, mi, false) + ".so";
}

static std::string get_exe_filename(const parser_options& po,
  const module_info& mi)
{
  return get_work_filename(po, mi, false) + ".exe";
}

static std::string get_info_filename(const parser_options& po,
  const module_info& mi)
{
  return get_work_filename(po, mi, false) + ".inf";
}

static bool check_source_timestamp(const parser_options& po, module_info& mi,
  const std::string& info_filename)
{
  bool not_modified = true;
  unsigned long long src_mask = 0;
  unsigned long long src_mask_bit = 1;
  module_info::source_files_type::iterator i;
  for (i = mi.source_files.begin(); i != mi.source_files.end();
    ++i, src_mask_bit <<= 1) {
    const std::string& fn = i->filename;
    while (true) {
      struct stat sbuf;
      if (stat(fn.c_str(), &sbuf) != 0) {
	if (errno == ENOENT) {
	  break;
	}
	arena_error_throw(0, "%s:0: failed to stat: errno=%d\n",
	  fn.c_str(), errno);
      }
      if (sbuf.st_ctime > mi.source_checksum.timestamp) {
	if (po.verbose > 1) {
	  fprintf(stderr,
	    "check_source_timestamp %s: st_ctime=%lu checksum=%lu\n",
	    fn.c_str(), (unsigned long)sbuf.st_ctime,
	    (unsigned long)mi.source_checksum.timestamp);
	}
	not_modified = false;
      }
      if (sbuf.st_mtime > mi.source_checksum.timestamp) {
	if (po.verbose > 1) {
	  fprintf(stderr,
	    "check_source_timestamp %s: st_mtime=%lu checksum=%lu\n",
	    fn.c_str(), (unsigned long)sbuf.st_mtime,
	    (unsigned long)mi.source_checksum.timestamp);
	}
	not_modified = false;
      }
      src_mask |= src_mask_bit;
      break;
    }
  }
  if (src_mask != mi.src_mask) {
    if (po.verbose > 1) {
      fprintf(stderr,
	"check_source_timestamp (%s,%s): src_mask=%llu mi.src_mask=%llu\n",
	mi.unique_namespace.c_str(), mi.aux_filename.c_str(),
	src_mask, mi.src_mask);
    }
    return false;
  }
  return not_modified;
}

static std::string checksum_string(const checksum_type& c)
{
  std::string r;
  r += ulong_to_string_hexadecimal(c.timestamp);
  r += ":";
  r += to_hexadecimal(c.md5sum);
  return r;
}

static bool load_infofile(const parser_options& po, module_info& mi)
{
  const std::string cc_filename = get_cc_filename(po, mi);
  struct stat sbuf;
  if (stat(cc_filename.c_str(), &sbuf) != 0) {
    if (errno != ENOENT) {
      arena_error_throw(0, "%s:0: failed to stat: errno=%d\n",
	cc_filename.c_str(), errno);
    }
    return false;
  }
  const std::string info_filename = get_info_filename(po, mi);
  if (po.verbose > 1) {
    fprintf(stderr, "loading %s\n", info_filename.c_str());
  }
  const std::string content = read_file_content(info_filename, false);
  if (content.empty()) {
    DBG_INF(fprintf(stderr, "load_infofile: %s fmt1\n",
      info_filename.c_str()));
    return false;
  }
  strarrlist sa;
  parse_string_as_strarrlist(content, sa);
  strarrlist::const_iterator line = sa.begin();
  if (line == sa.end() || line->size() != 2) {
    DBG_INF(fprintf(stderr, "load_infofile: %s fmt2 %d\n",
      info_filename.c_str(), line == sa.end() ? 0 : (int)line->size()));
    return false;
  }
  /* moduleinfo: mi_timestamp mi_md5 */
  mi.info_checksum.timestamp = ulong_from_string_hexadecimal((*line)[0]);
  mi.info_checksum.md5sum = from_hexadecimal((*line)[1]);
  if (po.verbose > 1) {
    fprintf(stderr, "'%s': info_checksum: %s\n", info_filename.c_str(),
      checksum_string(mi.info_checksum).c_str());
  }
  ++line;
  while (line != sa.end()) {
    if (line->size() == 6 && (*line)[0] == "self") {
      /* self: self ns fn src_timestamp src_md5 src_mask */
      mi.unique_namespace = (*line)[1];
      mi.aux_filename = (*line)[2];
      mi.source_checksum.timestamp = ulong_from_string_hexadecimal((*line)[3]);
      mi.source_checksum.md5sum = from_hexadecimal((*line)[4]);
      mi.src_mask = ulong_from_string_hexadecimal((*line)[5]);
      if (po.verbose > 1) {
	fprintf(stderr, "'%s': source_checksum: %s\n", info_filename.c_str(),
	  checksum_string(mi.source_checksum).c_str());
      }
      if (!check_source_timestamp(po, mi, info_filename)) {
	DBG_INF(fprintf(stderr, "load_infofile: %s timestamp\n",
	  info_filename.c_str()));
	if (po.verbose > 1) {
	  fprintf(stderr, "'%s': source_checksum: modified\n",
	    info_filename.c_str());
	}
	return false;
      }
    } else if (line->size() == 2 && (*line)[0] == "link") {
      /* link: link name */
      const std::string m = (*line)[1];
      mi.self_copts.link.append_if(m);
      if (po.verbose > 1) {
	fprintf(stderr, "'%s': link %s\n", info_filename.c_str(),
	  m.c_str());
      }
    } else if (line->size() == 2 && (*line)[0] == "libdir") {
      /* libdir: libdir name */
      const std::string m = (*line)[1];
      mi.self_copts.libdir.append_if(m);
      if (po.verbose > 1) {
	fprintf(stderr, "'%s': libdir %s\n", info_filename.c_str(),
	  m.c_str());
      }
    } else if (line->size() == 2 && (*line)[0] == "ldflags") {
      /* ldflags: ldflags name */
      const std::string m = (*line)[1];
      mi.self_copts.ldflags.append_if(m);
      if (po.verbose > 1) {
	fprintf(stderr, "'%s': ldflags %s\n", info_filename.c_str(),
	  m.c_str());
      }
    } else if (line->size() == 5 && (*line)[0] == "import") {
      /* import: import ns pub src_timestamp src_md5 */
      const std::string ns = (*line)[1];
      unsigned long sort = ulong_from_string_hexadecimal((*line)[2]);
      const time_t timestamp = ulong_from_string_hexadecimal((*line)[3]);
      const std::string md5sum = from_hexadecimal((*line)[4]);
      checksum_type& cs = mi.depsrc_checksum[ns];
      cs.timestamp = timestamp;
      cs.md5sum = md5sum;
      mi.imports.deps.push_back(import_info());
      import_info& ii = mi.imports.deps.back();
      ii.ns = ns;
      ii.import_public = (sort == 1);
      if (po.verbose > 1) {
	fprintf(stderr, "'%s': import %s: %s %lu\n", info_filename.c_str(),
	  ns.c_str(), checksum_string(cs).c_str(), sort);
      }
    } else if (line->size() == 4 && (*line)[0] == "indirect") {
      /* indirect: indirect ns src_timestamp src_md5 */
      const std::string ns = (*line)[1];
      const time_t timestamp = ulong_from_string_hexadecimal((*line)[2]);
      const std::string md5sum = from_hexadecimal((*line)[3]);
      checksum_type& cs = mi.depsrc_checksum[ns];
      cs.timestamp = timestamp;
      cs.md5sum = md5sum;
      if (po.verbose > 1) {
	fprintf(stderr, "'%s': indirect %s: %s\n", info_filename.c_str(),
	  ns.c_str(), checksum_string(cs).c_str());
      }
    } else {
      if (po.verbose > 0) {
	std::string ln;
	for (std::vector<std::string>::const_iterator i = line->begin();
	  i != line->end(); ++i) {
	  if (!ln.empty()) {
	    ln += "\t";
	  }
	  ln += (*i);
	}
	fprintf(stderr, "'%s': unrecognized line: %s\n", info_filename.c_str(),
	  ln.c_str());
      }
    }
    ++line;
  }
  DBG_INF(fprintf(stderr, "load_infofile: %s done\n",
    info_filename.c_str()));
  return mi.source_checksum.timestamp != 0;
}

/* opens a module and examine its imports */
static void get_module_deps_from_source(module_info& mi)
{
  mi.imports.deps.clear();
  source_info si;
  si.minfo = &mi;
  si.mode = compile_mode_linkonly;
  arena_init(); /* TODO: RAII */
  pxc_parse_file(si, mi.imports);
  arena_error_throw_pushed();
  arena_clear();
  if (!mi.aux_filename.empty()) {
    mi.unique_namespace = mi.imports.main_unique_namespace;
  }
}

static void load_source_and_calc_checksum(const parser_options& po,
  module_info& mi)
{
  time_t tstamp = 0;
  md5_context md5;
  bool loaded = false;
  unsigned long long src_mask = 0;
  unsigned long long src_mask_bit = 1;
  module_info::source_files_type::iterator i;
  std::string notfound_fnlist;
  for (i = mi.source_files.begin(); i != mi.source_files.end();
    ++i, src_mask_bit <<= 1) {
    const std::string& fn = i->filename;
    if (po.verbose > 0) {
      fprintf(stderr, "loading source %s\n", fn.c_str());
    }
    while (true) {
      const time_t time_now = time(0);
      struct stat sbuf1, sbuf2;
      if (stat(fn.c_str(), &sbuf1) != 0) {
	if (errno == ENOENT) {
	  if (po.verbose > 0) {
	    fprintf(stderr, "loading source %s: not found\n", fn.c_str());
	  }
	  if (!notfound_fnlist.empty()) {
	    notfound_fnlist += ",";
	  }
	  notfound_fnlist += fn;
	  break;
	}
	arena_error_throw(0, "-:-: failed to stat '%s': errno=%d\n",
	  fn.c_str(), errno);
      }
      const std::string content = read_file_content(fn, true);
      if (po.verbose > 0 && !content.empty()) {
	fprintf(stderr, "loading source %s: found\n", fn.c_str());
      }
      if (stat(fn.c_str(), &sbuf2) != 0) {
	arena_error_throw(0, "-:-: failed to stat '%s': errno=%d\n",
	  fn.c_str(), errno);
      }
      if (sbuf1.st_mtime == sbuf2.st_mtime &&
	sbuf1.st_ctime == sbuf2.st_ctime) {
	const time_t tm1 = std::max(sbuf1.st_mtime, sbuf1.st_ctime);
	if (tm1 < time_now - PXC_TIMESTAMP_MARGIN) {
	  i->content = content;
	  tstamp = std::max(tstamp, tm1);
	  loaded = true;
	  break;
	}
      }
      /* reload the source file because it is possibly modified after we
       * stat()ed the file first. it is safe to set PSC_TIMESTAMP_MARGIN zero
       * if the system clock is never be skewed. */
      sleep(1);
    }
    md5.update(i->content);
    if (loaded) {
      src_mask |= src_mask_bit;
      break;
    }
  }
  if (!loaded) {
    arena_error_throw(0,
      "-:-: failed to load namespace '%s'\n"
      "-:-: (tried files: %s)\n",
      mi.unique_namespace.c_str(), notfound_fnlist.c_str());
  }
  mi.source_checksum.timestamp = tstamp;
  mi.source_checksum.md5sum = md5.final();
  mi.src_mask = src_mask;
  get_module_deps_from_source(mi);
  DBG_SUM(fprintf(stderr, "ns=%s ts=%d\n", mi.ns.c_str(), (int)tstamp));
}

static void load_source_content(const parser_options& po, module_info& mi)
{
  if (mi.source_loaded) {
    return;
  }
  bool source_modified = false;
  if (mi.source_checksum.timestamp == 0) {
    /* info is not loaded */
    if (!load_infofile(po, mi)) {
      DBG_MOD(fprintf(stderr, "load_source_content (%s,%s): no info file\n",
	mi.ns.c_str(), mi.aux_filename.c_str()));
      if (po.verbose > 1) {
	fprintf(stderr, "%s: info file not found => source modified\n",
	  mi.get_name().c_str());
      }
      source_modified = true;
    } else {
      DBG_MOD(fprintf(stderr, "load_source_content (%s,%s): found info file\n",
	mi.ns.c_str(), mi.aux_filename.c_str()));
    }
  }
  module_info mi_copy = mi;
  load_source_and_calc_checksum(po, mi_copy);
  if (mi.source_checksum != mi_copy.source_checksum) {
    source_modified = true;
    if (po.verbose > 1) {
      fprintf(stderr, "%s: loaded contet: source modified: info:%s disk:%s\n",
	mi.get_name().c_str(),
	checksum_string(mi.source_checksum).c_str(),
	checksum_string(mi_copy.source_checksum).c_str());
    }
  }
  mi = mi_copy;
  mi.source_modified = source_modified;
  DBG_MOD(fprintf(stderr, "load_source_content (%s,%s): modified=%d\n",
    mi.ns.c_str(), mi.aux_filename.c_str(), (int)source_modified));
  mi.source_loaded = true;
}

/* opens namespace 'ns' or file 'aux_fn' and returns its content */
static void load_source_info(const parser_options& po, module_info& mi)
{
  if (mi.source_checksum.timestamp == 0) {
    if (!load_infofile(po, mi)) {
      DBG_MOD(fprintf(stderr, "load_source_info (%s,%s): no info file\n",
	mi.ns.c_str(), mi.aux_filename.c_str()));
      load_source_content(po, mi);
      mi.source_modified = true;
    } else {
      DBG_MOD(fprintf(stderr, "load_source_info (%s,%s): found info file\n",
	mi.ns.c_str(), mi.aux_filename.c_str()));
    }
  }
}

/* opens namespace 'ns' or file 'aux_fn' and examine its imports recursively */
static void get_module_info_rec(const parser_options& po,
  const std::string& ns, const std::string& aux_fn, all_modules_info& ami,
  strset& ancestors, strset& checked)
  /* ns is null aux_fn is nonemopty when it's the main module */
{
  const std::string modname = aux_fn.empty() ? ns : aux_fn;
  if (ancestors.find(modname) != ancestors.end()) {
    arena_error_throw(0, "-:0: a module dependency cycle is found: '%s'",
      modname.c_str()); /* TODO: message */
  }
  if (checked.find(modname) != checked.end()) {
    return;
  }
  if (ami.modules.find(modname) == ami.modules.end()) {
    /* create a new module_info */
    module_info& mi = ami.modules[modname];
    mi.unique_namespace = ns;
    mi.aux_filename = aux_fn;
    mi.source_files.clear();
    if (!aux_fn.empty()) {
      mi.source_files.push_back(source_file_info());
      source_file_info& si = mi.source_files.back();
      si.filename = aux_fn;
    } else {
      const std::string ns_filename = get_filename_for_ns(ns);
      for (strlist::const_iterator i = po.profile.incdir.begin();
	i != po.profile.incdir.end(); ++i) {
	std::string s = *i;
	if (s.size() == 0) {
	  continue;
	}
	if (s[s.size() - 1] != '/') {
	  s += "/";
	}
	s += ns_filename;
	mi.source_files.push_back(source_file_info());
	source_file_info& si = mi.source_files.back();
	si.filename = s;
      }
    }
  }
  module_info& mi = ami.modules[modname];
  load_source_info(po, mi);
  ancestors.insert(modname);
  checked.insert(modname);
  for (imports_type::deps_type::const_iterator i = mi.imports.deps.begin();
    i != mi.imports.deps.end(); ++i) {
    get_module_info_rec(po, i->ns, "", ami, ancestors, checked);
  }
  ancestors.erase(modname);
}

static module_info& get_modinfo_by_name(all_modules_info& ami,
  const std::string& modname)
{
  all_modules_info::modules_type::iterator i = ami.modules.find(modname);
  assert(i != ami.modules.end());
  return i->second;
}

static void prepare_workdir(const parser_options& po, all_modules_info& ami,
  const module_info& mi, const strset& link_srcs)
{
  strset dirs;
  for (strset::const_iterator i = link_srcs.begin(); i != link_srcs.end();
    ++i) {
    module_info& mi = get_modinfo_by_name(ami, *i);
    const std::string d = get_work_filename(po, mi, true);
    dirs.insert(d);
  }
  for (strset::const_iterator j = dirs.begin(); j != dirs.end(); ++j) {
    mkdir_hier(*j);
  }
}

static void get_indirect_imports(const all_modules_info& ami,
  const std::string& modname, bool follow_private, bool is_main,
  strset& cc_srcs_r, strlist& cc_srcs_ord_r)
{
  if (cc_srcs_r.find(modname) != cc_srcs_r.end()) {
    return;
  }
  assert(ami.modules.find(modname) != ami.modules.end());
  const module_info& md = ami.modules.find(modname)->second;
  for (imports_type::deps_type::const_iterator i = md.imports.deps.begin();
    i != md.imports.deps.end(); ++i) {
    if (is_main || i->import_public || follow_private) {
      get_indirect_imports(ami, i->ns, follow_private, false, cc_srcs_r,
	cc_srcs_ord_r);
    }
    DBG(fprintf(stderr, "%s imports %s pub=%d\n", modname.c_str(),
      i->ns.c_str(), (int)i->import_public));
  }
  cc_srcs_r.insert(modname);
  cc_srcs_ord_r.push_back(modname);
}

static void generate_info_file(const parser_options& po,
  const all_modules_info& ami, const module_info& mi, const std::string& ifn)
{
  std::string str;
  time_t info_ts = 0;
  /* self: ns fn src_timestamp src_md5 src_mask */
  str += "self\t" + mi.unique_namespace + "\t" + mi.aux_filename + "\t" +
    ulong_to_string_hexadecimal(mi.source_checksum.timestamp) + "\t" +
    to_hexadecimal(mi.source_checksum.md5sum) + "\t" +
    ulong_to_string_hexadecimal(mi.src_mask) + "\n";
  info_ts = std::max(mi.source_checksum.timestamp, info_ts);
  /* coptions link */
  for (coptions::optvalues::list_val_type::const_iterator i =
    mi.self_copts.link.list_val.begin();
    i != mi.self_copts.link.list_val.end(); ++i) {
    str += "link\t" + (*i) + "\n";
  }
  /* coptions libdir */
  for (coptions::optvalues::list_val_type::const_iterator i =
    mi.self_copts.libdir.list_val.begin();
    i != mi.self_copts.libdir.list_val.end(); ++i) {
    str += "libdir\t" + (*i) + "\n";
  }
  /* coptions ldflags */
  for (coptions::optvalues::list_val_type::const_iterator i =
    mi.self_copts.ldflags.list_val.begin();
    i != mi.self_copts.ldflags.list_val.end(); ++i) {
    str += "ldflags\t" + (*i) + "\n";
  }
  /* direct imports */
  strset dimps;
  for (imports_type::deps_type::const_iterator i = mi.imports.deps.begin();
    i != mi.imports.deps.end(); ++i) {
    all_modules_info::modules_type::const_iterator j = ami.modules.find(i->ns);
    assert(j != ami.modules.end());
    /* import: ns pub src_timestamp src_md5 */
    str += "import\t" + i->ns + "\t";
    str += i->import_public ? "1\t" : "0\t";
    str += ulong_to_string_hexadecimal(j->second.source_checksum.timestamp)
      + "\t";
    str += to_hexadecimal(j->second.source_checksum.md5sum) + "\n";
    info_ts = std::max(j->second.source_checksum.timestamp, info_ts);
    dimps.insert(i->ns);
  }
  /* non-direct imports */
  for (strset::const_iterator i = mi.cc_srcs.begin(); i != mi.cc_srcs.end();
    ++i) {
    if (dimps.find(*i) != dimps.end()) {
      continue; /* direct import */
    }
    if (i->empty()) {
      continue; /* file */
    }
    all_modules_info::modules_type::const_iterator j = ami.modules.find(*i);
    assert(j != ami.modules.end());
    /* dep: ns 2 src_timestamp src_md5 */
    str += "indirect\t" + (*i) + "\t";
    str += ulong_to_string_hexadecimal(j->second.source_checksum.timestamp)
      + "\t";
    str += to_hexadecimal(j->second.source_checksum.md5sum) + "\n";
    info_ts = std::max(j->second.source_checksum.timestamp, info_ts);
  }
  /* cal info checksum */
  md5_context info_md5;
  info_md5.update(str);
  /* moduleinfo: mi_timestamp mi_md5 */
  std::string info_sum = ulong_to_string_hexadecimal(info_ts) + "\t" +
    to_hexadecimal(info_md5.final()) + "\n";
  write_file_content(ifn, info_sum + str);
}

static std::string
make_cxx_opts(const coptions::optvalues& ovs, const std::string& p)
{
  std::string r;
  std::list<std::string>::const_iterator i;
  for (i = ovs.list_val.begin(); i != ovs.list_val.end(); ++i) {
    r += " ";
    r += p;
    r += *i;
  }
  return r;
}

static void get_coptions(const parser_options& po, all_modules_info& ami,
  const module_info& mi_main, coptions& copt_r)
{
  for (strset::const_iterator i = mi_main.link_srcs.begin();
    i != mi_main.link_srcs.end(); ++i) {
    module_info& mi_c = get_modinfo_by_name(ami, *i);
    copt_r.append(mi_c.self_copts);
    DBG_LINK(fprintf(stderr, "link %s %d\n", i->c_str(),
      mi_c.self_copts.link.list_val.size()));
    DBG_LINK(fprintf(stderr, "copt_r %d\n", copt_r.link.list_val.size()));
  }
}

/* compiles cc files and generate a so file */
static void compile_cxx_all(const parser_options& po,
  all_modules_info& ami, module_info& mi_main)
{
  coptions copt;
  get_coptions(po, ami, mi_main, copt);
  const std::string libdir_str = make_cxx_opts(copt.libdir, "-L");
  const std::string link_str = make_cxx_opts(copt.link, "-l");
  const std::string ldflags_str = make_cxx_opts(copt.ldflags, "");
  DBG_LINK(fprintf(stderr, "copt.link sz=%d\n", copt.link.list_val.size()));
  const std::string ofn = po.profile.generate_dynamic ?
    get_so_filename(po, mi_main) : get_exe_filename(po, mi_main);
  std::string genopt;
  if (!po.profile.generate_dynamic) {
    genopt = "-lpthread";
  } else {
    #ifdef __APPLE__
    genopt = "-fPIC -undefined dynamic_lookup -bundle -bundle_loader "
      + po.argv0 + " -lpthread";
    #else
    genopt = "-fPIC -shared -lpthread";
    #endif
  }
  /*
  #ifdef __APPLE__
  std::string cmd = po.profile.cxx + " " + po.profile.cflags
    + " -g -Wall -fPIC -undefined dynamic_lookup -bundle -bundle_loader "
    + po.argv0 + " -o '" + ofn + "' -lpthread";
  #else
  std::string cmd = po.profile.cxx + " " + po.profile.cflags
    + " -g -Wall -fPIC -shared -o '" + ofn + "' -lpthread";
  #endif
  */
  std::string cmd = po.profile.cxx + " " + po.profile.cflags + " " + genopt
    + " -o '" + ofn + "'";
  for (all_modules_info::modules_type::const_iterator i = ami.modules.begin();
    i != ami.modules.end(); ++i) {
    const std::string fn = get_o_filename(po, i->second);
    cmd += " '" + fn + "'";
  }
  cmd += ldflags_str + libdir_str + link_str + " " + po.profile.ldflags;
  int r = 0;
  std::string obuf;
  r = popen_cmd(cmd + " 2>&1", obuf);
  if (r != 0) {
    arena_error_throw(0, "%s\n%s", cmd.c_str(), obuf.c_str());
  }
  if (po.verbose > 0) {
    fprintf(stderr, "%s\n%s", cmd.c_str(), obuf.c_str());
  }
}

static void compile_module_to_cc_srcs(const parser_options& po,
  all_modules_info& ami, module_info& mi_main, generate_main_e gmain)
{
  prepare_workdir(po, ami, mi_main, mi_main.link_srcs);
  arena_init(); /* TODO: RAII */
  for (strlist::const_iterator i = mi_main.cc_srcs_ord.begin();
    i != mi_main.cc_srcs_ord.end(); ++i) {
    imports_type cursrc_imports;
    module_info& mi = get_modinfo_by_name(ami, *i);
    source_info si;
    si.minfo = &mi;
    si.mode = &mi == &mi_main ? compile_mode_main : compile_mode_import;
    pxc_parse_file(si, cursrc_imports);
    if (mi.unique_namespace != cursrc_imports.main_unique_namespace) {
      arena_error_throw(0,
	"-:0: invalid namespace declaration '%s' for expected namespace '%s'",
	cursrc_imports.main_unique_namespace.c_str(),
	mi.unique_namespace.c_str());
    }
    #if 0
    fprintf(stderr, "src=%s mins=%s srcns=%s\n", i->c_str(),
      mi.unique_namespace.c_str(),
      cursrc_imports.main_unique_namespace.c_str());
    #endif
  }
  arena_error_throw_pushed();
  /* remove old files */
  {
    unlink_if(get_cc_filename(po, mi_main));
    unlink_if(get_o_filename(po, mi_main));
    unlink_if(get_so_filename(po, mi_main));
    unlink_if(get_exe_filename(po, mi_main));
    unlink_if(get_info_filename(po, mi_main));
  }
  /* compile to cc */
  {
    const std::string cc_filename = get_cc_filename(po, mi_main);
    const std::string tmp_fn = cc_filename + ".tmp";
    tmpfile_guard g(tmp_fn);
    mi_main.self_copts = coptions();
    arena_compile(tmp_fn, mi_main.self_copts, gmain);
    if (rename(tmp_fn.c_str(), cc_filename.c_str()) != 0) {
      arena_error_throw(0, "-:0: rename('%s', '%s') failed: errno=%d",
	tmp_fn.c_str(), cc_filename.c_str(), errno);
    }
  }
  /* compile to o */
  {
    const std::string cfn = get_cc_filename(po, mi_main);
    const std::string ofn = get_o_filename(po, mi_main);
    const std::string cflags_str = make_cxx_opts(
      mi_main.self_copts.cflags, "");
    const std::string incdir_str = make_cxx_opts(
      mi_main.self_copts.incdir, "-I");
    const std::string cmd = po.profile.cxx + " " + po.profile.cflags
      + " -g -Wall -fPIC"
      + cflags_str + incdir_str + " -I. -c '" + cfn + "' -o '" + ofn + "'";
    std::string obuf;
    int r = popen_cmd(cmd + " 2>&1", obuf);
    if (r != 0) {
      arena_error_throw(0, "%s\n%s", cmd.c_str(), obuf.c_str());
    }
    if (po.verbose > 0) {
      fprintf(stderr, "%s\n%s", cmd.c_str(), obuf.c_str());
    }
  }
  /* compile to so or exe */
  if (!mi_main.aux_filename.empty()) {
    compile_cxx_all(po, ami, mi_main);
  }
  /* generate info */
  {
    const std::string info_filename = get_info_filename(po, mi_main);
    const std::string tmp_fn = info_filename + ".tmp";
    tmpfile_guard g(tmp_fn);
    generate_info_file(po, ami, mi_main, tmp_fn);
    if (rename(tmp_fn.c_str(), info_filename.c_str()) != 0) {
      arena_error_throw(0, "-:0: rename('%s', '%s') failed: errno=%d",
	tmp_fn.c_str(), info_filename.c_str(), errno);
    }
  }
  arena_clear();
}

/* compiles px files and generate one cxx file */
static void compile_module_to_cc(const parser_options& po,
  all_modules_info& ami, const std::string& modname, generate_main_e gmain)
{
  strset cc_srcs;
  strlist cc_srcs_ord;
  get_indirect_imports(ami, modname, false, true, cc_srcs, cc_srcs_ord);
  strset link_srcs;
  strlist link_srcs_ord;
  get_indirect_imports(ami, modname, true, true, link_srcs, link_srcs_ord);
  module_info& mi_main = get_modinfo_by_name(ami, modname);
  DBG(fprintf(stderr, "COMPILE %s\n", fn.c_str()));
  mi_main.cc_srcs = cc_srcs;
  mi_main.cc_srcs_ord = cc_srcs_ord;
  mi_main.link_srcs = link_srcs;
  mi_main.link_srcs_ord = link_srcs_ord;
  compile_module_to_cc_srcs(po, ami, mi_main, gmain);
}

/* compiles namespace 'ns' or file 'fn' and generate cc files recursively */
static bool compile_modules_rec(const parser_options& po,
  all_modules_info& ami, const std::string& modname, generate_main_e gmain)
{
  module_info& md = get_modinfo_by_name(ami, modname);
  bool modified = false;
  try {
    /* build imports first */
    for (imports_type::deps_type::const_iterator i = md.imports.deps.begin();
      i != md.imports.deps.end(); ++i) {
      const import_info& ii = *i;
      modified |= compile_modules_rec(po, ami, ii.ns, generate_main_none);
    }
    if (md.need_rebuild || modified) {
      /* TODO: don't rebuild cc if !md.need_rebuild */
      compile_module_to_cc(po, ami, modname, gmain);
      modified = true;	    
      md.need_rebuild = false;
    }
  } catch (const std::exception& e) {
    std::string s = e.what();
    s = "-:0: (while compiling '" + modname + "')\n" + s;
    throw std::runtime_error(s);
  }
  return modified;
}

static int execute_mod(const auto_dl& dl, const std::string& symbol_name)
{
  if (dl.get() == 0) {
    fprintf(stderr, "%s\n", dlerror());
    return 1;
  }
  void *const sym = dlsym(dl.get(), symbol_name.c_str());
  if (sym == 0) {
    fprintf(stderr, "%s\n", dlerror());
    return 1;
  }
  int (*f)(void) = (int (*)(void))sym;
  int r = f();
  return r;
}

static bool check_need_rebuild(const parser_options& po,
  all_modules_info& ami, module_info& mi)
{
  if (mi.need_rebuild) {
    return false;
  }
  /* source modified? */
  for (strset::const_iterator i = mi.cc_srcs.begin(); i != mi.cc_srcs.end();
    ++i) {
    module_info& mi_imp = ami.modules[*i];
    if (mi_imp.source_modified) {
      DBG_CHK(fprintf(stderr, "need_rebuild=%s import=%s\n", mi.ns.c_str(),
	mi_imp.ns.c_str()));
      if (po.verbose > 0) {
	fprintf(stderr, "%s: source modified => %s: need rebuild\n",
	  mi_imp.get_name().c_str(), mi.get_name().c_str());
      }
      mi.need_rebuild = true;
      break;
    }
  }
  /* checsums are consistent? */
  for (depsrc_checksum_type::const_iterator i = mi.depsrc_checksum.begin();
    i != mi.depsrc_checksum.end(); ++i) {
    module_info& mi_dep = ami.modules[i->first];
    if (mi_dep.source_checksum != i->second) {
      DBG_CHK(fprintf(stderr, "need_rebuild=%s dep=%s\n", mi.ns.c_str(),
	mi_dep.ns.c_str()));
      if (po.verbose > 0) {
	fprintf(stderr, "checksum mismatch %s:%s %s:%s\n",
	  mi_dep.get_name().c_str(),
	  checksum_string(mi_dep.source_checksum).c_str(),
	  i->first.c_str(),
	  checksum_string(i->second).c_str());
      }
      mi.need_rebuild = true;
      break;
    }
  }
  bool loaded = false;
  if (mi.need_rebuild) {
    if (!mi.source_loaded) {
      load_source_content(po, mi);
      loaded = true;
    }
    for (strset::const_iterator i = mi.cc_srcs.begin(); i != mi.cc_srcs.end();
      ++i) {
      module_info& mi_dep = ami.modules[*i];
      if (!mi_dep.source_loaded) {
	load_source_content(po, mi_dep);
	loaded = true;
      }
    }
  }
  return loaded;
}

struct filelock_lockobj {
  filelock_lockobj(const std::string& fn)
    : fn(fn)
  {
    lock();
  }
  ~filelock_lockobj() {
    /* unlocked because it's closed */
  }
  void lock() {
    if (afp.get() == 0) {
      auto_fp fp(fopen(fn.c_str(), "a"));
      if (fp.get() == 0) {
	arena_error_throw(0, "-:0: failed to open '%s': errno=%d\n",
	  fn.c_str(), errno);
      }
      if (flock(fileno(fp.get()), LOCK_EX) != 0) {
	arena_error_throw(0, "-:0: failed to lock '%s': errno=%d\n",
	  fn.c_str(), errno);
      }
      afp = fp;
    }
  }
  void unlock() {
    if (afp.get() != 0) {
      auto_fp fp;
      afp = fp;
    }
  }
private:
  const std::string fn;
  auto_fp afp;
};

static void load_profile(parser_options& po)
{
  if (po.verbose > 0) {
    fprintf(stderr, "loading profile %s\n", po.profile_name.c_str());
  }
  const std::string pstr = read_file_content(po.profile_name, false);
  md5_context md5;
  md5.update(pstr);
  po.profile.md5sum = to_hexadecimal(md5.final());
  strlist lines;
  split_string(pstr, '\n', lines);
  for (strlist::const_iterator i = lines.begin(); i != lines.end(); ++i) {
    const std::string& s = *i;
    const size_t eq = s.find('=');
    if (eq == std::string::npos) {
      po.profile.mapval[s] = "";
      continue;
    }
    const std::string k = trim_space(s.substr(0, eq));
    const std::string v = trim_space(s.substr(eq + 1));
    po.profile.mapval[k] = v;
    if (po.verbose > 1) {
      fprintf(stderr, "profile: %s = %s\n", k.c_str(), v.c_str());
    }
  }
  strmap::const_iterator iter;
  iter = po.profile.mapval.find("generate_dynamic");
  if (iter != po.profile.mapval.end() && atoi(iter->second.c_str()) != 0) {
    po.profile.generate_dynamic = true;
  }
  iter = po.profile.mapval.find("incdir");
  const std::string incstr = iter != po.profile.mapval.end()
    ? iter->second : "";
  split_string(incstr, ':', po.profile.incdir);
  if (po.profile.incdir.empty()) {
    po.profile.incdir.push_back("/usr/share/pxc/");
    po.profile.incdir.push_back("/usr/local/share/pxc/");
    po.profile.incdir.push_back(".");
  }
  iter = po.profile.mapval.find("cxx");
  po.profile.cxx = iter != po.profile.mapval.end()
    ? iter->second : "g++";
  iter = po.profile.mapval.find("cflags");
  po.profile.cflags = iter != po.profile.mapval.end()
    ? iter->second : "-g -O3 -DNDEBUG";
  iter = po.profile.mapval.find("ldflags");
  po.profile.ldflags = iter != po.profile.mapval.end()
    ? iter->second : "";
}

static void check_profile_md5sum(const parser_options& po)
{
  const std::string chksum_file = po.work_dir + "/chksum";
  const std::string old_md5sum = read_file_content(chksum_file, false);
  if (old_md5sum != po.profile.md5sum) {
    unlink_recursive(po.work_dir + "/fn");
    unlink_recursive(po.work_dir + "/ns");
    unlink_if(chksum_file);
  }
  write_file_content(chksum_file, po.profile.md5sum);
}

static int compile_and_execute(parser_options& po,
  const std::string& fn)
{
  mkdir_hier(po.work_dir);
  filelock_lockobj lock(po.work_dir + "/lock");
  load_profile(po);
  check_profile_md5sum(po);
  all_modules_info ami;
  while (true) {
    /* loop until loaded modules are consistent and up-to-date */
    strset ancestors, checked;
    get_module_info_rec(po, "", fn, ami, ancestors, checked);
    bool loaded = false;
    all_modules_info::modules_type::iterator i;
    for (i = ami.modules.begin(); i != ami.modules.end(); ++i) {
      module_info& mi = i->second;
      mi.cc_srcs.clear();
      mi.cc_srcs_ord.clear();
      get_indirect_imports(ami,
	!mi.aux_filename.empty() ? mi.aux_filename : mi.unique_namespace,
	false, true, mi.cc_srcs, mi.cc_srcs_ord);
      loaded |= check_need_rebuild(po, ami, mi);
    }
    if (!loaded) {
      break;
    }
  }
  module_info& mi_main = get_modinfo_by_name(ami, fn);
  std::string exe_fn = po.profile.generate_dynamic ?
    get_so_filename(po, mi_main) : get_exe_filename(po, mi_main);
  if (exe_fn.size() > 0 && exe_fn[0] != '/') {
    exe_fn = "./" + exe_fn;
  }
  if (!po.no_build && (!po.no_update || !file_exist(exe_fn))) {
    compile_modules_rec(po, ami, fn,
      po.profile.generate_dynamic ? generate_main_dl : generate_main_exe);
  }
  if (po.no_execute) {
    return 0;
  }
  const std::string sn = arena_get_ns_main_funcname(mi_main.unique_namespace)
    + "$cm";
  if (po.show_out) {
    fprintf(stdout, "%s\t%s\n", exe_fn.c_str(), sn.c_str());
    fprintf(stdout, "(close stdin to exit)\n");
    fflush(stdout);
    /* wait until stdin is closed */
    char c = 0;
    while ((c = fgetc(stdin)) != EOF) { }
    /* lock is released here */
    return 0;
  } else if (!po.profile.generate_dynamic) {
    char *e_argv[2];
    e_argv[0] = strdup(exe_fn.c_str());
    e_argv[1] = 0;
    return execv(e_argv[0], e_argv);
  } else {
    auto_dl dl(dlopen(exe_fn.c_str(), RTLD_NOW | RTLD_GLOBAL));
    lock.unlock();
    return execute_mod(dl, sn);
  }
}

static bool check_path_validity(const std::string& path)
{
  const size_t len = path.size();
  for (size_t i = 0; i < len; ++i) {
    unsigned char c = path[i];
    if (c < 0x20 || c == 0x7f) {
      return false;
    }
    switch (c) {
    case '\\':
    case '"':
    case '`':
    case '\'':
    case '*':
    case '|':
    case '!':
    case '?':
    case '~':
    case '$':
    case '<':
    case '>':
    case '&':
    case '[':
    case ']':
    case '#':
      return false;
    default:
      break;
    }
  }
  return true;
}

static int prepare_options(parser_options& po, int argc, char **argv)
{
  if (argc >= 1) {
    po.argv0 = argv[0];
    struct stat sbuf;
    if (stat(argv[0], &sbuf) == 0) {
      po.argv0_ctime = sbuf.st_ctime;
    }
  } else {
    po.argv0 = "pxc";
  }
  int i = 1;
  for (; i < argc; ++i) {
    const std::string s = argv[i];
    if (s.empty()) {
      continue;
    }
    if (s[0] == '-') {
      if (s == "--clean" || s == "-c") {
	po.clean_flag = true;
      } else if ((s == "--verbose" || s == "-v") && i + 1 < argc) {
	++i;
	po.verbose = atoi(argv[i]);
      } else if ((s == "--profile" || s == "-p") && i + 1 < argc) {
	++i;
	po.profile_name = argv[i];
      } else if ((s == "--work-dir" || s == "-w") && i + 1 < argc) {
	++i;
	po.work_dir = argv[i];
      } else if (s == "--no-build") {
	po.no_build = true;
      } else if (s == "--no-update") {
	po.no_update = true;
      } else if (s == "--no-execute") {
	po.no_execute = true;
      } else if (s == "--show-out") {
	po.show_out = true;
      } else if (s == "--no-realpath") {
	po.no_realpath = true;
      } else {
	fprintf(stderr, "unknown option '%s'\n", s.c_str());
	return 1;
      }
    } else {
      po.src_filename = s;
      break;
    }
  }
  {
    char buf[64];
    int c = 0;
    for (++i; i < argc; ++i) {
      snprintf(buf, sizeof(buf), "PXC_ARG%d", c++);
      const std::string s = argv[i];
      setenv(buf, s.c_str(), 1);
    }
    snprintf(buf, sizeof(buf), "PXC_ARG%d", c++);
    unsetenv(buf);
  }
  if (po.work_dir.empty()) {
    po.work_dir = get_home_directory() + "/.pxc";
  }
  if (po.profile_name.empty()) {
    po.profile_name = "/etc/pxc.profile";
  } else if (po.profile_name.find('/') == po.profile_name.npos) {
    po.profile_name = "/etc/" + po.profile_name + ".profile";
  }
  if (!check_path_validity(po.work_dir)) {
    fprintf(stderr, "working directory contains an invalid character: '%s'\n",
      po.work_dir.c_str());
    return 1;
  }
  if (!check_path_validity(po.work_dir)) {
    fprintf(stderr, "filename contains an invalid character: '%s'\n",
      po.src_filename.c_str());
    return 1;
  }
  po.work_dir += "/" + escape_hex_non_alnum(po.profile_name);
  return 0;
}

static int cdriver_main(int argc, char **argv)
{
  try {
    parser_options po;
    int r = prepare_options(po, argc, argv);
    if (r != 0) {
      return r;
    }
    if (!po.src_filename.empty()) {
      std::string fn = po.src_filename;
      if (!po.no_realpath) {
	fn = get_canonical_path(fn);
      }
      return compile_and_execute(po, fn);
    }
    return 0;
  } catch (const std::exception& e) {
    std::string mess(e.what());
    if (!mess.empty() && mess[mess.size() - 1] != '\n') {
      mess += "\n";
    }
    fprintf(stderr, "%s", mess.c_str());
  }
  return 1;
}

};

int main(int argc, char **argv)
{
  return pxc::cdriver_main(argc, argv);
}

