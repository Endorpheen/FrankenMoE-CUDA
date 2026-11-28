#include "gdn-mma-types.h"

static float fg174m_load(uint64_t p, uint64_t i) { return fg_device_global_load(p + 4 * i); }
static void fg174m_store(uint64_t p, uint64_t i, float v) { fg_device_global_store(p + 4 * i, v); }
static F4_g fg174m_load4(uint64_t p, uint64_t i) {
    const float4 v = *(__global const float4 *) (p + 4 * i);
    return (F4_g) {v.x, v.y, v.z, v.w};
}
static float fg174m_shfl_up(float v, int32_t delta) {
    uint32_t r;
    __asm__ volatile("shfl.sync.up.b32 %0, %1, %2, 0, -1;" : "=r"(r) : "r"(as_uint(v)), "r"(delta));
    return as_float(r);
}
static float fg174m_shfl(float v, int32_t lane) {
    uint32_t r;
    __asm__ volatile("shfl.sync.idx.b32 %0, %1, %2, 31, -1;" : "=r"(r) : "r"(as_uint(v)), "r"(lane));
    return as_float(r);
}
static void fg174m_sm_store_u32(uint64_t p, uint64_t i, uint32_t v) { ((__local uint32_t *) p)[i] = v; }
static float fg174m_sm_f32(uint64_t p, uint64_t i) { return ((__local float *) p)[i]; }
static void fg174m_sm_store_f32(uint64_t p, uint64_t i, float v) { ((__local float *) p)[i] = v; }
static float fg174m_sm_bf16(uint64_t p, uint64_t i) {
    return as_float((uint32_t) ((__local uint16_t *) p)[i] << 16);
}
static void fg174m_sm_store_bf16(uint64_t p, uint64_t i, float v) {
    uint16_t bits;
    __asm__("cvt.rn.bf16.f32 %0, %1;" : "=h"(bits) : "f"(v));
    ((__local uint16_t *) p)[i] = bits;
}
static uint32_t fg174m_cvt2(float a, float b) {
    uint32_t bits;
    __asm__("cvt.rn.bf16x2.f32 %0, %2, %1;" : "=r"(bits) : "f"(a), "f"(b));
    return bits;
}
static float fg174m_wlo(uint32_t w) { return as_float(w << 16); }
static float fg174m_whi(uint32_t w) { return as_float(w & 0xffff0000u); }

#define GDN_LDM4(name, mode) \
    static U4_g name(uint64_t a) { \
        U4_g r; \
        __asm__ volatile("ldmatrix.sync.aligned.m8n8.x4" mode ".shared.b16 {%0, %1, %2, %3}, [%4];" \
            : "=r"(r.a_g), "=r"(r.b_g), "=r"(r.c_g), "=r"(r.d_g) : "r"((uint32_t) a) : "memory"); \
        return r; \
    }
#define GDN_LDM2(name, mode) \
    static U2_g name(uint64_t a) { \
        U2_g r; \
        __asm__ volatile("ldmatrix.sync.aligned.m8n8.x2" mode ".shared.b16 {%0, %1}, [%2];" \
            : "=r"(r.a_g), "=r"(r.b_g) : "r"((uint32_t) a) : "memory"); \
        return r; \
    }
GDN_LDM4(fg174m_ldm4, "")
GDN_LDM4(fg174m_ldm4t, ".trans")
GDN_LDM2(fg174m_ldm2, "")
GDN_LDM2(fg174m_ldm2t, ".trans")
#undef GDN_LDM4
#undef GDN_LDM2

static F4_g fg174m_mma(F4_g d, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t b0, uint32_t b1) {
    __asm__ volatile("mma.sync.aligned.m16n8k16.row.col.f32.bf16.bf16.f32 "
                     "{%0, %1, %2, %3}, {%4, %5, %6, %7}, {%8, %9}, {%0, %1, %2, %3};"
        : "+f"(d.x_g), "+f"(d.y_g), "+f"(d.z_g), "+f"(d.w_g)
        : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(b0), "r"(b1));
    return d;
}

#include "gdn-mma-device.h"

#include "cuda-gdn-mma-entries.h"
