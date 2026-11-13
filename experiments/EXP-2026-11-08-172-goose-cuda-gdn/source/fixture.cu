// EXP168 bounded real-CUDA correctness fixture for the actual public GGML gated delta net API.
// Oracle: fg174s_native_ggml_cuda_op_gated_delta_net and fg174s_native_ggml_cuda_op_gated_delta_net_fused_cache,
// the unchanged native ggml_cuda_op_gated_delta_net[_fused_cache] under the fg174s_ prefix (declared here,
// defined by the parent build; this fixture never re-implements native internals). Candidate: the real public
// entries ggml_cuda_op_gated_delta_net(ctx,dst) and ggml_cuda_op_gated_delta_net_fused_cache(ctx,dst,cache)
// declared by the frozen public header gated_delta_net.cuh; the native runtime picks scalar or MMA paths per
// case, the fixture never dispatches or names a kernel itself.
// Node ABI is not invented: nodes are hand-wired exactly as the native op consumes them (dst->op =
// GGML_OP_GATED_DELTA_NET; src[0..5] = q,k,v,g,beta,state; K = op_params int32[0]); dst is the documented
// packed output (sdk-ggml.h): attention scores [S_v,H_v,n_tokens,n_seqs] followed by K recurrent-state
// snapshot slots [S_v,S_v,H_v,n_seqs] (slot 0 = final state, slot s = s tokens back; when n_tokens < K only
// slots 0..n_tokens-1 are written, when n_tokens > K only slot 0 is guaranteed). The contiguous shape
// {S_v,H_v,T+K*S_v,Nseq} has ggml_nbytes == attention + K slots exactly, so the op's checked-span guard
// passes without assuming any other node shape. Source strides: q/k contiguous rows with identical strides
// (ggml_are_same_stride), v contiguous rows, g/beta/state fully contiguous, state = [S_v,S_v,H_v,n_seqs].
// Fused cache: caller-owned external float allocation {data, slot_stride} (gated_delta_net.cuh), guarded by
// 64-byte 0xA5 canaries; slot_stride padding between rollback slots exercises the fused-cache stride and
// must stay untouched; slot_stride is 0 when K==1. The fused dst snapshot tail must stay 0xA5 (the fused
// entry writes snapshots into the cache instead of dst).
// Comparison policy: candidate-vs-native dst and fused-cache bytes are compared bitwise with zero tolerance
// over identical finite inputs; q/k/v/g/beta/state_in are re-read after every run and must be byte-identical;
// oracle and candidate each own fully separate cudaMalloc buffer sets so equality cannot come from aliasing.
// No CPU reference, no adaptive tolerance, no timings, no microbenchmarks. One case runs the candidate under
// CUDA graph capture + replay. Setup synchronizes the default stream (cudaDeviceSynchronize) after all
// uploads/canary memsets, before the non-blocking ctx.stream() computes.
// Diagnostics are readonly and weak-optional (parent may adapt the precise ABI before compile):
//   fg174s_cuda_host_seen() / fg174s_cuda_device_read(uint64_t out[2]);
//   MMA witness opt-in via -DFG_GDN_MMA_WITNESS: fg174m_cuda_host_seen() / fg174m_cuda_device_read(uint64_t out[2]).
// Rollback GGML_GOOSE_CUDA_GDN=0: the candidate entries route to the public originals; all candidate counters
// must read zero. Otherwise main asserts the active scalar witness nonzero (fg174s_cuda_host_seen() != 0);
// MMA is never required unless FG_GDN_MMA_WITNESS is compiled in. Exit codes: 0 pass, 2 CUDA error, 3 failure.
#include "gated_delta_net.cuh"
#include "ggml-cuda/common.cuh"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

void fg174s_native_ggml_cuda_op_gated_delta_net(ggml_backend_cuda_context &, ggml_tensor *);
void fg174s_native_ggml_cuda_op_gated_delta_net_fused_cache(ggml_backend_cuda_context &, ggml_tensor *,
                                                            ggml_cuda_gated_delta_net_fused_cache);
