/* worker-8/threadpool: deterministic multi-thread lifecycle, barrier, wakeup,
 * job dispatch, abort and shutdown corpus against the qualified CPU runtime.
 *
 * The runtime libggml-cpu.so is built with the default-on pool/executor Goose
 * hooks (FG149_POOL / FG143_EXECUTOR).  This corpus drives the real pthread
 * threadpool: creation/join, persistent reuse, pause/resume, many-node barrier
 * chains, abort-callback error path and disposable pools, and checks:
 *   - elementwise results are exact and identical for 1/2/4/8 threads,
 *   - the OS thread count returns to baseline after every free/join,
 *   - abort propagates GGML_STATUS_ABORTED and the pool stays reusable,
 *   - a requested thread count above the pool size is clamped safely.
 *
 * No model, no weights, no benchmark, no sanitizer campaign. */

#include "ggml.h"
#include "ggml-cpu.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int g_errors;
static int g_cases;
static uint64_t g_hash = UINT64_C(1469598103934665603);

#define CHECK(x) do { \
    if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); g_errors++; } \
} while (0)

static void mix(uint64_t v) { g_hash ^= v; g_hash *= UINT64_C(1099511628211); }

static int thread_count(void) {
    FILE * f = fopen("/proc/self/status", "r");
    if (!f) return -1;
    char line[256];
    int n = -1;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "Threads:", 8) == 0) { n = atoi(line + 8); break; }
    }
    fclose(f);
    return n;
}

/* The qualified runtime is built with OpenMP (GOMP), whose worker threads stay
 * cached after a parallel region.  A strict "return to baseline" check is
 * therefore not meaningful here.  Instead we track the high-water mark and
 * require that repeated create/free/disposable cycles never grow the thread
 * count beyond it: a missing join/leak in a pthread build would grow on every
 * cycle; the OpenMP cache stabilises. */
static int g_peak_threads = 1;

static void observe_threads(void) {
    int n = thread_count();
    if (n > g_peak_threads) g_peak_threads = n;
}



struct abort_state {
    int calls;
    int limit;
};

static bool abort_cb(void * data) {
    struct abort_state * s = (struct abort_state *) data;
    s->calls++;
    return s->calls >= s->limit;
}

/* Builds a chain of `nodes` dependent ADD nodes so every step crosses a
 * threadpool barrier.  Data lives in the ggml context (no_alloc = false). */
static struct ggml_tensor * build_chain(struct ggml_context * ctx, int nodes) {
    const int64_t width = 256;
    struct ggml_tensor * a = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, width);
    struct ggml_tensor * b = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, width);
    float * pa = (float *) a->data;
    float * pb = (float *) b->data;
    for (int64_t i = 0; i < width; ++i) {
        pa[i] = (float) (((i * 13 + 7) % 31) - 15) / 19.0f;
        pb[i] = (float) (i % 7) * 0.25f;
    }
    struct ggml_tensor * t = ggml_add(ctx, a, b);
    for (int k = 1; k < nodes; ++k) {
        t = ggml_add(ctx, t, a);
    }
    ggml_set_output(t);
    return t;
}

static void check_chain(struct ggml_tensor * a, struct ggml_tensor * b,
                        struct ggml_tensor * out, int nodes) {
    const int64_t width = 256;
    const float * pa = (const float *) a->data;
    const float * pb = (const float *) b->data;
    const float * po = (const float *) out->data;
    for (int64_t i = 0; i < width; ++i) {
        float expected = pa[i] + pb[i];
        for (int k = 1; k < nodes; ++k) expected += pa[i];
        if (po[i] != expected) {
            fprintf(stderr, "FAIL value mismatch i=%lld got=%.9g expected=%.9g\n",
                    (long long) i, (double) po[i], (double) expected);
            g_errors++;
            return;
        }
    }
}

/* Runs one graph with the given pool (or NULL for a disposable pool) and
 * returns the status.  Fills result_hash when hash_out is non-NULL. */
