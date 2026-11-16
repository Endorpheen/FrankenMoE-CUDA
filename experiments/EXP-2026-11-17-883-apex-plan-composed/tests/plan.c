// Apex's own architecture plan, asked of the shipped library rather than of a standalone harness.
//
// The parameter vector uses the same indices apex_shape.c/apex_layer.c use, filled with Apex's GGUF values:
// n_embd 2048, block_count 41 with nextn 1 (so n_layer 40, n_layer_all 41), 16 heads, 2 kv heads,
// full_attention_interval 4, has_output 1.
//
// The expected answer is the one gdn_plan and stack_dispatch already proved: 41 layers, 10 attention,
// 30 recurrent, 1 MTP.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

typedef int64_t (*plan_fn)(const int64_t *, int64_t, int64_t, int64_t *, int64_t);
typedef int (*enabled_fn)(void);

int main(int argc, char ** argv) {
    const char * lib = argc > 1 ? argv[1] : "runtime/goose-stage/lib/libllama.so";
    void * h = dlopen(lib, RTLD_NOW | RTLD_LOCAL);
    if (!h) { printf("FG_Q35_PLAN FAIL dlopen: %s\n", dlerror()); return 1; }

    enabled_fn en = (enabled_fn) dlsym(h, "fg_q35s_enabled");
    plan_fn plan = (plan_fn) dlsym(h, "fg_q35s_plan");
    if (!en || !plan) { printf("FG_Q35_PLAN FAIL symbol missing\n"); return 1; }
    if (!en()) { printf("FG_Q35_PLAN SKIP disabled\n"); return 0; }

    int64_t p[32] = { 0 }, kinds[512] = { 0 };
    p[0] = 2048;   // n_embd
    p[2] = 40;     // n_layer      = block_count - nextn
    p[3] = 41;     // n_layer_all  = n_layer + nextn
    p[4] = 16;     // n_head
    p[5] = 2;      // n_head_kv
    p[17] = 4;     // full_attention_interval
    p[18] = 1;     // has_output

    const int64_t n = plan(p, 32, 0, kinds, 512);
    int64_t c[8] = { 0 };
    for (int64_t i = 0; i < n && i < 512; ++i) {
        if (kinds[i] >= 0 && kinds[i] < 8) c[kinds[i]]++;
    }
    const int ok = n == 41 && c[0] == 10 && c[1] == 30 && c[2] == 1;
    printf("FG_Q35_PLAN layers=%lld attn=%lld recr=%lld mtp=%lld %s\n",
           (long long) n, (long long) c[0], (long long) c[1], (long long) c[2], ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
