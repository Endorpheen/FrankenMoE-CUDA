#pragma once
#include <stdint.h>
struct fg141_plan {int64_t mode,kind,first,stride,start,count,nk,row0,col0,out0,slab,route_first,slots,blocks,y_stride,origin,input_first,output_base,dense,defer_bias,width,output_rows,ith,nth,enabled,forced,out_stride1,out_stride2,out_stride3,n11,n12,n13,n02,n03,nb02,nb03,context,need_quant,src_nb0,src_nb1,src_nb2,src_nb3,workspace_mod,wdata_bytes;};
void fg141_entry(const struct fg141_plan*,const void*,int64_t,const void*,int64_t,const void*,int64_t,void*);
void fg141_unpack(int,const void*,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,void*);
void fg141_micro(int,void*,int64_t,int64_t,int64_t,int64_t,int64_t);
void fg141_gather(int,const void*,int64_t,const void*,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,void*);
int64_t fg141_execute(const struct fg141_plan*,const void*,int64_t,const void*,int64_t,const void*,int64_t,void*,float*,int64_t);
int64_t fg141_workspace_offset(const struct fg141_plan*);
uint8_t fg141_general_enabled(void);
#ifndef FG141_KERNEL_ONLY
void fg141_quantize(int64_t,int64_t,int64_t,int64_t);
void fg141_reset(int64_t,int32_t);
void fg141_barrier(int64_t);
int32_t fg141_claim(int64_t);
#endif
