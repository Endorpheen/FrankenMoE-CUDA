#include "bridge.h"
#include <stddef.h>
#include <string.h>
#ifdef FG140_NO_WITNESS
#define fg140_witness(c,k,v) ((void)(c),(void)(k),(void)(v))
#endif
#define main fg140_unused_main
#include "../../generated/moe-control.c"
#undef main
_Static_assert(sizeof(gg_type_1)==sizeof(struct fg140_plan),"Plan ABI");
_Static_assert(sizeof(gg_type_4)==sizeof(struct fg140_row),"Row ABI");
_Static_assert(offsetof(gg_type_1,f0)==offsetof(struct fg140_plan,context),"context ABI");
_Static_assert(offsetof(gg_type_1,f1)==offsetof(struct fg140_plan,mode),"mode ABI");
_Static_assert(offsetof(gg_type_1,f2)==offsetof(struct fg140_plan,ith),"ith ABI");
_Static_assert(offsetof(gg_type_1,f3)==offsetof(struct fg140_plan,nth),"nth ABI");
_Static_assert(offsetof(gg_type_1,f4)==offsetof(struct fg140_plan,experts),"experts ABI");
_Static_assert(offsetof(gg_type_1,f5)==offsetof(struct fg140_plan,real_experts),"real_experts ABI");
_Static_assert(offsetof(gg_type_1,f6)==offsetof(struct fg140_plan,ids_per_token),"ids_per_token ABI");
_Static_assert(offsetof(gg_type_1,f7)==offsetof(struct fg140_plan,tokens),"tokens ABI");
_Static_assert(offsetof(gg_type_1,f8)==offsetof(struct fg140_plan,need_quant),"need_quant ABI");
_Static_assert(offsetof(gg_type_1,f9)==offsetof(struct fg140_plan,block_size),"block_size ABI");
_Static_assert(offsetof(gg_type_1,f10)==offsetof(struct fg140_plan,block_bytes),"block_bytes ABI");
_Static_assert(offsetof(gg_type_1,f11)==offsetof(struct fg140_plan,ne10),"ne10 ABI");
_Static_assert(offsetof(gg_type_1,f12)==offsetof(struct fg140_plan,ne11),"ne11 ABI");
_Static_assert(offsetof(gg_type_1,f13)==offsetof(struct fg140_plan,ne12),"ne12 ABI");
_Static_assert(offsetof(gg_type_1,f14)==offsetof(struct fg140_plan,ne13),"ne13 ABI");
_Static_assert(offsetof(gg_type_1,f15)==offsetof(struct fg140_plan,nb10),"nb10 ABI");
_Static_assert(offsetof(gg_type_1,f16)==offsetof(struct fg140_plan,nb11),"nb11 ABI");
_Static_assert(offsetof(gg_type_1,f17)==offsetof(struct fg140_plan,nb12),"nb12 ABI");
_Static_assert(offsetof(gg_type_1,f18)==offsetof(struct fg140_plan,nb13),"nb13 ABI");
_Static_assert(offsetof(gg_type_1,f19)==offsetof(struct fg140_plan,output_rows),"output_rows ABI");
_Static_assert(offsetof(gg_type_1,f20)==offsetof(struct fg140_plan,row_size),"row_size ABI");
_Static_assert(offsetof(gg_type_1,f21)==offsetof(struct fg140_plan,ids_stride0),"ids_stride0 ABI");
_Static_assert(offsetof(gg_type_1,f22)==offsetof(struct fg140_plan,ids_stride1),"ids_stride1 ABI");
_Static_assert(offsetof(gg_type_1,f23)==offsetof(struct fg140_plan,source_contiguous),"source_contiguous ABI");
_Static_assert(offsetof(gg_type_1,f24)==offsetof(struct fg140_plan,is_iq2s),"is_iq2s ABI");
_Static_assert(offsetof(gg_type_1,f25)==offsetof(struct fg140_plan,fused_available),"fused_available ABI");
_Static_assert(offsetof(gg_type_1,f26)==offsetof(struct fg140_plan,disable_chunking),"disable_chunking ABI");
_Static_assert(offsetof(gg_type_1,f27)==offsetof(struct fg140_plan,tiled_bytes),"tiled_bytes ABI");
_Static_assert(offsetof(gg_type_1,f28)==offsetof(struct fg140_plan,cache_line),"cache_line ABI");
_Static_assert(offsetof(gg_type_1,f29)==offsetof(struct fg140_plan,wdata_mod),"wdata_mod ABI");
_Static_assert(offsetof(gg_type_1,f30)==offsetof(struct fg140_plan,wdata_bytes),"wdata_bytes ABI");
_Static_assert(offsetof(gg_type_1,f31)==offsetof(struct fg140_plan,dst_bytes),"dst_bytes ABI");
_Static_assert(offsetof(gg_type_1,f32)==offsetof(struct fg140_plan,counts_offset),"counts_offset ABI");
_Static_assert(offsetof(gg_type_1,f33)==offsetof(struct fg140_plan,rows_offset),"rows_offset ABI");
_Static_assert(offsetof(gg_type_1,f34)==offsetof(struct fg140_plan,chunks_offset),"chunks_offset ABI");
_Static_assert(offsetof(gg_type_1,f35)==offsetof(struct fg140_plan,scratch_offset),"scratch_offset ABI");
_Static_assert(offsetof(gg_type_1,f36)==offsetof(struct fg140_plan,total_bytes),"total_bytes ABI");
_Static_assert(offsetof(gg_type_1,f37)==offsetof(struct fg140_plan,apex_math),"apex_math ABI");
_Static_assert(offsetof(gg_type_1,f38)==offsetof(struct fg140_plan,weight_row_bytes),"weight_row_bytes ABI");
_Static_assert(offsetof(gg_type_1,f39)==offsetof(struct fg140_plan,nb01),"nb01 ABI");
_Static_assert(offsetof(gg_type_1,f40)==offsetof(struct fg140_plan,nb02),"nb02 ABI");
_Static_assert(offsetof(gg_type_1,f41)==offsetof(struct fg140_plan,dst_nb1),"dst_nb1 ABI");
_Static_assert(offsetof(gg_type_1,f42)==offsetof(struct fg140_plan,dst_nb2),"dst_nb2 ABI");
void fg140_entry(struct fg140_plan*p,int64_t phase,int64_t*counts,int64_t nc,struct fg140_row*rows,int64_t nr,const int32_t*ids,int64_t ni,const int32_t*mask,int64_t nm,const int32_t*lut,int64_t nl,uint8_t*dst,int64_t nd){
 gg_function_1((gg_type_1*)p,phase,gg_view(counts,nc),gg_view(rows,nr),gg_view((void*)ids,ni),gg_view((void*)mask,nm),gg_view((void*)lut,nl),gg_view(dst,nd),0);
}
#ifdef FG140_TEST
struct fg140_callbacks {
 void(*quantize)(int64_t,int64_t,int64_t,int64_t);
 void(*reset)(int64_t,int64_t,int32_t);
 void(*barrier)(int64_t);
 void(*ready)(int64_t,int64_t);
 uint8_t(*tiled)(int64_t,int64_t,int64_t);
 void(*chunk)(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
 void(*matrix)(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
 int32_t(*claim)(int64_t,int64_t);
 void(*stats)(int64_t,int64_t);
 void(*witness)(int64_t,int64_t,int64_t);
};
static struct fg140_callbacks callbacks;
void fg140_set_callbacks(const void*p){memcpy(&callbacks,p,sizeof(callbacks));}
void fg140_quantize(int64_t c,int64_t a,int64_t b,int64_t n){callbacks.quantize(c,a,b,n);}
void fg140_reset(int64_t c,int64_t e,int32_t n){callbacks.reset(c,e,n);}
void fg140_barrier(int64_t c){callbacks.barrier(c);}
void fg140_ready(int64_t c,int64_t e){callbacks.ready(c,e);}
uint8_t fg140_tiled(int64_t c,int64_t e,int64_t n){return callbacks.tiled(c,e,n);}
void fg140_chunk(int64_t c,int64_t e,int64_t a,int64_t b,int64_t x,int64_t y,int64_t f){callbacks.chunk(c,e,a,b,x,y,f);}
void fg140_matrix(int64_t c,int64_t w,int64_t y,int64_t o,int64_t n,int64_t r){callbacks.matrix(c,w,y,o,n,r);}
int32_t fg140_claim(int64_t c,int64_t e){return callbacks.claim(c,e);}
void fg140_iq_stats(int64_t c,int64_t i){callbacks.stats(c,i);}
void fg140_witness(int64_t c,int64_t k,int64_t v){callbacks.witness(c,k,v);}
#endif
