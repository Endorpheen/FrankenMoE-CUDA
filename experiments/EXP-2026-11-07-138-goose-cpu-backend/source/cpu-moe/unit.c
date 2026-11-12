#include "bridge.h"
#include <stddef.h>
#define main fg_moe_unused_main
#include "../../generated/cpu-moe.c"
#undef main
_Static_assert(sizeof(gg_type_1)==sizeof(struct fg_moe_plan),"Plan ABI");
_Static_assert(sizeof(gg_type_4)==8,"Row ABI");
_Static_assert(offsetof(gg_type_1,f0)==offsetof(struct fg_moe_plan,context),"context ABI");
_Static_assert(offsetof(gg_type_1,f1)==offsetof(struct fg_moe_plan,ith),"ith ABI");
_Static_assert(offsetof(gg_type_1,f2)==offsetof(struct fg_moe_plan,nth),"nth ABI");
_Static_assert(offsetof(gg_type_1,f3)==offsetof(struct fg_moe_plan,experts),"experts ABI");
_Static_assert(offsetof(gg_type_1,f4)==offsetof(struct fg_moe_plan,ids_per_token),"ids_per_token ABI");
_Static_assert(offsetof(gg_type_1,f5)==offsetof(struct fg_moe_plan,tokens),"tokens ABI");
_Static_assert(offsetof(gg_type_1,f6)==offsetof(struct fg_moe_plan,need_quant),"need_quant ABI");
_Static_assert(offsetof(gg_type_1,f7)==offsetof(struct fg_moe_plan,block_size),"block_size ABI");
_Static_assert(offsetof(gg_type_1,f8)==offsetof(struct fg_moe_plan,block_bytes),"block_bytes ABI");
_Static_assert(offsetof(gg_type_1,f9)==offsetof(struct fg_moe_plan,ne00),"ne00 ABI");
_Static_assert(offsetof(gg_type_1,f10)==offsetof(struct fg_moe_plan,ne01),"ne01 ABI");
_Static_assert(offsetof(gg_type_1,f11)==offsetof(struct fg_moe_plan,ne10),"ne10 ABI");
_Static_assert(offsetof(gg_type_1,f12)==offsetof(struct fg_moe_plan,ne11),"ne11 ABI");
_Static_assert(offsetof(gg_type_1,f13)==offsetof(struct fg_moe_plan,ne12),"ne12 ABI");
_Static_assert(offsetof(gg_type_1,f14)==offsetof(struct fg_moe_plan,ne13),"ne13 ABI");
_Static_assert(offsetof(gg_type_1,f15)==offsetof(struct fg_moe_plan,nb01),"nb01 ABI");
_Static_assert(offsetof(gg_type_1,f16)==offsetof(struct fg_moe_plan,nb02),"nb02 ABI");
_Static_assert(offsetof(gg_type_1,f17)==offsetof(struct fg_moe_plan,nb10),"nb10 ABI");
_Static_assert(offsetof(gg_type_1,f18)==offsetof(struct fg_moe_plan,nb11),"nb11 ABI");
_Static_assert(offsetof(gg_type_1,f19)==offsetof(struct fg_moe_plan,nb12),"nb12 ABI");
_Static_assert(offsetof(gg_type_1,f20)==offsetof(struct fg_moe_plan,nb13),"nb13 ABI");
_Static_assert(offsetof(gg_type_1,f21)==offsetof(struct fg_moe_plan,dst_nb1),"dst_nb1 ABI");
_Static_assert(offsetof(gg_type_1,f22)==offsetof(struct fg_moe_plan,dst_nb2),"dst_nb2 ABI");
_Static_assert(offsetof(gg_type_1,f23)==offsetof(struct fg_moe_plan,row_size),"row_size ABI");
_Static_assert(offsetof(gg_type_1,f24)==offsetof(struct fg_moe_plan,source_contiguous),"source_contiguous ABI");
_Static_assert(offsetof(gg_type_1,f25)==offsetof(struct fg_moe_plan,x_base),"x_base ABI");
_Static_assert(offsetof(gg_type_1,f26)==offsetof(struct fg_moe_plan,y_base),"y_base ABI");
_Static_assert(offsetof(gg_type_1,f27)==offsetof(struct fg_moe_plan,w_base),"w_base ABI");
_Static_assert(offsetof(gg_type_1,f28)==offsetof(struct fg_moe_plan,dst_base),"dst_base ABI");
_Static_assert(offsetof(gg_type_1,f29)==offsetof(struct fg_moe_plan,disable_chunking),"disable_chunking ABI");
_Static_assert(offsetof(gg_type_1,f30)==offsetof(struct fg_moe_plan,weight_row_bytes),"weight_row_bytes ABI");
void fg_selfhost_cpu_moe(const struct fg_moe_plan*p,int64_t*counts,struct mmid_row_mapping*rows,int64_t nrows) {
 gg_function_1((gg_type_1*)p,gg_view(counts,p->experts),gg_view(rows,nrows),0);
}
