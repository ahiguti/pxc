
// vim:sw=2:ts=8:ai

#ifndef PXCRT_BUILTIN_HPP
#define PXCRT_BUILTIN_HPP

#include <vector>
#include <string>
#include <map>
#include <stdint.h>
#include <stdexcept>

#ifdef PXCRT_DBG_RC
#include <stdio.h>
#define DBG_RC(x) x
#else
#define DBG_RC(x)
#endif

namespace pxcrt {

typedef std::string string;

void throw_invalid_index() __attribute__((noreturn));
void throw_null_dereference() __attribute__((noreturn));
void throw_invalid_field() __attribute__((noreturn));
void throw_would_invalidate() __attribute__((noreturn));

template <typename T> inline void check_null(T& ptr)
{
  if (ptr == 0) throw_null_dereference();
}

template <typename T> inline void check_null(const T& ptr)
{
  if (ptr == 0) throw_null_dereference();
}

struct tpdummy;

struct unit {
  unit() { }
  unit(int) { }
  bool operator ==(const unit&) { return true; }
  bool operator <(const unit&) { return false; }
};

template <typename Tc>
struct guard {
  typedef typename Tc::data_type data_type;
  Tc& data;
  data_type& elem;
  data_type& get() { return elem; }
  guard(Tc& data, data_type& elem) : data(data), elem(elem) {
    data.inc_invalidate_guard(); }
  ~guard() { data.dec_invalidate_guard(); }
private:
  guard(const guard&);
  guard& operator =(const guard&);
};

template <typename Tc>
struct cguard {
  typedef typename Tc::data_type data_type;
  const Tc& data;
  const data_type& elem;
  const data_type& get() const { return elem; }
  cguard(const Tc& data, const data_type& elem) : data(data), elem(elem) {
    data.inc_invalidate_guard(); }
  ~cguard() { data.dec_invalidate_guard(); }
private:
  cguard(const cguard&);
  cguard& operator =(const cguard&);
};

template <typename Tc>
struct guardx {
  typedef typename Tc::data_type data_type;
  Tc& data;
  guardx(Tc& data) : data(data) { data.inc_invalidate_guard(); }
  ~guardx() { data.dec_invalidate_guard(); }
private:
  guardx(const guardx&);
  guardx& operator =(const guardx&);
};

template <typename Tc>
struct cguardx {
  typedef typename Tc::data_type data_type;
  const Tc& data;
  cguardx(const Tc& data) : data(data) { data.inc_invalidate_guard(); }
  ~cguardx() { data.dec_invalidate_guard(); }
private:
  cguardx(const cguardx&);
  cguardx& operator =(const cguardx&);
};

template <typename T>
struct vector {
  template <typename Tc> friend struct guard;
  template <typename Tc> friend struct cguard;
  template <typename Tc> friend struct guardx;
  template <typename Tc> friend struct cguardx;
  typedef T data_type;
  typedef typename std::vector<T>::size_type size_type;
  typedef typename std::vector<T>::iterator iterator;
  typedef typename std::vector<T>::const_iterator const_iterator;
  vector() : invalidate_guard_count(0) { }
  vector(const vector& x) : v(x.v), invalidate_guard_count(0) { }
  vector& operator =(const vector& x) {
    check_resize();
    v = x.v;
    return *this;
  }
  size_type size() const { return v.size(); }
  bool empty() const { return v.empty(); }
  T& operator [](size_type idx) {
    if (idx >= v.size()) { throw_invalid_index(); }
    return v[idx];
  }
  const T& operator [](size_type idx) const {
    if (idx >= v.size()) { throw_invalid_index(); }
    return v[idx];
  }
  void resize(size_type sz) {
    check_resize();
    v.resize(sz);
  }
  void clear() {
    check_resize();
    v.clear();
  }
  void push_back(const T& x) {
    check_resize();
    v.push_back(x);
  }
  T pop_back() {
    check_resize();
    if (v.empty()) { throw_invalid_index(); }
    const T r = v.back();
    v.pop_back();
    return r;
  }
  iterator begin() { return v.begin(); }
  const_iterator begin() const { return v.begin(); }
  iterator end() { return v.end(); }
  const_iterator end() const { return v.end(); }
private:
  void check_resize() {
    if (invalidate_guard_count != 0) { throw_would_invalidate(); }
    // printf("chk cnt=%lu\n", invalidate_guard_count);
  }
  void inc_invalidate_guard() const {
    ++invalidate_guard_count;
    // printf("inc cnt=%lu\n", invalidate_guard_count);
  }
  void dec_invalidate_guard() const {
    --invalidate_guard_count;
    // printf("dec cnt=%lu\n", invalidate_guard_count);
  }
private:
  std::vector<T> v;
  mutable size_t invalidate_guard_count;
};

template <typename T, size_t len>
struct farray {
  typedef size_t size_type;
  size_type size() const { return len; }
  T& operator [](size_type idx) {
    if (idx >= len) { throw_invalid_index(); }
    return v[idx];
  }
private:
  T v[len];
};

template <typename Tk, typename Tm>
struct map {
  template <typename Tc> friend struct guard;
  template <typename Tc> friend struct cguard;
  template <typename Tc> friend struct guardx;
  template <typename Tc> friend struct cguardx;
  typedef Tm data_type;
  typedef typename std::map<Tk, Tm>::size_type size_type;
  typedef typename std::map<Tk, Tm>::iterator iterator;
  typedef typename std::map<Tk, Tm>::const_iterator const_iterator;
  map() : invalidate_guard_count(0) { }
  map(const map& x) : v(x.v), invalidate_guard_count(0) { }
  map& operator =(const map& x) {
    check_resize();
    v = x.v;
    return *this;
  }
  Tm& operator [](const Tk& k) { return v[k]; }
  size_type size() const { return v.size(); }
  void erase(const Tk& k) {
    check_resize();
    v.erase(k);
  }
  void clear() {
    check_resize();
    v.clear();
  }
  iterator begin() { return v.begin(); }
  const_iterator begin() const { return v.begin(); }
  iterator end() { return v.end(); }
  const_iterator end() const { return v.end(); }
private:
  void check_resize() {
    if (invalidate_guard_count != 0) { throw_would_invalidate(); }
  }
  void inc_invalidate_guard() const { ++invalidate_guard_count; }
  void dec_invalidate_guard() const { --invalidate_guard_count; }
private:
  std::map<Tk, Tm> v;
  mutable size_t invalidate_guard_count;
};

template <typename T>
struct rcval {
  rcval() : count$z(1), value$z() {
    DBG_RC(fprintf(stdout, "c1 %p\n", this));
  }
  explicit rcval(const T& v) : count$z(1), value$z(v) {
    DBG_RC(fprintf(stdout, "c2 %p\n", this));
  }
  void incref$z() const {
    DBG_RC(fprintf(stdout, "%p %zu +1\n", this, count$z));
    ++count$z;
  }
  void decref$z() const {
    DBG_RC(fprintf(stdout, "%p %zu -1\n", this, count$z));
    if (--count$z == 0) {
      delete this;
      DBG_RC(fprintf(stdout, "d  %p\n", this));
    }
  }
private:
  mutable size_t count$z;
public:
  T value$z;
private:
  rcval(const rcval&);
  rcval& operator =(const rcval&);
};

template <typename T>
struct rcptr { /* non-const pointer to T */
  typedef rcval<T> *rawptr;
  rcptr() : ptr(0) { }
  rcptr(const rcptr& x) : ptr(x.ptr) { if (ptr) ptr->incref$z(); }
  rcptr(rcval<T> *x) : ptr(x) { if (ptr) ptr->incref$z(); }
  explicit rcptr(const T& x) : ptr(new rcval<T>(x)) { }
  ~rcptr() { if (ptr) ptr->decref$z(); }
  rcptr& operator =(const rcptr& x) { return set(x.ptr); }
  rcptr& operator =(rcval<T> *x) { return set(x); }
  operator rcval<T> *() const { return ptr; }
  /* don't follow 'value$z' */
  rcval<T> *get() const { return ptr; }
  rcval<T>& operator *() const { return *ptr; }
  rcval<T> *operator ->() const { return ptr; }
private:
  rcval<T> *ptr;
private:
  rcptr& set(rcval<T> *x) {
    if (x != ptr) {
      if (x) { x->incref$z(); }
      if (ptr) { ptr->decref$z(); }
      ptr = x;
    }
    return *this;
  }
};

template <typename T>
struct crcptr { /* const pointer to T */
  typedef const rcval<T> *rawptr;
  crcptr() : ptr(0) { }
  crcptr(const crcptr& x) : ptr(x.ptr) { if (ptr) ptr->incref$z(); }
  crcptr(const rcptr<T>& x) : ptr(x.get()) { if (ptr) ptr->incref$z(); }
    /* implicit conv required */
  crcptr(const rcval<T> *x) : ptr(x) { if (ptr) ptr->incref$z(); }
  explicit crcptr(const T& x) : ptr(new rcval<T>(x)) { }
  ~crcptr() { if (ptr) ptr->decref$z(); }
  crcptr& operator =(const crcptr& x) { return set(x.ptr); }
  crcptr& operator =(const rcval<T> *x) { return set(x); }
  crcptr& operator =(const rcptr<T>& x) { return set(x.ptr); }
  operator const rcval<T> *() const { return ptr; }
  rcval<T> *get() const { return ptr; }
  /* does not follow 'value$' */
  const rcval<T>& operator *() const { return *ptr; }
  const rcval<T> *operator ->() const { return ptr; }
private:
  const rcval<T> *ptr;
private:
  crcptr& set(rcval<T> *x) {
    if (x != ptr) {
      if (x) x->incref$z();
      if (ptr) ptr->decref$z();
      ptr = x;
    }
    return *this;
  }
};

template <typename T>
struct ircptr { /* T must have an intrusive count */
  typedef T *rawptr;
  template <typename Tx> friend struct ircptr;
  ircptr() : ptr(0) { }
  ircptr(const ircptr& x) : ptr(x.ptr) { if (ptr) ptr->incref$z(); }
  ircptr(T *x) : ptr(x) { if (ptr) ptr->incref$z(); }
  /* ref{foo} to ref{ifoo} */
  template <typename Tx> ircptr(const ircptr<Tx>& x) : ptr(x.get()) {
    if (ptr) ptr->incref$z();
  }
  /* foo to ref{ifoo} */
  template <typename Tx> explicit ircptr(const Tx& x) : ptr(new Tx(x)) { }
  ~ircptr() { if (ptr) ptr->decref$z(); }
  ircptr& operator =(T *x) { return set(x); }
  template <typename Tx> ircptr& operator =(Tx *x) { return set(x); }
  ircptr& operator =(const ircptr& x) { return set(x.ptr); }
  template <typename Tx> ircptr& operator =(const ircptr<Tx>& x) {
    return set(x.ptr);
  }
  operator T *() const { return ptr; }
  T *get() const { return ptr; }
  const T& operator *() const { return *ptr; }
  T& operator *() { return *ptr; }
  const T *operator ->() const { return ptr; }
  T *operator ->() { return ptr; }
private:
  T *ptr;
private:
  ircptr& set(T *x) {
    if (x != ptr) {
      if (x) x->incref$z();
      if (ptr) ptr->decref$z();
      ptr = x;
    }
    return *this;
  }
};

string void_to_string(void);
string unit_to_string(unit v);
string bool_to_string(bool v);
string uchar_to_string(unsigned char v);
string char_to_string(char v);
string ushort_to_string(unsigned short v);
string short_to_string(short v);
string uint_to_string(unsigned int v);
string int_to_string(int v);
string ulong_to_string(unsigned long long v);
string long_to_string(long long v);
string size_t_to_string(size_t v);
string double_to_string(double v);
string float_to_string(float v);

void string_to_void(const string& v);
unit string_to_unit(const string& v);
bool string_to_bool(const string& v);
unsigned char string_to_uchar(const string& v);
char string_to_char(const string& v);
unsigned short string_to_ushort(const string& v);
short string_to_short(const string& v);
unsigned int string_to_uint(const string& v);
int string_to_int(const string& v);
unsigned long long string_to_ulong(const string& v);
long long string_to_long(const string& v);
size_t string_to_size_t(const string& v);
double string_to_double(const string& v);
float string_to_float(const string& v);

inline size_t string_size(const std::string& s)
  { return s.size(); }
inline bool string_empty(const std::string& s)
  { return s.empty(); }
inline void string_resize(std::string& s, unsigned long long len)
  { s.resize(len); }
inline void string_clear(std::string& s)
  { s.clear(); }
inline void string_append(std::string& s, const std::string& x)
  { s += x; }
inline void string_prepend(std::string& s, const std::string& x)
  { s.insert(s.begin(), x.begin(), x.end()); }

struct invalid_index : public std::logic_error { invalid_index(); };
struct invalid_field : public std::logic_error { invalid_field(); };
struct null_dereference : public std::logic_error { null_dereference(); };
struct would_invalidate : public std::logic_error { would_invalidate(); };

extern vector<string> pxc_argv;

}; // namespace pxcrt

#endif

