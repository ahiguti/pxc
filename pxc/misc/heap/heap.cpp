#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#define LEN 13107000

template <size_t size> struct tl_allocator {
  static void *allocate() {
    if (free_list != 0) {
      void *r = free_list;
      free_list = *(void **)free_list;
      return r;
    }
    extend();
    void *r = free_list;
    free_list = *(void **)free_list;
    return r;
  }
  static void deallocate(void *p) {
    *(void **)p = free_list;
    free_list = p;
  }
private:
  static void extend() {
    char *b = (char *)malloc(sizeof(void *) + block_size);
    *(void **)(b) = blocks;
    blocks = b;
    b += sizeof(void *);
    size_t const bsz = block_size / size;
    for (size_t i = 0; i < bsz; ++i) {
      deallocate(b + i * size);
    }
  }
private:
  enum { block_size = 1024 * 127 };
  static __thread void *blocks;
  static __thread void *free_list;
};
template <size_t size> __thread void *tl_allocator<size>::blocks = 0;
template <size_t size> __thread void *tl_allocator<size>::free_list = 0;

void *ptr[LEN];


int main(int argc, char **argv)
{
  int c = argc > 1 ? atoi(argv[1]) : 0;
  if (c == 0) {
    for (int j = 0; j < 20; ++j) {
      for (int i = 0; i < LEN; ++i) {
        ptr[i] = malloc(32);
      }
      for (int i = 0; i < LEN; ++i) {
        free(ptr[i]);
      }
    }
  } else if (c == 1) {
    for (int j = 0; j < 20; ++j) {
      for (int i = 0; i < LEN; ++i) {
        ptr[i] = new char[32];
      }
      for (int i = 0; i < LEN; ++i) {
        char *p = (char *)ptr[i];
        delete [] p;
      }
    }
  } else {
    tl_allocator<32> a;
    for (int j = 0; j < 20; ++j) {
      for (int i = 0; i < LEN; ++i) {
        ptr[i] = a.allocate();
      }
      for (int i = 0; i < LEN; ++i) {
        a.deallocate(ptr[i]);
      }
    }
  }
}

