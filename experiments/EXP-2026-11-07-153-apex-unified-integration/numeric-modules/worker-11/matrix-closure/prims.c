/* Host primitives for the matrix-closure generated unit. */
#include <immintrin.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

float fgc_half(uint16_t v) { return _cvtsh_ss(v); }
uint16_t fgc_to_half(float v) { return _cvtss_sh(v, _MM_FROUND_TO_NEAREST_INT); }
uint32_t fgc_bits(float x) { uint32_t v; memcpy(&v, &x, 4); return v; }
float fgc_from_bits(uint32_t v) { float x; memcpy(&x, &v, 4); return x; }
float fgc_roundf(float x) { return roundf(x); }
uint8_t fgc_f2i8(float x) { return (uint8_t) (int8_t) (int) roundf(x); }
void fgc_witness(int64_t op, int64_t value) { (void) op; (void) value; }
