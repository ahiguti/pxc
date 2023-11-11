#include <string>

using namespace std;

struct foo {
  enum { intval_e, strval_e } e;
  // typedef int __attribute__((may_alias)) int_ali;
  // typedef string __attribute__((may_alias)) string_ali;
  union {
    char intval[sizeof(int)];
    char strval[sizeof(string)];
  } u;
  foo() { init(); }
  ~foo() { deinit(); }
  foo(const foo& x) { init(x); }
  foo& operator =(const foo& x) {
    if (this != &x) { deinit(); init(x); }
    return *this;
  }
  void init() {
    #if 0
    typedef int __attribute__((may_alias)) int_ali;
    e = intval_e;
    int_ali *p = (int_ali *)u.intval;
    *p = 0;
    #endif
    #if 0
    typedef int __attribute__((may_alias)) int_ali;
    int_ali * p = (int_ali *)u.intval;
    *p = 0;
    #endif
    string *p = (string *)u.strval;
    *p = std::string("abc");
  }
  void init(const foo& x) {
        int * p0 = (int *)u.intval;
        *p0 = 0;
    switch (x.e) {
    case intval_e:
      {
        // int *__attribute__((may_alias)) p0 = (int *)u.intval;
        // int *__attribute__((may_alias)) p1 = (int *)x.u.intval;
        int * p0 = (int *)u.intval;
        int * p1 = (int *)x.u.intval;
        *p0 = *p1;
        *p0 = 0;
      }
      break;
    case strval_e:
      {
        // string *__attribute__((may_alias)) p0 = (string *)u.strval;
        // string *__attribute__((may_alias)) p1 = (string *)x.u.strval;
        string * p0 = (string *)u.strval;
        string * p1 = (string *)x.u.strval;
        new (p0) string(*p1);
      }
      break;
    }
  }
  void deinit() {
    switch (e) {
    case intval_e:
      {
        typedef int xt;
        // int *__attribute__((may_alias)) p0 = (int *)u.intval;
        int *p0 = (int *)u.intval;
        p0->xt::~xt();
      }
      break;
    case strval_e:
      {
        // string *__attribute__((may_alias)) p0 = (string *)u.strval;
        string * p0 = (string *)u.strval;
        p0->string::~string();
      }
      break;
    }
  }
};

int main()
{
  foo f;
  foo g;
  f = g;
}

