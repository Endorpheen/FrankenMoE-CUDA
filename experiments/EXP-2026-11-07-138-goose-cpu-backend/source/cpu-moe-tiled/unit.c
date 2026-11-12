#include "bridge.h"
#include <immintrin.h>
#include <stddef.h>
#include <stdio.h>
#include <stdatomic.h>
static float fg_tiled_half(uint16_t x){return _cvtsh_ss(x);}
#ifndef FG_TILED_TRACE
#define FG_TILED_TRACE 0
#endif
#if FG_TILED_TRACE
static atomic_uint_fast64_t fg_q4_microtiles,fg_q6_microtiles;
#endif
static void fg_tiled_witness(int64_t kind){
#if FG_TILED_TRACE
 if(kind==12)atomic_fetch_add_explicit(&fg_q4_microtiles,1,memory_order_relaxed);
 if(kind==14)atomic_fetch_add_explicit(&fg_q6_microtiles,1,memory_order_relaxed);
#else
 (void)kind;
#endif
}
#define main fg_tiled_unused_main
#include "../../generated/cpu-moe-tiled.c"
#undef main
_Static_assert(sizeof(gg_type_1)==sizeof(struct fg_tiled_plan),"Plan ABI");
_Static_assert(sizeof(gg_type_3)==144,"Q4 ABI");
_Static_assert(sizeof(gg_type_10)==210,"Q6 ABI");
_Static_assert(sizeof(gg_type_15)==292,"Q8 ABI");
_Static_assert(sizeof(gg_type_22)==8,"Route ABI");
_Static_assert(sizeof(gg_type_26)==100352,"Weight tile ABI");
_Static_assert(sizeof(gg_type_30)==82944,"Activation tile ABI");
_Static_assert(sizeof(gg_type_25)==445440,"Workspace ABI");
_Static_assert(offsetof(gg_type_25,f33)==100352 && offsetof(gg_type_25,f34)==183296,"Workspace fields ABI");
_Static_assert(offsetof(gg_type_1,f0)==offsetof(struct fg_tiled_plan,width),"width ABI");
_Static_assert(offsetof(gg_type_1,f1)==offsetof(struct fg_tiled_plan,output_rows),"output_rows ABI");
_Static_assert(offsetof(gg_type_1,f2)==offsetof(struct fg_tiled_plan,weight_stride),"weight_stride ABI");
_Static_assert(offsetof(gg_type_1,f3)==offsetof(struct fg_tiled_plan,slots),"slots ABI");
_Static_assert(offsetof(gg_type_1,f4)==offsetof(struct fg_tiled_plan,ith),"ith ABI");
_Static_assert(offsetof(gg_type_1,f5)==offsetof(struct fg_tiled_plan,nth),"nth ABI");
_Static_assert(offsetof(gg_type_1,f6)==offsetof(struct fg_tiled_plan,enabled),"enabled ABI");
_Static_assert(offsetof(gg_type_1,f7)==offsetof(struct fg_tiled_plan,forced),"forced ABI");
_Static_assert(offsetof(gg_type_1,f8)==offsetof(struct fg_tiled_plan,output_stride1),"output_stride1 ABI");
_Static_assert(offsetof(gg_type_1,f9)==offsetof(struct fg_tiled_plan,output_stride2),"output_stride2 ABI");
_Static_assert(offsetof(gg_type_1,f10)==offsetof(struct fg_tiled_plan,kind),"kind ABI");
uint8_t fg_selfhost_tiled(const struct fg_tiled_plan *p,const void *x,int64_t nx,const void *y,int64_t ny,const void *routes,int64_t nr,void *workspace,float *dst,int64_t ndst) {
 return gg_function_1((gg_type_1*)p,gg_view((void*)x,p->kind==12?nx:0),gg_view((void*)x,p->kind==14?nx:0),gg_view((void*)y,ny),gg_view((void*)routes,nr),(gg_type_25*)workspace,gg_view(dst,ndst),0);
}
#if FG_TILED_TRACE
__attribute__((destructor)) static void fg_tiled_report(void){fprintf(stderr,"FG_SELFHOST_TILED q4_microtiles=%llu q6_microtiles=%llu\n",(unsigned long long)atomic_load(&fg_q4_microtiles),(unsigned long long)atomic_load(&fg_q6_microtiles));}
#endif
