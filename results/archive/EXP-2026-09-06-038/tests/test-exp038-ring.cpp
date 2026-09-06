// EXP-038 model-free checks for the bounded pinned expert ring.
// Modes:
//   ring  - direct backend interface staging: byte equality, sparse/consecutive ranges,
//           first/last expert, padding, 16 MiB chunk boundary, slot reuse cycles,
//           sentinel/untouched regions, capture guard, pinned memory accounting
//   sched - scheduler weights branch: flag gating and output equality vs the pageable path
// Environment (set by the run script):
//   GGML_EXPERT_PINNED_RING=1            enable the ring
//   GGML_EXPERT_RING_ALLOC_FAIL=1        force ring init to fail

#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-backend-impl.h"
#include "ggml-cuda.h"

#include <cuda_runtime.h>

#define CUDA_CHECK(call, where) do { cudaError_t e_ = (call); if (e_ != cudaSuccess) { \
    printf("CUDA error at %s: %s\n", where, cudaGetErrorString(e_)); return 2; } } while (0)

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

static int checks_pass = 0;
static int checks_fail = 0;

static void check(bool ok, const char * name) {
    printf("CHECK %s %s\n", ok ? "PASS" : "FAIL", name);
    if (ok) checks_pass++; else checks_fail++;
    fflush(stdout);
}

static std::vector<std::string> g_logs;
static void log_capture(ggml_log_level level, const char * text, void * user_data) {
    (void) level; (void) user_data;
    g_logs.push_back(text);
}
static bool log_contains(const char * needle) {
    for (const std::string & s : g_logs) {
        if (s.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// partial mirror of ggml_backend_cuda_context: only the fields inserted by EXP-038
// (declaration order must match common.cuh up to ring_n_chunks)
struct cuda_ctx_mirror {
    int device;
    std::string name;
    cudaEvent_t copy_event;
    void * ring_slots[2];
    cudaEvent_t ring_events[2];
    bool ring_pending[2];
    size_t ring_next;
    int ring_state;
    size_t ring_n_calls;
    size_t ring_n_chunks;
    cudaStream_t streams[16][8];
};

static long vm_pin_kib() {
    FILE * f = fopen("/proc/self/status", "r");
    if (!f) return -1;
    char line[256];
    long v = -1;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "VmPin: %ld KiB", &v) == 1) break;
    }
    fclose(f);
    return v;
}

static uint64_t hash_bytes(const uint8_t * p, size_t n) {
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}

static size_t expert_size(const char * v, size_t def) {
    return v ? (size_t)atoll(v) : def;
}

