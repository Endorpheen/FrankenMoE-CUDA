// Generated-unit adapter for worker-6 provider.goose.
// Compiles the qualified Goose C emission into a callable unit. It only
// supplies primitive helpers (bit casts, IEEE half conversion, witness
// counters); all numerical work stays in the generated Goose code.
#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>
#include "provider.h"

static uint32_t fg143_bits(float x) { uint32_t v; memcpy(&v, &x, 4); return v; }
static float fg143_from_bits(uint32_t v) { float x; memcpy(&x, &v, 4); return x; }
static float fg143_half(uint16_t v) { return _cvtsh_ss(v); }
static uint16_t fg143_to_half(float v) { return _cvtss_sh(v, _MM_FROUND_TO_NEAREST_INT); }
static float fg141_half(uint16_t v) { return _cvtsh_ss(v); }
static float fg143_codec_from_bits(uint32_t v) { float x; memcpy(&x, &v, 4); return x; }

static atomic_uint_fast64_t fg006_calls[128];
static atomic_uint_fast64_t fg006_values[128];
static void fg143_witness(int64_t op, int64_t values) {
    if (op < 0 || op >= 128 || values < 0) __builtin_trap();
    atomic_fetch_add_explicit(fg006_calls + op, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(fg006_values + op, (uint64_t)values, memory_order_relaxed);
}

#define main fg006_generated_unused_main
#include "provider-generated.c"
#undef main

_Static_assert(sizeof(struct fg006_tensor) == sizeof(gg_type_2), "tensor ABI");
_Static_assert(sizeof(struct fg006_node) == sizeof(gg_type_1), "node ABI");
_Static_assert(offsetof(gg_type_1, f4) == offsetof(struct fg006_node, x), "x ABI");
_Static_assert(offsetof(gg_type_1, f5) == offsetof(struct fg006_node, y), "y ABI");
_Static_assert(offsetof(gg_type_1, f6) == offsetof(struct fg006_node, z), "z ABI");
_Static_assert(offsetof(gg_type_1, f7) == offsetof(struct fg006_node, d), "d ABI");
_Static_assert(offsetof(gg_type_1, f8) == offsetof(struct fg006_node, op), "op ABI");
_Static_assert(offsetof(gg_type_1, f9) == offsetof(struct fg006_node, ith), "ith ABI");
_Static_assert(offsetof(gg_type_1, f10) == offsetof(struct fg006_node, nth), "nth ABI");
_Static_assert(offsetof(gg_type_1, f11) == offsetof(struct fg006_node, ip), "ip ABI");
_Static_assert(offsetof(gg_type_1, f12) == offsetof(struct fg006_node, fp), "fp ABI");

void fg006_goose_workspace(const struct fg006_node * p, int64_t nth, int64_t info[4]) {
    struct fg006_node local = *p;
    local.ith = 0;
    local.nth = nth;
    local.ip[15] = 1464816203;
    uint8_t ok = gg_function_1((gg_type_12 *) &local,
            gg_view(0, 0), gg_view(0, 0), gg_view(0, 0), gg_view(0, 0),
            gg_view(0, 0), gg_view(0, 0), gg_view(info, 4), 0);
    if (!ok) __builtin_trap();
}

uint8_t fg006_goose_validate(const struct fg006_node * p, const void * ids) {
    struct fg006_node local = *p;
    local.ith = 0;
    local.nth = 1;
    local.ip[15] = 1464816204;
    return gg_function_1((gg_type_12 *) &local,
            gg_view(0, 0), gg_view((void *) ids, p->y.bytes), gg_view(0, 0),
            gg_view(0, 0), gg_view(0, 0), gg_view(0, 0), gg_view(0, 0), 0);
}

uint8_t fg006_goose_compute(const struct fg006_node * p,
        const void * x, const void * y, const void * z, void * dst,
        float * scratch, int64_t nf, uint8_t * raw, int64_t raw_bytes,
        int64_t * indices, int64_t ni) {
    return gg_function_1((gg_type_12 *) p,
            gg_view((void *) x, p->x.bytes), gg_view((void *) y, p->y.bytes),
            gg_view((void *) z, p->z.bytes), gg_view(dst, p->d.bytes),
            gg_view(scratch, nf), gg_view(raw, raw_bytes), gg_view(indices, ni), 0);
}

__attribute__((destructor)) static void fg006_report(void) {
    for (int i = 0; i < 128; ++i) {
        uint64_t n = atomic_load(fg006_calls + i);
        if (n) fprintf(stderr, "FG006_GOOSE op=%d calls=%llu values=%llu\n",
                i, (unsigned long long) n,
                (unsigned long long) atomic_load(fg006_values + i));
    }
}
