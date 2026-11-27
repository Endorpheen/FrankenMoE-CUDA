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
static void fg182d_fail(int64_t code) { fg248d_fail(code); }

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

#include "matmul-device.h"

__kernel void goose_quantized_matmul(
    __global uint8_t * weights, __global uint8_t * input, __global uint8_t * output,
    __global uint64_t * jobs, uint64_t row_stride, int32_t extent, int32_t kind, int32_t job_base,
    int32_t job_words) {
    uint32_t lane, row, job, lanes;
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(row));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(job));
    __asm__("mov.u32 %0, %%ntid.x;" : "=r"(lanes));
    __local float scratch[256];
    const uint64_t at = (uint64_t) job_words * ((uint64_t) job_base + job);
    const uint64_t plane = job_words == 3 ? jobs[at + 2] : 0;
    fg_device_matmul_g((uint64_t) weights + plane + row * row_stride, (uint64_t) input + jobs[at],
        (uint64_t) output + jobs[at + 1] + 4 * (uint64_t) row, (uint64_t) scratch,
        extent, kind, lane, lanes);
}

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

static float fg_attn_shfl(float value, int32_t delta) {
    return fg_device_shfl(value, delta);
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

#include "q4-attention-device.h"

__kernel void goose_q4_attention_tiles(__global uint8_t * q, __global uint8_t * k,
    __global uint8_t * v, __global uint8_t * mask, __global float * scratch,
    __global int64_t * cfg, float scale, int32_t query_base) {
    uint32_t task, tile, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(task));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(tile));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    fg_attn_tiles_g((uint64_t) q, (uint64_t) k, (uint64_t) v, (uint64_t) mask,
        (uint64_t) scratch, (uint64_t) cfg, scale, task, (int64_t) query_base + task, tile, lane);
}

__kernel void goose_q4_attention_merge(__global float * scratch, __global uint8_t * output,
    __global int64_t * cfg, int32_t query_base) {
    uint32_t task, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(task));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    fg_attn_merge_g((uint64_t) scratch, (uint64_t) output, (uint64_t) cfg,
        task, (int64_t) query_base + task, lane);
}

static float fg174s_load(uint64_t address, uint64_t index) {
    return fg_device_global_load(address + 4 * index);
}

static void fg174s_store(uint64_t address, uint64_t index, float value) {
    fg_device_global_store(address + 4 * index, value);
}

static float fg174s_expf(float value) { return fg_attn_exp(value); }

static float fg174s_fma(float a, float b, float sum) { return fg_device_fma(a, b, sum); }

static float fg174s_mul_rn(float a, float b) {
    return fg_device_mul_rn(a, b);
}

static float fg174s_shfl_xor(float value, int32_t delta, int32_t width) {
    uint32_t result;
    const int32_t bound = ((32 - width) << 8) | 31;
    __asm__ volatile("shfl.sync.bfly.b32 %0, %1, %2, %3, -1;"
                     : "=r"(result) : "r"(as_uint(value)), "r"(delta), "r"(bound));
    return as_float(result);
}

static uint32_t fg174s_umulhi(uint32_t a, uint32_t b) {
    return (uint32_t) (((uint64_t) a * b) >> 32);
}

// Eight pointer words precede the Goose-generated strides and launch plan.
typedef struct { uint64_t words[57]; } gdn_launch_packet;

static uint64_t fg174s_dget(int64_t packet, int64_t table, int64_t index) {
    const uint64_t * data = (const uint64_t *) packet;
    return data[(table == 0 ? 0 : table == 2 ? 8 : 32) + index];
}

static float fg174s_df(int64_t packet, int64_t index) {
    return as_float((uint32_t) ((const uint64_t *) packet)[49 + index]);
}

static uint32_t fg174s_dm(int64_t packet, int64_t row, int64_t index) {
    return (uint32_t) ((const uint64_t *) packet)[50 + 3 * row + index];
}

static void fg174s_sync(void) { fg_device_barrier(); }

#include "gdn-device.h"

