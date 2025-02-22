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
#include <time.h>
#include <set>
#include <list>
#include <deque>
#include "expr.hpp"
#include "expr_misc.hpp"
#include "util.hpp"
#include "parser.hpp"
#include "pxc_md5.hpp"

#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <assert.h>
#include <stdexcept>

#define DBG(x)
#define DBG_SRC(x)
#define DBG_SRCLIST(x)
#define DBG_SUM(x)
#define DBG_MOD(x)
#define DBG_INF(x)
#define DBG_CHK(x)
#define DBG_LINK(x)
#define DBG_READ(x)

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
  std::string emit_threaded_dll;
  bool gcc_compat;
  bool generate_dynamic; /* TODO */
  bool show_warnings;
  bool error_detail;
  size_t recursion_limit;
  size_t safe_mode;
  profile_settings() : gcc_compat(true), generate_dynamic(false),
    show_warnings(false), error_detail(false), recursion_limit(0),
    safe_mode(1) { }
};

typedef std::list<source_info> sources_type;

struct parser_options {
  std::string profile_name;
  std::string src_filename;
  profile_settings profile;
  std::string work_dir;
  std::string gen_cc_dir;
  int verbose;
  bool clean_flag;
  bool clean_all_flag;
  bool no_build;
  bool no_update;
  bool no_execute;
  bool no_realpath;
  int trim_path;
  bool gen_out;
  bool gen_single_cc;
  std::string argv0;
  time_t argv0_ctime; // TODO: remove. unused.
  parser_options() : verbose(0), clean_flag(false), clean_all_flag(false),
    no_build(false), no_update(false), no_execute(false), no_realpath(false),
    trim_path(0), gen_out(false), gen_single_cc(false), argv0_ctime(0) { }
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
    fn = trim_path(mi.aux_filename, po.trim_path);
    fn = escape_hex_filename(fn);
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

static std::string replace_or_append_suffix(const std::string& fn,
  const std::string& suf)
{
  std::string rfn = fn;
  size_t const sl = rfn.rfind('/');
  size_t const dot = rfn.rfind('.');
  if (dot != rfn.npos && (sl == rfn.npos || sl < dot)) {
    rfn = rfn.substr(0, dot);
  }
  return rfn + suf;
}

static std::string get_filename_with_suf(const parser_options& po, const module_info& mi, const std::string& suf)
{
  return replace_or_append_suffix(get_work_filename(po, mi, false), suf);
}

static std::string get_cc_filename(const parser_options& po,
  const module_info& mi)
{
  return get_filename_with_suf(po, mi, ".cc");
}

static std::string get_o_filename(const parser_options& po,
  const module_info& mi)
{
  return get_filename_with_suf(po, mi, ".o");
}

static std::string get_so_filename(const parser_options& po,
  const module_info& mi)
{
  return get_filename_with_suf(po, mi, ".so");
}

static std::string get_exe_filename(const parser_options& po,
  const module_info& mi)
{
  return get_filename_with_suf(po, mi, ".exe");
}

static std::string get_info_filename(const parser_options& po,
  const module_info& mi)
{
  return get_filename_with_suf(po, mi, ".inf");
}

static size_t count_newline(std::string const& s)
{
  size_t n = 0;
  for (char ch: s) {
    if (ch == '\n') {
      ++n;
    }
  }
  return n;
}

static source_file_content_ptr read_source_file_content(const std::string& fn,
  int trim)
{
  source_file_content r;
  const std::string fnh = fn + "h";
  if (file_access(fnh)) {
    DBG_READ(fprintf(stderr, "read_file_content 1 %s\n", fnh.c_str()));
    std::string s = read_file_content(fnh, true) + "\n";
    r.fragment_sizes.push_back(std::make_pair(trim_path(fnh, trim),
      count_newline(s)));
    r.content += s;
  }
  if (file_access(fn)) {
    DBG_READ(fprintf(stderr, "read_file_content 2 %s\n", fn.c_str()));
    std::string s = read_file_content(fn, true) + "\n";
    r.fragment_sizes.push_back(std::make_pair(trim_path(fn, trim),
      count_newline(s)));
    r.content += s;
  }
  return std::make_shared<source_file_content>(std::move(r));
}

struct source_statbuf {
  struct stat sb_header = { };
  struct stat sb_body = { };
  unsigned hdrbdy_mask = 0;
  bool time_eq(source_statbuf const& x) const {
    return
      sb_header.st_mtime == x.sb_header.st_mtime &&
      sb_header.st_ctime == x.sb_header.st_ctime &&
      sb_body.st_mtime == x.sb_body.st_mtime &&
      sb_body.st_ctime == x.sb_body.st_ctime &&
      hdrbdy_mask == x.hdrbdy_mask;
  }
  time_t time_max() const {
    return std::max(
      std::max(sb_header.st_mtime, sb_header.st_ctime),
      std::max(sb_body.st_mtime, sb_body.st_ctime));
  }
};

static int stat_source_file(const char *fn, source_statbuf& sbuf,
  bool stat_header = true)
{
  sbuf = { };
  int e = 0;
  if (stat_header) {
    const std::string fnh = std::string(fn) + "h";
    const int r0 = stat(fnh.c_str(), &sbuf.sb_header);
    if (r0 != 0) {
      sbuf.sb_header = { };
      if (errno != ENOENT) {
        e = errno;
      }
    } else {
      sbuf.hdrbdy_mask |= 1;
    }
  }
  {
    const int r1 = stat(fn, &sbuf.sb_body);
    if (r1 != 0) {
      sbuf.sb_body = { };
      if (errno != ENOENT) {
        e = errno;
      }
    } else {
      sbuf.hdrbdy_mask |= 2;
    }
  }
  return e;
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
      source_statbuf sbuf;
      int e = stat_source_file(fn.c_str(), sbuf);
      if (e != 0) {
        arena_error_throw(0, "%s:0: Failed to stat: errno=%d\n",
          fn.c_str(), e);
      }
      if (sbuf.hdrbdy_mask == 0) {
        break;
      }
      if (sbuf.time_max() != mi.source_checksum.timestamp ||
        sbuf.hdrbdy_mask != mi.source_checksum.hdrbdy_mask) {
        if (po.verbose > 1) {
          fprintf(stderr,
            "check_source_timestamp %s: mtime=%lu:%u checksum_mtime=%lu:%u\n",
            fn.c_str(), (unsigned long)sbuf.time_max(), sbuf.hdrbdy_mask,
            (unsigned long)mi.source_checksum.timestamp,
            mi.source_checksum.hdrbdy_mask);
        }
        not_modified = false;
      }
      src_mask |= src_mask_bit;
      break;
    }
    if (src_mask != 0) {
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
  r += ulong_to_string_hexadecimal(c.hdrbdy_mask);
  r += ":";
  r += to_hexadecimal(c.md5sum);
  return r;
}

static bool load_infofile(const parser_options& po, module_info& mi)
{
  if (!po.gen_cc_dir.empty()) {
    return false;
  }
  const std::string cc_filename = get_cc_filename(po, mi);
  struct stat sbuf = { };
  if (stat(cc_filename.c_str(), &sbuf) != 0) {
    if (errno != ENOENT) {
      arena_error_throw(0, "%s:0: Failed to stat: errno=%d\n",
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
    if (line->size() == 7 && (*line)[0] == "self") {
      /* self: self ns fn src_timestamp src_hdrbdy src_md5 src_mask */
      mi.unique_namespace = (*line)[1];
      mi.aux_filename = (*line)[2];
      mi.source_checksum.timestamp = ulong_from_string_hexadecimal((*line)[3]);
      mi.source_checksum.hdrbdy_mask =
        ulong_from_string_hexadecimal((*line)[4]);
      mi.source_checksum.md5sum = from_hexadecimal((*line)[5]);
      mi.src_mask = ulong_from_string_hexadecimal((*line)[6]);
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
    } else if (line->size() == 6 && (*line)[0] == "import") {
      /* import: import ns pub src_timestamp src_hdrbdy src_md5 */
      const std::string ns = (*line)[1];
      unsigned long sort = ulong_from_string_hexadecimal((*line)[2]);
      const time_t timestamp = ulong_from_string_hexadecimal((*line)[3]);
      const unsigned hdrbdy_mask = ulong_from_string_hexadecimal((*line)[4]);
      const std::string md5sum = from_hexadecimal((*line)[5]);
      checksum_type& cs = mi.depsrc_checksum[ns];
      cs.timestamp = timestamp;
      cs.hdrbdy_mask = hdrbdy_mask;
      cs.md5sum = md5sum;
      mi.imports.deps.push_back(import_info());
      import_info& ii = mi.imports.deps.back();
      ii.ns = ns;
      ii.import_public = (sort == 1);
      if (po.verbose > 1) {
        fprintf(stderr, "'%s': import %s: %s %lu\n", info_filename.c_str(),
          ns.c_str(), checksum_string(cs).c_str(), sort);
      }
    } else if (line->size() == 5 && (*line)[0] == "indirect") {
      /* indirect: indirect ns src_timestamp src_hdrbdy src_md5 */
      const std::string ns = (*line)[1];
      const time_t timestamp = ulong_from_string_hexadecimal((*line)[2]);
      const time_t hdrbdy_mask = ulong_from_string_hexadecimal((*line)[3]);
      const std::string md5sum = from_hexadecimal((*line)[4]);
      checksum_type& cs = mi.depsrc_checksum[ns];
      cs.timestamp = timestamp;
      cs.hdrbdy_mask = hdrbdy_mask;
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

struct arena_guard {
  arena_guard(const parser_options& po) {
    arena_init();
    arena_set_recursion_limit(po.profile.recursion_limit);
  }
  ~arena_guard() {
    arena_clear();
  }
};

/* opens a module and examine its imports */
static void get_module_deps_from_source(const parser_options& po,
  module_info& mi)
{
  mi.imports.deps.clear();
  source_info si;
  si.minfo = &mi;
  si.mode = compile_mode_linkonly;
  arena_guard ag(po);
  pxc_parse_file(si, mi.imports);
  arena_error_throw_pushed();
  if (!mi.aux_filename.empty()) {
    mi.unique_namespace = mi.imports.main_unique_namespace;
  }
}

static void load_source_and_calc_checksum(const parser_options& po,
  module_info& mi)
{
  time_t tstamp = 0;
  unsigned hdrbdy_mask = 0;
  md5_context md5;
  unsigned long long src_mask = 0;
  unsigned long long src_mask_bit = 1;
    /* no need to oring? */
  module_info::source_files_type::iterator i;
  std::string notfound_fnlist;
  for (i = mi.source_files.begin(); i != mi.source_files.end();
    ++i, src_mask_bit <<= 1) {
    bool loaded = false;
    const std::string& fn = i->filename;
    if (po.verbose > 9) {
      fprintf(stderr, "loading source %s\n", fn.c_str());
    }
    while (true) {
      const time_t time_now = time(0);
      source_statbuf sbuf1, sbuf2;
      int e = stat_source_file(fn.c_str(), sbuf1);
      if (e != 0) {
        arena_error_throw(0, "-:-: Failed to stat '%s': errno=%d\n",
          fn.c_str(), e);
      }
      if (sbuf1.hdrbdy_mask == 0) {
        if (po.verbose > 9) {
          fprintf(stderr, "loading source %s: not found\n", fn.c_str());
        }
        if (!notfound_fnlist.empty()) {
          notfound_fnlist += ",";
        }
        notfound_fnlist += fn;
        break;
      }
      const source_file_content_ptr content = read_source_file_content(fn,
        po.trim_path);
      if (po.verbose > 9 && !content->content.empty()) {
        fprintf(stderr, "loading source %s: found\n", fn.c_str());
      }
      e = stat_source_file(fn.c_str(), sbuf2);
      if (e != 0) {
        arena_error_throw(0, "-:-: Failed to stat '%s': errno=%d\n",
          fn.c_str(), e);
      }
      if (sbuf2.hdrbdy_mask == 0) {
        continue;
      }
      /* modified while loading? */
      if (sbuf1.time_eq(sbuf2)) {
        /* if time() equals to mtime/ctime, modifications cant be detected
         * by comparing mtime/ctime. to avoid this situation, retry until
         * time() larger than mtime/ctime.
         */
        const time_t tm1 = std::max(sbuf1.time_max(), sbuf1.time_max());
        if (tm1 < time_now - PXC_TIMESTAMP_MARGIN) {
          i->content = content;
          tstamp = std::max(tstamp, tm1);
          hdrbdy_mask = sbuf1.hdrbdy_mask;
          loaded = true;
          break;
        }
      }
      /* reload the source file because it is possibly modified after we
       * stat()ed the file first. it is safe to set PSC_TIMESTAMP_MARGIN zero
       * if the system clock is never be skewed. */
      sleep(1);
    }
    if (i->content != nullptr) {
      md5.update(i->content->content);
    }
    if (loaded) {
      src_mask |= src_mask_bit;
      break;
    }
  }
  if (src_mask == 0) {
    arena_error_throw(0,
      "-:-: Failed to load namespace '%s'\n"
      "-:-: (tried files: %s)\n",
      mi.unique_namespace.c_str(), notfound_fnlist.c_str());
  }
  mi.source_checksum.timestamp = tstamp;
  mi.source_checksum.hdrbdy_mask = hdrbdy_mask;
  mi.source_checksum.md5sum = md5.final();
  mi.src_mask = src_mask;
  get_module_deps_from_source(po, mi);
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
  if (po.verbose > 1) {
    fprintf(stderr, "%s: loaded context\n", mi.get_name().c_str());
  }
  if (mi.source_checksum != mi_copy.source_checksum) {
    source_modified = true;
    if (po.verbose > 1) {
      fprintf(stderr, "%s: loaded contet: source modified: "
        "info-chksum:%s disk-chksum:%s\n",
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
      if (po.verbose > 1) {
        fprintf(stderr, "load_source_info (%s,%s): no info file\n",
          mi.get_name().c_str(), mi.aux_filename.c_str());
      }
      load_source_content(po, mi);
      mi.source_modified = true;
    } else {
      if (po.verbose > 1) {
        fprintf(stderr, "load_source_info (%s,%s): found info file\n",
          mi.get_name().c_str(), mi.aux_filename.c_str());
      }
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
    arena_error_throw(0, "-:-: A module dependency cycle is found: '%s'",
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
      si.filename_trim = trim_path(si.filename, po.trim_path);
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
        si.filename_trim = trim_path(si.filename, po.trim_path);
      }
    }
  }
  module_info& mi = ami.modules[modname];
  load_source_info(po, mi);
  ancestors.insert(modname);
  checked.insert(modname);
  for (imports_type::deps_type::const_iterator i = mi.imports.deps.begin();
    i != mi.imports.deps.end(); ++i) {
    try {
      get_module_info_rec(po, i->ns, "", ami, ancestors, checked);
    } catch (const std::exception& e) {
      std::string s = e.what();
      const std::string mn = trim_path(modname, po.trim_path);
      s = "-:-: (imported from '" + mn + "')\n" + s;
      throw std::runtime_error(s);
    }
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

static void get_indirect_imports(const parser_options& po,
  const all_modules_info& ami, const std::string& modname,
  bool is_main, strset& cc_srcs_r, strlist& cc_srcs_ord_r)
{
  if (po.gen_single_cc) {
    is_main = true;
  }
  if (cc_srcs_r.find(modname) != cc_srcs_r.end()) {
    return;
  }
  if (ami.modules.find(modname) == ami.modules.end()) {
    arena_error_throw(0, "-:-: Namespace '%s' not found\n", modname.c_str());
  }
  const module_info& md = ami.modules.find(modname)->second;
  for (imports_type::deps_type::const_iterator i = md.imports.deps.begin();
    i != md.imports.deps.end(); ++i) {
    get_indirect_imports(po, ami, i->ns, false, cc_srcs_r, cc_srcs_ord_r);
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
  /* self: ns fn src_timestamp src_hdrbdy src_md5 src_mask */
  str += "self\t" + mi.unique_namespace + "\t" + mi.aux_filename + "\t" +
    ulong_to_string_hexadecimal(mi.source_checksum.timestamp) + "\t" +
    ulong_to_string_hexadecimal(mi.source_checksum.hdrbdy_mask) + "\t" +
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
    /* import: ns pub src_timestamp src_hdrbdy src_md5 */
    str += "import\t" + i->ns + "\t";
    str += i->import_public ? "1\t" : "0\t";
    str += ulong_to_string_hexadecimal(j->second.source_checksum.timestamp)
      + "\t";
    str += ulong_to_string_hexadecimal(j->second.source_checksum.hdrbdy_mask)
      + "\t";
    str += to_hexadecimal(j->second.source_checksum.md5sum) + "\n";
    info_ts = std::max(j->second.source_checksum.timestamp, info_ts);
    dimps.insert(i->ns);
  }
  /* indirect imports */
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
    /* dep: ns 2 src_timestamp src_hdrbdy src_md5 */
    str += "indirect\t" + (*i) + "\t";
    str += ulong_to_string_hexadecimal(j->second.source_checksum.timestamp)
      + "\t";
    str += ulong_to_string_hexadecimal(j->second.source_checksum.hdrbdy_mask)
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

static bool need_to_relink(const parser_options& po,
  all_modules_info& ami, module_info& mi_main)
{
  const std::string ofn = po.profile.generate_dynamic ?
    get_so_filename(po, mi_main) : get_exe_filename(po, mi_main);
  struct stat osbuf;
  if (stat(ofn.c_str(), &osbuf) != 0) {
    if (errno == ENOENT) {
      return true;
    }
    arena_error_throw(0, "%s:0: Failed to stat: errno=%d\n",
      ofn.c_str(), errno);
  }
  const time_t ofn_time = std::min(osbuf.st_mtime, osbuf.st_ctime);
  for (all_modules_info::modules_type::const_iterator i = ami.modules.begin();
    i != ami.modules.end(); ++i) {
    const std::string fn = get_o_filename(po, i->second);
    source_statbuf sbuf;
    const bool stat_header = false;
    int e = stat_source_file(fn.c_str(), sbuf, stat_header);
    if (e != 0) {
      arena_error_throw(0, "%s:0: Failed to stat: errno=%d\n",
        fn.c_str(), e);
    }
    const time_t fn_time = sbuf.time_max();
    if (fn_time + PXC_TIMESTAMP_MARGIN >= ofn_time) {
      return true;
    }
  }
  return false;
}

static std::string get_cxx_and_cflags(const parser_options& po,
  module_info const& mi_main)
{
  const std::string cflags_str = make_cxx_opts(
    mi_main.self_copts.cflags, "");
  const std::string incdir_str = make_cxx_opts(
    mi_main.self_copts.incdir, "-I");
  const std::string opt_gcc = po.profile.gcc_compat ? " -g -fPIC" : "";
  const std::string cmd = po.profile.cxx + " " + po.profile.cflags
    + opt_gcc + cflags_str + incdir_str + " -I.";
  return cmd;
}

static std::string get_link_flags(const parser_options& po,
  all_modules_info& ami, module_info& mi_main)
{
  coptions copt;
  get_coptions(po, ami, mi_main, copt);
  const std::string libdir_str = make_cxx_opts(copt.libdir, "-L");
  const std::string link_str = make_cxx_opts(copt.link, "-l");
  const std::string ldflags_str = make_cxx_opts(copt.ldflags, "");
  DBG_LINK(fprintf(stderr, "copt.link sz=%d\n", copt.link.list_val.size()));
  std::string genopt;
  if (!po.profile.gcc_compat) {
    genopt = " ";
  } else if (!po.profile.generate_dynamic) {
    genopt = " -lpthread ";
  } else {
    const strmap::const_iterator i = po.profile.mapval.find("platform");
    if (i != po.profile.mapval.end() && i->second == "Darwin") {
      genopt = " -fPIC -undefined dynamic_lookup -bundle -bundle_loader "
        + po.argv0 + " -lpthread ";
    } else {
      genopt = " -fPIC -shared -lpthread ";
    }
  }
  const std::string link_str_all = genopt + po.profile.ldflags
    + ldflags_str + libdir_str + link_str;
  return link_str_all;
}

/* compiles cc files and generate a so file */
static void compile_cxx_all(const parser_options& po,
  all_modules_info& ami, module_info& mi_main)
{
  std::string ofn;
  if (po.gen_single_cc) {
    ofn = replace_or_append_suffix(mi_main.aux_filename, ".exe");
  } else {
    ofn = po.profile.generate_dynamic ?
      get_so_filename(po, mi_main) : get_exe_filename(po, mi_main);
  }
  const std::string ofn_tmp = ofn + ".tmp";
  std::string cmd = po.profile.cxx + " " + po.profile.cflags
    + " -o " + ofn_tmp;
  if (po.gen_single_cc) {
    cmd += " " + replace_or_append_suffix(mi_main.aux_filename, ".o");
  } else {
    for (all_modules_info::modules_type::const_iterator i
      = ami.modules.begin(); i != ami.modules.end(); ++i) {
      const std::string fn = get_o_filename(po, i->second);
      cmd += " " + fn;
    }
  }
  const std::string link_str = get_link_flags(po, ami, mi_main);
  DBG_SRC(
    if (po.gen_single_cc) {
      const std::string cxx_str = get_cxx_and_cflags(po, mi_main);
      fprintf(stderr, "%s\n", cxx_str.c_str());
      fprintf(stderr, "%s\n", link_str.c_str());
    }
  )
  cmd += link_str;
  int r = 0;
  std::string obuf;
  tmpfile_guard g(ofn_tmp);
  r = popen_cmd(cmd, obuf);
  if (r != 0) {
    arena_error_throw(0, "%s\n%s", cmd.c_str(), obuf.c_str());
  }
  if (po.verbose > 0 || (po.profile.show_warnings && !obuf.empty())) {
    fprintf(stderr, "%s\n%s", cmd.c_str(), obuf.c_str());
  }
  if (rename(ofn_tmp.c_str(), ofn.c_str()) != 0) {
    arena_error_throw(0, "-:-: rename('%s', '%s') failed: errno=%d",
      ofn_tmp.c_str(), ofn.c_str(), errno);
  }
}

static std::string get_expected_namespace_for_file(std::string fn)
{
  const size_t sl = fn.rfind('/');
  if (sl != fn.npos) {
    fn = fn.substr(sl + 1);
  }
  const size_t dot = fn.rfind('.');
  if (dot != fn.npos) {
    fn = fn.substr(0, dot);
  }
  return fn;
}

static int compare_file_contents(const std::string& f0, const std::string& f1)
{
  DBG_READ(fprintf(stderr, "read_file_content 0 %s\n", f0.c_str()));
  DBG_READ(fprintf(stderr, "read_file_content 0 %s\n", f1.c_str()));
  const std::string s0 = file_exist(f0) ? read_file_content(f0, true) : "";
  const std::string s1 = file_exist(f1) ? read_file_content(f1, true) : "";
  return s0 == s1 ? 0 : 1;
}

static void copy_file_atomic(const std::string& src, const std::string& dst,
  bool set_exec)
{
  const std::string tfn = dst + ".tmp";
  tmpfile_guard g(tfn);
  copy_file(src, tfn); /* throws */
  if (set_exec && chmod(tfn.c_str(), 0755) != 0) {
    arena_error_throw(0, "-:-: chmod('%s') failed: errno=%d",
      tfn.c_str(), errno);
  }
  if (rename(tfn.c_str(), dst.c_str()) != 0) {
    arena_error_throw(0, "-:-: rename('%s', '%s') failed: errno=%d",
      tfn.c_str(), dst.c_str(), errno);
  }
}

static void compile_module_to_cc_srcs(const parser_options& po,
  all_modules_info& ami, module_info& mi_main, generate_main_e gmain)
{
  prepare_workdir(po, ami, mi_main, mi_main.link_srcs);
  arena_guard ag(po);
  for (strlist::const_iterator i = mi_main.cc_srcs_ord.begin();
    i != mi_main.cc_srcs_ord.end(); ++i) {
    imports_type cursrc_imports;
    module_info& mi = get_modinfo_by_name(ami, *i);
    source_info si;
    si.minfo = &mi;
    si.mode = (&mi == &mi_main || po.gen_single_cc)
      ? compile_mode_main : compile_mode_import;
    pxc_parse_file(si, cursrc_imports);
    if (mi.unique_namespace != cursrc_imports.main_unique_namespace) {
      arena_error_throw(0,
        "-:-: Invalid namespace declaration '%s' for expected namespace '%s'",
        cursrc_imports.main_unique_namespace.c_str(),
        mi.unique_namespace.c_str());
    }
    if (si.mode == compile_mode_main && !mi.aux_filename.empty()) {
      const std::string exns = get_expected_namespace_for_file(
        mi.aux_filename);
      if (cursrc_imports.main_unique_namespace != exns) {
        arena_error_throw(0,
          "-:-: Invalid namespace declaration '%s' for '%s'",
          cursrc_imports.main_unique_namespace.c_str(),
          trim_path(mi.aux_filename, po.trim_path).c_str());
      }
    }
    DBG_SRC(fprintf(stderr, "src=%s mins=%s srcns=%s fn=%s\n", i->c_str(),
      mi.unique_namespace.c_str(),
      cursrc_imports.main_unique_namespace.c_str(), mi.aux_filename.c_str()));
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
    const double t1 = gettimeofday_double();
    arena_compile(po.profile.mapval, po.gen_single_cc, tmp_fn,
      mi_main.self_copts, gmain);
    if (po.profile.safe_mode != 0) {
      for (all_modules_info::modules_type::const_iterator i
        = ami.modules.begin(); i != ami.modules.end(); ++i) {
        const module_info& mi = i->second;
        const std::string uniqns = mi.unique_namespace;
        nssafety_e const s = nspropmap[uniqns].safety;
        if (!mi.aux_filename.empty()) {
          if (s != nssafety_e_safe) {
            arena_error_throw(0, "-:-: Unsafe namespace is not allowed: '%s'",
              mi.aux_filename.c_str());
          }
        } else {
          size_t bmax = po.profile.incdir.size();
          if (bmax >= po.profile.safe_mode) {
            bmax -= po.profile.safe_mode;
          }
          /* first #bmax path elements are allowed to be unsafe */
          size_t const bmexp = 1ULL << bmax;
          if (bmexp < mi.src_mask) {
            arena_error_throw(0,
              "-:-: Unsafe namespace is not allowed: namespace '%s'",
              i->first.c_str());
          }
        }
      }
    }
    const double t2 = gettimeofday_double();
    if (po.verbose > 0) {
      fprintf(stderr, "compiled %s %f\n", mi_main.unique_namespace.c_str(),
        t2 - t1);
    }
    if (!po.gen_cc_dir.empty()) {
      /* generate cc file to gen_cc_dir */
      std::string sfn = po.gen_cc_dir;
      // po.src_filename + ".gen/";
      mkdir_hier(sfn);
      std::string f;
      if (!mi_main.aux_filename.empty()) {
        /* filename instead of namespace */
        f = mi_main.aux_filename;
        std::string::size_type delim = f.rfind('/');
        if (delim != f.npos) {
          f = f.substr(delim);
        }
      } else {
        f = mi_main.unique_namespace;
      }
      for (size_t i = 0; i < f.size(); ++i) {
        if (f[i] == ':') {
          f[i] = '_';
        }
      }
      /* note: filename uniqueness is not guaranteed */
      sfn += replace_or_append_suffix(f, ".cc");
      if (compare_file_contents(tmp_fn, sfn) != 0) {
        copy_file_atomic(tmp_fn, sfn, false);
      }
    } else {
      if (rename(tmp_fn.c_str(), cc_filename.c_str()) != 0) {
        arena_error_throw(0, "-:-: rename('%s', '%s') failed: errno=%d",
          tmp_fn.c_str(), cc_filename.c_str(), errno);
      }
    }
  }
  /* compile to o */
  if (po.no_build) {
    return;
  }
  if (po.gen_cc_dir.empty() || po.gen_single_cc) {
    std::string cfn;
    std::string ofn;
    if (po.gen_single_cc) {
      std::string bp;
      if (!mi_main.aux_filename.empty() && mi_main.aux_filename[0] == '/') {
        bp = mi_main.aux_filename;
      } else {
        bp = po.gen_cc_dir + "/" + mi_main.aux_filename;
      }
      cfn = replace_or_append_suffix(bp, ".cc");
      ofn = replace_or_append_suffix(bp, ".o");
    } else {
      cfn = get_cc_filename(po, mi_main);
      ofn = get_o_filename(po, mi_main);
    }
    const std::string cmd = get_cxx_and_cflags(po, mi_main) +
      " -c " + cfn + " -o " + ofn;
    std::string obuf;
    int r = popen_cmd(cmd, obuf);
    if (r != 0) {
      arena_error_throw(0, "%s\n%s", cmd.c_str(), obuf.c_str());
    }
    if (po.verbose > 0 || (po.profile.show_warnings && !obuf.empty())) {
      fprintf(stderr, "%s\n%s", cmd.c_str(), obuf.c_str());
    }
  }
  /* generate info */
  if (po.gen_cc_dir.empty()) {
    const std::string info_filename = get_info_filename(po, mi_main);
    const std::string tmp_fn = info_filename + ".tmp";
    tmpfile_guard g(tmp_fn);
    generate_info_file(po, ami, mi_main, tmp_fn);
    if (rename(tmp_fn.c_str(), info_filename.c_str()) != 0) {
      arena_error_throw(0, "-:-: Failed to rename('%s', '%s'): errno=%d",
        tmp_fn.c_str(), info_filename.c_str(), errno);
    }
  }
}

static void get_mi_srcs(const parser_options& po, all_modules_info& ami,
  const std::string& modname)
{
  strset cc_srcs;
  strlist cc_srcs_ord;
  strset link_srcs;
  strlist link_srcs_ord;
  get_indirect_imports(po, ami, modname, true, cc_srcs, cc_srcs_ord);
  get_indirect_imports(po, ami, modname, true, link_srcs, link_srcs_ord);
  module_info& mi_main = get_modinfo_by_name(ami, modname);
  mi_main.cc_srcs = cc_srcs;
  mi_main.cc_srcs_ord = cc_srcs_ord;
  mi_main.link_srcs = link_srcs;
  mi_main.link_srcs_ord = link_srcs_ord;
}

/* compiles px files and generate one cxx file */
static void compile_module_to_cc(const parser_options& po,
  all_modules_info& ami, const std::string& modname, generate_main_e gmain)
{
  get_mi_srcs(po, ami, modname);
  if (po.verbose > 1) {
    fprintf(stderr, "COMPILE %s\n", modname.c_str());
  }
  module_info& mi_main = get_modinfo_by_name(ami, modname);
  compile_module_to_cc_srcs(po, ami, mi_main, gmain);
}

/* compiles namespace 'ns' or file 'fn' and generate cc files recursively */
static bool compile_modules_rec(const parser_options& po,
  all_modules_info& ami, const std::string& modname, generate_main_e gmain)
{
  module_info& md = get_modinfo_by_name(ami, modname);
  bool modified = false;
  if (md.compile_completed) {
    return false;
  }
  try {
    /* build imports first */
    if (!po.gen_single_cc) {
      for (imports_type::deps_type::const_iterator i = md.imports.deps.begin();
        i != md.imports.deps.end(); ++i) {
        const import_info& ii = *i;
        modified |= compile_modules_rec(po, ami, ii.ns, generate_main_none);
      }
      if (md.need_rebuild) {
        compile_module_to_cc(po, ami, modname, gmain);
        modified = true;
        md.need_rebuild = false;
      }
    } else {
      /* gen_single_cc mode */
      compile_module_to_cc(po, ami, modname, gmain);
      modified = true;
    }
  } catch (const std::exception& e) {
    std::string s = e.what();
    s = "-:-: (while compiling '" + trim_path(modname, po.trim_path) + "')\n"
      + s;
    throw std::runtime_error(s);
  }
  md.compile_completed = true;
  return modified; /* unused */
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
      if (po.verbose > 9) {
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
      if (po.verbose > 9) {
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
  } else {
    if (po.verbose > 9) {
      fprintf(stderr, "%s: need not to rebuild\n",
        mi.get_name().c_str());
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
        arena_error_throw(0, "-:-: Failed to open '%s': errno=%d\n",
          fn.c_str(), errno);
      }
      int flg = fcntl(fileno(fp.get()), F_GETFD, 0);
      if (flg == -1) {
        arena_error_throw(0, "-:-: Failed to fcntl(F_GETFD) '%s': errno=%d\n",
          fn.c_str(), errno);
      }
      flg |= FD_CLOEXEC; /* close on exec */
      if (fcntl(fileno(fp.get()), F_SETFD, flg) == -1) {
        arena_error_throw(0, "-:-: Failed to fcntl(F_SETFD) '%s': errno=%d\n",
          fn.c_str(), errno);
      }
      if (flock(fileno(fp.get()), LOCK_EX) != 0) {
        arena_error_throw(0, "-:-: Failed to lock '%s': errno=%d\n",
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

static std::string subst_variables(const std::string& str,
  const std::map<std::string, std::string>& m)
{
  /* find '%{foo}' from str */
  std::string r;
  for (size_t i = 0; i < str.size(); ++i) {
    const char ch = str[i];
    if (ch == '%' && i + 2 < str.size() && str[i + 1] == '{') {
      size_t p = i + 2;
      while (i + 1 < str.size() && str[i] != '}') {
        ++i;
      }
      if (str[i] == '}') {
        std::string k = str.substr(p, i - p);
        std::map<std::string, std::string>::const_iterator iter = m.find(k);
        if (iter != m.end()) {
          r.insert(r.end(), iter->second.begin(), iter->second.end());
        }
        continue;
      }
    }
    r.push_back(ch);
  }
  return r;
}

static void load_profile(parser_options& po)
{
  if (po.verbose > 1) {
    fprintf(stderr, "loading profile %s\n", po.profile_name.c_str());
  }
  const std::string pstr = read_file_content(po.profile_name, true);
  md5_context md5;
  md5.update(pstr);
  po.profile.md5sum = to_hexadecimal(md5.final());
  strlist lines;
  split_string(pstr, '\n', lines);
  for (strlist::const_iterator i = lines.begin(); i != lines.end(); ++i) {
    std::string s = *i;
    const size_t ch = s.find('#');
    if (ch != std::string::npos) {
      s = s.substr(0, ch);
    }
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
  iter = po.profile.mapval.find("platform");
  std::string platform;
  if (iter == po.profile.mapval.end()) {
    struct utsname utsn;
    if (::uname(&utsn) < 0) {
      arena_error_throw(0, "-:-: uname() failed: errno=%d", errno);
    }
    platform = std::string(utsn.sysname);
    // fprintf(stderr, "uname: %s", platform.c_str());
    po.profile.mapval["platform"] = platform;
  }
  iter = po.profile.mapval.find("gcc_compat");
  if (iter != po.profile.mapval.end()) {
    po.profile.gcc_compat = atoi(iter->second.c_str()) != 0;
  }
  iter = po.profile.mapval.find("generate_dynamic");
  if (iter != po.profile.mapval.end() && atoi(iter->second.c_str()) != 0) {
    po.profile.generate_dynamic = true;
  }
  iter = po.profile.mapval.find("incdir");
  std::string incstr = iter != po.profile.mapval.end()
    ? iter->second : "";
  incstr = subst_variables(incstr, po.profile.mapval);
  split_string(incstr, ':', po.profile.incdir);
  if (po.profile.incdir.empty()) {
    po.profile.incdir.push_back("/usr/local/share/pxclib/");
    po.profile.incdir.push_back("/usr/share/pxclib/");
    po.profile.incdir.push_back(".");
  }
  iter = po.profile.mapval.find("cxx");
  po.profile.cxx = iter != po.profile.mapval.end()
    ? iter->second : "g++";
  iter = po.profile.mapval.find("cflags");
  po.profile.cflags = iter != po.profile.mapval.end()
    ? iter->second
    : "-rdynamic -g -O3 -DNDEBUG -Wall -Wno-unused -Wno-attributes";
    /* note: adding -Wno-attributes is a workaround for that gcc warns
     * when attribute(("may_alias")) is specified for a struct. */
  iter = po.profile.mapval.find("ldflags");
  po.profile.ldflags = iter != po.profile.mapval.end()
    ? iter->second : "";
  iter = po.profile.mapval.find("show_warnings");
  if (iter != po.profile.mapval.end()) {
    po.profile.show_warnings = true;
  }
  iter = po.profile.mapval.find("error_detail");
  if (iter != po.profile.mapval.end()) {
    po.profile.error_detail = true;
  }
  iter = po.profile.mapval.find("emit_threaded_dll");
  po.profile.emit_threaded_dll = iter != po.profile.mapval.end()
    ? iter->second : "";
  iter = po.profile.mapval.find("safe_mode");
  po.profile.safe_mode = iter != po.profile.mapval.end()
    ? atoi(iter->second.c_str()) : 1;
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
  filelock_lockobj lock(po.work_dir + "_lock");
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
      if (mi.aux_filename.empty() && mi.unique_namespace.empty()) {
        /* this ns is removed from the dependency list */
        continue;
      }
      get_indirect_imports(po, ami,
        !mi.aux_filename.empty() ? mi.aux_filename : mi.unique_namespace,
        true, mi.cc_srcs, mi.cc_srcs_ord);
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
  /* compile to .o */
  if (!po.no_update || !file_exist(exe_fn)) {
    compile_modules_rec(po, ami, fn,
      po.profile.generate_dynamic ? generate_main_dl : generate_main_exe);
  }
  if (po.no_build) {
    return 0;
  }
  if (!po.gen_cc_dir.empty() && !po.gen_single_cc && po.no_execute) {
    return 0;
  }
  /* compile to exe or so */
  if (po.gen_single_cc || need_to_relink(po, ami, mi_main)) {
    get_mi_srcs(po, ami, fn);
      /* need to set link_srcs for mi_main here because it is not set when
       * compilation to cc/o is skipped */
    compile_cxx_all(po, ami, mi_main);
  }
  if (po.gen_out) {
    std::string base_filename = po.src_filename;
    if (base_filename.size() > 3 &&
      base_filename.substr(base_filename.size() - 3) == ".px") {
      base_filename = base_filename.substr(0, base_filename.size() - 3);
    }
    base_filename += (po.profile.generate_dynamic ? ".so" : ".exe");
    const std::string gen_out = base_filename;
    const std::string ofn = po.profile.generate_dynamic ?
      get_so_filename(po, mi_main) : get_exe_filename(po, mi_main);
    copy_file_atomic(ofn, gen_out, true);
  }
  if (po.no_execute) {
    return 0;
  }
  const std::string sn = arena_get_ns_main_funcname(mi_main.unique_namespace)
    + "$cm";
  if (!po.profile.generate_dynamic) {
    char *e_argv[2];
    e_argv[0] = strdup(exe_fn.c_str());
    e_argv[1] = 0;
    /* note: lock will be released when execv is succeeded */
    execv(e_argv[0], e_argv);
    fprintf(stderr, "Failed to execute '%s': errno=%d\n", exe_fn.c_str(),
      errno);
    return 2;
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

static int clean_workdir(parser_options const& po, const std::string& dn)
{
  if (dn.size() < 8 || dn.substr(dn.size() - 8) != ".pxcwork") {
    /* for safety */
    fprintf(stderr, "internal error: invalid working directory: '%s'\n",
      dn.c_str());
    return 1;
  }
  fprintf(stderr, "cleaning: '%s'\n", dn.c_str());
  try {
    filelock_lockobj lock(dn + "_lock"); /* throws */
    unlink_recursive(dn); /* throws */
  } catch (const std::exception& e) {
    if (po.verbose > 1) {
      fprintf(stderr, "warning: %s", e.what());
    }
  }
  return 0;
}

static int clean_workdir_all(const parser_options& po)
{
  std::vector<std::string> ents = read_directory(po.work_dir + "/");
  for (size_t i = 0; i < ents.size(); ++i) {
    const std::string e = ents[i];
    if (e.size() < 8 || e.substr(e.size() - 8) != ".pxcwork") {
      continue;
    }
    int r = clean_workdir(po, po.work_dir + "/" + e);
    if (r != 0) {
      return r;
    }
  }
  for (size_t i = 0; i < ents.size(); ++i) {
    const std::string e = ents[i];
    if (e.size() < 13 || e.substr(e.size() - 13) != ".pxcwork_lock") {
      continue;
    }
    unlink_if(po.work_dir + "/" + e);
      /* NOTE: causes problem if another process is doing compilation */
  }
  return 0;
}

static int prepare_options(parser_options& po, int argc, char **argv)
{
  if (argc >= 1) {
    po.argv0 = argv[0];
    source_statbuf sbuf;
    const bool has_header = false;
    int const e = stat_source_file(argv[0], sbuf, has_header);
    if (e == 0) {
      po.argv0_ctime = sbuf.time_max();
    }
  } else {
    po.argv0 = "pxc";
  }
  bool err = false;
  int i = 1;
  for (; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg.empty()) {
      continue;
    }
    std::string::size_type p = arg.find('=');
    std::string s;
    std::string param;
    if (p != arg.npos) {
      s = arg.substr(0, p);
      param = arg.substr(p + 1);
    } else {
      s = arg;
    }
    if (s[0] == '-') {
      if (param.empty()) {
        if (s == "--clean" || s == "-c") {
          po.clean_flag = true;
        } else if (s == "--clean-all" || s == "-C") {
          po.clean_all_flag = true;
        } else if (s == "--generate" || s == "-g") {
          po.gen_out = true;
        } else if (s == "--generate-single-cc" || s == "-gs") {
          po.gen_single_cc = true;
        } else if (s == "--no-build" || s == "-nb") {
          po.no_build = true;
        } else if (s == "--no-update" || s == "-nu") {
          po.no_update = true;
        } else if (s == "--no-execute" || s == "-ne") {
          po.no_execute = true;
        } else if (s == "--no-realpath") {
          po.no_realpath = true; /* for regression test */
        } else {
          err = true;
        }
      } else {
        if (s == "--verbose" || s == "-v") {
          po.verbose = atoi(param.c_str());
        } else if (s == "--profile" || s == "-p") {
          po.profile_name = param;
        } else if (s == "--work-dir" || s == "-w") {
          po.work_dir = param;
        } else if (s == "--generate-cc") {
          po.gen_cc_dir = param;
        } else if (s == "--trim-path") {
          po.trim_path = atoi(param.c_str()); /* for regression test */
        } else {
          err = true;
        }
      }
      if (err) {
        fprintf(stderr, "Unknown option '%s'\n", arg.c_str());
        break;
      }
    } else {
      po.src_filename = s;
      break; /* remaining arguments are passed to the executable */
    }
  }
  if (err || argc == 1) {
    fprintf(stderr,
      "Usage: %s [OPTIONS ...] SOURCE_FILE\n"
      "  Options:\n"
      "  --clean | -c\n"
      "  --clean-all | -C\n"
      "  --generate | -g\n"
      "  --no-build | -nb\n"
      "  --no-update | -nu\n"
      "  --no-execute | -ne\n"
      "  --no-realpath | -nr\n"
      "  --profile=FILE | -p=FILE\n"
      "  --work-dir=DIRECTORY | -w=DIRECTORY\n"
      "  --verbose=VALUE | -v=VALUE\n"
      "  --generate-cc=DIRECTORY\n"
      "  --generate-single-cc\n"
      , argv[0]);
    return 1;
  }
  {
    if (po.gen_single_cc && po.gen_cc_dir.empty()) {
      po.gen_cc_dir = ".";
    }
    if (!po.gen_cc_dir.empty()) {
      po.no_execute = true;
    }
    if (po.gen_out) {
      po.no_execute = true;
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
    while (true) {
      snprintf(buf, sizeof(buf), "PXC_ARG%d", c++);
      const char *p = getenv(buf);
      if (p == 0) {
        break;
      }
      unsetenv(buf);
    }
  }
  if (po.work_dir.empty()) {
    po.work_dir = get_home_directory() + "/.pxc";
  }

  {
    static const char *etc_path[] = { "/usr/local/etc", "/etc", 0 };
    for (const char **p = etc_path; *p != 0; ++p) {
      std::string dstr(*p);
      std::string prof_path;
      if (po.profile_name.empty()) {
        prof_path = dstr + "/pxc/pxc.profile";
      } else if (!po.no_realpath) {
        if (po.profile_name.find('/') == std::string::npos) {
          prof_path = dstr + "/pxc/pxc_" + po.profile_name + ".profile";
        } else {
          prof_path = po.profile_name;
        }
        prof_path = get_canonical_path(prof_path);
      }
      if (file_access(prof_path)) {
        po.profile_name = prof_path;
        break;
      }
    }
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
  if (po.clean_all_flag) {
    int r = clean_workdir_all(po);
    if (r != 0) {
      return r;
    }
  }
  po.work_dir += "/" +
    escape_hex_filename(trim_path(po.profile_name, po.trim_path)) + ".pxcwork";
  return 0;
}

static int cdriver_main(int argc, char **argv)
{
  try {
    extend_rlimit_stack(); /* pxc consumes stack */
    parser_options po;
    int r = prepare_options(po, argc, argv);
    if (r != 0) {
      return r;
    }
    if (po.clean_flag) {
      r = clean_workdir(po, po.work_dir);
      if (r != 0) {
        return r;
      }
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

void *stack_bottom = 0; // for debugging

int main(int argc, char **argv)
{
  int dummy = 0;
  stack_bottom = &dummy;
  return pxc::cdriver_main(argc, argv);
}

