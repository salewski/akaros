#pragma once

// We now leverage GCC's built-in varargs support.
// The old code was correct for i386's calling convention
// but breaks when args are passed in registers.
// --asw

typedef __builtin_va_list va_list;

#define va_start(v, l)	__builtin_va_start(v, l)
#define va_end(v)	__builtin_va_end(v)
#define va_copy(d, s)	__builtin_va_copy(d, s)
#define va_arg(v, l)	__builtin_va_arg(v, l)
