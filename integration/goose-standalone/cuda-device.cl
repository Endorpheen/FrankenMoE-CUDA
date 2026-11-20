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
#define GS_UNREACHABLE(file, line) device_trap()
#define gs_add_i64(a, b, file, line) device_add(a, b)
#define gs_sub_i64(a, b, file, line) device_sub(a, b)
#define gs_mul_i64(a, b, file, line) device_mul(a, b)
#define gs_add_u64(a, b) device_uadd(a, b)
#define gs_div_i64(a, b, file, line) device_div(a, b)
#define gs_mod_i64(q, a, b, file, line) device_mod(a, b)
#define gs_shl_i64(a,b) ((int64_t)((uint64_t)(a) << ((b)&63)))
#define gs_shr_i64(a,b) ((a) >> ((b)&63))
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

#include "matmul-device.h"

__kernel void goose_quantized_matmul(
    __global uint8_t * weights, __global uint8_t * input, __global uint8_t * output,
    __global uint64_t * jobs, uint64_t row_stride, int32_t block_count, int32_t kind, int32_t job_base) {
    uint32_t lane, row, job, lanes;
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(row));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(job));
    __asm__("mov.u32 %0, %%ntid.x;" : "=r"(lanes));
    __local float scratch[256];
    const uint64_t at = 2 * ((uint64_t) job_base + job);
    fg_device_matmul_g((uint64_t) weights + row * row_stride, (uint64_t) input + jobs[at],
        (uint64_t) output + jobs[at + 1] + 4 * (uint64_t) row, (uint64_t) scratch,
        block_count, kind, lane, lanes);
}
