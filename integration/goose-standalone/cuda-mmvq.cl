typedef struct { uint32_t mp_g, shift_g, divisor_g; } MmvqFastDiv_g;

#include "cuda-mmvq-ptx-0.h"
static float fg185q_ld_h2(uint64_t p, uint64_t off) { return fg248d_ld_half(p + off); }
static float fg185q_ld_h2b(uint64_t p, uint64_t off) { return fg248d_ld_half(p + off + 2); }
static float fg185q_shfl_xor(float v, int32_t delta, int32_t width) {
    return fg_device_shfl(v, delta);
}

static int32_t fg186q_ld_i8(uint64_t p, uint64_t off) { return fg248d_ld_i8(p + off); }
static float fg186q_ld_half(uint64_t p, uint64_t off) { return fg248d_ld_half(p + off); }

static float fg178q_ld_f32(uint64_t p, uint64_t off) { return fg_device_global_load(p + off); }
static void fg178q_st_f16(uint64_t p, uint64_t off, float v) { *(__global uint16_t *) (p + off) = mmq_half_bits(v); }
#include "cuda-mmvq-ptx-1.h"
static void fg178q_pdl_sync(void) {}
static void fg178q_pdl_lc(void) {}

#include "mmvq-q4-device.h"
#include "mmvq-q6-device.h"
#define gs_add_u32(a, b) ((uint32_t) ((a) + (b)))
#include "mmvq-pack-device.h"
#undef gs_add_u32
#include "mmvq-device.h"

#include "cuda-mmvq-entries.h"
