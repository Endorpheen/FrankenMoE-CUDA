// Exact CPU_REPACK provider contract for the Apex Q4_K_M + 5950X profile.
// Layouts and prototypes are copied from:
//   inputs/native/ggml/src/ggml-cpu/repack.h
//   inputs/native/ggml/src/ggml-cpu/repack.cpp
// This header publishes the ABI; it does not add owned C++ wrappers and does
// not reinterpret records as classes.
#pragma once
#include <stddef.h>
#include <stdint.h>

struct ggml_tensor;

#ifdef __cplusplus
extern "C" {
#endif

#define FG006_REPACK_QK_K 256
#define FG006_REPACK_QK8_0 32
#define FG006_REPACK_SCALE_SIZE 12
#define FG006_REPACK_TENSOR_ALIGNMENT 64

// block_q4_K raw layout, 144 bytes.
// d:u16 @0, dmin:u16 @2, scales:u8[12] @4, qs:u8[128] @16.
typedef struct fg006_block_q4_K {
    uint16_t d;
    uint16_t dmin;
    uint8_t  scales[12];
    uint8_t  qs[128];
} fg006_block_q4_K;

// block_q4_Kx8 repacked layout, 1152 bytes.
// d:u16[8] @0, dmin:u16[8] @16, scales:u8[96] @32, qs:u8[1024] @128.
typedef struct fg006_block_q4_Kx8 {
    uint16_t d[8];
    uint16_t dmin[8];
    uint8_t  scales[96];
    uint8_t  qs[1024];
} fg006_block_q4_Kx8;

// block_q8_K raw activation layout, 292 bytes.
// d:f32 @0, qs:i8[256] @4, bsums:i16[16] @260.
typedef struct fg006_block_q8_K {
    float   d;
    int8_t  qs[256];
    int16_t bsums[16];
} fg006_block_q8_K;

// block_q8_Kx4 packed activation layout, 1168 bytes.
// d:f32[4] @0, qs:i8[1024] @16, bsums:i16[64] @1040.
typedef struct fg006_block_q8_Kx4 {
    float   d[4];
    int8_t  qs[1024];
    int16_t bsums[64];
} fg006_block_q8_Kx4;

enum fg006_repack_kind {
    FG006_REPACK_NONE = 0,
    FG006_REPACK_Q4_K_8X8_Q8_K = 1
};

// 5950X profile: CPU_REPACK Q4_K is selected exactly when AVX2 is present and
// ne[1] % 8 == 0. Q6_K/Q5_K/Q2_K have no x86 branch in the source and are not
// published here.
static inline int fg006_repack_q4_K_supported(int has_avx2, int64_t ne1) {
    return has_avx2 && ne1 > 0 && (ne1 % 8) == 0;
}

// Exact native prototypes. The provider's repack callback runs on tensor load;
// gemv/gemm run from GGML_OP_MUL_MAT/MUL_MAT_ID through extra_buffer_type.
int   fg006_repack_enabled(void);
int   fg006_repack_q4_Kx8(void * dst, const void * src, int64_t nrows, int64_t nblocks);
int   fg006_quantize_mat_q8_K_4x8(const float * x, void * vy, int64_t k);
void  fg006_gemv_q4_K_8x8_q8_K(int n, float * s, size_t bs, const void * vx, const void * vy, int nr, int nc);
void  fg006_gemm_q4_K_8x8_q8_K(int n, float * s, size_t bs, const void * vx, const void * vy, int nr, int nc);

// Workspace contract for forward_mul_mat: nbw1 = row_size(Q8_K, ne10),
// nbw2 = nbw1 * ne11, wsize >= nbw2 * ne12. The provider owns no global
// scratch; work data is supplied by ggml_cplan.
// Ownership: tensor->extra points at the static tensor traits for the selected
// repack kind; the CPU_REPACK buffer owns tensor bytes and calls repack on
// set_tensor. Records do not own the extra handle.

#ifdef __cplusplus
}
#endif
