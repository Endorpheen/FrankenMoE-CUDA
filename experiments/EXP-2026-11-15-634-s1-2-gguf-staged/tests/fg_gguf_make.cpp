// Staged S1.2 helper: writes a small synthetic GGUF file for the split-plan
// differential. It exists so the split tool comparison needs no model: the
// input is a handful of float tensors with a deterministic byte pattern.
//
// Use the OFF build to create the inputs; both the OFF and the ON split tool
// then read exactly the same bytes.
#include "ggml.h"
#include "gguf.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

int main(int argc, char ** argv) {
    if (argc != 4 && argc != 5) {
        fprintf(stderr, "usage: %s OUT.gguf N_TENSORS ELEMS_PER_TENSOR [LAST_ELEMS]\n", argv[0]);
        return 2;
    }
    const char * path = argv[1];
    const int    n_t  = atoi(argv[2]);
    const int    ne0  = atoi(argv[3]);
    // Optional: the last tensor gets this many elements instead, so the split
    // gate can build an input whose padded tensor bytes hit a size limit
    // exactly (the `>` and `>=` thresholds differ only on equality).
    const int    ne0_last = argc == 5 ? atoi(argv[4]) : 0;
    if (n_t < 1 || ne0 < 1) {
        fprintf(stderr, "bad arguments\n");
        return 2;
    }

    ggml_init_params gp{};
    gp.mem_size   = 64u * 1024u * 1024u;
    gp.mem_buffer = nullptr;
    gp.no_alloc   = false;
    ggml_context * g = ggml_init(gp);
    if (g == nullptr) {
        fprintf(stderr, "ggml_init failed\n");
        return 1;
    }

    gguf_context * c = gguf_init_empty();
    gguf_set_val_str(c, "general.name", "fg-goose-split-input");
    gguf_set_val_u32(c, "general.file_type", 0);
    gguf_set_val_u32(c, "general.alignment", 32);

    for (int i = 0; i < n_t; i++) {
        const int cols = (ne0_last > 0 && i == n_t - 1) ? ne0_last : ne0;
        ggml_tensor * t = ggml_new_tensor_2d(g, GGML_TYPE_F32, cols, 1);
        char name[64];
        snprintf(name, sizeof name, "blk.%d.weight", i);
        ggml_set_name(t, name);
        auto * p = static_cast<uint8_t *>(t->data);
        const size_t n = ggml_nbytes(t);
        for (size_t k = 0; k < n; k++) {
            p[k] = static_cast<uint8_t>(i * 17 + k * 13 + 1);
        }
        gguf_add_tensor(c, t);
    }

    const bool ok = gguf_write_to_file(c, path, /*only_meta =*/ false);
    gguf_free(c);
    ggml_free(g);
    if (!ok) {
        fprintf(stderr, "write failed\n");
        return 1;
    }
    printf("wrote %s n_tensors=%d ne0=%d\n", path, n_t, ne0);
    return 0;
}
