// EXP-052 model-free gate harness: exercise the two-variant CUDA-graph slot
// state machine on a tiny synthetic graph and verify eviction/recovery
// scenarios against the control path (graphs disabled).
//
// Scenarios (selected via argv[1]; each run is a separate process so the
// one-shot env seams and per-process diag statics are deterministic):
//
//   alt      S1  shape alternation 3,1,1 x4 on ONE graph key (in-place
//            a->ne[1] mutation, same tensors -> same key, alternating
//            properties). VARIANTS=2 must converge to launch for BOTH slots;
//            VARIANTS=1 must reproduce the EXP-050 trap (0 launches).
//   rebuild  S1b same shape every round, but the ggml context is re-inited on
//            the SAME metadata buffer each round (the llama reset() technique:
//            same tensor addresses -> same key). A captured graph must stay
//            valid across rebuilds: capture once, then launch only.
//   third    S2  warm both variants, then a third shape (7) evicts by LRU,
//            then both original shapes recover to launch. Checksums must equal
//            control for every round.
//   shift    S3  same shape, but a dummy tensor allocated first shifts all
//            metadata addresses -> NEW key -> first_call/capture on the new
//            key while results stay identical.
//   failupd  S4  with GGML_GRAPH_TEST_FAIL_UPDATE=1: after both slots are
//            warm, renaming the NODE tensor (its full struct is part of the
//            slot snapshot) resets the warm slot; the next stable call
//            re-captures and its ExecUpdate fails (seam) -> the slot's
//            instance is destroyed and re-instantiated while the sibling slot
//            keeps launching and the repaired slot launches again afterwards.
//   mem      S6  N distinct graph keys warmed to 2 variants each (VARIANTS=2)
//            vs 1 instance per key (VARIANTS=1). Free-VRAM lines give the
//            measured device cost per extra instance; host cost is exact
//            arithmetic from the sizeof events in the log.
//   sweep    S5  after both slots are warm, sleep 11 s so the 10 s eviction
//            sweep drops the key; the next compute starts over (first_call)
//            and re-converges; teardown must be clean (exit 0).
//
// Build: see run-model-free-exp052.sh. No model files are loaded.

#include "ggml.h"
#include "ggml-alloc.h"
#include "ggml-backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

static const int64_t K = 64; // inner dim
static const int64_t M = 8;  // out rows

struct world {
    ggml_backend_t backend;
    struct ggml_context * ctx;
    void * meta;                 // fixed metadata buffer (llama buf_compute_meta analog)
    size_t meta_size;
    struct ggml_tensor * w, * a, * out;
    // saved data/buffer pointers survive ctx re-init (backend buffer is kept alive)
    void * w_data, * a_data, * out_data;
    ggml_backend_buffer_t w_buf, a_buf, out_buf;
    ggml_backend_buffer_t buf;   // first allocation owns the memory
    int64_t a_cols_alloc;        // allocated columns of a
};

static double checksum(const float * data, int n) {
    double acc = 0.0;
    for (int i = 0; i < n; i++) {
        acc += data[i] * (double) (i + 1);
    }
    return acc;
}

// create tensors in a FIXED order so metadata addresses are deterministic
static void build_tensors(struct world * W) {
    W->w   = ggml_new_tensor_2d(W->ctx, GGML_TYPE_F32, K, M);
    W->a   = ggml_new_tensor_2d(W->ctx, GGML_TYPE_F32, K, W->a_cols_alloc);
    W->out = ggml_mul_mat(W->ctx, W->w, W->a); // [M, a->ne[1]]
}

static void fill_inputs(struct world * W) {
    const int nw = (int) ggml_nelements(W->w);
    const int na = (int) ggml_nelements(W->a);
    float * tmp = malloc(sizeof(float) * (na > nw ? na : nw));
    for (int i = 0; i < nw; i++) {
        tmp[i] = (float) ((i % 17) - 8) * 0.25f;
    }
    ggml_backend_tensor_set(W->w, tmp, 0, sizeof(float) * nw);
    for (int i = 0; i < na; i++) {
        tmp[i] = (float) ((i % 23) - 11) * 0.5f;
    }
    ggml_backend_tensor_set(W->a, tmp, 0, sizeof(float) * na);
    free(tmp);
}

