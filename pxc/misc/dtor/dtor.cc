
#include <stdio.h>
#include <new>
#include <vector>

namespace hoge {

struct foo {
  foo() : x(0) { }
  foo(int _0) : x(_0) { }
  ~foo() { fprintf(stderr, "x=%d\n", x); }
  int x;
};

typedef foo bar;

};

using namespace std;
using namespace hoge;

int main()
{
#if 0
  vector<bar> v;
  v.push_back(bar(3));
#endif
#if 0
  char buf[sizeof(bar)];
  bar *p = reinterpret_cast<bar *>(buf);
  new (p) bar(10);
  p->x = 123;
  p->bar::~bar(); // clang++ fails to compile
#endif
  char buf[sizeof(bar)];
  hoge::bar *p = reinterpret_cast<bar *>(buf);
  new (p) bar(10);
  p->x = 123;
  typedef ::hoge::bar tt;
  // virtual call
  p->~tt(); // clang++ compiles
}

