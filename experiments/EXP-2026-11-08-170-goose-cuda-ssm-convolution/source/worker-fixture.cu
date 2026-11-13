// EXP170s bounded CUDA correctness fixture for the actual public SSM convolution API.
// Oracle: fg170s_native_ggml_cuda_op_ssm_conv, the unchanged native ggml_cuda_op_ssm_conv under the
// fg170s_ prefix (declared here, defined by the parent build; this fixture never includes or
// re-implements native internals). Candidate: the real public ggml_cuda_op_ssm_conv(ctx,dst,bias_add,silu_dst).
// Both sides always receive identical actual context/tensor inputs (same shapes, strides, values);
// oracle and candidate each own a fully separate buffer set with distinct cudaMalloc addresses, so no
// raw input/output address is ever shared and bitwise equality cannot come from aliasing.
// Comparison policy: native-vs-candidate logical outputs are compared bitwise with zero tolerance and
// inputs are finite by construction. An independent double-precision CPU sliding-window convolution
// reference (written directly against the logical strided tensors, not a port of either device path)
// is checked with the documented finite tolerance kRefAbs + kRefRel*|ref|; that tolerance exists ONLY
// for the independent fp64 reference: at most 15 F32 multiply-accumulate terms with |term| <= 0.25
// plus one expf keep device rounding error orders of magnitude below it. No tolerance is applied
// anywhere on the native-vs-candidate path.
// Tail policy: the native long-token kernel reads (32-local_n_t) unused floats past each channel row
// of a ragged final z-block, so every native run gets 64 slack floats appended past the logical conv_x
// tensor (native is never invoked on an exact-size tail). The candidate bridge defaults to the bounded
// tail; grid D hands the candidate an exact-size conv_x allocation (ragged 33/65-token tails) to prove
// that no access passes ggml_nbytes, with native skipped for those cases. Setup synchronizes the
// default stream (cudaDeviceSynchronize) after all uploads/canary memsets, before the non-blocking
// ctx.stream() computes (EXP168 finding). No timings, no microbenchmarks, no algorithm changes.
#include "ssm-conv.cuh"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

void fg170s_native_ggml_cuda_op_ssm_conv(ggml_backend_cuda_context &, ggml_tensor *, ggml_tensor *, ggml_tensor *);
extern "C" uint64_t fg170s_cuda_host_seen();
extern "C" int fg170s_cuda_device_read(uint64_t out[2]);

static size_t checks = 0, cases = 0, paired = 0, exact_tail = 0, ref_points = 0;
static constexpr double kRefAbs = 1e-5, kRefRel = 1e-6; // documented tolerance, independent fp64 reference only
static void ck(cudaError_t e) { if (e != cudaSuccess) { fprintf(stderr, "CUDA %s\n", cudaGetErrorString(e)); std::exit(2); } }
static void req(bool b, const char * s) { ++checks; if (!b) { fprintf(stderr, "FAIL %s\n", s); std::exit(3); } }

static float xval(size_t i) { const unsigned h = (unsigned) (i * 2654435761u + 1u); return ((int) (h % 2001u) - 1000) / 1000.0f; } // finite [-1,1]
static float wval(size_t i) { const unsigned h = (unsigned) (i * 40503u + 7u);    return ((int) (h % 501u) - 250) / 1000.0f; }     // finite [-0.25,0.25]
static float bval(size_t i) { const unsigned h = (unsigned) (i * 97u + 13u);      return ((int) (h % 501u) - 250) / 1000.0f; }     // finite [-0.25,0.25]

struct Buf {
    static constexpr size_t guard = 64; // 64-byte 0xA5 canary on every buffer head and tail
    unsigned char * raw = nullptr; size_t n = 0;
    void alloc(size_t bytes) { n = bytes + 2 * guard; ck(cudaMalloc(&raw, n)); ck(cudaMemset(raw, 0xA5, n)); }
    ~Buf() { if (raw) ck(cudaFree(raw)); }
    void put(const std::vector<unsigned char> & v) const { ck(cudaMemcpy(raw + guard, v.data(), v.size(), cudaMemcpyHostToDevice)); }
    std::vector<unsigned char> get() const { std::vector<unsigned char> v(n); ck(cudaMemcpy(v.data(), raw, n, cudaMemcpyDeviceToHost)); return v; }
};

