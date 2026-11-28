typedef struct { uint32_t mp_g, shift_g, divisor_g; } MmvqFastDiv_g;

static int32_t mmvq_dp4a(int32_t a, int32_t b, int32_t c) {
    int32_t dot;
    __asm__("dp4a.s32.s32 %0, %1, %2, %3;" : "=r"(dot) : "r"(a), "r"(b), "r"(c));
    return dot;
}

static int32_t fg185q_ld_i32(uint64_t p, uint64_t off) { return *(__global int32_t *) (p + off); }
static uint32_t fg185q_ld_u16(uint64_t p, uint64_t off) { return *(__global uint16_t *) (p + off); }
static float fg185q_ld_h2(uint64_t p, uint64_t off) { return fg248d_ld_half(p + off); }
static float fg185q_ld_h2b(uint64_t p, uint64_t off) { return fg248d_ld_half(p + off + 2); }
static float fg185q_shfl_xor(float v, int32_t delta, int32_t width) {
    return fg_device_shfl(v, delta);
}

static int32_t fg186q_ld_i8(uint64_t p, uint64_t off) { return fg248d_ld_i8(p + off); }
static float fg186q_ld_half(uint64_t p, uint64_t off) { return fg248d_ld_half(p + off); }

static float fg178q_ld_f32(uint64_t p, uint64_t off) { return fg_device_global_load(p + off); }
static void fg178q_st_i8(uint64_t p, uint64_t off, int32_t v) { *(__global uint8_t *) (p + off) = (uint8_t) v; }
static void fg178q_st_f16(uint64_t p, uint64_t off, float v) { *(__global uint16_t *) (p + off) = mmq_half_bits(v); }
static uint32_t fg178q_mulhi(uint32_t a, uint32_t b) {
    uint32_t high;
    __asm__("mul.hi.u32 %0, %1, %2;" : "=r"(high) : "r"(a), "r"(b));
    return high;
}
static void fg178q_pdl_sync(void) {}
static void fg178q_pdl_lc(void) {}

#include "mmvq-q4-device.h"
#include "mmvq-q6-device.h"
#define gs_add_u32(a, b) ((uint32_t) ((a) + (b)))
static uint64_t mmvq_job_word(uint64_t jobs, uint64_t index) {
    return ((__global const uint64_t *) jobs)[index];
}
#include "mmvq-pack-device.h"
#undef gs_add_u32
#include "mmvq-device.h"

#include "cuda-mmvq-entries.h"
