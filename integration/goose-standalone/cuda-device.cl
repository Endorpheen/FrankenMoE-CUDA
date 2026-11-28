#include "cuda-intrinsic-aliases.h"
// OpenCL C bindings for operations required by the Goose-emitted device code.
typedef uchar uint8_t;
typedef ushort uint16_t;
typedef uint uint32_t;
typedef ulong uint64_t;
typedef char int8_t;
typedef int int32_t;
typedef long int64_t;

static void device_trap(void) {
    __asm__ volatile("trap;");
    __builtin_unreachable();
}

static int64_t device_range(int64_t value, int64_t lo, int64_t hi) {
    if (value < lo || value > hi) device_trap();
    return value;
}

static uint64_t device_range_u(uint64_t value, uint64_t hi) {
    if (value > hi) device_trap();
    return value;
}

static uint64_t device_udiv(uint64_t a, uint64_t b) {
    if (b == 0) device_trap();
    return a / b;
}

static uint64_t device_umod(uint64_t a, uint64_t b) {
    if (b == 0) device_trap();
    return a % b;
}

static int64_t device_add(int64_t a, int64_t b) {
    int64_t result;
    if (__builtin_add_overflow(a, b, &result)) device_trap();
    return result;
}

static int64_t device_sub(int64_t a, int64_t b) {
    int64_t result;
    if (__builtin_sub_overflow(a, b, &result)) device_trap();
    return result;
}

static int64_t device_mul(int64_t a, int64_t b) {
    int64_t result;
    if (__builtin_mul_overflow(a, b, &result)) device_trap();
    return result;
}

static uint64_t device_uadd(uint64_t a, uint64_t b) {
    uint64_t result;
    if (__builtin_add_overflow(a, b, &result)) device_trap();
    return result;
}

static int64_t device_div(int64_t a, int64_t b) {
    if (a < 0 || b <= 0) device_trap();
    return a / b;
}

static int64_t device_mod(int64_t a, int64_t b) {
    if (a < 0 || b <= 0) device_trap();
    return a % b;
}

#define GS_RANGE(value, lo, hi, type, file, line) device_range(value, lo, hi)
#define GS_RANGE_U(value, hi, type, file, line) device_range_u(value, hi)
#define GS_IDX(value, size, file, line) device_range(value, 0, (size) - 1)
#define GS_MEMZERO(at, bytes) __builtin_memset(at, 0, bytes)
#define GS_NOINLINE __attribute__((noinline))
#define GS_UNREACHABLE(file, line) device_trap()
#define gs_add_i64(a, b, file, line) device_add(a, b)
#define gs_sub_i64(a, b, file, line) device_sub(a, b)
#define gs_mul_i64(a, b, file, line) device_mul(a, b)
#define gs_add_u64(a, b) device_uadd(a, b)
#define gs_div_i64(a, b, file, line) device_div(a, b)
#define gs_mod_i64(q, a, b, file, line) device_mod(a, b)
#define gs_div_u64(a, b, file, line) device_udiv(a, b)
#define gs_mod_u64(a, b, file, line) device_umod(a, b)
#define gs_sub_u64(a, b) ((uint64_t)(a) - (uint64_t)(b))
#define gs_mul_u64(a, b) ((uint64_t)(a) * (uint64_t)(b))
#define gs_mul_u32(a, b) ((uint32_t)(a) * (uint32_t)(b))
#define gs_sub_u32(a, b) ((uint32_t)(a) - (uint32_t)(b))
#define gs_div_i32(a, b, file, line) ((int32_t)device_div(a, b))
#define gs_mod_i32(q, a, b, file, line) ((int32_t)device_mod(a, b))
#define gs_add_i32(a, b, file, line) ((int32_t)((int64_t)(a) + (int64_t)(b)))
#define gs_sub_i32(a, b, file, line) ((int32_t)((int64_t)(a) - (int64_t)(b)))
#define gs_mul_i32(a, b, file, line) ((int32_t)((int64_t)(a) * (int64_t)(b)))
#define gs_shl_i64(a,b) ((int64_t)((uint64_t)(a) << ((b)&63)))
#define gs_shr_i64(a,b) ((a) >> ((b)&63))
#define gs_shr_i32(a,b) ((a) >> ((b)&31))
#define gs_shr_u64(a,b) ((a) >> ((b)&63))
#define gs_shr_u32(a,b) ((a) >> ((b)&31))
#define gs_shl_u32(a,b) ((a) << ((b)&31))

static uint32_t fg248d_ld_u8(uint64_t address) { return *(__global uint8_t *) address; }
static int32_t fg248d_ld_i8(uint64_t address) { return *(__global int8_t *) address; }

