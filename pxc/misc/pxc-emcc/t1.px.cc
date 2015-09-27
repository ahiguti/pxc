#define PXC_PROFILE_cflags "-g -O3 -DNDEBUG -Wall -Wno-unused -Wno-strict-aliasing -Wno-free-nonheap-object -I/usr/local/include"
#define PXC_PROFILE_cxx "g++ --std=c++11"
#define PXC_PROFILE_incdir "/usr/local/share/pxc/pxc_%{platform}/:/usr/local/share/pxc/pxc_core/:/usr/local/share/pxc/pxc_ext/:/usr/share/pxc/pxc_%{platform}/:/usr/share/pxc/pxc_core/:/usr/share/pxc/pxc_ext/:."
#define PXC_PROFILE_platform "Darwin"
#define PXC_PROFILE_safe_mode "1"
/* type definitions */
/* inline */

#if defined(_MSC_VER)
#define PXC_MSVC
#define PXC_WINDOWS
#else
#define PXC_POSIX
#endif

#if defined(__EMSCRIPTEN__)
#define PXC_EMSCRIPTEN
#define PXC_NO_ATOMIC
#endif

#if PXC_PROFILE_noexceptions
#define PXC_NOEXCEPTIONS 1
#define PXC_THROW(x) ::abort();
#define PXC_TRY
#define PXC_CATCH(x) if(0)
#define PXC_NOTHROW throw()
#define PXC_RETHROW
#define BOOST_NO_EXCEPTIONS
#else
#define PXC_THROW(x) throw x;
#define PXC_TRY try
#define PXC_CATCH(x) catch(x)
#define PXC_NOTHROW throw()
#define PXC_RETHROW throw
#endif

#if defined(PXC_MSVC)
#pragma warning(disable: 4521 4522 4146)
#define SSIZE_MAX _I64_MAX
#include <windows.h>
#include <malloc.h>
#undef min
#undef max
#define alloca _alloca
#if _MSC_VER < 1900
#define snprintf _snprintf
#endif
#endif

#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <climits>
#include <stdexcept>
#ifndef PXC_NO_ATOMIC
#include <atomic>
#endif

#ifdef PXC_POSIX
#include <alloca.h>
#endif

#if defined(PXC_MSVC)
#define __attribute__(x)
#define PXC_NO_INLINE __declspec(noinline)
#define PXC_FORCE_INLINE __forceinline
#else
#define PXC_NO_INLINE __attribute__((noinline))
#define PXC_FORCE_INLINE inline
#endif

#ifdef BOOST_NO_EXCEPTIONS
namespace boost {
template <typename T> void throw_exception(T const& x) { ::abort(); }
};
#endif

#include <boost/type_traits/is_pod.hpp>
#include <boost/static_assert.hpp>

namespace pxcrt {

struct bt_unit { };
extern bt_unit unit_value; /* used when a reference is required */
extern char **process_argv; /* set by int main(int argc, char **argv) */
typedef bool bt_bool;
typedef unsigned char bt_uchar;
typedef char bt_char;
typedef signed char bt_schar;
typedef unsigned short bt_ushort;
typedef short bt_short;
typedef unsigned int bt_uint;
typedef int bt_int;
typedef unsigned long bt_ulong;
typedef long bt_long;
typedef unsigned long long bt_ulonglong;
typedef long long bt_longlong;
typedef ::size_t bt_size_t;
#if defined(PXC_MSVC)
#include <BaseTsd.h>
typedef SSIZE_T bt_ssize_t;
#else
typedef ::ssize_t bt_ssize_t;
#endif
typedef float bt_float;
typedef double bt_double;
struct bt_tpdummy;

struct mtcount { /* reference counter for multi-threaded shared objects */
#if defined(PXC_MSVC)
  mtcount(long x = 1) { std::atomic_init(&count$z, 1); }
  mtcount(const mtcount& x) { std::atomic_init(&count$z, 1); }
#else
  mtcount(long x = 1) : count$z(1) { }
  mtcount(const mtcount& x) : count$z(1) { }
#endif
  mtcount& operator =(const mtcount& x) { return *this; }
#ifdef PXC_NO_ATOMIC
  void incref$z() {
    ++count$z;
  }
  bool decref$z() {
    return (--count$z == 0);
  }
  long refcnt$z() const { return count$z; }
private:
  long count$z;
#else
  void incref$z() {
    std::atomic_fetch_add_explicit(&count$z, 1L, std::memory_order_relaxed);
  }
  bool decref$z() {
    if (std::atomic_fetch_sub_explicit(&count$z, 1L, std::memory_order_release)
      == 1) {
      std::atomic_thread_fence(std::memory_order_acquire);
      return true;
    }
    return false;
  }
  long refcnt$z() const { return count$z.load(std::memory_order_seq_cst); }
private:
  std::atomic_long count$z;
#endif
};

struct stcount { /* reference counter for single-threaded shared objects */
  stcount(long x = 1) : count$z(1) { }
  stcount(const stcount& x) : count$z(1) { }
  stcount& operator =(const stcount& x) { return *this; }
  void incref$z() { ++count$z; }
  bool decref$z() { return (--count$z == 0); }
  long refcnt$z() const { return count$z; }
private:
  long count$z;
};

}; // namespace pxcrt
// extern "types" inline
;
/* inline */

/* workaround for boost/cstdint.hpp:52 */
#ifndef __GLIBC_PREREQ
#define __GLIBC_PREREQ(x, y) 0
#endif
#include <boost/cast.hpp>

namespace numeric {
template <typename Tto, typename Tfrom> struct numeric_cast_impl {
  Tto convert(Tfrom const& x) const {
    return boost::numeric_cast<Tto>(x);
  }
};
template <typename Tto, typename Tfrom> struct static_cast_impl {
  Tto convert(Tfrom const& x) const {
    return static_cast<Tto>(x);
  }
};
}; // namespace numeric
;
/* inline */

#ifdef PXCRT_DBG_POOL
#define DBG_POOL(x) x
#else
#define DBG_POOL(x)
#endif

#define PXCRT_NO_LOCAL_POOL

#if defined(__APPLE__) && !defined(PXCRT_NO_LOCAL_POOL)
#define PXCRT_NO_LOCAL_POOL
#endif
#if defined(PXC_WINDOWS) && !defined(PXCRT_NO_LOCAL_POOL)
#define PXCRT_NO_LOCAL_POOL
#endif
#if defined(ANDROID) && !defined(PXCRT_NO_LOCAL_POOL)
#define PXCRT_NO_LOCAL_POOL
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <memory>

namespace pxcrt {

static inline void *global_allocate_n(size_t sz)
{
#if defined(_MSC_VER) && !defined(_WIN64)
  // TODO: use malloc instead
  void *const p = _aligned_malloc(sz, 16);
#else
  void *const p = malloc(sz);
#endif
  if (p == 0) {
    throw std::bad_alloc();
  }
  return p;
}

static inline void *global_reallocate_n(void *optr, size_t osz, size_t nsz)
{
#if defined(_MSC_VER) && !defined(_WIN64)
  void *const p = _aligned_realloc(optr, nsz, 16);
#else
  void *const p = realloc(optr, nsz);
#endif
  if (p == 0) {
    throw std::bad_alloc();
  }
  return p;
}

static inline void global_deallocate_n(void *ptr, size_t sz)
{
#if defined(_MSC_VER) && !defined(_WIN64)
  _aligned_free(ptr);
#else
  free(ptr);
#endif
}

void clear_local_pool();

#ifndef PXCRT_NO_LOCAL_POOL

/* thread-local allocator */

extern thread_local void *local_pool_blocks;

enum {
  local_pool_block_size = 2000,
  local_pool_block_alloc_size = local_pool_block_size + sizeof(void *)
};

template <size_t sz> struct local_pool
{
  static void *allocate() {
    if (free_list != 0) {
      void *r = free_list;
      free_list = *(void **)free_list;
      DBG_POOL(fprintf(stderr, "a %zu %p\n", sz, r));
      return r;
    }
    extend();
    void *r = free_list;
    free_list = *(void **)free_list;
    DBG_POOL(fprintf(stderr, "A %zu %p\n", sz, r));
    return r;
  }
  static void deallocate(void *p) {
    DBG_POOL(fprintf(stderr, "d %zu %p\n", sz, p));
    append_free_list(p);
  }
  static void append_free_list(void *p) {
    *(void **)p = free_list;
    free_list = p;
  }
private:
  static void extend() {
    DBG_POOL(fprintf(stderr, "X %zu\n", sz));
    char *b = (char *)global_allocate_n(local_pool_block_alloc_size);
    *(void **)(b) = local_pool_blocks;
    local_pool_blocks = b;
    b += sizeof(void *);
    size_t const bsz = local_pool_block_size / sz;
    for (size_t i = 0; i < bsz; ++i) {
      append_free_list(b + i * sz);
    }
  }
private:
  static thread_local void *free_list;
};

template <size_t sz> thread_local void *local_pool<sz>::free_list = 0;

template <bool is_small> struct local_pool_allocator;

template <size_t sz, size_t seg, bool fit> struct local_pool_segment_size;

template <size_t sz, size_t seg>
struct local_pool_segment_size<sz, seg, true> {
  enum { value = seg };
};

template <size_t sz, size_t seg>
struct local_pool_segment_size<sz, seg, false> {
  enum { nfit = (sz <= seg * 2) };
  enum { value = local_pool_segment_size<sz, seg * 2, nfit>::value };
};

template <> struct local_pool_allocator<true>
{
  template <size_t sz> static void *allocate() {
    /* 8, 16, 32, 64, ... */
    enum { pool_block_sz = local_pool_segment_size<sz, 4, false>::value };
    return local_pool<pool_block_sz>::allocate();
  }
  template <size_t sz> static void deallocate(void *ptr) {
    enum { pool_block_sz = local_pool_segment_size<sz, 4, false>::value };
    return local_pool<pool_block_sz>::deallocate(ptr);
  }
};

template <> struct local_pool_allocator<false>
{
  template <size_t sz> static void *allocate() {
    return global_allocate_n(sz);
  }
  template <size_t sz> static void deallocate(void *ptr) {
    global_deallocate_n(ptr, sz);
  }
};

template <typename T> static inline T *local_allocate()
{
  enum { is_small = sizeof(T) <= 64 ? true : false };
  return static_cast<T *>(
    local_pool_allocator<is_small>::template allocate<sizeof(T)>());
}

template <typename T> static inline void local_deallocate(const T *ptr)
{
  enum { is_small = sizeof(T) <= 64 ? true : false };
  local_pool_allocator<is_small>::template deallocate<sizeof(T)>(
    const_cast<T *>(ptr));
}

enum { use_local_threshold = 64 };

static inline void *local_allocate_n(size_t sz)
{
  if (sz <= use_local_threshold) {
    return local_pool_allocator<true>::allocate<use_local_threshold>();
  } else {
    return global_allocate_n(sz);
  }
}

static inline void *local_reallocate_n(void *optr, size_t osz, size_t nsz)
{
  if (osz <= use_local_threshold) {
    if (nsz <= use_local_threshold) {
      return optr;
    } else {
      void *nptr = global_allocate_n(nsz);
      memcpy(nptr, optr, osz);
      local_pool_allocator<true>::deallocate<use_local_threshold>(optr);
      return nptr;
    }
  } else {
    if (nsz <= use_local_threshold) {
      void *nptr = local_pool_allocator<true>::allocate<use_local_threshold>();
      memcpy(nptr, optr, nsz);
      global_deallocate_n(optr, osz);
      return nptr;
    } else {
      return global_reallocate_n(optr, osz, nsz);
    }
  }
}

static inline void local_deallocate_n(void *ptr, size_t sz)
{
  if (sz <= use_local_threshold) {
    local_pool_allocator<true>::deallocate<use_local_threshold>(ptr);
  } else {
    global_deallocate_n(ptr, sz);
  }
}

#else

template <typename T> static inline T *local_allocate()
{
  return static_cast<T *>(global_allocate_n(sizeof(T)));
}

template <typename T> static inline void local_deallocate(const T *ptr)
{
  return global_deallocate_n(const_cast<T *>(ptr), sizeof(T));
}

static inline void *local_allocate_n(size_t sz)
{
  return global_allocate_n(sz);
}

static inline void *local_reallocate_n(void *optr, size_t osz, size_t nsz)
{
  return global_reallocate_n(optr, osz, nsz);
}

static inline void local_deallocate_n(void *ptr, size_t sz)
{
  global_deallocate_n(ptr, sz);
}

#endif

template <typename T> static inline T *allocate_single()
{
  return reinterpret_cast<T *>(global_allocate_n(sizeof(T)));
}

template <typename T> static inline void deallocate_single(const T *ptr)
{
  global_deallocate_n(const_cast<T *>(ptr), sizeof(T));
}

}; // namespace pxcrt
;
/* inline */

#if 0
#define PXC_NO_BOUNDS_CHECKING 1
#else
#undef PXC_NO_BOUNDS_CHECKING
#endif

#include <algorithm>
#include <type_traits>

namespace pxcrt {

void throw_bad_alloc();
void throw_would_invalidate();
void throw_invalid_index();

/* alternative to uninitialized_* functions. optimized for pod types. */

template <typename T> struct is_trivially_copyable {
  enum { value = boost::is_pod<T>::value };
  // enum { value = std::is_trivially_copyable<T>::value };
};

template <typename T, bool copy_flag> struct placement_new;
template <typename T> struct placement_new<T, true>
{ void operator ()(T *dest, const T& src) { new (dest) T(src); }; };
template <typename T> struct placement_new<T, false>
{ void operator ()(T *dest, T& src) { new (dest) T(std::move(src)); }; };

template <typename T, bool copy_flag> struct copy_or_move;
template <typename T> struct copy_or_move<T, true>
{ void operator ()(T& dest, const T& src) { dest = src; }; };
template <typename T> struct copy_or_move<T, false>
{ void operator ()(T& dest, T& src) { dest = src; }; };

/*
template <typename T, bool copyable> struct add_const_if_not;
template <typename T> struct add_const_if_not<T, false>
{ typedef const T type; };
template <typename T> struct add_const_if_not<T, true>
{ typedef T type; };
*/

template <typename T> PXC_FORCE_INLINE void
deallocate_uninitialized(T *ptr, size_t len)
{
  global_deallocate_n(ptr, len * sizeof(T));
}

template <typename T> PXC_FORCE_INLINE T *
allocate_uninitialized(size_t len)
{
  void *p = global_allocate_n(len * sizeof(T));
  return static_cast<T *>(p);
}

template <typename T> PXC_FORCE_INLINE void
deinitialize_n(T *ptr, size_t len)
{
  if (!is_trivially_copyable<T>::value) {
    for (T *p = ptr + len; p != ptr; --p) {
      (p - 1)->T::~T();
    }
  }
}

template <typename T> PXC_FORCE_INLINE void
deinitialize_1(T *ptr)
{
  if (!is_trivially_copyable<T>::value) {
    ptr->T::~T();
  }
}

template <typename T, typename Tsrc> PXC_FORCE_INLINE void
initialize_fill_n(T *ptr, size_t len, Tsrc& x)
{
  if (is_trivially_copyable<T>::value) {
    for (size_t i = 0; i < len; ++i) {
      ptr[i] = x;
    }
  } else {
    size_t i = 0;
    PXC_TRY {
      for (; i < len; ++i) {
	/* new (ptr + i) T(x); */
	placement_new<T, std::is_const<Tsrc>::value>()(ptr + i, x);
      }
    } PXC_CATCH(...) {
      deinitialize_n(ptr, i);
      PXC_RETHROW;
    }
  }
}

template <typename T, typename Tsrc> PXC_FORCE_INLINE void
initialize_copymove_n(T *ptr, size_t len, Tsrc *src)
{
  if (is_trivially_copyable<T>::value) {
    memcpy(ptr, src, len * sizeof(T));
  } else {
    size_t i = 0;
    PXC_TRY {
      for (; i < len; ++i) {
	/* new (ptr + i) T(src[i]); */
	placement_new<T, std::is_const<Tsrc>::value>()(ptr + i, src[i]);
      }
    } PXC_CATCH(...) {
      deinitialize_n(ptr, i);
      PXC_RETHROW;
    }
  }
}

template <typename T, typename Tsrc> PXC_FORCE_INLINE void
initialize_1(T *ptr, Tsrc& x)
{
  if (is_trivially_copyable<T>::value) {
    *ptr = x;
  } else {
    /* new (ptr) T(x); */
    placement_new<T, std::is_const<Tsrc>::value>()(ptr, x);
  }
}

template <typename T> PXC_FORCE_INLINE T *
reserve_n(T *ptr, size_t vlen, size_t alloc_olen, size_t alloc_nlen)
{
  if (is_trivially_copyable<T>::value) {
    void *p = global_reallocate_n(ptr, alloc_olen * sizeof(T),
      alloc_nlen * sizeof(T));
    return static_cast<T *>(p);
  } else {
    T *p = allocate_uninitialized<T>(alloc_nlen);
    PXC_TRY {
      /* Tsrc is const if T is not move-constructible */
      /*
      typedef typename
	add_const_if_not<T, std::is_move_constructible<T>::value>::type Tsrc;
      initialize_copymove_n<T, Tsrc>(p, vlen, ptr);
      */
      initialize_copymove_n<T, T>(p, vlen, ptr);
    } PXC_CATCH(...) {
      deallocate_uninitialized<T>(p, alloc_nlen);
      PXC_RETHROW;
    }
    deinitialize_n(ptr, vlen);
    deallocate_uninitialized<T>(ptr, vlen);
    return p;
  }
}

template <typename T, typename Tsrc> PXC_FORCE_INLINE void
copymove_n(T *ptr, size_t len, Tsrc *src)
{
  if (is_trivially_copyable<T>::value) {
    memmove(ptr, src, len * sizeof(T));
  } else {
    for (size_t i = 0; i < len; ++i) {
      /* ptr[i] = src[i]; */
      copy_or_move<T, std::is_const<Tsrc>::value>()(ptr[i], src[i]);
    }
  }
}

template <typename T, typename Tsrc> PXC_FORCE_INLINE void
copymove_backward_n(T *ptr, size_t len, Tsrc *src)
{
  if (is_trivially_copyable<T>::value) {
    memmove(ptr, src, len * sizeof(T));
  } else {
    for (size_t i = len; i > 0; --i) {
      copy_or_move<T, std::is_const<Tsrc>::value>()(ptr[i - 1], src[i - 1]);
    }
  }
}

template <typename T, typename Tsrc> PXC_FORCE_INLINE void
insert_n(T *x, size_t& xlen, Tsrc *y, size_t ylen, size_t xpos)
  /* xlen is modified so that it is always the size of initialized range */
{
  /* x[0 .. xlen] must be initialized, and x[xlen .. xlen + ylen]
   * uninitialized */
  const size_t olen = xlen;
  const size_t nlen = olen + ylen;
  if (olen - xpos >= ylen) {
    /* moving size >= ylen */
    /* append from x */
    initialize_copymove_n<T, Tsrc>(x + xlen, ylen, x + olen - ylen);
    xlen = nlen;
    /* move */
    copymove_backward_n<T, Tsrc>(x + xpos + ylen, olen - ylen - xpos, x + xpos);
    /* copy from y */
    copymove_n(x + xpos, ylen, y);
  } else {
    /* moving size < ylen */
    /* append from y */
    initialize_copymove_n(x + xlen, ylen - (olen - xpos), y + (olen - xpos));
    xlen = xpos + ylen;
    /* append from x */
    initialize_copymove_n<T, Tsrc>(x + xlen, olen - xpos, x + xpos);
    xlen = nlen;
    /* copy from y */
    copymove_n(x + xpos, olen - xpos, y);
  }
}

/* utility functions for array types */

template <typename T> static PXC_FORCE_INLINE bt_size_t
array_find(const T *arr, bt_size_t len, bt_size_t offset, T const& value)
{
  if (offset >= len) {
    return len;
  }
  if (sizeof(T) == 1) {
    const T *const p = static_cast<const T *>(
      ::memchr(arr + offset, value, len - offset));
    return p != 0 ? (p - arr) : len;
  } else {
    bt_size_t i = offset;
    for (; i < len && arr[i] != value; ++i) { }
    return i;
  }
}

template <typename T> static PXC_FORCE_INLINE bt_size_t
find_mapped_memchr(const T& c, bt_size_t offset, typename T::mapped_type v)
{
  return array_find(c.rawarr(), c.size(), offset, v);
}

template <typename T> static PXC_FORCE_INLINE typename T::mapped_type
get_elem_value(const T& c, bt_size_t offset)
{
  return c[offset];
}

template <typename T> static PXC_FORCE_INLINE typename T::mapped_type
get_elem_value_nochek(const T& c, bt_size_t offset)
{
  return c.rawarr()[offset];
}

template <typename Tx, typename Ty> PXC_FORCE_INLINE int
compare_memcmp(const Tx& x, const Ty& y)
{
  BOOST_STATIC_ASSERT((
    sizeof(typename Tx::mapped_type) == sizeof(typename Ty::mapped_type)));
  const size_t xlen = x.size();
  const size_t ylen = y.size();
  const size_t clen = std::min(xlen, ylen);
  const int c = memcmp(x.rawarr(), y.rawarr(),
    clen * sizeof(typename Tx::mapped_type));
  if (c < 0) {
    return -1;
  } else if (c > 0) {
    return 1;
  }
  if (xlen < ylen) {
    return -1;
  } else if (xlen > ylen) {
    return 1;
  }
  return 0;
}

template <typename Tx, typename Ty> PXC_FORCE_INLINE bool
eq_memcmp(const Tx& x, const Ty& y)
{
  BOOST_STATIC_ASSERT((
    sizeof(typename Tx::mapped_type) == sizeof(typename Ty::mapped_type)));
  return x.size() == y.size() &&
    memcmp(x.rawarr(), y.rawarr(),
      x.size() * sizeof(typename Tx::mapped_type)) == 0;
}

template <typename Tx, typename Ty> PXC_FORCE_INLINE bool
lt_memcmp(const Tx& x, const Ty& y)
{
  BOOST_STATIC_ASSERT((
    sizeof(typename Tx::mapped_type) == sizeof(typename Ty::mapped_type)));
  const size_t xlen = x.size();
  const size_t ylen = y.size();
  const size_t clen = std::min(xlen, ylen);
  const int c = memcmp(x.rawarr(), y.rawarr(),
    clen * sizeof(typename Tx::mapped_type));
  if (c < 0) {
    return true;
  } else if (c == 0 && xlen < ylen) {
    return true;
  } else {
    return false;
  }
}

template <typename Tx, typename Ty> PXC_FORCE_INLINE bool
ne_memcmp(const Tx& x, const Ty& y) { return !eq_memcmp(x, y); }
template <typename Tx, typename Ty> PXC_FORCE_INLINE bool
gt_memcmp(const Tx& x, const Ty& y) { return lt_memcmp(y, x); }
template <typename Tx, typename Ty> PXC_FORCE_INLINE bool
le_memcmp(const Tx& x, const Ty& y) { return !lt_memcmp(y, x); }
template <typename Tx, typename Ty> PXC_FORCE_INLINE bool
ge_memcmp(const Tx& x, const Ty& y) { return !lt_memcmp(x, y); }

template <typename T> PXC_FORCE_INLINE size_t
hash_podarr(const T& x)
{
  typedef typename T::mapped_type mt;
  mt const* const p = x.rawarr();
  size_t n = x.size();
  size_t r = 0;
  for (size_t i = 0; i < n; ++i) {
    r ^= p[i] + 0x9e3779b9 + (r << 6) + (r >> 2);
  }
  return r;
}

template <typename Tx, typename Ty> PXC_FORCE_INLINE void
copy_memmove(const Tx& x, Ty& y)
{
  BOOST_STATIC_ASSERT((
    sizeof(typename Tx::mapped_type) == sizeof(typename Ty::mapped_type)));
  const size_t xlen = x.size();
  const size_t ylen = y.size();
  const size_t clen = std::min(xlen, ylen);
  memmove(y.rawarr(), x.rawarr(), clen * sizeof(typename Tx::mapped_type));
}

template <typename Tx, typename Tv> static PXC_FORCE_INLINE void
array_push_back(Tx& x, Tv const& v)
{ x.push_back(v); }

template <typename Tx, typename Tv> static PXC_FORCE_INLINE void
array_push_back_move(Tx& x, Tv& v)
{ x.push_back(std::move(v)); }

template <typename Tx> static PXC_FORCE_INLINE typename Tx::mapped_type
array_pop_back(Tx& x)
{ return x.pop_back(); }

template <typename Tx, typename Tv> static PXC_FORCE_INLINE void
array_push_front(Tx& x, Tv const& v)
{ x.push_front(v); }

template <typename Tx, typename Tv> static PXC_FORCE_INLINE void
array_push_front_move(Tx& x, Tv& v)
{ x.push_front(std::move(v)); }

template <typename Tx> static PXC_FORCE_INLINE typename Tx::mapped_type
array_pop_front(Tx& x)
{ return x.pop_front(); }

template <typename Tx, typename Tv> static PXC_FORCE_INLINE void
array_append(Tx& x, Tv const& v)
{ x.append(v); }

template <typename Tx, typename Tv> static PXC_FORCE_INLINE void
array_insert(Tx& x, bt_size_t i, Tv const& v)
{ x.insert(i, v); }

template <typename Tx> static PXC_FORCE_INLINE void
array_erase(Tx& x, bt_size_t i0, bt_size_t i1)
{ x.erase(i0, i1); }

}; // namespace pxcrt

;
/* inline */
namespace pxcrt {

struct refguard_base {
  refguard_base()
  #if ! 0
    : refguard_count(0)
  #endif
  { }
  template <typename Tc> struct guard_ref {
    guard_ref(Tc& x) : v(x) { v.inc_refguard(); }
    ~guard_ref() { v.dec_refguard(); }
    Tc& get() { return v; }
    typename Tc::range_type get_range()
      { return typename Tc::range_type(v); }
    typename Tc::crange_type get_crange()
      { return typename Tc::crange_type(v); }
  private:
    Tc& v;
    guard_ref(const guard_ref&);
    guard_ref& operator =(const guard_ref&);
  };
  template <typename Tc> struct guard_val {
    guard_val(const Tc& x) : v(x) { v.inc_refguard(); }
    ~guard_val() { v.dec_refguard(); }
    Tc& get() { return v; }
    typename Tc::range_type get_range()
      { return typename Tc::range_type(v); }
    typename Tc::crange_type get_crange()
      { return typename Tc::crange_type(v); }
  private:
    Tc v;
    guard_val(const guard_val&);
    guard_val& operator =(const guard_val&);
  };
  #if 0
  bool refguard_is_zero() const { return true; }
  void inc_refguard() const { }
  void dec_refguard() const { }
  void check_resize() { }
  #else
  bool refguard_is_zero() const { return refguard_count == 0; }
  void inc_refguard() const { ++refguard_count; }
  void dec_refguard() const { --refguard_count; }
  void check_resize() {
    if (refguard_count != 0) { pxcrt::throw_would_invalidate(); }
  }
  mutable size_t refguard_count;
  #endif
};

}; // namespace pxcrt
;
/* inline */
#include <limits>
#include <boost/type_traits.hpp>
namespace pxcrt {

template <typename T>
struct bt_cslice {
  typedef T mapped_type;
  bt_cslice() : v(0), len(0) { }
  bt_cslice(const T *ptr, bt_size_t o) : v(ptr), len(o) { }
  template <typename Tc> bt_cslice(const Tc& c)
    : v(c.rawarr()), len(c.size()) { }
  template <typename Tc> bt_cslice(const Tc& c, bt_size_t o1, bt_size_t o2) {
    if (o2 > c.size()) { o2 = c.size(); }
    if (o1 > o2) { o1 = o2; }
    v = c.rawarr() + o1;
    len = o2 - o1;
  }
  bt_bool empty() const { return len == 0; }
  bt_size_t size() const { return len; }
  bt_size_t find(bt_size_t offset, T const& value) const {
    return array_find(v, len, offset, value);
  }
  void increment_front(size_t i) {
    #ifndef PXC_NO_BOUNDS_CHECKING
    if (len < i) { pxcrt::throw_invalid_index(); }
    #endif
    v += i;
    len -= i;
  }
  void decrement_back(size_t i) {
    #ifndef PXC_NO_BOUNDS_CHECKING
    if (len < i) { pxcrt::throw_invalid_index(); }
    #endif
    len -= i;
  }
  bt_cslice<T> next_token(T const& delim) {
    bt_size_t const offset = array_find(v, len, 0, delim);
    bt_cslice<T> r(v, offset);
    if (offset < len) {
      v += offset + 1;
      len -= offset + 1;
    } else {
      v += len;
      len = 0;
    }
    return r;
  }
  bt_cslice<T> range() const {
    return bt_cslice<T>(*this);
  }
  bt_cslice<T> crange() const {
    return bt_cslice<T>(*this);
  }
  const T& operator [](bt_size_t idx) const {
    #ifndef PXC_NO_BOUNDS_CHECKING
    if (idx >= len) { pxcrt::throw_invalid_index(); }
    #endif
    return v[idx];
  }
  const T& operator *() const {
    #ifndef PXC_NO_BOUNDS_CHECKING
    if (len == 0) { pxcrt::throw_invalid_index(); }
    #endif
    return *v;
  }
  T value_at(bt_size_t idx) const {
    #ifndef PXC_NO_BOUNDS_CHECKING
    if (idx >= len) { pxcrt::throw_invalid_index(); }
    #endif
    return v[idx];
  }
  T deref_value() const {
    #ifndef PXC_NO_BOUNDS_CHECKING
    if (len == 0) { pxcrt::throw_invalid_index(); }
    #endif
    return *v;
  }
  const T *begin() const { return v; }
  const T *end() const { return v + len; }
  const T *rawarr() const { return v; }
//private:
//  bt_cslice& operator =(const bt_cslice&); /* not allowed */
protected:
  const T *v;
  bt_size_t len;
};

template <typename T>
struct bt_slice {
  typedef T mapped_type;
  bt_slice() : v(0), len(0) { }
  bt_slice(T *ptr, bt_size_t o) : v(ptr), len(o) { }
  template <typename Tc> bt_slice(Tc& c) : v(c.rawarr()), len(c.size()) { }
  template <typename Tc> bt_slice(Tc& c, bt_size_t o1, bt_size_t o2) {
    if (o2 > c.size()) { o2 = c.size(); }
    if (o1 > o2) { o1 = o2; }
    v = c.rawarr() + o1;
    len = o2 - o1;
  }
  bt_bool empty() const { return len == 0; }
  bt_size_t size() const { return len; }
  void set(bt_size_t i, T const& value) const { if (i < len) v[i] = value; }
  bt_size_t find(bt_size_t offset, T const& value) const {
    return array_find(v, len, offset, value);
  }
  void next() {
    #ifndef PXC_NO_BOUNDS_CHECKING
    if (len == 0) { pxcrt::throw_invalid_index(); }
    #endif
    ++v;
    --len;
  }
  void increment_front(size_t i) {
    #ifndef PXC_NO_BOUNDS_CHECKING
    if (len < i) { pxcrt::throw_invalid_index(); }
    #endif
    v += i;
    len -= i;
  }
  void decrement_back(size_t i) {
    #ifndef PXC_NO_BOUNDS_CHECKING
    if (len < i) { pxcrt::throw_invalid_index(); }
    #endif
    len -= i;
  }
  bt_slice<T> next_token(T const& delim) {
    bt_size_t const offset = array_find(v, len, 0, delim);
    bt_slice<T> r(v, offset);
    if (offset < len) {
      v += offset + 1;
      len -= offset + 1;
    } else {
      v += len;
      len = 0;
    }
    return r;
  }
  bt_slice<T> range() const {
    return bt_slice<T>(*this);
  }
  bt_cslice<T> crange() const {
    return bt_cslice<T>(*this);
  }
  T& operator [](bt_size_t idx) const {
    #ifndef PXC_NO_BOUNDS_CHECKING
    if (idx >= len) { pxcrt::throw_invalid_index(); }
    #endif
    return v[idx];
  }
  T& operator *() const {
    #ifndef PXC_NO_BOUNDS_CHECKING
    if (len == 0) { pxcrt::throw_invalid_index(); }
    #endif
    return *v;
  }
  T value_at(bt_size_t idx) const {
    #ifndef PXC_NO_BOUNDS_CHECKING
    if (idx >= len) { pxcrt::throw_invalid_index(); }
    #endif
    return v[idx];
  }
  T deref_value() const {
    #ifndef PXC_NO_BOUNDS_CHECKING
    if (len == 0) { pxcrt::throw_invalid_index(); }
    #endif
    return *v;
  }
  T *begin() const { return v; }
  T *end() const { return v + len; }
  T *rawarr() const { return v; }
//private:
//  bt_slice& operator =(const bt_slice&); /* not allowed */
protected:
  T *v;
  bt_size_t len;
};

struct bt_strlit : public bt_cslice<bt_uchar> {
  typedef bt_uchar mapped_type;
  typedef bt_uchar char_type;
  typedef size_t size_type;
  typedef const char_type *iterator;
  typedef const char_type *const_iterator;
  typedef bt_cslice<char_type> range_type;
  typedef bt_cslice<char_type> crange_type;
  bt_strlit() { }
  template <size_type n> bt_strlit(const char (& ptr)[n])
    : bt_cslice<bt_uchar>(reinterpret_cast<const char_type *>(ptr), n - 1) { }
  bt_size_t find(bt_size_t offset, char_type const& value) const {
    return array_find(v, len, offset, value);
  }
  bt_strlit& operator =(const bt_strlit& x) { /* allowed */
    v = x.v;
    len = x.len;
    return *this;
  }
  range_type range() {
    return range_type(*this);
  }
  crange_type crange() const {
    return crange_type(*this);
  }
};

template <typename T>
struct pxcvarray : public refguard_base {
  typedef T mapped_type;
  typedef pxcrt::bt_size_t size_type;
  typedef T *iterator;
  typedef const T *const_iterator;
  typedef pxcrt::bt_slice<T> range_type;
  typedef pxcrt::bt_cslice<T> crange_type;
  pxcvarray() : start(0), valid_len(0), alloc_len(0) { }
  ~pxcvarray() {
    deinitialize_n(start, valid_len);
    deallocate_uninitialized(start, alloc_len);
  }
  pxcvarray(const pxcvarray& x)
    { construct_internal(x.start, x.valid_len); }
  pxcvarray(pxcvarray&& x)
    { move_construct_internal(x); }
  pxcvarray(pxcvarray& x) {
    if (std::is_copy_constructible<T>::value) {
      construct_internal(x.start, x.valid_len);
    } else {
      move_construct_internal(x);
    }
  }
  pxcvarray(const crange_type& x)
    { construct_internal(x.rawarr(), x.size()); }
  pxcvarray(const range_type& x)
    { construct_internal(x.rawarr(), x.size()); }
  pxcvarray(const T *ptr, size_type len)
    { construct_internal(ptr, len); }
  pxcvarray& operator =(const pxcvarray& x) {
    check_resize();
    if (&x != this) {
      clear_internal();
      append_internal(x.start, x.valid_len);
    }
    return *this;
  }
  pxcvarray& operator =(pxcvarray&& x) {
    this->swap(x);
    return *this;
  }
  pxcvarray& operator =(pxcvarray& x) {
    if (std::is_copy_assignable<T>::value) {
      return *this = const_cast<const pxcvarray&>(x);
    } else {
      return *this = const_cast<pxcvarray&&>(x);
    }
  }
  bt_bool empty() const { return valid_len == 0; }
  size_type size() const { return valid_len; }
  bt_size_t find(bt_size_t offset, T const& value) const {
    return array_find(start, valid_len, offset, value);
  }
  T& operator [](size_type idx) {
    #ifndef PXC_NO_BOUNDS_CHECKING
    if (idx >= valid_len) { pxcrt::throw_invalid_index(); }
    #endif
    return start[idx];
  }
  const T& operator [](size_type idx) const {
    #ifndef PXC_NO_BOUNDS_CHECKING
    if (idx >= valid_len) { pxcrt::throw_invalid_index(); }
    #endif
    return start[idx];
  }
  void resize(size_type sz, T const& x) {
    check_resize();
    if (sz > valid_len) {
      reserve_internal(sz);
      initialize_fill_n(start + valid_len, sz - valid_len, x);
    } else {
      deinitialize_n(start + sz, valid_len - sz);
    }
    valid_len = sz;
  }
  void clear() {
    check_resize();
    clear_internal();
  }
  template <typename Tc> friend Tc&
  pxcvarray_push_back_uninitialized(pxcvarray<Tc>& v);
  template <typename Tc> friend void
  pxcvarray_rawarr_set_valid_len(pxcvarray<Tc>& v,
    typename pxcvarray<Tc>::size_type len);
  void push_back(T const& x) {
    push_back_internal(x);
  }
  void push_back(T&& x) {
    push_back_internal(x);
  }
  T pop_back() {
    check_resize();
    if (empty()) { pxcrt::throw_invalid_index(); }
    const T r = start[valid_len - 1];
    deinitialize_1(start + valid_len - 1);
    --valid_len;
    return r;
  }
  void reserve(size_type sz) {
    check_resize();
    reserve_internal(sz);
  }
  void append(const crange_type& x) {
    check_resize();
    append_internal(x.rawarr(), x.size());
  }
  void append(const T *ptr, size_type len) {
    check_resize();
    append_internal(ptr, len);
  }
  void insert(size_type pos, const crange_type& x) {
    check_resize();
    insert_internal(pos, x.rawarr(), x.size());
  }
  void erase(size_type first, size_type last) {
    check_resize();
    erase_internal(first, last);
  }
  void swap(pxcvarray& x) {
    check_resize();
    x.check_resize();
    std::swap(start, x.start);
    std::swap(valid_len, x.valid_len);
    std::swap(alloc_len, x.alloc_len);
  }
  bt_slice<T> range() {
    return bt_slice<T>(start, valid_len);
  }
  bt_cslice<T> crange() const {
    return bt_cslice<T>(start, valid_len);
  }
  iterator begin() { return start; }
  const_iterator begin() const { return start; }
  iterator end() { return start + valid_len; }
  const_iterator end() const { return start + valid_len; }
  T *rawarr() { return start; }
  const T *rawarr() const { return start; }
  template <size_t mul> T *reserve_back(size_type len) {
    check_resize();
    if (len >= ::std::numeric_limits<size_type>::max() / mul) {
      pxcrt::throw_bad_alloc(); /* overflow */
    }
    const size_type osz = valid_len;
    const size_type nsz = osz + len * mul;
    if (nsz < osz) {
      pxcrt::throw_bad_alloc(); /* overflow */
    }
    reserve_internal(nsz);
    return start + osz;
  }
  void rawarr_set_valid_len(size_type len) {
    valid_len = len;
  }
private:
  void construct_internal(const T *ptr, size_type len) {
    alloc_len = valid_len = len;
    start = allocate_uninitialized<T>(alloc_len);
    if (is_trivially_copyable<T>::value) {
      initialize_copymove_n(start, valid_len, ptr);
    } else {
      PXC_TRY {
	initialize_copymove_n(start, valid_len, ptr);
      } PXC_CATCH(...) {
	deallocate_uninitialized(start, alloc_len);
	PXC_RETHROW;
      }
    }
  }
  void move_construct_internal(pxcvarray& x) {
    x.check_resize();
    start = x.start;
    valid_len = x.valid_len;
    alloc_len = x.alloc_len;
    x.start = 0;
    x.valid_len = 0;
    x.alloc_len = 0;
  }
  void clear_internal() {
    deinitialize_n(start, valid_len);
    valid_len = 0;
  }
  template <typename Tx> void push_back_internal(Tx& x) {
    /* re-ordered for speed */
    if (refguard_is_zero()) {
      /* valid_len + 1 never overflows */
      if (valid_len + 1 < alloc_len) {
	/* fast path */
	initialize_1(start + valid_len, x);
	valid_len++;
	return;
      } else {
	/* slow path */
	reserve_internal(valid_len + 1);
	initialize_1(start + valid_len, x);
	valid_len++;
	return;
      }
    } else {
      pxcrt::throw_would_invalidate();
    }
  }
  void append_internal(const T *p, size_type len) {
    size_type nlen = valid_len + len;
    if (nlen < valid_len) {
      pxcrt::throw_bad_alloc(); /* overflow */
    }
    reserve_internal(nlen);
    initialize_copymove_n(start + valid_len, len, p);
    valid_len = nlen;
  }
  void insert_internal(size_type pos, const T *p, size_type plen) {
    if (plen == 0) {
      return;
    }
    if (pos > valid_len) {
      pxcrt::throw_invalid_index();
    }
    const size_type vlen = valid_len;
    const size_type nlen = vlen + plen;
    if (nlen < vlen) {
      pxcrt::throw_bad_alloc(); /* overflow */
    }
    reserve_internal(nlen);
    insert_n(start, valid_len /* modified */, p, plen, pos);
  }
  void erase_internal(size_type first, size_type last) {
    if (last > valid_len || first > last) {
      pxcrt::throw_invalid_index();
    }
    if (first >= last) {
      return;
    }
    const size_type elen = last - first;
    copymove_n(start + first, valid_len - last, start + last);
    deinitialize_n(start + valid_len - elen, elen);
    valid_len -= elen;
  }
  void reserve_internal(size_type len) {
    if (len <= alloc_len) {
      return;
    }
    size_type nl = alloc_len;
    do {
      const size_type nl_n = (nl >= 1) ? (nl * 2) : 1;
      const size_type max_len = ((size_t)-1) / sizeof(T);
      if (nl_n <= nl || nl_n > max_len) {
        PXC_THROW(std::bad_alloc()); /* overflow */
      }
      nl = nl_n;
    } while (nl < len);
    start = reserve_n(start, valid_len, alloc_len, nl);
    alloc_len = nl;
  }
private:
  T *start;
  size_type valid_len;
  size_type alloc_len;
};

template <typename T> static inline void
pxcvarray_swap(pxcvarray<T>& x, pxcvarray<T>& y)
{
  x.swap(y);
}

template <typename T> static inline T&
pxcvarray_push_back_uninitialized(pxcvarray<T>& v)
{
  /* re-ordered for speed */
  if (v.refguard_count == 0) {
    /* valid_len + 1 never overflows */
    if (v.valid_len + 1 < v.alloc_len) {
      /* fast path */
      return v.start[v.valid_len];
    } else {
      /* slow path */
      v.reserve_internal(v.valid_len + 1);
      return v.start[v.valid_len];
    }
  } else {
    pxcrt::throw_would_invalidate();
  }
}

template <typename T> static inline void
pxcvarray_rawarr_set_valid_len(pxcvarray<T>& v,
  typename pxcvarray<T>::size_type len)
{
  v.valid_len = len;
}

typedef pxcvarray<bt_uchar> bt_string;
typedef bt_slice<bt_uchar> strref;
typedef bt_cslice<bt_uchar> cstrref;

template <typename Tx, typename Ty> static inline bt_bool
pod_array_eq(Tx const& x, Ty const& y)
{ return eq_memcmp(x, y); }
template <typename Tx, typename Ty> static inline bt_int
pod_array_compare(Tx const& x, Ty const& y)
{ return compare_memcmp(x, y); }
template <typename T> static inline bt_size_t
pod_array_hash(T const& x)
{ return hash_podarr(x); }
template <typename Tx, typename Ty> static inline void
pod_array_copy(Tx const& x, Ty const& y)
{ return copy_memmove(x, y); }

static inline bt_string cstr_to_string(const char *cstr)
{
  return bt_string(
    reinterpret_cast<const unsigned char *>(cstr),
    strlen(cstr));
}

template <typename Tarr, typename Tv> static inline void
array_resize(Tarr& arr, bt_size_t i, Tv const& v)
{
  arr.resize(i, v);
}

}; // namespace pxcrt
;
/* inline */

