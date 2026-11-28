#define gs_add_u32(a,b) ((a)+(b))
#define gs_shl_u64(a,b) ((a)<<(b))

static float fg_dsa_pack_f16(uint64_t a) { return fg175q_h2f(*((__global const uint16_t *)a)); }

static uint32_t fg_dsa_pack_bits(float x) { return as_uint(x); }

static float fg_dsa_pack_float(uint32_t x) { return as_float(x); }

#include "cuda-dsa-certified-ptx-0.h"
#include "dsa-certified-device.h"

#include "cuda-dsa-entries.h"