static void world_init(struct world * W, ggml_backend_t backend, ggml_backend_buffer_type_t buft, int64_t a_cols) {
    memset(W, 0, sizeof(*W));
    W->backend = backend;
    W->meta_size = 1 * 1024 * 1024;
    W->meta = aligned_alloc(4096, W->meta_size);
    W->a_cols_alloc = a_cols;

    struct ggml_init_params ip = {
        /*.mem_size   =*/ W->meta_size,
        /*.mem_buffer =*/ W->meta,
        /*.no_alloc   =*/ true,
    };
    W->ctx = ggml_init(ip);
    build_tensors(W);

    ggml_backend_buffer_t buf = ggml_backend_alloc_ctx_tensors_from_buft(W->ctx, buft);
    W->buf = buf;
    // freeze the data/buffer mapping; it survives every ctx re-init below
    W->w_data = W->w->data;   W->w_buf = W->w->buffer;
    W->a_data = W->a->data;   W->a_buf = W->a->buffer;
    W->out_data = W->out->data; W->out_buf = W->out->buffer;

    fill_inputs(W);
}

// llama reset() analog: re-init the ggml context on the SAME metadata buffer
// and re-create the tensors in the same order -> same addresses -> same key
static void world_rebuild(struct world * W, int64_t a_cols) {
    ggml_free(W->ctx);
    struct ggml_init_params ip = {
        /*.mem_size   =*/ W->meta_size,
        /*.mem_buffer =*/ W->meta,
        /*.no_alloc   =*/ true,
    };
    W->ctx = ggml_init(ip);
    W->a_cols_alloc = a_cols;
    build_tensors(W);
    // data lives in the persistent backend buffer; restore the mapping exactly
    W->w->data = W->w_data;     W->w->buffer = W->w_buf;
    W->a->data = W->a_data;     W->a->buffer = W->a_buf;
    W->out->data = W->out_data; W->out->buffer = W->out_buf;
}

static struct ggml_cgraph * build_graph(struct world * W) {
    struct ggml_cgraph * gf = ggml_new_graph(W->ctx);
    ggml_build_forward_expand(gf, W->out);
    return gf;
}

// set the active shape (columns of a) and compute; returns checksum of the
// written extent [M x cols]
static double round_compute(struct world * W, struct ggml_cgraph * gf, int64_t cols) {
    W->a->ne[1] = cols;
    if (ggml_backend_graph_compute(W->backend, gf) != GGML_STATUS_SUCCESS) {
        fprintf(stderr, "gate: FAIL compute cols=%lld\n", (long long) cols);
        exit(1);
    }
    const int n = (int) (M * cols);
    float * host = malloc(sizeof(float) * n);
    ggml_backend_tensor_get(W->out, host, 0, sizeof(float) * n);
    double s = checksum(host, n);
    free(host);
    return s;
}

static void world_free(struct world * W) {
    ggml_free(W->ctx);
    ggml_backend_buffer_free(W->buf);
    free(W->meta);
}

static void print_sums(const char * name, const double * sums, int rounds) {
    printf("gate: scenario=%s rounds=%d", name, rounds);
    for (int r = 0; r < rounds; r++) {
        printf(" r%d=%.6f", r, sums[r]);
    }
    printf("\n");
}

// ---------------------------------------------------------------- scenarios

static ggml_backend_t pick_backend(void) {
    ggml_backend_dev_t dev = ggml_backend_dev_by_type(GGML_BACKEND_DEVICE_TYPE_GPU);
    if (dev == NULL) {
        fprintf(stderr, "gate: FAIL no GPU device\n");
        exit(1);
    }
    ggml_backend_t backend = ggml_backend_dev_init(dev, NULL);
    if (backend == NULL) {
        fprintf(stderr, "gate: FAIL cannot init GPU backend\n");
        exit(1);
    }
    return backend;
}

