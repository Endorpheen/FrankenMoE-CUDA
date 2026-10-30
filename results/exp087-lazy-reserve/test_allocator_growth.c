#include "ggml.h"
#include "ggml-alloc.h"
#include "ggml-backend-impl.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct fake_state {
    size_t requests[64];
    size_t count;
    size_t max_size;
    size_t fail_above;
    int live;
    _Alignas(64) unsigned char storage[65536];
};

static const char * fake_name(ggml_backend_buffer_type_t type) { (void)type; return "growth-test"; }
static size_t fake_align(ggml_backend_buffer_type_t type) { (void)type; return 64; }
static size_t fake_max(ggml_backend_buffer_type_t type) { return ((struct fake_state *)type->context)->max_size; }
static bool fake_host(ggml_backend_buffer_type_t type) { (void)type; return true; }
static void * fake_base(ggml_backend_buffer_t buffer) { return ((struct fake_state *)buffer->context)->storage; }
static void fake_free(ggml_backend_buffer_t buffer) { ((struct fake_state *)buffer->context)->live--; }
static ggml_backend_buffer_t fake_alloc(ggml_backend_buffer_type_t type, size_t size) {
    struct fake_state * state = type->context;
    assert(state->count < 64);
    state->requests[state->count++] = size;
    if (size > state->fail_above) return NULL;
    struct ggml_backend_buffer_i iface = {0};
    iface.get_base = fake_base;
    iface.free_buffer = fake_free;
    state->live++;
    return ggml_backend_buffer_init(type, iface, state, size);
}

static struct ggml_backend_buffer_type make_type(struct fake_state * state) {
    memset(state, 0, sizeof(*state));
    state->max_size = state->fail_above = SIZE_MAX;
    struct ggml_backend_buffer_type type = {0};
    type.context = state;
    type.iface.get_name = fake_name;
    type.iface.alloc_buffer = fake_alloc;
    type.iface.get_alignment = fake_align;
    type.iface.get_max_size = fake_max;
    type.iface.is_host = fake_host;
    return type;
}

static struct ggml_context * context(void) {
    const struct ggml_init_params params = { .mem_size = 2 * 1024 * 1024, .mem_buffer = NULL, .no_alloc = true };
    struct ggml_context * ctx = ggml_init(params);
    assert(ctx);
    return ctx;
}

static struct ggml_cgraph * graph(struct ggml_context * ctx, int side) {
    struct ggml_tensor * a = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, side, side);
    struct ggml_tensor * b = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, side, side);
    ggml_set_input(a);
    ggml_set_input(b);
    struct ggml_tensor * out = ggml_mul_mat(ctx, a, b);
    ggml_set_output(out);
    struct ggml_cgraph * result = ggml_new_graph_custom(ctx, 32, false);
    ggml_build_forward_expand(result, out);
    return result;
}

static size_t measure(ggml_gallocr_t alloc, struct ggml_cgraph * graph) {
    size_t sizes[2] = {0};
    ggml_gallocr_reserve_n_size(alloc, graph, NULL, NULL, sizes);
    return sizes[0];
}

static void basic_cases(void) {
    for (int mode = 0; mode < 7; mode++) {
        struct fake_state state;
        struct ggml_backend_buffer_type type = make_type(&state);
        struct ggml_context * ctx = context();
        struct ggml_cgraph * g = graph(ctx, 16);
        ggml_gallocr_t alloc = ggml_gallocr_new(&type);
        size_t needed = measure(alloc, g);
        assert(needed > 0 && state.count == 0);
        size_t limit = needed * 8;
        if (mode != 0) ggml_gallocr_set_buffer_growth_limits(alloc, &limit);
        if (mode == 2) ggml_gallocr_set_buffer_growth_limits(alloc, NULL);
        if (mode == 3) { limit = 0; ggml_gallocr_set_buffer_growth_limits(alloc, &limit); }
        if (mode == 4) { limit = needed + 128; ggml_gallocr_set_buffer_growth_limits(alloc, &limit); }
        if (mode == 5) { limit = needed / 2; ggml_gallocr_set_buffer_growth_limits(alloc, &limit); }
        if (mode == 6) state.max_size = needed * 2;
        assert(measure(alloc, g) == needed && state.count == 0);
        assert(ggml_gallocr_reserve(alloc, g));
        assert(state.count == 1);
        size_t expected = (mode == 1) ? needed * 4 : (mode == 4) ? limit : (mode == 6) ? state.max_size : needed;
        assert(state.requests[0] == expected);
        assert(ggml_gallocr_get_buffer_size(alloc, 0) == expected);
        assert(ggml_gallocr_alloc_graph(alloc, g));
        if (mode == 1) {
            struct ggml_cgraph * larger = graph(ctx, 20);
            assert(ggml_gallocr_alloc_graph(alloc, larger));
            assert(state.count == 1);
        }
        ggml_gallocr_free(alloc);
        assert(state.live == 0);
        ggml_free(ctx);
    }
}