extern "C" __attribute__((weak)) uint64_t fg174s_cuda_host_seen(void);
extern "C" __attribute__((weak)) int fg174s_cuda_device_read(uint64_t out[2]);
#ifdef FG_GDN_MMA_WITNESS
extern "C" __attribute__((weak)) uint64_t fg174m_cuda_host_seen(void);
extern "C" __attribute__((weak)) int fg174m_cuda_device_read(uint64_t out[2]);
#endif

static size_t checks = 0, cases = 0, paired = 0, fused_cases = 0, captures = 0;
static void ck(cudaError_t e) { if (e != cudaSuccess) { fprintf(stderr, "CUDA %s\n", cudaGetErrorString(e)); std::exit(2); } }
static void req(bool b, const char * s) { ++checks; if (!b) { fprintf(stderr, "FAIL %s\n", s); std::exit(3); } }
static bool rollback() { const char * e = std::getenv("GGML_GOOSE_CUDA_GDN"); return e && std::strcmp(e, "0") == 0; }

// deterministic finite inputs; g is a log-decay gate (native applies expf), kept negative so the state stays bounded
static float xval(size_t i) { const unsigned h = (unsigned) (i * 31337u + 11u); return ((int) (h % 50003u) - 25001) / 131072.0f; } // +-0.19
static float kval(size_t i) { return xval(i + 7919u); }
static float vval(size_t i) { return xval(i + 104729u); }
static float gval(size_t i) { return -(0.05f + 0.12f * (float) (i % 8)); }  // [-0.89,-0.05] -> expf in (0.41,0.95)
static float bval(size_t i) { return 0.5f + 0.03f * (float) (i % 13); }     // (0.5,0.86)
static float sval(size_t i) { const unsigned h = (unsigned) (i * 97u + 5u); return ((int) (h % 16u) - 8) / 128.0f; } // +-0.059

struct Buf {
    static constexpr size_t guard = 64; // 64-byte 0xA5 canary on every buffer head and tail
    unsigned char * raw = nullptr; size_t n = 0;
    void alloc(size_t bytes) { n = bytes + 2 * guard; ck(cudaMalloc(&raw, n)); ck(cudaMemset(raw, 0xA5, n)); }
    ~Buf() { if (raw) ck(cudaFree(raw)); }
    void put(const std::vector<unsigned char> & v) const { ck(cudaMemcpy(raw + guard, v.data(), v.size(), cudaMemcpyHostToDevice)); }
    std::vector<unsigned char> get() const { std::vector<unsigned char> v(n); ck(cudaMemcpy(v.data(), raw, n, cudaMemcpyDeviceToHost)); return v; }
};

static void shape(ggml_tensor & t, int64_t n0, int64_t n1, int64_t n2, int64_t n3) {
    std::memset(&t, 0, sizeof t);
    t.type = GGML_TYPE_F32;
    t.ne[0] = n0; t.ne[1] = n1; t.ne[2] = n2; t.ne[3] = n3;
    t.nb[0] = 4; t.nb[1] = t.nb[0] * (size_t) n0; t.nb[2] = t.nb[1] * (size_t) n1; t.nb[3] = t.nb[2] * (size_t) n2;
}

static void fill(std::vector<unsigned char> & h, const ggml_tensor & t, float (* f)(size_t), bool zero) {
    h.assign((size_t) t.nb[3] * (size_t) t.ne[3], 0xA5);
    size_t at = 0;
    for (int64_t i3 = 0; i3 < t.ne[3]; i3++) for (int64_t i2 = 0; i2 < t.ne[2]; i2++) for (int64_t i1 = 0; i1 < t.ne[1]; i1++) for (int64_t i0 = 0; i0 < t.ne[0]; i0++) {
        const float v = zero ? 0.0f : f(at++);
        std::memcpy(h.data() + (size_t) i0 * t.nb[0] + (size_t) i1 * t.nb[1] + (size_t) i2 * t.nb[2] + (size_t) i3 * t.nb[3], &v, 4);
    }
}

struct Case { const char * id; int S, Hk, Hv, T, N, Nq, K; bool kda, fused, capture, zero_state; int pad; const char * note; };

