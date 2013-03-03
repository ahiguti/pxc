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
/* inline c */
/* inline */
namespace pxcrt {
invalid_index::invalid_index() : std::logic_error("invalid_index") { }
invalid_field::invalid_field() : std::logic_error("invalid_field") { }
null_dereference::null_dereference() : std::logic_error("null_dereference") { }
would_invalidate::would_invalidate() : std::logic_error("would_invalidate") { }
bad_alloc::bad_alloc() : std::bad_alloc() { }
virtual_function_call::virtual_function_call()
  : std::logic_error("virtual_function_call") { }
void throw_invalid_index() { throw invalid_index(); }
void throw_invalid_field() { throw invalid_field(); }
void throw_null_dereference() { throw null_dereference(); }
void throw_would_invalidate() { throw would_invalidate(); }
void throw_virtual_function_call() { throw virtual_function_call(); }
void throw_bad_alloc() { throw bad_alloc(); }
}; // namespace pxcrt
namespace $n {
/* global variables */
/* function definitions */
}; /* namespace */
/* package main */
namespace $n {
}; /* namespace */
namespace exception$n { namespace impl$n { namespace builtin$n { 
void _exception$$impl$$builtin$$nsmain()
{
}
};};}; /* namespace exception::impl::builtin */
/* package main c */
static int i$_exception$$impl$$builtin$$nsmain$init = 0;
extern "C" {
void _exception$$impl$$builtin$$nsmain$c()
{
 if (i$_exception$$impl$$builtin$$nsmain$init == 0) {
  ::exception$n::impl$n::builtin$n::_exception$$impl$$builtin$$nsmain();
  i$_exception$$impl$$builtin$$nsmain$init = 1;
 }
}
}; /* extern "C" */
