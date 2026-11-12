// EXP169t bounded CUDA correctness fixture for the actual native topk-moe public API.
// Oracle: fg169t_native_ aliases of the unchanged native ggml_cuda_op_topk_moe / ggml_cuda_should_use_topk_moe.
// Only real public arguments are driven (ctx, logits, weights, ids, clamp, scale, bias, args); no graph src/op_params API.
// Effects/kernel adapter, builds and GPU/Apex gates are parent work; this file has no timings and no execution claims.
// Comparison policy: float outputs bit-exact F32. Documented NaN exception (pattern 3 / nan_ok cases only): float
// outputs may differ only where BOTH sides are NaN (presence must match, payload bytes ignored); ids and guard canaries are always bit-exact.
// ids: [K,rows] I32 with nb[1]=E*4; bytes of positions k>=K per row must remain untouched 0xA5 canaries, except in safe
// ids-over-logits alias cases (ids->data == logits->data with the same row stride; no bias/output overlap and no
// cross-block output/logit compaction alias is exercised). clamp/scale carry only op_params metadata, data=NULL.
// Native CUDA partial-block exit (row >= n_rows early return before __syncthreads) is permitted and is never reported as
// a deadlock from the early return alone; rows 1/7/9/17 exercise partial blocks. Setup synchronizes the default stream
// (cudaDeviceSynchronize) after uploads, before the non-blocking ctx.stream() is used (EXP168 finding).
#include "topk-moe.cuh"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
void fg169t_native_ggml_cuda_op_topk_moe(ggml_backend_cuda_context &, const ggml_tensor *, ggml_tensor *, ggml_tensor *,
                                         const ggml_tensor *, const ggml_tensor *, const ggml_tensor *, const ggml_cuda_topk_moe_args &);
