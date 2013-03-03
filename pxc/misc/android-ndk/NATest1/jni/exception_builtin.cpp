/* type definitions */
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
/* inline c */
namespace $n {
/* global variables */
/* function definitions */
}; /* namespace */
/* package main */
namespace $n {
}; /* namespace */
namespace exception$n { namespace builtin$n { 
void _exception$$builtin$$nsmain()
{
}
};}; /* namespace exception::builtin */
/* package main c */
static int i$_exception$$builtin$$nsmain$init = 0;
extern "C" {
void _exception$$builtin$$nsmain$c()
{
 if (i$_exception$$builtin$$nsmain$init == 0) {
  _exception$$impl$$builtin$$nsmain$c();
  ::exception$n::builtin$n::_exception$$builtin$$nsmain();
  i$_exception$$builtin$$nsmain$init = 1;
 }
}
}; /* extern "C" */
