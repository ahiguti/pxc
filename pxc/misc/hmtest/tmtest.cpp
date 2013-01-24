
#include <time.h>
// #include <unordered_map>
#include <map>
#include <stdio.h>

// typedef std::unordered_map<int, int> map_type;
typedef std::map<int, int> map_type;

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

