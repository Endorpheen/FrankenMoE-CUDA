#include <stdio.h>
#include <stdint.h>
#include "llama.h"
int main(int argc, char ** argv) {
    llama_backend_init();
    struct llama_model_params mp = llama_model_default_params();
    mp.vocab_only = true; mp.n_gpu_layers = 0;
    struct llama_model * m = llama_model_load_from_file(argv[1], mp);
    if (!m) { printf("load failed\n"); return 1; }
    const int n = llama_model_meta_count(m);
    char key[512], val[4096];
    for (int i = 0; i < n; ++i) {
        int32_t kl = llama_model_meta_key_by_index(m, i, key, sizeof key);
        int32_t vl = llama_model_meta_val_str_by_index(m, i, val, sizeof val);
        printf("META %d %.*s = %.*s\n", i, kl, key, vl, val);
    }
    printf("META_COUNT %d\n", n);
    llama_model_free(m);
    return 0;
}
