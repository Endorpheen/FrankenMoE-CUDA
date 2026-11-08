/* Host primitive definitions for the generated unit when it is linked as an
 * ordinary object instead of being included in the fixture. */
#include <immintrin.h>
#include <stdint.h>
#include <string.h>
#include "apex_matrix.h"
uint32_t fgm_bits(float x) { uint32_t v; memcpy(&v,&x,4); return v; }
float fgm_from_bits(uint32_t v) { float x; memcpy(&x,&v,4); return x; }
float fgm_half(uint16_t v) { return _cvtsh_ss(v); }
uint16_t fgm_to_half(float v) { return _cvtss_sh(v, _MM_FROUND_TO_NEAREST_INT); }
void fgm_witness(int64_t op, int64_t value) { (void) op; (void) value; }
