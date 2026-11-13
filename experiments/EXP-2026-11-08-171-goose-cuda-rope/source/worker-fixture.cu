// EXP172r bounded CUDA correctness fixture for the actual native public RoPE API (rope.cu / rope.cuh).
// Candidate: public ggml_cuda_op_rope / ggml_cuda_op_rope_back(ctx, dst) of the goose fg172r build.
// Oracle: fg172r_native_ aliases of the unchanged native ggml_cuda_op_rope / ggml_cuda_op_rope_back.
// Only real public arguments are driven: ctx, dst with dst->src[0]=x (F32/F16), dst->src[1]=pos (I32),
// dst->src[2]=freq_factors (F32, optional) and op_params laid out exactly as native rope.cu reads them
// (int32 [1]=n_dims [2]=mode [4]=n_ctx_orig [11..14]=mrope sections [15]=n_offs; float [5]=freq_base
// [6]=freq_scale [7]=ext_factor [8]=attn_factor [9]=beta_fast [10]=beta_slow). The fused entries
// ggml_cuda_op_rope_fused / ggml_cuda_op_rms_norm_mul_rope_fused are fg172r op ids 2/3 (rejected upstream)
// and are not exercised. Effects/kernel adapter, builds and GPU/Apex gates are parent work; this file has
// no timings and makes no compilation or execution claims.
// Apex path: ne00=256, n_dims=64, sections {11,11,10,0}, freq_base 1e7, n_ctx_orig 40960, modes
// GGML_ROPE_TYPE_IMROPE(40) / GGML_ROPE_TYPE_MROPE(8) as native constants (rope_multi half pairs with
// separate t/h/w pos slots t|h|w|extra), plus GGML_ROPE_TYPE_NORMAL(0) / GGML_ROPE_TYPE_NEOX(2) coverage,
// sequence position sets {0}, {1}, {large=100000} and one mixed set; n_dims=64 < ne00=256 keeps a large
// unrotated tail that must be copied exactly; one case exercises n_offs=8 and one n_dims==ne00 (empty tail).
// Comparison policy: every dst byte bit-exact candidate vs native (F32/F16 payload and unwritten 0xA5
// canaries alike), 64B head/tail guards per buffer, x/pos/freq_factors immutable inputs verified after
// every run, unrotated tail (elements outside [n_offs, n_offs+n_dims) in channel space for every layout)
// equal to the input bitwise in both outputs. All inputs finite; native finite bitwise equality expected,
// no NaN exception exists.
// Independent sincos oracle, declared a priori, never relaxed, and never weakening the unconditional native
// bitwise comparison: all-positions-0 cases require rotated outputs to equal the input exactly (cos(0)=1,
// sin(0)=0); all-positions-1 F32 forward cases are recomputed independently in double on the host with a
// fixed 16-ULP relative tolerance (floor |v|>=1). Large-position cases (theta up to ~1e5 rad) are
// intentionally oracle-free: host vs device powf divergence scales with |theta| and exceeds any honest
// result-ULP bound, so those cases rely solely on the bitwise native comparison. F16 and backward cases are
// covered by the bitwise policy and tail checks only.
// Setup uploads with explicit cudaMemcpy and synchronizes the default stream (cudaDeviceSynchronize) before
// the non-blocking ctx.stream() is used (EXP168 finding). Native partial-block early exit (i0 >= ne00) is
// permitted and is never reported as a deadlock. Diagnostics fg172r_cuda_host_seen() /
// fg172r_cuda_device_read(uint64_t out[2]) are gated on GGML_GOOSE_CUDA_ROPE=0 (rollback: host counter and
// both device words must read 0; otherwise host bit0 compute witness is required); device word
// interpretation is parent-adaptable and the values are only printed.
#include "rope.cuh"
#include <cuda_fp16.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

void fg172r_native_ggml_cuda_op_rope(ggml_backend_cuda_context &, ggml_tensor *);
void fg172r_native_ggml_cuda_op_rope_back(ggml_backend_cuda_context &, ggml_tensor *);
extern "C" uint64_t fg172r_cuda_host_seen();
extern "C" int fg172r_cuda_device_read(uint64_t *);

