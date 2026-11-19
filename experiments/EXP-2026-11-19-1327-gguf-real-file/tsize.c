/* The ported tensor-size computation against ggml, for every tensor of the real model. */
#include "gguf.h"
#include "ggml.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int64_t * data; int64_t len; } sl_i64;
typedef int64_t (*fn_size)(sl_i64, sl_i64, int64_t);

int main(int argc, char ** argv) {
    const char * path = argc > 1 ? argv[1] : "/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf";
    struct gguf_init_params gp = { .no_alloc = true, .ctx = NULL };
    struct gguf_context * g = gguf_init_from_file(path, gp);
    if (!g) { printf("no file\n"); return 2; }
    const int64_t nt = (int64_t) gguf_get_n_tensors(g);

    void * h = dlopen("/home/end0/AI/FrankenMoE-CUDA/builds/flint-acc/bin/libllama.so.0", RTLD_LAZY);
    fn_size size_fn = h ? (fn_size) dlsym(h, "s3_tensor_size") : NULL;
    if (!size_fn) { printf("no s3_tensor_size\n"); return 3; }

    /* trec: stride 8, [2] = type. tne: 4 extents per tensor. */
    int64_t * trec = calloc((size_t) nt * 8, sizeof(int64_t));
    int64_t * tne  = calloc((size_t) nt * 4, sizeof(int64_t));
    for (int64_t i = 0; i < nt; ++i) {
        trec[i * 8 + 2] = (int64_t) gguf_get_tensor_type(g, i);
        const int64_t * ne = gguf_get_tensor_ne(g, i);
        for (int d = 0; d < 4; ++d) { tne[i * 4 + d] = ne[d]; }
    }
    sl_i64 s_ne;  s_ne.data = tne;  s_ne.len = nt * 4;
    sl_i64 s_rec; s_rec.data = trec; s_rec.len = nt * 8;

    int bad = 0;
    for (int64_t i = 0; i < nt; ++i) {
        const int64_t port = size_fn(s_ne, s_rec, i);
        const int64_t expect = (int64_t) ggml_row_size(gguf_get_tensor_type(g, i), gguf_get_tensor_ne(g, i)[0]);
        /* ggml_nbytes = row_size(ne0) * ne1 * ne2 * ne3 / (blck_size ratio already inside row_size) */
        const int64_t * ne = gguf_get_tensor_ne(g, i);
        int64_t want = expect;
        for (int d = 1; d < 4; ++d) { want *= ne[d]; }
        if (port != want) {
            ++bad;
            if (bad <= 5) printf("SIZE MISMATCH i=%lld type=%d ne=%lld,%lld,%lld,%lld port=%lld want=%lld\n",
                                 (long long) i, (int) gguf_get_tensor_type(g, i),
                                 (long long) ne[0], (long long) ne[1], (long long) ne[2], (long long) ne[3],
                                 (long long) port, (long long) want);
        }
    }
    printf("tensor sizes: %lld/%lld equal\n", (long long) (nt - bad), (long long) nt);
    gguf_free(g);
    return bad ? 1 : 0;
}
