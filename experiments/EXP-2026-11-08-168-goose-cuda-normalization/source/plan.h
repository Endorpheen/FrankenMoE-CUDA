#ifndef FG168_PLAN_H
#define FG168_PLAN_H
#include <stdint.h>
struct fg168_plan{uint64_t ptrs[5],dims[5][4],strides[5][4],iv[24];float fv[8];uint32_t magic[8][3];};
#endif