__kernel void goose_gated_delta_net(gdn_launch_packet arguments) {
    const uint64_t * packet = arguments.words;
    uint32_t head, sequence, segment, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(head));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(sequence));
    __asm__("mov.u32 %0, %%ctaid.z;" : "=r"(segment));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    const uint32_t effective = (uint32_t) packet[43];
    gdn_g((int64_t) packet, head, sequence, segment, lane % effective, lane / effective);
}

__kernel void goose_gdn_decode(gdn_launch_packet arguments) {
    uint32_t head, sequence, segment, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(head));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(sequence));
    __asm__("mov.u32 %0, %%ctaid.z;" : "=r"(segment));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    gdn_decode_g((int64_t) arguments.words, head, sequence, segment, lane % 32, lane / 32);
}

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

__kernel void goose_set_q4_rows(__global uint8_t * src, __global uint8_t * dst,
    uint64_t stride, uint32_t blocks, uint64_t tasks) {
    uint32_t group, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(group));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    const uint64_t work = (uint64_t) group * 128 + lane;
    if (work >= tasks) return;
    const uint64_t row = work / blocks;
    const uint64_t src_at = row * stride + (work % blocks) * 128;
    fg176r_quant_q4_0_g((uint64_t) src, src_at, (uint64_t) dst, work * 18);
}

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

#include "vector-device.h"
#include "cuda-rms-reference.cl"

__kernel void goose_vector_ops(__global uint8_t * x, __global uint8_t * y,
    __global uint8_t * out, vector_launch_packet cfg, float param0, float param1) {
    uint32_t group, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(group));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    fg_vector_ops_g((uint64_t) x, (uint64_t) y, (uint64_t) out,
        (uint64_t) cfg.words, param0, param1, group, lane);
}

__kernel void goose_vector_flat(__global uint8_t * x, __global uint8_t * y,
    __global uint8_t * out, int64_t op, int64_t count, float param0, float param1) {
    uint32_t group, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(group));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    fg_vector_flat_g((uint64_t) x, (uint64_t) y, (uint64_t) out,
        op, count, param0, param1, (int64_t) group * 128 + lane);
}

__kernel void goose_vector_flat4(__global const uint8_t * x, __global const uint8_t * y,
    __global uint8_t * out, int64_t op, int64_t vectors, float param0, float param1) {
    uint32_t group, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(group));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    fg_vector_flat4_g((uint64_t) x, (uint64_t) y, (uint64_t) out,
        op, vectors, param0, param1, (int64_t) group * 128 + lane);
}

__kernel void goose_moe_add_shared_residual(__global const uint8_t * routed,
    __global const uint8_t * shared, __global const uint8_t * gate,
    __global const uint8_t * residual, __global uint8_t * output) {
    uint32_t block, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(block));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    fg_moe_add_shared_residual_g((uint64_t) routed, (uint64_t) shared, (uint64_t) gate,
        (uint64_t) residual, (uint64_t) output, (uint64_t) block * 128 + lane);
}

static float fg_rope_float(uint64_t cfg, int64_t i) {
    return as_float((uint32_t) fg_attn_word(cfg, i));
}

static int32_t fg_rope_position(uint64_t positions, int64_t i) {
    return ((__global int32_t *) positions)[i];
}

static float fg_rope_load(uint64_t address, int64_t size) {
    return size == 4 ? fg_device_global_load(address) : fg248d_ld_half(address);
}

static void fg_rope_store(uint64_t address, int64_t size, float value) {
    if (size == 4) fg_device_global_store(address, value);
    else fg176r_st_f16(address, 0, value);
}

static void fg_rope_copy_pair(uint64_t src, uint64_t dst, int64_t size) {
    if (size == 4) {
        ((__global uint32_t *) dst)[0] = ((__global uint32_t *) src)[0];
        ((__global uint32_t *) dst)[1] = ((__global uint32_t *) src)[1];
    } else {
        ((__global uint16_t *) dst)[0] = ((__global uint16_t *) src)[0];
        ((__global uint16_t *) dst)[1] = ((__global uint16_t *) src)[1];
    }
}

