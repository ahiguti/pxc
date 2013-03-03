/* type definitions */
/* inline */
#include <stdexcept>
#include <stdint.h>
namespace pxcrt {
struct bt_unit { };
extern bt_unit unit_value; /* used when a reference is required */
typedef bool bt_bool;
typedef ::uint8_t bt_uchar;
typedef ::int8_t bt_char;
typedef ::uint16_t bt_ushort;
typedef ::int16_t bt_short;
typedef ::uint32_t bt_uint;
typedef ::int32_t bt_int;
typedef ::uint64_t bt_ulong;
typedef ::int64_t bt_long;
typedef ::size_t bt_size_t;
typedef float bt_float;
typedef double bt_double;
struct bt_tpdummy;
int main_nothrow(void (*main_f)(void));
}; // namespace pxcrt
namespace $n {
}; /* namespace */
/* inline c hdr */
/* function prototype decls */
namespace $n {
}; /* namespace */
/* inline c */
/* inline */
#include <string>
#include <unistd.h>
namespace pxcrt {
bt_unit unit_value;
int main_nothrow(void (*main_f)(void))
{
  try {
    (*main_f)();
  } catch (const std::exception& e) {
    std::string mess(e.what());
    if (!mess.empty() && mess[mess.size() - 1] != '\n') {
      mess += "\n";
    }
    ::write(2, mess.data(), mess.size());
    return 1;
  } catch (...) {
    const std::string mess = "unknown exception\n";
    ::write(2, mess.data(), mess.size());
    return 1;
  }
  return 0;
}
}; // namespace pxcrt
namespace $n {
/* global variables */
/* function definitions */
}; /* namespace */
/* package main */
namespace $n {
}; /* namespace */
namespace numeric$n { namespace impl$n { namespace integral$n { 
void _numeric$$impl$$integral$$nsmain()
{
}
};};}; /* namespace numeric::impl::integral */
/* package main c */
static int i$_numeric$$impl$$integral$$nsmain$init = 0;
extern "C" {
void _numeric$$impl$$integral$$nsmain$c()
{
 if (i$_numeric$$impl$$integral$$nsmain$init == 0) {
  ::numeric$n::impl$n::integral$n::_numeric$$impl$$integral$$nsmain();
  i$_numeric$$impl$$integral$$nsmain$init = 1;
 }
}
}; /* extern "C" */
