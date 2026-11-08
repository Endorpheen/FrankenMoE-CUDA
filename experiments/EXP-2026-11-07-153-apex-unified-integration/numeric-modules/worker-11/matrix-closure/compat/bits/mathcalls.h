/* CUDA 12.6 predates glibc 2.43.  glibc exposes cospi/sinpi/rsqrt with a C++
 * noexcept specification that CUDA math_functions.h does not have.  Remove
 * only those C23 extension declarations for the isolated nvcc build; no Apex
 * code uses cospi/sinpi and CUDA keeps its own rsqrt. */
#undef __GLIBC_USE_IEC_60559_FUNCS_EXT_C23
#define __GLIBC_USE_IEC_60559_FUNCS_EXT_C23 0
#include_next <bits/mathcalls.h>