// Reduce angles before the PTX trigonometric instructions. Split 2*pi
// into two doubles so long contexts keep the small remainder accurately.
static float fg_rope_reduce(float theta) {
    if ((as_uint(theta) & 0x7f800000u) == 0x7f800000u) return as_float(0x7fc00000u);
    const double scaled = (double) theta * 0.15915494309189533577;
    int64_t n;
    __asm__("cvt.rni.s64.f64 %0, %1;" : "=l"(n) : "d"(scaled));
    double rem = (double) theta;
    const double neg = -(double) n;
    __asm__("fma.rn.f64 %0, %1, %2, %0;" : "+d"(rem) : "d"(neg), "d"(6.283185307179586));
    __asm__("fma.rn.f64 %0, %1, %2, %0;" : "+d"(rem) : "d"(neg), "d"(2.4492935982947064e-16));
    return (float) rem;
}

static float fg_rope_sin(float theta) {
    float result;
    __asm__("sin.approx.f32 %0, %1;" : "=f"(result) : "f"(fg_rope_reduce(theta)));
    return result;
}

static float fg_rope_cos(float theta) {
    float result;
    __asm__("cos.approx.f32 %0, %1;" : "=f"(result) : "f"(fg_rope_reduce(theta)));
    return result;
}

#include "rope-device.h"

static int64_t position_thread(void) {
    uint32_t group, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(group));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    return (int64_t) group * 128 + lane;
}

__kernel void goose_rope_phases(__global uint8_t * positions, __global uint8_t * factors,
    __global float * table, __global int64_t * cfg) {
    fg_rope_phases_g((uint64_t) positions, (uint64_t) factors, (uint64_t) table,
        (uint64_t) cfg, position_thread());
}

__kernel void goose_rope_apply(__global uint8_t * x, __global uint8_t * out,
    __global float * table, __global int64_t * cfg) {
    fg_rope_apply_g((uint64_t) x, (uint64_t) out, (uint64_t) table, (uint64_t) cfg, position_thread());
}

__kernel void goose_rope_decode(__global uint8_t * x, __global uint8_t * positions,
    __global uint8_t * factors, __global uint8_t * out, __global int64_t * cfg) {
    uint32_t head, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(head));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    fg_rope_decode_g((uint64_t) x, (uint64_t) positions, (uint64_t) factors,
        (uint64_t) out, (uint64_t) cfg, head, lane);
}

static float fg170s_load(uint64_t address, uint64_t i) { return fg_device_global_load(address + 4 * i); }
static void fg170s_store(uint64_t address, uint64_t i, float value) { fg_device_global_store(address + 4 * i, value); }
static float fg170s_expf(float value) { return fg_attn_exp(value); }
static void fg170s_lc(void) {}
static void fg170s_sync(void) {}
static void fg170s_barrier(void) { fg_device_barrier(); }
static float fg170s_shared_load(uint64_t address, uint64_t i) { return ((__local float *) address)[i]; }
static void fg170s_shared_store(uint64_t address, uint64_t i, float value) {
    ((__local float *) address)[i] = value;
}
static float fg170s_fma(float x, float w, float sum) {
    return fg_device_fma_ftz(x, w, sum);
}

#include "ssm-conv-device.h"

__kernel void goose_ssm_conv_direct(__global float *history, __global float *mixed, __global float *weights,
    __global float *output, uint64_t channels, uint64_t tokens, int32_t silu) {
    uint32_t group, time, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(group));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(time));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    ssm_conv_direct_g((uint64_t) history, (uint64_t) mixed, (uint64_t) weights, (uint64_t) output,
        channels, tokens, (uint64_t) group * 128 + lane, (uint64_t) time * 32, silu != 0);
}

__kernel void goose_ssm_conv_save_tail(__global float *history, __global float *mixed, uint64_t channels, uint64_t tokens) {
    ssm_conv_save_tail_g((uint64_t) history, (uint64_t) mixed, channels, tokens, position_thread());
}