#include <algorithm>
#include <limits>

#define PXCRT_DBG_SVECTOR1(x)
#define PXCRT_DBG_SVECTOR2(x)

namespace pxcrt {

struct svarray_rep {
  /* must be a pod so that empty_rep is initialized wo constructor call */
  size_t refcnt;
  size_t valid_len;
  size_t alloc_len;
};

extern svarray_rep empty_rep; /* for empty value */

template <typename T>
struct svarray {
  typedef T mapped_type;
  typedef pxcrt::bt_size_t size_type;
  typedef T *iterator;
  typedef const T *const_iterator;
  typedef pxcrt::bt_slice<T> range_type;
  typedef pxcrt::bt_cslice<T> crange_type;
  svarray() : rep(&empty_rep) { }
  svarray(const svarray& x) : rep(x.rep) { rep_incref(rep); }
  svarray(const crange_type& x) {
    rep_init(x.begin(), x.size(), x.size());
    rep_zerofill();
  }
  svarray(const range_type& x) {
    rep_init(x.begin(), x.size(), x.size());
    rep_zerofill();
  }
  svarray(const T *start, size_type sz, size_type asz) {
    rep_init(start, sz, asz);
    rep_zerofill();
  }
  void operator =(const svarray& x) {
    if (&x != this) {
      rep_incref(x.rep);
      rep_decref(rep);
      rep = x.rep;
    }
  }
  ~svarray() { rep_decref(rep); }
  bt_bool empty() const { return size() == 0; }
  size_type size() const { return rep->valid_len; }
  size_type find(bt_size_t offset, T const& value) const {
    return array_find(begin(), size(), offset, value);
  }
  T& operator [](size_type idx) {
    if (idx >= size()) { pxcrt::throw_invalid_index(); }
    return begin()[idx];
  }
  const T& operator [](size_type idx) const {
    if (idx >= size()) { pxcrt::throw_invalid_index(); }
    return begin()[idx];
  }
  void resize(size_type sz, T const& x) {
    if (sz == 0) {
      clear();
      return;
    }
    size_type const olen = size();
    rep_make_unique(sz);
    if (sz > olen) {
      initialize_fill_n(end(), sz - olen, x);
      rep->valid_len = sz;
      rep_zerofill();
    }
  }
  void clear() {
    rep_decref(rep);
    rep = &empty_rep;
  }
  void swap(svarray& x) {
    svarray_rep *orep = rep;
    rep = x.rep;
    x.rep = orep;
  }
  void push_back(T const& x) {
    if (rep->refcnt == 1 && rep->valid_len < rep->alloc_len) {
      /* fast path */
      PXCRT_DBG_SVECTOR2(fprintf(stderr, "fastpath\n"));
      initialize_1(end(), x);
      rep->valid_len += 1;
      rep_zerofill();
      return;
    } else {
      /* slow path */
      PXCRT_DBG_SVECTOR2(fprintf(stderr, "slowpath %zu %zu %zu\n", rep->refcnt,
	rep->valid_len, rep->alloc_len));
      rep_make_unique(size() + 1);
      initialize_1(end(), x);
      rep->valid_len += 1;
      rep_zerofill();
    }
  }
  T pop_back() {
    if (empty()) { pxcrt::throw_invalid_index(); }
    const T r = end()[-1];
    size_type nlen = size() - 1;
    if (nlen == 0) {
      clear();
      return r;
    }
    const bool is_uniq = rep->refcnt == 1;
    rep_make_unique(nlen);
    if (is_trivially_copyable<T>::value && sizeof(T) == 1) {
      if (is_uniq) {
	assert(rep->valid_len < rep->alloc_len);
	*end() = T(); /* FIXME: requires defcon */
      } else {
	/* new copy */
	rep_zerofill();
      }
    }
    return r;
  }
  void reserve(size_type sz) {
    rep_make_unique(std::max(size(), sz));
    rep_zerofill();
  }
  void append(const crange_type& x) {
    append(x.begin(), x.size());
  }
  void append(const T *ptr, size_type len) {
    size_type const olen = size();
    size_type const nlen = olen + len;
    if (nlen < olen) { pxcrt::throw_bad_alloc(); } /* overflow */
    rep_make_unique(nlen);
    initialize_copymove_n(begin() + olen, len, ptr);
    rep->valid_len = nlen;
    rep_zerofill();
  }
  void insert(size_type pos, const crange_type& x) {
    size_type const olen = size();
    size_type const plen = x.size();
    size_type const nlen = olen + plen;
    if (pos > olen) { pxcrt::throw_invalid_index(); }
    if (nlen < olen) { pxcrt::throw_bad_alloc(); } /* overflow */
    rep_make_unique(nlen);
    if (plen == 0) { return; }
    insert_n(begin(), rep->valid_len /* modified */, x.begin(), plen, pos);
    rep_zerofill();
  }
  void erase(size_type first, size_type last) {
    size_type const olen = size();
    if (last > olen || first > last) { pxcrt::throw_invalid_index(); }
    if (first >= last) { return; }
    size_type const elen = last - first;
    if (rep->refcnt == 1) {
      /* already unique. erase in-place */
      T *const start = begin();
      copymove_n(start + first, olen - last, start + last);
      deinitialize_n(start + olen - elen, elen);
      rep->valid_len -= elen;
    } else {
      /* create a copy */
      svarray x(begin(), first, olen - elen);
      x.append(begin() + last, olen - last);
      this->swap(x);
    }
    rep_zerofill();
  }
  bt_slice<T> range() {
    return bt_slice<T>(begin(), size());
  }
  bt_cslice<T> crange() const {
    return bt_cslice<T>(begin(), size());
  }
  const_iterator begin() const {
    return reinterpret_cast<const T *>(
      reinterpret_cast<const char *>(rep) + sizeof(svarray_rep));
  }
  const_iterator end() const {
    return begin() + size();
  }
  iterator begin() {
    return reinterpret_cast<T *>(
      reinterpret_cast<char *>(rep) + sizeof(svarray_rep));
  }
  iterator end() {
    return begin() + size();
  }
  T *rawarr() { return begin(); }
  const T *rawarr() const { return begin(); }
  template <size_t mul> T *reserve_back(size_type len) {
    if (len >= ((size_t)-1) / mul) {
      pxcrt::throw_bad_alloc(); /* overflow */
    }
    const size_type osz = size();
    const size_type nsz = osz + len * mul;
    if (nsz < osz) {
      pxcrt::throw_bad_alloc(); /* overflow */
    }
    rep_make_unique(nsz);
    rep_zerofill();
    return begin() + osz;
  }
  void rawarr_set_valid_len(size_type len) {
    assert(rep->refcnt == 1);
    rep->valid_len = len;
    rep_zerofill();
  }
  template <typename Tc> struct guard_ref {
    guard_ref(Tc& x) : v(x) { }
    Tc& get() { return v; }
    typename Tc::range_type get_range()
      { return typename Tc::range_type(v); }
    typename Tc::crange_type get_crange()
      { return typename Tc::crange_type(v); }
  private:
    Tc v; /* shares rep */
    guard_ref(const guard_ref&);
    guard_ref& operator =(const guard_ref&);
  };
  template <typename Tc> struct guard_val {
    guard_val(const Tc& x) : v(x) { }
    Tc& get() { return v; }
    typename Tc::range_type get_range()
      { return typename Tc::range_type(v); }
    typename Tc::crange_type get_crange()
      { return typename Tc::crange_type(v); }
  private:
    Tc v;
    guard_val(const guard_val&);
    guard_val& operator =(const guard_val&);
  };
private:
  svarray_rep *rep;
private:
  static inline void rep_incref(svarray_rep *rep) {
    if (rep->refcnt != 0) {
      PXCRT_DBG_SVECTOR1(fprintf(stderr, "rep_decref incref %p %zu\n", rep,
	rep->refcnt + 1));
      ++rep->refcnt;
    }
  }
  static inline void rep_decref(svarray_rep *rep) {
    if (rep->refcnt != 0) {
      PXCRT_DBG_SVECTOR1(fprintf(stderr, "rep_decref decref %p %zu\n", rep,
	rep->refcnt - 1));
      if (--rep->refcnt == 0) {
	T *const p = reinterpret_cast<T *>(
	  reinterpret_cast<char *>(rep) + sizeof(svarray_rep));
	deinitialize_n(p, rep->valid_len);
	PXCRT_DBG_SVECTOR1(fprintf(stderr, "rep_decref dealloc %p\n", rep));
	global_deallocate_n(rep,
	  sizeof(svarray_rep) + sizeof(T) * rep->alloc_len);
      }
    }
  }
  void rep_zerofill() {
    if (is_trivially_copyable<T>::value && sizeof(T) == 1) {
      char *p = reinterpret_cast<char *>(begin());
      size_t pos = rep->valid_len;
      const size_t mask = sizeof(size_t) - 1;
      const size_t epos = (pos + mask) & ~mask;
      assert(epos <= rep->alloc_len);
      if (epos != pos) {
	memset(p + pos, 0, epos - pos);
      }
    }
  }
  void rep_init(const T *ptr, size_t sz, size_t asz) {
    rep = &empty_rep;
    assert(rep->refcnt == 0 && rep->valid_len == 0 && rep->alloc_len == 0);
    if (asz != 0) {
      const size_type max_len =
	(- sizeof(svarray_rep) - sizeof(size_t)) / sizeof(T);
      if (asz >= max_len) { pxcrt::throw_bad_alloc(); }
      if (is_trivially_copyable<T>::value && sizeof(T) == 1) {
	const size_t mask = sizeof(size_t) - 1;
//fprintf(stderr, "asz pre=%zu\n", asz);
	asz = (asz + mask) & ~mask; /* align for 0-filling */
//fprintf(stderr, "asz post=%zu\n", asz);
      }
      const size_type aszt = asz * sizeof(T) + sizeof(svarray_rep);
      void *const vrep = global_allocate_n(aszt);
      svarray_rep *const nrep = reinterpret_cast<svarray_rep *>(vrep);
      PXCRT_DBG_SVECTOR1(fprintf(stderr, "rep_init alloc %p\n", nrep));
      nrep->refcnt = 1;
      nrep->valid_len = 0;
      nrep->alloc_len = asz;
      T *const start = reinterpret_cast<T *>(
	reinterpret_cast<char *>(nrep) + sizeof(svarray_rep));
      if (is_trivially_copyable<T>::value) {
	initialize_copymove_n(start, sz, ptr);
	#if 0
	if (sizeof(T) == 1) {
	  /* 0-filling */
	  memset(start + sz, 0, asz - sz);
	}
	#endif
      } else {
	PXC_TRY {
	  initialize_copymove_n(start, sz, ptr);
	} PXC_CATCH(...) {
	  PXCRT_DBG_SVECTOR1(fprintf(stderr, "rep_init dealloc %p\n", rep));
	  global_deallocate_n(rep,
	    sizeof(svarray_rep) + sizeof(T) * rep->alloc_len);
	  PXC_RETHROW;
	}
      }
      nrep->valid_len = sz;
      rep = nrep;
    }
  }
  void rep_make_unique(size_type asz) {
    if (rep->refcnt != 1) {
      /* need to create a unique rep */
      rep_make_unique_resize(asz);
    } else {
      /* already unique */
      if (asz > rep->alloc_len) {
	/* extend alloc_len */
	size_type nl = rep->alloc_len;
	do {
	  const size_type nl_n = (nl >= 1) ? (nl * 2) : 1;
	  const size_type max_len =
	    (-(sizeof(svarray_rep) + sizeof(T))) / sizeof(T) - 8;
	  if (nl_n <= nl || nl_n > max_len) { pxcrt::throw_bad_alloc(); }
	  nl = nl_n;
	} while (nl < asz);
	rep_make_unique_resize(nl);
      } else if (asz < rep->valid_len) {
	/* shrink valid_len */
	deinitialize_n(begin() + asz, rep->valid_len - asz);
	rep->valid_len = asz;
      } else {
	/* nothing to do */
      }
    }
    assert(rep->refcnt == 1);
  }
  void rep_make_unique_resize(size_type asz) {
    if (is_trivially_copyable<T>::value) {
      if (rep->refcnt != 1) {
	svarray_rep *const orep = rep;
	asz = std::max(asz, size_type(1));
	  /* set asz nonzero so that rep_init allocate a unique buffer */
	PXCRT_DBG_SVECTOR1(fprintf(stderr, "rep_make_unique_resize init1 %p\n",
	  rep));
	rep_init(begin(), std::min(size(), asz), asz);
	rep_decref(orep);
      } else {
	PXCRT_DBG_SVECTOR1(fprintf(stderr, "rep_make_unique_resize pre %p\n",
	  rep));
	if (sizeof(T) == 1) {
	  const size_t mask = sizeof(size_t) - 1;
//fprintf(stderr, "asztr pre=%zu\n", asz);
	  asz = (asz + mask) & ~mask; /* align for 0-filling */
//fprintf(stderr, "asztr post=%zu\n", asz);
	}
	rep = (svarray_rep *)global_reallocate_n(rep,
	    sizeof(svarray_rep) + sizeof(T) * rep->alloc_len,
	    sizeof(svarray_rep) + sizeof(T) * asz);
	PXCRT_DBG_SVECTOR1(fprintf(stderr, "rep_make_unique_resize post %p\n",
	  rep));
	rep->alloc_len = asz;
	#if 0
	if (sizeof(T) == 1) {
	  /* 0-filling */
	  memset(end(), 0, asz - rep->valid_len);
	}
	#endif
      }
    } else {
      svarray_rep *const orep = rep;
      PXC_TRY {
	asz = std::max(asz, size_type(1));
	  /* set asz nonzero so that rep_init allocate a unique buffer */
	rep_init(begin(), std::min(size(), asz), asz);
      } PXC_CATCH(...) {
	rep_decref(orep);
	PXC_RETHROW;
      } 
      rep_decref(orep);
    }
  }
};

template <typename T> struct msvarray : public svarray<T> {
  typedef pxcrt::bt_size_t size_type;
  typedef pxcrt::bt_slice<T> range_type;
  typedef pxcrt::bt_cslice<T> crange_type;
  msvarray() { }
  msvarray(const msvarray& x) : svarray<T>(x) { }
  msvarray(const crange_type& x) : svarray<T>(x) { }
  msvarray(const range_type& x) : svarray<T>(x) { }
  msvarray(const T *start, size_type sz, size_type asz)
    : svarray<T>(start, sz, asz) { }
};

template <typename T> struct csvarray : public svarray<T> {
  typedef pxcrt::bt_size_t size_type;
  typedef pxcrt::bt_slice<T> range_type;
  typedef pxcrt::bt_cslice<T> crange_type;
  csvarray() { }
  csvarray(const csvarray& x) : svarray<T>(x) { }
  csvarray(const crange_type& x) : svarray<T>(x) { }
  csvarray(const range_type& x) : svarray<T>(x) { }
  csvarray(const T *start, size_type sz, size_type asz)
    : svarray<T>(start, sz, asz) { }
};

template <typename Tx, typename Ty, bool is_sstr> struct
pod_sarray_eq_impl;

template <typename Tx, typename Ty> struct
pod_sarray_eq_impl<Tx, Ty, true>
{
  bt_bool operator ()(Tx const& x, Ty const& y) const
  {
    const size_t xs0 = x.size();
    const size_t ys0 = y.size();
    if (xs0 != ys0) {
      return false;
    }
    const size_t sz = (xs0 + (sizeof(size_t)-1)) / sizeof(size_t);
    const bt_ulong *const xp = reinterpret_cast<const bt_ulong *>(x.begin());
    const bt_ulong *const yp = reinterpret_cast<const bt_ulong *>(y.begin());
    for (size_t i = 0; i < sz; ++i) {
      if (xp[i] != yp[i]) {
	return false;
      }
    }
    return true;
  }
};

template <typename Tx, typename Ty> struct
pod_sarray_eq_impl<Tx, Ty, false>
{
  bt_bool operator ()(Tx const& x, Ty const& y) const
  {
    return eq_memcmp(x, y);
  }
};

template <typename Tx, typename Ty, bool is_sstr> struct
pod_sarray_compare_impl;

template <typename Tx, typename Ty> struct
pod_sarray_compare_impl<Tx, Ty, true>
{
  bt_int operator ()(Tx const& x, Ty const& y) const
  {
    const bt_ulong *const xp = reinterpret_cast<const bt_ulong *>(x.begin());
    const bt_ulong *const yp = reinterpret_cast<const bt_ulong *>(y.begin());
    const size_t xs0 = x.size();
    const size_t ys0 = y.size();
    const size_t xsz = (xs0 + (sizeof(size_t)-1)) / sizeof(size_t);
    const size_t ysz = (ys0 + (sizeof(size_t)-1)) / sizeof(size_t);
    const size_t sz = xsz < ysz ? xsz : ysz; 
    for (size_t i = 0; i < sz; ++i) {
      if (xp[i] < yp[i]) {
	return -1;
      }
      if (xp[i] > yp[i]) {
	return 1;
      }
    }
    if (xs0 > ys0) {
      return 1;
    }
    if (xs0 < ys0) {
      return -1;
    }
    return 0;
  }
};

template <typename Tx, typename Ty> struct
pod_sarray_compare_impl<Tx, Ty, false>
{
  bt_int operator ()(Tx const& x, Ty const& y) const
  {
    return compare_memcmp(x, y);
  }
};

template <typename T, bool is_sstr> struct
pod_sarray_hash_impl;

template <typename T> struct
pod_sarray_hash_impl<T, true>
{
  bt_size_t operator ()(T const& x)
  {
    const bt_ulong *const xp = reinterpret_cast<const bt_ulong *>(x.begin());
    const size_t xs0 = x.size();
    const size_t xsz = (xs0 + (sizeof(size_t)-1)) / sizeof(size_t);
    size_t r = 0;
    for (size_t i = 0; i < xsz; ++i) {
      r ^= size_t(xp[i]) + 0x9e3779b9 + (r << 6) + (r >> 2);
    }
    return r;
  }
};

template <typename T> struct
pod_sarray_hash_impl<T, false>
{
  bt_size_t operator ()(T const& x)
  {
    return hash_podarr(x);
  }
};

template <typename Tx, typename Ty> struct is_sstring_pair {
  typedef typename Tx::mapped_type mtx;
  typedef typename Ty::mapped_type mty;
  enum {
    value = (
      is_trivially_copyable<mtx>::value &&
      is_trivially_copyable<mty>::value &&
      sizeof(mtx) == 1 &&
      sizeof(mty) == 1) };
};

template <typename Tx, typename Ty> static inline bt_bool
pod_sarray_eq(Tx const& x, Ty const& y)
{
  return pod_sarray_eq_impl<Tx, Ty, is_sstring_pair<Tx, Ty>::value>()(x, y);
}

template <typename Tx, typename Ty> static inline bt_int
pod_sarray_compare(Tx const& x, Ty const& y)
{
  return pod_sarray_compare_impl<Tx, Ty, is_sstring_pair<Tx, Ty>::value>()
    (x, y);
}

template <typename T> static inline bt_size_t
pod_sarray_hash(T const& x)
{
  return pod_sarray_hash_impl<T, is_sstring_pair<T, T>::value>()(x);
}

}; // namespace pxcrt

#undef PXCRT_DBG_SVECTOR1
#undef PXCRT_DBG_SVECTOR2

;
/* inline */
namespace pxcrt {
template <typename T, size_t len>
struct farray {
  typedef T mapped_type;
  typedef size_t size_type;
  typedef T *iterator;
  typedef const T *const_iterator;
  typedef pxcrt::bt_slice<T> range_type;
  typedef pxcrt::bt_cslice<T> crange_type;
  farray() : v() { }
  bool empty() const { return len == 0; }
  size_t size() const { return len; }
  T& operator [](size_type idx) {
    #ifdef PXC_NO_BOUNDS_CHECKING
    if (idx >= len) { pxcrt::throw_invalid_index(); }
    #endif
    return v[idx];
  }
  const T& operator [](size_type idx) const {
    #ifdef PXC_NO_BOUNDS_CHECKING
    if (idx >= len) { pxcrt::throw_invalid_index(); }
    #endif
    return v[idx];
  }
  T value_at(size_type idx) const {
    #ifdef PXC_NO_BOUNDS_CHECKING
    if (idx >= len) { pxcrt::throw_invalid_index(); }
    #endif
    return v[idx];
  }
  bt_slice<T> range() {
    return bt_slice<T>(v, len);
  }
  bt_cslice<T> crange() const {
    return bt_cslice<T>(v, len);
  }
  iterator begin() { return v; }
  const_iterator begin() const { return v; }
  iterator end() { return v + len; }
  const_iterator end() const { return v + len; }
  T *rawarr() { return v; }
  const T *rawarr() const { return v; }
private:
  T v[len];
};

}; // namespace pxcrt 
;
/* inline */
namespace pxcrt {

template <typename T>
struct darray {
  typedef T mapped_type;
  typedef pxcrt::bt_size_t size_type;
  typedef T *iterator;
  typedef const T *const_iterator;
  typedef pxcrt::bt_slice<T> range_type;
  typedef pxcrt::bt_cslice<T> crange_type;
  ~darray() {
    deinitialize_n(start, valid_len);
    deallocate_uninitialized(start, valid_len);
  }
  darray(size_type len, const T& val) {
    construct_fill(len, val);
  }
  darray(const darray& x) {
    construct_copy(x.start, x.valid_len);
  }
  darray(const range_type& x) {
    construct_copy(x.rawarr(), x.size());
  }
  darray(const crange_type& x) {
    construct_copy(x.rawarr(), x.size());
  }
  bool empty() const { return valid_len == 0; }
  size_type size() const { return valid_len; }
  T& operator [](size_type idx) {
    #ifdef PXC_NO_BOUNDS_CHECKING
    if (idx >= valid_len) { pxcrt::throw_invalid_index(); }
    #endif
    return start[idx];
  }
  const T& operator [](size_type idx) const {
    #ifdef PXC_NO_BOUNDS_CHECKING
    if (idx >= valid_len) { pxcrt::throw_invalid_index(); }
    #endif
    return start[idx];
  }
  T value_at(size_type idx) const {
    #ifdef PXC_NO_BOUNDS_CHECKING
    if (idx >= valid_len) { pxcrt::throw_invalid_index(); }
    #endif
    return start[idx];
  }
  bt_slice<T> range() {
    return bt_slice<T>(start, valid_len);
  }
  bt_cslice<T> crange() const {
    return bt_cslice<T>(start, valid_len);
  }
  iterator begin() { return start; }
  const_iterator begin() const { return start; }
  iterator end() { return start + valid_len; }
  const_iterator end() const { return start + valid_len; }
  T *rawarr() { return start; }
  const T *rawarr() const { return start; }
private:
  void construct_fill(size_type len, const T& val) {
    valid_len = len;
    start = allocate_uninitialized<T>(valid_len);
    if (is_trivially_copyable<T>::value) {
      initialize_fill_n(start, valid_len, val);
    } else {
      PXC_TRY {
	initialize_fill_n(start, valid_len, val);
      } PXC_CATCH(...) {
	deallocate_uninitialized(start, valid_len);
	PXC_RETHROW;
      }
    }
  }
  void construct_copy(const T *arr, size_type len) {
    valid_len = len;
    start = allocate_uninitialized<T>(valid_len);
    if (is_trivially_copyable<T>::value) {
      initialize_copymove_n(start, valid_len, arr);
    } else {
      PXC_TRY {
       initialize_copymove_n(start, valid_len, arr);
      } PXC_CATCH(...) {
	deallocate_uninitialized(start, valid_len);
        PXC_RETHROW;
      }
    }
  }
private:
  T *start;
  size_type valid_len;
private:
  darray(); /* not allowed */
  darray& operator =(const darray& x); /* not allowed */
};

template <typename T>
struct darrayst {
  typedef T mapped_type;
  typedef pxcrt::bt_size_t size_type;
  typedef T *iterator;
  typedef const T *const_iterator;
  typedef pxcrt::bt_slice<T> range_type;
  typedef pxcrt::bt_cslice<T> crange_type;
  ~darrayst() {
    deinitialize_n(start, valid_len);
    if (is_heap) {
      deallocate_uninitialized(start, valid_len);
    }
  }
  darrayst(size_type len, const T& val, void *preal_ptr = 0) {
    construct_fill(len, val, preal_ptr);
  }
  darrayst(const range_type& x) {
    construct_copy(x.rawarr(), x.size(), 0);
  }
  darrayst(const crange_type& x) {
    construct_copy(x.rawarr(), x.size(), 0);
  }
  darrayst(const darrayst& x) {
    construct_copy(x.rawarr(), x.size(), 0);
  }
  bool empty() const { return valid_len == 0; }
  size_type size() const { return valid_len; }
  T& operator [](size_type idx) {
    #ifdef PXC_NO_BOUNDS_CHECKING
    if (idx >= valid_len) { pxcrt::throw_invalid_index(); }
    #endif
    return start[idx];
  }
  const T& operator [](size_type idx) const {
    #ifdef PXC_NO_BOUNDS_CHECKING
    if (idx >= valid_len) { pxcrt::throw_invalid_index(); }
    #endif
    return start[idx];
  }
  T value_at(size_type idx) const {
    #ifdef PXC_NO_BOUNDS_CHECKING
    if (idx >= valid_len) { pxcrt::throw_invalid_index(); }
    #endif
    return start[idx];
  }
  bt_slice<T> range() {
    return bt_slice<T>(start, valid_len);
  }
  bt_cslice<T> crange() const {
    return bt_cslice<T>(start, valid_len);
  }
  iterator begin() { return start; }
  const_iterator begin() const { return start; }
  iterator end() { return start + valid_len; }
  const_iterator end() const { return start + valid_len; }
  T *rawarr() { return start; }
  const T *rawarr() const { return start; }
  void construct_fill(size_type len, const T& val, void *preal_ptr) {
    valid_len = len;
    if (preal_ptr != 0) {
      start = static_cast<T *>(preal_ptr);
      is_heap = false;
    } else {
      start = allocate_uninitialized<T>(valid_len);
      is_heap = true;
    }
    if (is_trivially_copyable<T>::value) {
      initialize_fill_n(start, valid_len, val);
    } else {
      PXC_TRY {
	initialize_fill_n(start, valid_len, val);
      } PXC_CATCH(...) {
	deallocate_uninitialized(start, valid_len);
	PXC_RETHROW;
      }
    }
  }
  void construct_copy(const T *arr, size_type len, void *preal_ptr) {
    valid_len = len;
    if (preal_ptr != 0) {
      start = static_cast<T *>(preal_ptr);
      is_heap = false;
    } else {
      start = allocate_uninitialized<T>(valid_len);
      is_heap = true;
    }
    if (is_trivially_copyable<T>::value) {
      initialize_copymove_n(start, valid_len, arr);
    } else {
      PXC_TRY {
       initialize_copymove_n(start, valid_len, arr);
      } PXC_CATCH(...) {
	deallocate_uninitialized(start, valid_len);
        PXC_RETHROW;
      }
    }
  }
  T *start;
  size_type valid_len;
  bool is_heap;
private:
  darrayst(); /* not allowed */
  darrayst& operator =(const darrayst& x); /* not allowed */
};

}; // namespace pxcrt
;
/* inline */
#include <limits>
#include <boost/container/deque.hpp>
  /* boost::container::deque<T> allows T to be incomplete */
namespace pxcrt {

template <typename T>
struct deque_cslice {
  typedef T mapped_type;
  typedef typename boost::container::deque<T>::const_iterator iterator;
  typedef typename boost::container::deque<T>::const_iterator const_iterator;
  deque_cslice() : v() { }
  template <typename Tc> deque_cslice(const Tc& c)
    : v(c.begin(), c.end()) { }
  template <typename Tc> deque_cslice(const Tc& c, bt_size_t o1,
    bt_size_t o2) {
    if (o2 > c.size()) { o2 = c.size(); }
    if (o1 > o2) { o1 = o2; }
    v.first = c.begin() + o1;
    v.second = v.first + (o2 - o1);
  }
  iterator begin() const { return v.first; } 
  iterator end() const { return v.second; } 
  bt_bool empty() const { return v.first == v.second; }
  bt_size_t size() const { return v.second - v.first; }
  bt_size_t find(bt_size_t offset, T const& value) const {
    bt_size_t p = std::min(offset, size());
    for (iterator i = v.first + p; i < v.second; ++i, ++offset) {
      if (*i == value) {
	break;
      }
    }
    return offset;
  }
  void increment_front(size_t i) {
    if (size() < i) { pxcrt::throw_invalid_index(); }
    v.first += i;
  }
  void decrement_back(size_t i) {
    if (size() < i) { pxcrt::throw_invalid_index(); }
    v.second -= i;
  }
  const T& operator [](bt_size_t idx) const {
    if (idx >= size()) { pxcrt::throw_invalid_index(); }
    return v.first[idx];
  }
  const T& operator *() const {
    if (empty()) { pxcrt::throw_invalid_index(); }
    return *v;
  }
  T value_at(bt_size_t idx) const {
    if (idx >= size()) { pxcrt::throw_invalid_index(); }
    return v.first[idx];
  }
  T deref_value() const {
    if (empty()) { pxcrt::throw_invalid_index(); }
    return v.first[0];
  }
  iterator rawarr() const { return v.first; }
private:
  deque_cslice& operator =(const deque_cslice&); /* not allowed */
protected:
  std::pair<iterator, iterator> v;
};

template <typename T>
struct deque_slice {
  typedef T mapped_type;
  typedef typename boost::container::deque<T>::iterator iterator;
  typedef typename boost::container::deque<T>::const_iterator const_iterator;
  deque_slice() : v() { }
  template <typename Tc> deque_slice(Tc& c)
    : v(c.begin(), c.end()) { }
  template <typename Tc> deque_slice(Tc& c, bt_size_t o1, bt_size_t o2) {
    if (o2 > c.size()) { o2 = c.size(); }
    if (o1 > o2) { o1 = o2; }
    v.first = c.begin() + o1;
    v.second = v.first + (o2 - o1);
  }
  iterator begin() const { return v.first; } 
  iterator end() const { return v.second; } 
  bt_bool empty() const { return v.first == v.second; }
  bt_size_t size() const { return v.second - v.first; }
  bt_size_t find(bt_size_t offset, T const& value) const {
    bt_size_t p = std::min(offset, size());
    for (iterator i = v.first + p; i < v.second; ++i, ++offset) {
      if (*i == value) {
	break;
      }
    }
    return offset;
  }
  void increment_front(size_t i) {
    if (size() < i) { pxcrt::throw_invalid_index(); }
    v.first += i;
  }
  void decrement_back(size_t i) {
    if (size() < i) { pxcrt::throw_invalid_index(); }
    v.second -= i;
  }
  T& operator [](bt_size_t idx) const {
    if (idx >= size()) { pxcrt::throw_invalid_index(); }
    return v.first[idx];
  }
  T& operator *() const {
    if (empty()) { pxcrt::throw_invalid_index(); }
    return *v;
  }
  T value_at(bt_size_t idx) const {
    if (idx >= size()) { pxcrt::throw_invalid_index(); }
    return v.first[idx];
  }
  T deref_value() const {
    if (empty()) { pxcrt::throw_invalid_index(); }
    return v.first[0];
  }
  iterator rawarr() const { return v.first; }
private:
  deque_slice& operator =(const deque_slice&); /* not allowed */
protected:
  std::pair<iterator, iterator> v;
};

template <typename T>
struct pxcdeque : public refguard_base {
  typedef T mapped_type;
  typedef pxcrt::bt_size_t size_type;
  typedef boost::container::deque<T> base_deque;
  typedef typename base_deque::iterator iterator;
  typedef typename base_deque::const_iterator const_iterator;
  typedef deque_slice<T> range_type;
  typedef deque_cslice<T> crange_type;
  pxcdeque() { }
  pxcdeque(const crange_type& x) : value(x.begin(), x.end()) { }
  pxcdeque(const range_type& x) : value(x.begin(), x.end()) { }
  bt_bool empty() const { return value.empty(); }
  size_type size() const { return value.size(); }
  bt_size_t find(bt_size_t offset, T const& v) const {
    bt_size_t i = 0, n = size();
    for (; i < n; ++i) {
      if (value[i] == v) {
	break;
      }
    }
    return i;
  }
  T& operator [](size_type idx) {
    if (idx >= size()) { pxcrt::throw_invalid_index(); }
    return value[idx];
  }
  const T& operator [](size_type idx) const {
    if (idx >= size()) { pxcrt::throw_invalid_index(); }
    return value[idx];
  }
  void resize(size_type sz, T const& x) {
    check_resize();
    value.resize(sz, x);
  }
  void clear() {
    check_resize();
    value.clear();
  }
  void push_back(T const& x) {
    check_resize();
    value.push_back(x);
  }
  T pop_back() {
    check_resize();
    if (empty()) { pxcrt::throw_invalid_index(); }
    T r = value[size() - 1];
    value.pop_back();
    return r;
  }
  void push_front(T const& x) {
    check_resize();
    value.push_front(x);
  }
  T pop_front() {
    check_resize();
    if (empty()) { pxcrt::throw_invalid_index(); }
    T r = value[0];
    value.pop_front();
    return r;
  }
  void insert(size_type pos, const crange_type& x) {
    check_resize();
    if (pos > size()) { pxcrt::throw_invalid_index(); }
    value.insert(value.begin() + pos, x.begin(), x.end());
  }
  void erase(size_type first, size_type last) {
    if (last > size() || first > last) { pxcrt::throw_invalid_index(); }
    if (first >= last) {
      return;
    }
    iterator const ifirst = value.begin() + first;
    iterator const ilast = ifirst + (last - first);
    value.erase(ifirst, ilast);
  }
  iterator begin() { return value.begin(); }
  iterator end() { return value.end(); }
  const_iterator begin() const { return value.begin(); }
  const_iterator end() const { return value.end(); }
private:
  base_deque value;
};

}; // namespace pxcrt
;
/* inline */

#define PXCRT_ALLOCA_LIMIT 200
#define PXCRT_ALLOCA_NTSTRING(tobj, s) \
  pxcrt::alloca_ntstring tobj(s, \
    (s.size() > 0 && s.rawarr()[s.size() - 1] == '\0') ? 0 : \
    (s.size() < PXCRT_ALLOCA_LIMIT) ? alloca(s.size() + 1) : 0)

