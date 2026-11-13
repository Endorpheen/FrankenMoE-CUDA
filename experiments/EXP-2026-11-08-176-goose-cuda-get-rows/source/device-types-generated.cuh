#pragma once
#include <stdint.h>
// Native init_fastdiv_values packs <mp, L, divisor> as three uint32 words; the
// emitted Goose struct has exactly that layout, padded to 1 to keep the ABI
// equal to the native uint3.
typedef struct fg176gd_gg_type_3 fg176gd_gg_type_3;
#pragma pack(push, 1)
struct fg176gd_gg_type_3 {
  uint32_t f0;
  uint32_t f1;
  uint32_t f2;
};
#pragma pack(pop)
using fg176g_FastDiv = fg176gd_gg_type_3;
static_assert(sizeof(fg176g_FastDiv)==12,"Goose FastDiv POD width");
