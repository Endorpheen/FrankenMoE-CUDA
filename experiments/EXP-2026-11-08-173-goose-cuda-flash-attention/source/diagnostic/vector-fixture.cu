#include "vector-test-shim.h"
#include "ggml-cuda/common.cuh"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>




static size_t checks = 0, cases = 0, captures = 0;
static int cur_case = -1;
static char cur_desc[112] = "setup";

static void ck(cudaError_t e) { if (e != cudaSuccess) { fprintf(stderr, "CUDA %s case=%d %s\n", cudaGetErrorString(e), cur_case, cur_desc); std::exit(2); } }
static void req(bool ok, const char * what) { ++checks; if (!ok) { fprintf(stderr, "FAIL %s case=%d %s\n", what, cur_case, cur_desc); std::exit(3); } }
static void hexfail(const char * what, int64_t i0, int64_t i1, int64_t i2, int64_t i3, uint32_t expected, uint32_t actual) {
    fprintf(stderr, "FAIL %s case=%d %s elem=[%lld,%lld,%lld,%lld] expected=0x%08x actual=0x%08x\n", what, cur_case, cur_desc,
            (long long) i0, (long long) i1, (long long) i2, (long long) i3, expected, actual);
    std::exit(4);
}

struct Buffer {
    unsigned char * raw = nullptr;
    size_t n = 0;
    std::vector<unsigned char> shadow;
    static constexpr size_t guard = 64;
    explicit Buffer(std::vector<unsigned char> host) : shadow(std::move(host)) {
        n = shadow.size();
        ck(cudaMalloc(&raw, n));
        ck(cudaMemcpy(raw, shadow.data(), n, cudaMemcpyHostToDevice));
    }
    ~Buffer() { if (raw) { ck(cudaFree(raw)); } }
    void * data() const { return raw + guard; }
    float * fdata() const { return (float *) data(); }
    float2 * f2data() const { return (float2 *) data(); }
    int * idata() const { return (int *) data(); }
    std::vector<unsigned char> fetch() const { std::vector<unsigned char> now(n); ck(cudaMemcpy(now.data(), raw, n, cudaMemcpyDeviceToHost)); return now; }
    bool intact() const { return fetch() == shadow; }
};

static std::vector<unsigned char> stage(size_t bytes) { return std::vector<unsigned char>(bytes + 2*Buffer::guard, 0xA5); }
static unsigned char * at(std::vector<unsigned char> & v, size_t off) { return v.data() + Buffer::guard + off; }
static bool guards_ok(const std::vector<unsigned char> & v) {
    for (size_t i = 0; i < Buffer::guard; i++) if (v[i] != 0xA5 || v[v.size() - 1 - i] != 0xA5) return false;
    return true;
}
static void put_u16(std::vector<unsigned char> & v, size_t off, uint16_t bits) { std::memcpy(at(v, off), &bits, 2); }
static void put_f32(std::vector<unsigned char> & v, size_t off, float f) { std::memcpy(at(v, off), &f, 4); }