namespace pxcrt {

struct alloca_ntstring {
  alloca_ntstring(bt_cslice<bt_uchar> const& s, void *extbuf)
    : need_free(false) {
    const unsigned char *const p = s.rawarr();
    const size_t len = s.size();
    if (len == 0) {
      buffer = "";
      return;
    }
    if (p[len - 1] == '\0') {
      buffer = reinterpret_cast<const char *>(p);
      return;
    }
    char *mbuf = 0;
    if (extbuf != 0) {
      mbuf = static_cast<char *>(extbuf);
    } else {
      if (len + 1 == 0) {
	PXC_THROW(std::bad_alloc());
      }
      mbuf = static_cast<char *>(malloc(len + 1));
      if (mbuf == 0) {
	PXC_THROW(std::bad_alloc());
      }
      need_free = true;
    }
    memcpy(mbuf, p, len);
    mbuf[len] = '\0';
    buffer = mbuf;
  }
  ~alloca_ntstring() {
    if (need_free) {
      free(const_cast<char *>(buffer));
    }
  }
  const char *get() const { return buffer; }
private:
  alloca_ntstring();
  alloca_ntstring(const alloca_ntstring&);
  alloca_ntstring& operator =(const alloca_ntstring&);
  const char *buffer;
  bool need_free;
};

}; // namespace pxcrt
;
/* inline */
#include <cmath>
#include <cfloat>
namespace numeric {
}; // namespace numeric
;
/* inline */
#include <boost/type_traits.hpp>
#include <boost/numeric/conversion/bounds.hpp>
#include <boost/limits.hpp>
namespace numeric {
template <typename T, bool arith> struct limit_impl_arithmetic;
template <typename T> struct limit_impl_arithmetic <T, true> {
  static T lowest() { return ::boost::numeric::bounds<T>::lowest(); }
  static T highest() { return ::boost::numeric::bounds<T>::highest(); }
  static T smallest() { return ::boost::numeric::bounds<T>::smallest(); }
};
template <typename T> struct limit_impl_arithmetic <T, false> {
  static T lowest() { return T(); }
  static T highest() { return T(); }
  static T smallest() { return T(); }
};
template <typename T> struct limit_impl
  : public limit_impl_arithmetic<T, ::boost::is_arithmetic<T>::value > { };
}; // namespace numeric
;
/* inline */

namespace pxcrt {

template <typename Tf>
struct compare_less {
  Tf f;
  template <typename Tk>
  inline bool operator () (const Tk& x, const Tk& y) const {
    return f.call$f(x, y) < 0;
  }
};

};

;
/* inline */
#include <map>
#include <set>
namespace pxcrt {

template <typename Tk, typename Tm, typename Tcmp>
struct map_conf {
  typedef std::map<Tk, Tm, compare_less<Tcmp> > base_map;
  typedef typename base_map::key_type key_type;
  typedef typename base_map::mapped_type mapped_type;
  typedef typename base_map::iterator iterator;
  typedef typename base_map::const_iterator const_iterator;
  static inline key_type const& get_key(iterator const& iter)
    { return iter->first; }
  static inline key_type const& get_ckey(const_iterator const& iter)
    { return iter->first; }
  static inline mapped_type& get_mapped(iterator const& iter)
    { return iter->second; }
  static inline mapped_type const& get_cmapped(const_iterator const& iter)
    { return iter->second; }
  static inline mapped_type& get_mapped(base_map& m, key_type const& k)
    { return m[k]; }
  static inline bool insert(base_map& m, key_type const& k,
    mapped_type const& v)
    { return m.insert(typename base_map::value_type(k, v)).second; }
  static inline bool erase(base_map& m, key_type const& k)
    { return m.erase(k); }
};

template <typename Tk, typename Tcmp>
struct set_conf {
  typedef std::set<Tk, compare_less<Tcmp> > base_map;
  typedef typename base_map::key_type key_type;
  typedef pxcrt::bt_unit mapped_type;
  typedef typename base_map::const_iterator iterator; /* always const */
  typedef typename base_map::const_iterator const_iterator;
  static inline key_type const& get_key(iterator const& iter)
    { return *iter; }
  static inline key_type const& get_ckey(const_iterator const& iter)
    { return *iter; }
  static inline mapped_type& get_mapped(iterator const& iter)
    { return pxcrt::unit_value; }
  static inline mapped_type const& get_cmapped(const_iterator const& iter)
    { return pxcrt::unit_value; }
  static inline mapped_type& get_mapped(base_map& m, key_type const& k)
    { m.insert(k); return pxcrt::unit_value; }
  static inline bool insert(base_map& m, key_type const& key,
    mapped_type const& val)
    { return m.insert(key).second; }
  static inline bool erase(base_map& m, key_type const& k)
    { return m.erase(k); }
};

template <typename Tk, typename Tm, typename Tcmp>
struct multimap_conf {
  typedef std::multimap<Tk, Tm, compare_less<Tcmp> > base_map;
  typedef typename base_map::key_type key_type;
  typedef typename base_map::mapped_type mapped_type;
  typedef typename base_map::value_type value_type;
  typedef typename base_map::iterator iterator;
  typedef typename base_map::const_iterator const_iterator;
  static inline key_type const& get_key(iterator const& iter)
    { return iter->first; }
  static inline key_type const& get_ckey(const_iterator const& iter)
    { return iter->first; }
  static inline mapped_type& get_mapped(iterator const& iter)
    { return iter->second; }
  static inline mapped_type const& get_cmapped(const_iterator const& iter)
    { return iter->second; }
  static inline mapped_type& get_mapped(base_map& m, key_type const& k) {
    iterator i = m.find(k);
    if (i == m.end()) {
      i = m.insert(value_type(k, mapped_type()));
    }
    return i->second;
  }
  static inline bool insert(base_map& m, key_type const& key,
    mapped_type const& val)
    { m.insert(value_type(key, val)); return true; }
  static inline bool erase(base_map& m, key_type const& k)
    { return m.erase(k); }
};

template <typename Tcnf> struct with_guard;

template <typename Tcnf, bool is_const> struct range_conf;

template <typename Tcnf> struct range_conf<Tcnf, false> {
  typedef typename Tcnf::iterator iterator;
  typedef typename Tcnf::mapped_type mapped_type;
  typedef with_guard<Tcnf> map_type;
};

template <typename Tcnf> struct range_conf<Tcnf, true> {
  typedef typename Tcnf::const_iterator iterator;
  typedef typename Tcnf::mapped_type const mapped_type;
  typedef with_guard<Tcnf> const map_type;
};

template <typename Tcnf, bool is_const> struct range_base {
  typedef typename Tcnf::base_map base_map;
  typedef typename base_map::key_type key_type;
  typedef typename range_conf<Tcnf, is_const>::mapped_type mapped_type;
  typedef typename range_conf<Tcnf, is_const>::iterator iterator;
  typedef typename base_map::const_iterator const_iterator;
  typedef typename base_map::key_compare key_compare;
  typedef typename range_conf<Tcnf, is_const>::map_type map_type;
  range_base() : start(), finish() { }
  range_base(const iterator& i0, const iterator& i1)
    : start(i0), finish(i1) { }
  range_base(map_type& c, const key_type& k0, const key_type& k1)
    : start(), finish() {
    key_compare cmp;
    if (cmp(k0, k1)) {
      start = c.lower_bound(k0);
      finish = c.upper_bound(k1);
    }
  }
  template <typename Tc> range_base(Tc& c)
    : start(c.begin()), finish(c.end()) { }
  bt_bool empty() const { return start == finish; }
  mapped_type& deref() const {
    if (empty()) { pxcrt::throw_invalid_index(); }
    return Tcnf::get_mapped(start);
  }
  mapped_type const& cderef() const {
    if (empty()) { pxcrt::throw_invalid_index(); }
    return Tcnf::get_cmapped(start);
  }
  static inline key_type const& get_key(const iterator& iter) {
    return Tcnf::get_key(iter);
  }
  static inline key_type const& get_ckey(const const_iterator& iter) {
    return Tcnf::get_ckey(iter);
  }
  static inline mapped_type& get_mapped(const iterator& iter) {
    return Tcnf::get_mapped(iter);
  }
  static inline const mapped_type& get_cmapped(const const_iterator& iter) {
    return Tcnf::get_cmapped(iter);
  }
  void increment_front(size_t i) {
    while (i > 0) {
      if (empty()) { pxcrt::throw_invalid_index(); }
      ++start;
      --i;
    }
  }
  range_base<Tcnf, is_const> range() const {
    return range_base<Tcnf, is_const>(start, finish);
  }
  range_base<Tcnf, true> crange() const {
    return range_base<Tcnf, true>(start, finish);
  }
  iterator begin() const { return start; }
  iterator end() const { return finish; }
public:
  iterator start;
  iterator finish;
};

template <typename Tcnf>
struct with_guard : public refguard_base {
  typedef Tcnf conf_type;
  typedef typename Tcnf::base_map base_map;
  typedef range_base<Tcnf, false> range_type;
  typedef range_base<Tcnf, true> crange_type;
  typedef typename base_map::key_type key_type;
  typedef typename Tcnf::mapped_type mapped_type;
  typedef typename base_map::size_type size_type;
  typedef typename base_map::iterator iterator;
  typedef typename base_map::const_iterator const_iterator;
  typedef typename base_map::iterator find_type;
  typedef typename base_map::const_iterator cfind_type;
  with_guard() { }
  with_guard(const with_guard& x)
    : v(x.v) { }
  with_guard(with_guard&& x)
    : v(x.v) { }
  with_guard(const range_type& x)
    : v(x.start, x.finish) { }
  with_guard(const crange_type& x)
    : v(x.start, x.finish) { }
  with_guard& operator =(const with_guard& x) {
    check_resize();
    v = x.v;
    return *this;
  }
  with_guard& operator =(with_guard&& x) {
    check_resize();
    v = x.v;
    return *this;
  }
  mapped_type& operator [](const key_type& k) {
    /* no need to call check_resize() because v[k] never invalidate any
     * iterators */
    return Tcnf::get_mapped(v, k);
  }
  mapped_type value_at(const key_type& k) { return Tcnf::get_mapped(v, k); }
  static inline key_type const& get_key(const iterator& iter) {
    return Tcnf::get_key(iter);
  }
  static inline key_type const& get_ckey(const const_iterator& iter) {
    return Tcnf::get_ckey(iter);
  }
  static inline mapped_type& get_mapped(const iterator& iter) {
    return Tcnf::get_mapped(iter);
  }
  static inline const mapped_type& get_cmapped(const const_iterator& iter) {
    return Tcnf::get_cmapped(iter);
  }
  bool exists(const key_type& k) const { return v.find(k) != v.end(); }
  iterator find(const key_type& k) { return v.find(k); }
  const_iterator find(const key_type& k) const { return v.find(k); }
  iterator notfound() { return end(); }
  const_iterator notfound() const { return end(); }
  iterator lower_bound(const key_type& k) { return v.lower_bound(k); }
  const_iterator lower_bound(const key_type& k) const {
    return v.lower_bound(k);
  }
  iterator upper_bound(const key_type& k) { return v.upper_bound(k); }
  const_iterator upper_bound(const key_type& k) const {
    return v.upper_bound(k);
  }
  bt_bool empty() const { return v.empty(); }
  size_type size() const { return v.size(); }
  bool insert(const key_type& k, const mapped_type& m) {
    /* insert() does not invalidate iterators */
    return Tcnf::insert(v, k, m);
  }
  size_type erase(const key_type& k) {
    check_resize();
    return v.erase(k);
  }
  void clear() {
    check_resize();
    v.clear();
  }
  void swap(with_guard& x) {
    check_resize();
    x.check_resize();
    v.swap(x.v);
  }
  range_type range() {
    return range_type(v.begin(), v.end());
  }
  crange_type crange() const {
    return crange_type(v.begin(), v.end());
  }
  range_type equal_range(const key_type& k) {
    const std::pair<iterator, iterator> r = v.equal_range(k);
    return range_type(r.first, r.second);
  }
  crange_type equal_crange(const key_type& k) const {
    const std::pair<const_iterator, const_iterator> r = v.equal_range(k);
    return crange_type(r.first, r.second);
  }
  iterator begin() { return v.begin(); }
  const_iterator begin() const { return v.begin(); }
  iterator end() { return v.end(); }
  const_iterator end() const { return v.end(); }
private:
  base_map v;
};

template <typename T> static inline void memfunc_swap(T& x, T& y)
{
  x.swap(y);
}

}; // namespace pxcrt
;
/* inline */

#include <stdexcept>
#include <vector>

namespace pxcrt {

void throw_invalid_index() __attribute__((noreturn));
void throw_null_dereference() __attribute__((noreturn));
void throw_invalid_field() __attribute__((noreturn));
void throw_would_invalidate() __attribute__((noreturn));
void throw_bad_alloc() __attribute__((noreturn));
void set_stack_trace_limit(size_t sz);

struct exception {
  exception();
  virtual ~exception() PXC_NOTHROW { }
  virtual bt_string message() const = 0;
  std::vector<void *> trace;
};

struct logic_error : virtual exception, std::logic_error {
  logic_error();
  virtual bt_string message() const;
};

struct runtime_error : virtual exception, std::runtime_error {
  runtime_error();
  virtual bt_string message() const;
};

struct bad_alloc : virtual exception, std::bad_alloc {
  bad_alloc();
  virtual bt_string message() const;
};

struct invalid_index : logic_error {
  virtual bt_string message() const;
};

struct invalid_field : logic_error {
  virtual bt_string message() const;
};

struct would_invalidate : logic_error {
  virtual bt_string message() const;
};

int main_nothrow(void (*main_f)(void));

}; // namespace pxcrt
;
/* inline */

#include <algorithm>

#define PXCRT_DBG_HASH(x)

namespace pxcrt {

struct denom_info {
  size_t num;
  size_t (*func)(size_t);
};

struct compare_denom_info {
  bool operator ()(const denom_info& x, const denom_info& y) const {
    return x.num < y.num;
  }
};

extern const denom_info denom_list[38];

template <size_t N> size_t modulo_fixed(size_t x)
{
  return x % N;
}

template <typename Tk, typename Tm>
struct hmap_entry {
  Tk first;
  Tm second;
  hmap_entry(const Tk& k, const Tm& m) : first(k), second(m) { }
};

template <typename Tk, typename Tm>
struct hmap_node {
  typedef hmap_entry<Tk, Tm> entry_type;
  typedef Tk key_type;
  typedef Tm mapped_type;
  hmap_node *next;
  entry_type entry;
  hmap_node(hmap_node *n, const Tk& k, const Tm& m)
    : next(n), entry(k, m) { }
};

template <typename Tnode, bool is_const> struct hmap_iterator;

template <typename Tnode> struct hmap_iterator<Tnode, false> {
  typedef typename Tnode::key_type key_type;
  typedef typename Tnode::mapped_type mapped_type;
  typedef Tnode node_type;
  node_type *pnode;
  node_type **pbucket;
};

template <typename Tnode> struct hmap_iterator<Tnode, true> {
  typedef const typename Tnode::key_type key_type;
  typedef const typename Tnode::mapped_type mapped_type;
  typedef const Tnode node_type;
  node_type *pnode;
  node_type **pbucket;
};

template <typename Tnode, bool is_const>
struct hmap_range {
  typedef hmap_iterator<Tnode, is_const> iterator;
  typedef hmap_iterator<Tnode, true> const_iterator;
  typedef typename iterator::key_type key_type;
  typedef typename iterator::mapped_type mapped_type;
  typedef typename iterator::node_type node_type;
  hmap_range() { start.pnode = 0; start.pbucket = 0; pbucket_end = 0; }
  template <typename Tc> hmap_range(Tc& c) {
    node_type **b = c.get_buckets();
    node_type **b_end = b + c.hdr.num_buckets;
    start.pnode = 0;
    for (; b != b_end; ++b) {
      if (*b != 0) {
	start.pnode = *b;
	break;
      }
    }
    start.pbucket = b;
    pbucket_end = b_end;
  }
  bool empty() const { return start.pbucket == pbucket_end; }
  void increment_front(size_t n) {
    for (size_t i = 0; i < n; ++i) {
      increment_front_one();
    }
  }
private:
  void increment_front_one() {
    if (empty()) { pxcrt::throw_invalid_index(); }
    node_type *p = start.pnode->next;
    if (p != 0) {
      start.pnode = p;
      return;
    }
    node_type **b = start.pbucket + 1;
    for (node_type *b = start.pbucket + 1; b != pbucket_end; ++b) {
      if (*b != 0) {
	start.pnode = *b;
	start.pbucket = b;
	return;
      }
    }
    start.pnode = 0;
    start.pbucket = pbucket_end;
  }
  mapped_type& deref() const {
    if (empty()) { pxcrt::throw_invalid_index(); }
    return start.pnode->entry.second;
  }
  mapped_type const& cderef() const {
    if (empty()) { pxcrt::throw_invalid_index(); }
    return start.pnode->entry.second;
  }
  static inline key_type const& get_key(const iterator& iter) {
    return iter.pnode->first;
  }
  static inline key_type const& get_ckey(const const_iterator& iter) {
    return iter.pnode->first;
  }
  static inline mapped_type& get_mapped(const iterator& iter) {
    return iter.pnode->second;
  }
  static inline mapped_type const& get_cmapped(const const_iterator& iter) {
    return iter.pnode->second;
  }
  iterator begin() const { return start; }
  iterator end() const {
    iterator i;
    i.pnode = 0;
    i.pbucket = pbucket_end;
    return i;
  }
private:
  iterator start;
  node_type **pbucket_end;
};

struct hmbuckets_hdr {
  size_t num_buckets;
  size_t (*buckets_modf)(size_t);
  size_t num_entries;
  size_t denom_ent;
};

template <typename Tk, typename Tm, typename Thash, typename Teq>
struct hash_map : public refguard_base {
  typedef Tk key_type;
  typedef Tm mapped_type;
  typedef Tm *find_type;
  typedef Tm const *cfind_type;
  typedef hmap_node<Tk, Tm> node_type;
  typedef hmap_entry<Tk, Tm> entry_type;
  typedef hmap_range<node_type, false> range_type;
  typedef hmap_range<node_type, true> crange_type;
  typedef hmap_iterator<node_type, false> iterator;
  typedef hmap_iterator<node_type, true> const_iterator;
  typedef size_t size_type;
  hash_map(size_t initial_size) {
    const size_t n = 38;
    const denom_info e = { initial_size, 0 };
    const denom_info *p = std::lower_bound(denom_list, denom_list + n, e,
      compare_denom_info());
    if (p == denom_list + n) {
      --p;
    }
    const size_t sz = p->num;
    const size_t max_len = (-sizeof(hmbuckets_hdr)) / sizeof(node_type *) - 1;
    if (sz >= max_len) { pxcrt::throw_bad_alloc(); }
    hdr = static_cast<hmbuckets_hdr *>(global_allocate_n(
      sizeof(hmbuckets_hdr) + sz * sizeof(node_type *)));
    hmbuckets_hdr& h = *hdr;
    node_type **const buckets = get_buckets();
    h.num_buckets = sz;
    h.buckets_modf = p->func;
    h.num_entries = 0;
    h.denom_ent = p - denom_list;
    for (size_type i = 0; i < h.num_buckets; ++i) {
      buckets[i] = 0;
    }
  }
  ~hash_map() {
    hmbuckets_hdr& h = *hdr;
    node_type **const buckets = get_buckets();
    for (size_type i = 0; i < h.num_buckets; ++i) {
      node_type *p = buckets[i];
      while (p != 0) {
	node_type *np = p->next;
	delete p;
	p = np;
      }
    }
    global_deallocate_n(hdr,
      sizeof(hmbuckets_hdr) + hdr->num_buckets * sizeof(node_type *));
  }
  Tm& operator [](const Tk& k) {
    rehash_if();
    hmbuckets_hdr& h = *hdr;
    size_t const hashval = hash.__call$f(k);
    size_t const bkt = h.buckets_modf(hashval);
    node_type **const buckets = get_buckets();
    assert(bkt < h.num_buckets);
    node_type *cur = buckets[bkt];
    for (node_type *p = cur; p != 0; p = p->next) {
      entry_type& pe = p->entry;
      if (eq.__call$f(pe.first, k)) {
	return pe.second;
      }
    }
    node_type *const nn = new node_type(cur, k, Tm());
    buckets[bkt] = nn;
    ++h.num_entries;
    return nn->entry.second;
  }
  bool insert(const Tk& k, const Tm& m, size_type hashval) {
    rehash_if();
    hmbuckets_hdr& h = *hdr;
    size_t const bkt = h.buckets_modf(hashval);
    node_type **const buckets = get_buckets();
    assert(bkt < h.num_buckets);
    node_type *cur = buckets[bkt];
    for (node_type *p = cur; p != 0; p = p->next) {
      entry_type& pe = p->entry;
      if (eq.__call$f(pe.first, k)) {
	pe.second = m;
	return false; /* not created */
      }
    }
    node_type *const nn = new node_type(cur, k, m);
    buckets[bkt] = nn;
    ++h.num_entries;
    return true; /* created */
  }
  bool insert(const Tk& k, const Tm& m) {
    return insert(k, m, hash.__call$f(k));
  }
  const Tm *find(const Tk& k, size_type hashval) const {
    size_t const bkt = hdr->buckets_modf(hashval);
    return find_internal(k, bkt);
  }
  Tm *find(const Tk& k, size_type hashval) {
    size_t const bkt = hdr->buckets_modf(hashval);
    return find_internal(k, bkt);
  }
  const Tm *find(const Tk& k) const {
    return find(k, hash.__call$f(k));
  }
  Tm *find(const Tk& k) {
    return find(k, hash.__call$f(k));
  }
  const Tm *notfound() const { return 0; }
  Tm *notfound() { return 0; }
  static inline Tm& get_mapped(Tm *ptr) { return *ptr; }
  static inline const Tm& get_cmapped(const Tm *ptr) { return *ptr; }
  void rehash_if() {
    if (!refguard_is_zero()) {
      return;
    }
    size_t const n = hdr->denom_ent;
    const denom_info& di = denom_list[n];
    if (hdr->num_entries > di.num && n < 38 - 1) {
      rehash_internal(hdr->num_entries);
    }
  }
  void rehash(size_type nsz) {
    if (!refguard_is_zero()) {
      return;
    }
    rehash_internal(nsz);
  }
  size_t size() const { return hdr->num_entries; }
  size_t bucket_count() const { return hdr->num_buckets; }
  size_t bucket_size(size_t bkt) const {
    if (bkt >= hdr->num_buckets) { return 0; }
    node_type *p = get_buckets()[bkt];
    size_t r = 0;
    for (p = get_buckets()[bkt]; p != 0; p = p->next, ++r) { }
    return r;
  }
private:
  void swap_internal(hash_map& x) {
    hmbuckets_hdr *hdr_x = x.hdr;
    x.hdr = hdr;
    hdr = hdr_x;
  }
  void rehash_internal(size_type nsz) {
    PXCRT_DBG_HASH(fprintf(stderr, "rehash %zu\n", nsz));
    hash_map x(nsz);
    node_type **const buckets = get_buckets();
    node_type **const x_buckets = x.get_buckets();
    hmbuckets_hdr& h= *hdr;
    hmbuckets_hdr& x_h = *x.hdr;
    for (node_type **b = buckets; b != buckets + h.num_buckets; ++b) {
      while (*b != 0) {
	node_type *p = *b;
	node_type *p_next = p->next;
	size_t const x_hashval = x.hash.__call$f(p->entry.first);
	size_t const x_bkt = x_h.buckets_modf(x_hashval);
	p->next = x_buckets[x_bkt];
	x_buckets[x_bkt] = p;
	*b = p_next;
	++x_h.num_entries;
	--h.num_entries;
      }
    }
    swap_internal(x);
  }
  Tm *find_internal(const Tk& k, size_type bkt) const {
    node_type **const buckets = get_buckets();
    assert(bkt < hdr->num_buckets);
    for (node_type *p = buckets[bkt]; p != 0; p = p->next) {
      entry_type& pe = p->entry;
      if (eq.__call$f(pe.first, k)) {
	return &pe.second;
      }
    }
    return 0;
  }
  node_type **get_buckets() const {
    return reinterpret_cast<node_type **>(
      reinterpret_cast<char *>(hdr) + sizeof(hmbuckets_hdr));
  }
private:
  hash_map(const hash_map& x);
  hash_map& operator =(const hash_map& x);
private:
  hmbuckets_hdr *hdr;
  Thash hash;
  Teq eq;
};

}; // namespace pxcrt

#undef PXCRT_DBG_HASH

;
/* inline */

#include <string>
#include <errno.h>

#ifdef PXCRT_DBG_RC
#define DBG_RC(x) x
#else
#define DBG_RC(x)
#endif
#ifdef PXCRT_DBG_MTX
#define DBG_MTX(x) x
#else
#define DBG_MTX(x)
#endif
#ifdef PXCRT_DBG_COND
#define DBG_COND(x) x
#else
#define DBG_COND(x)
#endif

#include <cstdlib>
#include <mutex>
#include <condition_variable>

namespace pxcrt {

typedef std::recursive_mutex mutex;
typedef std::condition_variable_any condition_variable;

struct monitor {
  monitor() { }
  monitor(monitor const& x) { }
  monitor& operator =(monitor const& x) {
    return *this;
  }
  mutex mtx;
  condition_variable cond;
};

template <typename T>
struct rcval {
  /* rcval: single threaded, mutable or immutable */
  typedef T value_type;
  rcval() : value() {
    DBG_RC(fprintf(stdout, "c1 %p\n", this));
  }
  explicit rcval(const T& v) : value(v) {
    DBG_RC(fprintf(stdout, "c2 %p\n", this));
  }
  template <typename T0> explicit
  rcval(const T0& a0) : value(a0) { }
  template <typename T0, typename T1>
  rcval(const T0& a0, const T1& a1) : value(a0, a1) { }
  template <typename T0, typename T1, typename T2>
  rcval(const T0& a0, const T1& a1, const T2& a2) : value(a0, a1, a2) { }
  void incref$z() const {
    DBG_RC(fprintf(stdout, "%p %ld +1\n", this, refcnt$z()));
    count$z.incref$z();
  }
  void decref$z() const {
    DBG_RC(fprintf(stdout, "%p %ld -1\n", this, refcnt$z()));
    if (count$z.decref$z()) {
      this->~rcval<T>();
      this->deallocate(this);
      DBG_RC(fprintf(stdout, "d  %p\n", this));
    }
  }
  size_t refcnt$z() const { return count$z.refcnt$z(); }
  /* void lock$z() const { } */
  /* void unlock$z() const { } */
  /* void wait$z() const { } */
  /* void notify_one$z() const { } */
  /* void notify_all$z() const { } */
  static rcval<T> *allocate() {
    return local_allocate< rcval<T> >();
  }
  static void deallocate(const rcval<T> *ptr) {
    local_deallocate< rcval<T> >(ptr);
  }
public:
  mutable stcount count$z;
  T value;
private:
  rcval(const rcval&);
  rcval& operator =(const rcval&);
};

template <typename T>
struct trcval {
  /* trcval: multithreaded, mutable */
  typedef T value_type;
  trcval() : value() {
    DBG_RC(fprintf(stdout, "c1 %p\n", this));
  }
  explicit trcval(const T& v) : value(v) {
    DBG_RC(fprintf(stdout, "c2 %p\n", this));
  }
  template <typename T0> explicit
  trcval(const T0& a0) : value(a0) { }
  template <typename T0, typename T1>
  trcval(const T0& a0, const T1& a1) : value(a0, a1) { }
  template <typename T0, typename T1, typename T2>
  trcval(const T0& a0, const T1& a1, const T2& a2) : value(a0, a1, a2) { }
  void incref$z() const {
    DBG_RC(fprintf(stdout, "ir %p %ld +1\n", this, refcnt$z()));
    count$z.incref$z();
  }
  void decref$z() const {
    DBG_RC(fprintf(stdout, "dr %p %ld -1\n", this, refcnt$z()));
    if (count$z.decref$z()) {
      this->~trcval<T>();
      this->deallocate(this);
      DBG_RC(fprintf(stdout, "de %p\n", this));
    }
  }
  size_t refcnt$z() const { return count$z.refcnt$z(); }
  void lock$z() const { monitor$z.mtx.lock(); }
  void unlock$z() const { monitor$z.mtx.unlock(); }
  void wait$z() const { monitor$z.cond.wait(monitor$z.mtx); }
  void notify_one$z() const { monitor$z.cond.notify_one(); }
  void notify_all$z() const { monitor$z.cond.notify_all(); }
  static trcval<T> *allocate() {
    return allocate_single< trcval<T> >();
  }
  static void deallocate(const trcval<T> *ptr) {
    deallocate_single< trcval<T> >(ptr);
  }
public:
  mutable mtcount count$z;
  mutable monitor monitor$z;
  T value;
private:
  trcval(const trcval&);
  trcval& operator =(const trcval&);
};

template <typename T>
struct tircval {
  /* tircval: multithreded, immutable */
  typedef T value_type;
  tircval() : value() {
    DBG_RC(fprintf(stdout, "c1 %p\n", this));
  }
  explicit tircval(const T& v) : value(v) {
    DBG_RC(fprintf(stdout, "c2 %p\n", this));
  }
  template <typename T0> explicit
  tircval(const T0& a0) : value(a0) { }
  template <typename T0, typename T1>
  tircval(const T0& a0, const T1& a1) : value(a0, a1) { }
  template <typename T0, typename T1, typename T2>
  tircval(const T0& a0, const T1& a1, const T2& a2) : value(a0, a1, a2) { }
  void incref$z() const {
    DBG_RC(fprintf(stdout, "ir %p %ld +1\n", this, refcnt$z()));
    count$z.incref$z();
  }
  void decref$z() const {
    DBG_RC(fprintf(stdout, "dr %p %ld -1\n", this, refcnt$z()));
    if (count$z.decref$z()) {
      this->~tircval<T>();
      this->deallocate(this);
      DBG_RC(fprintf(stdout, "de %p\n", this));
    }
  }
  size_t refcnt$z() const { return count$z.refcnt$z(); }
  /* void lock$z() const { } */
  /* void unlock$z() const { } */
  /* void wait$z() const { } */
  static tircval<T> *allocate() {
    return allocate_single< tircval<T> >();
  }
  static void deallocate(const tircval<T> *ptr) {
    deallocate_single< tircval<T> >(ptr);
  }
public:
  mutable mtcount count$z;
  T value;
private:
  tircval(const tircval&);
  tircval& operator =(const tircval&);
};

template <typename T>
struct rcptr { /* T must have an intrusive count */
  typedef T target_type;
  typedef void range_type; /* guard object requires this */
  typedef void crange_type; /* guard object requires this */
  template <typename Tx> friend struct rcptr;
  rcptr(T *rawptr) : ptr(rawptr) { } /* for fast boxing constr */
  rcptr(const rcptr& x) : ptr(x.ptr) { ptr->incref$z(); }
  /* ptr{foo} to ptr{ifoo} */
  template <typename Tx> rcptr(const rcptr<Tx>& x) : ptr(x.get()) {
    ptr->incref$z();
  }
  /* boxing foo to ptr{ifoo} */
  template <typename Tx> explicit rcptr(const Tx& x)
    : ptr(make_rawptr(x)) { }
  ~rcptr() { ptr->decref$z(); }
  rcptr& operator =(T *x) { return set(x); }
  template <typename Tx> rcptr& operator =(Tx *x) { return set(x); }
  rcptr& operator =(const rcptr& x) { return set(x.ptr); }
  template <typename Tx> rcptr& operator =(const rcptr<Tx>& x) {
    return set(x.ptr);
  }
  T *get() const { return ptr; }
  T& operator *() const { return *ptr; }
  T *operator ->() const { return ptr; }
  void inc_refguard() const { ptr->lock$z(); }
  void dec_refguard() const { ptr->unlock$z(); }
  template <typename Tx> static inline T *make_rawptr(const Tx& x) {
    T *p = T::allocate();
    PXC_TRY {
      new (p) T(x);
    } PXC_CATCH(...) {
      T::deallocate(p);
      PXC_RETHROW;
    }
    return p;
  }
  static inline T *make_rawptr() {
    T *p = T::allocate();
    PXC_TRY {
      new (p) T();
    } PXC_CATCH(...) {
      T::deallocate(p);
      PXC_RETHROW;
    }
    return p;
  }
  template <typename Tc> struct guard_ref {
    guard_ref(Tc& x) : v(x) { v.inc_refguard(); }
    ~guard_ref() { v.dec_refguard(); }
    Tc& get() { return v; }
    typename Tc::range_type get_range() 
      { return typename Tc::range_type(v); }
    typename Tc::crange_type get_crange() 
      { return typename Tc::crange_type(v); }
  private:
    Tc& v;
    guard_ref(const guard_ref&);
    guard_ref& operator =(const guard_ref&);
  };
  template <typename Tc> struct guard_val {
    guard_val(const Tc& x) : v(x) { v.inc_refguard(); }
    ~guard_val() { v.dec_refguard(); }
    Tc& get() { return v; }
    typename Tc::range_type get_range() 
      { return typename Tc::range_type(v); }
    typename Tc::crange_type get_crange() 
      { return typename Tc::crange_type(v); }
  private:
    Tc v;
    guard_val(const guard_val&);
    guard_val& operator =(const guard_val&);
  };
private:
  T *ptr; /* not nullable */
private:
  rcptr& set(T *x) {
    x->incref$z();
    ptr->decref$z();
    ptr = x;
    return *this;
  }
};

template <typename T> struct lock_guard
{
  lock_guard(const rcptr<T>& p) : ptr(p), objref(*p) { objref.lock$z(); }
  ~lock_guard() { objref.unlock$z(); }
  T *get() const { return &objref; }
  T& operator *() const { return objref; }
  T *operator ->() const { return &objref; }
  void wait() { objref.wait$z(); }
  void notify_one() { objref.notify_one$z(); }
  void notify_all() { objref.notify_all$z(); }
private:
  lock_guard(const lock_guard&);
  lock_guard& operator =(const lock_guard&);
private:
  rcptr<T> ptr;
  T& objref;
};

template <typename T> struct lock_cguard
{
  lock_cguard(const rcptr<const T>& p) : ptr(p), objref(*p)
    { objref.lock$z(); }
  ~lock_cguard() { objref.unlock$z(); }
  const T *get() const { return &objref; }
  const T& operator *() const { return objref; }
  const T *operator ->() const { return &objref; }
  void wait() { objref.wait$z(); }
  void notify_one() { objref.notify_one$z(); }
  void notify_all() { objref.notify_all$z(); }
private:
  lock_cguard(const lock_cguard&);
  lock_cguard& operator =(const lock_cguard&);
private:
  rcptr<const T> ptr;
  const T& objref;
};

template <typename T> typename T::value_type deref_value(const rcptr<T>& p)
{
  return p->value;
}

template <typename T> T deref(const rcptr<T>& p)
{
  return *p;
}

template <typename Tto, typename Tfrom> Tto const&
downcast_const(Tfrom const& from) {
  return dynamic_cast<Tto const&>(from);
}

template <typename Tto, typename Tfrom> Tto&
downcast_mutable(Tfrom& from) {
  return dynamic_cast<Tto&>(from);
}

template <typename T, typename Tobj> bool
instanceof(Tobj const& x) {
  return dynamic_cast<T const *>(&x) != 0;
}

template <typename Tto, typename Tfrom> Tto
pointer_downcast(Tfrom const& from) {
  typename Tto::target_type& ref = dynamic_cast<typename Tto::target_type&>(
    *(from.get()));
  ref.incref$z();
  return Tto(&ref);
}

}; // namespace pxcrt
;
/* inline */
namespace pxcrt {
template <typename Tbase, typename Tid>
struct distinct { typedef Tbase type; };
};
;
/* inline */
namespace pxcrt {
struct io {
  io(int const dummy) { } /* disable default constructor */
};
};
;
/* inline */
namespace pxcrt {
#if defined(PXC_POSIX) && !defined(PXC_EMSCRIPTEN)
typedef ::suseconds_t suseconds_t;
#else
typedef long suseconds_t;
#endif
};
;
/* inline */
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef PXC_POSIX
#include <unistd.h>
#else
#include <io.h>
#endif
namespace pxcrt {
using namespace pxcrt;

#ifdef PXC_POSIX
typedef struct ::stat statbuf;
typedef ::off_t off_t;
typedef ::dev_t dev_t;
typedef ::ino_t ino_t;
typedef ::mode_t mode_t;
typedef ::nlink_t nlink_t;
typedef ::uid_t uid_t;
typedef ::gid_t gid_t;
typedef ::blksize_t blksize_t;
typedef ::blkcnt_t blkcnt_t;
#else
typedef struct ::_stat statbuf;
typedef __int64 off_t;
typedef unsigned __int64 dev_t;
typedef unsigned __int64 ino_t;
typedef unsigned mode_t;
typedef unsigned nlink_t;
typedef unsigned uid_t;
typedef unsigned gid_t;
typedef unsigned __int64 blksize_t;
typedef unsigned __int64 blkcnt_t;
#define S_ISUID   _S_ISUID
#define S_ISGID   _S_ISGID
#define S_ISVTX   _S_ISVTX
#define S_IREAD   _S_IREAD
#define S_IWRITE  _S_IWRITE
#define S_IEXEC   _S_IEXEC
#define S_IRUSR   _S_IREAD
#define S_IWUSR   _S_IWRITE
#define S_IXUSR   _S_IEXEC
#define S_IRWXU   (_S_IREAD|_S_IWRITE|_S_IEXEC)
#define S_IRGRP   _S_IREAD
#define S_IWGRP   _S_IWRITE
#define S_IXGRP   _S_IEXEC
#define S_IRWXG   (_S_IREAD|_S_IWRITE|_S_IEXEC)
#define S_IROTH   _S_IREAD
#define S_IWOTH   _S_IWRITE
#define S_IXOTH   _S_IEXEC
#define S_IRWXO   (_S_IREAD|_S_IWRITE|_S_IEXEC)
#endif

struct file_st;

int file_st_close(file_st& f);

struct file_st_rep {
  file_st_rep(int fd) : fd(fd) { }
  ~file_st_rep() {
    /* don't close stdin/out/err */
    if (fd > 2) {
#ifdef PXC_POSIX
      ::close(fd);
#else
      ::_close(fd);
#endif
    }
  }
  int get() const { return fd; }
  friend int file_st_close(file_st& f);
private:
  int fd;
  file_st_rep(const file_st_rep&);
  file_st_rep& operator =(const file_st_rep&);
};

struct file_mt_rep {
  file_mt_rep(int fd) : fd(fd) { }
  ~file_mt_rep() {
    /* don't close stdin/out/err */
    if (fd > 2) {
#ifdef PXC_POSIX
      ::close(fd);
#else
      ::_close(fd);
#endif
    }
  }
  int get() const { return fd; }
private:
  const int fd;
  file_mt_rep(const file_mt_rep&);
  file_mt_rep& operator =(const file_mt_rep&);
};

struct file {
  virtual int get() const = 0;
  virtual void incref$z() const = 0;
  virtual void decref$z() const = 0;
  virtual size_t refcnt$z() const = 0;
};

struct file_st : public file {
  typedef ::pxcrt::rcptr< pxcrt::rcval<file_st_rep> > ptr_type;
  ptr_type ptr;
  file_st(int fd) : ptr(ptr_type::make_rawptr(fd)) { }
  virtual int get() const { return ptr->value.get(); }
  size_t refcnt$z() const { return count.refcnt$z(); }
  void incref$z() const { count.incref$z(); }
  void decref$z() const {
    if (count.decref$z()) {
      this->~file_st();
      this->deallocate(this);
    }
  }
  static file_st *allocate() {
    return pxcrt::allocate_single<file_st>();
  }
  static void deallocate(file_st const *ptr) {
    pxcrt::deallocate_single<file_st>(ptr);
  }
  mutable pxcrt::stcount count;
};

struct file_mt : public file {
  typedef ::pxcrt::rcptr< pxcrt::tircval<file_mt_rep> > ptr_type;
  ptr_type ptr;
  file_mt(int fd) : ptr(ptr_type::make_rawptr(fd)) { }
  virtual int get() const { return ptr->value.get(); }
  size_t refcnt$z() const { return count.refcnt$z(); }
  void incref$z() const { count.incref$z(); }
  void decref$z() const {
    if (count.decref$z()) {
      this->~file_mt();
      this->deallocate(this);
    }
  }
  /* lock/unlock are no-op */
  void lock$z() const { }
  void unlock$z() const { }
  /* does not implement wait/notify */
  static file_mt *allocate() {
    return pxcrt::allocate_single<file_mt>();
  }
  static void deallocate(file_mt const *ptr) {
    pxcrt::deallocate_single<file_mt>(ptr);
  }
  mutable pxcrt::mtcount count;
};

};
;
/* inline */
#include <csignal>
namespace pxcrt {
typedef void (*sighandler_t)(int);
};
;
/* inline */
#ifdef PXC_POSIX
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

