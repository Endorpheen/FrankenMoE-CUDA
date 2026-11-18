#ifndef FG_CPU_SIMD_BLOCKS_H
#define FG_CPU_SIMD_BLOCKS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { const uint8_t *data; int64_t len; } fg_cpu_simd_bytes;
#define FG_CPU_SIMD_API __attribute__((visibility("default")))
FG_CPU_SIMD_API int32_t fg_cpu_simd_row_q4_k_q8_k_x8(fg_cpu_simd_bytes w, fg_cpu_simd_bytes a,
                                                     int32_t nb, int32_t nc, float * out);
#ifdef __cplusplus
}
#endif
#endif