static float fg248d_ld_half(uint64_t address) {
    const uint16_t bits = *(__global uint16_t *) address;
    float result;
    __asm__("cvt.f32.f16 %0, %1;" : "=f"(result) : "h"(bits));
    return result;
}

static void fg248d_st_f32(uint64_t address, float value) { *(__local float *) address = value; }
static void fg248d_fail(int64_t code) { (void) code; device_trap(); }

static uint32_t fg182d_ld_u8(uint64_t address, uint64_t offset) { return fg248d_ld_u8(address + offset); }
static float fg182d_ld_half(uint64_t address, uint64_t offset) { return fg248d_ld_half(address + offset); }
static void fg182d_st_f32(uint64_t address, uint64_t offset, float value) { fg248d_st_f32(address + offset, value); }

#include "q4k-device.h"
#include "q6k-device.h"

static void fg_device_decode_q4(uint64_t w, uint64_t scratch, int32_t lane) {
    fg182d_q4_K_g(w, scratch, 0, lane);
}
static void fg_device_decode_q6(uint64_t w, uint64_t scratch, int64_t lane) {
    q6_K_g(w, scratch, 0, lane);
}
static float fg_device_global_load(uint64_t at) { return *(__global float *) at; }
static void fg_device_global_store(uint64_t at, float v) { *(__global float *) at = v; }
static float fg_device_shared_load(uint64_t at) { return *(__local float *) at; }
static void fg_device_shared_store(uint64_t at, float v) { *(__local float *) at = v; }
static void fg_device_barrier(void) { __asm__ volatile("bar.sync 0;" ::: "memory"); }
static float fg_device_fma(float a, float b, float sum) {
    float result;
    __asm__("fma.rn.f32 %0, %1, %2, %3;" : "=f"(result) : "f"(a), "f"(b), "f"(sum));
    return result;
}

static float fg_device_fma_ftz(float a, float b, float sum) {
    float result;
    __asm__("fma.rn.ftz.f32 %0, %1, %2, %3;" : "=f"(result) : "f"(a), "f"(b), "f"(sum));
    return result;
}

static float fg_device_shfl(float value, int32_t delta) {
    uint32_t result;
    __asm__ volatile("shfl.sync.bfly.b32 %0, %1, %2, 31, -1;"
                     : "=r"(result) : "r"(as_uint(value)), "r"(delta));
    return as_float(result);
}

static float fg_device_mul_rn(float a, float b) {
    float result;
    __asm__("mul.rn.f32 %0, %1, %2;" : "=f"(result) : "f"(a), "f"(b));
    return result;
}

static uint64_t fg_device_matmul_job_word(uint64_t jobs, uint64_t index) {
    return ((__global const uint64_t *)jobs)[index];
}

#include "matmul-device.h"

static uint32_t fg175q_ld_u8(uint64_t at, uint64_t offset) {
    return fg248d_ld_u8(at + offset);
}

static uint32_t fg175q_ld_u16(uint64_t at, uint64_t offset) {
    return *(__global uint16_t *) (at + offset);
}

static float fg175q_h2f(uint32_t bits) {
    float result;
    __asm__("cvt.f32.f16 %0, %1;" : "=f"(result) : "h"((uint16_t) bits));
    return result;
}

static int64_t fg_attn_word(uint64_t config, int64_t index) {
    return ((__global int64_t *) config)[index];
}


static float fg_attn_exp(float value) {
    float result;
    __asm__("ex2.approx.ftz.f32 %0, %1;" : "=f"(result) : "f"(value * 1.4426950408889634f));
    return result;
}

static float fg_device_reciprocal(float value) {
    float reciprocal;
    __asm__("rcp.approx.ftz.f32 %0, %1;" : "=f"(reciprocal) : "f"(value));
    return reciprocal;
}

static float fg_attn_neg_inf(void) {
    return as_float(0xff800000u);
}

