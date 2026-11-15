// The recurrence gate: two tokens, where the second reads what the first wrote.
//
// This is the check a single-token gate cannot be. With a zeroed conv state the state
// path is an identity - every node can be bit-exact and the layer is still not
// recurrent. Here token 1's conv input is built from the state token 0 produced.
//
//   state --reshape_3d--> conv_states [kernel-1, channels, 1]
//   conv_input = concat(conv_states, transpose(qkv), 0)
//   conv_out   = ssm_conv(conv_input, kernel)
//   next state = the tail of conv_input, rows (kernel-1+T-keep)..(kernel-1+T-1)
//
// Compared: the conv output, the tail that becomes the next state, and the slot
// offset the destination view is computed from. The tail is then written into the
// state buffer directly, which is what ggml_cpy does inside the graph; driving cpy
// here would only test the backend allocator's view_src handling, which is not a
// decision this project owns.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ggml.h"
#include "ggml-cpu.h"
#include "main.h"

extern void fg_goose_host_ensure_init(void);

// Goose exports. Without these declarations an implicit int return truncates the
// 64-bit handle and the first dereference faults.
extern int64_t fg158_reshape3(int64_t c, int64_t a, int64_t n0, int64_t n1, int64_t n2);
extern int64_t gdn_conv_input(int64_t c, int64_t state, int64_t mixed, int64_t elem);
extern int64_t gdn_conv_tail(int64_t c, int64_t conv_input, int64_t kernel, int64_t channels,
                             int64_t n_seqs, int64_t elem);
extern int64_t gdn_conv_slot(int64_t c, int64_t states_all, int64_t kernel, int64_t channels,
                             int64_t n_seqs, int64_t slot, int64_t mem_size, int64_t kv_head,
                             int64_t elem);

typedef struct { float * conv; int64_t conv_n; float * tail; int64_t tail_n; int64_t slot_off; } step;

static int run_token_goose(const float * state, const float * qkv, const float * kern,
                           int64_t kernel, int64_t channels, int64_t keep, step * out) {
    const size_t elem = sizeof(float);
    struct ggml_init_params gp = { 512u * 1024u * 1024u, NULL, /*.no_alloc =*/ false };
    struct ggml_context * gc = ggml_init(gp);
    if (gc == NULL) return 1;

    struct ggml_tensor * st = ggml_new_tensor_1d(gc, GGML_TYPE_F32, keep * channels);
    memcpy(st->data, state, (size_t) (keep * channels) * sizeof(float));
    struct ggml_tensor * q = ggml_new_tensor_2d(gc, GGML_TYPE_F32, channels, 1);
    memcpy(q->data, qkv, (size_t) channels * sizeof(float));
    struct ggml_tensor * k = ggml_new_tensor_2d(gc, GGML_TYPE_F32, kernel, channels);
    memcpy(k->data, kern, (size_t) (kernel * channels) * sizeof(float));

    struct ggml_tensor * reshaped = (struct ggml_tensor *) (uintptr_t) fg158_reshape3(
        (int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) st, keep, channels, 1);
    struct ggml_tensor * cin = (struct ggml_tensor *) (uintptr_t) gdn_conv_input(
        (int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) reshaped, (int64_t) (uintptr_t) q,
        (int64_t) elem);
    struct ggml_tensor * cout = ggml_ssm_conv(gc, cin, k);
    struct ggml_tensor * tail = (struct ggml_tensor *) (uintptr_t) gdn_conv_tail(
        (int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) cin, kernel, channels, 1,
        (int64_t) elem);
    struct ggml_tensor * dst = (struct ggml_tensor *) (uintptr_t) gdn_conv_slot(
        (int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) st, kernel, channels, 1, 0, 1, 0,
        (int64_t) elem);

    if (!reshaped || !cin || !cout || !tail || !dst) {
        printf("  a Goose node returned null\n");
        ggml_free(gc);
        return 1;
    }

    struct ggml_cgraph * g = ggml_new_graph(gc);
    ggml_build_forward_expand(g, cout);
    ggml_graph_compute_with_ctx(gc, g, 1);

    out->conv_n = ggml_nelements(cout);
    out->conv = (float *) malloc((size_t) out->conv_n * sizeof(float));
    memcpy(out->conv, cout->data, (size_t) out->conv_n * sizeof(float));

    out->tail_n = ggml_nelements(tail);
    out->tail = (float *) malloc((size_t) out->tail_n * sizeof(float));
    memcpy(out->tail, tail->data, (size_t) out->tail_n * sizeof(float));

    out->slot_off = (int64_t) dst->view_offs;
    ggml_free(gc);
    return 0;
}