bool fg169t_native_ggml_cuda_should_use_topk_moe(const ggml_tensor *, const ggml_tensor *, const ggml_tensor *, const ggml_tensor *);
extern "C" uint64_t fg169t_cuda_host_seen();
extern "C" int fg169t_cuda_device_read(uint64_t *);
static size_t checks = 0, cases = 0, native_cases = 0, nan_cases = 0, oracle_rows = 0, elig_cases = 0;
static void ck(cudaError_t e) { if (e != cudaSuccess) { fprintf(stderr, "CUDA %s\n", cudaGetErrorString(e)); std::exit(2); } }
static void req(bool b, const char * s) { ++checks; if (!b) { fprintf(stderr, "FAIL %s\n", s); std::exit(3); } }
static void shape(ggml_tensor & t, ggml_type ty, int64_t ne0, int64_t ne1, int64_t ne2, size_t nb1 = 0) {
    std::memset(&t, 0, sizeof t);
    t.type = ty; t.ne[0] = ne0; t.ne[1] = ne1; t.ne[2] = ne2; t.ne[3] = 1;
    t.nb[0] = 4; t.nb[1] = nb1 ? nb1 : t.nb[0] * ne0; t.nb[2] = t.nb[1] * ne1; t.nb[3] = t.nb[2] * ne2;
}
struct Buf {
    unsigned char * raw; size_t n;
    static constexpr size_t guard = 64;
    explicit Buf(size_t bytes) : n(bytes + 2 * guard) { ck(cudaMalloc(&raw, n)); ck(cudaMemset(raw, 0xA5, n)); }
    ~Buf() { ck(cudaFree(raw)); }
    void put(const std::vector<unsigned char> & v) { ck(cudaMemcpy(raw + guard, v.data(), v.size(), cudaMemcpyHostToDevice)); }
    std::vector<unsigned char> get() const { std::vector<unsigned char> v(n); ck(cudaMemcpy(v.data(), raw, n, cudaMemcpyDeviceToHost)); return v; }
};
static float logit_at(size_t i, int pattern) {
    if (pattern == 1) return -20.0f + 0.25f * (float) i; // strictly monotonic logits
    if (pattern == 2) return 0.5f * (float) ((i * 7 + 3) % 5) - 1.0f; // five-way tied logits
    const float base = float((int) ((i * 31337) % 50003) - 25001) / 4096.0f; // deterministic spread
    if (pattern == 3 && i * 31 % 11 == 5) { const uint32_t bits = 0x7fc12345u; float v; std::memcpy(&v, &bits, 4); return v; } // sparse NaN, fixed payload
    return base;
}
static bool feq(const unsigned char * a, const unsigned char * b, bool nan_ok) {
    uint32_t x, y; std::memcpy(&x, a, 4); std::memcpy(&y, b, 4);
    if (x == y) return true; if (!nan_ok) return false;
    float fa, fb; std::memcpy(&fa, &x, 4); std::memcpy(&fb, &y, 4);
    return std::isnan(fa) && std::isnan(fb); // documented NaN policy: presence must match, payload may differ
}
// Independent ID oracle (finite patterns only): score-descending order with lowest-index tie-break, after an
// independent host mirror of the group mask. Covers monotonic and tied logits without reusing native kernel math.
static void oracle_ids(const unsigned char * ids_bytes, const unsigned char * logits_bytes, const unsigned char * bias_bytes,
                       int E, int rows, int K, int mode, int n_groups, int n_group_used) {
    auto bydesc = [](const std::pair<float, int> & a, const std::pair<float, int> & b) {
        return a.first > b.first || (a.first == b.first && a.second < b.second); };
    std::vector<float> v(E), s(E);
    for (int r = 0; r < rows; r++) {
        float mx = -INFINITY, sum = 0.0f;
        for (int e = 0; e < E; e++) { std::memcpy(&v[e], logits_bytes + (size_t)(r * E + e) * 4, 4); mx = fmaxf(mx, v[e]); }
        if (mode == 0) {
            for (int e = 0; e < E; e++) { v[e] = expf(v[e] - mx); sum += v[e]; }
            for (int e = 0; e < E; e++) v[e] /= sum;
        } else for (int e = 0; e < E; e++)
            v[e] = mode == 2 ? 1.0f / (1.0f + expf(-v[e])) : (mode == 3 ? sqrtf(v[e] > 20.0f ? v[e] : logf(1.0f + expf(v[e]))) : v[e]);
        for (int e = 0; e < E; e++) { float b = 0.0f; if (bias_bytes) std::memcpy(&b, bias_bytes + (size_t) e * 4, 4); s[e] = v[e] + b; }
        uint32_t keep = 0; int npg = 0;
        if (n_groups > 0) {
            npg = E / n_groups; std::vector<std::pair<float, int>> gs;
            for (int g = 0; g < n_groups; g++) {
                float t1 = -INFINITY, t2 = -INFINITY;
                for (int e = g * npg; e < (g + 1) * npg; e++) { t2 = fmaxf(t2, fminf(t1, s[e])); t1 = fmaxf(t1, s[e]); }
                gs.emplace_back(t1 + t2, g);
            }
            std::sort(gs.begin(), gs.end(), bydesc);
            for (int g = 0; g < n_group_used; g++) keep |= 1u << gs[g].second;
        }
        std::vector<std::pair<float, int>> ord;
        for (int e = 0; e < E; e++) ord.emplace_back(n_groups > 0 && !((keep >> (e / npg)) & 1) ? -INFINITY : s[e], e);
        std::sort(ord.begin(), ord.end(), bydesc);
        for (int k = 0; k < K; k++) {
            int32_t got; std::memcpy(&got, ids_bytes + (size_t)(r * E + k) * 4, 4);
            req(got == ord[k].second, "independent oracle ids (score desc, lowest-index ties)");
        }
    }
}
static void run(ggml_backend_cuda_context & ctx, int E, int rows, int K, int mode, int pattern,
                bool with_norm, bool with_scale, bool with_bias, int n_groups, int n_group_used, bool alias) {
    const bool nan_ok = pattern == 3;
    const size_t lb = (size_t) E * rows * 4, wb = (size_t) K * rows * 4, ib = (size_t) E * rows * 4, bb = (size_t) E * 4;
    ggml_cuda_topk_moe_args args{};
    args.sigmoid = mode == 2; args.sqrt_softplus = mode == 3; args.softmax = mode == 0 || mode == 1;
    args.delayed_softmax = mode == 1; args.prob_bias = with_bias; args.norm = with_norm; args.scale = with_scale;
    args.n_groups = n_groups; args.n_group_used = n_group_used;
    ggml_tensor lg, wt, idt, cl, sc, bs;
    shape(lg, GGML_TYPE_F32, E, rows, 1);
    shape(wt, GGML_TYPE_F32, 1, K, rows); // packed [1,K,rows,1]
    shape(idt, GGML_TYPE_I32, K, rows, 1, (size_t) E * 4); // nb[1]=E*4, llama.cpp topk-moe layout
    if (with_norm) { shape(cl, GGML_TYPE_F32, 1, 1, 1); const float c = 0.75f; std::memcpy(&cl.op_params[0], &c, 4); }
    if (with_scale) { shape(sc, GGML_TYPE_F32, 1, 1, 1); const float s = 2.5f; std::memcpy(&sc.op_params[0], &s, 4); }
    if (with_bias) shape(bs, GGML_TYPE_F32, E, 1, 1);
    std::vector<unsigned char> hl(lb, 0xA5), hw(wb, 0xA5), hi(ib, 0xA5), hb(bb, 0xA5);
    for (size_t i = 0; i < lb / 4; i++) { const float f = logit_at(i, pattern); std::memcpy(hl.data() + i * 4, &f, 4); }
    for (int e = 0; e < E; e++) { const float f = 0.25f * (float) ((e * 13) % 7 - 3); std::memcpy(hb.data() + e * 4, &f, 4); }
    Buf L(lb), W(wb), I(alias ? 0 : ib), B(with_bias ? bb : 0);
    lg.data = L.raw + Buf::guard; wt.data = W.raw + Buf::guard; idt.data = alias ? lg.data : I.raw + Buf::guard;
    if (with_bias) bs.data = B.raw + Buf::guard;
    auto setup = [&]() {
        L.put(hl); W.put(hw); if (!alias) I.put(hi); if (with_bias) B.put(hb);
        ck(cudaDeviceSynchronize()); // default-stream setup sync before the non-blocking ctx.stream()
    };
    setup();
    fg169t_native_ggml_cuda_op_topk_moe(ctx, &lg, &wt, &idt, with_norm ? &cl : nullptr, with_scale ? &sc : nullptr,
                                        with_bias ? &bs : nullptr, args);
    ck(cudaStreamSynchronize(ctx.stream()));
    const std::vector<unsigned char> nw = W.get(), ni = alias ? L.get() : I.get();
    setup();
    ggml_cuda_op_topk_moe(ctx, &lg, &wt, &idt, with_norm ? &cl : nullptr, with_scale ? &sc : nullptr,
                          with_bias ? &bs : nullptr, args);
    ck(cudaStreamSynchronize(ctx.stream()));
    const std::vector<unsigned char> gw = W.get(), gi = alias ? L.get() : I.get();
    for (size_t i = 0; i < nw.size(); i++) req(feq(nw.data() + i, gw.data() + i, nan_ok), "weights bit-exact vs native");
    req(std::memcmp(ni.data(), gi.data(), ni.size()) == 0, "ids bit-exact vs native");
    if (!alias)
        for (size_t i = 0; i < ib; i++)
            if (((Buf::guard + i) / 4) % (size_t) E >= (size_t) K) req(ni[Buf::guard + i] == 0xA5 && gi[Buf::guard + i] == 0xA5, "untouched ids tail canary");
    for (size_t i = 0; i < Buf::guard; i++) req(nw[i] == 0xA5 && gw[i] == 0xA5 && ni[i] == 0xA5 && gi[i] == 0xA5, "head guard canaries");
    for (size_t i = nw.size() - Buf::guard; i < nw.size(); i++) req(nw[i] == 0xA5 && gw[i] == 0xA5, "weights tail guard canaries");
    for (size_t i = ni.size() - Buf::guard; i < ni.size(); i++) req(ni[i] == 0xA5 && gi[i] == 0xA5, "ids tail guard canaries");
    for (int r = 0; r < rows; r++) {
        std::vector<char> seen(E, 0);
        for (int k = 0; k < K; k++) {
            int32_t id; std::memcpy(&id, gi.data() + Buf::guard + (size_t)(r * E + k) * 4, 4);
            req(id >= 0 && id < E, "ids in range");
            req(!seen[id]++, "ids distinct");
        }
    }
    if (!nan_ok && !alias) { oracle_ids(gi.data() + Buf::guard, hl.data(), with_bias ? hb.data() : nullptr, E, rows, K, mode, n_groups, n_group_used); oracle_rows += rows; }
    if (!nan_ok && !with_bias)
        for (int r = 0; r < rows; r++) for (int k = 1; k < K; k++) {
            float a, b;
            std::memcpy(&a, gw.data() + Buf::guard + ((size_t) r * K + k - 1) * 4, 4);
            std::memcpy(&b, gw.data() + Buf::guard + ((size_t) r * K + k) * 4, 4);
            req(a >= b, "weights monotone (no bias)");
        }
    ++cases; ++native_cases; if (nan_ok) ++nan_cases;
}
int main() {
    ggml_backend_cuda_context ctx(0);
    ck(cudaSetDevice(0));
    const int EL[12] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 288, 512, 576};
    const int RS[5] = {1, 7, 8, 9, 17};
    const bool rollback = std::getenv("GGML_GOOSE_CUDA_TOPK_MOE") && std::strcmp(std::getenv("GGML_GOOSE_CUDA_TOPK_MOE"), "0") == 0;
    auto kk = [](int E, int * ks) { ks[0] = 1; ks[1] = E < 8 ? E : 8; };
    for (int ei = 0; ei < 12; ei++) for (int ri = 0; ri < 5; ri++) { // A: experts x rows x K x modes, random logits
        int ks[2]; kk(EL[ei], ks); const int nk = ks[1] == 1 ? 1 : 2;
        for (int ki = 0; ki < nk; ki++) for (int mode = 0; mode < 4; mode++) run(ctx, EL[ei], RS[ri], ks[ki], mode, 0, false, false, false, 0, 0, false);
    }
    const int EB[7] = {1, 2, 4, 8, 16, 288, 576};
    for (int ei = 0; ei < 7; ei++) for (int ri = 0; ri < 2; ri++) { // B: monotonic/tied/NaN patterns
        int ks[2]; kk(EB[ei], ks); const int nk = ks[1] == 1 ? 1 : 2;
        for (int ki = 0; ki < nk; ki++) for (int mode = 0; mode < 4; mode++) for (int pat = 1; pat < 4; pat++)
            run(ctx, EB[ei], ri ? 17 : 7, ks[ki], mode, pat, false, false, false, 0, 0, false);
    }
    const int EC[4] = {2, 8, 32, 256}, NE[4] = {7, 3, 7, 3}; // C: extra masks bit0=norm clamp 0.75, bit1=scale 2.5, bit2=bias
    const int EX[4][7] = {{1,2,3,4,5,6,7},{2,4,6},{1,2,3,4,5,6,7},{1,2,4}};
    for (int ei = 0; ei < 4; ei++) for (int ri = 0; ri < 2; ri++) {
        int ks[2]; kk(EC[ei], ks); const int nk = ks[1] == 1 ? 1 : 2;
        for (int ki = 0; ki < nk; ki++) for (int mode = 0; mode < 4; mode++) for (int x = 0; x < NE[mode]; x++)
            run(ctx, EC[ei], ri ? 9 : 1, ks[ki], mode, 0, EX[mode][x] & 1, EX[mode][x] & 2, EX[mode][x] & 4, 0, 0, false);
    }
    const int GD[7][3] = {{8,2,1},{8,4,2},{16,4,2},{16,8,2},{32,8,4},{256,8,4},{512,8,4}}; // D: valid grouped combos
    for (int d = 0; d < 7; d++) {
        const int E = GD[d][0], ng = GD[d][1], nu = GD[d][2], cap = nu * (E / ng);
        int ks[2]; kk(E, ks); ks[1] = ks[1] < cap ? ks[1] : cap; const int nk = ks[1] == 1 ? 1 : 2;
        for (int ki = 0; ki < nk; ki++) for (int ri = 0; ri < 2; ri++) for (int mode = 0; mode < 4; mode += 2)
            for (int pat = 0; pat < 4; pat += 2) for (int x = 0; x < 3; x++)
                run(ctx, E, ri ? 17 : 7, ks[ki], mode, pat, x == 2, false, x == 1, ng, nu, false);
    }
    for (int ei = 3; ei < 5; ei++) { // E: safe ids-over-logits alias, same row stride
        int ks[2]; kk(EL[ei], ks); const int nk = ks[1] == 1 ? 1 : 2;
        for (int ki = 0; ki < nk; ki++) for (int ri = 0; ri < 2; ri++) run(ctx, EL[ei], ri ? 17 : 1, ks[ki], 0, 0, false, false, false, 0, 0, true);
    }
    const int EV[16][5] = { // F: eligibility (E, gate 0=softmax/1=unary/2=none, uop, variant, expected)
        {8,0,0,0,1},{8,0,0,1,0},{8,0,0,2,0},{8,0,0,3,0},{8,0,0,4,0},{8,0,0,5,0},{8,0,0,6,0},{8,1,GGML_UNARY_OP_SIGMOID,0,1},
        {8,1,GGML_UNARY_OP_SOFTPLUS,0,1},{8,1,GGML_UNARY_OP_GELU,0,0},{8,2,0,0,1},{288,0,0,0,1},{576,0,0,0,1},{96,0,0,0,0},{1024,0,0,0,0},{8,1,GGML_UNARY_OP_SIGMOID,5,0}};
    for (const auto & ev : EV) {
        const int E = ev[0], gate = ev[1], uop = ev[2], var = ev[3]; const bool expect = ev[4];
        ggml_tensor g, lg, wt, idt, m1, m2;
        shape(lg, GGML_TYPE_F32, E, 1, 1); if (var == 5) lg.nb[1] += 16;
        shape(wt, GGML_TYPE_F32, 1, 8, 1); if (var == 6) wt.nb[1] = 16;
        shape(idt, GGML_TYPE_I32, 8, 1, 1, (size_t) E * 4);
        shape(g, GGML_TYPE_F32, 4, 1, 1); shape(m1, GGML_TYPE_F32, 4, 1, 1); shape(m2, GGML_TYPE_F32, 4, 1, 1);
        if (gate == 0) { g.op = GGML_OP_SOFT_MAX; const float p[2] = {var == 1 ? 2.0f : 1.0f, var == 2 ? 0.5f : 0.0f}; std::memcpy(g.op_params, p, 8); }
        else if (gate == 1) { g.op = GGML_OP_UNARY; g.op_params[0] = uop; }
        g.src[0] = &lg; if (var == 3) g.src[1] = &m1; if (var == 4) g.src[2] = &m2;
        req(fg169t_native_ggml_cuda_should_use_topk_moe(&g, &wt, &lg, &idt) == expect, "eligibility native verdict");
        req(ggml_cuda_should_use_topk_moe(&g, &wt, &lg, &idt) == expect, "eligibility goose verdict/parity");
        ++elig_cases;
    }
    uint64_t dev[2] = {}; const int devrc = fg169t_cuda_device_read(dev);
    const uint64_t host = fg169t_cuda_host_seen();
    if (rollback) {
        req(host == 0, "rollback leaves host counter 0");
        req(dev[0] == 0 && dev[1] == 0, "rollback leaves device words 0");
    } else {
        req((host & 1) != 0, "host compute bit witnessed");
        req((host & 2) != 0, "host eligibility bit witnessed");
        req(dev[0] != 0, "device expert_count_mask witnessed");
        req(dev[1] != 0, "device flags_mask witnessed"); // full flag coverage is intentionally not required in one Apex run
    }
    printf("PASS cases=%zu native_paired=%zu nan_policy=%zu oracle_rows=%zu eligibility=%zu checks=%zu rollback=%d host=%llu device=[%llu,%llu] rc=%d\n",
           cases, native_cases, nan_cases, oracle_rows, elig_cases, checks, (int) rollback,
           (unsigned long long) host, (unsigned long long) dev[0], (unsigned long long) dev[1], devrc);
    return 0;
}
