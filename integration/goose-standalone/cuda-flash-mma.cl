#include "flash-mma-types.h"


static void fg_flash_half_store(uint64_t dst, float v) { *(__global uint16_t *) dst = mmq_half_bits(v); }

#include "flash-load-device.h"
#include "flash-stage-device.h"

static void fg175l_load_mask(uint64_t first, uint64_t dst, int64_t stride, int64_t row, int64_t query,
                            int64_t queries, int32_t cols, int32_t lane, int32_t warp) {
    (void) stride;
    fg_flash_load_causal_g(first, dst, row, query, queries, cols, lane, warp);
}

static float fg175c_fg176i_hhi(uint32_t w) { return fg175q_h2f(w >> 16); }
static uint32_t fg175c_fg176i_h2(float a, float b) {
    return (uint32_t) mmq_half_bits(a) | ((uint32_t) mmq_half_bits(b) << 16);
}
#include "cuda-flash-mma-ptx-0.h"
static Flash_U4_g fg175c_fg176i_ldm4(uint64_t address) {
    const U4_g r = fg174m_ldm4(address);
    return (Flash_U4_g) {r.a_g, r.b_g, r.c_g, r.d_g};
}
static Flash_U4_g fg175c_fg176i_ldm4t(uint64_t address) {
    const U4_g r = fg174m_ldm4t(address);
    return (Flash_U4_g) {r.a_g, r.b_g, r.c_g, r.d_g};
}
#include "cuda-flash-mma-matrix-ptx-0.h"
static float fg175c_fg175p_gload(uint64_t p, uint64_t i) { return fg_device_global_load(p + 4 * i); }
static void fg175c_fg175p_gstore(uint64_t p, uint64_t i, float v) { fg_device_global_store(p + 4 * i, v); }
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
