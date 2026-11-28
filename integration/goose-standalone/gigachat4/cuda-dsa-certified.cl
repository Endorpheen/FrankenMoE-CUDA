static uint32_t fg_dsa_int_word(uint64_t address) {
    return *(__global const uint32_t *)address;
}
#define gs_add_u32(a,b) ((a)+(b))
#define gs_shl_u64(a,b) ((a)<<(b))

static float fg_dsa_pack_f16(uint64_t a) { return fg175q_h2f(*((__global const uint16_t *)a)); }

static uint32_t fg_dsa_pack_bits(float x) { return as_uint(x); }

static float fg_dsa_pack_float(uint32_t x) { return as_float(x); }

static uint32_t fg_dsa_pack_xor(uint32_t x, uint32_t delta) {
    uint32_t result;
    __asm__("shfl.sync.bfly.b32 %0, %1, %2, 31, -1;" : "=r"(result) : "r"(x), "r"(delta));
    return result;
}

static int32_t fg_dsa_pack_rni(float x) {
    int32_t result;
    __asm__("cvt.rni.s32.f32 %0, %1;" : "=r"(result) : "f"(x));
    return result;
}

static void fg_dsa_pack_byte(uint64_t p, int32_t x) { *((__global int8_t *)p) = (int8_t)x; }

static float fg_dsa_integer_to_float(int64_t dot, float a, float b) {
    float x;
    __asm__("cvt.rn.f32.s64 %0, %1;" : "=f"(x) : "l"(dot));
    return fg_device_mul_rn(fg_device_mul_rn(x, a), b);
}

static float fg_dsa_bound_ceil_add(float a, float b) {
    float x;
    __asm__("add.rp.f32 %0, %1, %2;" : "=f"(x) : "f"(a), "f"(b));
    return x;
}

static float fg_dsa_bound_ceil_mul(float a, float b) {
    float x;
    __asm__("mul.rp.f32 %0, %1, %2;" : "=f"(x) : "f"(a), "f"(b));
    return x;
}

static float fg_dsa_bound_xor(float a, int32_t delta) {
    return fg_attn_shfl(a, delta);
}

static void fg_dsa_bound_atomic_max(uint64_t p, uint32_t v) {
    __asm__ volatile("red.global.max.u32 [%0], %1;" :: "l"(p), "r"(v) : "memory");
}

#include "dsa-certified-device.h"

__kernel void goose_dsa_pack_power2(__global const uint8_t *input, __global int8_t *hi,
    __global int8_t *lo, __global float *scales, int64_t stride, int32_t half_input) {
    uint32_t row, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(row));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    dsa_pack_power2_g((uint64_t)input, (uint64_t)hi, (uint64_t)lo,
        (uint64_t)scales, stride, row, lane, half_input);
}

__kernel void goose_dsa_integer_score(__global const int8_t *khi, __global const int8_t *klo,
    __global const int8_t *qhi, __global const int8_t *qlo, __global const float *kscale,
    __global const float *qscale, __global const float *weights, __global const uint16_t *mask,
    __global float *out, int64_t cells) {
    __local float dots[1024] __attribute__((aligned(16)));
    uint32_t tile, query, thread;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(tile));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(query));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    dsa_integer_score_tile_g((uint64_t)khi, (uint64_t)klo, (uint64_t)qhi, (uint64_t)qlo,
        (uint64_t)kscale, (uint64_t)qscale, (uint64_t)weights, (uint64_t)mask,
        (uint64_t)out, (uint64_t)dots, cells, query, tile, thread);
}

__kernel void goose_dsa_bound_metadata(__global const uint8_t *input, __global const float *scales,
    __global float *l1, __global float *key_max, int64_t stride, int32_t keys) {
    uint32_t row, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(row));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    dsa_bound_metadata_g((uint64_t)input, (uint64_t)scales, (uint64_t)l1,
        (uint64_t)key_max, stride, row, lane, keys);
}

__kernel void goose_dsa_bound_query(__global const float *q_l1, __global const float *q_scales,
    __global const float *key_max, __global const float *weights, __global float *output) {
    uint32_t row, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(row));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    dsa_bound_query_g((uint64_t)q_l1, (uint64_t)q_scales, (uint64_t)key_max,
        (uint64_t)weights, (uint64_t)output, row, lane);
}

__kernel void goose_dsa_score_cutoff(__global const float *scores, __global float *cutoffs, int64_t cells) {
    __local uint32_t bins[16];
    uint32_t row, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(row));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    dsa_score_cutoff_g((uint64_t)scores, (uint64_t)cutoffs, cells, row, lane, 256, (uint64_t)bins);
}

__kernel void goose_dsa_compact(__global const float *scores, __global const uint16_t *mask,
    __global const float *cutoffs, __global const float *errors, __global uint32_t *fixed,
    __global uint32_t *band, __global uint32_t *counts, int64_t cells, int64_t band_capacity) {
    __local uint32_t bins[3];
    uint32_t row, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(row));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    dsa_compact_g((uint64_t)scores, (uint64_t)mask, (uint64_t)cutoffs, (uint64_t)errors,
        (uint64_t)fixed, (uint64_t)band, (uint64_t)counts, (uint64_t)bins, cells, row,
        lane, 256, band_capacity);
}

__kernel void goose_dsa_replay(__global const uint8_t *q, __global const uint8_t *k,
    __global const uint8_t *weights, __global const uint8_t *mask, __global float *out,
    __global const int64_t *cfg, __global const uint32_t *band, __global const uint32_t *counts,
    int64_t capacity) {
    uint32_t tile, row, tid;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(tile));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(row));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(tid));
    dsa_replay_g((uint64_t)q, (uint64_t)k, (uint64_t)weights, (uint64_t)mask,
        (uint64_t)out, (uint64_t)cfg, (uint64_t)band, (uint64_t)counts, capacity,
        row, tile, tid);
}

__kernel void goose_dsa_finalize(__global const float *scores, __global const uint32_t *band,
    __global const uint32_t *counts, __global uint32_t *out, __global uint32_t *status, int64_t cells) {
    __local uint64_t keys[1024];
    __local uint32_t invalid;
    uint32_t row, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(row));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    dsa_finalize_g((uint64_t)scores,(uint64_t)band,(uint64_t)counts,(uint64_t)out,
        (uint64_t)status,(uint64_t)keys,(uint64_t)&invalid,row,lane,256,cells);
}

__kernel void goose_dsa_fallback_score(__global const uint8_t *q, __global const uint8_t *k,
    __global const uint8_t *weights, __global const uint8_t *mask, __global float *scores,
    __global const int64_t *cfg, __global const uint32_t *status) {
    uint32_t tile, step, row, lane;
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(row));
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(tile));
    __asm__("mov.u32 %0, %%nctaid.x;" : "=r"(step));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    dsa_fallback_score_g((uint64_t)q, (uint64_t)k, (uint64_t)weights, (uint64_t)mask,
        (uint64_t)scores, (uint64_t)cfg, (uint64_t)status, row, tile, step, lane);
}

__kernel void goose_dsa_fallback_top(__global const float *scores, __global uint32_t *ids,
    __global const uint32_t *status, int64_t cells) {
    __local uint32_t bins[16];
    __local uint64_t keys[2048];
    uint32_t row, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(row));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    dsa_fallback_top_g((uint64_t)scores, (uint64_t)ids, (uint64_t)status, cells, row, lane,
        (uint64_t)bins, (uint64_t)keys);
}
