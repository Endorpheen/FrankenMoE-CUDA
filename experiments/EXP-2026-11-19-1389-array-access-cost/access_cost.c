/* The cost of reading one string element through the port's reader, as a function of the element index.
 * The encoder's table needs 248 320 token strings and 247 587 merge strings; if this is linear per call the
 * table build is quadratic and will never finish inside a load. */
#include "llama.h"
#include <stdio.h>
#include <stdint.h>
#include <time.h>

typedef int (*gr_find_fn)(void *, int64_t, int64_t, int64_t, void *);

extern int fg_port_array_str(int64_t key_index, int64_t element, const uint8_t ** ptr, int64_t * len);

static double now_ms(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

int main(int argc, char ** argv) {
    const char * path = argc > 1 ? argv[1] : "/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf";
    llama_backend_init();
    struct llama_model_params mp = llama_model_default_params();
    mp.n_gpu_layers = 0;
    mp.vocab_only = true;
    struct llama_model * model = llama_model_load_from_file(path, mp);
    if (!model) { printf("load failed\n"); return 2; }

    const int64_t probes[] = { 0, 1000, 10000, 100000, 240000 };
    for (unsigned i = 0; i < sizeof(probes)/sizeof(probes[0]); ++i) {
        const uint8_t * p = 0; int64_t n = 0;
        const double t0 = now_ms();
        int reps = 0;
        for (int r = 0; r < 20; ++r) { if (fg_port_array_str(0, probes[i], &p, &n)) reps++; }
        const double t1 = now_ms();
        printf("element %7lld: %7.3f ms per call (%d ok)\n", (long long) probes[i], (t1 - t0) / 20.0, reps);
    }
    llama_model_free(model);
    llama_backend_free();
    return 0;
}
