// Staged S1.3 seam: the Goose P7-04 quant stack behind one build flag.
//
// Three native entry points dispatch into the composed Goose unit when
// FG_GOOSE_QUANT is defined:
//
//   dequantize_row_q4_0()      -> fg_goose_quant_dequant_row_q4_0()
//   quantize_row_q8_0_ref()    -> fg_goose_quant_quantize_row_q8_0()
//   ggml_blck_size()           -> fg_goose_quant_blck_size()
//
// The Goose side is one translation unit, goose-gen/quant-stack.c, generated
// from the composed goose-gen/quant-stack.goose. The host keeps the native
// bodies compiled in both configurations, so switching the flag off is a
// rebuild and not a revert, and a refusal from the module falls back to the
// native body in the same call.
//
// Public ABI: none of the three signatures changes. The only additions are
// the fg_goose_* symbols, which exist in the library only when the flag is on.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef FG_GOOSE_QUANT

// All three return 0 when the Goose path produced the whole result, and a
// negative value when the module refused the type or the geometry. A refusal
// is not an error: the native wrapper then runs the native body.
int fg_goose_quant_dequant_row_q4_0(const void * src, float * dst, int64_t k);
int fg_goose_quant_quantize_row_q8_0(const float * src, void * dst, int64_t k);

// Block size as the module sees it: positive when the Goose side owns the
// type, -1 otherwise (the native type-traits table answers those).
int fg_goose_quant_blck_size(int type);

// Witness counters for the gate. dequant/quant count one per row entry the
// module served; blck counts one per block-size dispatch it answered.
int64_t fg_goose_quant_row_calls(void);
int64_t fg_goose_quant_blck_calls(void);

// Read-only view of the native IQ tables for the composed module
// (implemented in ggml-quants.c, where those tables live).
int64_t fg_goose_iq2_grid(int64_t g, int64_t i);
int64_t fg_goose_iq3_grid(int64_t g, int64_t i);
int64_t fg_goose_iq2_map(int64_t g, int64_t u);
int64_t fg_goose_iq2_nb(int64_t g, int64_t off);
int64_t fg_goose_iq3_map(int64_t g, int64_t u);
int64_t fg_goose_iq3_nb(int64_t g, int64_t off);

#else // FG_GOOSE_QUANT

// With the flag off nothing in the library can reach the Goose module, so the
// counters are zero by construction, not by measurement.
static inline int64_t fg_goose_quant_row_calls(void)  { return 0; }
static inline int64_t fg_goose_quant_blck_calls(void) { return 0; }

#endif // FG_GOOSE_QUANT

#ifdef __cplusplus
}
#endif
