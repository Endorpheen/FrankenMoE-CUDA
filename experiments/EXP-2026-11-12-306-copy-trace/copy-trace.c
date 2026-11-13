#include "ggml.h"
#include "ggml-backend.h"
#include <dlfcn.h>
#include <stdio.h>

typedef void (*copy_fn)(const struct ggml_tensor *, struct ggml_tensor *);

void ggml_backend_tensor_copy(const struct ggml_tensor * src, struct ggml_tensor * dst) {
    static copy_fn real = 0;
    if (!real) real = (copy_fn) dlsym(RTLD_NEXT, "ggml_backend_tensor_copy");
    fprintf(stderr, "COPY src=%s(%s/%zu) dst=%s(%s/%zu)\n",
            src->name, src->buffer ? ggml_backend_buffer_name(src->buffer) : "-", ggml_nbytes(src),
            dst->name, dst->buffer ? ggml_backend_buffer_name(dst->buffer) : "-", ggml_nbytes(dst));
    real(src, dst);
}