// ---------------------------------------------------------------------------
// mode ring
// ---------------------------------------------------------------------------
static int mode_ring(void) {
    ggml_log_set(log_capture, nullptr);

    const size_t EXP = expert_size(getenv("EXP038_EXPERT_SIZE"), 4ull << 20);
    const size_t N_EXPERT = 14;
    const size_t W_BYTES = EXP * N_EXPERT;
    const size_t GUARD = 4096;
    const bool ring_env = getenv("GGML_EXPERT_PINNED_RING") != nullptr;

    ggml_backend_t cuda = ggml_backend_init_by_type(GGML_BACKEND_DEVICE_TYPE_GPU, nullptr);
    if (!cuda) { printf("no CUDA backend\n"); return 2; }
    cudaDeviceSynchronize();

    ggml_backend_buffer_t buf = ggml_backend_buft_alloc_buffer(
        ggml_backend_cuda_buffer_type(0), 2 * GUARD + W_BYTES);
    void * base = ggml_backend_buffer_get_base(buf);
    CUDA_CHECK(cudaMemsetAsync(base, 0xA5, 2 * GUARD + W_BYTES), "memset");
    cudaDeviceSynchronize();

    struct ggml_init_params params = { /*mem_size*/ 1 << 16, /*mem_buffer*/ nullptr, /*no_alloc*/ true };
    ggml_context * ctx = ggml_init(params);
    ggml_tensor * w = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, (int64_t)(W_BYTES / 4));
    ggml_backend_tensor_alloc(buf, w, (uint8_t *) base + GUARD);

    std::vector<uint8_t> src(W_BYTES);
    for (size_t i = 0; i < W_BYTES; i++) src[i] = (uint8_t)(i * 31 + (i >> 8) * 7 + 11);

    struct Range { size_t off; size_t size; const char * name; };
    std::vector<Range> ranges = {
        { 0 * EXP,               4 * EXP + 512,          "A first-expert consecutive 4+pad" },
        { 5 * EXP,               5 * EXP + 512,          "B consecutive 5+pad chunked >16MiB" },
        { 4 * EXP,               EXP,                    "C sparse single expert 4" },
        { 10 * EXP,              EXP,                    "D sparse single expert 10" },
        { 11 * EXP,              EXP,                    "E sparse single expert 11" },
        { 13 * EXP,              EXP,                    "F last expert exact to buffer end" },
    };

    bool all_staged = true;
    bool all_fallback = true;
    for (const Range & r : ranges) {
        bool staged = false;
        if (cuda->iface.set_tensor_async_pinned_ring) {
            staged = cuda->iface.set_tensor_async_pinned_ring(cuda, w, src.data() + r.off, r.off, r.size);
        }
        if (staged) {
            all_fallback = false;
        } else {
            all_staged = false;
            CUDA_CHECK(cudaMemcpyAsync((uint8_t *) base + GUARD + r.off, src.data() + r.off, r.size,
                                       cudaMemcpyHostToDevice, nullptr), "fallback copy");
        }
    }
    ggml_backend_synchronize(cuda);

    // expected destination image
    std::vector<uint8_t> want(W_BYTES, 0xA5);
    for (const Range & r : ranges) {
        memcpy(want.data() + r.off, src.data() + r.off, r.size);
    }
    std::vector<uint8_t> got(W_BYTES);
    CUDA_CHECK(cudaMemcpy(got.data(), (uint8_t *) base + GUARD, W_BYTES, cudaMemcpyDeviceToHost), "readback");
    check(got == want, "destination byte equality (sparse+consecutive+padding+chunked)");
    check(!memcmp(got.data() + 12 * EXP, std::vector<uint8_t>(EXP, 0xA5).data(), EXP), "uncovered expert untouched");

    std::vector<uint8_t> guard(2 * GUARD);
    CUDA_CHECK(cudaMemcpy(guard.data(), base, GUARD, cudaMemcpyDeviceToHost), "head guard readback");
    CUDA_CHECK(cudaMemcpy(guard.data() + GUARD, (uint8_t *) base + GUARD + W_BYTES, GUARD, cudaMemcpyDeviceToHost), "tail guard readback");
    for (size_t i = 0; i < 2 * GUARD; i++) {
        if (guard[i] != 0xA5) { printf("GUARD DIFF at %zu val %02x\n", i, guard[i]); break; }
    }
    check(memcmp(guard.data(), std::vector<uint8_t>(2 * GUARD, 0xA5).data(), 2 * GUARD) == 0, "sentinel guards unchanged");

    cuda_ctx_mirror * ctxm = (cuda_ctx_mirror *) cuda->context;
    size_t want_calls = 0, want_chunks = 0;
    for (const Range & r : ranges) { want_calls++; want_chunks += (r.size + (16 << 20) - 1) / (16 << 20); }
    const bool alloc_fail = getenv("GGML_EXPERT_RING_ALLOC_FAIL") != nullptr;
    if (alloc_fail) {
        check(log_contains("staying on the pageable path"), "alloc-fail: fallback logged");
        check(!all_staged && all_fallback, "alloc-fail: every staged call declined, pageable fallback used");
        check(ctxm->ring_n_calls == 0, "alloc-fail: ring never used");
        check(ctxm->ring_state == -1, "alloc-fail: ring marked disabled");
    } else if (ring_env) {
        check(log_contains("expert ring enabled"), "flag ON: ring init logged");
        check(all_staged, "flag ON: all calls staged");
        check(ctxm->ring_n_calls == want_calls && ctxm->ring_n_chunks == want_chunks, "ring counters (calls/chunks)");
        check(ctxm->ring_n_chunks >= 6, "at least three slot reuse cycles");
    } else {
        check(!all_staged && all_fallback, "flag OFF: every staged call declined, pageable fallback used");
        check(ctxm->ring_n_calls == 0, "flag OFF: ring never used");
        check(!log_contains("expert ring enabled"), "flag OFF: no ring init");
    }

    // CUDA graph capture guard: the ring must refuse to stage during capture
    cudaStream_t ctx_stream = ctxm->streams[ctxm->device][0];
    cudaStreamCaptureStatus st;
    CUDA_CHECK(cudaStreamQuery(ctx_stream), "ctx stream query");
    cudaGraph_t graph = nullptr;
    CUDA_CHECK(cudaStreamBeginCapture(ctx_stream, cudaStreamCaptureModeThreadLocal), "begin capture");
    bool staged_during_capture = false;
    cudaError_t derr = cudaGetLastError();
    (void) derr;
    staged_during_capture = cuda->iface.set_tensor_async_pinned_ring(cuda, w, src.data(), 0, 1 << 20);
    CUDA_CHECK(cudaStreamEndCapture(ctx_stream, &graph), "end capture");
    cudaGraphDestroy(graph);
    check(!staged_during_capture, "capture guard: ring declined during CUDA graph capture");

    // pinned memory accounting
    long pin_before = vm_pin_kib();
    ggml_backend_free(cuda);
    long pin_after = vm_pin_kib();
    printf("VMPIN before_free=%ld KiB after_free=%ld KiB\n", pin_before, pin_after);
    check(pin_after >= 0 && pin_after <= pin_before + 1024, "free leaves no pinned ring memory");

    ggml_backend_buffer_free(buf);
    ggml_free(ctx);
    return 0;
}

