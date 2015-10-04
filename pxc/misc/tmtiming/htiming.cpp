
#include <time.h>
// #include <unordered_map>
#include <map>
#include <vector>
#include <string>
#include <stdio.h>
#include <boost/unordered_map.hpp>

// typedef std::unordered_map<int, int> map_type;
// typedef std::map<std::string, std::string> map_type;
typedef boost::unordered_map<std::string, std::string> map_type;

size_t test_size = 10000000;
size_t loop = 1;

std::vector<std::string> aval;
map_type mval;

static long test1(long v) {
  long x = 1;
  for (size_t i = 0; i < test_size; ++i) {
    x = (x * 1664525 + 1013904223) & 0xffffffffL;
    const std::string k = aval[(int)(x % test_size)];
    const std::string m = mval[k];
    v += m.size();
  }
  return v;
}

int main()
{
  aval.resize(test_size);
  for (size_t i = 0; i < test_size; ++i) {
    char buf[32];
    snprintf(buf, 32, "%d", (int)i);
    std::string s(buf);
    aval[i] = s;
    std::string ms = s + "val";
    mval[s] = ms;
  }
  long t1 = time(0);
  long v = 0;
  for (size_t i = 0; i < loop; ++i) {
    v = test1(v);
  }
  long t2 = time(0);
  printf("%ld %d\n", v, (int)(t2 - t1));
}

