/* Runtime validation of the P5-02 fix: decline fg143 whenever a secondary source is block-quantized, so the native
   flash-attention implementation runs, exactly as the flint line does. */
#include "ggml.h"
#include <dlfcn.h>
#include <stdint.h>

typedef uint8_t (*try_fn)(const void *, struct ggml_tensor *);

static int fg_scalar(enum ggml_type t) {
    switch (t) {
        case GGML_TYPE_F32: case GGML_TYPE_F16: case GGML_TYPE_BF16:
        case GGML_TYPE_I8: case GGML_TYPE_I16: case GGML_TYPE_I32:
        case GGML_TYPE_I64: case GGML_TYPE_F64: return 1;
        default: return 0;
    }
}

uint8_t fg143_try(const void * params, struct ggml_tensor * t) {
    static try_fn real = 0;
    if (!real) real = (try_fn) dlsym(RTLD_NEXT, "fg143_try");
    if (!real) return 0;
    if (t->src[1] && !fg_scalar(t->src[1]->type)) return 0;
    if (t->src[2] && !fg_scalar(t->src[2]->type)) return 0;
    return real(params, t);
}
