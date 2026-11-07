#pragma once
#include <stdint.h>
struct fg139_task {int64_t mode,kind,blocks,rows,weight_stride,first,cols,slots,dst_stride1,dst_stride2,route_first,input_contiguous,input_row_blocks,input_stride1,input_stride2,output_stride;};
struct fg139_route {int32_t slot,token;};
#ifdef __cplusplus
extern "C" {
#endif
void fg139_execute(const struct fg139_task*,const void*,int64_t,const void*,int64_t,float*,int64_t,const void*,int64_t);
void fg139_dot(int,int,const void*,const void*,float*);
void fg139_dequantize(int,const void*,float*,int64_t);
#ifdef __cplusplus
}
#endif