static void fill_q(std::vector<unsigned char> & v, const int64_t ne[4], const size_t nb[4], bool zero) {
    size_t lin = 0;
    for (int64_t i3 = 0; i3 < ne[3]; i3++) for (int64_t i2 = 0; i2 < ne[2]; i2++) for (int64_t i1 = 0; i1 < ne[1]; i1++) for (int64_t i0 = 0; i0 < ne[0]; i0++) {
        put_f32(v, i0*nb[0] + i1*nb[1] + i2*nb[2] + i3*nb[3], zero ? 0.0f : float(int((lin*31337ull) % 50003ull) - 25001) / 4096.0f);
        lin++;
    }
}
static const uint16_t kv_dbits[8] = {0x2800, 0x3000, 0x0000, 0xa800, 0xb000, 0x2c00, 0xac00, 0x3400};
static void fill_kv(std::vector<unsigned char> & v, const int64_t ne[4], const size_t nb[4], uint64_t seed) {
    for (int64_t i3 = 0; i3 < ne[3]; i3++) for (int64_t i2 = 0; i2 < ne[2]; i2++) for (int64_t i1 = 0; i1 < ne[1]; i1++) {
        const size_t base = i1*nb[1] + i2*nb[2] + i3*nb[3];
        for (int b = 0; b < 8; b++) {
            unsigned char blk[18];
            const uint16_t d = kv_dbits[(seed + uint64_t(i1) + 3*b) % 8];
            std::memcpy(blk, &d, 2);
            for (int j = 0; j < 16; j++) blk[2+j] = (unsigned char)(((j + b + seed) & 0xF) | (((5*j + 7*b + seed) & 0xF) << 4));
            std::memcpy(at(v, base + size_t(b)*18), blk, 18);
        }
    }
}
static void fill_mask(std::vector<unsigned char> & v, int64_t kvlen, int64_t nq, int64_t ns, const size_t nb[4]) {
    for (int64_t i3 = 0; i3 < ns; i3++) for (int64_t i1 = 0; i1 < nq; i1++) for (int64_t i0 = 0; i0 < kvlen; i0++) put_u16(v, i3*nb[3] + i0*nb[0] + i1*nb[1], i0 <= i1 ? 0x0000 : 0xfc00);
}
static void fill_sinks(std::vector<unsigned char> & v) { for (int i = 0; i < 16; i++) put_f32(v, size_t(i)*4, float(int(i % 9) - 4) / 8.0f); }
static void tset(ggml_tensor & t, ggml_type ty, const int64_t ne[4], const size_t nb[4], void * data) {
    std::memset(&t, 0, sizeof t);
    t.type = ty;
    for (int i = 0; i < 4; i++) { t.ne[i] = ne[i]; t.nb[i] = nb[i]; }
    t.data = data;
}
static void wire_dst(ggml_tensor & td, ggml_tensor & tq, ggml_tensor & tk, ggml_tensor & tv, ggml_tensor * tm, ggml_tensor * ts, const int64_t ne[4], const size_t nb[4], void * data) {
    tset(td, GGML_TYPE_F32, ne, nb, data);
    td.op = GGML_OP_FLASH_ATTN_EXT;
    const float op[3] = {1.0f/16.0f, 0.0f, 0.0f};
    std::memcpy(td.op_params, op, sizeof op);
    td.src[0] = &tq; td.src[1] = &tk; td.src[2] = &tv; td.src[3] = tm; td.src[4] = ts;
}

struct Case { int n, nq, kvlen, pb, nseq, qpad, kvpad, mask, sinks, kvmax, zero_q, capture; };
static const Case case_table[12] = {
    {1,1,256,1,1, 0, 0,0,0,  0,0,1}, {1,2,256,1,1, 0, 0,1,0,  0,0,0},
    {1,3,512,2,1, 4, 0,1,1,  0,0,0}, {1,2,512,4,2, 0,18,1,1,256,0,0},
    {2,2,256,1,1, 0, 0,1,0,  0,0,0}, {2,3,256,1,1, 0, 0,1,0,  0,0,0},
    {2,1,512,2,1, 4, 0,1,1,  0,0,0}, {2,2,512,2,2, 0,18,1,1,512,0,0},
    {2,3,512,4,2, 4,18,1,1,256,0,0}, {1,1,256,1,1, 0, 0,0,1,  0,1,0},
    {1,3,256,1,2, 0,18,0,1,  0,0,0}, {2,2,256,4,1, 4, 0,1,1,256,0,0},
};

static void compare_dst(const std::vector<unsigned char> & a, const std::vector<unsigned char> & b, const int64_t ne[4], const size_t nb[4]) {
    req(guards_ok(a) && guards_ok(b), "dst guards");
    for (int64_t i3 = 0; i3 < ne[3]; i3++) for (int64_t i2 = 0; i2 < ne[2]; i2++) for (int64_t i1 = 0; i1 < ne[1]; i1++) for (int64_t i0 = 0; i0 < ne[0]; i0++) {
        const size_t off = Buffer::guard + i0*nb[0] + i1*nb[1] + i2*nb[2] + i3*nb[3];
        uint32_t ba, bb;
        std::memcpy(&ba, a.data() + off, 4);
        std::memcpy(&bb, b.data() + off, 4);
        if (ba != bb) hexfail("dst bitwise", i0, i1, i2, i3, ba, bb);
        if ((ba & 0x7f800000u) == 0x7f800000u) hexfail("dst non-finite", i0, i1, i2, i3, ba, bb);
        if (ba == 0xa5a5a5a5u) hexfail("dst unwritten", i0, i1, i2, i3, ba, bb);
    }
}

