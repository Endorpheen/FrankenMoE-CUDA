#ifndef FG169_PLAN_H
#define FG169_PLAN_H
#include <stdint.h>
struct fg169_plan{uint64_t ptrs[3],dims[2][4],strides[2][4],iv[16];};
#endif
