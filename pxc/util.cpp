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

#include "util.hpp"
#include "auto_free.hpp"
#include "auto_fp.hpp"
#include "expr.hpp"

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

namespace pxc {

std::string get_canonical_path(const std::string& x)
{
  auto_free p(realpath(x.c_str(), 0));
  if (p.get() == 0) {
    return x;
  }
  const std::string s = std::string(p.get());
  return s;
}

std::string trim_path(const std::string& s, int trim_path)
{
  if (trim_path != 0) {
    int n = 0;
    size_t p = s.size();
    while (p > 0) {
      if (s[p - 1] == '/') {
        if (++n >= trim_path) {
          break;
        }
      }
      --p;
    }
    return s.substr(p);
  }
  return s;
}

std::string read_file_content(const std::string& fn, bool err_thr)
{
  std::string s;
  std::vector<char> buf;
  buf.resize(16384);
  auto_fp fp(fopen(fn.c_str(), "r"));
  if (fp.get() == 0) {
    if (!err_thr) {
      return std::string();
    }
    arena_error_throw(0, "%s:0: Failed to open: errno=%d\n",
      fn.c_str(), errno);
  }
  while (!feof(fp.get())) {
    const size_t r = fread(&buf[0], 1, buf.size(), fp.get());
    if (ferror(fp.get())) {
      arena_error_throw(0, "%s:0: Failed to read: errno=%d\n",
        fn.c_str(), errno);
    }
    s.append(&buf[0], r);
    if (r == 0) {
      break;
    }
  }
  return s;
}

bool file_access(const std::string& fn)
{
  if (::access(fn.c_str(), R_OK) == 0) {
    return true;
  }
  return false;
}

std::vector<std::string> read_directory(const std::string& dn)
{
  std::vector<std::string> r;
  auto_dir dir(opendir(dn.c_str()));
  if (dir.get()) {
    while (true) {
      struct dirent *const ent = readdir(dir.get()); /* TODO: THREAD? */
      if (ent == 0) {
        break;
      }
      const std::string c = ent->d_name;
      if (c == "." || c == "..") {
        continue;
      }
      r.push_back(c);
    }
  }
  return r;
}

std::string ulong_to_string_hexadecimal(unsigned long long v)
{
  char buf[32];
  const int len = snprintf(buf, sizeof(buf), "%llx", v);
  return std::string(buf, buf + len);
}

unsigned long long ulong_from_string_hexadecimal(const std::string& s)
{
  unsigned long long v = 0;
  const int r = sscanf(s.c_str(), "%llx", &v);
  return r == 1 ? v : 0;
}

unsigned char uchar_from_hexadecimal(int c)
{
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return 0;
}

std::string from_hexadecimal(const std::string& str)
{
  std::string r;
  const size_t sz = str.size();
  for (size_t i = 0; i * 2 + 1 < sz; ++i) {
    unsigned char c1 = uchar_from_hexadecimal(str[i * 2]);
    unsigned char c2 = uchar_from_hexadecimal(str[i * 2 + 1]);
    r.push_back(c1 * 16 + c2);
  }
  return r;
}

int uchar_to_hexadecimal(unsigned char c)
{
  if (c < 10) {
    return c + '0';
  }
  if (c < 16) {
    return c - 10 + 'a';
  }
  return '0';
}

std::string to_hexadecimal(const std::string& str)
{
  /* NOTE: slow */
  std::string r;
  const size_t sz = str.size();
  for (size_t i = 0; i < sz; ++i) {
    unsigned char c = str[i];
    r.push_back(uchar_to_hexadecimal(c / 16));
    r.push_back(uchar_to_hexadecimal(c % 16));
  }
  return r;
}

bool is_valid_hexadecimal_char(char ch)
{
  if (ch >= '0' && ch <= '9') {
    return true;
  }
  if (ch >= 'a' && ch <= 'f') {
    return true;
  }
  if (ch >= 'A' && ch <= 'F') {
    return true;
  }
  return false;
}

std::string unescape_c_str_literal(const char *str, bool& success_r)
{
  success_r = true;
  std::string s;
  size_t len = strlen(str);
  if (len < 2) {
    success_r = false;
    return std::string();
  }
  if (str[0] != '\"' || str[len - 1] != '\"') {
    success_r = false;
    return std::string();
  }
  str += 1;
  len -= 2;
  for (size_t i = 0; i < len; ++i) {
    char ch = str[i];
    if (ch == '\\' && i + 1 < len) {
      ch = str[i + 1];
      ++i;
      if (ch == 'x') {
        if (i + 2 < len) {
          const char c1 = str[i + 1];
          const char c2 = str[i + 2];
          if (!is_valid_hexadecimal_char(c1) ||
            !is_valid_hexadecimal_char(c2)) {
            success_r = false;
          } else {
            const unsigned char rch = (uchar_from_hexadecimal(c1) << 4) |
              uchar_from_hexadecimal(c2);
            s.push_back(rch);
            i += 2;
          }
        } else {
          success_r = false;
        }
      } else if (ch == 'a' || ch == 'A') {
        s.push_back('\a');
      } else if (ch == 'b' || ch == 'B') {
        s.push_back('\b');
      } else if (ch == 'f' || ch == 'F') {
        s.push_back('\f');
      } else if (ch == 'n' || ch == 'N') {
        s.push_back('\n');
      } else if (ch == 'r' || ch == 'R') {
        s.push_back('\r');
      } else if (ch == 't' || ch == 'T') {
        s.push_back('\t');
      } else if (ch == 'v' || ch == 'V') {
        s.push_back('\v');
      } else if (ch == '\'') {
        s.push_back('\'');
      } else if (ch == '\"') {
        s.push_back('\"');
      } else if (ch == '\\') {
        s.push_back('\\');
      } else if (ch == '\?') {
        s.push_back('\?');
      } else if (ch == '0') {
        s.push_back('\0');
      } else {
        success_r = false;
      }
    } else {
      s.push_back(ch);
    }
  }
  return s;
}

std::string escape_c_str_literal(const std::string& s)
{
  std::string r;
  r.push_back('\"');
  const size_t sz = s.size();
  for (size_t i = 0; i < sz; ++i) {
    const unsigned char ch = s[i];
    if (ch == '\\' || ch == '\'' || ch == '\"' || ch < 0x20) {
      r.push_back('\\');
      r.push_back('0' + ((ch >> 6) & 0x07));
      r.push_back('0' + ((ch >> 3) & 0x07));
      r.push_back('0' + (ch & 0x07));
    } else {
      r.push_back(ch);
    }
  }
  r.push_back('\"');
  return r;
}

typedef std::deque< std::vector<std::string> > strarrlist;

void parse_string_as_strarrlist(const std::string& data, strarrlist& r)
{
  const size_t sz = data.size();
  r.clear();
  std::vector<std::string> line;
  size_t i = 0;
  while (true) {
    const size_t prev = i;
    while (i < sz && data[i] != '\t' && data[i] != '\n') {
      ++i;
    }
    if (i >= sz) {
      break;
    }
    line.push_back(data.substr(prev, i - prev));
    if (data[i] == '\n') {
      r.push_back(line);
      line.clear();
    }
    ++i;
  }
  if (!line.empty()) {
    r.push_back(line);
  }
}

void split_string(const std::string& str, char delim, strlist& r)
{
  const size_t sz = str.size();
  r.clear();
  size_t i = 0;
  size_t prev = 0;
  while (true) {
    prev = i;
    while (i < sz && str[i] != delim) {
      ++i;
    }
    if (i >= sz) {
      break;
    }
    r.push_back(str.substr(prev, i - prev));
    ++i;
  }
  if (prev != i) {
    r.push_back(str.substr(prev, i - prev));
  }
}

static inline bool is_space_char(char ch)
{
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

void split_string_space(const std::string& str, strlist& r)
{
  const size_t sz = str.size();
  r.clear();
  size_t i = 0;
  size_t prev = 0;
  while (true) {
    prev = i;
    while (i < sz && is_space_char(str[i])) {
      ++i;
    }
    if (i >= sz) {
      break;
    }
    r.push_back(str.substr(prev, i - prev));
    ++i;
  }
  if (prev != i) {
    r.push_back(str.substr(prev, i - prev));
  }
}

std::string trim_space(const std::string& str)
{
  const size_t sz = str.size();
  size_t i = 0;
  while (i < sz && is_space_char(str[i])) {
    ++i;
  }
  size_t j = sz;
  while (j > 0 && is_space_char(str[j - 1])) {
    --j;
  }
  if (i == 0 && j == sz) {
    return str;
  }
  return str.substr(i, j - i);
}

std::string join_string(const strlist& sl, const std::string& delim)
{
  std::string r;
  for (strlist::const_iterator i = sl.begin(); i != sl.end(); ++i) {
    if (i != sl.begin()) {
      r += delim;
    }
    r += (*i);
  }
  return r;
}

long long string_to_long(const std::string& s)
{
  return strtoll(s.c_str(), 0, 10);
}

unsigned long long string_to_ulong(const std::string& s)
{
  return strtoull(s.c_str(), 0, 10);
}

std::string ulong_to_string(unsigned long long v)
{
  char buf[32];
  snprintf(buf, sizeof(buf), "%llu", v);
  return std::string(buf);
}

std::string long_to_string(long long v)
{
  char buf[32];
  snprintf(buf, sizeof(buf), "%lld", v);
  return std::string(buf);
}

bool file_exist(const std::string& fn)
{
  struct stat sbuf;
  if (stat(fn.c_str(), &sbuf) == 0) {
    return true;
  }
  if (errno != ENOENT) {
    arena_error_throw(0, "-:0: Failed to stat('%s'): errno=%d",
      fn.c_str(), errno);
  }
  return false;
}

void mkdir_hier(const std::string& d)
{
  size_t len = d.size();
  struct stat sbuf;
  while (len && d[len - 1] == '/') {
    --len;
  }
  if (len == 0) {
    return;
  }
  const std::string d_nosl = d.substr(0, len);
  if (stat(d_nosl.c_str(), &sbuf) == 0) {
    return;
  }
  if (errno != ENOENT) {
    arena_error_throw(0, "-:0: Failed to stat('%s'): errno=%d",
      d_nosl.c_str(), errno);
  }
  while (len && d[len - 1] != '/') {
    --len;
  }
  mkdir_hier(d.substr(0, len));
  if (mkdir(d_nosl.c_str(), 0777) != 0) {
    arena_error_throw(0, "-:0: Failed to mkdir '%s': errno=%d", d.c_str(),
      errno);
  }
}

void write_file_content(const std::string& fn, const std::string& data)
{
  auto_fp fp(fopen(fn.c_str(), "w"));
  if (fp.get() == 0) {
    arena_error_throw(0, "%s:0: Failed to open: errno=%d\n",
      fn.c_str(), errno);
  }
  if (fwrite(data.data(), 1, data.size(), fp.get()) != data.size() ||
    fp.close() != 0) {
    arena_error_throw(0, "%s:0: Failed to write: errno=%d\n",
      fn.c_str(), errno);
  }
}

template <size_t N> struct auto_fds {
  int fds[N];
  auto_fds() {
    for (size_t i = 0; i < N; ++i) {
      fds[i] = -1;
    }
  }
  auto_fds(const auto_fds&) = delete;
  auto_fds& operator =(const auto_fds&) = delete;
  ~auto_fds() {
    for (size_t i = 0; i < N; ++i) {
      if (fds[i] >= 0) {
        close(fds[i]);
        fds[i] = -1;
      }
    }
  }
  void close_one(size_t idx) {
    if (idx < N && fds[idx] >= 0) {
      close(fds[idx]);
      fds[idx] = -1;
    }
  }
  int get_one(size_t idx) {
    if (idx < N) {
      return fds[idx];
    }
    return -1;
  }
};

int popen_cmd(const std::string& cmd, std::string& out_r)
{
  std::vector<std::string> args;
  std::vector<char *> raw_args;
  {
    std::string tok;
    for (size_t i = 0; i < cmd.size(); ++i) {
      const char ch = cmd[i];
      if (ch == ' ') {
        if (!tok.empty()) {
          tok.push_back('\0');
          args.push_back(tok);
        }
        tok = "";
      } else {
        tok.push_back(ch);
      }
    }
    if (!tok.empty()) {
      tok.push_back('\0');
      args.push_back(tok);
    }
    for (size_t i = 0; i < args.size(); ++i) {
      raw_args.push_back(&args[i][0]);
    }
    if (raw_args.empty()) {
      return -1;
    }
    raw_args.push_back(nullptr);
  }
  auto_fds<2> afds;
  if (pipe(&afds.fds[0]) < 0) {
    return -1;
  }
  const pid_t pid = fork();
  if (pid == 0) {
    // child process
    afds.close_one(0); // read end (stdout)
    dup2(afds.get_one(1), 1);
    dup2(afds.get_one(1), 2);
    afds.close_one(1);
    execvp(raw_args[0], &raw_args[0]);
    _exit(-1);
  }
  afds.close_one(1);
  std::vector<char> buf(4096);
  while (true) {
    const ssize_t rlen = read(afds.get_one(0), &buf[0], buf.size());
    if (rlen < 0) {
      if (errno == EAGAIN || errno == EINTR) {
        continue;
      }
      return -1;
    }
    if (rlen == 0) {
      break;
    }
    out_r.append(buf.data(), rlen);
  }
  int r = -1;
  while (true) {
    int st = 0;
    const int wpid = waitpid(pid, &st, 0);
    if (wpid < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -2;
    }
    if (wpid == pid) {
      if (WIFEXITED(st)) {
        r = WEXITSTATUS(st);
      }
      break;
    }
  }
  return r;
}

void unlink_recursive(const std::string& fn)
{
  struct stat sbuf;
  if (stat(fn.c_str(), &sbuf) != 0) {
    if (errno != ENOENT) {
      arena_error_throw(0, "-:0: Failed to stat('%s'): errno=%d",
        fn.c_str(), errno);
    }
    return;
  }
  if (S_ISDIR(sbuf.st_mode)) {
    strlist cl;
    auto_dir dir(opendir(fn.c_str()));
    if (dir.get()) {
      while (true) {
        struct dirent *const ent = readdir(dir.get()); /* TODO: THREAD? */
        if (ent == 0) {
          break;
        }
        const std::string c = ent->d_name;
        if (c == "." || c == "..") {
          continue;
        }
        cl.push_back(c);
      }
    }
    dir.close();
    for (strlist::const_iterator i = cl.begin(); i != cl.end(); ++i) {
      unlink_recursive(fn + "/" + (*i));
    }
    rmdir_if(fn);
  } else {
    unlink_if(fn);
  }
}

void rmdir_if(const std::string& fn)
{
  if (rmdir(fn.c_str()) != 0 && errno != ENOENT) {
    arena_error_throw(0, "-:0: Failed to rmdir('%s'): errno=%d",
      fn.c_str(), errno);
  }
}

void unlink_if(const std::string& fn)
{
  if (unlink(fn.c_str()) != 0 && errno != ENOENT) {
    arena_error_throw(0, "-:0: Failed to unlink('%s'): errno=%d",
      fn.c_str(), errno);
  }
}

void copy_file(const std::string& ffrom, const std::string& fto)
{
  auto_fp rfp(fopen(ffrom.c_str(), "r"));
  if (rfp.get() == 0) {
    arena_error_throw(0, "%s:0: Failed to open: errno=%d\n",
      ffrom.c_str(), errno);
  }
  auto_fp wfp(fopen(fto.c_str(), "w"));
  if (wfp.get() == 0) {
    arena_error_throw(0, "%s:0: Failed to open: errno=%d\n",
      fto.c_str(), errno);
  }
  std::vector<char> buf;
  buf.resize(16384);
  while (!feof(rfp.get())) {
    size_t e = fread(&buf[0], 1, buf.size(), rfp.get());
    if (ferror(rfp.get())) {
      arena_error_throw(0, "%s:0: Failed to read: errno=%d\n",
        ffrom.c_str(), errno);
    }
    if (e == 0) {
      break;
    }
    size_t wrsize = e;
    e = fwrite(&buf[0], 1, wrsize, wfp.get());
    if (ferror(wfp.get()) || e != wrsize) {
      arena_error_throw(0, "%s:0: Failed to write: errno=%d\n",
        fto.c_str(), errno);
    }
  }
  if (wfp.close() != 0) {
    arena_error_throw(0, "%s:0: Failed to write: errno=%d\n",
      fto.c_str(), errno);
  }
}

std::string get_home_directory()
{
  const char *const s = getenv("HOME");
  if (s == 0) {
    arena_error_throw(0, "-:0: Environment variable HOME is not set\n");
  }
  return s;
}

std::string escape_hex_controls(const std::string& str)
{
  std::string r;
  const size_t sz = str.size();
  r.reserve(sz);
  for (size_t i = 0; i < sz; ++i) {
    const unsigned char c = str[i];
    if (c < 0x20 || c == '%') {
      r.push_back('%');
      r.push_back(uchar_to_hexadecimal(c / 16));
      r.push_back(uchar_to_hexadecimal(c % 16));
    } else {
      r.push_back(c);
    }
  }
  return r;
}

std::string unescape_hex_controls(const std::string& str)
{
  std::string r;
  const size_t sz = str.size();
  r.reserve(sz);
  for (size_t i = 0; i < sz; ++i) {
    const unsigned char c = str[i];
    if (c == '%' && i + 2 < sz) {
      const unsigned char c1 = uchar_from_hexadecimal(str[i + 1]);
      const unsigned char c2 = uchar_from_hexadecimal(str[i + 2]);
      r.push_back(c1 * 16 + c2);
      i += 2;
    } else {
      r.push_back(c);
    }
  }
  return r;
}

std::string escape_hex_filename(const std::string& str)
{
  std::string r;
  const size_t sz = str.size();
  r.reserve(sz);
  for (size_t i = 0; i < sz; ++i) {
    const unsigned char c = str[i];
    if (c < 0x20 || c == '%' || c == '/' || (c == '.' && i == 0)) {
      r.push_back('%');
      r.push_back(uchar_to_hexadecimal(c / 16));
      r.push_back(uchar_to_hexadecimal(c % 16));
    } else {
      r.push_back(c);
    }
  }
  return r;
}

static inline bool is_non_alnum(unsigned char c)
{
  if (c >= '0' && c <= '9') {
    return false;
  }
  if (c >= 'a' && c <= 'z') {
    return false;
  }
  if (c >= 'A' && c <= 'Z') {
    return false;
  }
  return true;
}

std::string escape_hex_non_alnum(const std::string& str)
{
  std::string r;
  const size_t sz = str.size();
  r.reserve(sz);
  for (size_t i = 0; i < sz; ++i) {
    const unsigned char c = str[i];
    if (is_non_alnum(c)) {
      r.push_back('_');
      if (c == '_') {
        r.push_back('_');
      } else {
        r.push_back(uchar_to_hexadecimal(c / 16));
        r.push_back(uchar_to_hexadecimal(c % 16));
      }
    } else {
      r.push_back(c);
    }
  }
  return r;
}

std::string unescape_hex_non_alnum(const std::string& str)
{
  std::string r;
  const size_t sz = str.size();
  r.reserve(sz);
  for (size_t i = 0; i < sz; ++i) {
    const unsigned char c = str[i];
    if (c == '_' && i + 1 < sz && str[i + 1] == '_') {
      r.push_back('_');
    } else if (c == '_' && i + 2 < sz) {
      const unsigned char c1 = uchar_from_hexadecimal(str[i + 1]);
      const unsigned char c2 = uchar_from_hexadecimal(str[i + 2]);
      r.push_back(c1 * 16 + c2);
      i += 2;
    } else {
      r.push_back(c);
    }
  }
  return r;
}

double gettimeofday_double()
{
  struct timeval tv;
  gettimeofday(&tv, 0);
  double rv = tv.tv_usec;
  rv /= 1000000;
  rv += tv.tv_sec;
  return rv;
}

void extend_rlimit_stack()
{
  struct rlimit rlim;
  int r = getrlimit(RLIMIT_STACK, &rlim);
  if (r != 0) {
    arena_error_throw(0, "-:0: Failed to get RLIMIT_STACK: errno=%d\n", errno);
  }
  rlim.rlim_cur = rlim.rlim_max;
  r = setrlimit(RLIMIT_STACK, &rlim);
  if (r != 0) {
    arena_error_throw(0, "-:0: Failed to set RLIMIT_STACK: errno=%d\n", errno);
  }
}

};