static void shape(ggml_tensor & t, int64_t ne0, int64_t ne1, int64_t ne2, size_t nb1 = 0, size_t nb2 = 0) {
    std::memset(&t, 0, sizeof t);
    t.type = GGML_TYPE_F32;
    t.ne[0] = ne0; t.ne[1] = ne1; t.ne[2] = ne2; t.ne[3] = 1;
    t.nb[0] = 4; t.nb[1] = nb1 ? nb1 : (size_t) ne0 * 4; t.nb[2] = nb2 ? nb2 : t.nb[1] * (size_t) ne1; t.nb[3] = t.nb[2] * (size_t) ne2;
}

static void run_case(ggml_backend_cuda_context & ctx, int dc, int nt, int nr, int ns, int fuse, unsigned pad, bool exact) {
    // fuse: 0 plain, 1 fused SiLU, 2 fused bias+SiLU (add.src0=dst), 3 fused bias+SiLU with swapped ADD operands.
    const bool fuse_silu = fuse >= 1, fuse_bias = fuse >= 2, swapped = fuse == 3;
    const int win = nt + dc - 1; // conv_x window tokens per channel row (nb[1] stays exact; native asserts it)
    const size_t nb1x = (size_t) win * 4, nb2x = nb1x * (size_t) nr + (pad & 4 ? 64 : 0);
    const size_t nb1w = (size_t) dc * 4 + (pad & 8 ? 32 : 0);
    const size_t nb1o = (size_t) nr * 4 + (pad & 1 ? 64 : 0), nb2o = nb1o * (size_t) nt + (pad & 2 ? 64 : 0);
    const size_t spanx = (size_t) (ns - 1) * nb2x + (size_t) (nr - 1) * nb1x + nb1x;
    const size_t spanw = (size_t) (nr - 1) * nb1w + (size_t) dc * 4;
    const size_t spano = (size_t) (ns - 1) * nb2o + (size_t) (nt - 1) * nb1o + (size_t) nr * 4;
    const size_t spanb = (size_t) nr * 4;
    const size_t slack = exact ? 0 : 256; // 64 read-slack floats past conv_x; native ragged-tail overread is <= 31 floats
    if (exact) req(nt > 32 && nt % 32 != 0, "exact-size tails target ragged long tokens");
    std::vector<unsigned char> hx(spanx + slack, 0xC5), hw(spanw, 0xA5), ho(spano, 0xA5), hb(fuse_bias ? spanb : 0, 0);
    for (int s = 0; s < ns; s++) for (int c = 0; c < nr; c++) for (int t = 0; t < win; t++) {
        const float v = xval(((size_t) s * nr + c) * (size_t) win + (size_t) t);
        std::memcpy(hx.data() + (size_t) s * nb2x + (size_t) c * nb1x + (size_t) t * 4, &v, 4);
    }
    for (int c = 0; c < nr; c++) for (int j = 0; j < dc; j++) {
        const float v = wval((size_t) c * (size_t) dc + (size_t) j);
        std::memcpy(hw.data() + (size_t) c * nb1w + (size_t) j * 4, &v, 4);
    }
    if (fuse_bias) for (int c = 0; c < nr; c++) { const float v = bval((size_t) c); std::memcpy(hb.data() + (size_t) c * 4, &v, 4); }
    std::vector<double> ref((size_t) nt * nr * ns); // independent double-precision CPU convolution reference
    for (int s = 0; s < ns; s++) for (int t = 0; t < nt; t++) for (int c = 0; c < nr; c++) {
        double acc = 0.0;
        for (int j = 0; j < dc; j++) {
            float xf, wf;
            std::memcpy(&xf, hx.data() + (size_t) s * nb2x + (size_t) c * nb1x + (size_t) (t + j) * 4, 4);
            std::memcpy(&wf, hw.data() + (size_t) c * nb1w + (size_t) j * 4, 4);
            acc += (double) xf * (double) wf;
        }
        if (fuse_bias) { float bf; std::memcpy(&bf, hb.data() + (size_t) c * 4, 4); acc += (double) bf; }
        ref[((size_t) s * nt + t) * nr + c] = fuse_silu ? acc / (1.0 + std::exp(-acc)) : acc;
    }
    std::vector<char> cov(spano, 0); // logical out bytes; every other out byte must remain the 0xA5 canary
    for (int s = 0; s < ns; s++) for (int t = 0; t < nt; t++) for (int c = 0; c < nr; c++) {
        const size_t off = (size_t) s * nb2o + (size_t) t * nb1o + (size_t) c * 4;
        cov[off] = cov[off + 1] = cov[off + 2] = cov[off + 3] = 1;
    }
    struct Side { ggml_tensor x, w, d, sil, bias, add; Buf bx, bw, bb, bo, bs; } sd[2]; // side 0 oracle, side 1 candidate
    auto wire = [&](Side & S) {
        shape(S.x, win, nr, ns, nb1x, nb2x);
        shape(S.w, dc, nr, 1, nb1w);
        shape(S.d, nr, nt, ns, nb1o, nb2o);
        S.d.op = GGML_OP_SSM_CONV; S.d.src[0] = &S.x; S.d.src[1] = &S.w;
        shape(S.sil, nr, nt, ns, nb1o, nb2o);
        if (fuse_bias) {
            shape(S.bias, nr, 1, 1);
            S.add.op = GGML_OP_ADD;
            if (swapped) { S.add.src[0] = &S.bias; S.add.src[1] = &S.d; } else { S.add.src[0] = &S.d; S.add.src[1] = &S.bias; }
        }
    };
    auto build = [&](int k) {
        Side & S = sd[k];
        wire(S);
        S.bx.alloc(spanx + slack); S.bw.alloc(spanw); S.bo.alloc(spano);
        if (fuse_silu) S.bs.alloc(spano);
        if (fuse_bias) S.bb.alloc(spanb);
        S.x.data = S.bx.raw + Buf::guard; S.w.data = S.bw.raw + Buf::guard; S.d.data = S.bo.raw + Buf::guard;
        if (fuse_silu) S.sil.data = S.bs.raw + Buf::guard;
        if (fuse_bias) S.bias.data = S.bb.raw + Buf::guard;
    };
    auto upload = [&](int k) {
        Side & S = sd[k];
        S.bx.put(hx); S.bw.put(hw); S.bo.put(ho);
        if (fuse_silu) S.bs.put(ho);
        if (fuse_bias) S.bb.put(hb);
        ck(cudaDeviceSynchronize()); // uploads/canary memsets finish before the non-blocking ctx.stream()
    };
    auto invoke = [&](int k, bool native) {
        Side & S = sd[k];
        if (fuse_silu) {
            if (native) fg170s_native_ggml_cuda_op_ssm_conv(ctx, &S.d, fuse_bias ? &S.add : nullptr, &S.sil);
            else        ggml_cuda_op_ssm_conv(ctx, &S.d, fuse_bias ? &S.add : nullptr, &S.sil);
        } else {
            if (native) fg170s_native_ggml_cuda_op_ssm_conv(ctx, &S.d, nullptr, nullptr);
            else        ggml_cuda_op_ssm_conv(ctx, &S.d, nullptr, nullptr);
        }
        ck(cudaStreamSynchronize(ctx.stream()));
    };
    auto fetch = [&](int k) {
        std::vector<std::vector<unsigned char>> v(5);
        v[0] = sd[k].bx.get(); v[1] = sd[k].bw.get(); v[2] = sd[k].bo.get();
        if (fuse_bias) v[3] = sd[k].bb.get();
        if (fuse_silu) v[4] = sd[k].bs.get();
        return v;
    };
    const bool run_native = !exact; // native is never invoked on an exact-size tail
    std::vector<std::vector<unsigned char>> na, ca;
    if (run_native) { build(0); upload(0); invoke(0, true); na = fetch(0); ++paired; }
    build(1); upload(1); invoke(1, false); ca = fetch(1);
    ++cases; if (exact) ++exact_tail;
    auto guards = [&](const std::vector<unsigned char> & v, const char * what) {
        for (size_t i = 0; i < Buf::guard; i++) req(v[i] == 0xA5 && v[v.size() - 1 - i] == 0xA5, what);
    };
    auto unchanged = [&](const std::vector<unsigned char> & v, const std::vector<unsigned char> & h, const char * what) {
        req(std::memcmp(v.data() + Buf::guard, h.data(), h.size()) == 0, what);
    };
    auto out_at = [&](const std::vector<unsigned char> & v, int s, int t, int c) {
        return v.data() + Buf::guard + (size_t) s * nb2o + (size_t) t * nb1o + (size_t) c * 4;
    };
    auto check_out = [&](const std::vector<unsigned char> & v) {
        for (size_t i = 0; i < spano; i++) if (!cov[i]) req(v[Buf::guard + i] == 0xA5, "output padding canary untouched");
        for (int s = 0; s < ns; s++) for (int t = 0; t < nt; t++) for (int c = 0; c < nr; c++) {
            const double r = ref[((size_t) s * nt + t) * nr + c];
            float f; std::memcpy(&f, out_at(v, s, t, c), 4);
            req(std::isfinite(f) && std::fabs((double) f - r) <= kRefAbs + kRefRel * std::fabs(r),
                "independent fp64 convolution reference (documented finite tolerance)");
            ++ref_points;
        }
    };
    for (int k = 0; k < 2; k++) {
        if (k == 0 && !run_native) continue;
        const std::vector<std::vector<unsigned char>> & v = k == 0 ? na : ca;
        guards(v[0], "conv_x head/tail canaries");
        guards(v[1], "weight head/tail canaries");
        guards(v[2], "conv-node out head/tail canaries");
        if (fuse_bias) guards(v[3], "bias head/tail canaries");
        if (fuse_silu) guards(v[4], "silu out head/tail canaries");
        unchanged(v[0], hx, "conv_x bytes unchanged (native slack included; kernels never write inputs)");
        unchanged(v[1], hw, "weight bytes unchanged");
        if (fuse_bias) unchanged(v[3], hb, "bias bytes unchanged");
        if (fuse_silu) for (size_t i = 0; i < spano; i++) req(v[2][Buf::guard + i] == 0xA5, "conv-node output untouched when silu_dst is the real out");
        check_out(fuse_silu ? v[4] : v[2]);
    }
    if (run_native) { // bitwise candidate-vs-native over the logical output; zero tolerance, finite inputs
        const std::vector<unsigned char> & an = fuse_silu ? na[4] : na[2];
        const std::vector<unsigned char> & cn = fuse_silu ? ca[4] : ca[2];
        for (int s = 0; s < ns; s++) for (int t = 0; t < nt; t++) for (int c = 0; c < nr; c++)
            req(std::memcmp(out_at(an, s, t, c), out_at(cn, s, t, c), 4) == 0, "candidate bitwise-identical to native oracle");
    }
}