static size_t checks = 0, cases = 0, f16_cases = 0, back_cases = 0, ff_cases = 0, oracle_pairs = 0;
static void ck(cudaError_t e) { if (e != cudaSuccess) { fprintf(stderr, "CUDA %s\n", cudaGetErrorString(e)); std::exit(2); } }
static void req(bool b, const char * s) { ++checks; if (!b) { fprintf(stderr, "FAIL %s\n", s); std::exit(3); } }
static void shape(ggml_tensor & t, ggml_type ty, int64_t ne0, int64_t ne1, int64_t ne2, size_t nb1 = 0) {
    std::memset(&t, 0, sizeof t);
    t.type = ty; t.ne[0] = ne0; t.ne[1] = ne1; t.ne[2] = ne2; t.ne[3] = 1;
    t.nb[0] = ggml_type_size(ty); t.nb[1] = nb1 ? nb1 : t.nb[0] * ne0; t.nb[2] = t.nb[1] * ne1; t.nb[3] = t.nb[2] * ne2;
}
struct Buf {
    unsigned char * raw; size_t n;
    static constexpr size_t guard = 64;
    explicit Buf(size_t bytes) : n(bytes + 2 * guard) { ck(cudaMalloc(&raw, n)); ck(cudaMemset(raw, 0xA5, n)); }
    ~Buf() { ck(cudaFree(raw)); }
    void put(const std::vector<unsigned char> & v) { ck(cudaMemcpy(raw + guard, v.data(), v.size(), cudaMemcpyHostToDevice)); }
    std::vector<unsigned char> get() const { std::vector<unsigned char> v(n); ck(cudaMemcpy(v.data(), raw, n, cudaMemcpyDeviceToHost)); return v; }
};
static float xval(size_t e) { // deterministic finite inputs, occasional large-magnitude spikes
    const float base = (float) ((int) ((e * 7 + 3) % 13) - 6) * 0.25f;
    return (e % 64 == 0) ? base * 1024.0f : base;
}
static void putv(std::vector<unsigned char> & b, size_t off, float v, bool f16) {
    if (f16) { const __half h = __float2half(v); std::memcpy(b.data() + off, &h, 2); } else std::memcpy(b.data() + off, &v, 4);
}
static void posfill(std::vector<unsigned char> & hp, int64_t ns, int pset, bool multi) { // separate t|h|w|extra slots
    const int L = 100000;
    for (int64_t i = 0; i < ns; i++) for (int k = 0; k < (multi ? 4 : 1); k++) {
        int v;
        if (pset == 0) v = 0; else if (pset == 1) v = 1; else if (pset == 2) v = L;
        else v = k == 0 ? (int) i : (k == 1 ? 1 : (k == 2 ? L - 13 : 40960 + (int) i));
        std::memcpy(hp.data() + (size_t) (k * ns + i) * 4, &v, 4);
    }
}
static void fill_params(ggml_tensor & t, int n_dims, int mode, int n_offs) {
    std::memset(t.op_params, 0, sizeof t.op_params);
    t.op_params[1] = n_dims; t.op_params[2] = mode; t.op_params[4] = 40960;
    const float pv[6] = {10000000.0f, 1.0f, 0.0f, 1.0f, 32.0f, 1.0f};
    for (int i = 0; i < 6; i++) std::memcpy(&t.op_params[5 + i], &pv[i], 4);
    const int sect[4] = {11, 11, 10, 0};
    std::memcpy(&t.op_params[11], sect, sizeof sect);
    t.op_params[15] = n_offs;
}
static float ulpf(float v) { const float n = std::nextafter(v, v + std::fabs(v) + 1.0f); return std::fabs(n - v); }
static bool near_tol(float got, double want) { // fixed 16-ULP relative tolerance, declared before any run
    const float w = (float) want;
    return got == w || std::fabs(got - w) <= 16.0f * ulpf(std::fmax(std::fabs(w), 1.0f));
}
static void run(ggml_backend_cuda_context & ctx, int mode, int64_t ne0, int n_dims, int n_offs,
                int64_t nh, int64_t ns, int f16, int back, int ff_on, int pset, int strided) {
    const ggml_type ty = f16 ? GGML_TYPE_F16 : GGML_TYPE_F32;
    const size_t es = ggml_type_size(ty);
    const bool multi = (mode & GGML_ROPE_TYPE_MROPE) != 0; // MROPE(8) or IMROPE(40): rope_multi half pairs
    const bool neox = (mode & GGML_ROPE_TYPE_NEOX) != 0;
    const int64_t np = multi ? 4 * ns : ns; // pos I32 count: t|h|w|extra slots for multi, plain sequence otherwise
    const int64_t nf = n_dims / 2;
    ggml_tensor x, dc, dn, ps, ff;
    shape(x, ty, ne0, nh, ns, strided ? (size_t) ((ne0 + 16) * es) : 0);
    shape(dc, ty, ne0, nh, ns);
    shape(dn, ty, ne0, nh, ns);
    shape(ps, GGML_TYPE_I32, np, 1, 1);
    if (ff_on) shape(ff, GGML_TYPE_F32, nf, 1, 1);
    fill_params(dc, n_dims, mode, n_offs);
    std::memcpy(dn.op_params, dc.op_params, sizeof dn.op_params);
    dc.op = dn.op = back ? GGML_OP_ROPE_BACK : GGML_OP_ROPE;
    dc.src[0] = dn.src[0] = &x; dc.src[1] = dn.src[1] = &ps; dc.src[2] = dn.src[2] = ff_on ? &ff : nullptr;
    const size_t xb = (size_t) x.nb[3], db = (size_t) dc.nb[3];
    const int64_t xs01 = x.nb[1] / es, xs02 = x.nb[2] / es;
    std::vector<unsigned char> hx(xb, 0xA5), hp((size_t) np * 4, 0xA5), hf(ff_on ? (size_t) nf * 4 : 0, 0xA5), hd(db, 0xA5);
    for (int64_t i2 = 0; i2 < ns; i2++) for (int64_t i1 = 0; i1 < nh; i1++) for (int64_t i0 = 0; i0 < ne0; i0++)
        putv(hx, ((size_t) (i1 * xs02 + i2) * xs01 + i0) * es, xval(i0 + i1 * ne0 + i2 * ne0 * nh), f16);
    posfill(hp, ns, pset, multi);
    for (int64_t j = 0; j < nf; j++) { const float v = 1.0f + 0.125f * (float) (j % 7); std::memcpy(hf.data() + j * 4, &v, 4); }
    Buf X(xb), P((size_t) np * 4), F(ff_on ? (size_t) nf * 4 : 0), DC(db), DN(db);
    x.data = X.raw + Buf::guard; ps.data = P.raw + Buf::guard;
    dc.data = DC.raw + Buf::guard; dn.data = DN.raw + Buf::guard;
    if (ff_on) ff.data = F.raw + Buf::guard;
    auto setup = [&]() {
        X.put(hx); P.put(hp); if (ff_on) F.put(hf); DC.put(hd); DN.put(hd);
        ck(cudaDeviceSynchronize()); // default-stream setup sync before the non-blocking ctx.stream()
    };
    setup();
    if (back) fg172r_native_ggml_cuda_op_rope_back(ctx, &dn); else fg172r_native_ggml_cuda_op_rope(ctx, &dn);
    ck(cudaStreamSynchronize(ctx.stream()));
    const std::vector<unsigned char> bn = DN.get(), bxn = X.get(), bpn = P.get(), bfn = ff_on ? F.get() : std::vector<unsigned char>();
    setup();
    if (back) ggml_cuda_op_rope_back(ctx, &dc); else ggml_cuda_op_rope(ctx, &dc);
    ck(cudaStreamSynchronize(ctx.stream()));
    const std::vector<unsigned char> bc = DC.get(), bxc = X.get(), bpc = P.get(), bfc = ff_on ? F.get() : std::vector<unsigned char>();
    req(bn == bc, "dst bit-exact vs native (payload and canaries)");
    for (size_t i = 0; i < Buf::guard; i++) req(bn[i] == 0xA5 && bc[i] == 0xA5, "head guard canaries");
    for (size_t i = bn.size() - Buf::guard; i < bn.size(); i++) req(bn[i] == 0xA5 && bc[i] == 0xA5, "tail guard canaries");
    auto intact = [&](const std::vector<unsigned char> & g, const std::vector<unsigned char> & h) {
        if (h.empty()) return g.empty();
        for (size_t i = 0; i < Buf::guard; i++) if (g[i] != 0xA5 || g[g.size() - Buf::guard + i] != 0xA5) return false;
        return std::memcmp(g.data() + Buf::guard, h.data(), h.size()) == 0;
    };
    req(intact(bxn, hx) && intact(bxc, hx), "x immutable with intact guards");
    req(intact(bpn, hp) && intact(bpc, hp), "pos immutable with intact guards");
    req(!ff_on || (intact(bfn, hf) && intact(bfc, hf)), "freq_factors immutable with intact guards");
    for (int64_t i2 = 0; i2 < ns; i2++) for (int64_t i1 = 0; i1 < nh; i1++) for (int64_t c = 0; c < ne0; c++) {
        if (c >= n_offs && c < n_offs + n_dims) continue; // rotated window in channel space for every layout
        const size_t xo = ((size_t) (i1 * xs02 + i2) * xs01 + c) * es, dof = ((size_t) (i1 * ne0 + i2) * ne0 + c) * es;
        req(std::memcmp(bc.data() + Buf::guard + dof, hx.data() + xo, es) == 0 &&
            std::memcmp(bn.data() + Buf::guard + dof, hx.data() + xo, es) == 0, "tail copied exact");
    }
    if (!f16 && !back && pset <= 1) { // independent oracle scope, see header; reads the native result of record
        const int sv[4] = {11, 11, 10, 0};
        const int sect_dims = sv[0] + sv[1] + sv[2] + sv[3];
        const float ts = std::pow(10000000.0f, -2.0f / (float) n_dims);
        auto posv = [&](int64_t k) { int32_t v; std::memcpy(&v, hp.data() + (size_t) k * 4, 4); return (float) v; };
        for (int64_t i2 = 0; i2 < ns; i2++) for (int64_t i1 = 0; i1 < nh; i1++) {
            const int64_t xrow = (i1 * xs02 + i2) * xs01, drow = (i1 * ne0 + i2) * ne0;
            for (int64_t w = 0; w < n_dims / 2; w++) {
                const int64_t e0 = neox || multi ? n_offs + w : n_offs + 2 * w;
                const int64_t e1 = neox || multi ? e0 + n_dims / 2 : e0 + 1;
                float x0, x1;
                std::memcpy(&x0, hx.data() + (size_t) (xrow + e0) * 4, 4);
                std::memcpy(&x1, hx.data() + (size_t) (xrow + e1) * 4, 4);
                float pv;
                if (multi) {
                    const int sector = (int) (w % sect_dims);
                    const int sec_w = sv[0] + sv[1];
                    if (mode == GGML_ROPE_TYPE_IMROPE) { // native rope_multi imrope priority: h, w, t, extra
                        if (sector % 3 == 1 && sector < 3 * sv[1]) pv = posv(i2 + ns);
                        else if (sector % 3 == 2 && sector < 3 * sv[2]) pv = posv(i2 + 2 * ns);
                        else if (sector % 3 == 0 && sector < 3 * sv[0]) pv = posv(i2);
                        else pv = posv(i2 + 3 * ns);
                    } else {
                        if (sector < sv[0]) pv = posv(i2);
                        else if (sector < sec_w) pv = posv(i2 + ns);
                        else if (sector < sec_w + sv[2]) pv = posv(i2 + 2 * ns);
                        else pv = posv(i2 + 3 * ns);
                    }
                } else pv = posv(i2);
                float ffac = 1.0f;
                if (ff_on) std::memcpy(&ffac, hf.data() + (size_t) w * 4, 4);
                const float theta = pv * std::pow(ts, (float) w) / ffac; // ext_factor=0: yarn mix is identity
                const double cd = std::cos((double) theta), sd = std::sin((double) theta);
                const double g0 = (double) x0 * cd - (double) x1 * sd, g1 = (double) x0 * sd + (double) x1 * cd;
                float r0, r1;
                std::memcpy(&r0, bn.data() + Buf::guard + (size_t) (drow + e0) * 4, 4);
                std::memcpy(&r1, bn.data() + Buf::guard + (size_t) (drow + e1) * 4, 4);
                if (pset == 0) req(r0 == x0 && r1 == x1, "independent identity oracle (pos=0 exact)");
                else req(near_tol(r0, g0) && near_tol(r1, g1), "independent sincos oracle (fixed 16 ULP, declared a priori)");
                ++oracle_pairs;
            }
        }
    }
    ++cases; f16_cases += f16; back_cases += back; ff_cases += ff_on;
}
int main() {
    ggml_backend_cuda_context ctx(0);
    ck(cudaSetDevice(0));
    const bool rollback = std::getenv("GGML_GOOSE_CUDA_ROPE") && std::strcmp(std::getenv("GGML_GOOSE_CUDA_ROPE"), "0") == 0;
    const int MS[2] = {GGML_ROPE_TYPE_IMROPE, GGML_ROPE_TYPE_MROPE};
    const int64_t HC[3][2] = {{16, 1}, {16, 3}, {2, 5}}; // heads, seq
    for (int mi = 0; mi < 2; mi++) for (int ci = 0; ci < 3; ci++) for (int p = 0; p < 4; p++) // A: Apex core
        run(ctx, MS[mi], 256, 64, 0, HC[ci][0], HC[ci][1], 0, 0, 0, p, 0);
    for (int mi = 0; mi < 2; mi++) for (int f16 = 0; f16 < 2; f16++) for (int back = 0; back < 2; back++) for (int p = 2; p < 4; p++) // B
        run(ctx, MS[mi], 256, 64, 0, 16, 2, f16, back, 0, p, 0);
    run(ctx, MS[0], 256, 64, 0, 16, 3, 0, 0, 1, 1, 0); // C: freq_factors
    run(ctx, MS[1], 256, 64, 0, 16, 3, 0, 0, 1, 1, 0);
    run(ctx, GGML_ROPE_TYPE_NEOX, 64, 64, 0, 2, 3, 0, 0, 1, 1, 0);
    run(ctx, GGML_ROPE_TYPE_NORMAL, 64, 64, 0, 2, 3, 0, 0, 0, 0, 0); // D: standard/NEOX
    run(ctx, GGML_ROPE_TYPE_NORMAL, 64, 64, 0, 2, 3, 0, 0, 0, 2, 0);
    run(ctx, GGML_ROPE_TYPE_NORMAL, 64, 64, 0, 2, 3, 1, 0, 0, 3, 0);
    run(ctx, GGML_ROPE_TYPE_NORMAL, 64, 64, 0, 2, 3, 0, 1, 0, 2, 0);
    run(ctx, GGML_ROPE_TYPE_NORMAL, 256, 64, 0, 16, 2, 0, 0, 0, 3, 0);
    run(ctx, GGML_ROPE_TYPE_NEOX, 256, 64, 0, 16, 2, 0, 0, 0, 3, 0);
    run(ctx, GGML_ROPE_TYPE_NEOX, 256, 64, 0, 16, 2, 1, 0, 0, 2, 0);
    run(ctx, GGML_ROPE_TYPE_NEOX, 256, 64, 0, 16, 2, 0, 1, 0, 2, 0);
    run(ctx, GGML_ROPE_TYPE_NEOX, 64, 64, 0, 2, 3, 0, 0, 0, 3, 0);
    run(ctx, GGML_ROPE_TYPE_NEOX, 256, 64, 8, 16, 2, 0, 0, 0, 3, 0);
    run(ctx, GGML_ROPE_TYPE_NEOX, 256, 64, 0, 16, 2, 1, 1, 0, 2, 0);
    run(ctx, GGML_ROPE_TYPE_NORMAL, 64, 64, 0, 2, 3, 1, 1, 0, 2, 0);
    run(ctx, MS[0], 256, 64, 0, 16, 2, 0, 0, 0, 3, 1); // E: strided x rows
    run(ctx, MS[0], 256, 64, 0, 16, 2, 0, 1, 0, 2, 1);
    run(ctx, MS[0], 64, 64, 0, 2, 3, 0, 0, 0, 3, 0); // F: n_dims == ne00, empty tail
    uint64_t dev[2] = {}; const int devrc = fg172r_cuda_device_read(dev);
    req(devrc == 0, "CUDA witness read success");
    const uint64_t host = fg172r_cuda_host_seen();
    if (rollback) {
        req(host == 0, "rollback leaves host counter 0");
        req(dev[0] == 0 && dev[1] == 0, "rollback leaves device words 0");
    } else {
        req((host & 1) != 0, "host compute bit witnessed"); // device words recorded only; interpretation is parent-adaptable
    }
    printf("PASS cases=%zu f16=%zu back=%zu ff=%zu oracle_pairs=%zu checks=%zu rollback=%d host=%llu device=[%llu,%llu] rc=%d\n",
           cases, f16_cases, back_cases, ff_cases, oracle_pairs, checks, (int) rollback,
           (unsigned long long) host, (unsigned long long) dev[0], (unsigned long long) dev[1], devrc);
    return 0;
}