#ifndef GOOSE_CUDA_FROZEN_ABI
static void fg_dsa_store_u16(uint64_t base, int64_t index, uint32_t value) {
    ((__global uint16_t *)base)[index] = (uint16_t)value;
}
static int32_t fg_dsa_load_i32(uint64_t base, int64_t index) {
    return ((__global const int32_t *)base)[index];
}
static uint32_t fg_mla_mma_kpair(uint64_t base) {
    return *(__global const uint32_t *)base;
}
static uint32_t fg_mla_mma_qpair(uint64_t base) {
    const __global float *p = (const __global float *)base;
    uint16_t a, b;
    __asm__("cvt.rn.f16.f32 %0, %1;" : "=h"(a) : "f"(p[0]));
    __asm__("cvt.rn.f16.f32 %0, %1;" : "=h"(b) : "f"(p[1]));
    return (uint32_t)a | ((uint32_t)b << 16);
}
#ifdef GOOSE_CUDA_MMQ_IMAGE
#define fg_mla_mma_f16(acc,a0,a1,a2,a3,b0,b1) ({ \
    __typeof__(acc) d = (acc); \
    __asm__ volatile("mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32 " \
        "{%0,%1,%2,%3}, {%4,%5,%6,%7}, {%8,%9}, {%0,%1,%2,%3};" \
        : "+f"(d.a_g), "+f"(d.b_g), "+f"(d.c_g), "+f"(d.d_g) \
        : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(b0), "r"(b1)); d; })
#else
#define fg_mla_mma_f16(acc,a0,a1,a2,a3,b0,b1) (acc)
#endif
static void fg_mla_score_put(uint64_t base, int64_t index, float value) {
    ((__local float *)base)[index] = value;
}
static float fg_mla_score_get(uint64_t base, int64_t index) {
    return ((__local const float *)base)[index];
}
static void fg_mla_score_sync(void) {
    __asm__ volatile("bar.sync 0;" ::: "memory");
}
static uint32_t fg_mla_pack_probability(float a, float b) {
    uint16_t lo, hi;
    __asm__("cvt.rn.f16.f32 %0, %1;" : "=h"(lo) : "f"(a));
    __asm__("cvt.rn.f16.f32 %0, %1;" : "=h"(hi) : "f"(b));
    return (uint32_t)lo | ((uint32_t)hi << 16);
}
static uint64_t fg_dsa_top_shift(uint64_t value, int64_t bits) { return value << bits; }
static uint32_t fg_dsa_top_bits(uint64_t base, int64_t index) {
    return ((__global const uint32_t *)base)[index];
}
static void fg_dsa_bins_clear(uint64_t base, int64_t index) {
    ((__local uint32_t *)base)[index] = 0;
}
static uint32_t fg_dsa_bins_get(uint64_t base, int64_t index) {
    return ((__local const uint32_t *)base)[index];
}
static uint32_t fg_dsa_bins_add(uint64_t base, int64_t index) {
    uint32_t old;
    __asm__ volatile("atom.shared.add.u32 %0, [%1], 1;" : "=r"(old) :
        "r"((uint32_t)(base + 4*index)) : "memory");
    return old;
}
static void fg_dsa_top_put_key(uint64_t base, int64_t index, uint64_t value) {
    ((__local uint64_t *)base)[index] = value;
}
static uint64_t fg_dsa_top_get_key(uint64_t base, int64_t index) {
    return ((__local const uint64_t *)base)[index];
}
static void fg_dsa_top_store(uint64_t base, int64_t index, uint32_t value) {
    ((__global uint32_t *)base)[index] = value;
}
static uint32_t fg_mla_any_visible(uint32_t seen) {
    uint32_t count;
    __asm__ volatile("{ .reg .pred visible; setp.ne.u32 visible, %1, 0;"
        " bar.red.popc.u32 %0, 0, visible; }" : "=r"(count) : "r"(seen) : "memory");
    return count;
}
static uint32_t fg_mla_visible_ballot(uint32_t seen) {
    uint32_t vote;
    __asm__ volatile("{ .reg .pred selected; setp.ne.u32 selected, %1, 0;"
        " vote.sync.ballot.b32 %0, selected, 0xffffffff; }" : "=r"(vote) : "r"(seen));
    return vote;
}
static uint32_t fg_mla_mask_popcount(uint32_t word) {
    uint32_t count;
    __asm__("popc.b32 %0, %1;" : "=r"(count) : "r"(word));
    return count;
}
static void fg_mla_store_index(uint64_t base, int64_t index, int32_t value) {
    ((__global int32_t *)base)[index] = value;
}
#endif
#include "q4-attention-device.h"
#undef fg_mla_mma_f16
#include "cuda-attention-entries.h"




static float fg174s_shfl_xor(float value, int32_t delta, int32_t width) {
    uint32_t result;
    const int32_t bound = ((32 - width) << 8) | 31;
    __asm__ volatile("shfl.sync.bfly.b32 %0, %1, %2, %3, -1;"
                     : "=r"(result) : "r"(as_uint(value)), "r"(delta), "r"(bound));
    return as_float(result);
}

// Eight pointer words precede the Goose-generated strides and launch plan.
typedef struct { uint64_t words[57]; } gdn_launch_packet;

