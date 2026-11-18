// Temporary ABI debt: the accepted EXP156 POD projections/name primitive,
// exposed once for the standalone unit. No reshape/permutation policy is here.
#include <stdint.h>
#include <string.h>
#include "ggml.h"

uint64_t fg156_tget(int64_t p, int64_t k, int64_t i) {
    const struct ggml_tensor * t = (const void *) (intptr_t) p;
    switch (k) {
        case 0: return (uint64_t) t->ne[i];
        case 1: return t->nb[i];
        case 2: return (uintptr_t) t->view_src;
        case 3: return t->view_offs;
        case 4: return (uintptr_t) t->data;
        case 5: return t->type;
        case 6: return (uintptr_t) t->ne;
        default: ggml_abort(__FILE__, __LINE__, "Invalid tensor projection");
    }
}

void fg156_tset(int64_t p, int64_t k, int64_t i, uint64_t v) {
    struct ggml_tensor * t = (void *) (intptr_t) p;
    switch (k) {
        case 0: t->ne[i] = (int64_t) v; return;
        case 1: t->nb[i] = v; return;
        case 2: t->view_src = (void *) (uintptr_t) v; return;
        case 3: t->view_offs = v; return;
        case 4: t->data = (void *) (uintptr_t) v; return;
        case 5: t->type = (enum ggml_type) v; return;
        case 7: t->op = (enum ggml_op) v; return;
        case 8: t->src[i] = (void *) (uintptr_t) v; return;
        case 9: memcpy(t->op_params, &v, sizeof v); return;
        case 10: { int32_t q = (int32_t) v; memcpy((char *) t->op_params + i * sizeof q, &q, sizeof q); return; }
        default: ggml_abort(__FILE__, __LINE__, "Invalid tensor projection write");
    }
}

void fg156_name(int64_t p, int64_t source, int64_t style) {
    const struct ggml_tensor * s = (const void *) (intptr_t) source;
    const char * format;
    switch (style) {
        case 0: format = "%s (view)"; break;
        case 1: format = "%s (reshaped)"; break;
        case 2: format = "%s (permuted)"; break;
        case 3: format = "%s (transposed)"; break;
        default: ggml_abort(__FILE__, __LINE__, "Invalid constructor name style");
    }
    ggml_format_name((void *) (intptr_t) p, format, s->name);
}
