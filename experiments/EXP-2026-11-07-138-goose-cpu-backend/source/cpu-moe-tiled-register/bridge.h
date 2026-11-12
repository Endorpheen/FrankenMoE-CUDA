#pragma once
#include <stdint.h>
struct fg_tiled_plan {
int64_t width;
int64_t output_rows;
int64_t weight_stride;
int64_t slots;
int64_t ith;
int64_t nth;
int64_t enabled;
int64_t forced;
int64_t output_stride1;
int64_t output_stride2;
int64_t kind;
};
uint8_t fg_selfhost_tiled(const struct fg_tiled_plan*,const void*,int64_t,const void*,int64_t,const void*,int64_t,void*,float*,int64_t);
