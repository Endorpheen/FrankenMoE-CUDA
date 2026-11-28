#include "flash-mma-types.h"


static void fg_flash_mask_half(uint64_t dst, uint32_t bits) { *(__local uint16_t *) dst = (uint16_t) bits; }
static void fg_flash_half_store(uint64_t dst, float v) { *(__global uint16_t *) dst = mmq_half_bits(v); }

#include "flash-load-device.h"
#include "flash-stage-device.h"

static void fg175l_load_mask(uint64_t first, uint64_t dst, int64_t stride, int64_t row, int64_t query,
                            int64_t queries, int32_t cols, int32_t lane, int32_t warp) {
    (void) stride;
    fg_flash_load_causal_g(first, dst, row, query, queries, cols, lane, warp);
}

static uint32_t fg175c_fg176i_ldu(uint64_t p) { return *(__local uint32_t *) p; }
static void fg175c_fg176i_stu(uint64_t p, uint32_t v) { *(__local uint32_t *) p = v; }
static float fg175c_fg176i_hhi(uint32_t w) { return fg175q_h2f(w >> 16); }
static uint32_t fg175c_fg176i_h2(float a, float b) {
    return (uint32_t) mmq_half_bits(a) | ((uint32_t) mmq_half_bits(b) << 16);
}
static uint32_t fg175c_fg176i_mulh2(uint32_t a, uint32_t b) {
    uint32_t out;
    __asm__("mul.rn.f16x2 %0, %1, %2;" : "=r"(out) : "r"(a), "r"(b));
    return out;
}

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
static void fg168f_sync(void) {}
static void fg168f_lc(void) {}
#define fg175c_fg175p_h2mul fg175c_fg176i_mulh2
#define fg175c_fg175p_h2pack fg175c_fg176i_h2

#define GS_NOINLINE __attribute__((noinline))
#include "flash-mma-device.h"
#undef GS_NOINLINE
#undef fg175c_fg175p_h2mul
#undef fg175c_fg175p_h2pack

#include "cuda-flash-mma-entries.h"
