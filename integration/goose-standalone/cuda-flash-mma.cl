#include "flash-mma-types.h"

__asm__(".extern .shared .align 16 .b8 goose_flash_shared[];");

static void fg175l_cp16(uint64_t dst, uint64_t src) {
    fg_cuda_cp16(dst, src);
}

static void fg_flash_mask_half(uint64_t dst, uint32_t bits) { *(__local uint16_t *) dst = (uint16_t) bits; }
static void fg_flash_half_store(uint64_t dst, float v) { *(__global uint16_t *) dst = mmq_half_bits(v); }
static F2_g fg_flash_unpack(uint64_t block, int64_t pair) { return dequant_q4_0_g(block, pair); }

#include "flash-load-device.h"
#include "flash-stage-device.h"

static void fg175l_load_kv(uint64_t src, uint64_t dst, int64_t stride, int64_t row, int32_t lane, int32_t warp) {
    fg_flash_load_kv_g(src, dst, stride, row, lane, warp);
}
static void fg175l_load_mask(uint64_t first, uint64_t dst, int64_t stride, int64_t row, int64_t query,
                            int64_t queries, int32_t cols, int32_t lane, int32_t warp) {
    (void) stride;
    fg_flash_load_causal_g(first, dst, row, query, queries, cols, lane, warp);
}

static uint32_t fg175c_fg176i_bits(float v) { return as_uint(v); }
static float fg175c_fg176i_frombits(uint32_t v) { return as_float(v); }
static uint32_t fg175c_fg176i_ldu(uint64_t p) { return *(__local uint32_t *) p; }
static void fg175c_fg176i_stu(uint64_t p, uint32_t v) { *(__local uint32_t *) p = v; }
static float fg175c_fg176i_hlo(uint32_t w) { return fg175q_h2f(w); }
static float fg175c_fg176i_hhi(uint32_t w) { return fg175q_h2f(w >> 16); }
static uint32_t fg175c_fg176i_h2(float a, float b) {
    return (uint32_t) mmq_half_bits(a) | ((uint32_t) mmq_half_bits(b) << 16);
}
static uint32_t fg175c_fg176i_mulh2(uint32_t a, uint32_t b) {
    uint32_t out;
    __asm__("mul.rn.f16x2 %0, %1, %2;" : "=r"(out) : "r"(a), "r"(b));
    return out;
}
static float fg175c_fg176i_expf(float v) { return fg_attn_exp(v); }
static float fg175c_fg176i_fmaf(float a, float b, float c) { return fg_device_fma(a, b, c); }
static float fg175c_fg176i_shflx(float x, int32_t d) { return fg_device_shfl(x, d); }
static void fg175c_fg176i_cpwait(void) { fg_cuda_cpwait(); }
static void fg175c_fg176i_barrier(void) { fg_device_barrier(); }

static Flash_U4_g fg175c_fg176i_ldm4(uint64_t address) {
    const U4_g r = fg174m_ldm4(address);
    return (Flash_U4_g) {r.a_g, r.b_g, r.c_g, r.d_g};
}
static Flash_U4_g fg175c_fg176i_ldm4t(uint64_t address) {
    const U4_g r = fg174m_ldm4t(address);
    return (Flash_U4_g) {r.a_g, r.b_g, r.c_g, r.d_g};
}
static Flash_F4_g fg175c_fg176i_mma_f32(Flash_F4_g d, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3,
                                       uint32_t b0, uint32_t b1) {
    __asm__ volatile("mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32 "
                     "{%0,%1,%2,%3}, {%4,%5,%6,%7}, {%8,%9}, {%0,%1,%2,%3};"
        : "+f"(d.x_g), "+f"(d.y_g), "+f"(d.z_g), "+f"(d.w_g)
        : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(b0), "r"(b1));
    return d;
}
static Flash_U2_g fg175c_fg176i_mma_f16(Flash_U2_g d, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3,
                                       uint32_t b0, uint32_t b1) {
    __asm__ volatile("mma.sync.aligned.m16n8k16.row.col.f16.f16.f16.f16 "
                     "{%0,%1}, {%2,%3,%4,%5}, {%6,%7}, {%0,%1};"
        : "+r"(d.a_g), "+r"(d.b_g) : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(b0), "r"(b1));
    return d;
}