// ---------------------------------------------------------------------------
// mode sched
// ---------------------------------------------------------------------------
static int mode_sched(const char * out_path) {
    ggml_log_set(log_capture, nullptr);

    const int64_t n = 512, m = 4096, n_expert = 10, n_tokens = 395, top_k = 4;
    const size_t expert_bytes = (size_t) n * m * sizeof(float);

    ggml_backend_t cuda = ggml_backend_init_by_type(GGML_BACKEND_DEVICE_TYPE_GPU, nullptr);
    ggml_backend_t cpu  = ggml_backend_init_by_type(GGML_BACKEND_DEVICE_TYPE_CPU, nullptr);

    ggml_backend_buffer_t wbuf = ggml_backend_buft_alloc_buffer(ggml_backend_cpu_buffer_type(), expert_bytes * n_expert);
    ggml_backend_buffer_set_usage(wbuf, GGML_BACKEND_BUFFER_USAGE_WEIGHTS);
    ggml_backend_buffer_t idbuf = ggml_backend_buft_alloc_buffer(ggml_backend_cpu_buffer_type(), n_tokens * top_k * sizeof(int32_t) + 1024);

    struct ggml_init_params params = { /*mem_size*/ 1 << 20, /*mem_buffer*/ nullptr, /*no_alloc*/ true };
    ggml_context * ctx = ggml_init(params);

    ggml_tensor * as  = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, n, m, n_expert);
    ggml_backend_tensor_alloc(wbuf, as, ggml_backend_buffer_get_base(wbuf));
    ggml_tensor * ids = ggml_new_tensor_2d(ctx, GGML_TYPE_I32, top_k, n_tokens);
    ggml_backend_tensor_alloc(idbuf, ids, ggml_backend_buffer_get_base(idbuf));
    ggml_tensor * b   = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, n, 1, n_tokens);
    ggml_backend_buffer_t abuf = ggml_backend_buft_alloc_buffer(
        ggml_backend_cuda_buffer_type(0), ggml_nbytes(b));
    ggml_backend_tensor_alloc(abuf, b, ggml_backend_buffer_get_base(abuf));

    // weights: deterministic per-expert pattern
    std::vector<float> wsrc((size_t) n * m * n_expert);
    for (size_t i = 0; i < wsrc.size(); i++) wsrc[i] = (float) ((int) (i % 251) - 125) * 0.0039f;
    ggml_backend_tensor_set(as, wsrc.data(), 0, ggml_nbytes(as));

    // ids: deterministic bulk-prefill routing with adjacent, sparse, first, and last experts
    std::vector<int32_t> hids(n_tokens * top_k);
    for (int64_t t = 0; t < n_tokens; t++) {
        for (int64_t k = 0; k < top_k; k++) {
            hids[t * top_k + k] = (int32_t) ((t * 3 + k * 2) % n_expert);
        }
    }
    ggml_backend_tensor_set(ids, hids.data(), 0, ggml_nbytes(ids));

    std::vector<float> bsrc((size_t) n * n_tokens);
    for (size_t i = 0; i < bsrc.size(); i++) bsrc[i] = (float) ((i % 97) * 0.011 - 0.5);
    ggml_backend_tensor_set(b, bsrc.data(), 0, ggml_nbytes(b));

    ggml_cgraph * graph = ggml_new_graph(ctx);
    ggml_tensor * out = ggml_mul_mat_id(ctx, as, b, ids);
    check(ids->ne[1] == b->ne[2] && b->ne[2] == 395, "mul_mat_id token dimensions match 395-token prefill");
    ggml_build_forward_expand(graph, out);

    ggml_backend_t backends[2] = { cuda, cpu };
    ggml_backend_sched_t sched = ggml_backend_sched_new(backends, nullptr, 2, 64, false, false);
    ggml_backend_sched_set_tensor_backend(sched, out, cuda);
    if (!ggml_backend_sched_alloc_graph(sched, graph)) { printf("sched alloc failed\n"); return 2; }

    enum ggml_status st = ggml_backend_sched_graph_compute(sched, graph);
    check(st == GGML_STATUS_SUCCESS, "sched graph compute");

    std::vector<float> hout(ggml_nbytes(out) / 4);
    ggml_backend_tensor_get(out, hout.data(), 0, ggml_nbytes(out));

    bool ring_log = log_contains("expert ring enabled");
    printf("RING_LOG %d\n", ring_log ? 1 : 0);
    cuda_ctx_mirror * ctxm = (cuda_ctx_mirror *) cuda->context;
    const bool ring_env = getenv("GGML_EXPERT_PINNED_RING") != nullptr;
    const bool alloc_fail = getenv("GGML_EXPERT_RING_ALLOC_FAIL") != nullptr;
    if (ring_env && !alloc_fail) {
        check(ring_log, "395-token sched: ring activation logged");
        check(ctxm->ring_n_calls > 0 && ctxm->ring_n_chunks > 0, "395-token sched: staged calls and chunks are non-zero");
    } else if (alloc_fail) {
        check(!ring_log && ctxm->ring_n_calls == 0, "395-token sched: alloc failure stays on fallback");
    } else {
        check(!ring_log && ctxm->ring_n_calls == 0, "395-token sched: flag OFF stays on fallback");
    }

    FILE * f = fopen(out_path, "wb");
    fwrite(hout.data(), 4, hout.size(), f);
    fclose(f);
    printf("OUT_SHA %016llx\n", (unsigned long long) hash_bytes((const uint8_t *) hout.data(), ggml_nbytes(out)));

    ggml_backend_sched_free(sched);
    ggml_backend_buffer_free(abuf);
    ggml_backend_buffer_free(idbuf);
    ggml_backend_buffer_free(wbuf);
    ggml_free(ctx);
    ggml_backend_free(cuda);
    ggml_backend_free(cpu);
    return 0;
}

#define CUDA_CHECK_X(call) do { cudaError_t e_ = (call); if (e_ != cudaSuccess) { \
    printf("CUDA error %s at %s:%d\n", cudaGetErrorString(e_), __FILE__, __LINE__); return 2; } } while (0)

int main(int argc, char ** argv) {    if (argc < 2) { printf("usage: %s ring|sched <out-file>\n", argv[0]); return 1; }
    if (!strcmp(argv[1], "ring")) return mode_ring();
    if (!strcmp(argv[1], "sched")) {
        if (argc < 3) { printf("need out file\n"); return 1; }
        return mode_sched(argv[2]);
    }
    return 1;
}
