
#include <stdexcept>
#include <memory>
#include <stdlib.h>
#include <assert.h>

template <typename Tk, typename Tm>
struct hashmap_entry {
  Tk first;
  Tm second;
  hashmap_entry(const Tk& k, const Tm& m) : first(k), second(m) { }
};

template <typename Tk, typename Tm>
struct hashmap_node {
  typedef hashmap_entry<Tk, Tm> entry_type;
  hashmap_node *next;
  entry_type entry;
  hashmap_node(hashmap_node *n, const Tk& k, const Tm& m)
    : next(n), entry(k, m) { }
};

template <typename Tk, typename Tm>
struct hashmap_buckets {
  typedef hashmap_node<Tk, Tm> node_type;
  typedef hashmap_entry<Tk, Tm> entry_type;
  typedef size_t size_type;
  hashmap_buckets(size_type ini_num_buckets) {
    num_entries = 0;
    num_buckets = ini_num_buckets;
    buckets = static_cast<node_type **>(
      malloc(num_buckets * sizeof(node_type *)));
    if (buckets == 0) {
      throw std::bad_alloc();
    }
    for (size_type i = 0; i < num_buckets; ++i) {
      buckets[i] = 0;
    }
  }
  ~hashmap_buckets() {
    for (size_type i = 0; i < num_buckets; ++i) {
      node_type *p = buckets[i];
      while (p != 0) {
        node_type *np = p->next;
        delete p;
        p = np;
      }
    }
    free(buckets);
  }
  bool insert(const Tk& k, const Tm& m, size_type bkt) {
    assert(bkt < num_buckets);
    node_type *cur = buckets[bkt];
    for (node_type *p = cur; p != 0; p = p->next) {
      entry_type& pe = p->entry;
      if (pe.first == k) {
        pe.second = m;
        return false; /* not created */
      }
    }
    /* append to the end of the buket */
    node_type *const nn = new node_type(cur, k, m);
    buckets[bkt] = nn;
    ++num_entries;
    return true; /* created */
  }
  const Tm *find(const Tk& k, size_type bkt) const {
    return find_internal(k, bkt);
  }
  Tm *find(const Tk& k, size_type bkt) {
    return find_internal(k, bkt);
  }
private:
  Tm *find_internal(const Tk& k, size_type bkt) const {
    assert(bkt < num_buckets);
    for (node_type *p = buckets[bkt]; p != 0; p = p->next) {
      entry_type& pe = p->entry;
      if (pe.first == k) {
        return &pe.second;
      }
    }
    return 0;
  }
  hashmap_buckets(const hashmap_buckets& x);
  hashmap_buckets& operator =(const hashmap_buckets& x);
private:
  node_type **buckets;
  size_type num_buckets;
  size_type num_entries;
};

#include <stdio.h>

typedef hashmap_buckets<int, int> map_type;

static int test1(const map_type& m, int cnt, int v) {
  for (int i = 0; i < cnt; ++i) {
    int k = i % 100;
    const int *p = m.find(k, k);
    if (p != 0) {
      v += *p;
    }
  }
  return v;
}
int main()
{
  map_type m(211);
  for (int i = 0; i < 100; ++i) {
    m.insert(i, i, i);
  }
  long t1 = time(0);
  int v = 0;
  for (int i = 0; i < 300; ++i) {
    v = test1(m, 1000000, v);
  }
  long t2 = time(0);
  printf("%d %d\n", v, (int)(t2 - t1));
}