namespace pxcrt {
#ifdef PXC_POSIX
typedef ::pid_t pid_t;
#else
typedef int pid_t; /* dummy */
#endif
};
;
/* inline */

#include <thread>
#include <system_error>

namespace pxcrt {

typedef std::thread *thread_ptr;

};

;
/* inline */
namespace pxcrt {
template <typename T> struct aligned_16 {
  aligned_16(T const& value0) : value(value0) { }
#ifdef _MSC_VER
  __declspec(align(16)) T value;
#elif defined(PXC_EMSCRIPTEN)
  T value;
#else
  T value alignas(16);
#endif
};
};
;
/* inline */

#include <cstdio>
#include <cstdlib>
#include <typeinfo>

namespace pxcrt {

template <typename T> struct debug_count_exit_hook {
  debug_count_exit_hook();
  ~debug_count_exit_hook();
  void dummy() { }
};

template <typename T> struct debug_count {
  debug_count() {
    count.incref$z();
    exit_hook.dummy();
  }
  debug_count(const debug_count&) {
    count.incref$z();
  }
  debug_count(debug_count&&) {
    count.incref$z();
  }
  ~debug_count() {
    count.decref$z();
  }
  bt_long get() const {
    return count.refcnt$z();
  }
  static mtcount count;
  static debug_count_exit_hook<T> exit_hook;
};

template <typename T> mtcount debug_count<T>::count;
template <typename T> debug_count_exit_hook<T> debug_count<T>::exit_hook;

template <typename T> debug_count_exit_hook<T>::debug_count_exit_hook()
{
  // char buf[1024];
  // snprintf(buf, 1024, "constr %s: %ld\n", typeid(T).name(),
  //   debug_count<T>::count.load());
  // ::write(1, buf, strlen(buf));
}

template <typename T> debug_count_exit_hook<T>::~debug_count_exit_hook()
{
  char buf[1024];
  snprintf(buf, 1024, "%s: %ld\n", typeid(T).name(),
    debug_count<T>::count.refcnt$z());
  ::write(1, buf, strlen(buf));
}

template <typename T> struct debug_trace {
  debug_trace() {
    fprintf(stderr, "%s: def %p\n", typeid(T).name(), this);
  }
  debug_trace(const debug_trace&) {
    fprintf(stderr, "%s: cpy %p\n", typeid(T).name(), this);
  }
  debug_trace(debug_trace&&) {
    fprintf(stderr, "%s: mov %p\n", typeid(T).name(), this);
  }
  ~debug_trace() {
    fprintf(stderr, "%s: des %p\n", typeid(T).name(), this);
  }
};

};

;
namespace $n {
}; /* namespace */
namespace exception$n { 
struct exception;
struct logic_error;
struct runtime_error;
struct bad_alloc;
struct exception;
struct logic_error;
struct runtime_error;
struct bad_alloc;
}; /* namespace exception */
namespace io$n { namespace file$n { 
struct file;
};}; /* namespace io::file */
namespace callable$n { 
struct tcallable$i$p$meta$n$$void$t$q$m$ll$r$;
struct callable$i$p$meta$n$$void$t$q$m$ll$r$;
}; /* namespace callable */
namespace text$n { namespace string$n { namespace serialize$n { 
struct ser_default$s;
};};}; /* namespace text::string::serialize */
namespace io$n { namespace process$n { 
struct pipe_process$s;
struct wait_t$s;
};}; /* namespace io::process */
namespace thread$n { namespace queue$n { 
struct task_queue$s;
struct task_queue_shared$s;
};}; /* namespace thread::queue */
namespace exception$n { 
struct unexpected_value_template$s$p$io$$errno$n$$errno_t$s$r$;
}; /* namespace exception */
namespace io$n { namespace errno$n { 
struct errno_or_value$v$p$io$$file$n$$file_st$s$r$;
struct errno_or_value$v$p$io$$file$n$$file_mt$s$r$;
struct errno_or_value$v$p$meta$n$$size_t$t$r$;
struct errno_or_value$v$p$io$$file$n$$off_t$s$r$;
};}; /* namespace io::errno */
namespace exception$n { 
struct unexpected_value_template$s$p$errno__t$ls$r$;
}; /* namespace exception */
namespace io$n { namespace errno$n { 
struct errno_or_value$v$p$pxcrt$$ptr$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$r$$r$;
};}; /* namespace io::errno */
namespace algebraic$n { 
struct pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$;
}; /* namespace algebraic */
namespace io$n { namespace errno$n { 
struct errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$;
};}; /* namespace io::errno */
namespace algebraic$n { 
struct option$v$p$io$$file$n$$file_st$s$r$;
}; /* namespace algebraic */
namespace io$n { namespace errno$n { 
struct errno_or_value$v$p$io$$process$n$$pid_t$s$r$;
struct errno_or_value$v$p$io$$process$n$$pipe_process$s$r$;
struct errno_or_value$v$p$io$$process$n$$wait_t$s$r$;
struct errno_or_value$v$p$io$$process$n$$status_t$s$r$;
};}; /* namespace io::errno */
namespace callable$n { 
struct tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$;
struct callable_ptr$s$p$meta$n$$void$t$q$m$ll$r$;
}; /* namespace callable */
namespace operator$n { 
struct tuple$s$p$m$ll$p$pxcrt$$tptr$p$thread$$queue$n$$task_queue_shared$s$r$$r$$q$m$ll$r$;
}; /* namespace operator */
namespace thread$n { 
struct thread_main_funcobj$s$p$thread$$queue$n$$queue_thread_main$f$r$;
struct thread$s$p$thread$$queue$n$$queue_thread_main$f$r$;
}; /* namespace thread */
namespace exception$n { 
struct runtime_error_template$s$p$thread__create$ls$r$;
struct runtime_error_template$s$p$thread__join$ls$r$;
}; /* namespace exception */
namespace algebraic$n { 
struct option$v$p$callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$r$;
}; /* namespace algebraic */
namespace exception$n { 
struct unexpected_value_template$s$p$unit$ls$r$;
}; /* namespace exception */
namespace callable$n { 
struct tcallable$i$p$meta$n$$void$t$q$m$ll$r$ {
 virtual ~tcallable$i$p$meta$n$$void$t$q$m$ll$r$() PXC_NOTHROW { }
 virtual size_t refcnt$z() const = 0;
 virtual void incref$z() const = 0;
 virtual void decref$z() const = 0;
 virtual void lock$z() const = 0;
 virtual void unlock$z() const = 0;
 virtual void wait$z() const = 0;
 virtual void notify_one$z() const = 0;
 virtual void notify_all$z() const = 0;
 virtual void __call$f() = 0;
};
struct callable$i$p$meta$n$$void$t$q$m$ll$r$ {
 virtual ~callable$i$p$meta$n$$void$t$q$m$ll$r$() PXC_NOTHROW { }
 virtual size_t refcnt$z() const = 0;
 virtual void incref$z() const = 0;
 virtual void decref$z() const = 0;
 virtual void __call$f() = 0;
};
}; /* namespace callable */
namespace text$n { namespace string$n { namespace serialize$n { 
struct ser_default$s {
 inline ser_default$s();
};
};};}; /* namespace text::string::serialize */
namespace io$n { namespace process$n { 
struct pipe_process$s {
 pxcrt::pid_t pid$; // localdecl
 pxcrt::file_mt file$; // localdecl
 inline pipe_process$s(pxcrt::pid_t pid0$, const pxcrt::file_mt& file0$);
};
struct wait_t$s {
 pxcrt::pid_t pid$; // localdecl
 int status$; // localdecl
 inline wait_t$s();
 inline wait_t$s(pxcrt::pid_t pid$, int status$);
};
};}; /* namespace io::process */
namespace thread$n { namespace queue$n { 
struct task_queue$s {
 pxcrt::rcptr< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > > shared$; // localdecl
 ::pxcrt::pxcvarray< ::callable$n::callable_ptr$s$p$meta$n$$void$t$q$m$ll$r$ > thrs$; // localdecl
 void push$f(const ::callable$n::tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$& e$);
 ~task_queue$s() PXC_NOTHROW;
 void construct$f(pxcrt::bt_size_t num_thrs$);
 inline task_queue$s(pxcrt::bt_size_t num_thrs$);
};
struct task_queue_shared$s {
 ::pxcrt::pxcdeque< ::callable$n::tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$ > queue$; // localdecl
 pxcrt::bt_int stop_thread$; // localdecl
 inline task_queue_shared$s();
};
};}; /* namespace thread::queue */
namespace exception$n { 
struct unexpected_value_template$s$p$io$$errno$n$$errno_t$s$r$ : pxcrt::runtime_error {
 ~unexpected_value_template$s$p$io$$errno$n$$errno_t$s$r$() PXC_NOTHROW { }
 size_t refcnt$z() const { return count$z.refcnt$z(); }
 void incref$z() const { count$z.incref$z(); }
 void decref$z() const { if (count$z.decref$z()) { this->~unexpected_value_template$s$p$io$$errno$n$$errno_t$s$r$(); this->deallocate(this); } }
 static unexpected_value_template$s$p$io$$errno$n$$errno_t$s$r$ *allocate() { return pxcrt::allocate_single< unexpected_value_template$s$p$io$$errno$n$$errno_t$s$r$ >(); }
 static void deallocate(unexpected_value_template$s$p$io$$errno$n$$errno_t$s$r$ const *ptr) { pxcrt::deallocate_single< unexpected_value_template$s$p$io$$errno$n$$errno_t$s$r$ >(ptr); }
 void lock$z() const { monitor$z.mtx.lock(); }
 void unlock$z() const { monitor$z.mtx.unlock(); }
 void wait$z() const { monitor$z.cond.wait(monitor$z.mtx); }
 void notify_one$z() const { monitor$z.cond.notify_one(); }
 void notify_all$z() const { monitor$z.cond.notify_all(); }
 mutable pxcrt::monitor monitor$z;
 mutable pxcrt::mtcount count$z;
 ::pxcrt::pxcvarray< pxcrt::bt_uchar > msg$; // localdecl
 inline ::pxcrt::pxcvarray< pxcrt::bt_uchar > message() const;
 inline unexpected_value_template$s$p$io$$errno$n$$errno_t$s$r$(const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& m$);
};
}; /* namespace exception */
namespace io$n { namespace errno$n { 
struct errno_or_value$v$p$io$$file$n$$file_st$s$r$ {
 typedef int errno$$ut;
 typedef pxcrt::file_st value$$ut;
 union {
  char errno$$u[sizeof(errno$$ut)] __attribute__((__aligned__(__alignof__(errno$$ut))));
  char value$$u[sizeof(value$$ut)] __attribute__((__aligned__(__alignof__(value$$ut))));
 } _$u;
 errno$$ut const *errno$$p() const { return (errno$$ut const *)_$u.errno$$u; }
 errno$$ut *errno$$p() { return (errno$$ut *)_$u.errno$$u; }
 value$$ut const *value$$p() const { return (value$$ut const *)_$u.value$$u; }
 value$$ut *value$$p() { return (value$$ut *)_$u.value$$u; }
 typedef enum {errno$$e, value$$e} type_$e; type_$e _$e;
 inline type_$e get_$e() const { return _$e; }
 errno_or_value$v$p$io$$file$n$$file_st$s$r$() {
  _$e = errno$$e;
  *(errno$$p()) = int();
 };
 ~errno_or_value$v$p$io$$file$n$$file_st$s$r$() PXC_NOTHROW { deinit$(); }
 errno_or_value$v$p$io$$file$n$$file_st$s$r$(const errno_or_value$v$p$io$$file$n$$file_st$s$r$& x) { init$(x); }
 errno_or_value$v$p$io$$file$n$$file_st$s$r$& operator =(const errno_or_value$v$p$io$$file$n$$file_st$s$r$& x) { if (this != &x) { deinit$(); init$(x); } return *this; }
 inline void init$(const errno_or_value$v$p$io$$file$n$$file_st$s$r$ & x); inline void deinit$();
 inline int errno$$r() const;
 inline pxcrt::file_st value$$r() const;
 inline int errno$$l(int x);
 inline pxcrt::file_st value$$l(pxcrt::file_st x);
};
struct errno_or_value$v$p$io$$file$n$$file_mt$s$r$ {
 typedef int errno$$ut;
 typedef pxcrt::file_mt value$$ut;
 union {
  char errno$$u[sizeof(errno$$ut)] __attribute__((__aligned__(__alignof__(errno$$ut))));
  char value$$u[sizeof(value$$ut)] __attribute__((__aligned__(__alignof__(value$$ut))));
 } _$u;
 errno$$ut const *errno$$p() const { return (errno$$ut const *)_$u.errno$$u; }
 errno$$ut *errno$$p() { return (errno$$ut *)_$u.errno$$u; }
 value$$ut const *value$$p() const { return (value$$ut const *)_$u.value$$u; }
 value$$ut *value$$p() { return (value$$ut *)_$u.value$$u; }
 typedef enum {errno$$e, value$$e} type_$e; type_$e _$e;
 inline type_$e get_$e() const { return _$e; }
 errno_or_value$v$p$io$$file$n$$file_mt$s$r$() {
  _$e = errno$$e;
  *(errno$$p()) = int();
 };
 ~errno_or_value$v$p$io$$file$n$$file_mt$s$r$() PXC_NOTHROW { deinit$(); }
 errno_or_value$v$p$io$$file$n$$file_mt$s$r$(const errno_or_value$v$p$io$$file$n$$file_mt$s$r$& x) { init$(x); }
 errno_or_value$v$p$io$$file$n$$file_mt$s$r$& operator =(const errno_or_value$v$p$io$$file$n$$file_mt$s$r$& x) { if (this != &x) { deinit$(); init$(x); } return *this; }
 inline void init$(const errno_or_value$v$p$io$$file$n$$file_mt$s$r$ & x); inline void deinit$();
 inline int errno$$r() const;
 inline pxcrt::file_mt value$$r() const;
 inline int errno$$l(int x);
 inline pxcrt::file_mt value$$l(pxcrt::file_mt x);
};
struct errno_or_value$v$p$meta$n$$size_t$t$r$ {
 typedef int errno$$ut;
 typedef pxcrt::bt_size_t value$$ut;
 union {
  char errno$$u[sizeof(errno$$ut)] __attribute__((__aligned__(__alignof__(errno$$ut))));
  char value$$u[sizeof(value$$ut)] __attribute__((__aligned__(__alignof__(value$$ut))));
 } _$u;
 errno$$ut const *errno$$p() const { return (errno$$ut const *)_$u.errno$$u; }
 errno$$ut *errno$$p() { return (errno$$ut *)_$u.errno$$u; }
 value$$ut const *value$$p() const { return (value$$ut const *)_$u.value$$u; }
 value$$ut *value$$p() { return (value$$ut *)_$u.value$$u; }
 typedef enum {errno$$e, value$$e} type_$e; type_$e _$e;
 inline type_$e get_$e() const { return _$e; }
 errno_or_value$v$p$meta$n$$size_t$t$r$() {
  _$e = errno$$e;
  *(errno$$p()) = int();
 };
 ~errno_or_value$v$p$meta$n$$size_t$t$r$() PXC_NOTHROW { deinit$(); }
 errno_or_value$v$p$meta$n$$size_t$t$r$(const errno_or_value$v$p$meta$n$$size_t$t$r$& x) { init$(x); }
 errno_or_value$v$p$meta$n$$size_t$t$r$& operator =(const errno_or_value$v$p$meta$n$$size_t$t$r$& x) { if (this != &x) { deinit$(); init$(x); } return *this; }
 inline void init$(const errno_or_value$v$p$meta$n$$size_t$t$r$ & x); inline void deinit$();
 inline int errno$$r() const;
 inline pxcrt::bt_size_t value$$r() const;
 inline int errno$$l(int x);
 inline pxcrt::bt_size_t value$$l(pxcrt::bt_size_t x);
};
struct errno_or_value$v$p$io$$file$n$$off_t$s$r$ {
 typedef int errno$$ut;
 typedef pxcrt::off_t value$$ut;
 union {
  char errno$$u[sizeof(errno$$ut)] __attribute__((__aligned__(__alignof__(errno$$ut))));
  char value$$u[sizeof(value$$ut)] __attribute__((__aligned__(__alignof__(value$$ut))));
 } _$u;
 errno$$ut const *errno$$p() const { return (errno$$ut const *)_$u.errno$$u; }
 errno$$ut *errno$$p() { return (errno$$ut *)_$u.errno$$u; }
 value$$ut const *value$$p() const { return (value$$ut const *)_$u.value$$u; }
 value$$ut *value$$p() { return (value$$ut *)_$u.value$$u; }
 typedef enum {errno$$e, value$$e} type_$e; type_$e _$e;
 inline type_$e get_$e() const { return _$e; }
 errno_or_value$v$p$io$$file$n$$off_t$s$r$() {
  _$e = errno$$e;
  *(errno$$p()) = int();
 };
 ~errno_or_value$v$p$io$$file$n$$off_t$s$r$() PXC_NOTHROW { deinit$(); }
 errno_or_value$v$p$io$$file$n$$off_t$s$r$(const errno_or_value$v$p$io$$file$n$$off_t$s$r$& x) { init$(x); }
 errno_or_value$v$p$io$$file$n$$off_t$s$r$& operator =(const errno_or_value$v$p$io$$file$n$$off_t$s$r$& x) { if (this != &x) { deinit$(); init$(x); } return *this; }
 inline void init$(const errno_or_value$v$p$io$$file$n$$off_t$s$r$ & x); inline void deinit$();
 inline int errno$$r() const;
 inline pxcrt::off_t value$$r() const;
 inline int errno$$l(int x);
 inline pxcrt::off_t value$$l(pxcrt::off_t x);
};
};}; /* namespace io::errno */
namespace exception$n { 
struct unexpected_value_template$s$p$errno__t$ls$r$ : pxcrt::runtime_error {
 ~unexpected_value_template$s$p$errno__t$ls$r$() PXC_NOTHROW { }
 size_t refcnt$z() const { return count$z.refcnt$z(); }
 void incref$z() const { count$z.incref$z(); }
 void decref$z() const { if (count$z.decref$z()) { this->~unexpected_value_template$s$p$errno__t$ls$r$(); this->deallocate(this); } }
 static unexpected_value_template$s$p$errno__t$ls$r$ *allocate() { return pxcrt::allocate_single< unexpected_value_template$s$p$errno__t$ls$r$ >(); }
 static void deallocate(unexpected_value_template$s$p$errno__t$ls$r$ const *ptr) { pxcrt::deallocate_single< unexpected_value_template$s$p$errno__t$ls$r$ >(ptr); }
 void lock$z() const { monitor$z.mtx.lock(); }
 void unlock$z() const { monitor$z.mtx.unlock(); }
 void wait$z() const { monitor$z.cond.wait(monitor$z.mtx); }
 void notify_one$z() const { monitor$z.cond.notify_one(); }
 void notify_all$z() const { monitor$z.cond.notify_all(); }
 mutable pxcrt::monitor monitor$z;
 mutable pxcrt::mtcount count$z;
 ::pxcrt::pxcvarray< pxcrt::bt_uchar > msg$; // localdecl
 inline ::pxcrt::pxcvarray< pxcrt::bt_uchar > message() const;
 inline unexpected_value_template$s$p$errno__t$ls$r$(const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& m$);
};
}; /* namespace exception */
namespace io$n { namespace errno$n { 
struct errno_or_value$v$p$pxcrt$$ptr$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$r$$r$ {
 typedef int errno$$ut;
 typedef pxcrt::rcptr< pxcrt::rcval< ::pxcrt::pxcvarray< pxcrt::bt_uchar > > > value$$ut;
 union {
  char errno$$u[sizeof(errno$$ut)] __attribute__((__aligned__(__alignof__(errno$$ut))));
  char value$$u[sizeof(value$$ut)] __attribute__((__aligned__(__alignof__(value$$ut))));
 } _$u;
 errno$$ut const *errno$$p() const { return (errno$$ut const *)_$u.errno$$u; }
 errno$$ut *errno$$p() { return (errno$$ut *)_$u.errno$$u; }
 value$$ut const *value$$p() const { return (value$$ut const *)_$u.value$$u; }
 value$$ut *value$$p() { return (value$$ut *)_$u.value$$u; }
 typedef enum {errno$$e, value$$e} type_$e; type_$e _$e;
 inline type_$e get_$e() const { return _$e; }
 errno_or_value$v$p$pxcrt$$ptr$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$r$$r$() {
  _$e = errno$$e;
  *(errno$$p()) = int();
 };
 ~errno_or_value$v$p$pxcrt$$ptr$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$r$$r$() PXC_NOTHROW { deinit$(); }
 errno_or_value$v$p$pxcrt$$ptr$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$r$$r$(const errno_or_value$v$p$pxcrt$$ptr$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$r$$r$& x) { init$(x); }
 errno_or_value$v$p$pxcrt$$ptr$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$r$$r$& operator =(const errno_or_value$v$p$pxcrt$$ptr$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$r$$r$& x) { if (this != &x) { deinit$(); init$(x); } return *this; }
 inline void init$(const errno_or_value$v$p$pxcrt$$ptr$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$r$$r$ & x); inline void deinit$();
 inline int errno$$r() const;
 inline pxcrt::rcptr< pxcrt::rcval< ::pxcrt::pxcvarray< pxcrt::bt_uchar > > > value$$r() const;
 inline int errno$$l(int x);
 inline pxcrt::rcptr< pxcrt::rcval< ::pxcrt::pxcvarray< pxcrt::bt_uchar > > > value$$l(pxcrt::rcptr< pxcrt::rcval< ::pxcrt::pxcvarray< pxcrt::bt_uchar > > > x);
};
};}; /* namespace io::errno */
namespace algebraic$n { 
struct pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$ {
 pxcrt::file_mt first$; // localdecl
 pxcrt::file_mt second$; // localdecl
 inline pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$(const pxcrt::file_mt& a0$, const pxcrt::file_mt& a1$);
};
}; /* namespace algebraic */
namespace io$n { namespace errno$n { 
struct errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$ {
 typedef int errno$$ut;
 typedef ::algebraic$n::pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$ value$$ut;
 union {
  char errno$$u[sizeof(errno$$ut)] __attribute__((__aligned__(__alignof__(errno$$ut))));
  char value$$u[sizeof(value$$ut)] __attribute__((__aligned__(__alignof__(value$$ut))));
 } _$u;
 errno$$ut const *errno$$p() const { return (errno$$ut const *)_$u.errno$$u; }
 errno$$ut *errno$$p() { return (errno$$ut *)_$u.errno$$u; }
 value$$ut const *value$$p() const { return (value$$ut const *)_$u.value$$u; }
 value$$ut *value$$p() { return (value$$ut *)_$u.value$$u; }
 typedef enum {errno$$e, value$$e} type_$e; type_$e _$e;
 inline type_$e get_$e() const { return _$e; }
 errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$() {
  _$e = errno$$e;
  *(errno$$p()) = int();
 };
 ~errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$() PXC_NOTHROW { deinit$(); }
 errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$(const errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$& x) { init$(x); }
 errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$& operator =(const errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$& x) { if (this != &x) { deinit$(); init$(x); } return *this; }
 inline void init$(const errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$ & x); inline void deinit$();
 inline int errno$$r() const;
 inline ::algebraic$n::pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$ value$$r() const;
 inline int errno$$l(int x);
 inline ::algebraic$n::pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$ value$$l(::algebraic$n::pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$ x);
};
};}; /* namespace io::errno */
namespace algebraic$n { 
struct option$v$p$io$$file$n$$file_st$s$r$ {
 typedef pxcrt::file_st some$$ut;
 union {
  char some$$u[sizeof(some$$ut)] __attribute__((__aligned__(__alignof__(some$$ut))));
 } _$u;
 some$$ut const *some$$p() const { return (some$$ut const *)_$u.some$$u; }
 some$$ut *some$$p() { return (some$$ut *)_$u.some$$u; }
 typedef enum {none$$e, some$$e} type_$e; type_$e _$e;
 inline type_$e get_$e() const { return _$e; }
 option$v$p$io$$file$n$$file_st$s$r$() {
  _$e = none$$e;
  /* unit */;
 };
 ~option$v$p$io$$file$n$$file_st$s$r$() PXC_NOTHROW { deinit$(); }
 option$v$p$io$$file$n$$file_st$s$r$(const option$v$p$io$$file$n$$file_st$s$r$& x) { init$(x); }
 option$v$p$io$$file$n$$file_st$s$r$& operator =(const option$v$p$io$$file$n$$file_st$s$r$& x) { if (this != &x) { deinit$(); init$(x); } return *this; }
 inline void init$(const option$v$p$io$$file$n$$file_st$s$r$ & x); inline void deinit$();
 inline pxcrt::bt_unit none$$r() const;
 inline pxcrt::file_st some$$r() const;
 inline pxcrt::bt_unit none$$l(pxcrt::bt_unit x);
 inline pxcrt::file_st some$$l(pxcrt::file_st x);
};
}; /* namespace algebraic */
namespace io$n { namespace errno$n { 
struct errno_or_value$v$p$io$$process$n$$pid_t$s$r$ {
 typedef int errno$$ut;
 typedef pxcrt::pid_t value$$ut;
 union {
  char errno$$u[sizeof(errno$$ut)] __attribute__((__aligned__(__alignof__(errno$$ut))));
  char value$$u[sizeof(value$$ut)] __attribute__((__aligned__(__alignof__(value$$ut))));
 } _$u;
 errno$$ut const *errno$$p() const { return (errno$$ut const *)_$u.errno$$u; }
 errno$$ut *errno$$p() { return (errno$$ut *)_$u.errno$$u; }
 value$$ut const *value$$p() const { return (value$$ut const *)_$u.value$$u; }
 value$$ut *value$$p() { return (value$$ut *)_$u.value$$u; }
 typedef enum {errno$$e, value$$e} type_$e; type_$e _$e;
 inline type_$e get_$e() const { return _$e; }
 errno_or_value$v$p$io$$process$n$$pid_t$s$r$() {
  _$e = errno$$e;
  *(errno$$p()) = int();
 };
 ~errno_or_value$v$p$io$$process$n$$pid_t$s$r$() PXC_NOTHROW { deinit$(); }
 errno_or_value$v$p$io$$process$n$$pid_t$s$r$(const errno_or_value$v$p$io$$process$n$$pid_t$s$r$& x) { init$(x); }
 errno_or_value$v$p$io$$process$n$$pid_t$s$r$& operator =(const errno_or_value$v$p$io$$process$n$$pid_t$s$r$& x) { if (this != &x) { deinit$(); init$(x); } return *this; }
 inline void init$(const errno_or_value$v$p$io$$process$n$$pid_t$s$r$ & x); inline void deinit$();
 inline int errno$$r() const;
 inline pxcrt::pid_t value$$r() const;
 inline int errno$$l(int x);
 inline pxcrt::pid_t value$$l(pxcrt::pid_t x);
};
struct errno_or_value$v$p$io$$process$n$$pipe_process$s$r$ {
 typedef int errno$$ut;
 typedef ::io$n::process$n::pipe_process$s value$$ut;
 union {
  char errno$$u[sizeof(errno$$ut)] __attribute__((__aligned__(__alignof__(errno$$ut))));
  char value$$u[sizeof(value$$ut)] __attribute__((__aligned__(__alignof__(value$$ut))));
 } _$u;
 errno$$ut const *errno$$p() const { return (errno$$ut const *)_$u.errno$$u; }
 errno$$ut *errno$$p() { return (errno$$ut *)_$u.errno$$u; }
 value$$ut const *value$$p() const { return (value$$ut const *)_$u.value$$u; }
 value$$ut *value$$p() { return (value$$ut *)_$u.value$$u; }
 typedef enum {errno$$e, value$$e} type_$e; type_$e _$e;
 inline type_$e get_$e() const { return _$e; }
 errno_or_value$v$p$io$$process$n$$pipe_process$s$r$() {
  _$e = errno$$e;
  *(errno$$p()) = int();
 };
 ~errno_or_value$v$p$io$$process$n$$pipe_process$s$r$() PXC_NOTHROW { deinit$(); }
 errno_or_value$v$p$io$$process$n$$pipe_process$s$r$(const errno_or_value$v$p$io$$process$n$$pipe_process$s$r$& x) { init$(x); }
 errno_or_value$v$p$io$$process$n$$pipe_process$s$r$& operator =(const errno_or_value$v$p$io$$process$n$$pipe_process$s$r$& x) { if (this != &x) { deinit$(); init$(x); } return *this; }
 inline void init$(const errno_or_value$v$p$io$$process$n$$pipe_process$s$r$ & x); inline void deinit$();
 inline int errno$$r() const;
 inline ::io$n::process$n::pipe_process$s value$$r() const;
 inline int errno$$l(int x);
 inline ::io$n::process$n::pipe_process$s value$$l(::io$n::process$n::pipe_process$s x);
};
struct errno_or_value$v$p$io$$process$n$$wait_t$s$r$ {
 typedef int errno$$ut;
 typedef ::io$n::process$n::wait_t$s value$$ut;
 union {
  char errno$$u[sizeof(errno$$ut)] __attribute__((__aligned__(__alignof__(errno$$ut))));
  char value$$u[sizeof(value$$ut)] __attribute__((__aligned__(__alignof__(value$$ut))));
 } _$u;
 errno$$ut const *errno$$p() const { return (errno$$ut const *)_$u.errno$$u; }
 errno$$ut *errno$$p() { return (errno$$ut *)_$u.errno$$u; }
 value$$ut const *value$$p() const { return (value$$ut const *)_$u.value$$u; }
 value$$ut *value$$p() { return (value$$ut *)_$u.value$$u; }
 typedef enum {errno$$e, value$$e} type_$e; type_$e _$e;
 inline type_$e get_$e() const { return _$e; }
 errno_or_value$v$p$io$$process$n$$wait_t$s$r$() {
  _$e = errno$$e;
  *(errno$$p()) = int();
 };
 ~errno_or_value$v$p$io$$process$n$$wait_t$s$r$() PXC_NOTHROW { deinit$(); }
 errno_or_value$v$p$io$$process$n$$wait_t$s$r$(const errno_or_value$v$p$io$$process$n$$wait_t$s$r$& x) { init$(x); }
 errno_or_value$v$p$io$$process$n$$wait_t$s$r$& operator =(const errno_or_value$v$p$io$$process$n$$wait_t$s$r$& x) { if (this != &x) { deinit$(); init$(x); } return *this; }
 inline void init$(const errno_or_value$v$p$io$$process$n$$wait_t$s$r$ & x); inline void deinit$();
 inline int errno$$r() const;
 inline ::io$n::process$n::wait_t$s value$$r() const;
 inline int errno$$l(int x);
 inline ::io$n::process$n::wait_t$s value$$l(::io$n::process$n::wait_t$s x);
};
struct errno_or_value$v$p$io$$process$n$$status_t$s$r$ {
 typedef int errno$$ut;
 typedef int value$$ut;
 union {
  char errno$$u[sizeof(errno$$ut)] __attribute__((__aligned__(__alignof__(errno$$ut))));
  char value$$u[sizeof(value$$ut)] __attribute__((__aligned__(__alignof__(value$$ut))));
 } _$u;
 errno$$ut const *errno$$p() const { return (errno$$ut const *)_$u.errno$$u; }
 errno$$ut *errno$$p() { return (errno$$ut *)_$u.errno$$u; }
 value$$ut const *value$$p() const { return (value$$ut const *)_$u.value$$u; }
 value$$ut *value$$p() { return (value$$ut *)_$u.value$$u; }
 typedef enum {errno$$e, value$$e} type_$e; type_$e _$e;
 inline type_$e get_$e() const { return _$e; }
 errno_or_value$v$p$io$$process$n$$status_t$s$r$() {
  _$e = errno$$e;
  *(errno$$p()) = int();
 };
 ~errno_or_value$v$p$io$$process$n$$status_t$s$r$() PXC_NOTHROW { deinit$(); }
 errno_or_value$v$p$io$$process$n$$status_t$s$r$(const errno_or_value$v$p$io$$process$n$$status_t$s$r$& x) { init$(x); }
 errno_or_value$v$p$io$$process$n$$status_t$s$r$& operator =(const errno_or_value$v$p$io$$process$n$$status_t$s$r$& x) { if (this != &x) { deinit$(); init$(x); } return *this; }
 inline void init$(const errno_or_value$v$p$io$$process$n$$status_t$s$r$ & x); inline void deinit$();
 inline int errno$$r() const;
 inline int value$$r() const;
 inline int errno$$l(int x);
 inline int value$$l(int x);
};
};}; /* namespace io::errno */
namespace callable$n { 
struct tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$ {
 pxcrt::rcptr< ::callable$n::tcallable$i$p$meta$n$$void$t$q$m$ll$r$ > p$; // localdecl
 inline void __call$f() const;
 inline tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$(const pxcrt::rcptr< ::callable$n::tcallable$i$p$meta$n$$void$t$q$m$ll$r$ >& p0$);
};
struct callable_ptr$s$p$meta$n$$void$t$q$m$ll$r$ {
 pxcrt::rcptr< ::callable$n::callable$i$p$meta$n$$void$t$q$m$ll$r$ > p$; // localdecl
 inline void __call$f() const;
 inline callable_ptr$s$p$meta$n$$void$t$q$m$ll$r$(const pxcrt::rcptr< ::callable$n::callable$i$p$meta$n$$void$t$q$m$ll$r$ >& p0$);
};
}; /* namespace callable */
namespace operator$n { 
struct tuple$s$p$m$ll$p$pxcrt$$tptr$p$thread$$queue$n$$task_queue_shared$s$r$$r$$q$m$ll$r$ {
 pxcrt::rcptr< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > > _0$; // localdecl
 inline tuple$s$p$m$ll$p$pxcrt$$tptr$p$thread$$queue$n$$task_queue_shared$s$r$$r$$q$m$ll$r$(const pxcrt::rcptr< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > >& a0$);
};
}; /* namespace operator */
namespace thread$n { 
struct thread_main_funcobj$s$p$thread$$queue$n$$queue_thread_main$f$r$ {
 ::operator$n::tuple$s$p$m$ll$p$pxcrt$$tptr$p$thread$$queue$n$$task_queue_shared$s$r$$r$$q$m$ll$r$ fld$; // localdecl
 pxcrt::bt_unit ret$; // localdecl
 inline void __call$f();
 inline thread_main_funcobj$s$p$thread$$queue$n$$queue_thread_main$f$r$(const pxcrt::rcptr< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > >& tq$);
};
struct thread$s$p$thread$$queue$n$$queue_thread_main$f$r$ : ::callable$n::callable$i$p$meta$n$$void$t$q$m$ll$r$ {
 size_t refcnt$z() const { return count$z; }
 void incref$z() const { ++count$z; }
 void decref$z() const { if (--count$z == 0) { this->~thread$s$p$thread$$queue$n$$queue_thread_main$f$r$(); this->deallocate(this); } }
 static thread$s$p$thread$$queue$n$$queue_thread_main$f$r$ *allocate() { return pxcrt::allocate_single< thread$s$p$thread$$queue$n$$queue_thread_main$f$r$ >(); }
 static void deallocate(thread$s$p$thread$$queue$n$$queue_thread_main$f$r$ const *ptr) { pxcrt::deallocate_single< thread$s$p$thread$$queue$n$$queue_thread_main$f$r$ >(ptr); }
 mutable long count$z;
 ::thread$n::thread_main_funcobj$s$p$thread$$queue$n$$queue_thread_main$f$r$ fobj$; // localdecl
 pxcrt::bt_bool need_join$; // localdecl
 pxcrt::thread_ptr thd$; // localdecl
 inline void init$f();
 inline ~thread$s$p$thread$$queue$n$$queue_thread_main$f$r$() PXC_NOTHROW;
 inline void __call$f();
 inline thread$s$p$thread$$queue$n$$queue_thread_main$f$r$(const pxcrt::rcptr< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > >& tq$);
};
}; /* namespace thread */
namespace exception$n { 
struct runtime_error_template$s$p$thread__create$ls$r$ : pxcrt::runtime_error {
 ~runtime_error_template$s$p$thread__create$ls$r$() PXC_NOTHROW { }
 size_t refcnt$z() const { return count$z.refcnt$z(); }
 void incref$z() const { count$z.incref$z(); }
 void decref$z() const { if (count$z.decref$z()) { this->~runtime_error_template$s$p$thread__create$ls$r$(); this->deallocate(this); } }
 static runtime_error_template$s$p$thread__create$ls$r$ *allocate() { return pxcrt::allocate_single< runtime_error_template$s$p$thread__create$ls$r$ >(); }
 static void deallocate(runtime_error_template$s$p$thread__create$ls$r$ const *ptr) { pxcrt::deallocate_single< runtime_error_template$s$p$thread__create$ls$r$ >(ptr); }
 void lock$z() const { monitor$z.mtx.lock(); }
 void unlock$z() const { monitor$z.mtx.unlock(); }
 void wait$z() const { monitor$z.cond.wait(monitor$z.mtx); }
 void notify_one$z() const { monitor$z.cond.notify_one(); }
 void notify_all$z() const { monitor$z.cond.notify_all(); }
 mutable pxcrt::monitor monitor$z;
 mutable pxcrt::mtcount count$z;
 ::pxcrt::pxcvarray< pxcrt::bt_uchar > msg$; // localdecl
 inline ::pxcrt::pxcvarray< pxcrt::bt_uchar > message() const;
 inline runtime_error_template$s$p$thread__create$ls$r$(const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& m$);
};
struct runtime_error_template$s$p$thread__join$ls$r$ : pxcrt::runtime_error {
 ~runtime_error_template$s$p$thread__join$ls$r$() PXC_NOTHROW { }
 size_t refcnt$z() const { return count$z.refcnt$z(); }
 void incref$z() const { count$z.incref$z(); }
 void decref$z() const { if (count$z.decref$z()) { this->~runtime_error_template$s$p$thread__join$ls$r$(); this->deallocate(this); } }
 static runtime_error_template$s$p$thread__join$ls$r$ *allocate() { return pxcrt::allocate_single< runtime_error_template$s$p$thread__join$ls$r$ >(); }
 static void deallocate(runtime_error_template$s$p$thread__join$ls$r$ const *ptr) { pxcrt::deallocate_single< runtime_error_template$s$p$thread__join$ls$r$ >(ptr); }
 void lock$z() const { monitor$z.mtx.lock(); }
 void unlock$z() const { monitor$z.mtx.unlock(); }
 void wait$z() const { monitor$z.cond.wait(monitor$z.mtx); }
 void notify_one$z() const { monitor$z.cond.notify_one(); }
 void notify_all$z() const { monitor$z.cond.notify_all(); }
 mutable pxcrt::monitor monitor$z;
 mutable pxcrt::mtcount count$z;
 ::pxcrt::pxcvarray< pxcrt::bt_uchar > msg$; // localdecl
 inline ::pxcrt::pxcvarray< pxcrt::bt_uchar > message() const;
 inline runtime_error_template$s$p$thread__join$ls$r$(const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& m$);
};
}; /* namespace exception */
namespace algebraic$n { 
struct option$v$p$callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$r$ {
 typedef ::callable$n::tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$ some$$ut;
 union {
  char some$$u[sizeof(some$$ut)] __attribute__((__aligned__(__alignof__(some$$ut))));
 } _$u;
 some$$ut const *some$$p() const { return (some$$ut const *)_$u.some$$u; }
 some$$ut *some$$p() { return (some$$ut *)_$u.some$$u; }
 typedef enum {none$$e, some$$e} type_$e; type_$e _$e;
 inline type_$e get_$e() const { return _$e; }
 option$v$p$callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$r$() {
  _$e = none$$e;
  /* unit */;
 };
 ~option$v$p$callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$r$() PXC_NOTHROW { deinit$(); }
 option$v$p$callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$r$(const option$v$p$callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$r$& x) { init$(x); }
 option$v$p$callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$r$& operator =(const option$v$p$callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$r$& x) { if (this != &x) { deinit$(); init$(x); } return *this; }
 inline void init$(const option$v$p$callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$r$ & x); inline void deinit$();
 inline pxcrt::bt_unit none$$r() const;
 inline ::callable$n::tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$ some$$r() const;
 inline pxcrt::bt_unit none$$l(pxcrt::bt_unit x);
 inline ::callable$n::tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$ some$$l(::callable$n::tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$ x);
};
}; /* namespace algebraic */
namespace exception$n { 
struct unexpected_value_template$s$p$unit$ls$r$ : pxcrt::runtime_error {
 ~unexpected_value_template$s$p$unit$ls$r$() PXC_NOTHROW { }
 size_t refcnt$z() const { return count$z.refcnt$z(); }
 void incref$z() const { count$z.incref$z(); }
 void decref$z() const { if (count$z.decref$z()) { this->~unexpected_value_template$s$p$unit$ls$r$(); this->deallocate(this); } }
 static unexpected_value_template$s$p$unit$ls$r$ *allocate() { return pxcrt::allocate_single< unexpected_value_template$s$p$unit$ls$r$ >(); }
 static void deallocate(unexpected_value_template$s$p$unit$ls$r$ const *ptr) { pxcrt::deallocate_single< unexpected_value_template$s$p$unit$ls$r$ >(ptr); }
 void lock$z() const { monitor$z.mtx.lock(); }
 void unlock$z() const { monitor$z.mtx.unlock(); }
 void wait$z() const { monitor$z.cond.wait(monitor$z.mtx); }
 void notify_one$z() const { monitor$z.cond.notify_one(); }
 void notify_all$z() const { monitor$z.cond.notify_all(); }
 mutable pxcrt::monitor monitor$z;
 mutable pxcrt::mtcount count$z;
 ::pxcrt::pxcvarray< pxcrt::bt_uchar > msg$; // localdecl
 inline ::pxcrt::pxcvarray< pxcrt::bt_uchar > message() const;
 inline unexpected_value_template$s$p$unit$ls$r$(const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& m$);
};
}; /* namespace exception */
/* inline c hdr */
/* inline */
#include <boost/static_assert.hpp>
namespace numeric {
BOOST_STATIC_ASSERT((FLT_RADIX == 2));
using namespace pxcrt;
static inline bt_int fpclassify_double(bt_double x)
{ return std::fpclassify(x); }
static inline bt_int fpclassify_float(bt_float x)
{ return std::fpclassify(x); }
static inline bt_bool signbit_double(bt_double x)
{ return std::signbit(x) != 0; }
static inline bt_bool signbit_float(bt_float x)
{ return std::signbit(x) != 0; }
static inline bt_double frexp_double(bt_double x, bt_int& exp)
{ return std::frexp(x, &exp); }
static inline bt_float frexp_float(bt_float x, bt_int& exp)
{ return std::frexp(x, &exp); }
static inline bt_double ldexp_double(bt_double x, bt_int exp)
{ return std::ldexp(x, exp); }
static inline bt_float ldexp_float(bt_float x, bt_int exp)
{ return std::ldexp(x, exp); }
static inline bt_double modf_double(bt_double x, bt_double& ir)
{ return std::modf(x, &ir); }
static inline bt_float modf_float(bt_float x, bt_float& ir)
{ return std::modf(x, &ir); }
template <typename T> static inline T sin(T x) { return std::sin(x); }
template <typename T> static inline T cos(T x) { return std::cos(x); }
template <typename T> static inline T tan(T x) { return std::tan(x); }
template <typename T> static inline T asin(T x) { return std::asin(x); }
template <typename T> static inline T acos(T x) { return std::acos(x); }
template <typename T> static inline T atan(T x) { return std::atan(x); }
template <typename T> static inline T sinh(T x) { return std::sinh(x); }
template <typename T> static inline T cosh(T x) { return std::cosh(x); }
template <typename T> static inline T tanh(T x) { return std::tanh(x); }
template <typename T> static inline T exp(T x) { return std::exp(x); }
template <typename T> static inline T log(T x) { return std::log(x); }
template <typename T> static inline T log10(T x) { return std::log10(x); }
template <typename T> static inline T sqrt(T x) { return std::sqrt(x); }
template <typename T> static inline T ceil(T x) { return std::ceil(x); }
template <typename T> static inline T floor(T x) { return std::floor(x); }
template <typename T> static inline T fabs(T x) { return std::fabs(x); }
template <typename T> static inline T pow(T x, T y) { return std::pow(x, y); }
template <typename T> static inline T fmod(T x, T y) { return std::fmod(x, y); }
template <typename T> static inline T atan2(T x, T y) { return std::atan2(x, y); }
template <typename T> static inline bool isfinite(T x) { return std::isfinite(x); }
template <typename T> static inline bool isnormal(T x) { return std::isnormal(x); }
template <typename T> static inline bool isnan(T x) { return std::isnan(x); }
template <typename T> static inline bool isinf(T x) { return std::isinf(x); }

}; // namespace numeric
;
/* inline */
namespace pxcrt {
template <typename T> unsigned int union_tag_impl(const T& x)
{
  return x.get_$e();
}
};
;
/* inline */