static void run_case(ggml_backend_cuda_context & ctx, const Case & c) {
    const int N = c.n, nq = c.nq, kv = c.kvlen, pb = c.pb, ns = c.nseq;
    const size_t qblocks = size_t((nq + N - 1) / N);
    const int64_t qne[4] = {256, nq, 16, ns}, kne[4] = {256, kv, 2, ns}, mne[4] = {kv, nq, 1, ns}, dne[4] = {256, 16, nq, ns}, sne[4] = {16, 1, 1, 1};
    size_t qnb[4] = {4, size_t(1024 + c.qpad), 0, 0};
    qnb[2] = qnb[1]*size_t(nq) + c.qpad; qnb[3] = qnb[2]*16;
    size_t knb[4] = {18, size_t(144 + c.kvpad), 0, 0};
    knb[2] = knb[1]*size_t(kv) + c.kvpad; knb[3] = knb[2]*2 + c.kvpad;
    const size_t mnb[4] = {2, size_t(kv)*2, size_t(kv)*2*nq, size_t(kv)*2*nq};
    const size_t dnb[4] = {4, 1024, size_t(1024)*16, size_t(1024)*16*size_t(nq)};
    const size_t snb[4] = {4, 64, 64, 64};
    const size_t qbytes = (ns-1)*qnb[3] + 15*qnb[2] + (nq-1)*qnb[1] + 1024;
    const size_t kbytes = (ns-1)*knb[3] + knb[2] + (kv-1)*knb[1] + 144;
    const size_t mbytes = (ns-1)*mnb[3] + (nq-1)*mnb[1] + size_t(kv)*2;
    const size_t dbytes = (ns-1)*dnb[3] + (nq-1)*dnb[2] + 15*dnb[1] + 1024;
    const size_t parts_n = size_t(nq)*16*ns*pb*256, meta_n = size_t(nq)*16*ns*pb, kmax_n = qblocks*ns;

    std::vector<unsigned char> qh = stage(qbytes), kh = stage(kbytes), vh = stage(kbytes), dh = stage(dbytes);
    fill_q(qh, qne, qnb, c.zero_q);
    fill_kv(kh, kne, knb, 1);
    fill_kv(vh, kne, knb, 3);
    std::vector<unsigned char> mh, sh, kmh;
    if (c.mask) { mh = stage(mbytes); fill_mask(mh, kv, nq, ns, mnb); }
    if (c.sinks) { sh = stage(64); fill_sinks(sh); }
    if (c.kvmax) { kmh = stage(kmax_n*4); for (size_t i = 0; i < kmax_n; i++) std::memcpy(at(kmh, i*4), &c.kvmax, 4); }

    Buffer qa(qh), qb(qh), ka(kh), kb(kh), va(vh), vb(vh), da(dh), db(dh);
    Buffer parts_a(stage(parts_n*4)), parts_b(stage(parts_n*4)), meta_a(stage(meta_n*8)), meta_b(stage(meta_n*8));
    std::unique_ptr<Buffer> mask_a(c.mask ? new Buffer(mh) : nullptr), mask_b(c.mask ? new Buffer(mh) : nullptr);
    std::unique_ptr<Buffer> sinks_a(c.sinks ? new Buffer(sh) : nullptr), sinks_b(c.sinks ? new Buffer(sh) : nullptr);
    std::unique_ptr<Buffer> kvmax_a(c.kvmax ? new Buffer(kmh) : nullptr), kvmax_b(c.kvmax ? new Buffer(kmh) : nullptr);

    ggml_tensor tq{}, tk{}, tv{}, tm{}, ts{}, td{};
    tset(tq, GGML_TYPE_F32, qne, qnb, qa.data());
    tset(tk, GGML_TYPE_Q4_0, kne, knb, ka.data());
    tset(tv, GGML_TYPE_Q4_0, kne, knb, va.data());
    if (c.mask) tset(tm, GGML_TYPE_F16, mne, mnb, mask_a->data());
    if (c.sinks) tset(ts, GGML_TYPE_F32, sne, snb, sinks_a->data());
    wire_dst(td, tq, tk, tv, c.mask ? &tm : nullptr, c.sinks ? &ts : nullptr, dne, dnb, da.data());
    ggml_tensor tqb{}, tkb{}, tvb{}, tmb{}, tsb{}, tdb{};
    tset(tqb, GGML_TYPE_F32, qne, qnb, qb.data());
    tset(tkb, GGML_TYPE_Q4_0, kne, knb, kb.data());
    tset(tvb, GGML_TYPE_Q4_0, kne, knb, vb.data());
    if (c.mask) tset(tmb, GGML_TYPE_F16, mne, mnb, mask_b->data());
    if (c.sinks) tset(tsb, GGML_TYPE_F32, sne, snb, sinks_b->data());
    wire_dst(tdb, tqb, tkb, tvb, c.mask ? &tmb : nullptr, c.sinks ? &tsb : nullptr, dne, dnb, db.data());

    ck(cudaDeviceSynchronize());
    snprintf(cur_desc, sizeof cur_desc, "N=%d nq=%d kv=%d pb=%d ns=%d qpad=%d kvpad=%d mask=%d sinks=%d kvmax=%d zeroq=%d", N, nq, kv, pb, ns, c.qpad, c.kvpad, c.mask, c.sinks, c.kvmax, c.zero_q);
    fg175v_test_run(ctx, &td, N, pb, true, parts_a.fdata(), meta_a.f2data(), kvmax_a ? kvmax_a->idata() : nullptr);
    ck(cudaGetLastError());
    ck(cudaStreamSynchronize(ctx.stream()));
    fg175v_test_run(ctx, &tdb, N, pb, false, parts_b.fdata(), meta_b.f2data(), kvmax_b ? kvmax_b->idata() : nullptr);
    ck(cudaGetLastError());
    ck(cudaStreamSynchronize(ctx.stream()));
    const std::vector<unsigned char> ref = da.fetch();
    compare_dst(ref, db.fetch(), dne, dnb);
    req(qa.intact() && qb.intact(), "q unchanged"); req(ka.intact() && kb.intact(), "k unchanged"); req(va.intact() && vb.intact(), "v unchanged");
    if (c.mask) req(mask_a->intact() && mask_b->intact(), "mask unchanged");
    if (c.sinks) req(sinks_a->intact() && sinks_b->intact(), "sinks unchanged");
    if (c.kvmax) req(kvmax_a->intact() && kvmax_b->intact(), "kvmax unchanged");
    req(guards_ok(parts_a.fetch()) && guards_ok(parts_b.fetch()) && guards_ok(meta_a.fetch()) && guards_ok(meta_b.fetch()), "scratch guards");
    if (c.capture) {
        ck(cudaMemsetAsync(db.raw, 0xA5, db.n, ctx.stream()));
        ck(cudaStreamSynchronize(ctx.stream()));
        cudaGraph_t graph = nullptr;
        ck(cudaStreamBeginCapture(ctx.stream(), cudaStreamCaptureModeThreadLocal));
        fg175v_test_run(ctx, &tdb, N, pb, false, parts_b.fdata(), meta_b.f2data(), kvmax_b ? kvmax_b->idata() : nullptr);
        ck(cudaStreamEndCapture(ctx.stream(), &graph));
        size_t nodes = 0;
        ck(cudaGraphGetNodes(graph, nullptr, &nodes));
        req(nodes >= 1, "capture recorded kernels");
        cudaGraphExec_t exec = nullptr;
        ck(cudaGraphInstantiateWithFlags(&exec, graph, 0));
        ck(cudaGraphLaunch(exec, ctx.stream()));
        ck(cudaStreamSynchronize(ctx.stream()));
        compare_dst(ref, db.fetch(), dne, dnb);
        req(guards_ok(parts_b.fetch()) && guards_ok(meta_b.fetch()), "scratch guards after replay");
        ck(cudaGraphExecDestroy(exec));
        ck(cudaGraphDestroy(graph));
        captures++;
    }
    cases++;
}

int main() {
    ggml_backend_cuda_context ctx(0);
    ck(cudaSetDevice(0));
    for (int i = 0; i < int(sizeof(case_table)/sizeof(case_table[0])); i++) { cur_case = i; run_case(ctx, case_table[i]); }
    cur_case = -1;
    std::memcpy(cur_desc, "witness", 8);
    uint64_t words[4] = {};
    req(fg175v_test_witness(words) == 0, "device witness readable");
    req((words[0] & 7) == 7, "device witness paths bits 1|2|4");
    req((words[1] & 15) == 15, "device witness feature bits 1|2|4|8");
    ck(cudaDeviceSynchronize());
    printf("PASS cases=%zu checks=%zu captures=%zu witness paths=%llu features=%llu\n", cases, checks, captures, (unsigned long long)(words[0] & 7), (unsigned long long)(words[1] & 15));
    return 0;
}
