#ifndef FG167_PLAN_H
#define FG167_PLAN_H
#include <stdint.h>
struct fg167_plan{uint64_t ptrs[10];uint64_t dims[3][4],strides[3][4];uint32_t magic[8][3],grid[3],threads[3];uint32_t kind_a,kind_b,kind_d,op,fuse,path,mix;};
#endif
