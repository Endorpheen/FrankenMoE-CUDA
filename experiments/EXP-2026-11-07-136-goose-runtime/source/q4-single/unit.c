#include <immintrin.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
float fg_half_to_float(uint16_t x) { return _cvtsh_ss(x); }
#define main fg_unused_cli_main
#include "../../generated/q4-single.c"
#undef main
_Static_assert(sizeof(gg_type_1)==144, "Q4_K layout");
_Static_assert(sizeof(gg_type_8)==292, "Q8_K layout");
_Static_assert(offsetof(gg_type_1,f2)==4 && offsetof(gg_type_1,f3)==16, "Q4_K fields");
_Static_assert(offsetof(gg_type_8,f5)==4 && offsetof(gg_type_8,f6)==260, "Q8_K fields");
float fg_selfhost_q4dot(int n, const void *x, const void *y) {
    assert(n>0 && n%256==0);
    return gg_function_1(gg_view((void*)x,n/256),gg_view((void*)y,n/256),0);
}
