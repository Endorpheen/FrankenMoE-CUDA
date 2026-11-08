#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdatomic.h>
#include <stdio.h>
#include "bridge.h"
#define main fg153_shapes_unused_main
#include "../../generated/shapes.c"
#undef main
_Static_assert(sizeof(gg_type_1)==sizeof(struct fg153_shape),"shape size ABI");
_Static_assert(offsetof(gg_type_1,f0)==offsetof(struct fg153_shape,ne),"shape dimensions ABI");
_Static_assert(offsetof(gg_type_1,f1)==offsetof(struct fg153_shape,nb),"shape strides ABI");
_Static_assert(offsetof(gg_type_1,f2)==offsetof(struct fg153_shape,type_size),"shape element size ABI");
_Static_assert(offsetof(gg_type_1,f3)==offsetof(struct fg153_shape,block_size),"shape block ABI");
static atomic_uint_fast64_t calls[11];
int64_t fg153_shape_entry(int64_t mode,const struct fg153_shape*a,const struct fg153_shape*b,int64_t m,int64_t n){if(mode<0||mode>=11)__builtin_trap();gg_type_1 x,y;memcpy(&x,a,sizeof(x));memcpy(&y,b,sizeof(y));atomic_fetch_add_explicit(calls+mode,1,memory_order_relaxed);return gg_function_1(mode,&x,&y,m,n,0);}
__attribute__((destructor))static void fg153_report(void){for(int i=0;i<11;i++){uint64_t n=atomic_load(calls+i);if(n)fprintf(stderr,"FG153_SHAPE mode=%d calls=%llu\n",i,(unsigned long long)n);}}
