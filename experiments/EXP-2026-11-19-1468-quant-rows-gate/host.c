// The six host primitives the quant row modules need. They are byte and float accessors, nothing else: the decisions stay in
// Goose, and this file exists so the module can be compiled on its own for its gate.
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

float qz_ld_f32(uint64_t a) { float v; memcpy(&v, (const void *) (uintptr_t) a, sizeof(v)); return v; }
void  qz_st_f16(uint64_t a, float v) { _Float16 h = (_Float16) v; memcpy((void *) (uintptr_t) a, &h, sizeof(h)); }
void  qz_st_u8 (uint64_t a, int64_t v) { uint8_t b = (uint8_t) v; memcpy((void *) (uintptr_t) a, &b, 1); }
void  qz_st_u32(uint64_t a, int64_t v) { uint32_t w = (uint32_t) v; memcpy((void *) (uintptr_t) a, &w, 4); }
float qz_abs(float v) { return fabsf(v); }
int64_t qz_ld_u8(uint64_t a) { return (int64_t) *(const uint8_t *) (uintptr_t) a; }

// ---- the second module's primitives. The math ones are trivial wrappers; the grid and map ones belong to the quantiser
// itself, which this gate never calls - it asks only for the three size decisions. A null there would crash loudly if that
// assumption ever changed, which is the behaviour an unexercised path should have.
float qz2_ld_f32(uint64_t a) { float v; memcpy(&v, (const void *) (uintptr_t) a, 4); return v; }
void  qz2_st_f32(uint64_t a, float v) { memcpy((void *) (uintptr_t) a, &v, 4); }
float qz2_ld_f16(uint64_t a) { _Float16 h; memcpy(&h, (const void *) (uintptr_t) a, 2); return (float) h; }
void  qz2_st_f16(uint64_t a, float v) { _Float16 h = (_Float16) v; memcpy((void *) (uintptr_t) a, &h, 2); }
void  qz2_st_u8 (uint64_t a, int64_t v) { uint8_t b = (uint8_t) v; memcpy((void *) (uintptr_t) a, &b, 1); }
int64_t qz2_ld_u8(uint64_t a) { return (int64_t) *(const uint8_t *) (uintptr_t) a; }
void  qz2_st_u16(uint64_t a, int64_t v) { uint16_t h = (uint16_t) v; memcpy((void *) (uintptr_t) a, &h, 2); }
void  qz2_st_u32(uint64_t a, int64_t v) { uint32_t w = (uint32_t) v; memcpy((void *) (uintptr_t) a, &w, 4); }
float qz2_abs(float v) { return fabsf(v); }
float qz2_sqrtf(float v) { return sqrtf(v); }
float qz2_log2f(float v) { return log2f(v); }
float qz2_floorf(float v) { return floorf(v); }
float qz2_lroundf(float v) { return roundf(v); }
float qz2_ldexpf(float v, int64_t e) { return ldexpf(v, (int) e); }
int64_t qz2_bits(float v) { uint32_t b; memcpy(&b, &v, 4); return (int64_t) b; }
float qz2_of_bits(int64_t b) { uint32_t u = (uint32_t) b; float v; memcpy(&v, &u, 4); return v; }
int64_t qz2_nearest(double v) { return (int64_t) llround(v); }
uint64_t qz2_tmp(void) { static unsigned char buf[1 << 16]; return (uint64_t) (uintptr_t) buf; }
void qz2_fail(int64_t code) { fprintf(stderr, "qz2_fail(%lld)\n", (long long) code); }
void *qz2_iq2_grid(void) { return NULL; }
void *qz2_iq2_qgrid(void) { return NULL; }
void *qz2_iq2_map(void) { return NULL; }
int64_t qz2_iq2_nb(void) { return 0; }
void *qz2_iq3_grid(void) { return NULL; }
void *qz2_iq3_qgrid(void) { return NULL; }
void *qz2_iq3_map(void) { return NULL; }
int64_t qz2_iq3_nb(void) { return 0; }
void qz2_sort16(uint64_t a) { (void) a; }
