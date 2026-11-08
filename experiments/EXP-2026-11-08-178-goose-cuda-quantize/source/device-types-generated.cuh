#pragma once
#include <stdint.h>
// Native init_fastdiv_values packs <mp, L, divisor> as three uint32 words; the
// emitted Goose struct has exactly that layout.
typedef struct fg178qd_gg_type_3 fg178qd_gg_type_3;
#pragma pack(push, 1)
struct fg178qd_gg_type_3 { uint32_t f0; uint32_t f1; uint32_t f2; };
#pragma pack(pop)
using fg178q_FastDiv = fg178qd_gg_type_3;
static_assert(sizeof(fg178q_FastDiv)==12,"Goose FastDiv POD width");
