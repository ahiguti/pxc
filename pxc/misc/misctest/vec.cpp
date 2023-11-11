
#include <vector>
#include <string>
#include <stdlib.h>
#include <stdio.h>
// #include <type_traits>

template <typename T> struct podvector { /* unsafe for non-pod types */
  static_assert(std::is_pod<T>::value,
    "T must be a trivially-copyable type"); // c++0x
  podvector() : start(0), valid_len(0), alloc_len(0) { }
  ~podvector() { clear(); }
  void clear() {
    free(start);
    start = 0;
    valid_len = alloc_len = 0;
  }
  size_t size() const { return valid_len; }
  void push_back(const T& v) {
    reserve(valid_len + 1);
    start[valid_len] = v;
    ++valid_len;
  }
  void reserve(size_t len) {
    if (len <= alloc_len) {
      return;
    }
    size_t nl = alloc_len;
    do {
      const size_t nl_n = (nl >= 1) ? (nl * 2) : 1;
      if (nl_n <= nl || nl_n > max_len) {
        throw std::bad_alloc(); /* overflow */
      }
      nl = nl_n;
    } while (nl < len);
    void *const p = realloc(start, nl * sizeof(T));
    if (p == 0) {
      throw std::bad_alloc();
    }
    start = static_cast<T *>(p);
    alloc_len = nl;
  }
  T& operator [](size_t idx) { return start[idx]; }
  const T& operator [](size_t idx) const { return start[idx]; }
private:
  enum {
    max_len = ((size_t)-1) / sizeof(T)
  };
  podvector(const podvector& x);
  podvector& operator =(const podvector& x);
  T *start;
  size_t valid_len;
  size_t alloc_len;
};

template <typename T> struct myvector {
  myvector() : start(0), valid_len(0), alloc_len(0) { }
  ~myvector() { deinit(); }
  myvector(const myvector& x) { init(x); }
  myvector& operator =(const myvector& x) {
    if (&x != this) {
      deinit();
      init(x);
    }
    return *this;
  }
  void clear() {
    if (t_is_trivially_copyable) {
      valid_len = 0;
    } else {
      while (valid_len > 0) {
        (start + valid_len - 1)->T::~T(); /* destructor */
        --valid_len;
      }
    }
  }
  size_t size() const { return valid_len; }
  void push_back(const T& v) {
    reserve(valid_len + 1);
    if (t_is_trivially_copyable) {
      start[valid_len] = v;
    } else {
      new (start + valid_len) T(v); /* copy constructor */
    }
    ++valid_len;
  }
  void reserve(size_t len) {
    if (len <= alloc_len) {
      return;
    }
    size_t nl = alloc_len;
    do {
      const size_t nl_n = (nl >= 1) ? (nl * 2) : 1;
      if (nl_n <= nl || nl_n > max_len) {
        throw std::bad_alloc(); /* overflow */
      }
      nl = nl_n;
    } while (nl < len);
    T *p = 0;
    if (t_is_trivially_copyable) {
      p = static_cast<T *>(realloc(start, nl * sizeof(T)));
      if (p == 0) {
        throw std::bad_alloc();
      }
    } else {
      p = static_cast<T *>(malloc(nl * sizeof(T)));
      if (p == 0) {
        throw std::bad_alloc();
      }
      size_t idx = 0;
      try {
        for (; idx < valid_len; ++idx) {
          new (p + idx) T(start[idx]); /* copy constructor */
        }
      } catch (...) {
        while (idx > 0) {
          (start + idx - 1)->T::~T(); /* destructor */
          --idx;
        }
        free(p);
        throw;
      }
      idx = valid_len;
      while (idx > 0) {
        (start + idx - 1)->T::~T(); /* destructor */
        --idx;
      }
      free(start);
    }
    start = p;
    alloc_len = nl;
  }
  T& operator [](size_t idx) { return start[idx]; }
  const T& operator [](size_t idx) const { return start[idx]; }
  void swap(myvector& x) {
    swap(x.start, start);
    swap(x.valid_len, valid_len);
    swap(x.alloc_len, alloc_len);
  }
private:
  enum {
    t_is_trivially_copyable = false, // std::is_pod<T>::value,
    max_len = ((size_t)-1) / sizeof(T),
  };
  void deinit() {
    clear();
    free(start);
  }
  void init(const myvector& x) {
    start = static_cast<T *>(malloc(x.valid_len * sizeof(T)));
    if (start == 0) {
      throw std::bad_alloc();
    }
    alloc_len = x.alloc_len;
    if (t_is_trivially_copyable) {
      memcpy(start, x.start, x.valid_len * sizeof(T));
      valid_len = x.valid_len;
    } else {
      size_t idx = 0;
      try {
        while (idx < x.valid_len) {
          new (start + idx) T(x.start[idx]); /* constructor */
          ++idx;
        }
      } catch (...) {
        while (idx > 0) {
          (start + idx - 1)->T::~T(); /* destructor */
          --idx;
        }
        free(start);
        throw;
      }
      valid_len = x.valid_len;
    }
  }
private:
  T *start;
  size_t valid_len;
  size_t alloc_len;
};

