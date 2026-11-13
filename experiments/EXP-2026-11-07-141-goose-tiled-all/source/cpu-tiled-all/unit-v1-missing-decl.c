#define GGML_COMMON_IMPL_C
#include "ggml-common.h"
#include "bridge.h"
#include <immintrin.h>
#include <stddef.h>
#include <stdio.h>
#include <stdatomic.h>
static float fg141_half(uint16_t x){return _cvtsh_ss(x);}
#ifndef FG141_TRACE
#define FG141_TRACE 0
#endif
#if FG141_TRACE
static atomic_uint_fast64_t events[8][13];
#endif
static void fg141_witness(int64_t stage,int64_t kind,int64_t units){
#if FG141_TRACE
 if(stage<0||stage>=8||kind<0||kind>=13||units<0)__builtin_trap();atomic_fetch_add_explicit(&events[stage][kind],units,memory_order_relaxed);
#else
 (void)stage;(void)kind;(void)units;
#endif
}
#define main fg141_unused_main
#include "../../generated/cpu-tiled-all.c"
#undef main
_Static_assert(sizeof(gg_type_1)==sizeof(struct fg141_plan),"Plan ABI");
_Static_assert(sizeof(gg_type_5)==292,"Q8_K ABI");
_Static_assert(sizeof(gg_type_13)==8,"Route ABI");
_Static_assert(sizeof(gg_type_17)==100352,"Weight tile ABI");
_Static_assert(sizeof(gg_type_24)==82944,"Activation tile ABI");
_Static_assert(sizeof(gg_type_16)==445440,"Workspace ABI");
_Static_assert(offsetof(gg_type_1,f0)==offsetof(struct fg141_plan,mode),"mode ABI");
_Static_assert(offsetof(gg_type_1,f1)==offsetof(struct fg141_plan,kind),"kind ABI");
_Static_assert(offsetof(gg_type_1,f2)==offsetof(struct fg141_plan,first),"first ABI");
_Static_assert(offsetof(gg_type_1,f3)==offsetof(struct fg141_plan,stride),"stride ABI");
_Static_assert(offsetof(gg_type_1,f4)==offsetof(struct fg141_plan,start),"start ABI");
_Static_assert(offsetof(gg_type_1,f5)==offsetof(struct fg141_plan,count),"count ABI");
_Static_assert(offsetof(gg_type_1,f6)==offsetof(struct fg141_plan,nk),"nk ABI");
_Static_assert(offsetof(gg_type_1,f7)==offsetof(struct fg141_plan,row0),"row0 ABI");
_Static_assert(offsetof(gg_type_1,f8)==offsetof(struct fg141_plan,col0),"col0 ABI");
_Static_assert(offsetof(gg_type_1,f9)==offsetof(struct fg141_plan,out0),"out0 ABI");
_Static_assert(offsetof(gg_type_1,f10)==offsetof(struct fg141_plan,slab),"slab ABI");
_Static_assert(offsetof(gg_type_1,f11)==offsetof(struct fg141_plan,route_first),"route_first ABI");
_Static_assert(offsetof(gg_type_1,f12)==offsetof(struct fg141_plan,slots),"slots ABI");
_Static_assert(offsetof(gg_type_1,f13)==offsetof(struct fg141_plan,blocks),"blocks ABI");
_Static_assert(offsetof(gg_type_16,f28)==100352&&offsetof(gg_type_16,f29)==183296,"Workspace fields ABI");
_Static_assert(sizeof(block_q4_K)==144,"q4_K size");
_Static_assert(offsetof(block_q4_K,d)==0,"q4_K.d");
_Static_assert(offsetof(block_q4_K,dmin)==2,"q4_K.dmin");
_Static_assert(offsetof(block_q4_K,scales)==4,"q4_K.scales");
_Static_assert(offsetof(block_q4_K,qs)==16,"q4_K.qs");
_Static_assert(sizeof(block_q5_K)==176,"q5_K size");
_Static_assert(offsetof(block_q5_K,d)==0,"q5_K.d");
_Static_assert(offsetof(block_q5_K,dmin)==2,"q5_K.dmin");
_Static_assert(offsetof(block_q5_K,scales)==4,"q5_K.scales");
_Static_assert(offsetof(block_q5_K,qh)==16,"q5_K.qh");
_Static_assert(offsetof(block_q5_K,qs)==48,"q5_K.qs");
_Static_assert(sizeof(block_q6_K)==210,"q6_K size");
_Static_assert(offsetof(block_q6_K,ql)==0,"q6_K.ql");
_Static_assert(offsetof(block_q6_K,qh)==128,"q6_K.qh");
_Static_assert(offsetof(block_q6_K,scales)==192,"q6_K.scales");
_Static_assert(offsetof(block_q6_K,d)==208,"q6_K.d");
_Static_assert(sizeof(block_q3_K)==110,"q3_K size");
_Static_assert(offsetof(block_q3_K,hmask)==0,"q3_K.hmask");
_Static_assert(offsetof(block_q3_K,qs)==32,"q3_K.qs");
_Static_assert(offsetof(block_q3_K,scales)==96,"q3_K.scales");
_Static_assert(offsetof(block_q3_K,d)==108,"q3_K.d");
_Static_assert(sizeof(block_q2_K)==84,"q2_K size");
_Static_assert(offsetof(block_q2_K,scales)==0,"q2_K.scales");
_Static_assert(offsetof(block_q2_K,qs)==16,"q2_K.qs");
_Static_assert(offsetof(block_q2_K,d)==80,"q2_K.d");
_Static_assert(offsetof(block_q2_K,dmin)==82,"q2_K.dmin");
_Static_assert(sizeof(block_iq4_xs)==136,"iq4_xs size");
_Static_assert(offsetof(block_iq4_xs,d)==0,"iq4_xs.d");
_Static_assert(offsetof(block_iq4_xs,scales_h)==2,"iq4_xs.scales_h");
_Static_assert(offsetof(block_iq4_xs,scales_l)==4,"iq4_xs.scales_l");
_Static_assert(offsetof(block_iq4_xs,qs)==8,"iq4_xs.qs");
_Static_assert(sizeof(block_iq2_xxs)==66,"iq2_xxs size");
_Static_assert(offsetof(block_iq2_xxs,d)==0,"iq2_xxs.d");
_Static_assert(offsetof(block_iq2_xxs,qs)==2,"iq2_xxs.qs");
_Static_assert(sizeof(block_iq2_xs)==74,"iq2_xs size");
_Static_assert(offsetof(block_iq2_xs,d)==0,"iq2_xs.d");
_Static_assert(offsetof(block_iq2_xs,qs)==2,"iq2_xs.qs");
_Static_assert(offsetof(block_iq2_xs,scales)==66,"iq2_xs.scales");
_Static_assert(sizeof(block_iq2_s)==82,"iq2_s size");
_Static_assert(offsetof(block_iq2_s,d)==0,"iq2_s.d");
_Static_assert(offsetof(block_iq2_s,qs)==2,"iq2_s.qs");
_Static_assert(offsetof(block_iq2_s,qh)==66,"iq2_s.qh");
_Static_assert(offsetof(block_iq2_s,scales)==74,"iq2_s.scales");
_Static_assert(sizeof(block_iq3_xxs)==98,"iq3_xxs size");
_Static_assert(offsetof(block_iq3_xxs,d)==0,"iq3_xxs.d");
_Static_assert(offsetof(block_iq3_xxs,qs)==2,"iq3_xxs.qs");
_Static_assert(sizeof(block_iq3_s)==110,"iq3_s size");
_Static_assert(offsetof(block_iq3_s,d)==0,"iq3_s.d");
_Static_assert(offsetof(block_iq3_s,qs)==2,"iq3_s.qs");
_Static_assert(offsetof(block_iq3_s,qh)==66,"iq3_s.qh");
_Static_assert(offsetof(block_iq3_s,signs)==74,"iq3_s.signs");
_Static_assert(offsetof(block_iq3_s,scales)==106,"iq3_s.scales");
_Static_assert(sizeof(block_iq1_s)==50,"iq1_s size");
_Static_assert(offsetof(block_iq1_s,d)==0,"iq1_s.d");
_Static_assert(offsetof(block_iq1_s,qs)==2,"iq1_s.qs");
_Static_assert(offsetof(block_iq1_s,qh)==34,"iq1_s.qh");
_Static_assert(sizeof(block_iq1_m)==56,"iq1_m size");
_Static_assert(offsetof(block_iq1_m,qs)==0,"iq1_m.qs");
_Static_assert(offsetof(block_iq1_m,qh)==32,"iq1_m.qh");
_Static_assert(offsetof(block_iq1_m,scales)==48,"iq1_m.scales");
void fg141_entry(const struct fg141_plan*p,const void*x,int64_t nx,const void*y,int64_t ny,const void*routes,int64_t nr,void*w){
 gg_function_1((gg_type_1*)p,gg_view((void*)x,nx),gg_view((void*)y,ny),gg_view((void*)routes,nr),(gg_type_16*)w,gg_view((void*)iq2xxs_grid,256),gg_view((void*)iq2xs_grid,512),gg_view((void*)iq2s_grid,1024),gg_view((void*)iq3xxs_grid,256),gg_view((void*)iq3s_grid,512),gg_view((void*)iq1s_grid,NGRID_IQ1S),gg_view((void*)ksigns_iq2xs,128),gg_view((void*)kvalues_iq4nl,16),0);
}
void fg141_unpack(int kind,const void*x,int64_t nx,int64_t stride,int64_t first,int64_t start,int64_t count,int64_t nk,void*w){struct fg141_plan p={0};p.kind=kind;p.stride=stride;p.first=first;p.start=start;p.count=count;p.nk=nk;fg141_entry(&p,x,nx,NULL,0,NULL,0,w);}
#if FG141_TRACE
__attribute__((destructor))static void fg141_report(void){for(int s=0;s<8;++s)for(int k=0;k<13;++k){uint64_t n=atomic_load(&events[s][k]);if(n)fprintf(stderr,"FG141_TILED stage=%d kind=%d units=%llu\n",s,k,(unsigned long long)n);}}
#endif
