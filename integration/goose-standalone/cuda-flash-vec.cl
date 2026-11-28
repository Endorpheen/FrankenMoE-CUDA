#include "flash-vec-types.h"

static float fg175v_mul_rn(float a, float b) { return fg_device_mul_rn(a, b); }
static float fg175v_fma_rn(float a, float b, float c) { return fg_device_fma(a, b, c); }
static float fg175v_ld_f32(uint64_t p, uint64_t off) { return fg_device_global_load(p + off); }
static void fg175v_st_f32(uint64_t p, uint64_t off, float v) { fg_device_global_store(p + off, v); }
static uint32_t fg175v_ld_u16(uint64_t p, uint64_t off) { return fg175q_ld_u16(p, off); }
static int32_t fg175v_ld_i32(uint64_t p, uint64_t off) { return *(__global int32_t *) (p + off); }
static void fg175v_st_i32(uint64_t p, uint64_t off, int32_t v) { *(__global int32_t *) (p + off) = v; }
static float fg175v_h2f(uint32_t bits) { return fg175q_h2f(bits); }
static int32_t fg175v_dp4a(int32_t a, int32_t b, int32_t c) { return mmvq_dp4a(a, b, c); }
static float fg175v_expf(float v) { return fg_attn_exp(v); }
static float fg175v_powf(float base, int32_t exp) { return fg_attn_exp(fg_vector_log(base) * (float) exp); }
static float fg175v_fmaxf(float a, float b) { return fg_vector_max(a, b); }
static int32_t fg175v_round_i8(float v) { return fg180q_i32(fg180q_round(v)); }
static float fg175v_shfl_xor(float v, int32_t delta) { return fg_device_shfl(v, delta); }
static void fg175v_syncwarp(void) { __asm__ volatile("bar.warp.sync -1;" ::: "memory"); }
static void fg175v_barrier(void) { fg_device_barrier(); }
static void fg175v_lc(void) {}
static void fg175v_sync(void) {}
static float fg175v_sm_f32(uint64_t p, uint64_t i) { return ((__local float *) p)[i]; }
static void fg175v_sm_st_f32(uint64_t p, uint64_t i, float v) { ((__local float *) p)[i] = v; }
static int32_t fg175v_sm_i32(uint64_t p, uint64_t i) { return ((__local int32_t *) p)[i]; }
static void fg175v_sm_st_i32(uint64_t p, uint64_t i, int32_t v) { ((__local int32_t *) p)[i] = v; }

static int64_t flash_vector_plan_word(uint64_t address, int32_t index) {
    return ((__global const int64_t *) address)[index];
}

#include "flash-vec-device.h"

#include "cuda-flash-vec-entries.h"
