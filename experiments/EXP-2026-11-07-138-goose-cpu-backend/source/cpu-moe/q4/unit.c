#include <immintrin.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
static float fg_half_to_float(uint16_t x) { return _cvtsh_ss(x); }
#define main fg_tile_unused_cli_main
#include "../../../generated/cpu-moe-q4.c"
#undef main
_Static_assert(sizeof(gg_type_1)==144,"Q4_K ABI");
_Static_assert(sizeof(gg_type_8)==292,"Q8_K ABI");
_Static_assert(sizeof(gg_type_19)==64,"16-row return ABI");
void fg_selfhost_q4_tile(int n,int rows,float*out,const void*x,const void*y) {
 assert(n>0&&n%256==0&&rows>0&&rows<=16);
 gg_type_19 v=gg_function_1(gg_view((void*)x,(n/256)*rows),gg_view((void*)y,n/256),n/256,rows,0);
 memcpy(out,v.data,(size_t)rows*sizeof(float));
}