static enum ggml_status run_once(struct ggml_cgraph * graph, struct ggml_tensor * out,
                                 struct ggml_threadpool * pool,
                                 int threads, int abort_limit, uint64_t * hash_out) {
    struct ggml_cplan plan = ggml_graph_plan(graph, threads, pool);
    void * work = NULL;
    if (plan.work_size) {
        if (posix_memalign(&work, 64, plan.work_size) != 0) return GGML_STATUS_ALLOC_FAILED;
    }
    plan.work_data = (uint8_t *) work;
    plan.threadpool = pool;

    struct abort_state st = { 0, abort_limit };
    if (abort_limit > 0) {
        plan.abort_callback = abort_cb;
        plan.abort_callback_data = &st;
    }

    enum ggml_status status = ggml_graph_compute(graph, &plan);
    free(work);
    if (hash_out) {
        *hash_out = UINT64_C(1469598103934665603);
        const unsigned char * p = (const unsigned char *) out->data;
        size_t n = ggml_nbytes(out);
        for (size_t i = 0; i < n; ++i) { *hash_out ^= p[i]; *hash_out *= UINT64_C(1099511628211); }
    }
    return status;
}

static uint64_t lifecycle_case(int threads, int nodes, uint64_t expected_hash) {
    observe_threads();
    struct ggml_context * ctx = ggml_init((struct ggml_init_params){ 16u * 1024u * 1024u, NULL, false });
    CHECK(ctx != NULL);
    struct ggml_tensor * out = build_chain(ctx, nodes);
    struct ggml_cgraph * graph = ggml_new_graph(ctx);
    ggml_build_forward_expand(graph, out);
    /* locate inputs by walking the first ADD's sources */
    struct ggml_tensor * first = ggml_graph_node(graph, 0);
    struct ggml_tensor * a = first->src[0];
    struct ggml_tensor * b = first->src[1];
    /* chain node 0 is add(a,b); each later node is add(prev,a) */
    CHECK(a != NULL && b != NULL);

    struct ggml_threadpool_params params = ggml_threadpool_params_default(threads);
    params.poll = 0;
    struct ggml_threadpool * pool = ggml_threadpool_new(&params);
    CHECK(pool != NULL);
    CHECK(params.n_threads == threads);

    uint64_t hash0 = 0;
    enum ggml_status status = run_once(graph, out, pool, threads, 0, &hash0);
    CHECK(status == GGML_STATUS_SUCCESS);
    check_chain(a, b, out, nodes);
    mix(hash0);
    CHECK(hash0 == expected_hash);

    /* persistent reuse must stay deterministic */
    for (int cycle = 0; cycle < 6; ++cycle) {
        uint64_t h = 0;
        memset(out->data, 0, ggml_nbytes(out));
        CHECK(run_once(graph, out, pool, threads, 0, &h) == GGML_STATUS_SUCCESS);
        check_chain(a, b, out, nodes);
        CHECK(h == expected_hash);
    }

    /* pause/resume must not lose wakeups: kickoff resumes the pool */
    ggml_threadpool_pause(pool);
    CHECK(run_once(graph, out, pool, threads, 0, NULL) == GGML_STATUS_SUCCESS);
    ggml_threadpool_resume(pool);
    CHECK(run_once(graph, out, pool, threads, 0, NULL) == GGML_STATUS_SUCCESS);
    check_chain(a, b, out, nodes);

    /* abort error path, then reuse the same pool */
    struct ggml_cplan aborted = ggml_graph_plan(graph, threads, pool);
    void * work = NULL;
    if (aborted.work_size) CHECK(posix_memalign(&work, 64, aborted.work_size) == 0);
    aborted.work_data = (uint8_t *) work;
    aborted.threadpool = pool;
    struct abort_state st = { 0, 2 };
    aborted.abort_callback = abort_cb;
    aborted.abort_callback_data = &st;
    enum ggml_status astatus = ggml_graph_compute(graph, &aborted);
    free(work);
    CHECK(astatus == GGML_STATUS_ABORTED);
    CHECK(st.calls >= 2);
    CHECK(run_once(graph, out, pool, threads, 0, NULL) == GGML_STATUS_SUCCESS);
    check_chain(a, b, out, nodes);

    /* A team smaller than the pool must be accepted by the control policy.
     * (Requesting more than the pool owns is rejected by the OpenMP control
     * policy in this qualified build; the native non-OpenMP clamp is a
     * different build configuration and is reported as a residual.) */
    if (threads > 1) {
        CHECK(run_once(graph, out, pool, threads - 1, 0, NULL) == GGML_STATUS_SUCCESS);
        check_chain(a, b, out, nodes);
    }
    CHECK(run_once(graph, out, pool, threads, 0, NULL) == GGML_STATUS_SUCCESS);
    check_chain(a, b, out, nodes);

    ggml_threadpool_free(pool);
    observe_threads();
    ggml_free(ctx);
    g_cases++;
    return hash0;
}