#include <algorithm>

namespace pxcrt {

template <typename T, typename Tcmp> inline
void sort_compare(bt_slice<T> const& x)
{
  std::sort(x.rawarr(), x.rawarr() + x.size(), compare_less<Tcmp>());
}

template <typename T, typename Tcmp> inline
void stable_sort_lt(bt_slice<T> const& x)
{
  std::stable_sort(x.rawarr(), x.rawarr() + x.size(), compare_less<Tcmp>());
}

};
;
/* inline */

namespace pxcrt {

bt_string c_exception_message(std::exception const& ex);
bt_string c_exception_stack_trace(std::exception const& ex);
std::string c_exception_to_stdstring(const std::exception& ex);
void c_exception_log_stderr(const std::exception& ex);

}; // namespace pxcrt
;
/* inline */

#include <boost/functional/hash.hpp>

namespace pxcrt {

template <typename T> static inline size_t hash_c(T const& x)
{
  ::boost::hash<T> fobj;
  return fobj(x);
};

}; // namespace pxcrt

;
/* inline */
namespace pxcrt {

extern io io_system;

};
;
/* inline */
#ifdef PXC_WINDOWS
#define EDQUOT -1
#define EMULTIHOP -1
#define ESTALE -1
#endif
;
/* inline */
#include <ctime>
#ifdef PXC_POSIX
#include <sys/time.h>
#endif
;
/* inline */
namespace pxcrt {

static inline bt_int file_fileno(file const& f)
{
  return f.get();
}

file_st make_file_st(int fd); /* can be called from other io::* namespaces */
file_mt make_file_mt(int fd); /* can be called from other io::* namespaces */

};
;
/* inline */
namespace pxcrt {

void sighandler_set_signaled(int sig);

};
;
/* inline */

namespace pxcrt {

template <typename Tfuncobj> static inline void *
funcobj_wrap(void *arg)
{
  Tfuncobj *const a = static_cast<Tfuncobj *>(arg);
  a->__call$f();
  return 0;
}

template <typename Tfuncobj> int
thread_create(thread_ptr& thd, Tfuncobj& funcobj)
{
#ifndef PXC_EMSCRIPTEN
  try {
    thd = new std::thread(&funcobj_wrap<Tfuncobj>, &funcobj);
  } catch (std::system_error const& ex) {
    return ex.code().value();
  } catch (std::exception const& ex) {
    return 1;
  }
#endif
  return 0;
}

template <typename Tfuncobj> int
thread_join(thread_ptr& thd)
{
  if (thd) {
    thd->join();
  }
  return 0;
}

};

;
/* function prototype decls */
namespace exception$n { 
}; /* namespace exception */
extern "C" void _meta$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _meta$$initial$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _numeric$$integral$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _meta$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _numeric$$integral$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _meta$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _meta$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _meta$$vararg$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _numeric$$integral$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _numeric$$fp$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _meta$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _numeric$$integral$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _numeric$$fp$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _meta$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _meta$$family$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _numeric$$integral$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _allocator$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _meta$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _numeric$$integral$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _container$$array$$array_common$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _meta$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _container$$array$$array_common$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _container$$array$$refguard$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _container$$array$$varray$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _container$$array$$varray$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _container$$array$$varray$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _container$$array$$varray$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _container$$array$$varray$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _numeric$$integral$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _numeric$$cast$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _container$$array$$array_common$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _container$$array$$refguard$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _container$$array$$varray$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _container$$array$$svarray$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _container$$array$$farray$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _container$$array$$darray$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _container$$array$$deque$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _container$$array$$util$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _meta$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _meta$$family$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _numeric$$integral$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _numeric$$fp$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
extern "C" void _meta$$nsmain$c();
namespace exception$n { 
}; /* namespace exception */
namespace numeric { 
int fpclassify_double(pxcrt::bt_double x$);
int fpclassify_float(pxcrt::bt_float x$);
pxcrt::bt_bool signbit_double(pxcrt::bt_double x$);
pxcrt::bt_bool signbit_float(pxcrt::bt_float x$);
pxcrt::bt_double frexp_double(pxcrt::bt_double x$, pxcrt::bt_int& exp$);
pxcrt::bt_float frexp_float(pxcrt::bt_float x$, pxcrt::bt_int& exp$);
pxcrt::bt_double ldexp_double(pxcrt::bt_double x$, pxcrt::bt_int exp$);
pxcrt::bt_float ldexp_float(pxcrt::bt_float x$, pxcrt::bt_int exp$);
pxcrt::bt_double modf_double(pxcrt::bt_double x$, pxcrt::bt_double& exp$);
pxcrt::bt_float modf_double(pxcrt::bt_float x$, pxcrt::bt_float& exp$);
}; /* namespace numeric */
extern "C" void _numeric$$integral$$nsmain$c();
namespace numeric { 
}; /* namespace numeric */
extern "C" void _meta$$nsmain$c();
namespace numeric { 
}; /* namespace numeric */
extern "C" void _numeric$$integral$$nsmain$c();
namespace numeric { 
}; /* namespace numeric */
extern "C" void _meta$$nsmain$c();
namespace numeric { 
}; /* namespace numeric */
extern "C" void _meta$$family$$nsmain$c();
namespace numeric { 
}; /* namespace numeric */
extern "C" void _numeric$$integral$$nsmain$c();
namespace numeric { 
}; /* namespace numeric */
extern "C" void _container$$array$$nsmain$c();
namespace numeric { 
}; /* namespace numeric */
extern "C" void _meta$$nsmain$c();
namespace numeric { 
}; /* namespace numeric */
extern "C" void _meta$$family$$nsmain$c();
namespace numeric { 
}; /* namespace numeric */
extern "C" void _numeric$$cast$$nsmain$c();
namespace numeric { 
}; /* namespace numeric */
extern "C" void _generic$$nsmain$c();
namespace numeric { 
}; /* namespace numeric */
extern "C" void _numeric$$integral$$nsmain$c();
namespace numeric { 
}; /* namespace numeric */
extern "C" void _numeric$$integral$$nsmain$c();
namespace numeric { 
}; /* namespace numeric */
extern "C" void _container$$array$$nsmain$c();
namespace numeric { 
}; /* namespace numeric */
extern "C" void _numeric$$cast$$nsmain$c();
namespace numeric { 
}; /* namespace numeric */
extern "C" void _numeric$$limit$$nsmain$c();
namespace numeric { 
}; /* namespace numeric */
extern "C" void _numeric$$fp$$nsmain$c();
namespace numeric { 
}; /* namespace numeric */
extern "C" void _meta$$nsmain$c();
namespace numeric { 
}; /* namespace numeric */
namespace text { 
void fp_to_decimal_float(pxcrt::bt_float v$, ::pxcrt::pxcvarray< pxcrt::bt_uchar >& buf$);
void fp_to_decimal_double(pxcrt::bt_double v$, ::pxcrt::pxcvarray< pxcrt::bt_uchar >& buf$);
pxcrt::bt_float decimal_to_fp_nocheck_float(const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& buf$);
pxcrt::bt_double decimal_to_fp_nocheck_double(const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& buf$);
pxcrt::bt_float decimal_to_fp_float(::pxcrt::bt_cslice< pxcrt::bt_uchar >& buf$);
pxcrt::bt_double decimal_to_fp_double(::pxcrt::bt_cslice< pxcrt::bt_uchar >& buf$);
}; /* namespace text */
extern "C" void _numeric$$integral$$nsmain$c();
namespace text { 
}; /* namespace text */
extern "C" void _container$$array$$nsmain$c();
namespace text { 
}; /* namespace text */
namespace pxcrt { 
void minimal_encode(const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& src$, ::pxcrt::pxcvarray< pxcrt::bt_uchar >& buf$);
void minimal_decode(const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& src$, ::pxcrt::pxcvarray< pxcrt::bt_uchar >& buf$);
}; /* namespace pxcrt */
extern "C" void _numeric$$integral$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _container$$array$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _text$$serialize$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _generic$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _meta$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _meta$$family$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _numeric$$cast$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _text$$string$$positional$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _text$$string$$minimal_encode$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
namespace text$n { namespace string$n { namespace serialize$n { 
void unit_to_string$f(pxcrt::bt_unit v$, ::pxcrt::pxcvarray< pxcrt::bt_uchar >& buf$);
void bool_to_string$f(pxcrt::bt_bool v$, ::pxcrt::pxcvarray< pxcrt::bt_uchar >& buf$);
void string_append_none$f(::pxcrt::pxcvarray< pxcrt::bt_uchar >& v$);
pxcrt::bt_size_t token_length$f(const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& s$);
pxcrt::bt_size_t find_brace_close$f(const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& s$);
::pxcrt::pxcvarray< pxcrt::bt_uchar > str_decode$f83(const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& stok$);
::pxcrt::pxcvarray< pxcrt::bt_uchar > string_parse_string_unescape$f(::pxcrt::bt_cslice< pxcrt::bt_uchar >& s$);
pxcrt::bt_bool parse_bool$f87(const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& stok$);
pxcrt::bt_bool string_parse_bool$f(::pxcrt::bt_cslice< pxcrt::bt_uchar >& s$);
pxcrt::bt_unit string_parse_unit$f(::pxcrt::bt_cslice< pxcrt::bt_uchar >& s$);
pxcrt::bt_size_t string_parse_symbol_peek$f(::pxcrt::bt_cslice< pxcrt::bt_uchar >& stok$);
void string_parse_symbol$f(::pxcrt::bt_cslice< pxcrt::bt_uchar >& stok$, ::pxcrt::bt_cslice< pxcrt::bt_uchar >& s$);
void string_parse_token_skip$f(::pxcrt::bt_cslice< pxcrt::bt_uchar >& s$);
};};}; /* namespace text::string::serialize */
extern "C" void _numeric$$integral$$nsmain$c();
namespace text$n { namespace string$n { namespace serialize$n { 
};};}; /* namespace text::string::serialize */
extern "C" void _numeric$$fp$$nsmain$c();
namespace text$n { namespace string$n { namespace serialize$n { 
};};}; /* namespace text::string::serialize */
extern "C" void _numeric$$fpmath$$nsmain$c();
namespace text$n { namespace string$n { namespace serialize$n { 
};};}; /* namespace text::string::serialize */
extern "C" void _numeric$$union_tag$$nsmain$c();
namespace text$n { namespace string$n { namespace serialize$n { 
};};}; /* namespace text::string::serialize */
extern "C" void _container$$array$$nsmain$c();
namespace text$n { namespace string$n { namespace serialize$n { 
};};}; /* namespace text::string::serialize */
extern "C" void _meta$$nsmain$c();
namespace text$n { namespace string$n { namespace serialize$n { 
};};}; /* namespace text::string::serialize */
extern "C" void _meta$$family$$nsmain$c();
namespace text$n { namespace string$n { namespace serialize$n { 
};};}; /* namespace text::string::serialize */
extern "C" void _text$$string$$serialize$$nsmain$c();
namespace text$n { namespace string$n { namespace serialize$n { 
};};}; /* namespace text::string::serialize */
extern "C" void _container$$array$$varray$$nsmain$c();
namespace text$n { namespace string$n { namespace serialize$n { 
};};}; /* namespace text::string::serialize */
extern "C" void _ordered$$nsmain$c();
namespace text$n { namespace string$n { namespace serialize$n { 
};};}; /* namespace text::string::serialize */
extern "C" void _numeric$$integral$$nsmain$c();
namespace text$n { namespace string$n { namespace serialize$n { 
};};}; /* namespace text::string::serialize */
extern "C" void _container$$array$$nsmain$c();
namespace text$n { namespace string$n { namespace serialize$n { 
};};}; /* namespace text::string::serialize */
extern "C" void _ordered$$nsmain$c();
namespace text$n { namespace string$n { namespace serialize$n { 
};};}; /* namespace text::string::serialize */
extern "C" void _container$$tree_map$$tree_map$$nsmain$c();
namespace text$n { namespace string$n { namespace serialize$n { 
};};}; /* namespace text::string::serialize */
extern "C" void _numeric$$integral$$nsmain$c();
namespace text$n { namespace string$n { namespace serialize$n { 
};};}; /* namespace text::string::serialize */
extern "C" void _container$$array$$nsmain$c();
namespace text$n { namespace string$n { namespace serialize$n { 
};};}; /* namespace text::string::serialize */
extern "C" void _exception$$impl$$nsmain$c();
namespace text$n { namespace string$n { namespace serialize$n { 
};};}; /* namespace text::string::serialize */
extern "C" void _numeric$$integral$$nsmain$c();
namespace text$n { namespace string$n { namespace serialize$n { 
};};}; /* namespace text::string::serialize */
extern "C" void _container$$array$$nsmain$c();
namespace text$n { namespace string$n { namespace serialize$n { 
};};}; /* namespace text::string::serialize */
extern "C" void _meta$$nsmain$c();
namespace text$n { namespace string$n { namespace serialize$n { 
};};}; /* namespace text::string::serialize */
namespace pxcrt { 
::pxcrt::pxcvarray< pxcrt::bt_uchar > c_exception_message(const std::exception& ex$);
::pxcrt::pxcvarray< pxcrt::bt_uchar > c_exception_stack_trace(const std::exception& ex$);
::pxcrt::pxcvarray< pxcrt::bt_uchar > exception_message(const pxcrt::exception& ex$);
::pxcrt::pxcvarray< pxcrt::bt_uchar > exception_stack_trace(const pxcrt::exception& ex$);
}; /* namespace pxcrt */
namespace exception$n { 
void c_exception_append_to_string$f(const std::exception& ex$, ::pxcrt::pxcvarray< pxcrt::bt_uchar >& str$);
void exception_append_to_string$f(const pxcrt::exception& ex$, ::pxcrt::pxcvarray< pxcrt::bt_uchar >& str$);
}; /* namespace exception */
namespace pxcrt { 
void set_stack_trace_limit(pxcrt::bt_size_t sz$);
}; /* namespace pxcrt */
extern "C" void _numeric$$integral$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _container$$array$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _meta$$family$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _exception$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _text$$string$$serialize$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _meta$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _meta$$vararg$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _numeric$$integral$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _numeric$$fp$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _numeric$$fpmath$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _numeric$$union_tag$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _container$$array$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _ordered$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _meta$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
namespace equality$n { 
void hash_combine$f(pxcrt::bt_size_t& x$, pxcrt::bt_size_t v$);
}; /* namespace equality */
extern "C" void _container$$array$$varray$$nsmain$c();
namespace equality$n { 
}; /* namespace equality */
extern "C" void _numeric$$integral$$nsmain$c();
namespace equality$n { 
}; /* namespace equality */
extern "C" void _numeric$$cast$$nsmain$c();
namespace equality$n { 
}; /* namespace equality */
extern "C" void _exception$$nsmain$c();
namespace equality$n { 
}; /* namespace equality */
extern "C" void _meta$$nsmain$c();
namespace equality$n { 
}; /* namespace equality */
extern "C" void _operator$$nsmain$c();
namespace equality$n { 
}; /* namespace equality */
extern "C" void _equality$$nsmain$c();
namespace equality$n { 
}; /* namespace equality */
extern "C" void _numeric$$integral$$nsmain$c();
namespace equality$n { 
}; /* namespace equality */
extern "C" void _meta$$nsmain$c();
namespace equality$n { 
}; /* namespace equality */
extern "C" void _meta$$vararg$$nsmain$c();
namespace equality$n { 
}; /* namespace equality */
extern "C" void _meta$$family$$nsmain$c();
namespace equality$n { 
}; /* namespace equality */
extern "C" void _allocator$$nsmain$c();
namespace equality$n { 
}; /* namespace equality */
extern "C" void _numeric$$integral$$nsmain$c();
namespace equality$n { 
}; /* namespace equality */
extern "C" void _meta$$nsmain$c();
namespace equality$n { 
}; /* namespace equality */
extern "C" void _meta$$family$$nsmain$c();
namespace equality$n { 
}; /* namespace equality */
extern "C" void _container$$array$$nsmain$c();
namespace equality$n { 
}; /* namespace equality */
extern "C" void _numeric$$integral$$nsmain$c();
namespace equality$n { 
}; /* namespace equality */
extern "C" void _numeric$$cast$$nsmain$c();
namespace equality$n { 
}; /* namespace equality */
extern "C" void _text$$string$$serialize$$nsmain$c();
namespace equality$n { 
}; /* namespace equality */
extern "C" void _meta$$nsmain$c();
namespace equality$n { 
}; /* namespace equality */
extern "C" void _meta$$vararg$$nsmain$c();
namespace equality$n { 
}; /* namespace equality */
extern "C" void _meta$$family$$nsmain$c();
namespace equality$n { 
}; /* namespace equality */
namespace pxcrt { 
pxcrt::io debug_system();
}; /* namespace pxcrt */
extern "C" void _numeric$$integral$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _container$$array$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _text$$string$$serialize$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _meta$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _io$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _exception$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _pointer$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
namespace io$n { namespace errno$n { 
int io_get_errno$f(const pxcrt::io& sys$);
void io_set_errno$f(const pxcrt::io& sys$, int e$);
void errno_t_append_to_string$f(int e$, ::pxcrt::pxcvarray< pxcrt::bt_uchar >& s$);
void errno_t_check$f(int e$);
};}; /* namespace io::errno */
extern "C" void _numeric$$integral$$nsmain$c();
namespace io$n { namespace errno$n { 
};}; /* namespace io::errno */
extern "C" void _numeric$$fp$$nsmain$c();
namespace io$n { namespace errno$n { 
};}; /* namespace io::errno */
extern "C" void _numeric$$cast$$nsmain$c();
namespace io$n { namespace errno$n { 
};}; /* namespace io::errno */
extern "C" void _io$$nsmain$c();
namespace io$n { namespace errno$n { 
};}; /* namespace io::errno */
namespace pxcrt { 
::timeval gettimeofday(const pxcrt::io& iop$);
std::time_t time(const pxcrt::io& iop$);
::pxcrt::bt_ulonglong high_resolution_timer(const pxcrt::io& iop$);
::pxcrt::bt_ulonglong high_resolution_timer_frequency(const pxcrt::io& iop$);
pxcrt::bt_uint io_sleep(const pxcrt::io& iop$, pxcrt::bt_uint sec$);
pxcrt::bt_uint io_usleep(const pxcrt::io& iop$, pxcrt::bt_uint usec$);
}; /* namespace pxcrt */
namespace io$n { namespace time$n { 
pxcrt::bt_double io_gettimeofday_double$f(const pxcrt::io& iop$);
};}; /* namespace io::time */
namespace pxcrt { 
void time_init();
}; /* namespace pxcrt */
extern "C" void _numeric$$integral$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _numeric$$cast$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _container$$array$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _pointer$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _io$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _io$$errno$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _io$$time$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _exception$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _operator$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _algebraic$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _meta$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _meta$$vararg$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
namespace io$n { namespace file$n { 
::io$n::errno$n::errno_or_value$v$p$io$$file$n$$file_st$s$r$ io_open_st$f(const pxcrt::io& iop$, const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& fn$, int flags$, pxcrt::mode_t  md$);
::io$n::errno$n::errno_or_value$v$p$io$$file$n$$file_mt$s$r$ io_open_mt$f(const pxcrt::io& iop$, const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& fn$, int flags$, pxcrt::mode_t  md$);
};}; /* namespace io::file */
namespace pxcrt { 
pxcrt::bt_int io_open_fd(const pxcrt::io& iop$, const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& fn$, int flags$, pxcrt::mode_t  md$);
pxcrt::file_st io_make_file_st(const pxcrt::io& iop$, pxcrt::bt_int fd$);
pxcrt::file_mt io_make_file_mt(const pxcrt::io& iop$, pxcrt::bt_int fd$);
pxcrt::bt_int file_fileno(const pxcrt::file& f$);
int file_read_impl(const pxcrt::file& f$, ::pxcrt::pxcvarray< pxcrt::bt_uchar >& buf$, pxcrt::bt_size_t& len$);
}; /* namespace pxcrt */
namespace io$n { namespace file$n { 
::io$n::errno$n::errno_or_value$v$p$meta$n$$size_t$t$r$ file_read$f(const pxcrt::file& f$, ::pxcrt::pxcvarray< pxcrt::bt_uchar >& buf$, pxcrt::bt_size_t len$);
};}; /* namespace io::file */
namespace pxcrt { 
int file_write_impl(const pxcrt::file& f$, const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& buf$, pxcrt::bt_size_t& len_r$);
}; /* namespace pxcrt */
namespace io$n { namespace file$n { 
::io$n::errno$n::errno_or_value$v$p$meta$n$$size_t$t$r$ file_write$f(const pxcrt::file& f$, const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& buf$);
};}; /* namespace io::file */
namespace pxcrt { 
int file_lseek_impl(const pxcrt::file& f$, pxcrt::off_t& offset$, int whence$);
}; /* namespace pxcrt */
namespace io$n { namespace file$n { 
::io$n::errno$n::errno_or_value$v$p$io$$file$n$$off_t$s$r$ file_lseek$f(const pxcrt::file& f$, pxcrt::off_t offset$, int whence$);
};}; /* namespace io::file */
namespace pxcrt { 
int file_st_close(pxcrt::file_st& f$);
int io_stat(const pxcrt::io& iop$, const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& fn$, pxcrt::statbuf& buf$);
}; /* namespace pxcrt */
namespace io$n { namespace file$n { 
::io$n::errno$n::errno_or_value$v$p$meta$n$$size_t$t$r$ file_read_all$f(const pxcrt::file& f$, ::pxcrt::pxcvarray< pxcrt::bt_uchar >& buf$);
::io$n::errno$n::errno_or_value$v$p$meta$n$$size_t$t$r$ file_write_all$f(const pxcrt::file& f$, const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& buf$);
::io$n::errno$n::errno_or_value$v$p$pxcrt$$ptr$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$r$$r$ io_read_file$f(const pxcrt::io& iop$, const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& fn$);
::io$n::errno$n::errno_or_value$v$p$meta$n$$size_t$t$r$ io_write_file$f(const pxcrt::io& iop$, const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& fn$, const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& buf$);
::io$n::errno$n::errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$ io_pipe$f(const pxcrt::io& sys$);
};}; /* namespace io::file */
namespace pxcrt { 
::pxcrt::pxcvarray< pxcrt::file_mt > io_pipe_internal(const pxcrt::io& sys$, int& err_r$);
}; /* namespace pxcrt */
extern "C" void _numeric$$integral$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _container$$array$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _io$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _io$$file$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _io$$errno$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _text$$string$$serialize$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _text$$string$$split$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _algebraic$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _meta$$vararg$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _meta$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _meta$$family$$nsmain$c();
namespace pxcrt { 
pxcrt::file_st io_stdin(const pxcrt::io& iop$);
pxcrt::file_st io_stdout(const pxcrt::io& iop$);
pxcrt::file_st io_stderr(const pxcrt::io& iop$);
void debug_log_internal(const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& s$);
}; /* namespace pxcrt */
extern "C" void _numeric$$integral$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _io$$errno$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _io$$file$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _io$$nsmain$c();
namespace pxcrt { 
pxcrt::bt_bool set_signal_handler(const pxcrt::io& iop$, int sig$, pxcrt::sighandler_t handler$);
pxcrt::bt_bool get_signaled(const pxcrt::io& iop$);
}; /* namespace pxcrt */
extern "C" void _numeric$$integral$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _numeric$$cast$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _container$$array$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _algebraic$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _meta$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _io$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _io$$standard$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _io$$errno$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _io$$signal$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _io$$file$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _exception$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
namespace io$n { namespace process$n { 
void io__exit$f(const pxcrt::io& sys$, pxcrt::bt_int st$);
void exit$f(pxcrt::bt_int st$);
void io_abort$f(const pxcrt::io& sys$);
::io$n::errno$n::errno_or_value$v$p$io$$process$n$$pid_t$s$r$ io_fork$f(const pxcrt::io& sys$);
::io$n::errno$n::errno_or_value$v$p$io$$process$n$$pipe_process$s$r$ io_popen$f(const pxcrt::io& sys$, const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& command$, const ::pxcrt::bt_cslice< ::pxcrt::pxcvarray< pxcrt::bt_uchar > >& argv$, pxcrt::bt_bool for_write$, pxcrt::bt_bool search_path$);
::io$n::errno$n::errno_or_value$v$p$io$$process$n$$wait_t$s$r$ io_wait$f(const pxcrt::io& sys$, int& st_r$);
::io$n::errno$n::errno_or_value$v$p$io$$process$n$$wait_t$s$r$ io_waitpid$f(const pxcrt::io& sys$, pxcrt::pid_t p$, int opt$);
::io$n::errno$n::errno_or_value$v$p$io$$process$n$$status_t$s$r$ io_shell_exec$f(const pxcrt::io& sys$, const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& cmd$);
};}; /* namespace io::process */
namespace pxcrt { 
pxcrt::bt_bool wifexited(int st$);
pxcrt::bt_int wexitstatus(int st$);
pxcrt::bt_bool wifsignaled(int st$);
int wtermsig(int st$);
int execv(const pxcrt::io& sys$, const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& path$, const ::pxcrt::bt_cslice< ::pxcrt::pxcvarray< pxcrt::bt_uchar > >& argv$);
int execvp(const pxcrt::io& sys$, const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& path$, const ::pxcrt::bt_cslice< ::pxcrt::pxcvarray< pxcrt::bt_uchar > >& argv$);
void _exit_internal(const pxcrt::io& sys$, pxcrt::bt_int status$);
void exit_internal(const pxcrt::io& sys$, pxcrt::bt_int status$);
void abort_internal(const pxcrt::io& sys$);
pxcrt::pid_t fork_internal(const pxcrt::io& sys$);
pxcrt::pid_t wait_internal(const pxcrt::io& sys$, int& st_r$);
pxcrt::pid_t waitpid_internal(const pxcrt::io& sys$, pxcrt::pid_t pid$, int& st_r$, int opt$);
pxcrt::bt_int shell_exec_internal(const pxcrt::io& sys$, const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& cmd$);
int dup2_internal(const pxcrt::io& sys$, const pxcrt::file& ofp$, const pxcrt::file& nfp$);
void io_process_init();
}; /* namespace pxcrt */
extern "C" void _operator$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _pointer$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _meta$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _numeric$$integral$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _operator$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _pointer$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _io$$errno$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _algebraic$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _meta$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _exception$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _callable$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _text$$string$$serialize$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _numeric$$integral$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _container$$array$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _pointer$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _operator$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _thread$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _callable$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _algebraic$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
extern "C" void _meta$$nsmain$c();
namespace pxcrt { 
}; /* namespace pxcrt */
namespace thread$n { namespace queue$n { 
::algebraic$n::option$v$p$callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$r$ queue_wait_pop$f(const pxcrt::rcptr< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > >& tq$);
void queue_thread_main$f(const pxcrt::rcptr< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > >& tq$);
void queue_thread_stop$f(const pxcrt::rcptr< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > >& tq$, pxcrt::bt_int mode$);
void queue_push_notify$f(const pxcrt::rcptr< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > >& tq$, const ::callable$n::tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$& ep$);
};}; /* namespace thread::queue */
extern "C" void _numeric$$integral$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
extern "C" void _meta$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
extern "C" void _meta$$initial$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
extern "C" void _numeric$$integral$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
extern "C" void _numeric$$integral$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
extern "C" void _numeric$$fp$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
extern "C" void _algebraic$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
extern "C" void _container$$array$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
extern "C" void _container$$tree_map$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
extern "C" void _container$$hash_map$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
extern "C" void _pointer$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
extern "C" void _numeric$$distinct$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
extern "C" void _numeric$$cast$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
extern "C" void _text$$string$$serialize$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
extern "C" void _text$$string$$split$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
extern "C" void _io$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
extern "C" void _io$$process$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
extern "C" void _io$$standard$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
extern "C" void _io$$file$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
extern "C" void _generic$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
extern "C" void _ordered$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
extern "C" void _equality$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
extern "C" void _callable$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
extern "C" void _thread$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
extern "C" void _thread$$queue$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
extern "C" void _aligned$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
extern "C" void _debug_object$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
extern "C" void _operator$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
extern "C" void _common$$nsmain$c();
namespace thread$n { namespace queue$n { 
};}; /* namespace thread::queue */
namespace text$n { namespace string$n { namespace serialize$n { 
static inline void string_append_char$f$p$42$li$r$(::pxcrt::pxcvarray< pxcrt::bt_uchar >& v$);
static inline ::pxcrt::pxcvarray< pxcrt::bt_uchar > string_parse_token$f$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$q$text$$string$$serialize$n$$str_decode$f83$r$(::pxcrt::bt_cslice< pxcrt::bt_uchar >& s$);
static inline pxcrt::bt_bool string_parse_token$f$p$meta$n$$bool$t$q$text$$string$$serialize$n$$parse_bool$f87$r$(::pxcrt::bt_cslice< pxcrt::bt_uchar >& s$);
static inline ::pxcrt::pxcvarray< pxcrt::bt_uchar > to_string$f$p$io$$errno$n$$errno_t$s$r$(const int& x$);
static inline void serialize_to_string$f$p$io$$errno$n$$errno_t$s$r$(const int& x$, ::pxcrt::pxcvarray< pxcrt::bt_uchar >& out$);
};};}; /* namespace text::string::serialize */
namespace text$n { namespace serialize$n { 
static inline void serialize$f$p$text$$string$$serialize$n$$ser_default$s$q$io$$errno$n$$errno_t$s$r$(const int& x$, ::pxcrt::pxcvarray< pxcrt::bt_uchar >& o$);
};}; /* namespace text::serialize */
namespace numeric$n { namespace cast$n { 
static inline pxcrt::bt_double static_cast$f$p$meta$n$$double$t$q$io$$time$n$$time_t$s$r$(const std::time_t& x$);
static inline pxcrt::bt_double static_cast$f$p$meta$n$$double$t$q$io$$time$n$$suseconds_t$s$r$(const pxcrt::suseconds_t& x$);
};}; /* namespace numeric::cast */
namespace operator$n { 
static inline pxcrt::bt_size_t union_field$f$p$value$ls$q$io$$errno$n$$errno_or_value$v$p$meta$n$$size_t$t$r$$r$(const ::io$n::errno$n::errno_or_value$v$p$meta$n$$size_t$t$r$& x$);
}; /* namespace operator */
namespace io$n { namespace file$n { 
static inline ::io$n::errno$n::errno_or_value$v$p$io$$file$n$$file_st$s$r$ io_open$f$p$m$ll$p$m$ll$p$io$n$$io$s$q$0$li$r$$q$m$ll$p$container$$array$n$$cslice$s$p$meta$n$$uchar$t$r$$q$0$li$r$$q$m$ll$p$io$$file$n$$open_flags_t$s$q$0$li$r$$r$$r$(const pxcrt::io& a0$, const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& a1$, const int& a2$);
};}; /* namespace io::file */
namespace pointer$n { 
static inline pxcrt::rcptr< pxcrt::rcval< ::pxcrt::pxcvarray< pxcrt::bt_uchar > > > make_ptr$f$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$q$m$ll$r$();
static inline pxcrt::rcptr< pxcrt::rcval< ::pxcrt::pxcvarray< pxcrt::bt_uchar > > > box_pointer$f$p$pxcrt$$ptr$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$r$$q$m$ll$r$();
}; /* namespace pointer */
namespace operator$n { 
static inline pxcrt::file_st union_field$f$p$value$ls$q$io$$errno$n$$errno_or_value$v$p$io$$file$n$$file_st$s$r$$r$(const ::io$n::errno$n::errno_or_value$v$p$io$$file$n$$file_st$s$r$& x$);
}; /* namespace operator */
namespace io$n { namespace file$n { 
static inline ::io$n::errno$n::errno_or_value$v$p$io$$file$n$$file_st$s$r$ io_open$f$p$m$ll$p$m$ll$p$io$n$$io$s$q$0$li$r$$q$m$ll$p$container$$array$n$$cslice$s$p$meta$n$$uchar$t$r$$q$0$li$r$$q$m$ll$p$io$$file$n$$open_flags_t$s$q$0$li$r$$q$m$ll$p$io$$file$n$$mode_t$s$q$0$li$r$$r$$r$(const pxcrt::io& a0$, const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& a1$, const int& a2$, const pxcrt::mode_t & a3$);
};}; /* namespace io::file */
namespace operator$n { 
static inline pxcrt::pid_t union_field$f$p$value$ls$q$io$$errno$n$$errno_or_value$v$p$io$$process$n$$pid_t$s$r$$r$(const ::io$n::errno$n::errno_or_value$v$p$io$$process$n$$pid_t$s$r$& x$);
static inline ::algebraic$n::pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$ union_field$f$p$value$ls$q$io$$errno$n$$errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$$r$(const ::io$n::errno$n::errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$& x$);
}; /* namespace operator */
namespace numeric$n { namespace cast$n { 
static inline int static_cast$f$p$io$$process$n$$status_t$s$q$meta$n$$int$t$r$(const pxcrt::bt_int& x$);
};}; /* namespace numeric::cast */
namespace thread$n { 
static inline ::callable$n::callable_ptr$s$p$meta$n$$void$t$q$m$ll$r$ make_thread_ptr$f$p$thread$$queue$n$$queue_thread_main$f$r$(const pxcrt::rcptr< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > >& tq$);
}; /* namespace thread */
namespace pointer$n { 
static inline pxcrt::rcptr< ::thread$n::thread$s$p$thread$$queue$n$$queue_thread_main$f$r$ > make_ptr$f$p$thread$n$$thread$s$p$thread$$queue$n$$queue_thread_main$f$r$$q$m$ll$p$m$ll$p$pxcrt$$tptr$p$thread$$queue$n$$task_queue_shared$s$r$$q$0$li$r$$r$$r$(const pxcrt::rcptr< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > >& a0$);
static inline pxcrt::rcptr< ::thread$n::thread$s$p$thread$$queue$n$$queue_thread_main$f$r$ > box_pointer$f$p$pxcrt$$ptr$p$thread$n$$thread$s$p$thread$$queue$n$$queue_thread_main$f$r$$r$$q$m$ll$p$m$ll$p$pxcrt$$tptr$p$thread$$queue$n$$task_queue_shared$s$r$$q$0$li$r$$r$$r$(const pxcrt::rcptr< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > >& a0$);
static inline pxcrt::rcptr< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > > make_tptr$f$p$thread$$queue$n$$task_queue_shared$s$q$m$ll$r$();
static inline pxcrt::rcptr< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > > box_pointer$f$p$pxcrt$$tptr$p$thread$$queue$n$$task_queue_shared$s$r$$q$m$ll$r$();
}; /* namespace pointer */
namespace operator$n { 
static inline ::callable$n::tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$ union_field$f$p$some$ls$q$algebraic$n$$option$v$p$callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$r$$r$(const ::algebraic$n::option$v$p$callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$r$& x$);
}; /* namespace operator */
namespace text$n { namespace string$n { namespace serialize$n { 
static inline ::pxcrt::pxcvarray< pxcrt::bt_uchar > to_string$f$p$meta$n$$unit$t$r$(const pxcrt::bt_unit& x$);
static inline void serialize_to_string$f$p$meta$n$$unit$t$r$(const pxcrt::bt_unit& x$, ::pxcrt::pxcvarray< pxcrt::bt_uchar >& out$);
};};}; /* namespace text::string::serialize */
namespace text$n { namespace serialize$n { 
static inline void serialize$f$p$text$$string$$serialize$n$$ser_default$s$q$meta$n$$unit$t$r$(const pxcrt::bt_unit& x$, ::pxcrt::pxcvarray< pxcrt::bt_uchar >& o$);
};}; /* namespace text::serialize */
namespace io$n { namespace standard$n { 
static inline void println$f$p$m$ll$p$m$ll$p$container$$array$n$$strlit$s$q$0$li$r$$r$$r$(const ::pxcrt::bt_strlit& a0$);
};}; /* namespace io::standard */
namespace text$n { namespace string$n { namespace split$n { 
static inline ::pxcrt::pxcvarray< pxcrt::bt_uchar > string_join$f$p$_20$ls$q$m$ll$p$m$ll$p$container$$array$n$$strlit$s$q$0$li$r$$r$$r$(const ::pxcrt::bt_strlit& a0$);
};};}; /* namespace text::string::split */
#ifndef PXC_IMPORT_HEADER
/* inline c */
/* inline */