__kernel void goose_ssm_conv(__global uint8_t * x, __global uint8_t * weights,
    __global uint8_t * out, __global int64_t * cfg) {
    uint32_t sequence, group, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(sequence));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(group));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    if ((int64_t) group * 128 + lane >= cfg[5]) return;
    ssm_conv_serial_g((uint64_t) x, (uint64_t) weights, 0, (uint64_t) out,
        (int32_t) cfg[8], (int32_t) cfg[9], (int32_t) cfg[10], (int32_t) cfg[11],
        (int32_t) cfg[12], (int32_t) cfg[13], (int32_t) cfg[14],
        (int32_t) cfg[7], (int32_t) cfg[6], cfg[15] != 0, 0, lane, sequence, group);
}

__kernel void goose_ssm_conv_decode(__global uint8_t * x, __global uint8_t * weights,
    __global uint8_t * out, __global int64_t * cfg) {
    uint32_t sequence, group, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(sequence));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(group));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    const uint64_t channel = (uint64_t) group * 128 + lane;
    if (channel >= (uint64_t) cfg[5]) return;
    ssm_conv_decode_g((uint64_t) x + sequence * (uint64_t) cfg[10], (uint64_t) weights,
        (uint64_t) out + sequence * (uint64_t) cfg[14], cfg[9], cfg[11], channel, cfg[15] != 0);
}

__kernel void goose_ssm_conv_long(__global uint8_t * x, __global uint8_t * weights,
    __global uint8_t * out, __global int64_t * cfg) {
    enum { taps = 4, tile = 32, channels = 128 };
    __local float shared[channels * (tile + taps - 1)];
    uint32_t sequence, group, lane, tokens;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(sequence));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(group));
    __asm__("mov.u32 %0, %%ctaid.z;" : "=r"(tokens));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    ssm_conv_long_g((uint64_t) x, (uint64_t) weights, 0, (uint64_t) out, (uint64_t) shared,
        (int32_t) cfg[8], (int32_t) cfg[9], (int32_t) cfg[10], (int32_t) cfg[11],
        (int32_t) cfg[12], (int32_t) cfg[13], (int32_t) cfg[14],
        (int32_t) cfg[7], taps, cfg[15] != 0, 0, 1, lane, sequence, group, tokens);
}

static float fg283d_ld_f32(uint64_t src, uint64_t offset) {
    return fg_device_global_load(src + offset);
}
static void fg283d_st_i32(uint64_t out, uint64_t offset, int32_t value) {
    *(__global int32_t *) (out + offset) = value;
}
static int32_t fg283d_sload_i32(uint64_t at) { return *(__local int32_t *) at; }
static void fg283d_sstore_i32(uint64_t at, int32_t value) { *(__local int32_t *) at = value; }
static void fg283d_sync(void) { fg_device_barrier(); }
static void fg283d_fail(int64_t code) { fg248d_fail(code); }
static float fg169t_shuffle(float value, int32_t offset) { return fg_device_shfl(value, offset); }
static int32_t fg169t_ishuffle(int32_t value, int32_t offset) {
    return as_int(fg_device_shfl(as_float(value), offset));
}

#include "routing-device.h"

__kernel void goose_route_reduce(__global uint8_t * x, __global uint8_t * y,
    __global uint8_t * out, __global int64_t * cfg) {
    uint32_t row, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(row));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    fg_route_reduce_g((uint64_t) x, (uint64_t) out, (uint64_t) cfg, row, lane);
}

__kernel void goose_route_sort(__global uint8_t * x, __global uint8_t * y,
    __global uint8_t * out, __global int64_t * cfg) {
    uint32_t row, lane, lanes;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(row));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    __asm__("mov.u32 %0, %%ntid.x;" : "=r"(lanes));
    __local int32_t scratch[4096], flags[128];
    fg_route_sort_g((uint64_t) x, (uint64_t) out, (uint64_t) cfg, row, lane, lanes,
        (uint64_t) scratch, (uint64_t) flags);
}

__kernel void goose_route_topk(__global uint8_t * x, __global uint8_t * y,
    __global uint8_t * out, __global int64_t * cfg) {
    uint32_t group, thread;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(group));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    fg_route_topk_g((uint64_t) x, (uint64_t) out, (uint64_t) cfg,
        (int64_t) group * 4 + thread / 32, thread % 32);
}

