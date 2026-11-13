#pragma once
#include <stdint.h>
typedef struct fg168d_gg_type_8 fg168d_gg_type_8;
#pragma pack(push, 1)
struct fg168d_gg_type_8 {
  float f0;
  float f1;
};
#pragma pack(pop)
typedef struct fg168d_gg_type_10 fg168d_gg_type_10;
#pragma pack(push, 1)
struct fg168d_gg_type_10 {
  float f2;
  float f3;
  float f4;
  float f5;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { fg168d_gg_type_10 data[4]; } fg168d_gg_type_11;
#pragma pack(pop)

using fg168_F4=fg168d_gg_type_10;
static_assert(sizeof(fg168_F4)==16,"Goose float4 ABI");