static const Case CASES[] = {
    {"mma_whole_head_h32",128,16,32,64,1,1,1,false,false,false,false,0,"native whole-head MMA path"},
    {"mma_shared_seq_tail",128,16,16,65,2,1,1,false,false,false,false,0,"shared q/k sequences plus partial MMA chunk"},
    {"mma_fused_cache",128,16,16,65,1,1,2,false,true,false,false,32,"native MMA final-state slot only;other snapshots preserved"},
    {"sv16_t1_k1",         16,  1,  1,   1, 1, 1, 1, false, false, false, false,  0, "native SV16, scalar gate, T1, K1 normal state"},
    {"sv32_t1_k1",         32,  1,  1,   1, 1, 1, 1, false, false, false, false,  0, "native SV32, scalar gate, T1"},
    {"sv64_t1_k1",         64,  1,  1,   1, 1, 1, 1, false, false, false, false,  0, "native SV64, scalar gate, T1"},
    {"sv128_t1_k1",       128,  1,  1,   1, 1, 1, 1, false, false, false, false,  0, "native SV128, scalar gate, T1"},
    {"kda_g_vector",       32,  1,  2,   2, 1, 1, 1, true,  false, false, false,  0, "KDA g vector [S_v,H_v,T,N]"},
    {"scalar_gate",        32,  1,  2,   2, 1, 1, 1, false, false, false, false,  0, "scalar gate [1,H_v,T,N], same shape as kda_g_vector"},
    {"grouped_heads",      32,  2,  4,   3, 2, 2, 1, false, false, false, false,  0, "grouped q/k heads (Hk=2, Hv=4)"},
    {"grouped_shared_seq", 32,  2,  4,   3, 2, 1, 1, true,  false, false, false,  0, "grouped heads, shared q/k seq (rq3=2), KDA gate"},
    {"sv128_t16",         128, 16, 16,  16, 1, 1, 1, false, false, false, false,  0, "MMA boundary: n_tokens<64 keeps the scalar path"},
    {"sv128_t64",         128, 16, 16,  64, 1, 1, 1, false, false, false, false,  0, "MMA-eligible shape T64 (path chosen by native runtime)"},
    {"sv128_t65",         128, 16, 16,  65, 1, 1, 1, false, false, false, false,  0, "MMA-eligible remainder chunk T65"},
    {"sv128_t128",        128, 16, 16, 128, 1, 1, 1, false, false, false, false,  0, "MMA-eligible shape T128"},
    {"keep_rs_k2_t2",      32,  1,  1,   2, 1, 1, 2, false, false, false, false,  0, "keep_rs K2, T==K, both snapshot slots written"},
    {"keep_rs_k4_t4",      32,  1,  1,   4, 1, 1, 4, false, false, false, false,  0, "keep_rs K4, T==K, all four slots written"},
    {"keep_rs_k4_t2",      64,  1,  2,   2, 1, 1, 4, false, false, false, false,  0, "keep_rs K4, T<K: slots 2..3 caller-owned, identical poison"},
    {"zero_state",         32,  1,  1,   2, 1, 1, 1, false, false, false, true,   0, "zero initial recurrent state"},
    {"nseq3",              16,  1,  1,   2, 3, 3, 1, false, false, false, false,  0, "three sequences in one batch"},
    {"fused_k1",           16,  1,  1,   1, 1, 1, 1, false, true,  false, false,  0, "fused cache K1, slot_stride=0, owned external cache"},
    {"fused_k2",           32,  1,  1,   2, 1, 1, 2, false, true,  false, false,  0, "fused cache K2, exact slot_stride"},
    {"fused_stride_pad",   16,  1,  2,   2, 1, 1, 2, false, true,  false, false, 32, "fused cache K2, padded slot_stride (+32 floats)"},
    {"fused_grouped_kda",  32,  2,  4,   3, 1, 1, 1, true,  true,  false, false,  0, "fused cache, grouped heads, KDA gate, K1"},
    {"capture_replay",     32,  1,  1,   4, 1, 1, 2, false, false, true,  false,  0, "one CUDA graph capture/replay of the candidate entry"},
};

struct Side {
    ggml_tensor q, k, v, g, beta, st, dst;
    Buf bq, bk, bv, bg, bb, bs, bd, bc;
};