static void failure_cases(void) {
    for (int both_fail = 0; both_fail < 2; both_fail++) {
        struct fake_state state;
        struct ggml_backend_buffer_type type = make_type(&state);
        struct ggml_context * ctx = context();
        struct ggml_cgraph * g = graph(ctx, 16);
        ggml_gallocr_t alloc = ggml_gallocr_new(&type);
        size_t needed = measure(alloc, g), limit = needed * 8;
        ggml_gallocr_set_buffer_growth_limits(alloc, &limit);
        state.fail_above = both_fail ? needed / 2 : needed * 2;
        assert(ggml_gallocr_reserve(alloc, g) == !both_fail);
        assert(state.count == 2 && state.requests[0] == needed * 4 && state.requests[1] == needed);
        if (both_fail) {
            assert(state.live == 0);
            state.fail_above = SIZE_MAX;
            assert(ggml_gallocr_reserve(alloc, g));
            assert(ggml_gallocr_alloc_graph(alloc, g));
        }
        ggml_gallocr_free(alloc);
        assert(state.live == 0);
        ggml_free(ctx);
    }
}

static void alias_cases(void) {
    for (int disabled = 0; disabled < 2; disabled++) {
        struct fake_state state;
        struct ggml_backend_buffer_type type = make_type(&state);
        ggml_backend_buffer_type_t types[2] = {&type, &type};
        struct ggml_context * ctx = context();
        struct ggml_cgraph * g = graph(ctx, 16);
        ggml_gallocr_t alloc = ggml_gallocr_new_n(types, 2);
        size_t needed = measure(alloc, g), limits[2] = {needed * 4, disabled ? 0 : needed * 2};
        ggml_gallocr_set_buffer_growth_limits(alloc, limits);
        assert(ggml_gallocr_reserve_n(alloc, g, NULL, NULL));
        assert(state.count == 1 && state.requests[0] == (disabled ? needed : needed * 2));
        ggml_gallocr_free(alloc);
        assert(state.live == 0);
        ggml_free(ctx);
    }
}

static void multichunk_case(void) {
    struct fake_state state;
    struct ggml_backend_buffer_type type = make_type(&state);
    state.max_size = 512;
    struct ggml_context * ctx = context();
    struct ggml_cgraph * g = graph(ctx, 8);
    ggml_gallocr_t alloc = ggml_gallocr_new(&type);
    size_t needed = measure(alloc, g), limit = 4096;
    ggml_gallocr_set_buffer_growth_limits(alloc, &limit);
    assert(ggml_gallocr_reserve(alloc, g));
    assert(state.count >= 2);
    size_t total = 0;
    for (size_t i = 0; i < state.count; i++) { assert(state.requests[i] <= 512); total += state.requests[i]; }
    assert(total == needed);
    ggml_gallocr_free(alloc);
    assert(state.live == 0);
    ggml_free(ctx);
}

static void overflow_case(void) {
    struct fake_state state;
    struct ggml_backend_buffer_type type = make_type(&state);
    struct ggml_context * ctx = context();
    struct ggml_tensor * tensor = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, (int64_t)(SIZE_MAX / 12));
    ggml_set_input(tensor);
    ggml_set_output(tensor);
    struct ggml_cgraph * g = ggml_new_graph_custom(ctx, 32, false);
    ggml_build_forward_expand(g, tensor);
    ggml_gallocr_t alloc = ggml_gallocr_new(&type);
    size_t needed = measure(alloc, g), limit = SIZE_MAX - 4096;
    assert(needed > SIZE_MAX / 4 && needed < limit);
    ggml_gallocr_set_buffer_growth_limits(alloc, &limit);
    // This backend records virtual sizes only; the huge buffer is never accessed.
    assert(ggml_gallocr_reserve(alloc, g));
    assert(state.count == 1 && state.requests[0] == limit);
    ggml_gallocr_free(alloc);
    assert(state.live == 0);
    ggml_free(ctx);
}

int main(void) {
    basic_cases();
    failure_cases();
    alias_cases();
    multichunk_case();
    overflow_case();
    puts("PASS: exact/default, NULL reset, bounds, reuse, alias minimum, multichunk, overflow, OOM retry/recovery");
    return 0;
}