// S1: alternation 3,1,1 on one key
static int scenario_alt(void) {
    struct world W;
    ggml_backend_t backend = pick_backend();
    ggml_backend_buffer_type_t buft = ggml_backend_get_default_buffer_type(backend);
    world_init(&W, backend, buft, 3);
    struct ggml_cgraph * gf = build_graph(&W);

    const int rounds = 12; // 3,1,1 x4
    double sums[rounds];
    for (int r = 0; r < rounds; r++) {
        static const int64_t pat[3] = { 3, 1, 1 };
        sums[r] = round_compute(&W, gf, pat[r % 3]);
        fprintf(stderr, "gate: alt round %d cols=%lld checksum=%.6f\n", r,
                (long long) pat[r % 3], sums[r]);
    }
    print_sums("alt", sums, rounds);
    world_free(&W);
    ggml_backend_free(backend);
    return 0;
}

// S1b: same shape, ctx rebuilt on the same metadata buffer every round
static int scenario_rebuild(void) {
    struct world W;
    ggml_backend_t backend = pick_backend();
    ggml_backend_buffer_type_t buft = ggml_backend_get_default_buffer_type(backend);
    world_init(&W, backend, buft, 3);

    const int rounds = 6;
    double sums[rounds];
    for (int r = 0; r < rounds; r++) {
        if (r > 0) {
            world_rebuild(&W, 3);
        }
        struct ggml_cgraph * gf = build_graph(&W);
        sums[r] = round_compute(&W, gf, 3);
        // nodes[0] of this single-op forward graph is the out tensor itself
        fprintf(stderr, "gate: rebuild round %d key=%p checksum=%.6f\n", r,
                (void *) W.out, sums[r]);
    }
    print_sums("rebuild", sums, rounds);
    world_free(&W);
    ggml_backend_free(backend);
    return 0;
}

// S2: third shape evicts by LRU, then both originals recover
static int scenario_third(void) {
    struct world W;
    ggml_backend_t backend = pick_backend();
    ggml_backend_buffer_type_t buft = ggml_backend_get_default_buffer_type(backend);
    world_init(&W, backend, buft, 7); // allocate max extent once
    struct ggml_cgraph * gf = build_graph(&W);

    static const int64_t seq[] = { 3, 1, 1, 3, 1, 1,   // warm both variants
                                   7, 7,               // third shape: evict + warm
                                   1, 1,               // shape 1 back (recovery)
                                   3, 3, 3 };          // shape 3 back: reset, capture, launch
    const int rounds = (int) (sizeof(seq) / sizeof(seq[0]));
    double sums[rounds];
    for (int r = 0; r < rounds; r++) {
        sums[r] = round_compute(&W, gf, seq[r]);
        fprintf(stderr, "gate: third round %d cols=%lld checksum=%.6f\n", r,
                (long long) seq[r], sums[r]);
    }
    print_sums("third", sums, rounds);
    world_free(&W);
    ggml_backend_free(backend);
    return 0;
}