namespace pxcrt {

bt_unit unit_value;
char **process_argv;

}; // namespace pxcrt
// extern "implementation" inline
;
/* inline */
namespace pxcrt {

#ifndef PXCRT_NO_LOCAL_POOL

thread_local void *local_pool_blocks;

void __attribute__((destructor(101))) clear_local_pool()
{
  while (local_pool_blocks != 0) {
    void *next = *(void **)local_pool_blocks;
    global_deallocate_n(local_pool_blocks, local_pool_block_alloc_size);
    local_pool_blocks = next;
  }
}

#else

void clear_local_pool()
{
}

#endif

}; // namespace pxcrt
;
/* inline */
namespace pxcrt {

svarray_rep empty_rep; /* pod */

}; // namespace pxcrt
;
/* inline */
namespace text {
using namespace pxcrt;

static void fp_to_decimal_internal(const char *fmt, bt_double x, bt_string& s)
{
// FIXME: cease to use snprintf
  const size_t osz = s.size();
  char *buf = reinterpret_cast<char *>(s.reserve_back<1>(20));
  int len = snprintf(buf, 20, fmt, x);
  if (len < 0) {
    return;
  }
  if (len < 10) {
    s.rawarr_set_valid_len(osz + len);
    return;
  }
  const int alen = len;
  buf = reinterpret_cast<char *>(s.reserve_back<1>(alen + 1));
  len = snprintf(buf, alen + 1, fmt, x);
  if (len < 0) {
    return;
  }
  s.rawarr_set_valid_len(osz + len);
}

static cstrref get_decimal_part(cstrref const& buf)
{
  const size_t n = buf.size();
  const bt_uchar *const p = buf.rawarr();
  size_t i = 0;
  for (; i < n; ++i) {
    const bt_uchar ch = p[i];
    if ((ch >= '0' && ch <= '9') ||
      /*
      (ch >= 'a' && ch <= 'z') ||
      (ch >= 'A' && ch <= 'Z') ||
      */
      ch == '+' || ch == '-' || ch == '.') {
    } else {
      break;
    }
  }
  return cstrref(p, i);
}

void fp_to_decimal_double(bt_double x, bt_string& buf)
{ fp_to_decimal_internal("%.16f", x, buf); }

void fp_to_decimal_float(bt_float x, bt_string& buf)
{ fp_to_decimal_internal("%.7f", x, buf); }

bt_double decimal_to_fp_nocheck_double(cstrref const& buf)
{
  cstrref pbuf = get_decimal_part(buf);
  PXCRT_ALLOCA_NTSTRING(buf_nt, pbuf);
  double r = 0;
#ifdef _MSC_VER
  sscanf_s(buf_nt.get(), "%lf", &r);
#else
  sscanf(buf_nt.get(), "%lf", &r);
#endif
  return r;
}

bt_float decimal_to_fp_nocheck_float(cstrref const& buf)
{
  cstrref pbuf = get_decimal_part(buf);
  PXCRT_ALLOCA_NTSTRING(buf_nt, pbuf);
  float r = 0;
#ifdef _MSC_VER
  sscanf_s(buf_nt.get(), "%f", &r);
#else
  sscanf(buf_nt.get(), "%f", &r);
#endif
  return r;
}

bt_double decimal_to_fp_double(cstrref& buf)
{
  cstrref pbuf = get_decimal_part(buf);
  PXCRT_ALLOCA_NTSTRING(buf_nt, pbuf);
  double r = 0;
  int n = 0;
#ifdef _MSC_VER
  sscanf_s(buf_nt.get(), "%lf%n", &r, &n);
#else
  sscanf(buf_nt.get(), "%lf%n", &r, &n);
#endif
  if (n > 0) {
    buf.increment_front(n);
  }
  return r;
}

bt_float decimal_to_fp_float(cstrref& buf)
{
  cstrref pbuf = get_decimal_part(buf);
  PXCRT_ALLOCA_NTSTRING(buf_nt, pbuf);
  float r = 0;
  int n = 0;
#ifdef _MSC_VER
  sscanf_s(buf_nt.get(), "%f%n", &r, &n);
#else
  sscanf(buf_nt.get(), "%f%n", &r, &n);
#endif
  if (n > 0) {
    buf.increment_front(n);
  }
  return r;
}

};
;
/* inline */
namespace pxcrt {
using namespace pxcrt;

void minimal_encode(cstrref const& src, bt_string& buf)
{
  /* escapes 0x00 - 0x0f, 0x2c, 0x7a - 0x7f */
  const size_t srclen = src.size();
  const bt_uchar *const start = src.rawarr();
  const bt_uchar *const end = start + srclen;
  const size_t osize = buf.size();
  bt_uchar *const wstart = buf.reserve_back<2>(srclen); /* srclen * 2 */
  bt_uchar *wptr = wstart;
  for (const bt_uchar *p = start; p != end; ++p) {
    const bt_uchar c = p[0];
    if ((c >= 0x10 && c <= 0x7a && c != 0x2c) || c >= 0x80) {
      wptr[0] = c;
      ++wptr;
    } else if (c < 0x30) {
      wptr[0] = '~';
      wptr[1] = c + 0x20;
      wptr += 2;
    } else {
      wptr[0] = '~';
      wptr[1] = c - 0x20;
      wptr += 2;
    }
  }
  assert((size_t)(wptr - wstart) <= srclen * 2);
  buf.rawarr_set_valid_len(osize + wptr - wstart);
}

void minimal_decode(cstrref const& src, bt_string& buf)
{
  const size_t srclen = src.size();
  const bt_uchar *const start = src.rawarr();
  const bt_uchar *const end = start + srclen;
  const size_t osize = buf.size();
  bt_uchar *const wstart = buf.reserve_back<1>(srclen);
  bt_uchar *wptr = wstart;
  for (const bt_uchar *p = start; p != end; ++p) {
    if (p[0] == '~' && p + 1 != end) {
      bt_uchar ch = p[1];
      (*wptr++) = ch < 0x50 ? ch - 0x20 : ch + 0x20;
      p++;
    } else {
      (*wptr++) = p[0];
    }
  }
  assert((size_t)(wptr - wstart) <= srclen);
  buf.rawarr_set_valid_len(osize + wptr - wstart);
}

}; // namespace pxcrt
;
/* inline */
#include <string>
// #ifdef PXC_POSIX
// #include <unistd.h>
// #endif
#ifdef __GLIBC__
#include <execinfo.h>
#endif
namespace pxcrt {

logic_error::logic_error() : std::logic_error("pxcrt::logic_error")
{ }
bt_string logic_error::message() const
{ return cstr_to_string("logic_error"); }

runtime_error::runtime_error() : std::runtime_error("pxcrt::runtime_error")
{ }
bt_string runtime_error::message() const
{ return cstr_to_string("runtime_error"); }

bad_alloc::bad_alloc()
{ }
bt_string bad_alloc::message() const
{ return cstr_to_string("bad_alloc"); }

bt_string invalid_index::message() const
{ return cstr_to_string("invalid_index"); }
bt_string invalid_field::message() const
{ return cstr_to_string("invalid_field"); }
bt_string would_invalidate::message() const
{ return cstr_to_string("would_invalidate"); }

void throw_bad_alloc()
{ PXC_THROW(bad_alloc()); }
void throw_invalid_index()
{ PXC_THROW(invalid_index()); }
void throw_invalid_field()
{ PXC_THROW(invalid_field()); }
void throw_would_invalidate()
{ PXC_THROW(would_invalidate()); }

size_t stack_trace_limit = static_cast<size_t>(-1);

void set_stack_trace_limit(size_t sz)
{
  stack_trace_limit = sz;
}

exception::exception()
{
#ifdef __GLIBC__
  if (stack_trace_limit != 0) {
    size_t n = std::min(static_cast<size_t>(5), stack_trace_limit);
    while (true) {
      trace.resize(n);
      backtrace(&trace[0], trace.size());
      if (trace[trace.size() - 1] == 0) {
	while (trace[trace.size() - 1] == 0) {
	  trace.resize(trace.size() - 1);
	}
	break;
      }
      size_t n2 = std::min(n * 2, stack_trace_limit);
      if (n2 <= n) {
	break;
      }
      n = n2;
    }
  }
#endif
}

struct auto_free {
  auto_free(void *ptr) : ptr(ptr) { }
  ~auto_free() { free(ptr); }
private:
  auto_free(const auto_free&);
  auto_free& operator =(const auto_free&);
  void *ptr;
};

bt_string exception_message(pxcrt::exception const& exc)
{
  return exc.message();
}

bt_string exception_stack_trace(pxcrt::exception const& exc)
{
  bt_string r;
#ifdef __GLIBC__
  std::vector<void *> const& tr = exc.trace; 
  char **syms = backtrace_symbols(&tr[0], tr.size());
  if (syms == 0) {
    throw_bad_alloc();
  }
  auto_free af(syms);
  for (size_t i = 0; i < tr.size(); ++i) {
    r.append(cstrref(
      reinterpret_cast<const bt_uchar *>(syms[i]), strlen(syms[i])));
    r.push_back('\n');
  }
#endif
  return r;
}

bt_string c_exception_message(std::exception const& exc)
{
  const exception *const e = dynamic_cast<const exception *>(&exc);
  if (e == 0) {
    const char *const s = exc.what();
    const bt_uchar *const us = reinterpret_cast<const bt_uchar *>(s);
    return bt_string(us, strlen(s));
  } else {
    return e->message();
  }
}

bt_string c_exception_stack_trace(std::exception const& exc)
{
  const exception *const e = dynamic_cast<const exception *>(&exc);
  if (e == 0) {
    return bt_string();
  }
  return exception_stack_trace(*e);
}

std::string c_exception_to_stdstring(const std::exception& ex)
{
  std::string mess;
  bt_string s0 = c_exception_message(ex);
  mess.insert(mess.end(), s0.begin(), s0.end());
  if (!mess.empty() && mess[mess.size() - 1] != '\n') {
    mess += "\n";
  }
  bt_string st = c_exception_stack_trace(ex);
  mess.insert(mess.end(), st.begin(), st.end());
  return mess;
}

void c_exception_log_stderr(const std::exception& ex)
{
  const std::string mess = c_exception_to_stdstring(ex);
#ifdef PXC_POSIX
  ::write(2, mess.data(), mess.size());
#else
  _write(2, mess.data(), mess.size());
#endif
}

int main_nothrow(void (*main_f)(void))
{
  PXC_TRY {
    PXC_TRY {
      (*main_f)();
    } PXC_CATCH(const std::exception& ex) {
      #if !PXC_NOEXCEPTIONS
      c_exception_log_stderr(ex);
      return 1;
      #endif
    }
  } PXC_CATCH(...) {
    const char *const mess = "PXCMAIN: Uncaught exception\n";
#ifdef PXC_POSIX
    ::write(2, mess, strlen(mess));
#else
    _write(2, mess, strlen(mess));
#endif
    return 1;
  }
  return 0;
}

}; // namespace pxcrt
;
/* inline */

namespace pxcrt {

const denom_info denom_list[38] = {{17,&modulo_fixed<17>},{29,&modulo_fixed<29>},{37,&modulo_fixed<37>},{53,&modulo_fixed<53>},{67,&modulo_fixed<67>},{79,&modulo_fixed<79>},{97,&modulo_fixed<97>},{131,&modulo_fixed<131>},{193,&modulo_fixed<193>},{257,&modulo_fixed<257>},{389,&modulo_fixed<389>},{521,&modulo_fixed<521>},{769,&modulo_fixed<769>},{1031,&modulo_fixed<1031>},{1543,&modulo_fixed<1543>},{2053,&modulo_fixed<2053>},{3079,&modulo_fixed<3079>},{6151,&modulo_fixed<6151>},{12289,&modulo_fixed<12289>},{24593,&modulo_fixed<24593>},{49157,&modulo_fixed<49157>},{98317,&modulo_fixed<98317>},{196613,&modulo_fixed<196613>},{393241,&modulo_fixed<393241>},{786433,&modulo_fixed<786433>},{1572869,&modulo_fixed<1572869>},{3145739,&modulo_fixed<3145739>},{6291469,&modulo_fixed<6291469>},{12582917,&modulo_fixed<12582917>},{25165843,&modulo_fixed<25165843>},{50331653,&modulo_fixed<50331653>},{100663319,&modulo_fixed<100663319>},{201326611,&modulo_fixed<201326611>},{402653189,&modulo_fixed<402653189>},{805306457,&modulo_fixed<805306457>},{1610612741,&modulo_fixed<1610612741>},{3221225473,&modulo_fixed<3221225473>},{4294967291,&modulo_fixed<4294967291>}};

}; // namespace pxcrt

