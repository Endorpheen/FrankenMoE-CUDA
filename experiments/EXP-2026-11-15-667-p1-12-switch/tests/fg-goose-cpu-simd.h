// Runtime switch for the four CPU MoE block dot products (P1-12 integration).
//
// The compiler owner delivered a Goose SIMD surface with C-ABI single-block
// kernels (EXP665, commit 7738b34a, package cpu-simd-8b7a56bb): q4_K x q8_K,
// q6_K x q8_K, q8_0 x q8_0 and q4_0 x q8_0. This bridge is the switch that puts
// them on the CPU MoE path, following rule 10: the Goose kernel is the default
// and FG_GOOSE_CPU_SIMD_NATIVE exists only to take the native arm back.
//
// Scope and its one hard limit
// ---------------------------
// The delivered entry points take ONE block per call (256 elements for the
// K-quants, 32 for q8_0/q4_0). ggml calls the row kernels with n = nb * 256 and
// accumulates across blocks in a fixed single-precision order. A one-block fast
// path therefore only engages when nb == 1; for nb > 1 the native loop runs
// unchanged, which is what preserves the accumulation order and the fp32 result
// bit for bit. Forwarding a multi-block row to a one-block kernel would silently
// drop every block after the first, so the guard is not an optimisation.
//
// Failure policy: the Goose object is bound lazily; if it cannot be loaded the
// bridge reports unavailability once and every call takes the native arm. A
// missing kernel must never turn into a zeroed dot product.

#ifndef FG_GOOSE_CPU_SIMD_H
#define FG_GOOSE_CPU_SIMD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 1 when the switch may route a call to the Goose kernel, 0 for the native arm.
// Reads FG_GOOSE_CPU_SIMD_NATIVE once per process.
int fg_goose_cpu_simd_active(void);

// 1 when the Goose object resolved and reports AVX2/FMA support.
int fg_goose_cpu_simd_available(void);

// Number of calls actually routed to the Goose kernels, for the gate and for a
// live check that the switch is doing something.
long fg_goose_cpu_simd_calls(void);

// The four block kernels the switch needs, resolved from the delivered ABI.
typedef struct fg_cpu_simd_bytes {
    const uint8_t * data;
    int64_t len;
} fg_cpu_simd_bytes;

typedef int32_t (*fg_goose_dot_fn)(fg_cpu_simd_bytes a, fg_cpu_simd_bytes b,
                                   int32_t * minimum, float * value);

// One-block fast paths. Each returns 0 when it produced *value, and the caller
// then keeps the native result otherwise. `nb` is the number of 256-element
// blocks in the row; the bridge refuses anything but 1.
int fg_goose_dot_q4_k_q8_k(int nb, const void * vx, const void * vy, float * value);
int fg_goose_dot_q6_k_q8_k(int nb, const void * vx, const void * vy, float * value);
int fg_goose_dot_q8_0_q8_0(int nb, const void * vx, const void * vy, float * value);
int fg_goose_dot_q4_0_q8_0(int nb, const void * vx, const void * vy, float * value);

#ifdef __cplusplus
}
#endif

#endif // FG_GOOSE_CPU_SIMD_H
