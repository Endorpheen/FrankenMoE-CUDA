#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
static uint32_t fg_q8k_float_bits(float x) {uint32_t bits;memcpy(&bits,&x,sizeof(bits));return bits;}
#define main fg_unused_cli_main
#include "../../generated/q8k-quant.c"
#undef main
_Static_assert(sizeof(gg_type_3)==292,"Q8_K layout");
_Static_assert(offsetof(gg_type_3,f1)==4 && offsetof(gg_type_3,f2)==260,"Q8_K fields");
void fg_selfhost_quant_q8k(const float *x,void *y,int64_t n) {
    assert(n>=0&&n%256==0);
    gg_function_1(gg_view((void *)x,n),gg_view(y,n/256),0);
}
