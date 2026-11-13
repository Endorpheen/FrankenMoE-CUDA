#include "llama.h"
#include <stdio.h>
#include <string.h>
int main(int argc, char ** argv) {
    llama_backend_init();
    struct llama_model_params mp = llama_model_default_params();
    mp.n_gpu_layers = 0;
    struct llama_model * m = llama_model_load_from_file(argv[1], mp);
    if (!m) { printf("LOAD_FAIL\n"); return 1; }
    const int n = llama_model_meta_count(m);
    printf("count %d\n", n);
    unsigned long long h = 1469598103934665603ULL;
    for (int i = 0; i < n; ++i) {
        char k[512] = {0}, v[512] = {0};
        const int rk = llama_model_meta_key_by_index(m, i, k, sizeof(k));
        const int rv = llama_model_meta_val_str_by_index(m, i, v, sizeof(v));
        printf("kv %d %d %.200s=%.200s\n", rk, rv, k, v);
        for (const char * p = k; *p; ++p) { h ^= (unsigned char) *p; h *= 1099511628211ULL; }
        for (const char * p = v; *p; ++p) { h ^= (unsigned char) *p; h *= 1099511628211ULL; }
    }
    printf("hash %llx\n", h);
    /* miss semantics: -1 and buf[0] = 0 */
    char buf[8]; memset(buf, 'X', sizeof(buf));
    const int miss = llama_model_meta_val_str(m, "no.such.key", buf, sizeof(buf));
    printf("miss %d first %d\n", miss, (int) (unsigned char) buf[0]);
    /* truncation semantics: snprintf would-be length, buffer cleared by snprintf */
    char small[4]; memset(small, 'X', sizeof(small));
    const int trunc = llama_model_meta_val_str(m, "general.architecture", small, sizeof(small));
    printf("trunc %d s=%.4s\n", trunc, small);
    /* zero-size buffer */
    const int zero = llama_model_meta_val_str(m, "general.architecture", NULL, 0);
    printf("zero %d\n", zero);
    llama_model_free(m);
    llama_backend_free();
    return 0;
}
