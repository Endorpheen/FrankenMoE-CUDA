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

#include "flash-vec-device.h"

__kernel void goose_flash_vec_pack_query(__global const uint8_t * q, __global uint8_t * packed,
    __global const int64_t * cfg, float scale) {
    uint32_t head, lane;
    __local uint32_t shared[80];
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(head));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    fv_pack_query_g((uint64_t) q + (uint64_t) head * cfg[8], (uint64_t) packed + (uint64_t) head * 320,
        scale, lane, (uint64_t) shared);
}

#define FLASH_VEC_Q4(name, mode) \
__kernel void name(__global const uint8_t * q, __global const uint8_t * k, \
    __global const uint8_t * v, __global uint8_t * parts, __global uint8_t * meta, \
    __global const int64_t * cfg, int32_t keys, float scale) { \
    uint32_t head, tile, tiles, thread; \
    __local uint32_t shared[1152]; \
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(head)); \
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(tile)); \
    __asm__("mov.u32 %0, %%nctaid.y;" : "=r"(tiles)); \
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread)); \
    fv_fattn_vec_1_g((uint64_t) q, (uint64_t) k, (uint64_t) v, 0, 0, 0, \
        (uint64_t) parts, (uint64_t) meta, scale, 0.0f, 0.0f, 0.0f, 0, 0.0f, \
        256, 1, (int32_t) cfg[3], 1, cfg[7], cfg[8], 0, 256, keys, (int32_t) cfg[4], 1, \
        cfg[9], cfg[10], 0, cfg[11], cfg[12], 0, 1, 1, 1, 0, 0, 0, \
        mode, 0, tile, head, 1, tiles, thread % 32, thread / 32, (uint64_t) shared); \
}

FLASH_VEC_Q4(goose_flash_vec_q4, 1)
FLASH_VEC_Q4(goose_flash_vec_q4_packed, 0)
#undef FLASH_VEC_Q4

__kernel void goose_flash_vec_combine(__global const uint8_t * parts, __global const uint8_t * meta,
    __global uint8_t * output, int32_t tiles, int32_t heads) {
    uint32_t head, thread;
    __local float shared[128];
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(head));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    fv_combine_g((uint64_t) parts, (uint64_t) meta, (uint64_t) output,
        tiles, 0, head, 0, 1, heads, thread, (uint64_t) shared);
}