__kernel void goose_route_gather(__global uint8_t * x, __global uint8_t * y,
    __global uint8_t * out, __global int64_t * cfg) {
    fg_route_gather_g((uint64_t) x, (uint64_t) y, (uint64_t) out, (uint64_t) cfg, position_thread());
}

__kernel void goose_route_norm(__global uint8_t * x, __global uint8_t * y,
    __global uint8_t * out, __global int64_t * cfg) {
    uint32_t row, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(row));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    fg_route_norm_g((uint64_t) x, (uint64_t) y, (uint64_t) out, (uint64_t) cfg, row, lane);
}

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

static void fg434c_sync(void) { fg_device_barrier(); }

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

static void fg_fwht_quantize_shared(uint64_t src, uint64_t sb, uint64_t dst, uint64_t db) {
    fg176r_quant_q4_0_g(src, sb, dst, db);
}
static void fg_fwht_warp_sync(void) { __asm__ volatile("bar.warp.sync -1;" ::: "memory"); }
#include "fwht-device.h"

__kernel void goose_kv_fwht(__global const float * src, __global float * dst,
                           int32_t width, int64_t rows, float scale) {
    uint32_t block, thread;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(block));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    fg183f_row_g((uint64_t) src, (uint64_t) dst, width, rows, scale, block, 4, thread / 32, thread % 32, 32);
}

__kernel void goose_fwht_q4(__global const float *src, __global uchar *dst,
    int32_t width, int64_t rows, float scale) {
    __local float tile[4 * 256] __attribute__((aligned(16)));
    uint32_t block, thread;
    uint64_t shared;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(block));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    __asm__("cvta.shared.u64 %0, %1;" : "=l"(shared) : "l"((uint64_t) tile));
    const uint32_t warp = thread / 32;
    fwht_q4_write_g((uint64_t) src, (uint64_t) dst, shared + 4ul * warp * width,
        width, rows, scale, 4l * block + warp, thread % 32);
}

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

__kernel void goose_moe_weighted(__global const float * experts, __global const float * weights,
                                __global float * dst, int64_t width, int64_t used) {
    uint32_t token, tile, thread;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(token));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(tile));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    fg179w_reduce_g((uint64_t) experts, 0, (uint64_t) weights, (uint64_t) dst, width, used,
                    token, tile, thread, 256);
}

__kernel void goose_tensor_blit(__global uint8_t * x, __global uint8_t * y,
    __global uint8_t * out, __global int64_t * cfg) {
    fg_tensor_blit_g((uint64_t) x, (uint64_t) y, (uint64_t) out, (uint64_t) cfg, position_thread());
}

__kernel void goose_tensor_transpose_concat(__global const uint8_t * x, __global const uint8_t * y,
    __global uint8_t * out, __global const int64_t * cfg) {
    __local float tile[32 * 33] __attribute__((aligned(16)));
    uint32_t bx, by, thread;
    uint64_t shared;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(bx));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(by));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    __asm__("cvta.shared.u64 %0, %1;" : "=l"(shared) : "l"((uint64_t) tile));
    fg_tensor_transpose_concat_g((uint64_t) x, (uint64_t) y, (uint64_t) out, (uint64_t) cfg,
        bx, by, thread % 32, thread / 32, shared);
}

#ifdef GOOSE_CUDA_MMQ_IMAGE
static void fg_cuda_cp16(uint64_t dst, uint64_t src) {
    __asm__ volatile("cp.async.cg.shared.global [%0], [%1], 16;" :: "r"((uint32_t) dst), "l"(src) : "memory");
}
static void fg_cuda_cpwait(void) { __asm__ volatile("cp.async.wait_all;" ::: "memory"); }

#include "cuda-mmq.cl"
#include "cuda-mmvq.cl"
#include "cuda-dense-mma.cl"
#include "cuda-gdn-mma.cl"
#include "cuda-flash-mma.cl"
#include "cuda-flash-vec.cl"
#endif
