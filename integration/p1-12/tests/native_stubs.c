// Minimal stubs for the native ggml objects this verification links.
//
// The parity check needs the real quantize_row_*_ref and ggml_vec_dot_*
// implementations from ggml-quants.c and arch/x86/quants.c. Those objects also
// reference two things from ggml core that this program never reaches: the abort
// hook and the f16 lookup table. Supplying them here keeps the comparison
// linking against the *unmodified* native sources instead of a trimmed copy.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void ggml_abort(const char * file, int line, const char * fmt, ...) {
    fprintf(stderr, "native ggml aborted at %s:%d\n", file, line);
    abort();
}

// ggml_table_f32_f16 is what GGML_CPU_FP16_TO_FP32 indexes on this build. Filling
// it with zeros would silently zero every block scale, so it is filled below
// from the same conversion ggml.c uses.
float ggml_table_f32_f16[1 << 16];

__attribute__((constructor)) static void fill_f16_table(void) {
    for (int i = 0; i < (1 << 16); ++i) {
        const uint16_t h = (uint16_t) i;
        _Float16 f;
        __builtin_memcpy(&f, &h, sizeof f);
        ggml_table_f32_f16[i] = (float) f;
    }
}

// ggml_row_size is ggml core arithmetic over the type traits. The row
// quantizers call it only for whole-row entry points this program never
// reaches; the four reference row quantizers under test take an element count.
// The real value is type_size * ne / blck_size, which is what this mirrors for
// the types the linked objects can ask about.
#include "ggml.h"

// ggml_type_name is used by the validator's diagnostics only.
const char * ggml_type_name(enum ggml_type type) {
    switch (type) {
        case GGML_TYPE_Q4_0: return "q4_0";
        case GGML_TYPE_Q8_0: return "q8_0";
        case GGML_TYPE_Q4_K: return "q4_K";
        case GGML_TYPE_Q6_K: return "q6_K";
        default: return "unknown";
    }
}

// The mxfp4 / fp8 scale tables belong to types this program never builds a
// block for; the objects only need the symbols to link.
float ggml_table_f32_e8m0_half[256];
float ggml_table_f32_ue4m3[256];

int64_t ggml_blck_size(enum ggml_type type) {
    switch (type) {
        case GGML_TYPE_Q4_0: case GGML_TYPE_Q8_0: return 32;
        case GGML_TYPE_Q4_K: case GGML_TYPE_Q6_K: return 256;
        default: return 0;
    }
}

size_t ggml_row_size(enum ggml_type type, int64_t ne) {
    const int64_t bs = ggml_blck_size(type);
    const size_t ts = ggml_type_size(type);
    return ts * (size_t) (ne / bs);
}

size_t ggml_type_size(enum ggml_type type) {
    // The four types under test, from the native type_traits table.
    switch (type) {
        case GGML_TYPE_Q4_0: return sizeof(uint16_t) + 16;
        case GGML_TYPE_Q8_0: return sizeof(uint16_t) + 32;
        case GGML_TYPE_Q4_K: return 144;
        case GGML_TYPE_Q6_K: return 210;
        default: return 0;
    }
}
