
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

typedef size_t (*modulo_func)(size_t);

struct hashmap_buckets_hdr {
  size_t num_buckets;
  size_t num_entries;
  modulo_func buckets_modf;
};

template <typename Tk, typename Tm, typename Teq, typename Thash>
struct hashmap_buckets {
  typedef hashmap_node<Tk, Tm> node_type;
  typedef hashmap_entry<Tk, Tm> entry_type;
  typedef size_t size_type;
  hashmap_buckets(size_type ini_num_buckets, modulo_func mf) {
    hdr = static_cast<hashmap_buckets_hdr *>(malloc(
      sizeof(hashmap_buckets_hdr) +
      ini_num_buckets * sizeof(node_type *)));
      /* FIXME: check overflow */
    if (hdr == 0) {
      throw std::bad_alloc();
    }
    hashmap_buckets_hdr& h = *hdr;
    node_type **const buckets = get_buckets();
    h.num_entries = 0;
    h.num_buckets = ini_num_buckets;
    h.buckets_modf = mf;
    for (size_type i = 0; i < ini_num_buckets; ++i) {
      buckets[i] = 0;
    }
  }
  ~hashmap_buckets() {
    hashmap_buckets_hdr& h = *hdr;
    node_type **const buckets = get_buckets();
    for (size_type i = 0; i < h.num_buckets; ++i) {
      node_type *p = buckets[i];
      while (p != 0) {
        node_type *np = p->next;
        delete p;
        p = np;
      }
    }
    free(hdr);
  }
  bool insert(const Tk& k, const Tm& m, size_type bkt) {
    hashmap_buckets_hdr& h = *hdr;
    node_type **const buckets = get_buckets();
    assert(bkt < h.num_buckets);
    node_type *cur = buckets[bkt];
    for (node_type *p = cur; p != 0; p = p->next) {
      entry_type& pe = p->entry;
      if (eq(pe.first, k)) {
        pe.second = m;
        return false; /* not created */
      }
    }
    /* append to the end of the buket */
    node_type *const nn = new node_type(cur, k, m);
    buckets[bkt] = nn;
    ++h.num_entries;
    return true; /* created */
  }
  const Tm *find(const Tk& k, size_type bkt) const {
    return find_internal(k, bkt);
  }
  Tm *find(const Tk& k, size_type bkt) {
    return find_internal(k, bkt);
  }
  const Tm *find(const Tk& k) const {
    size_t bkt = hash(k);
    #if 0
    if (bkt >= hdr->num_buckets) {
      bkt %= hdr->num_buckets;
    }
    #endif
    #if 0
    if (bkt >= 211) {
      bkt %= 211;
    }
    #endif
    #if 1
    bkt = hdr->buckets_modf(bkt);
    #endif
    // bkt %= hdr->num_buckets;
    return find_internal(k, bkt);
  }
  Tm *find(const Tk& k) {
    size_t bkt = hash(k);
    #if 0
    if (bkt >= hdr->num_buckets) {
      bkt %= hdr->num_buckets;
    }
    #endif
    #if 0
    if (bkt >= 211) {
      bkt %= 211;
    }
    #endif
    #if 1
    bkt = hdr->buckets_modf(bkt);
    #endif
    // bkt %= hdr->num_buckets;
    return find_internal(k, bkt);
  }
private:
  Tm *find_internal(const Tk& k, size_type bkt) const {
    node_type **const buckets = get_buckets();
    assert(bkt < h.num_buckets);
    for (node_type *p = buckets[bkt]; p != 0; p = p->next) {
      entry_type& pe = p->entry;
      if (eq(pe.first, k)) {
        return &pe.second;
      }
    }
    return 0;
  }
  node_type **get_buckets() const {
    return reinterpret_cast<node_type **>(
      reinterpret_cast<char *>(hdr) + sizeof(hashmap_buckets_hdr));
  }
  hashmap_buckets(const hashmap_buckets& x);
  hashmap_buckets& operator =(const hashmap_buckets& x);
private:
  hashmap_buckets_hdr *hdr;
  Teq eq;
  Thash hash;
};

#include <stdio.h>

struct hash_fo {
  size_t operator ()(int x) const { return x; }
};

typedef hashmap_buckets<int, int, std::equal_to<int>, hash_fo> map_type;

int test1(const map_type& m, int cnt, int v) {
  int k = 0;
  for (int i = 0; i < cnt; ++i) {
    // int k = i % 100;
    // const int *p = m.find(k, k);
    const int *p = m.find(k);
    if (p != 0) {
      v += *p;
    }
    if (++k >= 100) k = 0;
  }
  return v;
}

static size_t mod211(size_t x)
{
  return x % 211;
}

int main()
{
  // map_type m(211);
  map_type m(211, mod211);
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