template <typename T> unsigned long long test_vector(size_t len,
  bool reserve_first)
{
  if (len == 0) {
    len = 1000;
  }
  T v;
  if (reserve_first) {
    v.reserve(reserve_first);
  }
  for (size_t i = 0; i < len; ++i) {
    unsigned char c = i;
    v.push_back(c);
  }
  unsigned long long r = 0;
  for (size_t i = 0; i < len; ++i) {
    r += v[i];
  }
  return r;
}

template <typename T> unsigned long long test_vector_chk(size_t len,
  bool reserve_first)
{
  if (len == 0) {
    len = 1000;
  }
  T v;
  if (reserve_first) {
    v.reserve(reserve_first);
  }
  for (size_t i = 0; i < len; ++i) {
    unsigned char c = i;
    v.push_back(c);
  }
  unsigned long long r = 0;
  for (size_t i = 0; i < len; ++i) {
    // if (!__builtin_expect(i < v.size(), true)) { throw std::exception(); }
    // r += v[i];
    r += v.at(i);
  }
  return r;
}

int main(int argc, char **argv)
{
  int test_num = 0;
  int test_param1 = 1000000;
  int test_param2 = 1000;
  int test_param3 = 0;
  if (argc > 1) {
    test_num = atoi(argv[1]);
    printf("test_num = %d\n", test_num);
  }
  if (argc > 2) {
    test_param1 = atoi(argv[2]);
    printf("test_param1 = %d\n", test_param1);
  }
  if (argc > 3) {
    test_param2 = atoi(argv[3]);
    printf("test_param2 = %d\n", test_param2);
  }
  if (argc > 4) {
    test_param3 = atoi(argv[4]);
    printf("test_param3 = %d\n", test_param3);
  }
  unsigned long long r = 0;
  switch (test_num) {
  case 0:
    printf("vector\n");
    for (size_t i = 0; i < test_param1; ++i) {
      r += test_vector< std::vector<unsigned char> >(test_param2,
        (bool)test_param3);
    }
    break;
  case 1:
    printf("std::string\n");
    for (size_t i = 0; i < test_param1; ++i) {
      r += test_vector< std::string >(test_param2,
        (bool)test_param3);
    }
    break;
  case 2:
    printf("podvector\n");
    for (size_t i = 0; i < test_param1; ++i) {
      r += test_vector< podvector<unsigned char> >(test_param2,
        (bool)test_param3);
    }
    break;
  case 3:
    printf("myvector\n");
    for (size_t i = 0; i < test_param1; ++i) {
      r += test_vector< myvector<unsigned char> >(test_param2,
        (bool)test_param3);
    }
    break;
  case 4:
    printf("vector_chk\n");
    for (size_t i = 0; i < test_param1; ++i) {
      r += test_vector_chk< std::vector<unsigned char> >(test_param2,
        (bool)test_param3);
    }
    break;
  default:
    break;
  }
  printf("%llu\n", r);
}