static uint64_t fg_gdn_read_word(int64_t packet, int64_t index) {
    return ((const uint64_t *)packet)[index];
}


#include "gdn-device.h"

static float fg176r_ld_f32(uint64_t src, uint64_t at) {
    float value;
    __asm__ volatile("ld.f32 %0, [%1];" : "=f"(value) : "l"(src + at) : "memory");
    return value;
}

static void fg176r_st_f16(uint64_t dst, uint64_t at, float value) {
    uint16_t bits;
    __asm__("cvt.rn.f16.f32 %0, %1;" : "=h"(bits) : "f"(value));
    *(__global uint16_t *) (dst + at) = bits;
}

static void fg176r_st_u8(uint64_t dst, uint64_t at, uint32_t value) {
    *(__global uint8_t *) (dst + at) = (uint8_t) value;
}

static float fg176r_fabs(float value) { return as_float(as_uint(value) & 0x7fffffffu); }

static int32_t fg176r_i8trunc(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0;
    return (int8_t) (int32_t) value;
}

#pragma OPENCL FP_CONTRACT OFF
#include "q4-set-rows-device.h"

static float fg_vector_log(float value) {
    float result;
    __asm__("lg2.approx.f32 %0, %1;" : "=f"(result) : "f"(value));
    return result * 0.6931471805599453f;
}

static float fg_vector_sqrt(float value) {
    float result;
    __asm__("sqrt.rn.f32 %0, %1;" : "=f"(result) : "f"(value));
    return result;
}

static float fg_vector_min(float a, float b) {
    float result;
    __asm__("min.f32 %0, %1, %2;" : "=f"(result) : "f"(a), "f"(b));
    return result;
}

static float fg_vector_max(float a, float b) {
    float result;
    __asm__("max.f32 %0, %1, %2;" : "=f"(result) : "f"(a), "f"(b));
    return result;
}

static double fg_vector_shuffle_f64(double value, int32_t delta) {
    uint64_t bits = as_ulong(value);
    const uint32_t lo = as_uint(fg_device_shfl(as_float((uint32_t) bits), delta));
    const uint32_t hi = as_uint(fg_device_shfl(as_float((uint32_t) (bits >> 32)), delta));
    return as_double(((uint64_t) hi << 32) | lo);
}

// Keep the validated shape words in CUDA parameter space. Both vector entries
// consume the same plan layout without a separate upload to device memory.
typedef struct { int64_t words[26]; } vector_launch_packet;
static int64_t fg_vector_word(uint64_t cfg, int64_t index) {
    return ((const int64_t *) cfg)[index];
}

typedef struct { float x_g, y_g, z_g, w_g; } CVVec4_g;
static CVVec4_g fg_vector_load4(uint64_t address) {
    const float4 value = *(__global const float4 *) address;
    return (CVVec4_g) {value.x, value.y, value.z, value.w};
}
static void fg_vector_store4(uint64_t address, CVVec4_g v) {
    *(__global float4 *) address = (float4) (v.x_g, v.y_g, v.z_g, v.w_g);
}

#ifndef GOOSE_CUDA_FROZEN_ABI
static double fg_glu_pow2_int(int64_t exponent) {
    return as_double((uint64_t)(exponent + 1023) << 52);
}

static int64_t fg_glu_round(double value) {
    int64_t result;
    __asm__("cvt.rni.s64.f64 %0, %1;" : "=l"(result) : "d"(value));
    return result;
}

static float fg_glu_div_rn(float a, float b) {
    float result;
    __asm__("div.rn.f32 %0, %1, %2;" : "=f"(result) : "f"(a), "f"(b));
    return result;
}

#endif
#include "vector-device.h"
#include "cuda-rms-reference.cl"

static float fg_rope_float(uint64_t cfg, int64_t i) {
    return as_float((uint32_t) fg_attn_word(cfg, i));
}

static int32_t fg_rope_position(uint64_t positions, int64_t i) {
    return ((__global int32_t *) positions)[i];
}

static void fg_rope_copy32(uint64_t src, uint64_t dst) { *(__global uint32_t *)dst = *(__global uint32_t *)src; }
static void fg_rope_copy16(uint64_t src, uint64_t dst) { *(__global uint16_t *)dst = *(__global uint16_t *)src; }
static int64_t fg_rope_round(double value) {
    int64_t result;
    __asm__("cvt.rni.s64.f64 %0, %1;" : "=l"(result) : "d"(value));
    return result;
}
static double fg_rope_fma64(double a, double b, double sum) {
    double result;
    __asm__("fma.rn.f64 %0, %1, %2, %3;" : "=d"(result) : "d"(a), "d"(b), "d"(sum));
    return result;
}