// S3: same shape, new metadata addresses -> new key
static int scenario_shift(void) {
    struct world W;
    ggml_backend_t backend = pick_backend();
    ggml_backend_buffer_type_t buft = ggml_backend_get_default_buffer_type(backend);
    world_init(&W, backend, buft, 3);
    struct ggml_cgraph * gf = build_graph(&W);

    const void * key1 = W.out; // nodes[0] == out for this single-op graph
    double sums[6];
    for (int r = 0; r < 3; r++) {
        sums[r] = round_compute(&W, gf, 3);
        fprintf(stderr, "gate: shift phase1 round %d checksum=%.6f\n", r, sums[r]);
    }

    // rebuild on the SAME buffer but allocate a dummy tensor FIRST: every
    // metadata address shifts, so the same shape lands on a NEW key
    ggml_free(W.ctx);
    struct ggml_init_params ip = { W.meta_size, W.meta, true };
    W.ctx = ggml_init(ip);
    struct ggml_tensor * dummy = ggml_new_tensor_2d(W.ctx, GGML_TYPE_F32, 16, 16);
    (void) dummy;
    build_tensors(&W);
    W.w->data = W.w_data;     W.w->buffer = W.w_buf;
    W.a->data = W.a_data;     W.a->buffer = W.a_buf;
    W.out->data = W.out_data; W.out->buffer = W.out_buf;
    gf = build_graph(&W);
    const void * key2 = W.out; // nodes[0] == out for this single-op graph

    for (int r = 3; r < 6; r++) {
        sums[r] = round_compute(&W, gf, 3);
        fprintf(stderr, "gate: shift phase2 round %d checksum=%.6f\n", r, sums[r]);
    }
    printf("gate: shift key1=%p key2=%p %s\n", key1, key2, key1 != key2 ? "DIFFER" : "SAME");
    print_sums("shift", sums, 6);
    world_free(&W);
    ggml_backend_free(backend);
    return key1 != key2 ? 0 : 1;
}

// S4: forced ExecUpdate failure destroys only the owning slot's instance
static int scenario_failupd(void) {
    struct world W;
    ggml_backend_t backend = pick_backend();
    ggml_backend_buffer_type_t buft = ggml_backend_get_default_buffer_type(backend);
    world_init(&W, backend, buft, 3);
    struct ggml_cgraph * gf = build_graph(&W);

    static const int64_t seq[] = { 3, 1, 1, 3, 1, 1,   // both slots warm
                                   3,                   // node renamed -> warm reset
                                   3,                   // stable -> recapture, ExecUpdate forced to fail
                                   1,                   // sibling slot must still launch
                                   3 };                 // re-instantiated slot launches
    const int rounds = (int) (sizeof(seq) / sizeof(seq[0]));
    double sums[rounds];
    for (int r = 0; r < rounds; r++) {
        if (r == 6) {
            // rename the node tensor in place: same key, its full struct is in
            // the slot snapshot -> properties change -> warm reset; the next
            // stable call re-captures and its ExecUpdate path the seam fails
            ggml_format_name(W.out, "exp052_renamed");
        }
        sums[r] = round_compute(&W, gf, seq[r]);
        fprintf(stderr, "gate: failupd round %d cols=%lld checksum=%.6f\n", r,
                (long long) seq[r], sums[r]);
    }
    print_sums("failupd", sums, rounds);
    world_free(&W);
    ggml_backend_free(backend);
    return 0;
}

// S5: eviction sweep drops the key after 10 s idle; recovery; clean teardown
static int scenario_sweep(void) {
    struct world W;
    ggml_backend_t backend = pick_backend();
    ggml_backend_buffer_type_t buft = ggml_backend_get_default_buffer_type(backend);
    world_init(&W, backend, buft, 3);
    struct ggml_cgraph * gf = build_graph(&W);

    static const int64_t seq[] = { 3, 1, 1, 3, 1, 1,  // warm both variants
                                   1, 1, 1 };         // after 11 s sleep: evicted, re-converge
    const int rounds = (int) (sizeof(seq) / sizeof(seq[0]));
    double sums[rounds];
    for (int r = 0; r < rounds; r++) {
        if (r == 6) {
            fprintf(stderr, "gate: sweep sleeping 11 s\n");
            sleep(11);
        }
        sums[r] = round_compute(&W, gf, seq[r]);
        fprintf(stderr, "gate: sweep round %d cols=%lld checksum=%.6f\n", r,
                (long long) seq[r], sums[r]);
    }
    print_sums("sweep", sums, rounds);
    world_free(&W);
    ggml_backend_free(backend);
    fprintf(stderr, "gate: sweep teardown clean\n");
    return 0;
}

