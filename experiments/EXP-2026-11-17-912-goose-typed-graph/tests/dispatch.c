// Test transport only; constructor decisions and algorithms remain in Goose.
#include <stdint.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>
#include "runtime.h"

static _Atomic uint64_t seen;
static int enabled = 1;
static int64_t route;

__attribute__((constructor)) static void start_module(void) {
    goose_init(0, NULL);
    const char * value = getenv("GGML_GOOSE_GRAPH_BUILDERS");
    enabled = !(value && strcmp(value, "0") == 0);
}

int ggt_test_enabled(void) { return enabled; }

__attribute__((visibility("default"))) void ggt_test_route(int64_t value) {
    route = value;
}

__attribute__((visibility("default"))) uint64_t fg158_builder_seen(void) {
    return atomic_load_explicit(&seen, memory_order_relaxed);
}

int64_t ggt_dispatch(const uint64_t * values) {
    atomic_fetch_or_explicit(&seen, UINT64_C(1) << values[0], memory_order_relaxed);
#if GGT_HAS_TYPED
    if (route) return gb_common_args((sl_i64) {(int64_t *) (uintptr_t) values, 16});
#endif
    return gb_common((int64_t) (uintptr_t) values);
}