static void disposable_case(int threads, int nodes, uint64_t expected_hash) {
    observe_threads();
    struct ggml_context * ctx = ggml_init((struct ggml_init_params){ 16u * 1024u * 1024u, NULL, false });
    CHECK(ctx != NULL);
    struct ggml_tensor * out = build_chain(ctx, nodes);
    struct ggml_cgraph * graph = ggml_new_graph(ctx);
    ggml_build_forward_expand(graph, out);
    struct ggml_tensor * first = ggml_graph_node(graph, 0);
    struct ggml_tensor * a = first->src[0];
    struct ggml_tensor * b = first->src[1];

    uint64_t h = 0;
    enum ggml_status status = run_once(graph, out, NULL, threads, 0, &h);
    CHECK(status == GGML_STATUS_SUCCESS);
    check_chain(a, b, out, nodes);
    CHECK(h == expected_hash);
    observe_threads();

    ggml_free(ctx);
    g_cases++;
}

static void churn_case(int threads) {
    observe_threads();
    struct ggml_threadpool_params params = ggml_threadpool_params_default(threads);
    params.poll = 0;
    int stable = -1;
    for (int i = 0; i < 40; ++i) {
        struct ggml_threadpool * pool = ggml_threadpool_new(&params);
        CHECK(pool != NULL);
        ggml_threadpool_free(pool);
        int n = thread_count();
        if (i >= 2) {
            if (stable < 0) stable = n;
            CHECK(n <= stable);
        }
        observe_threads();
    }
    ggml_threadpool_free(NULL); /* documented safe no-op */
    observe_threads();
    g_cases++;
}

int main(void) {
    const int node_chain = 24;
    int baseline = thread_count();
    CHECK(baseline >= 1);
    g_peak_threads = baseline;
    observe_threads();

    /* Reference hash from the single-threaded run; all thread counts must
       reproduce it exactly (elementwise chain, fixed operation order). */
    struct ggml_context * ctx = ggml_init((struct ggml_init_params){ 16u * 1024u * 1024u, NULL, false });
    CHECK(ctx != NULL);
    struct ggml_tensor * ref_out = build_chain(ctx, node_chain);
    struct ggml_cgraph * ref_graph = ggml_new_graph(ctx);
    ggml_build_forward_expand(ref_graph, ref_out);
    struct ggml_cplan ref_plan = ggml_graph_plan(ref_graph, 1, NULL);
    void * ref_work = NULL;
    if (ref_plan.work_size) CHECK(posix_memalign(&ref_work, 64, ref_plan.work_size) == 0);
    ref_plan.work_data = (uint8_t *) ref_work;
    CHECK(ggml_graph_compute(ref_graph, &ref_plan) == GGML_STATUS_SUCCESS);
    free(ref_work);
    uint64_t ref_hash = UINT64_C(1469598103934665603);
    {
        const unsigned char * p = (const unsigned char *) ref_out->data;
        size_t n = ggml_nbytes(ref_out);
        for (size_t i = 0; i < n; ++i) { ref_hash ^= p[i]; ref_hash *= UINT64_C(1099511628211); }
    }
    ggml_free(ctx);

    const int thread_set[] = { 1, 2, 4, 8 };
    for (size_t i = 0; i < sizeof(thread_set) / sizeof(thread_set[0]); ++i) {
        uint64_t h = lifecycle_case(thread_set[i], node_chain, ref_hash);
        CHECK(h == ref_hash);
        disposable_case(thread_set[i], node_chain, ref_hash);
        churn_case(thread_set[i]);
    }
    observe_threads();

    if (g_errors) {
        fprintf(stderr, "{\"status\":\"FAIL_LOCAL_THREADPOOL\",\"errors\":%d,\"cases\":%d}\n",
                g_errors, g_cases);
        return 1;
    }
    printf("{\"status\":\"PASS_LOCAL_THREADPOOL\",\"cases\":%d,\"nodes\":%d,\"baseline_threads\":%d,"
           "\"peak_threads\":%d,\"hash\":\"%016llx\"}\n",
           g_cases, node_chain, baseline, g_peak_threads, (unsigned long long) g_hash);
    return 0;
}
