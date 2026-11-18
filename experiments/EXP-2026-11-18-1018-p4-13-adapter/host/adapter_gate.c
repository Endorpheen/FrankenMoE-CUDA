// Differential gate: the Goose adapter against the recorded native reference
// (experiments/EXP-2026-11-18-1014-p4-13-alloc-gate/reference-native*.txt).
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
int64_t fg_adapter_dispatch(int64_t,int64_t,int64_t,int64_t,int64_t);
int fg161_live_count(void); size_t fg161_buffer_size(int64_t); uint64_t fg161_generation(int64_t);
static int64_t g_buft;
static void * worker(void *arg) {
    int *ok = (int *) arg; *ok = 0;
    for (int i = 0; i < 200; ++i) {
        int64_t h = fg_adapter_dispatch(0, g_buft, 3168, 0, 0);
        if (!h) return NULL;
        if (fg161_buffer_size(h) >= 3168) (*ok)++;
        fg_adapter_dispatch(1, h, 0, 0, 0);
    }
    return NULL;
}
int main(void) {
    ggml_backend_t cpu = ggml_backend_cpu_init();
    g_buft = (int64_t)(uintptr_t) ggml_backend_cpu_buffer_type();
    int fails = 0;
    int64_t h1 = fg_adapter_dispatch(0, g_buft, 12288, 0, 0);
    int64_t h2 = fg_adapter_dispatch(0, g_buft, 3168, 0, 0);
    printf("alloc sizes: %zu %zu (expect 12288 3168)\n", fg161_buffer_size(h1), fg161_buffer_size(h2));
    if (fg161_buffer_size(h1) != 12288 || fg161_buffer_size(h2) != 3168) fails++;
    printf("distinct handles: %d live=%d\n", h1 != h2, fg161_live_count());
    if (h1 == h2 || fg161_live_count() != 2) fails++;
    fg_adapter_dispatch(1, h1, 0, 0, 0);
    fg_adapter_dispatch(1, h2, 0, 0, 0);
    int64_t h3 = fg_adapter_dispatch(0, g_buft, 12288, 0, 0);
    printf("reuse handle: %d size=%zu\n", (int)(h3 == h1 || h3 == h2), fg161_buffer_size(h3));
    if (fg161_buffer_size(h3) != 12288) fails++;
    int64_t rs = fg_adapter_dispatch(2, h3, 0, 0, 0);
    printf("reset gen=%lld size_kept=%d\n", (long long) rs, fg161_buffer_size(h3) == 12288);
    if (rs != 1 || fg161_buffer_size(h3) != 12288) fails++;
    fg_adapter_dispatch(1, h3, 0, 0, 0);
    if (fg161_live_count() != 0) { printf("LEAK live=%d\n", fg161_live_count()); fails++; }
    int ok[4] = {0,0,0,0}; pthread_t th[4];
    for (int i = 0; i < 4; ++i) pthread_create(&th[i], NULL, worker, &ok[i]);
    for (int i = 0; i < 4; ++i) pthread_join(th[i], NULL);
    printf("threads: %d,%d,%d,%d (expect 200 each)\n", ok[0],ok[1],ok[2],ok[3]);
    if (ok[0]!=200||ok[1]!=200||ok[2]!=200||ok[3]!=200) fails++;
    ggml_backend_free(cpu);
    printf("GATE %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
