#pragma once
#include <stdint.h>
struct fg174s_plan{uint64_t ptrs[8],dims[8][4],strides[8][4],iv[24];float fv[8];uint32_t magic[2][3];};
struct fg174s_packet{int64_t args[5];fg174s_plan plan;};