;
/* inline */
namespace pxcrt {

io io_system(0);

io debug_system()
{
  return io(0);
}

};
;
/* inline */
#include <errno.h>
;
/* inline */
namespace pxcrt {
using namespace pxcrt;

#ifdef PXC_POSIX

::timeval gettimeofday(io const& iop)
{
  ::timeval tv = { };
  ::gettimeofday(&tv, 0); /* no need to check error */
  return tv;
}

bt_ulonglong high_resolution_timer(io const& iop)
{
  ::timeval tv = { };
  ::gettimeofday(&tv, 0); /* no need to check error */
  unsigned long long v = tv.tv_sec;
  v *= 1000000;
  v += tv.tv_usec;
  return v;
}

bt_ulonglong high_resolution_timer_frequency(io const& iop)
{
  return 1000000ULL;
}

#endif

#ifdef PXC_WINDOWS

::timeval gettimeofday(io const& iop)
{
  FILETIME ft = { };
  unsigned long long v = 0;
  GetSystemTimeAsFileTime(&ft);
  v = ft.dwHighDateTime;
  v <<= 32;
  v += ft.dwLowDateTime;
  v -= 116444736000000000ULL;
  v /= 10;
  ::timeval tv = { };
  tv.tv_sec = (long)(v / 1000000);
  tv.tv_usec = (long)(v % 1000000);
  return tv;
}

bt_ulonglong high_resolution_timer(io const& iop)
{
  LARGE_INTEGER v = { };
  if (!QueryPerformanceCounter(&v)) {
    return 0;
  }
  return v.QuadPart;
}

bt_ulonglong high_resolution_timer_frequency(io const& iop)
{
  LARGE_INTEGER v = { };
  if (!QueryPerformanceFrequency(&v)) {
    return 0;
  }
  return v.QuadPart;
}

#endif

std::time_t time(io const& iop)
{
  return std::time(0);
}

bt_uint io_sleep(io const& iop, bt_uint sec)
{
#ifdef PXC_POSIX
  ::sleep(sec);
#else
  Sleep(sec * 1000);
#endif
  return 0;
}

bt_uint io_usleep(io const& iop, bt_uint usec)
{
#ifdef PXC_POSIX
  ::usleep(usec);
#else
  Sleep(usec / 1000);
#endif
  return 0;
}

void time_init()
{
  /* not multi-thread safe */
#ifdef PXC_POSIX
  ::tzset();
#else
  _tzset();
#endif
}

}; // namespace pxcrt
;
/* inline */
namespace pxcrt {

typedef int errno_t;

bt_int io_open_fd(io const& iop, cstrref const& fn, int flags, mode_t md)
{
  PXCRT_ALLOCA_NTSTRING(fn_nt, fn);
#ifdef PXC_POSIX
  return ::open(fn_nt.get(), flags, md);
#else
  bt_int r = -1;
  errno = ::_sopen_s(&r, fn_nt.get(), flags, 0, md);
  return r;
#endif
}

file_st io_make_file_st(io const& iop, bt_int fd)
{
  return make_file_st(fd);
}

file_mt io_make_file_mt(io const& iop, bt_int fd)
{
  return make_file_mt(fd);
}

file_st make_file_st(bt_int fd)
{
  PXC_TRY {
    file_st r(fd); /* throws */
    return r;
  } PXC_CATCH(...) {
    if (fd >= 2) {
      file_st_rep f(fd); /* closes fd */
    }
    PXC_RETHROW;
  }
}

file_mt make_file_mt(bt_int fd)
{
  PXC_TRY {
    file_mt r(fd); /* throws */
    return r;
  } PXC_CATCH(...) {
    if (fd >= 2) {
      file_mt_rep f(fd); /* closes fd */
    }
    PXC_RETHROW;
  }
}

errno_t file_read_impl(file const& f, bt_string& buf, size_t& len)
{
  int const fd = f.get();
  bt_size_t sz = len;
  if (sz > SSIZE_MAX) {
    sz = SSIZE_MAX;
  }
  bt_size_t osz = buf.size();
  void *const ptr = buf.reserve_back<1>(sz);
#ifdef PXC_POSIX
  const pxcrt::bt_ssize_t rlen = ::read(fd, ptr, sz);
#else
  const pxcrt::bt_ssize_t rlen = ::_read(fd, ptr, sz);
#endif
  if (rlen < 0) {
    return errno;
  }
  buf.rawarr_set_valid_len(osz + rlen);
  len = rlen;
  return 0;
}

errno_t file_write_impl(file const& f, cstrref const& buf, size_t& len_r)
{
  int const fd = f.get();
  bt_size_t sz = buf.size();
  if (sz > SSIZE_MAX) {
    sz = SSIZE_MAX;
  }
  const unsigned char *const ptr = buf.rawarr();
#ifdef PXC_POSIX
  const pxcrt::bt_ssize_t wlen = ::write(fd, ptr, sz);
#else
  const pxcrt::bt_ssize_t wlen = ::_write(fd, ptr, sz);
#endif
  if (wlen < 0) {
    return errno;
  }
  len_r = wlen;
  return 0;
}

errno_t file_lseek_impl(file const& f, off_t& offset, int whence)
{
  int const fd = f.get();
#ifdef PXC_POSIX
  off_t r = ::lseek(fd, offset, whence);
#else
  off_t r = ::_lseeki64(fd, offset, whence);
#endif
  if (r == (off_t)-1) {
    offset = 0;
    return errno;
  }
  offset = r;
  return 0;
}

int /* errno_t */ file_st_close(file_st& f)
{
  int const fd = f.ptr->value.fd;
  f.ptr->value.fd = -1;
#ifdef PXC_POSIX
  int const rv = ::close(fd);
#else
  int const rv = ::_close(fd);
#endif
  return rv == 0 ? 0 : errno;
}

errno_t io_stat(io const& iop, cstrref const& fn, pxcrt::statbuf& buf)
{
  PXCRT_ALLOCA_NTSTRING(fn_nt, fn);
#ifdef PXC_POSIX
  const int e = ::stat(fn_nt.get(), &buf);
#else
  const int e = ::_stat(fn_nt.get(), &buf);
#endif
  if (e != 0) {
    return errno;
  }
  return 0;
}

::pxcrt::pxcvarray<file_mt> io_pipe_internal(::pxcrt::io const& sys,
  int& err_r)
{
  int fds[2];
#ifdef PXC_POSIX
  const int e = ::pipe(fds);
#else
  const int e = ::_pipe(fds, 4096, 0);
#endif
  if (e != 0) {
    err_r = errno;
    fds[0] = fds[1] = -1;
  }
  ::pxcrt::pxcvarray<file_mt> r;
  if (e == 0) {
    r.push_back(make_file_mt(fds[0]));
    r.push_back(make_file_mt(fds[1]));
  }
  return r;
}

};
;
/* inline */
namespace pxcrt {

typedef ::algebraic$n::option$v$p$io$$file$n$$file_st$s$r$ optfile;

/* TODO: slow */
file_st io_stdin(io const& iop)  {
  optfile pxc_stdin;
  pxc_stdin.some$$l(make_file_st(0));
  return pxc_stdin.some$$r();
}
file_st io_stdout(io const& iop) {
  optfile pxc_stdout;
  pxc_stdout.some$$l(make_file_st(1));
  return pxc_stdout.some$$r();
}
file_st io_stderr(io const& iop) {
  optfile pxc_stderr;
  pxc_stderr.some$$l(make_file_st(2));
  return pxc_stderr.some$$r();
}
void debug_log_internal(cstrref const& s)
{
#ifdef PXC_POSIX
  ::write(2, s.rawarr(), s.size());
#else
  _write(2, s.rawarr(), s.size());
#endif
}

};
;
/* inline */
namespace pxcrt {

static volatile int v_signalled[256];

void sighandler_set_signaled(int sig)
{
  if (sig >= 0 && sig < 256) {
    v_signalled[sig] = 1;
  }
}

bool get_signaled(io const& iop, int sig)
{
  if (sig >= 0 && sig < 256) {
    return (v_signalled[sig] != 0);
  }
  return false;
}

bool set_signal_handler(io const& iop, int sig, sighandler_t h)
{
  errno = 0;
  const sighandler_t r = std::signal(sig, h);
  return (r != SIG_ERR);
}

};
;
/* inline */
namespace pxcrt {

using namespace pxcrt;

void _exit_internal(io const& sys, bt_int st)
{
  _exit(st);
}

void exit_internal(io const& sys, bt_int st)
{
  exit(st);
}

void abort_internal(io const& sys)
{
  abort();
}

pxcrt::pid_t fork_internal(io const& sys)
{
#ifdef PXC_POSIX
  return fork();
#else
  abort();
#endif
}

pxcrt::pid_t wait_internal(io const& sys, int& st_r)
{
#ifdef PXC_POSIX
  return wait(&st_r);
#else
  abort();
#endif
}

pxcrt::pid_t waitpid_internal(io const& sys, pxcrt::pid_t pid, int& st_r,
  int opt)
{
#ifdef PXC_POSIX
  return waitpid(pid, &st_r, opt);
#else
  abort();
#endif
}

bool wifexited(int st)
{
#ifdef PXC_POSIX
  return WIFEXITED(st) != 0;
#else
  return true;
#endif
}

int wexitstatus(int st)
{
#ifdef PXC_POSIX
  return WIFEXITED(st) ? WEXITSTATUS(st) : 0;
#else
  return st;
#endif
}

bool wifsignaled(int st)
{
#ifdef PXC_POSIX
  return WIFSIGNALED(st) != 0;
#else
  return false;
#endif
}

int wtermsig(int st)
{
#ifdef PXC_POSIX
  return WIFSIGNALED(st) ? WTERMSIG(st) : 0;
#else
  return 0;
#endif
}

static int execv_internal(bool execvp_flag, cstrref const& path,
  bt_cslice<bt_string> const& argv)
{
#ifdef PXC_POSIX
  PXCRT_ALLOCA_NTSTRING(path_nt, path);
  pxcvarray<bt_string> argv_nt;
  argv_nt.resize(argv.size(), bt_string());
  char *argv_rp[argv.size() + 1];
  for (size_t i = 0; i < argv.size(); ++i) {
    argv_nt[i] = argv[i];
    argv_nt[i].push_back('\0');
    argv_rp[i] = reinterpret_cast<char *>(argv_nt[i].rawarr());
  }
  argv_rp[argv.size()] = 0;
  if (execvp_flag) {
    ::execvp(path_nt.get(), argv_rp);
  } else {
    ::execv(path_nt.get(), argv_rp);
  }
  return errno;
#else
  abort();
#endif
}

int execv(io const& sys, cstrref const& path,
  bt_cslice<bt_string> const& argv)
{
  return execv_internal(false, path, argv);
}

int execvp(io const& sys, cstrref const& file,
  bt_cslice<bt_string> const& argv)
{
  return execv_internal(true, file, argv);
}

int shell_exec_internal(io const& sys, cstrref const& cmd)
{
  PXCRT_ALLOCA_NTSTRING(cmd_nt, cmd);
  return std::system(cmd_nt.get());
}

int dup2_internal(io const& sys, file const& ofp, file const& nfp)
{
#ifdef PXC_POSIX
  if (dup2(ofp.get(), nfp.get()) < 0) {
#else
  if (_dup2(ofp.get(), nfp.get()) < 0) {
#endif
    return errno;
  }
  return 0;
}

};
;
namespace text$n { namespace string$n { namespace split$n { 
/* global variables */
/* function definitions */
};};}; /* namespace text::string::split */
namespace text$n { namespace string$n { namespace serialize$n { 
ser_default$s::ser_default$s(){
}
void unit_to_string$f(pxcrt::bt_unit v$, ::pxcrt::pxcvarray< pxcrt::bt_uchar >& buf$) {
 ::text$n::string$n::serialize$n::string_append_char$f$p$42$li$r$(buf$);
}
void bool_to_string$f(pxcrt::bt_bool v$, ::pxcrt::pxcvarray< pxcrt::bt_uchar >& buf$) {
 if (v$) {
  ::pxcrt::array_append< ::pxcrt::pxcvarray< pxcrt::bt_uchar >,::pxcrt::bt_strlit >(buf$ , pxcrt::bt_strlit("1"));
 } else {
  ::pxcrt::array_append< ::pxcrt::pxcvarray< pxcrt::bt_uchar >,::pxcrt::bt_strlit >(buf$ , pxcrt::bt_strlit("0"));
 }
}
void string_append_none$f(::pxcrt::pxcvarray< pxcrt::bt_uchar >& v$) {
}
pxcrt::bt_size_t token_length$f(const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& s$) {
 {
  const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& ag$fe = (s$);
  const size_t sz$fe = ag$fe.size();
  const pxcrt::bt_uchar *const ar$fe = ag$fe.rawarr();
  for (pxcrt::bt_size_t i$ = 0; i$ != sz$fe; ++i$) {
   pxcrt::bt_uchar ch$ = ar$fe[i$];
   {
    if (ch$ == 123U) {
     if (i$ == pxcrt::bt_size_t((0LL))) {
      return ::text$n::string$n::serialize$n::find_brace_close$f(::pxcrt::bt_cslice< pxcrt::bt_uchar >(s$, pxcrt::bt_size_t(1LL), s$.size())) + pxcrt::bt_size_t(1LL);
     } else {
      return i$;
     }
    }
    if (ch$ == 125U || ch$ == 44U) {
     return i$;
    }
   }
  }
 }
 return s$.size();
}
pxcrt::bt_size_t find_brace_close$f(const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& s$) {
 pxcrt::bt_size_t level$ = pxcrt::bt_size_t(1LL);
 {
  const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& ag$fe = (s$);
  const size_t sz$fe = ag$fe.size();
  const pxcrt::bt_uchar *const ar$fe = ag$fe.rawarr();
  for (pxcrt::bt_size_t i$ = 0; i$ != sz$fe; ++i$) {
   pxcrt::bt_uchar ch$ = ar$fe[i$];
   {
    if (ch$ == 125U) {
     if (--level$ == pxcrt::bt_size_t((0LL))) {
      return i$ + pxcrt::bt_size_t(1LL);
     }
    } else if (ch$ == 123U) {
     ++level$;
    }
   }
  }
 }
 return s$.size();
}
::pxcrt::pxcvarray< pxcrt::bt_uchar > str_decode$f83(const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& stok$) {
 ::pxcrt::pxcvarray< pxcrt::bt_uchar > r$ = ::pxcrt::pxcvarray< pxcrt::bt_uchar >();
 pxcrt::minimal_decode(stok$ , r$);
 return r$;
}
::pxcrt::pxcvarray< pxcrt::bt_uchar > string_parse_string_unescape$f(::pxcrt::bt_cslice< pxcrt::bt_uchar >& s$) {
 return ::text$n::string$n::serialize$n::string_parse_token$f$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$q$text$$string$$serialize$n$$str_decode$f83$r$(s$);
}
pxcrt::bt_bool parse_bool$f87(const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& stok$) {
 if (stok$.empty() || eq_memcmp(stok$,::pxcrt::bt_cslice< pxcrt::bt_uchar >(pxcrt::bt_strlit("0")))) {
  return false;
 } else {
  return true;
 }
}
pxcrt::bt_bool string_parse_bool$f(::pxcrt::bt_cslice< pxcrt::bt_uchar >& s$) {
 return ::text$n::string$n::serialize$n::string_parse_token$f$p$meta$n$$bool$t$q$text$$string$$serialize$n$$parse_bool$f87$r$(s$);
}
pxcrt::bt_unit string_parse_unit$f(::pxcrt::bt_cslice< pxcrt::bt_uchar >& s$) {
 ::text$n::string$n::serialize$n::string_parse_token_skip$f(s$);
 return pxcrt::bt_unit();
}
pxcrt::bt_size_t string_parse_symbol_peek$f(::pxcrt::bt_cslice< pxcrt::bt_uchar >& stok$) {
 const pxcrt::bt_size_t sz$ = stok$.size();
 const pxcrt::bt_size_t toklen$ = ::text$n::string$n::serialize$n::token_length$f(stok$);
 stok$.decrement_back(sz$ - toklen$);
 return toklen$;
}
void string_parse_symbol$f(::pxcrt::bt_cslice< pxcrt::bt_uchar >& stok$, ::pxcrt::bt_cslice< pxcrt::bt_uchar >& s$) {
 const pxcrt::bt_size_t sz$ = stok$.size();
 const pxcrt::bt_size_t toklen$ = ::text$n::string$n::serialize$n::token_length$f(stok$);
 stok$.decrement_back(sz$ - toklen$);
 s$.increment_front(toklen$);
}
void string_parse_token_skip$f(::pxcrt::bt_cslice< pxcrt::bt_uchar >& s$) {
 const pxcrt::bt_size_t toklen$ = ::text$n::string$n::serialize$n::token_length$f(s$);
 s$.increment_front(toklen$);
}
};};}; /* namespace text::string::serialize */
namespace exception$n { 
void c_exception_append_to_string$f(const std::exception& ex$, ::pxcrt::pxcvarray< pxcrt::bt_uchar >& str$) {
 ::pxcrt::array_append< ::pxcrt::pxcvarray< pxcrt::bt_uchar >,::pxcrt::pxcvarray< pxcrt::bt_uchar > >(str$ , pxcrt::c_exception_message(ex$));
 if (str$.size() > pxcrt::bt_size_t((0LL)) && pxcrt::bt_uchar(pxcrt::get_elem_value(str$,str$.size() - pxcrt::bt_size_t(1LL))) != 10U) {
  ::pxcrt::array_push_back< ::pxcrt::pxcvarray< pxcrt::bt_uchar >,pxcrt::bt_uchar >(str$ , 10U);
 }
 ::pxcrt::array_append< ::pxcrt::pxcvarray< pxcrt::bt_uchar >,::pxcrt::pxcvarray< pxcrt::bt_uchar > >(str$ , pxcrt::c_exception_stack_trace(ex$));
}
void exception_append_to_string$f(const pxcrt::exception& ex$, ::pxcrt::pxcvarray< pxcrt::bt_uchar >& str$) {
 ::pxcrt::array_append< ::pxcrt::pxcvarray< pxcrt::bt_uchar >,::pxcrt::pxcvarray< pxcrt::bt_uchar > >(str$ , pxcrt::exception_message(ex$));
 if (str$.size() > pxcrt::bt_size_t((0LL)) && pxcrt::bt_uchar(pxcrt::get_elem_value(str$,str$.size() - pxcrt::bt_size_t(1LL))) != 10U) {
  ::pxcrt::array_push_back< ::pxcrt::pxcvarray< pxcrt::bt_uchar >,pxcrt::bt_uchar >(str$ , 10U);
 }
 ::pxcrt::array_append< ::pxcrt::pxcvarray< pxcrt::bt_uchar >,::pxcrt::pxcvarray< pxcrt::bt_uchar > >(str$ , pxcrt::exception_stack_trace(ex$));
}
}; /* namespace exception */
namespace equality$n { 
void hash_combine$f(pxcrt::bt_size_t& x$, pxcrt::bt_size_t v$) {
 x$ ^= v$ + pxcrt::bt_size_t(2654435769LL) + (x$ << 6LL) + (x$ >> 2LL);
}
}; /* namespace equality */
namespace io$n { namespace errno$n { 
int io_get_errno$f(const pxcrt::io& sys$) {
 return errno;
 return 0;
}
void io_set_errno$f(const pxcrt::io& sys$, int e$) {
 errno = e$;
}
void errno_t_append_to_string$f(int e$, ::pxcrt::pxcvarray< pxcrt::bt_uchar >& s$) {
 const char *s = 0;
if (e$ == E2BIG) { s = "E2BIG"; }
else if (e$ == EACCES) { s = "EACCES"; }
else if (e$ == EADDRINUSE) { s = "EADDRINUSE"; }
else if (e$ == EADDRNOTAVAIL) { s = "EADDRNOTAVAIL"; }
else if (e$ == EAFNOSUPPORT) { s = "EAFNOSUPPORT"; }
else if (e$ == EAGAIN) { s = "EAGAIN"; }
else if (e$ == EALREADY) { s = "EALREADY"; }
else if (e$ == EBADF) { s = "EBADF"; }
else if (e$ == EBADMSG) { s = "EBADMSG"; }
else if (e$ == EBUSY) { s = "EBUSY"; }
else if (e$ == ECANCELED) { s = "ECANCELED"; }
else if (e$ == ECHILD) { s = "ECHILD"; }
else if (e$ == ECONNABORTED) { s = "ECONNABORTED"; }
else if (e$ == ECONNREFUSED) { s = "ECONNREFUSED"; }
else if (e$ == ECONNRESET) { s = "ECONNRESET"; }
else if (e$ == EDEADLK) { s = "EDEADLK"; }
else if (e$ == EDESTADDRREQ) { s = "EDESTADDRREQ"; }
else if (e$ == EDOM) { s = "EDOM"; }
else if (e$ == EDQUOT) { s = "EDQUOT"; }
else if (e$ == EEXIST) { s = "EEXIST"; }
else if (e$ == EFAULT) { s = "EFAULT"; }
else if (e$ == EFBIG) { s = "EFBIG"; }
else if (e$ == EHOSTUNREACH) { s = "EHOSTUNREACH"; }
else if (e$ == EIDRM) { s = "EIDRM"; }
else if (e$ == EILSEQ) { s = "EILSEQ"; }
else if (e$ == EINPROGRESS) { s = "EINPROGRESS"; }
else if (e$ == EINTR) { s = "EINTR"; }
else if (e$ == EINVAL) { s = "EINVAL"; }
else if (e$ == EIO) { s = "EIO"; }
else if (e$ == EISCONN) { s = "EISCONN"; }
else if (e$ == EISDIR) { s = "EISDIR"; }
else if (e$ == ELOOP) { s = "ELOOP"; }
else if (e$ == EMFILE) { s = "EMFILE"; }
else if (e$ == EMLINK) { s = "EMLINK"; }
else if (e$ == EMSGSIZE) { s = "EMSGSIZE"; }
else if (e$ == EMULTIHOP) { s = "EMULTIHOP"; }
else if (e$ == ENAMETOOLONG) { s = "ENAMETOOLONG"; }
else if (e$ == ENETDOWN) { s = "ENETDOWN"; }
else if (e$ == ENETRESET) { s = "ENETRESET"; }
else if (e$ == ENETUNREACH) { s = "ENETUNREACH"; }
else if (e$ == ENFILE) { s = "ENFILE"; }
else if (e$ == ENOBUFS) { s = "ENOBUFS"; }
else if (e$ == ENODATA) { s = "ENODATA"; }
else if (e$ == ENODEV) { s = "ENODEV"; }
else if (e$ == ENOENT) { s = "ENOENT"; }
else if (e$ == ENOEXEC) { s = "ENOEXEC"; }
else if (e$ == ENOLCK) { s = "ENOLCK"; }
else if (e$ == ENOLINK) { s = "ENOLINK"; }
else if (e$ == ENOMEM) { s = "ENOMEM"; }
else if (e$ == ENOMSG) { s = "ENOMSG"; }
else if (e$ == ENOPROTOOPT) { s = "ENOPROTOOPT"; }
else if (e$ == ENOSPC) { s = "ENOSPC"; }
else if (e$ == ENOSR) { s = "ENOSR"; }
else if (e$ == ENOSTR) { s = "ENOSTR"; }
else if (e$ == ENOSYS) { s = "ENOSYS"; }
else if (e$ == ENOTCONN) { s = "ENOTCONN"; }
else if (e$ == ENOTDIR) { s = "ENOTDIR"; }
else if (e$ == ENOTEMPTY) { s = "ENOTEMPTY"; }
else if (e$ == ENOTSOCK) { s = "ENOTSOCK"; }
else if (e$ == ENOTSUP) { s = "ENOTSUP"; }
else if (e$ == ENOTTY) { s = "ENOTTY"; }
else if (e$ == ENXIO) { s = "ENXIO"; }
else if (e$ == EOPNOTSUPP) { s = "EOPNOTSUPP"; }
else if (e$ == EOVERFLOW) { s = "EOVERFLOW"; }
else if (e$ == EPERM) { s = "EPERM"; }
else if (e$ == EPIPE) { s = "EPIPE"; }
else if (e$ == EPROTO) { s = "EPROTO"; }
else if (e$ == EPROTONOSUPPORT) { s = "EPROTONOSUPPORT"; }
else if (e$ == EPROTOTYPE) { s = "EPROTOTYPE"; }
else if (e$ == ERANGE) { s = "ERANGE"; }
else if (e$ == EROFS) { s = "EROFS"; }
else if (e$ == ESPIPE) { s = "ESPIPE"; }
else if (e$ == ESRCH) { s = "ESRCH"; }
else if (e$ == ESTALE) { s = "ESTALE"; }
else if (e$ == ETIME) { s = "ETIME"; }
else if (e$ == ETIMEDOUT) { s = "ETIMEDOUT"; }
else if (e$ == ETXTBSY) { s = "ETXTBSY"; }
else if (e$ == EWOULDBLOCK) { s = "EWOULDBLOCK"; }
else if (e$ == EXDEV) { s = "EXDEV"; }
if (s != 0) {
s$.append(reinterpret_cast<const unsigned char *>(s), std::strlen(s));
} else {
char buf[32];
int len = snprintf(buf, sizeof(buf), "errno(%d)",   static_cast<int>(e$));
s$.append(reinterpret_cast<unsigned char *>(buf), len);
};
}
void errno_t_check$f(int e$) {
 if (e$ != 0) {
  PXC_THROW(::exception$n::unexpected_value_template$s$p$io$$errno$n$$errno_t$s$r$((::pxcrt::pxcvarray< pxcrt::bt_uchar >::guard_ref< const ::pxcrt::pxcvarray< pxcrt::bt_uchar > > (::text$n::string$n::serialize$n::to_string$f$p$io$$errno$n$$errno_t$s$r$(e$)).get_crange())));
 }
}
};}; /* namespace io::errno */
namespace io$n { namespace time$n { 
pxcrt::bt_double io_gettimeofday_double$f(const pxcrt::io& iop$) {
 ::timeval tv$ = pxcrt::gettimeofday(iop$);
 const pxcrt::bt_double rv$ = ::numeric$n::cast$n::static_cast$f$p$meta$n$$double$t$q$io$$time$n$$time_t$s$r$(tv$.tv_sec) + ::numeric$n::cast$n::static_cast$f$p$meta$n$$double$t$q$io$$time$n$$suseconds_t$s$r$(tv$.tv_usec) / 1000000.0;
 return rv$;
}
};}; /* namespace io::time */
namespace io$n { namespace file$n { 
::io$n::errno$n::errno_or_value$v$p$io$$file$n$$file_st$s$r$ io_open_st$f(const pxcrt::io& iop$, const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& fn$, int flags$, pxcrt::mode_t  md$) {
 ::io$n::errno$n::errno_or_value$v$p$io$$file$n$$file_st$s$r$ r$;
 pxcrt::bt_int fd$ = pxcrt::io_open_fd(iop$ , fn$ , flags$ , md$);
 if (fd$ >= (0LL)) {
  (r$.value$$l(pxcrt::io_make_file_st(iop$ , fd$)));
 } else {
  (r$.errno$$l(::io$n::errno$n::io_get_errno$f(iop$)));
 }
 return r$;
}
::io$n::errno$n::errno_or_value$v$p$io$$file$n$$file_mt$s$r$ io_open_mt$f(const pxcrt::io& iop$, const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& fn$, int flags$, pxcrt::mode_t  md$) {
 ::io$n::errno$n::errno_or_value$v$p$io$$file$n$$file_mt$s$r$ r$;
 pxcrt::bt_int fd$ = pxcrt::io_open_fd(iop$ , fn$ , flags$ , md$);
 if (fd$ >= (0LL)) {
  (r$.value$$l(pxcrt::io_make_file_mt(iop$ , fd$)));
 } else {
  (r$.errno$$l(::io$n::errno$n::io_get_errno$f(iop$)));
 }
 return r$;
}
::io$n::errno$n::errno_or_value$v$p$meta$n$$size_t$t$r$ file_read$f(const pxcrt::file& f$, ::pxcrt::pxcvarray< pxcrt::bt_uchar >& buf$, pxcrt::bt_size_t len$) {
 ::io$n::errno$n::errno_or_value$v$p$meta$n$$size_t$t$r$ r$;
 const int e$ = pxcrt::file_read_impl(f$ , buf$ , len$);
 if (e$ != 0) {
  (r$.errno$$l(e$));
 } else {
  (r$.value$$l(len$));
 }
 return r$;
}
::io$n::errno$n::errno_or_value$v$p$meta$n$$size_t$t$r$ file_write$f(const pxcrt::file& f$, const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& buf$) {
 ::io$n::errno$n::errno_or_value$v$p$meta$n$$size_t$t$r$ r$;
 pxcrt::bt_size_t len$ = pxcrt::bt_size_t((0LL));
 const int e$ = pxcrt::file_write_impl(f$ , buf$ , len$);
 if (e$ != 0) {
  (r$.errno$$l(e$));
 } else {
  (r$.value$$l(len$));
 }
 return r$;
}
::io$n::errno$n::errno_or_value$v$p$io$$file$n$$off_t$s$r$ file_lseek$f(const pxcrt::file& f$, pxcrt::off_t offset$, int whence$) {
 ::io$n::errno$n::errno_or_value$v$p$io$$file$n$$off_t$s$r$ r$;
 const int e$ = pxcrt::file_lseek_impl(f$ , offset$ , whence$);
 if (e$ != 0) {
  (r$.errno$$l(e$));
 } else {
  (r$.value$$l(offset$));
 }
 return r$;
}
::io$n::errno$n::errno_or_value$v$p$meta$n$$size_t$t$r$ file_read_all$f(const pxcrt::file& f$, ::pxcrt::pxcvarray< pxcrt::bt_uchar >& buf$) {
 pxcrt::bt_size_t rlen$ = pxcrt::bt_size_t((0LL));
 while (true) {
  const pxcrt::bt_size_t len$ = pxcrt::bt_size_t(16384LL);
  const ::io$n::errno$n::errno_or_value$v$p$meta$n$$size_t$t$r$ r$ = ::io$n::file$n::file_read$f(f$ , buf$ , len$);
  {
   ::io$n::errno$n::errno_or_value$v$p$meta$n$$size_t$t$r$ const& ag$fe = (r$);
   if (ag$fe.get_$e() == ::io$n::errno$n::errno_or_value$v$p$meta$n$$size_t$t$r$::value$$e) {
    const pxcrt::bt_size_t rl$ = ag$fe.value$$r();
    {
     if (rl$ == pxcrt::bt_size_t((0LL))) {
      break;
     }
     rlen$ += rl$;
    }
   } else {
    return r$;
   }
  }
 }
 ::io$n::errno$n::errno_or_value$v$p$meta$n$$size_t$t$r$ r$;
 (r$.value$$l(rlen$));
 return r$;
}
::io$n::errno$n::errno_or_value$v$p$meta$n$$size_t$t$r$ file_write_all$f(const pxcrt::file& f$, const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& buf$) {
 pxcrt::bt_size_t len_r$ = pxcrt::bt_size_t((0LL));
 pxcrt::bt_size_t curpos$ = pxcrt::bt_size_t((0LL));
 pxcrt::bt_size_t endpos$ = buf$.size();
 while (curpos$ != endpos$) {
  const ::io$n::errno$n::errno_or_value$v$p$meta$n$$size_t$t$r$ r$ = ::io$n::file$n::file_write$f(f$ , ::pxcrt::bt_cslice< pxcrt::bt_uchar >(buf$, curpos$, endpos$));
  if ((r$.get_$e() == ::io$n::errno$n::errno_or_value$v$p$meta$n$$size_t$t$r$::errno$$e)) {
   return r$;
  }
  const pxcrt::bt_size_t wlen$ = (::operator$n::union_field$f$p$value$ls$q$io$$errno$n$$errno_or_value$v$p$meta$n$$size_t$t$r$$r$(r$));
  if (wlen$ == pxcrt::bt_size_t((0LL))) {
   break;
  }
  curpos$ += wlen$;
  len_r$ += wlen$;
 }
 ::io$n::errno$n::errno_or_value$v$p$meta$n$$size_t$t$r$ r$;
 (r$.value$$l(len_r$));
 return r$;
}
::io$n::errno$n::errno_or_value$v$p$pxcrt$$ptr$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$r$$r$ io_read_file$f(const pxcrt::io& iop$, const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& fn$) {
 ::io$n::errno$n::errno_or_value$v$p$pxcrt$$ptr$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$r$$r$ r$;
 const ::io$n::errno$n::errno_or_value$v$p$io$$file$n$$file_st$s$r$ f$ = ::io$n::file$n::io_open$f$p$m$ll$p$m$ll$p$io$n$$io$s$q$0$li$r$$q$m$ll$p$container$$array$n$$cslice$s$p$meta$n$$uchar$t$r$$q$0$li$r$$q$m$ll$p$io$$file$n$$open_flags_t$s$q$0$li$r$$r$$r$(iop$ , fn$ , O_RDONLY);
 {
  ::io$n::errno$n::errno_or_value$v$p$io$$file$n$$file_st$s$r$ const& ag$fe = (f$);
  if (ag$fe.get_$e() == ::io$n::errno$n::errno_or_value$v$p$io$$file$n$$file_st$s$r$::errno$$e) {
   const int err$ = ag$fe.errno$$r();
   {
    (r$.errno$$l(err$));
   }
  } else {
   const pxcrt::rcptr< pxcrt::rcval< ::pxcrt::pxcvarray< pxcrt::bt_uchar > > > buf$ = ::pointer$n::make_ptr$f$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$q$m$ll$r$();
   const ::io$n::errno$n::errno_or_value$v$p$meta$n$$size_t$t$r$ ra$ = ::io$n::file$n::file_read_all$f((::operator$n::union_field$f$p$value$ls$q$io$$errno$n$$errno_or_value$v$p$io$$file$n$$file_st$s$r$$r$(f$)) , ((pxcrt::rcptr< pxcrt::rcval< ::pxcrt::pxcvarray< pxcrt::bt_uchar > > >(buf$))->value));
   {
    ::io$n::errno$n::errno_or_value$v$p$meta$n$$size_t$t$r$ const& ag$fe = (ra$);
    if (ag$fe.get_$e() == ::io$n::errno$n::errno_or_value$v$p$meta$n$$size_t$t$r$::errno$$e) {
     const int err$ = ag$fe.errno$$r();
     {
      (r$.errno$$l(err$));
     }
    } else {
     (r$.value$$l(buf$));
    }
   }
  }
 }
 return r$;
}
::io$n::errno$n::errno_or_value$v$p$meta$n$$size_t$t$r$ io_write_file$f(const pxcrt::io& iop$, const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& fn$, const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& buf$) {
 ::io$n::errno$n::errno_or_value$v$p$meta$n$$size_t$t$r$ r$;
 const ::io$n::errno$n::errno_or_value$v$p$io$$file$n$$file_st$s$r$ f$ = ::io$n::file$n::io_open$f$p$m$ll$p$m$ll$p$io$n$$io$s$q$0$li$r$$q$m$ll$p$container$$array$n$$cslice$s$p$meta$n$$uchar$t$r$$q$0$li$r$$q$m$ll$p$io$$file$n$$open_flags_t$s$q$0$li$r$$q$m$ll$p$io$$file$n$$mode_t$s$q$0$li$r$$r$$r$(iop$ , fn$ , O_WRONLY | O_CREAT | O_TRUNC , S_IRWXU | S_IRWXG | S_IRWXO);
 {
  ::io$n::errno$n::errno_or_value$v$p$io$$file$n$$file_st$s$r$ const& ag$fe = (f$);
  if (ag$fe.get_$e() == ::io$n::errno$n::errno_or_value$v$p$io$$file$n$$file_st$s$r$::errno$$e) {
   const int err$ = ag$fe.errno$$r();
   {
    (r$.errno$$l(err$));
   }
  } else {
   r$ = ::io$n::file$n::file_write_all$f((::operator$n::union_field$f$p$value$ls$q$io$$errno$n$$errno_or_value$v$p$io$$file$n$$file_st$s$r$$r$(f$)) , buf$);
  }
 }
 return r$;
}
::io$n::errno$n::errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$ io_pipe$f(const pxcrt::io& sys$) {
 ::io$n::errno$n::errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$ r$;
 int err$ = 0;
 ::pxcrt::pxcvarray< pxcrt::file_mt > v$ = pxcrt::io_pipe_internal(sys$ , err$);
 if (err$ != 0) {
  (r$.errno$$l(err$));
 } else {
  (r$.value$$l(::algebraic$n::pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$((::pxcrt::pxcvarray< pxcrt::file_mt >::guard_ref< ::pxcrt::pxcvarray< pxcrt::file_mt > > (v$).get())[pxcrt::bt_size_t((0LL))] , (::pxcrt::pxcvarray< pxcrt::file_mt >::guard_ref< ::pxcrt::pxcvarray< pxcrt::file_mt > > (v$).get())[pxcrt::bt_size_t(1LL)])));
 }
 return r$;
}
};}; /* namespace io::file */
namespace io$n { namespace process$n { 
void io__exit$f(const pxcrt::io& sys$, pxcrt::bt_int st$) {
 pxcrt::_exit_internal(sys$ , st$);
}
void exit$f(pxcrt::bt_int st$) {
 pxcrt::exit_internal(pxcrt::io_system , st$);
}
void io_abort$f(const pxcrt::io& sys$) {
 pxcrt::abort_internal(sys$);
}
::io$n::errno$n::errno_or_value$v$p$io$$process$n$$pid_t$s$r$ io_fork$f(const pxcrt::io& sys$) {
 const pxcrt::pid_t pid$ = pxcrt::fork_internal(sys$);
 ::io$n::errno$n::errno_or_value$v$p$io$$process$n$$pid_t$s$r$ r$;
 if (pid$ < pxcrt::pid_t((0LL))) {
  (r$.errno$$l(::io$n::errno$n::io_get_errno$f(sys$)));
 } else {
  (r$.value$$l(pid$));
 }
 return r$;
}
pipe_process$s::pipe_process$s(pxcrt::pid_t pid0$, const pxcrt::file_mt& file0$) : pid$(pid0$), file$(file0$) {
}
::io$n::errno$n::errno_or_value$v$p$io$$process$n$$pipe_process$s$r$ io_popen$f(const pxcrt::io& sys$, const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& command$, const ::pxcrt::bt_cslice< ::pxcrt::pxcvarray< pxcrt::bt_uchar > >& argv$, pxcrt::bt_bool for_write$, pxcrt::bt_bool search_path$) {
 ::io$n::errno$n::errno_or_value$v$p$io$$process$n$$pipe_process$s$r$ r$;
 ::io$n::errno$n::errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$ p$ = ::io$n::file$n::io_pipe$f(sys$);
 {
  ::io$n::errno$n::errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$ const& ag$fe = (p$);
  if (ag$fe.get_$e() == ::io$n::errno$n::errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$::errno$$e) {
   const int e$ = ag$fe.errno$$r();
   {
    (r$.errno$$l(e$));
    return r$;
   }
  }
 }
 const ::io$n::errno$n::errno_or_value$v$p$io$$process$n$$pid_t$s$r$ rfork$ = ::io$n::process$n::io_fork$f(sys$);
 {
  ::io$n::errno$n::errno_or_value$v$p$io$$process$n$$pid_t$s$r$ const& ag$fe = (rfork$);
  if (ag$fe.get_$e() == ::io$n::errno$n::errno_or_value$v$p$io$$process$n$$pid_t$s$r$::errno$$e) {
   const int e$ = ag$fe.errno$$r();
   {
    (r$.errno$$l(e$));
    return r$;
   }
  }
 }
 const pxcrt::pid_t pid$ = (::operator$n::union_field$f$p$value$ls$q$io$$errno$n$$errno_or_value$v$p$io$$process$n$$pid_t$s$r$$r$(rfork$));
 if (pid$ == pxcrt::pid_t((0LL))) {
  try {
   int err$ = int();
   if (for_write$) {
    err$ = pxcrt::dup2_internal(sys$ , (::operator$n::union_field$f$p$value$ls$q$io$$errno$n$$errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$$r$(p$)).first$ , pxcrt::io_stdin(sys$));
   } else {
    err$ = pxcrt::dup2_internal(sys$ , (::operator$n::union_field$f$p$value$ls$q$io$$errno$n$$errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$$r$(p$)).second$ , pxcrt::io_stdout(sys$));
   }
   (p$.errno$$l(0));
   if (err$ == 0) {
    if (search_path$) {
     pxcrt::execvp(sys$ , command$ , argv$);
    } else {
     pxcrt::execv(sys$ , command$ , argv$);
    }
   }
  } catch (const std::exception& ex$) {
  }
  ::io$n::process$n::io__exit$f(sys$ , 127LL);
 }
 if (for_write$) {
  (r$.value$$l(::io$n::process$n::pipe_process$s(pid$ , (::operator$n::union_field$f$p$value$ls$q$io$$errno$n$$errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$$r$(p$)).second$)));
 } else {
  (r$.value$$l(::io$n::process$n::pipe_process$s(pid$ , (::operator$n::union_field$f$p$value$ls$q$io$$errno$n$$errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$$r$(p$)).first$)));
 }
 return r$;
}
wait_t$s::wait_t$s() : pid$(), status$(){
 /* pid$ */;
 /* status$ */;
}
wait_t$s::wait_t$s(pxcrt::pid_t pid$, int status$) : pid$(pid$), status$(status$){
 /* pid$ */;
 /* status$ */;
}
::io$n::errno$n::errno_or_value$v$p$io$$process$n$$wait_t$s$r$ io_wait$f(const pxcrt::io& sys$, int& st_r$) {
 int st$ = int();
 const pxcrt::pid_t pid$ = pxcrt::wait_internal(sys$ , st$);
 ::io$n::errno$n::errno_or_value$v$p$io$$process$n$$wait_t$s$r$ r$;
 if (pid$ < pxcrt::pid_t((0LL))) {
  (r$.errno$$l(::io$n::errno$n::io_get_errno$f(sys$)));
 } else {
  (r$.value$$l(::io$n::process$n::wait_t$s(pid$ , st$)));
 }
 return r$;
}
::io$n::errno$n::errno_or_value$v$p$io$$process$n$$wait_t$s$r$ io_waitpid$f(const pxcrt::io& sys$, pxcrt::pid_t p$, int opt$) {
 int st$ = int();
 const pxcrt::pid_t pid$ = pxcrt::waitpid_internal(sys$ , p$ , st$ , opt$);
 ::io$n::errno$n::errno_or_value$v$p$io$$process$n$$wait_t$s$r$ r$;
 if (pid$ < pxcrt::pid_t((0LL))) {
  (r$.errno$$l(::io$n::errno$n::io_get_errno$f(sys$)));
 } else {
  (r$.value$$l(::io$n::process$n::wait_t$s(pid$ , st$)));
 }
 return r$;
}
::io$n::errno$n::errno_or_value$v$p$io$$process$n$$status_t$s$r$ io_shell_exec$f(const pxcrt::io& sys$, const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& cmd$) {
 const pxcrt::bt_int st$ = pxcrt::shell_exec_internal(sys$ , cmd$);
 ::io$n::errno$n::errno_or_value$v$p$io$$process$n$$status_t$s$r$ r$;
 if (st$ < (0LL)) {
  (r$.errno$$l(::io$n::errno$n::io_get_errno$f(sys$)));
 } else {
  (r$.value$$l(::numeric$n::cast$n::static_cast$f$p$io$$process$n$$status_t$s$q$meta$n$$int$t$r$(st$)));
 }
 return r$;
}
};}; /* namespace io::process */
namespace thread$n { namespace queue$n { 
void task_queue$s::push$f(const ::callable$n::tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$& e$) {
 ::thread$n::queue$n::queue_push_notify$f(shared$ , e$);
}
task_queue$s::~task_queue$s() PXC_NOTHROW {
 try {
  ::thread$n::queue$n::queue_thread_stop$f(shared$ , 1LL);
 } catch (...) { ::abort(); }
}
void task_queue$s::construct$f(pxcrt::bt_size_t num_thrs$) {
 for (pxcrt::bt_size_t i$ = pxcrt::bt_size_t((0LL)); i$ < num_thrs$; ++i$) {
  ::pxcrt::array_push_back< ::pxcrt::pxcvarray< ::callable$n::callable_ptr$s$p$meta$n$$void$t$q$m$ll$r$ >,::callable$n::callable_ptr$s$p$meta$n$$void$t$q$m$ll$r$ >(thrs$ , ::thread$n::make_thread_ptr$f$p$thread$$queue$n$$queue_thread_main$f$r$(shared$));
 }
}
task_queue$s::task_queue$s(pxcrt::bt_size_t num_thrs$) : shared$(::pointer$n::make_tptr$f$p$thread$$queue$n$$task_queue_shared$s$q$m$ll$r$()), thrs$() {
 construct$f(num_thrs$);
}
task_queue_shared$s::task_queue_shared$s() : queue$(), stop_thread$((0LL)) {
}
::algebraic$n::option$v$p$callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$r$ queue_wait_pop$f(const pxcrt::rcptr< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > >& tq$) {
 ::algebraic$n::option$v$p$callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$r$ ent$;
 pxcrt::lock_guard< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > > lck$((tq$));
 ::thread$n::queue$n::task_queue_shared$s& q$ = ((lck$)->value);
 while (true) {
  const pxcrt::bt_int sf$ = q$.stop_thread$;
  const pxcrt::bt_bool em$ = q$.queue$.empty();
  if (sf$ < (0LL) || (sf$ > (0LL) && em$)) {
   break;
  }
  if (q$.queue$.empty()) {
   lck$.wait();
   continue;
  }
  (ent$.some$$l(::pxcrt::array_pop_front< ::pxcrt::pxcdeque< ::callable$n::tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$ > >(q$.queue$)));
  break;
 }
 return ent$;
}
void queue_thread_main$f(const pxcrt::rcptr< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > >& tq$) {
 while (true) {
  ::algebraic$n::option$v$p$callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$r$ ent$ = ::thread$n::queue$n::queue_wait_pop$f(tq$);
  if ((ent$.get_$e() == ::algebraic$n::option$v$p$callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$r$::none$$e)) {
   break;
  }
  (::operator$n::union_field$f$p$some$ls$q$algebraic$n$$option$v$p$callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$r$$r$(ent$)).__call$f();
 }
}
void queue_thread_stop$f(const pxcrt::rcptr< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > >& tq$, pxcrt::bt_int mode$) {
 pxcrt::lock_guard< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > > lck$((tq$));
 (((lck$)->value)).stop_thread$ = mode$;
 lck$.notify_all();
}
void queue_push_notify$f(const pxcrt::rcptr< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > >& tq$, const ::callable$n::tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$& ep$) {
 pxcrt::lock_guard< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > > lck$((tq$));
 ::thread$n::queue$n::task_queue_shared$s& q$ = ((lck$)->value);
 const pxcrt::bt_bool need_notify$ = q$.queue$.empty();
 ::pxcrt::array_push_back< ::pxcrt::pxcdeque< ::callable$n::tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$ >,::callable$n::tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$ >(q$.queue$ , ep$);
 if (need_notify$) {
  lck$.notify_one();
 }
}
};}; /* namespace thread::queue */
namespace text$n { namespace string$n { namespace serialize$n { 
static inline void string_append_char$f$p$42$li$r$(::pxcrt::pxcvarray< pxcrt::bt_uchar >& v$) {
 ::pxcrt::array_push_back< ::pxcrt::pxcvarray< pxcrt::bt_uchar >,pxcrt::bt_long >(v$ , 42LL);
}
static inline ::pxcrt::pxcvarray< pxcrt::bt_uchar > string_parse_token$f$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$q$text$$string$$serialize$n$$str_decode$f83$r$(::pxcrt::bt_cslice< pxcrt::bt_uchar >& s$) {
 const pxcrt::bt_size_t toklen$ = ::text$n::string$n::serialize$n::token_length$f(s$);
 ::pxcrt::bt_cslice< pxcrt::bt_uchar > stok$ = ::pxcrt::bt_cslice< pxcrt::bt_uchar >(s$, pxcrt::bt_size_t((0LL)), toklen$);
 s$.increment_front(toklen$);
 return ::text$n::string$n::serialize$n::str_decode$f83(stok$);
}
static inline pxcrt::bt_bool string_parse_token$f$p$meta$n$$bool$t$q$text$$string$$serialize$n$$parse_bool$f87$r$(::pxcrt::bt_cslice< pxcrt::bt_uchar >& s$) {
 const pxcrt::bt_size_t toklen$ = ::text$n::string$n::serialize$n::token_length$f(s$);
 ::pxcrt::bt_cslice< pxcrt::bt_uchar > stok$ = ::pxcrt::bt_cslice< pxcrt::bt_uchar >(s$, pxcrt::bt_size_t((0LL)), toklen$);
 s$.increment_front(toklen$);
 return ::text$n::string$n::serialize$n::parse_bool$f87(stok$);
}
};};}; /* namespace text::string::serialize */
namespace exception$n { 
unexpected_value_template$s$p$io$$errno$n$$errno_t$s$r$::unexpected_value_template$s$p$io$$errno$n$$errno_t$s$r$(const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& m$) : count$z(1), msg$(::pxcrt::pxcvarray< pxcrt::bt_uchar >(m$)) {
}
inline ::pxcrt::pxcvarray< pxcrt::bt_uchar > unexpected_value_template$s$p$io$$errno$n$$errno_t$s$r$::message() const {
 ::pxcrt::pxcvarray< pxcrt::bt_uchar > s$ = ::pxcrt::pxcvarray< pxcrt::bt_uchar >(pxcrt::bt_strlit("unexpected_value{errno_t}("));
 ::pxcrt::array_append< ::pxcrt::pxcvarray< pxcrt::bt_uchar >,::pxcrt::pxcvarray< pxcrt::bt_uchar > >(s$ , msg$);
 ::pxcrt::array_append< ::pxcrt::pxcvarray< pxcrt::bt_uchar >,::pxcrt::bt_strlit >(s$ , pxcrt::bt_strlit(")"));
 return s$;
}
}; /* namespace exception */
namespace text$n { namespace string$n { namespace serialize$n { 
static inline ::pxcrt::pxcvarray< pxcrt::bt_uchar > to_string$f$p$io$$errno$n$$errno_t$s$r$(const int& x$) {
 /* staticif empty *//* staticif-else */
 ::pxcrt::pxcvarray< pxcrt::bt_uchar > s$ = ::pxcrt::pxcvarray< pxcrt::bt_uchar >();
 ::text$n::string$n::serialize$n::serialize_to_string$f$p$io$$errno$n$$errno_t$s$r$(x$ , s$);
 return s$;
 /* staticif-else end */
}
static inline void serialize_to_string$f$p$io$$errno$n$$errno_t$s$r$(const int& x$, ::pxcrt::pxcvarray< pxcrt::bt_uchar >& out$) {
 ::text$n::serialize$n::serialize$f$p$text$$string$$serialize$n$$ser_default$s$q$io$$errno$n$$errno_t$s$r$(x$ , out$);
}
};};}; /* namespace text::string::serialize */
namespace text$n { namespace serialize$n { 
static inline void serialize$f$p$text$$string$$serialize$n$$ser_default$s$q$io$$errno$n$$errno_t$s$r$(const int& x$, ::pxcrt::pxcvarray< pxcrt::bt_uchar >& o$) {
 /* staticif */
 ::io$n::errno$n::errno_t_append_to_string$f(x$ , o$);
 /* staticif end */
}
};}; /* namespace text::serialize */
namespace numeric$n { namespace cast$n { 
static inline pxcrt::bt_double static_cast$f$p$meta$n$$double$t$q$io$$time$n$$time_t$s$r$(const std::time_t& x$) {
 /* staticif */
 numeric::static_cast_impl< pxcrt::bt_double,std::time_t > c$ = numeric::static_cast_impl< pxcrt::bt_double,std::time_t >();
 return c$.convert(x$);
 /* staticif end */
}
static inline pxcrt::bt_double static_cast$f$p$meta$n$$double$t$q$io$$time$n$$suseconds_t$s$r$(const pxcrt::suseconds_t& x$) {
 /* staticif */
 numeric::static_cast_impl< pxcrt::bt_double,pxcrt::suseconds_t > c$ = numeric::static_cast_impl< pxcrt::bt_double,pxcrt::suseconds_t >();
 return c$.convert(x$);
 /* staticif end */
}
};}; /* namespace numeric::cast */
namespace io$n { namespace errno$n { 
void errno_or_value$v$p$io$$file$n$$file_st$s$r$::init$(const errno_or_value$v$p$io$$file$n$$file_st$s$r$ & x) {
 _$e = errno$$e;
 switch (x.get_$e()) {
 case errno$$e: *(errno$$p()) = *(x.errno$$p()); break;
 case value$$e: new(value$$p()) pxcrt::file_st(*(x.value$$p())); break;
 }
 _$e = x._$e;
};
void errno_or_value$v$p$io$$file$n$$file_st$s$r$::deinit$() {
 typedef int io$$errno$n$$errno_t$s$dtor;
 typedef pxcrt::file_st io$$file$n$$file_st$s$dtor;
 switch (get_$e()) {
 case errno$$e: /* pod */; break;
 case value$$e: value$$p()->~io$$file$n$$file_st$s$dtor(); break;
 }
};
int errno_or_value$v$p$io$$file$n$$file_st$s$r$::errno$$r() const {
 if (get_$e() != errno$$e) { pxcrt::throw_invalid_field(); }
 return *errno$$p();
}
pxcrt::file_st errno_or_value$v$p$io$$file$n$$file_st$s$r$::value$$r() const {
 if (get_$e() != value$$e) { pxcrt::throw_invalid_field(); }
 return *value$$p();
}
int errno_or_value$v$p$io$$file$n$$file_st$s$r$::errno$$l(int x) {
 { deinit$(); _$e = errno$$e; *(errno$$p()) = x; _$e = errno$$e; }
 return (*errno$$p());
}
pxcrt::file_st errno_or_value$v$p$io$$file$n$$file_st$s$r$::value$$l(pxcrt::file_st x) {
 { deinit$(); _$e = errno$$e; new(value$$p()) pxcrt::file_st(x); _$e = value$$e; }
 return (*value$$p());
}
void errno_or_value$v$p$io$$file$n$$file_mt$s$r$::init$(const errno_or_value$v$p$io$$file$n$$file_mt$s$r$ & x) {
 _$e = errno$$e;
 switch (x.get_$e()) {
 case errno$$e: *(errno$$p()) = *(x.errno$$p()); break;
 case value$$e: new(value$$p()) pxcrt::file_mt(*(x.value$$p())); break;
 }
 _$e = x._$e;
};
void errno_or_value$v$p$io$$file$n$$file_mt$s$r$::deinit$() {
 typedef int io$$errno$n$$errno_t$s$dtor;
 typedef pxcrt::file_mt io$$file$n$$file_mt$s$dtor;
 switch (get_$e()) {
 case errno$$e: /* pod */; break;
 case value$$e: value$$p()->~io$$file$n$$file_mt$s$dtor(); break;
 }
};
int errno_or_value$v$p$io$$file$n$$file_mt$s$r$::errno$$r() const {
 if (get_$e() != errno$$e) { pxcrt::throw_invalid_field(); }
 return *errno$$p();
}
pxcrt::file_mt errno_or_value$v$p$io$$file$n$$file_mt$s$r$::value$$r() const {
 if (get_$e() != value$$e) { pxcrt::throw_invalid_field(); }
 return *value$$p();
}
int errno_or_value$v$p$io$$file$n$$file_mt$s$r$::errno$$l(int x) {
 { deinit$(); _$e = errno$$e; *(errno$$p()) = x; _$e = errno$$e; }
 return (*errno$$p());
}
pxcrt::file_mt errno_or_value$v$p$io$$file$n$$file_mt$s$r$::value$$l(pxcrt::file_mt x) {
 { deinit$(); _$e = errno$$e; new(value$$p()) pxcrt::file_mt(x); _$e = value$$e; }
 return (*value$$p());
}
void errno_or_value$v$p$meta$n$$size_t$t$r$::init$(const errno_or_value$v$p$meta$n$$size_t$t$r$ & x) {
 _$e = errno$$e;
 switch (x.get_$e()) {
 case errno$$e: *(errno$$p()) = *(x.errno$$p()); break;
 case value$$e: *(value$$p()) = *(x.value$$p()); break;
 }
 _$e = x._$e;
};
void errno_or_value$v$p$meta$n$$size_t$t$r$::deinit$() {
};
int errno_or_value$v$p$meta$n$$size_t$t$r$::errno$$r() const {
 if (get_$e() != errno$$e) { pxcrt::throw_invalid_field(); }
 return *errno$$p();
}
pxcrt::bt_size_t errno_or_value$v$p$meta$n$$size_t$t$r$::value$$r() const {
 if (get_$e() != value$$e) { pxcrt::throw_invalid_field(); }
 return *value$$p();
}
int errno_or_value$v$p$meta$n$$size_t$t$r$::errno$$l(int x) {
 { deinit$(); _$e = errno$$e; *(errno$$p()) = x; _$e = errno$$e; }
 return (*errno$$p());
}
pxcrt::bt_size_t errno_or_value$v$p$meta$n$$size_t$t$r$::value$$l(pxcrt::bt_size_t x) {
 { deinit$(); _$e = errno$$e; *(value$$p()) = x; _$e = value$$e; }
 return (*value$$p());
}
void errno_or_value$v$p$io$$file$n$$off_t$s$r$::init$(const errno_or_value$v$p$io$$file$n$$off_t$s$r$ & x) {
 _$e = errno$$e;
 switch (x.get_$e()) {
 case errno$$e: *(errno$$p()) = *(x.errno$$p()); break;
 case value$$e: *(value$$p()) = *(x.value$$p()); break;
 }
 _$e = x._$e;
};
void errno_or_value$v$p$io$$file$n$$off_t$s$r$::deinit$() {
};
int errno_or_value$v$p$io$$file$n$$off_t$s$r$::errno$$r() const {
 if (get_$e() != errno$$e) { pxcrt::throw_invalid_field(); }
 return *errno$$p();
}
pxcrt::off_t errno_or_value$v$p$io$$file$n$$off_t$s$r$::value$$r() const {
 if (get_$e() != value$$e) { pxcrt::throw_invalid_field(); }
 return *value$$p();
}
int errno_or_value$v$p$io$$file$n$$off_t$s$r$::errno$$l(int x) {
 { deinit$(); _$e = errno$$e; *(errno$$p()) = x; _$e = errno$$e; }
 return (*errno$$p());
}
pxcrt::off_t errno_or_value$v$p$io$$file$n$$off_t$s$r$::value$$l(pxcrt::off_t x) {
 { deinit$(); _$e = errno$$e; *(value$$p()) = x; _$e = value$$e; }
 return (*value$$p());
}
};}; /* namespace io::errno */
namespace operator$n { 
static inline pxcrt::bt_size_t union_field$f$p$value$ls$q$io$$errno$n$$errno_or_value$v$p$meta$n$$size_t$t$r$$r$(const ::io$n::errno$n::errno_or_value$v$p$meta$n$$size_t$t$r$& x$) {
 if (!(x$.get_$e() == ::io$n::errno$n::errno_or_value$v$p$meta$n$$size_t$t$r$::value$$e)) {
  {
   if ((x$.get_$e() == ::io$n::errno$n::errno_or_value$v$p$meta$n$$size_t$t$r$::errno$$e)) {
    PXC_THROW(::exception$n::unexpected_value_template$s$p$errno__t$ls$r$((::pxcrt::pxcvarray< pxcrt::bt_uchar >::guard_ref< const ::pxcrt::pxcvarray< pxcrt::bt_uchar > > (::text$n::string$n::serialize$n::to_string$f$p$io$$errno$n$$errno_t$s$r$(int((x$.errno$$r())))).get_crange())));
   }
  }
  /* staticif empty */
 }
 return (x$.value$$r());
}
}; /* namespace operator */
namespace exception$n { 
unexpected_value_template$s$p$errno__t$ls$r$::unexpected_value_template$s$p$errno__t$ls$r$(const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& m$) : count$z(1), msg$(::pxcrt::pxcvarray< pxcrt::bt_uchar >(m$)) {
}
inline ::pxcrt::pxcvarray< pxcrt::bt_uchar > unexpected_value_template$s$p$errno__t$ls$r$::message() const {
 ::pxcrt::pxcvarray< pxcrt::bt_uchar > s$ = ::pxcrt::pxcvarray< pxcrt::bt_uchar >(pxcrt::bt_strlit("unexpected_value{errno_t}("));
 ::pxcrt::array_append< ::pxcrt::pxcvarray< pxcrt::bt_uchar >,::pxcrt::pxcvarray< pxcrt::bt_uchar > >(s$ , msg$);
 ::pxcrt::array_append< ::pxcrt::pxcvarray< pxcrt::bt_uchar >,::pxcrt::bt_strlit >(s$ , pxcrt::bt_strlit(")"));
 return s$;
}
}; /* namespace exception */
namespace io$n { namespace errno$n { 
void errno_or_value$v$p$pxcrt$$ptr$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$r$$r$::init$(const errno_or_value$v$p$pxcrt$$ptr$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$r$$r$ & x) {
 _$e = errno$$e;
 switch (x.get_$e()) {
 case errno$$e: *(errno$$p()) = *(x.errno$$p()); break;
 case value$$e: new(value$$p()) pxcrt::rcptr< pxcrt::rcval< ::pxcrt::pxcvarray< pxcrt::bt_uchar > > >(*(x.value$$p())); break;
 }
 _$e = x._$e;
};
void errno_or_value$v$p$pxcrt$$ptr$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$r$$r$::deinit$() {
 typedef int io$$errno$n$$errno_t$s$dtor;
 typedef pxcrt::rcptr< pxcrt::rcval< ::pxcrt::pxcvarray< pxcrt::bt_uchar > > > pxcrt$$ptr$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$r$$dtor;
 switch (get_$e()) {
 case errno$$e: /* pod */; break;
 case value$$e: value$$p()->~pxcrt$$ptr$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$r$$dtor(); break;
 }
};
int errno_or_value$v$p$pxcrt$$ptr$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$r$$r$::errno$$r() const {
 if (get_$e() != errno$$e) { pxcrt::throw_invalid_field(); }
 return *errno$$p();
}
pxcrt::rcptr< pxcrt::rcval< ::pxcrt::pxcvarray< pxcrt::bt_uchar > > > errno_or_value$v$p$pxcrt$$ptr$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$r$$r$::value$$r() const {
 if (get_$e() != value$$e) { pxcrt::throw_invalid_field(); }
 return *value$$p();
}
int errno_or_value$v$p$pxcrt$$ptr$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$r$$r$::errno$$l(int x) {
 { deinit$(); _$e = errno$$e; *(errno$$p()) = x; _$e = errno$$e; }
 return (*errno$$p());
}
pxcrt::rcptr< pxcrt::rcval< ::pxcrt::pxcvarray< pxcrt::bt_uchar > > > errno_or_value$v$p$pxcrt$$ptr$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$r$$r$::value$$l(pxcrt::rcptr< pxcrt::rcval< ::pxcrt::pxcvarray< pxcrt::bt_uchar > > > x) {
 { deinit$(); _$e = errno$$e; new(value$$p()) pxcrt::rcptr< pxcrt::rcval< ::pxcrt::pxcvarray< pxcrt::bt_uchar > > >(x); _$e = value$$e; }
 return (*value$$p());
}
};}; /* namespace io::errno */
namespace io$n { namespace file$n { 
static inline ::io$n::errno$n::errno_or_value$v$p$io$$file$n$$file_st$s$r$ io_open$f$p$m$ll$p$m$ll$p$io$n$$io$s$q$0$li$r$$q$m$ll$p$container$$array$n$$cslice$s$p$meta$n$$uchar$t$r$$q$0$li$r$$q$m$ll$p$io$$file$n$$open_flags_t$s$q$0$li$r$$r$$r$(const pxcrt::io& a0$, const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& a1$, const int& a2$) {
 int flags$ = O_RDONLY;
 pxcrt::mode_t  md$ = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
 {
  /* staticif */
  flags$ = a2$;
  /* staticif end */
 }
 /* staticif empty */
 /* staticif empty */
 return ::io$n::file$n::io_open_st$f(a0$ , a1$ , flags$ , md$);
}
};}; /* namespace io::file */
namespace pointer$n { 
static inline pxcrt::rcptr< pxcrt::rcval< ::pxcrt::pxcvarray< pxcrt::bt_uchar > > > make_ptr$f$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$q$m$ll$r$() {
 return ::pointer$n::box_pointer$f$p$pxcrt$$ptr$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$r$$q$m$ll$r$();
}
static inline pxcrt::rcptr< pxcrt::rcval< ::pxcrt::pxcvarray< pxcrt::bt_uchar > > > box_pointer$f$p$pxcrt$$ptr$p$container$$array$n$$varray$s$p$meta$n$$uchar$t$r$$r$$q$m$ll$r$() {
 /* staticif empty *//* staticif-else */
 pxcrt::rcval< ::pxcrt::pxcvarray< pxcrt::bt_uchar > >*const r$p1 = pxcrt::rcval< ::pxcrt::pxcvarray< pxcrt::bt_uchar > >::allocate();
 try {
 new (&r$p1->value) ::pxcrt::pxcvarray< pxcrt::bt_uchar >();
 new (&r$p1->count$z) pxcrt::stcount(); /* nothrow */
 } catch (...) {
 pxcrt::rcval< ::pxcrt::pxcvarray< pxcrt::bt_uchar > >::deallocate(r$p1);
 throw;
 }
 pxcrt::rcptr< pxcrt::rcval< ::pxcrt::pxcvarray< pxcrt::bt_uchar > > > r$((r$p1));
 return r$;
 /* staticif-else end */
}
}; /* namespace pointer */
namespace operator$n { 
static inline pxcrt::file_st union_field$f$p$value$ls$q$io$$errno$n$$errno_or_value$v$p$io$$file$n$$file_st$s$r$$r$(const ::io$n::errno$n::errno_or_value$v$p$io$$file$n$$file_st$s$r$& x$) {
 if (!(x$.get_$e() == ::io$n::errno$n::errno_or_value$v$p$io$$file$n$$file_st$s$r$::value$$e)) {
  {
   if ((x$.get_$e() == ::io$n::errno$n::errno_or_value$v$p$io$$file$n$$file_st$s$r$::errno$$e)) {
    PXC_THROW(::exception$n::unexpected_value_template$s$p$errno__t$ls$r$((::pxcrt::pxcvarray< pxcrt::bt_uchar >::guard_ref< const ::pxcrt::pxcvarray< pxcrt::bt_uchar > > (::text$n::string$n::serialize$n::to_string$f$p$io$$errno$n$$errno_t$s$r$(int((x$.errno$$r())))).get_crange())));
   }
  }
  /* staticif empty */
 }
 return (x$.value$$r());
}
}; /* namespace operator */
namespace io$n { namespace file$n { 
static inline ::io$n::errno$n::errno_or_value$v$p$io$$file$n$$file_st$s$r$ io_open$f$p$m$ll$p$m$ll$p$io$n$$io$s$q$0$li$r$$q$m$ll$p$container$$array$n$$cslice$s$p$meta$n$$uchar$t$r$$q$0$li$r$$q$m$ll$p$io$$file$n$$open_flags_t$s$q$0$li$r$$q$m$ll$p$io$$file$n$$mode_t$s$q$0$li$r$$r$$r$(const pxcrt::io& a0$, const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& a1$, const int& a2$, const pxcrt::mode_t & a3$) {
 int flags$ = O_RDONLY;
 pxcrt::mode_t  md$ = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
 {
  /* staticif */
  flags$ = a2$;
  /* staticif end */
 }
 {
  md$ = a3$;
 }
 /* staticif empty */
 return ::io$n::file$n::io_open_st$f(a0$ , a1$ , flags$ , md$);
}
};}; /* namespace io::file */
namespace algebraic$n { 
pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$::pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$(const pxcrt::file_mt& a0$, const pxcrt::file_mt& a1$) : first$(a0$), second$(a1$) {
}
}; /* namespace algebraic */
namespace io$n { namespace errno$n { 
void errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$::init$(const errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$ & x) {
 _$e = errno$$e;
 switch (x.get_$e()) {
 case errno$$e: *(errno$$p()) = *(x.errno$$p()); break;
 case value$$e: new(value$$p()) ::algebraic$n::pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$(*(x.value$$p())); break;
 }
 _$e = x._$e;
};
void errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$::deinit$() {
 typedef int io$$errno$n$$errno_t$s$dtor;
 typedef ::algebraic$n::pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$ algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$dtor;
 switch (get_$e()) {
 case errno$$e: /* pod */; break;
 case value$$e: value$$p()->~algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$dtor(); break;
 }
};
int errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$::errno$$r() const {
 if (get_$e() != errno$$e) { pxcrt::throw_invalid_field(); }
 return *errno$$p();
}
::algebraic$n::pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$ errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$::value$$r() const {
 if (get_$e() != value$$e) { pxcrt::throw_invalid_field(); }
 return *value$$p();
}
int errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$::errno$$l(int x) {
 { deinit$(); _$e = errno$$e; *(errno$$p()) = x; _$e = errno$$e; }
 return (*errno$$p());
}
::algebraic$n::pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$ errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$::value$$l(::algebraic$n::pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$ x) {
 { deinit$(); _$e = errno$$e; new(value$$p()) ::algebraic$n::pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$(x); _$e = value$$e; }
 return (*value$$p());
}
};}; /* namespace io::errno */
namespace algebraic$n { 
void option$v$p$io$$file$n$$file_st$s$r$::init$(const option$v$p$io$$file$n$$file_st$s$r$ & x) {
 _$e = none$$e;
 switch (x.get_$e()) {
 case none$$e: /* unit */; break;
 case some$$e: new(some$$p()) pxcrt::file_st(*(x.some$$p())); break;
 }
 _$e = x._$e;
};
void option$v$p$io$$file$n$$file_st$s$r$::deinit$() {
 typedef pxcrt::bt_unit meta$n$$unit$t$dtor;
 typedef pxcrt::file_st io$$file$n$$file_st$s$dtor;
 switch (get_$e()) {
 case none$$e: /* unit */; break;
 case some$$e: some$$p()->~io$$file$n$$file_st$s$dtor(); break;
 }
};
pxcrt::bt_unit option$v$p$io$$file$n$$file_st$s$r$::none$$r() const {
 if (get_$e() != none$$e) { pxcrt::throw_invalid_field(); }
 return pxcrt::unit_value;
}
pxcrt::file_st option$v$p$io$$file$n$$file_st$s$r$::some$$r() const {
 if (get_$e() != some$$e) { pxcrt::throw_invalid_field(); }
 return *some$$p();
}
pxcrt::bt_unit option$v$p$io$$file$n$$file_st$s$r$::none$$l(pxcrt::bt_unit x) {
 { deinit$(); _$e = none$$e; /* unit */; _$e = none$$e; }
 return pxcrt::unit_value;
}
pxcrt::file_st option$v$p$io$$file$n$$file_st$s$r$::some$$l(pxcrt::file_st x) {
 { deinit$(); _$e = none$$e; new(some$$p()) pxcrt::file_st(x); _$e = some$$e; }
 return (*some$$p());
}
}; /* namespace algebraic */
namespace io$n { namespace errno$n { 
void errno_or_value$v$p$io$$process$n$$pid_t$s$r$::init$(const errno_or_value$v$p$io$$process$n$$pid_t$s$r$ & x) {
 _$e = errno$$e;
 switch (x.get_$e()) {
 case errno$$e: *(errno$$p()) = *(x.errno$$p()); break;
 case value$$e: *(value$$p()) = *(x.value$$p()); break;
 }
 _$e = x._$e;
};
void errno_or_value$v$p$io$$process$n$$pid_t$s$r$::deinit$() {
};
int errno_or_value$v$p$io$$process$n$$pid_t$s$r$::errno$$r() const {
 if (get_$e() != errno$$e) { pxcrt::throw_invalid_field(); }
 return *errno$$p();
}
pxcrt::pid_t errno_or_value$v$p$io$$process$n$$pid_t$s$r$::value$$r() const {
 if (get_$e() != value$$e) { pxcrt::throw_invalid_field(); }
 return *value$$p();
}
int errno_or_value$v$p$io$$process$n$$pid_t$s$r$::errno$$l(int x) {
 { deinit$(); _$e = errno$$e; *(errno$$p()) = x; _$e = errno$$e; }
 return (*errno$$p());
}
pxcrt::pid_t errno_or_value$v$p$io$$process$n$$pid_t$s$r$::value$$l(pxcrt::pid_t x) {
 { deinit$(); _$e = errno$$e; *(value$$p()) = x; _$e = value$$e; }
 return (*value$$p());
}
void errno_or_value$v$p$io$$process$n$$pipe_process$s$r$::init$(const errno_or_value$v$p$io$$process$n$$pipe_process$s$r$ & x) {
 _$e = errno$$e;
 switch (x.get_$e()) {
 case errno$$e: *(errno$$p()) = *(x.errno$$p()); break;
 case value$$e: new(value$$p()) ::io$n::process$n::pipe_process$s(*(x.value$$p())); break;
 }
 _$e = x._$e;
};
void errno_or_value$v$p$io$$process$n$$pipe_process$s$r$::deinit$() {
 typedef int io$$errno$n$$errno_t$s$dtor;
 typedef ::io$n::process$n::pipe_process$s io$$process$n$$pipe_process$s$dtor;
 switch (get_$e()) {
 case errno$$e: /* pod */; break;
 case value$$e: value$$p()->~io$$process$n$$pipe_process$s$dtor(); break;
 }
};
int errno_or_value$v$p$io$$process$n$$pipe_process$s$r$::errno$$r() const {
 if (get_$e() != errno$$e) { pxcrt::throw_invalid_field(); }
 return *errno$$p();
}
::io$n::process$n::pipe_process$s errno_or_value$v$p$io$$process$n$$pipe_process$s$r$::value$$r() const {
 if (get_$e() != value$$e) { pxcrt::throw_invalid_field(); }
 return *value$$p();
}
int errno_or_value$v$p$io$$process$n$$pipe_process$s$r$::errno$$l(int x) {
 { deinit$(); _$e = errno$$e; *(errno$$p()) = x; _$e = errno$$e; }
 return (*errno$$p());
}
::io$n::process$n::pipe_process$s errno_or_value$v$p$io$$process$n$$pipe_process$s$r$::value$$l(::io$n::process$n::pipe_process$s x) {
 { deinit$(); _$e = errno$$e; new(value$$p()) ::io$n::process$n::pipe_process$s(x); _$e = value$$e; }
 return (*value$$p());
}
};}; /* namespace io::errno */
namespace operator$n { 
static inline pxcrt::pid_t union_field$f$p$value$ls$q$io$$errno$n$$errno_or_value$v$p$io$$process$n$$pid_t$s$r$$r$(const ::io$n::errno$n::errno_or_value$v$p$io$$process$n$$pid_t$s$r$& x$) {
 if (!(x$.get_$e() == ::io$n::errno$n::errno_or_value$v$p$io$$process$n$$pid_t$s$r$::value$$e)) {
  {
   if ((x$.get_$e() == ::io$n::errno$n::errno_or_value$v$p$io$$process$n$$pid_t$s$r$::errno$$e)) {
    PXC_THROW(::exception$n::unexpected_value_template$s$p$errno__t$ls$r$((::pxcrt::pxcvarray< pxcrt::bt_uchar >::guard_ref< const ::pxcrt::pxcvarray< pxcrt::bt_uchar > > (::text$n::string$n::serialize$n::to_string$f$p$io$$errno$n$$errno_t$s$r$(int((x$.errno$$r())))).get_crange())));
   }
  }
  /* staticif empty */
 }
 return (x$.value$$r());
}
static inline ::algebraic$n::pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$ union_field$f$p$value$ls$q$io$$errno$n$$errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$$r$(const ::io$n::errno$n::errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$& x$) {
 if (!(x$.get_$e() == ::io$n::errno$n::errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$::value$$e)) {
  {
   if ((x$.get_$e() == ::io$n::errno$n::errno_or_value$v$p$algebraic$n$$pair$s$p$io$$file$n$$file_mt$s$q$io$$file$n$$file_mt$s$r$$r$::errno$$e)) {
    PXC_THROW(::exception$n::unexpected_value_template$s$p$errno__t$ls$r$((::pxcrt::pxcvarray< pxcrt::bt_uchar >::guard_ref< const ::pxcrt::pxcvarray< pxcrt::bt_uchar > > (::text$n::string$n::serialize$n::to_string$f$p$io$$errno$n$$errno_t$s$r$(int((x$.errno$$r())))).get_crange())));
   }
  }
  /* staticif empty */
 }
 return (x$.value$$r());
}
}; /* namespace operator */
namespace io$n { namespace errno$n { 
void errno_or_value$v$p$io$$process$n$$wait_t$s$r$::init$(const errno_or_value$v$p$io$$process$n$$wait_t$s$r$ & x) {
 _$e = errno$$e;
 switch (x.get_$e()) {
 case errno$$e: *(errno$$p()) = *(x.errno$$p()); break;
 case value$$e: new(value$$p()) ::io$n::process$n::wait_t$s(*(x.value$$p())); break;
 }
 _$e = x._$e;
};
void errno_or_value$v$p$io$$process$n$$wait_t$s$r$::deinit$() {
 typedef int io$$errno$n$$errno_t$s$dtor;
 typedef ::io$n::process$n::wait_t$s io$$process$n$$wait_t$s$dtor;
 switch (get_$e()) {
 case errno$$e: /* pod */; break;
 case value$$e: value$$p()->~io$$process$n$$wait_t$s$dtor(); break;
 }
};
int errno_or_value$v$p$io$$process$n$$wait_t$s$r$::errno$$r() const {
 if (get_$e() != errno$$e) { pxcrt::throw_invalid_field(); }
 return *errno$$p();
}
::io$n::process$n::wait_t$s errno_or_value$v$p$io$$process$n$$wait_t$s$r$::value$$r() const {
 if (get_$e() != value$$e) { pxcrt::throw_invalid_field(); }
 return *value$$p();
}
int errno_or_value$v$p$io$$process$n$$wait_t$s$r$::errno$$l(int x) {
 { deinit$(); _$e = errno$$e; *(errno$$p()) = x; _$e = errno$$e; }
 return (*errno$$p());
}
::io$n::process$n::wait_t$s errno_or_value$v$p$io$$process$n$$wait_t$s$r$::value$$l(::io$n::process$n::wait_t$s x) {
 { deinit$(); _$e = errno$$e; new(value$$p()) ::io$n::process$n::wait_t$s(x); _$e = value$$e; }
 return (*value$$p());
}
void errno_or_value$v$p$io$$process$n$$status_t$s$r$::init$(const errno_or_value$v$p$io$$process$n$$status_t$s$r$ & x) {
 _$e = errno$$e;
 switch (x.get_$e()) {
 case errno$$e: *(errno$$p()) = *(x.errno$$p()); break;
 case value$$e: *(value$$p()) = *(x.value$$p()); break;
 }
 _$e = x._$e;
};
void errno_or_value$v$p$io$$process$n$$status_t$s$r$::deinit$() {
};
int errno_or_value$v$p$io$$process$n$$status_t$s$r$::errno$$r() const {
 if (get_$e() != errno$$e) { pxcrt::throw_invalid_field(); }
 return *errno$$p();
}
int errno_or_value$v$p$io$$process$n$$status_t$s$r$::value$$r() const {
 if (get_$e() != value$$e) { pxcrt::throw_invalid_field(); }
 return *value$$p();
}
int errno_or_value$v$p$io$$process$n$$status_t$s$r$::errno$$l(int x) {
 { deinit$(); _$e = errno$$e; *(errno$$p()) = x; _$e = errno$$e; }
 return (*errno$$p());
}
int errno_or_value$v$p$io$$process$n$$status_t$s$r$::value$$l(int x) {
 { deinit$(); _$e = errno$$e; *(value$$p()) = x; _$e = value$$e; }
 return (*value$$p());
}
};}; /* namespace io::errno */
namespace numeric$n { namespace cast$n { 
static inline int static_cast$f$p$io$$process$n$$status_t$s$q$meta$n$$int$t$r$(const pxcrt::bt_int& x$) {
 /* staticif */
 numeric::static_cast_impl< int,pxcrt::bt_int > c$ = numeric::static_cast_impl< int,pxcrt::bt_int >();
 return c$.convert(x$);
 /* staticif end */
}
};}; /* namespace numeric::cast */
namespace callable$n { 
tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$::tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$(const pxcrt::rcptr< ::callable$n::tcallable$i$p$meta$n$$void$t$q$m$ll$r$ >& p0$) : p$(p0$) {
}
inline void tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$::__call$f() const {
 return ((*((pxcrt::rcptr< ::callable$n::tcallable$i$p$meta$n$$void$t$q$m$ll$r$ >::guard_val< const pxcrt::rcptr< ::callable$n::tcallable$i$p$meta$n$$void$t$q$m$ll$r$ > > (p$).get())))).__call$f();
}
callable_ptr$s$p$meta$n$$void$t$q$m$ll$r$::callable_ptr$s$p$meta$n$$void$t$q$m$ll$r$(const pxcrt::rcptr< ::callable$n::callable$i$p$meta$n$$void$t$q$m$ll$r$ >& p0$) : p$(p0$) {
}
inline void callable_ptr$s$p$meta$n$$void$t$q$m$ll$r$::__call$f() const {
 return ((*(pxcrt::rcptr< ::callable$n::callable$i$p$meta$n$$void$t$q$m$ll$r$ >(p$)))).__call$f();
}
}; /* namespace callable */
namespace thread$n { 
static inline ::callable$n::callable_ptr$s$p$meta$n$$void$t$q$m$ll$r$ make_thread_ptr$f$p$thread$$queue$n$$queue_thread_main$f$r$(const pxcrt::rcptr< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > >& tq$) {
 return ::callable$n::callable_ptr$s$p$meta$n$$void$t$q$m$ll$r$(::pointer$n::make_ptr$f$p$thread$n$$thread$s$p$thread$$queue$n$$queue_thread_main$f$r$$q$m$ll$p$m$ll$p$pxcrt$$tptr$p$thread$$queue$n$$task_queue_shared$s$r$$q$0$li$r$$r$$r$(tq$));
}
thread$s$p$thread$$queue$n$$queue_thread_main$f$r$::thread$s$p$thread$$queue$n$$queue_thread_main$f$r$(const pxcrt::rcptr< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > >& tq$) : count$z(1), fobj$(tq$), need_join$(false), thd$() {
 init$f();
}
inline void thread$s$p$thread$$queue$n$$queue_thread_main$f$r$::init$f() {
 const int err$ = pxcrt::thread_create< ::thread$n::thread_main_funcobj$s$p$thread$$queue$n$$queue_thread_main$f$r$ >(thd$ , fobj$);
 if (err$ != 0) {
  PXC_THROW(::exception$n::runtime_error_template$s$p$thread__create$ls$r$((::pxcrt::pxcvarray< pxcrt::bt_uchar >::guard_ref< const ::pxcrt::pxcvarray< pxcrt::bt_uchar > > (::text$n::string$n::serialize$n::to_string$f$p$io$$errno$n$$errno_t$s$r$(err$)).get_crange())));
 }
 need_join$ = true;
}
inline thread$s$p$thread$$queue$n$$queue_thread_main$f$r$::~thread$s$p$thread$$queue$n$$queue_thread_main$f$r$() PXC_NOTHROW {
 try {
  __call$f();
 } catch (...) { ::abort(); }
}
inline void thread$s$p$thread$$queue$n$$queue_thread_main$f$r$::__call$f() {
 if (need_join$) {
  need_join$ = false;
  const int err$ = pxcrt::thread_join< ::thread$n::thread_main_funcobj$s$p$thread$$queue$n$$queue_thread_main$f$r$ >(thd$);
  if (err$ != 0) {
   PXC_THROW(::exception$n::runtime_error_template$s$p$thread__join$ls$r$((::pxcrt::pxcvarray< pxcrt::bt_uchar >::guard_ref< const ::pxcrt::pxcvarray< pxcrt::bt_uchar > > (::text$n::string$n::serialize$n::to_string$f$p$io$$errno$n$$errno_t$s$r$(err$)).get_crange())));
  }
 }
 /* staticif empty */
}
thread_main_funcobj$s$p$thread$$queue$n$$queue_thread_main$f$r$::thread_main_funcobj$s$p$thread$$queue$n$$queue_thread_main$f$r$(const pxcrt::rcptr< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > >& tq$) : fld$(tq$), ret$() {
}
inline void thread_main_funcobj$s$p$thread$$queue$n$$queue_thread_main$f$r$::__call$f() {
 /* staticif */
 ::thread$n::queue$n::queue_thread_main$f(fld$._0$);
 /* staticif end */
}
}; /* namespace thread */
namespace operator$n { 
tuple$s$p$m$ll$p$pxcrt$$tptr$p$thread$$queue$n$$task_queue_shared$s$r$$r$$q$m$ll$r$::tuple$s$p$m$ll$p$pxcrt$$tptr$p$thread$$queue$n$$task_queue_shared$s$r$$r$$q$m$ll$r$(const pxcrt::rcptr< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > >& a0$) : _0$(a0$) {
}
}; /* namespace operator */
namespace exception$n { 
runtime_error_template$s$p$thread__create$ls$r$::runtime_error_template$s$p$thread__create$ls$r$(const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& m$) : count$z(1), msg$(::pxcrt::pxcvarray< pxcrt::bt_uchar >(m$)) {
}
inline ::pxcrt::pxcvarray< pxcrt::bt_uchar > runtime_error_template$s$p$thread__create$ls$r$::message() const {
 ::pxcrt::pxcvarray< pxcrt::bt_uchar > s$ = ::pxcrt::pxcvarray< pxcrt::bt_uchar >(pxcrt::bt_strlit("runtime_error{thread_create}("));
 ::pxcrt::array_append< ::pxcrt::pxcvarray< pxcrt::bt_uchar >,::pxcrt::pxcvarray< pxcrt::bt_uchar > >(s$ , msg$);
 ::pxcrt::array_append< ::pxcrt::pxcvarray< pxcrt::bt_uchar >,::pxcrt::bt_strlit >(s$ , pxcrt::bt_strlit(")"));
 return s$;
}
runtime_error_template$s$p$thread__join$ls$r$::runtime_error_template$s$p$thread__join$ls$r$(const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& m$) : count$z(1), msg$(::pxcrt::pxcvarray< pxcrt::bt_uchar >(m$)) {
}
inline ::pxcrt::pxcvarray< pxcrt::bt_uchar > runtime_error_template$s$p$thread__join$ls$r$::message() const {
 ::pxcrt::pxcvarray< pxcrt::bt_uchar > s$ = ::pxcrt::pxcvarray< pxcrt::bt_uchar >(pxcrt::bt_strlit("runtime_error{thread_join}("));
 ::pxcrt::array_append< ::pxcrt::pxcvarray< pxcrt::bt_uchar >,::pxcrt::pxcvarray< pxcrt::bt_uchar > >(s$ , msg$);
 ::pxcrt::array_append< ::pxcrt::pxcvarray< pxcrt::bt_uchar >,::pxcrt::bt_strlit >(s$ , pxcrt::bt_strlit(")"));
 return s$;
}
}; /* namespace exception */
namespace pointer$n { 
static inline pxcrt::rcptr< ::thread$n::thread$s$p$thread$$queue$n$$queue_thread_main$f$r$ > make_ptr$f$p$thread$n$$thread$s$p$thread$$queue$n$$queue_thread_main$f$r$$q$m$ll$p$m$ll$p$pxcrt$$tptr$p$thread$$queue$n$$task_queue_shared$s$r$$q$0$li$r$$r$$r$(const pxcrt::rcptr< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > >& a0$) {
 return ::pointer$n::box_pointer$f$p$pxcrt$$ptr$p$thread$n$$thread$s$p$thread$$queue$n$$queue_thread_main$f$r$$r$$q$m$ll$p$m$ll$p$pxcrt$$tptr$p$thread$$queue$n$$task_queue_shared$s$r$$q$0$li$r$$r$$r$(a0$);
}
static inline pxcrt::rcptr< ::thread$n::thread$s$p$thread$$queue$n$$queue_thread_main$f$r$ > box_pointer$f$p$pxcrt$$ptr$p$thread$n$$thread$s$p$thread$$queue$n$$queue_thread_main$f$r$$r$$q$m$ll$p$m$ll$p$pxcrt$$tptr$p$thread$$queue$n$$task_queue_shared$s$r$$q$0$li$r$$r$$r$(const pxcrt::rcptr< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > >& a0$) {
 /* staticif empty *//* staticif-else */
 ::thread$n::thread$s$p$thread$$queue$n$$queue_thread_main$f$r$*const r$p1 = ::thread$n::thread$s$p$thread$$queue$n$$queue_thread_main$f$r$::allocate();
 try {
 new (r$p1) ::thread$n::thread$s$p$thread$$queue$n$$queue_thread_main$f$r$(a0$);
 } catch (...) {
 ::thread$n::thread$s$p$thread$$queue$n$$queue_thread_main$f$r$::deallocate(r$p1);
 throw;
 }
 pxcrt::rcptr< ::thread$n::thread$s$p$thread$$queue$n$$queue_thread_main$f$r$ > r$((r$p1));
 return r$;
 /* staticif-else end */
}
static inline pxcrt::rcptr< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > > make_tptr$f$p$thread$$queue$n$$task_queue_shared$s$q$m$ll$r$() {
 return ::pointer$n::box_pointer$f$p$pxcrt$$tptr$p$thread$$queue$n$$task_queue_shared$s$r$$q$m$ll$r$();
}
static inline pxcrt::rcptr< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > > box_pointer$f$p$pxcrt$$tptr$p$thread$$queue$n$$task_queue_shared$s$r$$q$m$ll$r$() {
 /* staticif empty *//* staticif-else */
 pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s >*const r$p1 = pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s >::allocate();
 try {
 new (&r$p1->value) ::thread$n::queue$n::task_queue_shared$s();
 new (&r$p1->count$z) pxcrt::mtcount(); /* nothrow */
 try {
 new (&r$p1->monitor$z) pxcrt::monitor();
 } catch (...) {
 typedef ::thread$n::queue$n::task_queue_shared$s thread$$queue$n$$task_queue_shared$s$dtor;
 r$p1->value.~thread$$queue$n$$task_queue_shared$s$dtor();
 throw;
 }
 } catch (...) {
 pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s >::deallocate(r$p1);
 throw;
 }
 pxcrt::rcptr< pxcrt::trcval< ::thread$n::queue$n::task_queue_shared$s > > r$((r$p1));
 return r$;
 /* staticif-else end */
}
}; /* namespace pointer */
namespace algebraic$n { 
void option$v$p$callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$r$::init$(const option$v$p$callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$r$ & x) {
 _$e = none$$e;
 switch (x.get_$e()) {
 case none$$e: /* unit */; break;
 case some$$e: new(some$$p()) ::callable$n::tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$(*(x.some$$p())); break;
 }
 _$e = x._$e;
};
void option$v$p$callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$r$::deinit$() {
 typedef pxcrt::bt_unit meta$n$$unit$t$dtor;
 typedef ::callable$n::tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$ callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$dtor;
 switch (get_$e()) {
 case none$$e: /* unit */; break;
 case some$$e: some$$p()->~callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$dtor(); break;
 }
};
pxcrt::bt_unit option$v$p$callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$r$::none$$r() const {
 if (get_$e() != none$$e) { pxcrt::throw_invalid_field(); }
 return pxcrt::unit_value;
}
::callable$n::tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$ option$v$p$callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$r$::some$$r() const {
 if (get_$e() != some$$e) { pxcrt::throw_invalid_field(); }
 return *some$$p();
}
pxcrt::bt_unit option$v$p$callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$r$::none$$l(pxcrt::bt_unit x) {
 { deinit$(); _$e = none$$e; /* unit */; _$e = none$$e; }
 return pxcrt::unit_value;
}
::callable$n::tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$ option$v$p$callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$r$::some$$l(::callable$n::tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$ x) {
 { deinit$(); _$e = none$$e; new(some$$p()) ::callable$n::tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$(x); _$e = some$$e; }
 return (*some$$p());
}
}; /* namespace algebraic */
namespace operator$n { 
static inline ::callable$n::tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$ union_field$f$p$some$ls$q$algebraic$n$$option$v$p$callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$r$$r$(const ::algebraic$n::option$v$p$callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$r$& x$) {
 if (!(x$.get_$e() == ::algebraic$n::option$v$p$callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$r$::some$$e)) {
  {
   if ((x$.get_$e() == ::algebraic$n::option$v$p$callable$n$$tcallable_ptr$s$p$meta$n$$void$t$q$m$ll$r$$r$::none$$e)) {
    PXC_THROW(::exception$n::unexpected_value_template$s$p$unit$ls$r$((::pxcrt::pxcvarray< pxcrt::bt_uchar >::guard_ref< const ::pxcrt::pxcvarray< pxcrt::bt_uchar > > (::text$n::string$n::serialize$n::to_string$f$p$meta$n$$unit$t$r$(pxcrt::bt_unit((x$.none$$r())))).get_crange())));
   }
  }
  /* staticif empty */
 }
 return (x$.some$$r());
}
}; /* namespace operator */
namespace exception$n { 
unexpected_value_template$s$p$unit$ls$r$::unexpected_value_template$s$p$unit$ls$r$(const ::pxcrt::bt_cslice< pxcrt::bt_uchar >& m$) : count$z(1), msg$(::pxcrt::pxcvarray< pxcrt::bt_uchar >(m$)) {
}
inline ::pxcrt::pxcvarray< pxcrt::bt_uchar > unexpected_value_template$s$p$unit$ls$r$::message() const {
 ::pxcrt::pxcvarray< pxcrt::bt_uchar > s$ = ::pxcrt::pxcvarray< pxcrt::bt_uchar >(pxcrt::bt_strlit("unexpected_value{unit}("));
 ::pxcrt::array_append< ::pxcrt::pxcvarray< pxcrt::bt_uchar >,::pxcrt::pxcvarray< pxcrt::bt_uchar > >(s$ , msg$);
 ::pxcrt::array_append< ::pxcrt::pxcvarray< pxcrt::bt_uchar >,::pxcrt::bt_strlit >(s$ , pxcrt::bt_strlit(")"));
 return s$;
}
}; /* namespace exception */
namespace text$n { namespace string$n { namespace serialize$n { 
static inline ::pxcrt::pxcvarray< pxcrt::bt_uchar > to_string$f$p$meta$n$$unit$t$r$(const pxcrt::bt_unit& x$) {
 /* staticif empty *//* staticif-else */
 ::pxcrt::pxcvarray< pxcrt::bt_uchar > s$ = ::pxcrt::pxcvarray< pxcrt::bt_uchar >();
 ::text$n::string$n::serialize$n::serialize_to_string$f$p$meta$n$$unit$t$r$(x$ , s$);
 return s$;
 /* staticif-else end */
}
static inline void serialize_to_string$f$p$meta$n$$unit$t$r$(const pxcrt::bt_unit& x$, ::pxcrt::pxcvarray< pxcrt::bt_uchar >& out$) {
 ::text$n::serialize$n::serialize$f$p$text$$string$$serialize$n$$ser_default$s$q$meta$n$$unit$t$r$(x$ , out$);
}
};};}; /* namespace text::string::serialize */
namespace text$n { namespace serialize$n { 
static inline void serialize$f$p$text$$string$$serialize$n$$ser_default$s$q$meta$n$$unit$t$r$(const pxcrt::bt_unit& x$, ::pxcrt::pxcvarray< pxcrt::bt_uchar >& o$) {
 /* staticif */
 ::text$n::string$n::serialize$n::unit_to_string$f(x$ , o$);
 /* staticif end */
}
};}; /* namespace text::serialize */
namespace io$n { namespace standard$n { 
static inline void println$f$p$m$ll$p$m$ll$p$container$$array$n$$strlit$s$q$0$li$r$$r$$r$(const ::pxcrt::bt_strlit& a0$) {
 ::pxcrt::pxcvarray< pxcrt::bt_uchar > s$ = ::text$n::string$n::split$n::string_join$f$p$_20$ls$q$m$ll$p$m$ll$p$container$$array$n$$strlit$s$q$0$li$r$$r$$r$(a0$);
 ::pxcrt::array_append< ::pxcrt::pxcvarray< pxcrt::bt_uchar >,::pxcrt::bt_strlit >(s$ , pxcrt::bt_strlit("\012"));
 ::io$n::file$n::file_write_all$f(pxcrt::io_stdout(pxcrt::io_system) , (::pxcrt::pxcvarray< pxcrt::bt_uchar >::guard_ref< const ::pxcrt::pxcvarray< pxcrt::bt_uchar > > (s$).get_crange()));
}
};}; /* namespace io::standard */
namespace text$n { namespace string$n { namespace split$n { 
static inline ::pxcrt::pxcvarray< pxcrt::bt_uchar > string_join$f$p$_20$ls$q$m$ll$p$m$ll$p$container$$array$n$$strlit$s$q$0$li$r$$r$$r$(const ::pxcrt::bt_strlit& a0$) {
 ::pxcrt::pxcvarray< pxcrt::bt_uchar > s$ = ::pxcrt::pxcvarray< pxcrt::bt_uchar >();
 {
  /* staticif empty */
  {
   ::pxcrt::array_append< ::pxcrt::pxcvarray< pxcrt::bt_uchar >,::pxcrt::bt_strlit >(s$ , a0$);
  }
 }
 return s$;
}
};};}; /* namespace text::string::split */
/* package main */
namespace text$n { namespace string$n { namespace split$n { 
};};}; /* namespace text::string::split */
namespace t1$n { 
void _t1$$nsmain()
{
 {
  ::io$n::standard$n::println$f$p$m$ll$p$m$ll$p$container$$array$n$$strlit$s$q$0$li$r$$r$$r$(pxcrt::bt_strlit("hello"));
 }
}
}; /* namespace t1 */
/* package main c */
static int i$_t1$$nsmain$init = 0;
extern "C" {
void _t1$$nsmain$c()
{
 if (i$_t1$$nsmain$init == 0) {
  ::t1$n::_t1$$nsmain();
  i$_t1$$nsmain$init = 1;
 }
}
int main(int argc, char **argv)
{
 pxcrt::process_argv = argv;
 i$_t1$$nsmain$init = 0;
 return pxcrt::main_nothrow(& _t1$$nsmain$c);
}
}; /* extern "C" */
#endif /* PXC_IMPORT_HEADER */