static int run_token_native(const float * state, const float * qkv, const float * kern,
                            int64_t kernel, int64_t channels, int64_t keep, step * out) {
    const size_t elem = sizeof(float);
    struct ggml_init_params gp = { 512u * 1024u * 1024u, NULL, /*.no_alloc =*/ false };
    struct ggml_context * gc = ggml_init(gp);
    if (gc == NULL) return 1;

    struct ggml_tensor * st = ggml_new_tensor_1d(gc, GGML_TYPE_F32, keep * channels);
    memcpy(st->data, state, (size_t) (keep * channels) * sizeof(float));
    struct ggml_tensor * q = ggml_new_tensor_2d(gc, GGML_TYPE_F32, channels, 1);
    memcpy(q->data, qkv, (size_t) channels * sizeof(float));
    struct ggml_tensor * k = ggml_new_tensor_2d(gc, GGML_TYPE_F32, kernel, channels);
    memcpy(k->data, kern, (size_t) (kernel * channels) * sizeof(float));

    struct ggml_tensor * reshaped = ggml_reshape_3d(gc, st, keep, channels, 1);
    struct ggml_tensor * cin = ggml_concat(gc, reshaped, ggml_transpose(gc, q), 0);
    struct ggml_tensor * cout = ggml_ssm_conv(gc, cin, k);
    // The tail starts at ne[0] - keep rows: this is the native formula.
    const int64_t s_idx = cin->ne[0] - keep;
    struct ggml_tensor * tail = ggml_view_3d(gc, cin, keep, channels, 1,
        cin->nb[1], cin->nb[2], ggml_row_size(cin->type, s_idx));
    struct ggml_tensor * dst = ggml_view_2d(gc, st, keep * channels, 1,
        (size_t) elem * keep * channels, 0);

    struct ggml_cgraph * g = ggml_new_graph(gc);
    ggml_build_forward_expand(g, cout);
    ggml_graph_compute_with_ctx(gc, g, 1);

    out->conv_n = ggml_nelements(cout);
    out->conv = (float *) malloc((size_t) out->conv_n * sizeof(float));
    memcpy(out->conv, cout->data, (size_t) out->conv_n * sizeof(float));

    out->tail_n = ggml_nelements(tail);
    out->tail = (float *) malloc((size_t) out->tail_n * sizeof(float));
    memcpy(out->tail, tail->data, (size_t) out->tail_n * sizeof(float));

    out->slot_off = (int64_t) dst->view_offs;
    ggml_free(gc);
    return 0;
}

int main(void) {
    fg_goose_host_ensure_init();

    const int64_t kernel = 4, channels = 8192, keep = 3;
    const int64_t state_n = keep * channels;

    float * qkv  = (float *) malloc((size_t) channels * sizeof(float));
    float * kern = (float *) malloc((size_t) (kernel * channels) * sizeof(float));
    float * sA   = (float *) calloc((size_t) state_n, sizeof(float));
    float * sB   = (float *) calloc((size_t) state_n, sizeof(float));
    for (int64_t i = 0; i < channels; ++i) qkv[i] = (float) ((i % 17) - 8) / 32.0f;
    for (int64_t i = 0; i < kernel * channels; ++i) kern[i] = (float) ((i % 5) - 2) / 16.0f;

    int bad = 0;
    for (int t = 0; t < 2; ++t) {
        step a = {0}, b = {0};
        if (run_token_goose(sA, qkv, kern, kernel, channels, keep, &a) != 0) return 1;
        if (run_token_native(sB, qkv, kern, kernel, channels, keep, &b) != 0) return 1;

        int dc = 0, dt = 0;
        if (a.conv_n != b.conv_n) { printf("  token %d: conv count differs\n", t); return 1; }
        for (int64_t i = 0; i < a.conv_n; ++i) {
            if (memcmp(&a.conv[i], &b.conv[i], sizeof(float)) != 0) ++dc;
        }
        if (a.tail_n != b.tail_n) { printf("  token %d: tail count differs\n", t); return 1; }
        for (int64_t i = 0; i < a.tail_n; ++i) {
            if (memcmp(&a.tail[i], &b.tail[i], sizeof(float)) != 0) ++dt;
        }
        const int slot_ok = (a.slot_off == b.slot_off);
        printf("  token %d: conv n=%lld differs=%d | tail n=%lld differs=%d | slot_off=%lld ok=%d\n",
               t, (long long) a.conv_n, dc, (long long) a.tail_n, dt,
               (long long) a.slot_off, slot_ok);
        if (dc || dt || !slot_ok) ++bad;

        // Write the tail into the state, which is what ggml_cpy does in the graph.
        memcpy(sA, a.tail, (size_t) a.tail_n * sizeof(float));
        memcpy(sB, b.tail, (size_t) b.tail_n * sizeof(float));
        free(a.conv); free(a.tail); free(b.conv); free(b.tail);
    }

    int state_diff = 0, state_nonzero = 0;
    for (int64_t i = 0; i < state_n; ++i) {
        if (memcmp(&sA[i], &sB[i], sizeof(float)) != 0) ++state_diff;
        if (sA[i] != 0.0f) ++state_nonzero;
    }

    // The state must have CHANGED. An all-zero state after two tokens would mean the
    // tail carried nothing and both arms would agree on zeros while looking correct.
    printf("FG_GDN_RECUR tokens=2 state_elems=%lld state_differs=%d state_nonzero=%d %s\n",
           (long long) state_n, state_diff, state_nonzero,
           (bad == 0 && state_diff == 0 && state_nonzero > 0) ? "PASS" : "FAIL");

    free(qkv); free(kern); free(sA); free(sB);
    return 0;
}
