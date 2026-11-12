#pragma once
#include <stdint.h>
typedef struct{void*data;int64_t len;}fg176rd_gg_slice;


typedef struct fg176rd_gg_type_1 fg176rd_gg_type_1;
#pragma pack(push, 1)
struct fg176rd_gg_type_1 {
  uint32_t f0;
  uint32_t f1;
  uint32_t f2;
};
#pragma pack(pop)
typedef struct fg176rd_gg_type_8 fg176rd_gg_type_8;
#pragma pack(push, 1)
struct fg176rd_gg_type_8 {
  int64_t f3;
  int64_t f4;
  int64_t f5;
  int64_t f6;
};
#pragma pack(pop)
using fg176r_FastDiv = fg176rd_gg_type_1;
static_assert(sizeof(fg176r_FastDiv)==12,"Goose FastDiv POD width");