static float fg_rope_sin(float theta) {
    float result;
    __asm__("sin.approx.f32 %0, %1;" : "=f"(result) : "f"(theta));
    return result;
}

static float fg_rope_cos(float theta) {
    float result;
    __asm__("cos.approx.f32 %0, %1;" : "=f"(result) : "f"(theta));
    return result;
}

#include "rope-device.h"

static float fg170s_load(uint64_t address, uint64_t i) { return fg_device_global_load(address + 4 * i); }
static void fg170s_store(uint64_t address, uint64_t i, float value) { fg_device_global_store(address + 4 * i, value); }
static void fg170s_lc(void) {}
static void fg170s_sync(void) {}
static float fg170s_shared_load(uint64_t address, uint64_t i) { return ((__local float *) address)[i]; }
static void fg170s_shared_store(uint64_t address, uint64_t i, float value) {
    ((__local float *) address)[i] = value;
}

#include "ssm-conv-device.h"

static float fg283d_ld_f32(uint64_t src, uint64_t offset) {
    return fg_device_global_load(src + offset);
}
static void fg283d_st_i32(uint64_t out, uint64_t offset, int32_t value) {
    *(__global int32_t *) (out + offset) = value;
}
static int32_t fg283d_sload_i32(uint64_t at) { return *(__local int32_t *) at; }
static void fg283d_sstore_i32(uint64_t at, int32_t value) { *(__local int32_t *) at = value; }
static int32_t fg169t_ishuffle(int32_t value, int32_t offset) {
    return as_int(fg_device_shfl(as_float(value), offset));
}

#include "routing-device.h"

static void fg_device_copy_u32(uint64_t src, uint64_t dst) {
    *(__global uint32_t *) dst = *(__global uint32_t *) src;
}

static float fg434c_ld_f32(uint64_t base, uint64_t offset) {
    float value;
    __asm__ volatile("ld.f32 %0, [%1];" : "=f"(value) : "l"(base + offset) : "memory");
    return value;
}

static void fg434c_st_f32(uint64_t base, uint64_t offset, float value) {
    __asm__ volatile("st.f32 [%0], %1;" :: "l"(base + offset), "f"(value) : "memory");
}


#include "transform-device.h"

static float fg183f_ld_f32(uint64_t base, uint64_t byte) {
    return *(__global const float *) (base + byte);
}

static void fg183f_st_f32(uint64_t base, uint64_t byte, float value) {
    __asm__ volatile("st.f32 [%0], %1;" :: "l"(base + byte), "f"(value) : "memory");
}

static float fg183f_shfl_xor(float value, uint32_t mask, int32_t delta, int32_t width) {
    (void) mask;
    (void) width;
    return fg_device_shfl(value, delta);
}

static void fg183f_fail(int64_t code) {
    (void) code;
    device_trap();
}

static void fg_fwht_warp_sync(void) { __asm__ volatile("bar.warp.sync -1;" ::: "memory"); }
#include "fwht-device.h"

#include "cuda-fwht-entries.h"

#define fg179w_ld_f32 fg183f_ld_f32
static void fg179w_st_f32(uint64_t base, uint64_t byte, float value) {
    fg_device_global_store(base + byte, value);
}
#define fg179w_fail fg183f_fail
#pragma OPENCL FP_CONTRACT OFF
#include "moe-weighted-device.h"
#pragma OPENCL FP_CONTRACT ON
#undef fg179w_ld_f32
#undef fg179w_fail

#include "cuda-activation-entries.h"
#include "cuda-apex-epilogue-entries.h"

#ifdef GOOSE_CUDA_MMQ_IMAGE
static void fg_cuda_cp16(uint64_t dst, uint64_t src) {
    __asm__ volatile("cp.async.cg.shared.global [%0], [%1], 16;" :: "r"((uint32_t) dst), "l"(src) : "memory");
}
static void fg_cuda_cpwait(void) { __asm__ volatile("cp.async.wait_all;" ::: "memory"); }

#include "cuda-mmq.cl"
#if defined(GOOSE_APEX_UP_GATE_IMAGE)
static uint64_t apex_up_gate_word(uint64_t jobs, int64_t index) {
    return ((__global const uint64_t *)jobs)[index];
}
#endif
#include "cuda-mmvq.cl"
#include "cuda-dense-mma.cl"
#include "cuda-gdn-mma.cl"
#include "cuda-flash-mma.cl"
#include "cuda-flash-vec.cl"
#endif

#include "cuda-up-gate-entries.h"
