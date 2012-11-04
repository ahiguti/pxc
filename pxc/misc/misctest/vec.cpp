
#include <vector>
#include <string>
#include <stdlib.h>
#include <stdio.h>
// #include <type_traits>

template <typename T> struct myvector { /* unsafe for non-pod types */
  static_assert(std::is_pod<T>::value, "T must be a pod type"); // c++0x
  myvector() : start(0), valid_len(0), alloc_len(0) { }
  ~myvector() { clear(); }
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
      const size_t nl_n = (nl >= 10) ? (nl * 2) : 10;
      if (nl_n <= nl || nl_n > max_len) {
	throw std::bad_alloc(); /* overflow */
      }
      nl = nl_n;
    } while (nl < len);
    void *const p = realloc(start, nl);
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
  myvector(const myvector& x);
  myvector& operator =(const myvector& x);
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
  int test_param1 = 0;
  int test_param2 = 0;
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
  unsigned long long r = 0;
  switch (test_num) {
  case 0:
    printf("vector\n");
    for (size_t i = 0; i < 1000000; ++i) {
      r += test_vector< std::vector<unsigned char> >(test_param1,
	(bool)test_param2);
    }
    break;
  case 1:
    printf("std::string\n");
    for (size_t i = 0; i < 1000000; ++i) {
      r += test_vector< std::string >(test_param1,
	(bool)test_param2);
    }
    break;
  case 2:
    printf("myvector\n");
    for (size_t i = 0; i < 1000000; ++i) {
      r += test_vector< myvector<unsigned char> >(test_param1,
	(bool)test_param2);
    }
    break;
  case 3:
    printf("vector_chk\n");
    for (size_t i = 0; i < 1000000; ++i) {
      r += test_vector_chk< std::vector<unsigned char> >(test_param1,
	(bool)test_param2);
    }
    break;
  default:
    break;
  }
  printf("%llu\n", r);
}