// S6: N distinct keys warmed to 2 variants; free-VRAM difference vs VARIANTS=1
// gives the measured device cost of one extra captured instance per key.
static int scenario_mem(void) {
    struct world W;
    ggml_backend_t backend = pick_backend();
    ggml_backend_dev_t dev = ggml_backend_get_device(backend);
    ggml_backend_buffer_type_t buft = ggml_backend_get_default_buffer_type(backend);

    const int N = 100;
    memset(&W, 0, sizeof(W));
    W.meta_size = 16 * 1024 * 1024;
    W.meta = aligned_alloc(4096, W.meta_size);
    struct ggml_init_params ip = { W.meta_size, W.meta, true };
    W.ctx = ggml_init(ip);

    struct ggml_tensor ** w   = malloc(sizeof(*w) * N);
    struct ggml_tensor ** a   = malloc(sizeof(*a) * N);
    struct ggml_tensor ** out = malloc(sizeof(*out) * N);
    for (int k = 0; k < N; k++) {
        struct ggml_tensor * wt = ggml_new_tensor_2d(W.ctx, GGML_TYPE_F32, K, M);
        struct ggml_tensor * at = ggml_new_tensor_2d(W.ctx, GGML_TYPE_F32, K, 3);
        w[k] = wt; a[k] = at; out[k] = ggml_mul_mat(W.ctx, wt, at);
    }
    W.buf = ggml_backend_alloc_ctx_tensors_from_buft(W.ctx, buft);
    for (int k = 0; k < N; k++) {
        float tmp[4096];
        int nw = (int) ggml_nelements(w[k]), na = (int) ggml_nelements(a[k]);
        for (int i = 0; i < nw; i++) tmp[i] = (float) ((i % 17) - 8) * 0.25f;
        ggml_backend_tensor_set(w[k], tmp, 0, sizeof(float) * nw);
        for (int i = 0; i < na; i++) tmp[i] = (float) ((i % 23) - 11) * 0.5f;
        ggml_backend_tensor_set(a[k], tmp, 0, sizeof(float) * na);
    }

    size_t free0 = 0, free1 = 0, total = 0;
    ggml_backend_dev_memory(dev, &free0, &total);
    double agg = 0.0;
    const int64_t pat[4] = { 3, 3, 1, 1 };
    for (int k = 0; k < N; k++) {
        struct ggml_cgraph * gf = ggml_new_graph(W.ctx);
        ggml_build_forward_expand(gf, out[k]);
        for (int c = 0; c < 4; c++) {
            a[k]->ne[1] = pat[c];
            if (ggml_backend_graph_compute(backend, gf) != GGML_STATUS_SUCCESS) {
                fprintf(stderr, "gate: FAIL mem key=%d cols=%lld\n", k, (long long) pat[c]);
                exit(1);
            }
            float host[M * 3];
            ggml_backend_tensor_get(out[k], host, 0, sizeof(float) * (M * pat[c]));
            agg += checksum(host, (int) (M * pat[c]));
        }
    }
    ggml_backend_dev_memory(dev, &free1, &total);
    printf("gate: mem keys=%d free_before=%zu free_after=%zu total=%zu\n", N, free0, free1, total);
    printf("gate: scenario=mem rounds=%d r0=%.6f\n", 4 * N, agg);

    free(w); free(a); free(out);
    ggml_backend_buffer_free(W.buf);
    ggml_free(W.ctx);
    free(W.meta);
    ggml_backend_free(backend);
    return 0;
}

int main(int argc, char ** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s alt|rebuild|third|shift|failupd|sweep|mem\n", argv[0]);
        return 2;
    }
    if (strcmp(argv[1], "alt") == 0)    return scenario_alt();
    if (strcmp(argv[1], "rebuild") == 0) return scenario_rebuild();
    if (strcmp(argv[1], "third") == 0)  return scenario_third();
    if (strcmp(argv[1], "shift") == 0)  return scenario_shift();
    if (strcmp(argv[1], "failupd") == 0) return scenario_failupd();
    if (strcmp(argv[1], "sweep") == 0)  return scenario_sweep();
    if (strcmp(argv[1], "mem") == 0)    return scenario_mem();
    fprintf(stderr, "gate: unknown scenario %s\n", argv[1]);
    return 2;
}
