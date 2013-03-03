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
/* inline */
#include <stdexcept>
namespace pxcrt {
void throw_invalid_index() __attribute__((noreturn));
void throw_null_dereference() __attribute__((noreturn));
void throw_invalid_field() __attribute__((noreturn));
void throw_would_invalidate() __attribute__((noreturn));
void throw_virtual_function_call() __attribute__((noreturn));
void throw_bad_alloc() __attribute__((noreturn));
struct invalid_index : public std::logic_error
  { invalid_index(); };
struct invalid_field : public std::logic_error
  { invalid_field(); };
struct null_dereference : public std::logic_error
  { null_dereference(); };
struct would_invalidate : public std::logic_error
  { would_invalidate(); };
struct virtual_function_call : public std::logic_error
  { virtual_function_call(); };
struct bad_alloc : public std::bad_alloc
  { bad_alloc(); };
}; // namespace pxcrt
namespace $n {
}; /* namespace */
/* inline c hdr */
/* function prototype decls */
namespace $n {
}; /* namespace */
extern "C" void _exception$$impl$$builtin$$nsmain$c();
namespace $n {
}; /* namespace */
extern "C" void _numeric$$impl$$integral$$nsmain$c();
namespace $n {
}; /* namespace */
extern "C" void _exception$$builtin$$nsmain$c();
namespace $n {
}; /* namespace */
/* inline c */
namespace $n {
/* global variables */
/* function definitions */
}; /* namespace */
/* package main */
namespace $n {
}; /* namespace */
namespace numeric$n { namespace integral$n { 
void _numeric$$integral$$nsmain()
{
}
};}; /* namespace numeric::integral */
/* package main c */
static int i$_numeric$$integral$$nsmain$init = 0;
extern "C" {
void _numeric$$integral$$nsmain$c()
{
 if (i$_numeric$$integral$$nsmain$init == 0) {
  _exception$$impl$$builtin$$nsmain$c();
  _numeric$$impl$$integral$$nsmain$c();
  _exception$$builtin$$nsmain$c();
  ::numeric$n::integral$n::_numeric$$integral$$nsmain();
  i$_numeric$$integral$$nsmain$init = 1;
 }
}
}; /* extern "C" */
