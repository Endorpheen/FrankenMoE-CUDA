#pragma once
#include <stdint.h>
struct fg140_plan {int64_t context,mode,ith,nth,experts,real_experts,ids_per_token,tokens,need_quant,block_size,block_bytes,ne10,ne11,ne12,ne13,nb10,nb11,nb12,nb13,output_rows,row_size,ids_stride0,ids_stride1,source_contiguous,is_iq2s,fused_available,disable_chunking,tiled_bytes,cache_line,wdata_mod,wdata_bytes,dst_bytes,counts_offset,rows_offset,chunks_offset,scratch_offset,total_bytes,apex_math,weight_row_bytes,nb01,nb02,dst_nb1,dst_nb2;};
struct fg140_row {int32_t slot,token;};
void fg140_entry(struct fg140_plan*,int64_t,int64_t*,int64_t,struct fg140_row*,int64_t,const int32_t*,int64_t,const int32_t*,int64_t,const int32_t*,int64_t,uint8_t*,int64_t);
void fg140_quantize(int64_t,int64_t,int64_t,int64_t);
void fg140_reset(int64_t,int64_t,int32_t);
void fg140_barrier(int64_t);
void fg140_ready(int64_t,int64_t);
uint8_t fg140_tiled(int64_t,int64_t,int64_t);
void fg140_chunk(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
void fg140_matrix(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
int32_t fg140_claim(int64_t,int64_t);
void fg140_iq_stats(int64_t,int64_t);
void fg140_witness(int64_t,int64_t,int64_t);
void fg140_set_callbacks(const void*);
