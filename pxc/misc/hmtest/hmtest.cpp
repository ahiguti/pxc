
#include <time.h>
#include <tr1/unordered_map>
#include <map>
#include <stdio.h>
// #include <sparsehash/dense_hash_map>
// #include <sparsehash/sparse_hash_map>
#include <boost/unordered_map.hpp>

struct hash_int {
  inline size_t operator()(int x) const {
    return x;
  }
};

// typedef google::dense_hash_map<int, int> map_type;
// typedef google::sparse_hash_map<int, int> map_type;
typedef std::tr1::unordered_map<int, int, hash_int> map_type;
// typedef boost::unordered_map<int, int> map_type;
// typedef std::map<int, int> map_type;

static int test1(const map_type& m, int cnt, int v) {
  for (int i = 0; i < cnt; ++i) {
    map_type::const_iterator iter = m.find(i % 100);
    if (iter != m.end()) {
      v += iter->second;
    }
  }
  return v;
}
int main()
{
  map_type m;
  // m.set_empty_key(-1);
  for (int i = 0; i < 100; ++i) {
    m.insert(std::make_pair(i, i));
  }
  long t1 = time(0);
  int v = 0;
  for (int i = 0; i < 300; ++i) {
    v = test1(m, 1000000, v);
  }
  long t2 = time(0);
  printf("%d %d\n", v, (int)(t2 - t1));
}