static float fg175c_fg175p_gload(uint64_t p, uint64_t i) { return fg_device_global_load(p + 4 * i); }
static void fg175c_fg175p_gstore(uint64_t p, uint64_t i, float v) { fg_device_global_store(p + 4 * i, v); }
static uint32_t fg175c_fg175p_smld(uint64_t p, uint64_t i) { return ((__local uint32_t *) p)[i]; }
static void fg175c_fg175p_smst(uint64_t p, uint64_t i, uint32_t v) { ((__local uint32_t *) p)[i] = v; }
static float fg175c_fg175p_smldf(uint64_t p, uint64_t i) { return ((__local float *) p)[i]; }
static void fg175c_fg175p_smstf(uint64_t p, uint64_t i, float v) { ((__local float *) p)[i] = v; }
static float fg168f_ld_f32(uint64_t p, uint64_t off) { return fg_device_global_load(p + off); }
static void fg168f_st_f32(uint64_t p, uint64_t off, float v) { fg_device_global_store(p + off, v); }
static float fg168f_expf(float v) { return fg_attn_exp(v); }
static float fg168f_fmaxf(float a, float b) { return fg_vector_max(a, b); }
static void fg168f_sync(void) {}
static void fg168f_lc(void) {}
#define fg175c_fg175p_h2mul fg175c_fg176i_mulh2
#define fg175c_fg175p_h2pack fg175c_fg176i_h2

#define GS_NOINLINE __attribute__((noinline))
#include "flash-mma-device.h"
#undef GS_NOINLINE
#undef fg175c_fg175p_h2mul
#undef fg175c_fg175p_h2pack

__kernel void goose_flash_q4_stage(__global const uint8_t * src, __global uint16_t * dst,
    int64_t keys, int64_t padded, int64_t row_stride, int64_t head_stride) {
    uint32_t row, head, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(row));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(head));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    fg_flash_stage_g((uint64_t) src, (uint64_t) dst, keys, padded, row_stride, head_stride, row, head, lane);
}

__kernel void goose_flash_causal_mma(__global const float * q, __global const uint16_t * k,
    __global const uint16_t * v, __global float * out, __global const int64_t * cfg, float scale) {
    uint32_t tile, head, thread;
    uint64_t shared;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(tile));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(head));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    __asm__("mov.u64 %0, goose_flash_shared;" : "=l"(shared));
    fg_flash_causal_mma_g((uint64_t) q, (uint64_t) k, (uint64_t) v, (uint64_t) out,
        (uint64_t) cfg, scale, tile, head, thread % 32, thread / 32, shared);
}

__kernel void goose_flash_stream_mma(__global const float * q, __global const uint16_t * k,
    __global const uint16_t * v, __global float * out, __global float * fix,
    __global const int64_t * cfg, float scale) {
    uint32_t worker, workers, thread;
    uint64_t shared;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(worker));
    __asm__("mov.u32 %0, %%nctaid.x;" : "=r"(workers));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    __asm__("mov.u64 %0, goose_flash_shared;" : "=l"(shared));
    fg_flash_stream_mma_g((uint64_t) q, (uint64_t) k, (uint64_t) v, (uint64_t) out,
        (uint64_t) fix, (uint64_t) cfg, scale, worker, workers, thread % 32, thread / 32, shared);
}

__kernel void goose_flash_fixup_uniform(__global float * out, __global const float * fix,
    int32_t queries, int32_t heads, int32_t workers, int32_t blocks_per_tile) {
    uint32_t tile, query, head, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(tile));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(query));
    __asm__("mov.u32 %0, %%ctaid.z;" : "=r"(head));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    flash_launch_uniform_g((uint64_t)out, (uint64_t)fix, queries, heads, workers, blocks_per_tile, tile, query, head, lane);
}

__kernel void goose_flash_fixup_general(__global float * out, __global const float * fix,
    int32_t queries, int32_t heads, int32_t keys) {
    uint32_t worker, workers, query, head, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(worker));
    __asm__("mov.u32 %0, %%nctaid.x;" : "=r"(workers));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(query));
    __asm__("mov.u32 %0, %%ctaid.z;" : "=r"(head));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    flash_launch_general_g((uint64_t)out, (uint64_t)fix, queries, heads, keys, worker, query, head, workers, lane);
}
