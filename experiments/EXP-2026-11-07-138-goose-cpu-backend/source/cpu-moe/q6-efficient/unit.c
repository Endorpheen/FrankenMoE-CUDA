#include <immintrin.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>
static float fg_half_to_float(uint16_t x) {return _cvtsh_ss(x);}
#define main fg_moe_q6_unused_main
#include "../../../generated/cpu-moe-q6-efficient.c"
#undef main
_Static_assert(sizeof(gg_type_1)==210,"Q6_K ABI");
_Static_assert(offsetof(gg_type_1,f3)==208,"Q6_K scale ABI");
_Static_assert(sizeof(gg_type_10)==292,"Q8_K ABI");
_Static_assert(sizeof(gg_type_20)==64,"Tile return ABI");
void fg_selfhost_q6_tile(int n,int rows,float*out,const void*x,const void*y) {
 assert(n>0&&n%256==0&&rows>0&&rows<=16);
 gg_type_20 value=gg_function_1(gg_view((void*)x,(n/256)*rows),gg_view((void*)y,n/256),n/256,rows,0);
 memcpy(out,value.data,(size_t)rows*sizeof(float));
}
