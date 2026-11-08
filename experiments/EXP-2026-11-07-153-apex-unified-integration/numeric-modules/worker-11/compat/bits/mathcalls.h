/* CUDA 12.6 predates glibc 2.43.  glibc exposes cospi/sinpi/rsqrt with a
 * C++ noexcept specification that the CUDA math_functions.h declarations do
 * not have.  This shadow header only removes the conflicting C23 extension
 * declarations; CUDA keeps its own rsqrt and no Apex code uses cospi/sinpi.
 * It is an isolated build workaround, not a source change. */
#undef __GLIBC_USE_IEC_60559_FUNCS_EXT_C23
#define __GLIBC_USE_IEC_60559_FUNCS_EXT_C23 0
#include_next <bits/mathcalls.h>
