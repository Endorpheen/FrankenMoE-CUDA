/* services.c - raw effects only for canonical IO (no IO policy). */
#include "canonical-io.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-alloc.h"

int64_t fgcio_raw(int64_t op, int64_t a, int64_t b, int64_t c, int64_t d) {
    switch (op) {
        case 0: abort();
        case 1: memcpy((void *)(intptr_t) a, (const void *)(intptr_t) b, (size_t) c); return 0;
        case 2: memcpy((void *)(intptr_t) a, (const void *)(intptr_t) b, (size_t) c); return 0;
        case 3: { FILE * f = (FILE *)(intptr_t) a; size_t n = fwrite((const void *)(intptr_t) b, 1, (size_t) c, f);
                  return n == (size_t) c ? 0 : -1; }
        case 4: { FILE * f = (FILE *)(intptr_t) a; size_t n = fread((void *)(intptr_t) b, 1, (size_t) c, f);
                  return n == (size_t) c ? 1 : 0; }
        case 5: ggml_backend_tensor_get((const ggml_tensor *)(intptr_t) a, (void *)(intptr_t) b, (size_t) c, (size_t) d); return 0;
        case 6: ggml_backend_tensor_set((ggml_tensor *)(intptr_t) a, (const void *)(intptr_t) b, (size_t) c, (size_t) d); return 0;
        case 7: return *(const int64_t *)(intptr_t) a;
        case 8: *(int64_t *)(intptr_t) a = b; return 0;
        case 10: { FILE * f = (FILE *)(intptr_t) a; void * tmp = malloc((size_t) c); if (!tmp) return -1;
                   ggml_backend_tensor_get((const ggml_tensor *)(intptr_t) b, tmp, (size_t) d, (size_t) c);
                   size_t n = fwrite(tmp, 1, (size_t) c, f); free(tmp); return n == (size_t) c ? 0 : -1; }
        case 11: { FILE * f = (FILE *)(intptr_t) a; void * tmp = malloc((size_t) c); if (!tmp) return 0;
                   size_t n = fread(tmp, 1, (size_t) c, f);
                   if (n == (size_t) c) ggml_backend_tensor_set((ggml_tensor *)(intptr_t) b, tmp, (size_t) d, (size_t) c);
                   free(tmp); return n == (size_t) c ? 1 : 0; }
        case 12: return (int64_t)(intptr_t) ggml_backend_buffer_get_type(((const ggml_tensor *)(intptr_t) a)->buffer);
        case 13: return (int64_t) ggml_element_size((const ggml_tensor *)(intptr_t) a);
        case 14: return (int64_t) ((const ggml_tensor *)(intptr_t) a)->type;
        case 15: return (int64_t) ggml_nbytes((const ggml_tensor *)(intptr_t) a);
        case 16: return ggml_are_same_shape((const ggml_tensor *)(intptr_t) a, (const ggml_tensor *)(intptr_t) b) ? 1 : 0;
        case 17: return (int64_t)(intptr_t) ((const ggml_tensor *)(intptr_t) a)->view_src;
        case 18: return (int64_t) ((const ggml_tensor *)(intptr_t) a)->view_offs;
        case 19: ggml_backend_view_init((ggml_tensor *)(intptr_t) a); return 0;
        case 20: ggml_backend_tensor_copy((const ggml_tensor *)(intptr_t) a, (ggml_tensor *)(intptr_t) b); return 0;
        case 21: { ggml_init_params p = {(size_t) a, NULL, true}; return (int64_t)(intptr_t) ggml_init(p); }
        case 22: ggml_free((ggml_context *)(intptr_t) a); return 0;
        case 23: return (int64_t)(intptr_t) ggml_view_1d((ggml_context *)(intptr_t) a,
                         (ggml_tensor *)(intptr_t) b, c, (size_t) d);
        case 24: return (int64_t)(intptr_t) ggml_new_tensor_1d((ggml_context *)(intptr_t) a,
                         (ggml_type) b, c);
        case 25: return (int64_t) ggml_tensor_overhead();
        case 26: return (int64_t)(intptr_t) ggml_backend_alloc_ctx_tensors_from_buft(
                         (ggml_context *)(intptr_t) a, (ggml_backend_buffer_type_t)(intptr_t) b);
        case 27: return (int64_t) ggml_backend_buft_get_alignment((ggml_backend_buffer_type_t)(intptr_t) a);
        case 28: return (int64_t) ggml_backend_buft_get_alloc_size((ggml_backend_buffer_type_t)(intptr_t) a,
                         (const ggml_tensor *)(intptr_t) b);
        case 29: return (int64_t) ggml_backend_buffer_get_size((ggml_backend_buffer_t)(intptr_t) a);
        case 30: return a; /* ownership handle retained by caller (see Goose) */
        case 31: return (int64_t)(intptr_t) new ggml_tallocr(ggml_tallocr_new(
                         (ggml_backend_buffer_t)(intptr_t) a));
        case 32: ggml_tallocr_alloc((ggml_tallocr *)(intptr_t) a, (ggml_tensor *)(intptr_t) b); return 0;
        case 33: delete (ggml_tallocr *)(intptr_t) a; return 0;
        case 34: abort();
        case 38: ggml_backend_buffer_free((ggml_backend_buffer_t)(intptr_t) a); return 0;
        default: abort();
    }
}
