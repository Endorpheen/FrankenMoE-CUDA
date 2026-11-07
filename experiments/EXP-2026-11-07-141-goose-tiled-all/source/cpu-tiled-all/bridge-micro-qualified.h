#pragma once
#include <stdint.h>
struct fg141_plan {int64_t mode,kind,first,stride,start,count,nk,row0,col0,out0,slab,route_first,slots,blocks,y_stride;};
void fg141_entry(const struct fg141_plan*,const void*,int64_t,const void*,int64_t,const void*,int64_t,void*);
void fg141_unpack(int,const void*,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,void*);
void fg141_micro(int,void*,int64_t,int64_t,int64_t,int64_t,int64_t);
void fg141_gather(int,const void*,int64_t,const void*,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,void*);