static void run_case(ggml_backend_cuda_context & ctx, const Case & c) {
    const int64_t S = c.S, T = c.T, N = c.N, K = c.K;
    const size_t slot = (size_t) S * S * c.Hv * N, attn = (size_t) S * c.Hv * T * N;
    const size_t stride = slot + (size_t) c.pad;                      // fused-cache slot_stride in floats
    const size_t cache_span = c.fused ? (size_t) (K - 1) * stride + slot : 0;
    req(c.Hv % c.Hk == 0 && N % c.Nq == 0 && S > 0 && T > 0 && N > 0 && K >= 1, "case shape validity (labeled before launch)");
    req(S == 16 || S == 32 || S == 64 || S == 128, "S_v inside the native scalar switch {16,32,64,128}");
    Side sd[2]; // side 0 oracle, side 1 candidate: fully disjoint actual buffer sets
    auto wire = [&](Side & p) {
        shape(p.q, S, c.Hk, T, c.Nq);
        std::memcpy(&p.k, &p.q, sizeof(ggml_tensor));                 // k: identical shape and strides as q
        shape(p.v, S, c.Hv, T, N);
        shape(p.g, c.kda ? S : 1, c.Hv, T, N);
        shape(p.beta, 1, c.Hv, T, N);
        shape(p.st, S, S, c.Hv, N);
        shape(p.dst, S, c.Hv, T + K * S, N);                          // packed scores + K snapshot slots
        p.dst.op = GGML_OP_GATED_DELTA_NET;
        p.dst.src[0] = &p.q; p.dst.src[1] = &p.k; p.dst.src[2] = &p.v;
        p.dst.src[3] = &p.g; p.dst.src[4] = &p.beta; p.dst.src[5] = &p.st;
        p.dst.op_params[0] = c.K;                                     // native reads K via ggml_get_op_params_i32(dst, 0)
    };
    wire(sd[0]); wire(sd[1]);
    const size_t dst_span = (size_t) ggml_nbytes(&sd[0].dst);
    req(dst_span == (attn + (size_t) K * slot) * 4, "dst span = attention + K snapshot slots");
    std::vector<unsigned char> hq, hk, hv, hg, hb, hs, hcache;
    fill(hq, sd[0].q, xval, false); fill(hk, sd[0].k, kval, false); fill(hv, sd[0].v, vval, false);
    fill(hg, sd[0].g, gval, false); fill(hb, sd[0].beta, bval, false); fill(hs, sd[0].st, sval, c.zero_state);
    if (c.fused) { // slot bytes get the finite state pattern, inter-slot padding stays the 0xA5 canary
        hcache.assign(cache_span * 4, 0xA5);
        for (int j = 0; j < K; j++) for (size_t e = 0; e < slot; e++) {
            const float v = sval((size_t) j * slot + e);
            std::memcpy(hcache.data() + ((size_t) j * stride + e) * 4, &v, 4);
        }
    }
    for (int side = 0; side < 2; side++) {
        Side & p = sd[side];
        p.bq.alloc(hq.size()); p.bk.alloc(hk.size()); p.bv.alloc(hv.size());
        p.bg.alloc(hg.size()); p.bb.alloc(hb.size()); p.bs.alloc(hs.size()); p.bd.alloc(dst_span);
        if (c.fused) p.bc.alloc(cache_span * 4);
        p.q.data = p.bq.raw + Buf::guard; p.k.data = p.bk.raw + Buf::guard; p.v.data = p.bv.raw + Buf::guard;
        p.g.data = p.bg.raw + Buf::guard; p.beta.data = p.bb.raw + Buf::guard; p.st.data = p.bs.raw + Buf::guard;
        p.dst.data = p.bd.raw + Buf::guard;
        p.bq.put(hq); p.bk.put(hk); p.bv.put(hv); p.bg.put(hg); p.bb.put(hb); p.bs.put(hs);
        if (c.fused) p.bc.put(hcache);
        ck(cudaDeviceSynchronize()); // uploads and canary memsets finish before the non-blocking ctx.stream()
    }
    auto invoke = [&](int side, bool native) {
        Side & p = sd[side];
        const bool cap = c.capture && !native; // exactly one capture/replay, on the candidate side
        if (cap) ck(cudaStreamBeginCapture(ctx.stream(), cudaStreamCaptureModeGlobal));
        if (c.fused) {
            const ggml_cuda_gated_delta_net_fused_cache cache{(float *) (p.bc.raw + Buf::guard), (int64_t) (K > 1 ? (int64_t) stride : 0)};
            if (native) fg174s_native_ggml_cuda_op_gated_delta_net_fused_cache(ctx, &p.dst, cache);
            else        ggml_cuda_op_gated_delta_net_fused_cache(ctx, &p.dst, cache);
        } else {
            if (native) fg174s_native_ggml_cuda_op_gated_delta_net(ctx, &p.dst);
            else        ggml_cuda_op_gated_delta_net(ctx, &p.dst);
        }
        if (cap) {
            cudaGraph_t graph; cudaGraphExec_t exec;
            ck(cudaStreamEndCapture(ctx.stream(), &graph));
            ck(cudaGraphInstantiate(&exec, graph, nullptr, nullptr, 0));
            ck(cudaGraphLaunch(exec, ctx.stream()));
            ck(cudaStreamSynchronize(ctx.stream()));
            ck(cudaGraphExecDestroy(exec)); ck(cudaGraphDestroy(graph)); ++captures;
        } else {
            ck(cudaStreamSynchronize(ctx.stream()));
        }
    };
    invoke(0, true); ++paired;
    invoke(1, false);
    ++cases; if (c.fused) ++fused_cases;
    auto fetch = [&](int side) {
        std::vector<std::vector<unsigned char>> v(8);
        v[0] = sd[side].bq.get(); v[1] = sd[side].bk.get(); v[2] = sd[side].bv.get();
        v[3] = sd[side].bg.get(); v[4] = sd[side].bb.get(); v[5] = sd[side].bs.get(); v[6] = sd[side].bd.get();
        if (c.fused) v[7] = sd[side].bc.get();
        return v;
    };
    const std::vector<std::vector<unsigned char>> na = fetch(0), ca = fetch(1);
    auto guards = [&](const std::vector<unsigned char> & v, const char * w) {
        for (size_t i = 0; i < Buf::guard; i++) req(v[i] == 0xA5 && v[v.size() - 1 - i] == 0xA5, w);
    };
    auto unchanged = [&](const std::vector<unsigned char> & v, const std::vector<unsigned char> & h, const char * w) {
        req(std::memcmp(v.data() + Buf::guard, h.data(), h.size()) == 0, w);
    };
    auto finite_span = [&](const std::vector<unsigned char> & v, size_t off, size_t floats, const char * w) {
        for (size_t e = 0; e < floats; e++) {
            float f; std::memcpy(&f, v.data() + Buf::guard + off + e * 4, 4);
            req(std::isfinite(f), w);
        }
    };
    for (int side = 0; side < 2; side++) {
        const std::vector<std::vector<unsigned char>> & v = side ? ca : na;
        guards(v[0], "q head/tail canaries"); guards(v[1], "k head/tail canaries"); guards(v[2], "v head/tail canaries");
        guards(v[3], "g head/tail canaries"); guards(v[4], "beta head/tail canaries"); guards(v[5], "state_in head/tail canaries");
        guards(v[6], "dst head/tail canaries");
        unchanged(v[0], hq, "q bytes unchanged"); unchanged(v[1], hk, "k bytes unchanged"); unchanged(v[2], hv, "v bytes unchanged");
        unchanged(v[3], hg, "g bytes unchanged"); unchanged(v[4], hb, "beta bytes unchanged"); unchanged(v[5], hs, "state_in bytes unchanged");
        finite_span(v[6], 0, c.fused ? attn : attn + slot, "dst bytes finite (scores; + final-state slot on plain path)");
        if (c.fused) {
            guards(v[7], "fused-cache head/tail canaries");
            for (int j = 0; j < K; j++) finite_span(v[7], (size_t) j * stride * 4, slot, "fused-cache slot bytes finite");
        }
    }
    if (na[6] != ca[6]) {fprintf(stderr,"CASE %s\n",c.id);int shown=0;for(size_t off=Buf::guard;off+4<=na[6].size()-Buf::guard&&shown<12;off+=4){uint32_t a,b;float av,bv;memcpy(&a,na[6].data()+off,4);memcpy(&b,ca[6].data()+off,4);memcpy(&av,&a,4);memcpy(&bv,&b,4);if(a!=b){fprintf(stderr,"diff elem=%zu native=%a 0x%08x goose=%a 0x%08x\n",(off-Buf::guard)/4,av,a,bv,b);++shown;}}}
    req(na[6].size() == ca[6].size() && std::memcmp(na[6].data(), ca[6].data(), na[6].size()) == 0,
        "candidate dst bitwise-identical to native oracle (scores + state snapshots)");
    if (c.fused) {
        req(na[7].size() == ca[7].size() && std::memcmp(na[7].data(), ca[7].data(), na[7].size()) == 0,
            "candidate fused cache bitwise-identical to native oracle");
        for (int j = 0; j + 1 < K; j++)
            for (size_t b = ((size_t) j * stride + slot) * 4; b < (size_t) (j + 1) * stride * 4; b++)
                req(ca[7][Buf::guard + b] == 0xA5, "fused-cache inter-slot padding untouched (slot_stride honored)");
        for (size_t b = attn * 4; b < dst_span; b++) req(ca[6][Buf::guard + b] == 0xA5, "fused cache: dst snapshot tail untouched");
    }
}

