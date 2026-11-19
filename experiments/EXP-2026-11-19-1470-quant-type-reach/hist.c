// Which quantisation types the Apex model actually contains.
//
// The removal plan is blocked by twelve row types nobody has ported. A native body is only deletable when no supported
// scenario reads it (rule 28), so the first question is which types this runtime's own model uses: reading the tensor list of
// the GGUF is cheap, needs no weights and no GPU, and turns a list of twelve into a list of the ones that matter here.
#include "ggml.h"
#include "gguf.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

int main(int argc, char ** argv) {
    const char * path = argc > 1 ? argv[1] : "/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf";
    struct gguf_init_params ip = { /*.no_alloc =*/ true, /*.ctx =*/ NULL };
    struct gguf_context * g = gguf_init_from_file(path, ip);
    if (!g) { printf("cannot read %s\n", path); return 2; }
    const int64_t nt = gguf_get_n_tensors(g);
    long counts[GGML_TYPE_COUNT];
    memset(counts, 0, sizeof(counts));
    for (int64_t i = 0; i < nt; ++i) {
        const enum ggml_type t = gguf_get_tensor_type(g, i);
        if ((int) t >= 0 && (int) t < GGML_TYPE_COUNT) { ++counts[(int) t]; }
    }
    printf("apex model: tensors=%lld\n", (long long) nt);
    long distinct = 0;
    for (int t = 0; t < GGML_TYPE_COUNT; ++t) {
        if (counts[t] > 0) {
            ++distinct;
            printf("  %-12s %6ld tensor(s)\n", ggml_type_name((enum ggml_type) t), counts[t]);
        }
    }
    printf("apex model: distinct types=%ld\n", distinct);
    gguf_free(g);
    return 0;
}