int main() {
    ggml_backend_cuda_context ctx(0);
    ck(cudaSetDevice(0));
    const bool rollback = std::getenv("GGML_GOOSE_CUDA_SSM") && std::strcmp(std::getenv("GGML_GOOSE_CUDA_SSM"), "0") == 0;
    const int DC[6] = {3, 4, 5, 7, 9, 15};               // every native d_conv
    const int NT[9] = {1, 3, 4, 31, 32, 33, 63, 64, 65}; // serial/long token-tiled boundaries
    const int TB[3] = {4, 33, 65};
    for (int di = 0; di < 6; di++) for (int ti = 0; ti < 9; ti++) // A: full d_conv x token-boundary sweep, fuse rotating
        run_case(ctx, DC[di], NT[ti], 128, 1, (di * 9 + ti) % 4, 0, false);
    for (int nri = 0; nri < 2; nri++) for (int si = 0; si < 2; si++) for (int di = 0; di < 6; di++) for (int ti = 0; ti < 3; ti++)
        run_case(ctx, DC[di], TB[ti], nri ? 256 : 128, si ? 2 : 1, (nri * 2 + si + di + ti) % 4, 0, false); // B: channels x sequences
    const unsigned PADS[5] = {1, 2, 4, 8, 15}; // out token/sequence, conv_x sequence, weight row strides, and all combined
    for (int di = 0; di < 6; di += 5) for (int ti = 2; ti < 9; ti += 3) for (int pi = 0; pi < 5; pi++)
        run_case(ctx, DC[di], NT[ti], 256, 2, 3, PADS[pi], false); // C: swapped ADD operands over padded valid strides
    if (!rollback) // D: candidate bounded tail on exact-size conv_x; skipped under rollback, where the public op is the
        for (int di = 0; di < 6; di += 5) for (int ti = 5; ti < 9; ti += 3) // native algorithm that must never see an exact-size tail
            run_case(ctx, DC[di], NT[ti], 128, 1, 1, 0, true);
    uint64_t dev[2] = {}; const int devrc = fg170s_cuda_device_read(dev);
    req(devrc == 0, "fg170s_cuda_device_read returns cudaSuccess");
    const uint64_t host = fg170s_cuda_host_seen();
    if (rollback) {
        req(host == 0, "rollback leaves host witness 0");
        req(dev[0] == 0 && dev[1] == 0, "rollback leaves device witness words 0");
    } else {
        req(host != 0, "default host witness nonzero");
        req(dev[0] != 0 && dev[1] != 0, "default device witness words nonzero");
    }
    printf("PASS cases=%zu native_paired=%zu exact_tail=%zu ref_points=%zu checks=%zu rollback=%d host=%llu device=[%llu,%llu] rc=%d\n",
           cases, paired, exact_tail, ref_points, checks, (int) rollback, (unsigned long long) host,
           (unsigned long long) dev[0], (unsigned long long) dev[1], devrc);
    return 0;
}