int main() {
    ggml_backend_cuda_context ctx(0);
    ck(cudaSetDevice(0));
    const bool rb = rollback();
    for (const Case & c : CASES) {
#ifdef FG_GDN_SCALAR_COMPONENT_ONLY
        if (c.S == 128 && c.Hk == 16 && c.T >= 64 && !c.kda) continue;
#endif
        run_case(ctx,c);
    }
    req(cases > 0 && paired == cases, "every case ran native+candidate");
    const bool have_host = (const void *) &fg174s_cuda_host_seen != nullptr;
    const bool have_dev = (const void *) &fg174s_cuda_device_read != nullptr;
    uint64_t dev[2] = {};
    const int devrc = have_dev ? fg174s_cuda_device_read(dev) : -1;
    const uint64_t host = have_host ? fg174s_cuda_host_seen() : 0;
    req(have_host && have_dev && devrc == 0, "required scalar host/device diagnostics available and successful");
    if (have_host) {
        if (rb) req(host == 0, "rollback leaves fg174s host witness 0");
        else    req(host != 0, "active scalar witness nonzero");
    } else {
        fprintf(stderr, "LABEL fg174s_cuda_host_seen unresolved (weak); host witness asserts skipped\n");
    }
    if (have_dev && devrc == 0) {
        if (rb) req(dev[0] == 0 && dev[1] == 0, "rollback leaves fg174s device witness words 0");
        else    req((dev[0] | dev[1]) != 0, "fg174s device witness active");
    }
#ifdef FG_GDN_MMA_WITNESS
    const bool have_mh = (const void *) &fg174m_cuda_host_seen != nullptr;
    const bool have_md = (const void *) &fg174m_cuda_device_read != nullptr;
    uint64_t mdev[2] = {};
    const int mrc = have_md ? fg174m_cuda_device_read(mdev) : -1;
    const uint64_t mhost = have_mh ? fg174m_cuda_host_seen() : 0;
    req(have_mh && have_md && mrc == 0, "required MMA diagnostics available and successful");
    if (have_mh) {
        if (rb) req(mhost == 0, "rollback leaves fg174m host witness 0");
        else    req(mhost != 0, "MMA witness nonzero (FG_GDN_MMA_WITNESS)");
    } else {
        fprintf(stderr, "LABEL fg174m_cuda_host_seen unresolved (weak); MMA witness asserts skipped\n");
    }
    if (have_md && mrc == 0) {
        if (rb) req(mdev[0] == 0 && mdev[1] == 0, "rollback leaves fg174m device witness words 0");
        else    req((mdev[0] | mdev[1]) != 0, "fg174m device witness active");
    }
#endif
    printf("PASS captures=%zu cases=%zu native_paired=%zu fused=%zu checks=%zu rollback=%d host=%llu device=[%llu,%llu] rc=%d\n",
           captures, cases, paired, fused_cases, checks, (int) rb, (unsigned long long) host,
           (unsigned long long) dev[0], (unsigned long long) dev[1], devrc);
    return 0;
}
