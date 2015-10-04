
#include <string>
#include <map>
#include <stdlib.h>
#include <stdio.h>


int main(int argc, char **argv)
{
  typedef std::map<std::string, std::string> mtype;
  mtype m;
  m["foo"] = "abc"; // -fmudflap dies
  m["bar"] = "xyz";
  m["baz"] = "hoge";
  for (mtype::const_iterator i = m.begin(); i != m.end(); ++i) {
    printf("%s\n", i->first.c_str());
    printf("%s\n", i->second.c_str());
  }
  return 0;
}

