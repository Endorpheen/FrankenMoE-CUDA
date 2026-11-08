#include "common.cuh"
#include <array>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

static_assert(sizeof(block_q4_0) == 18, "block_q4_0 layout");
static_assert(QK4_0 == 32, "QK4_0 values");

void ggml_cuda_op_set_rows(ggml_backend_cuda_context &, ggml_tensor *);
extern "C" uint64_t fg176r_cuda_host_seen();
extern "C" int fg176r_cuda_device_read(uint64_t*);

static const size_t GUARD = 128;
static const unsigned char SRC_FILL = 0xA5;
static const unsigned char DST_FILL = 0x5A;
static size_t g_checks = 0;
static size_t g_cases = 0;

static void ck(cudaError_t e) {
    if (e != cudaSuccess) {
        std::fprintf(stderr, "CUDA %s\n", cudaGetErrorString(e));
        std::exit(2);
    }
}

static void req(bool b, const char * m) {
    ++g_checks;
    if (!b) {
        std::fprintf(stderr, "FAIL %s\n", m);
        std::exit(3);
    }
}

struct Buf {
    unsigned char * raw = nullptr;
    size_t n = 0;
    Buf(size_t payload, unsigned char fill) : n(payload + 2 * GUARD) {
        ck(cudaMalloc(reinterpret_cast<void **>(&raw), n));
        ck(cudaMemset(raw, fill, n));
    }
    ~Buf() { if (raw) ck(cudaFree(raw)); }
    Buf(const Buf &) = delete;
    Buf & operator=(const Buf &) = delete;
    void * data() { return raw + GUARD; }
    size_t size() const { return n - 2 * GUARD; }
};

static void upload(Buf & b, const std::vector<unsigned char> & v) {
    req(v.size() == b.size(), "payload size");
    ck(cudaMemcpy(b.data(), v.data(), v.size(), cudaMemcpyHostToDevice));
}

static std::vector<unsigned char> download(Buf & b) {
    std::vector<unsigned char> v(b.n);
    ck(cudaMemcpy(v.data(), b.raw, b.n, cudaMemcpyDeviceToHost));
    return v;
}

static void init_tensor(ggml_tensor & t, ggml_type ty, std::array<int64_t, 4> ne,
                        size_t pr, size_t pp, size_t pb) {
    t = ggml_tensor{};
    t.type = ty;
    for (int i = 0; i < 4; ++i) t.ne[i] = ne[i];
    t.nb[0] = ggml_type_size(ty);
    t.nb[1] = ggml_row_size(ty, ne[0]) + pr;
    t.nb[2] = t.nb[1] * size_t(ne[1]) + pp;
    t.nb[3] = t.nb[2] * size_t(ne[2]) + pb;
}

static size_t span(const ggml_tensor & t) {
    return ggml_row_size(t.type, t.ne[0])
         + size_t(t.ne[1] - 1) * t.nb[1]
         + size_t(t.ne[2] - 1) * t.nb[2]
         + size_t(t.ne[3] - 1) * t.nb[3];
}

static ggml_type dst_type_of(int k) { return k ? GGML_TYPE_Q4_0 : GGML_TYPE_F32; }
static ggml_type idx_type_of(int k) { return k ? GGML_TYPE_I64 : GGML_TYPE_I32; }

struct Case {
    int dtype;
    int itype;
    int ne0;
    int caprows;
    int nrows;
    int nhead;
    int nseq;
    int chead;
    int cseq;
    int ps[3];
    int pd[3];
    int pi[3];
    int pattern;
};

static float randf(size_t k) {
    uint32_t x = uint32_t(k * 2654435761ull) + 0x9E3779B9u;
    uint32_t b = ((x & 0x807FFFFFu) | ((115u + (x % 23u)) << 23)) | 1u;
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}

static float src_value(int p, size_t i0, size_t k) {
    if (p == 0) return 0.0f;
    if (p == 1) return (k & 1) ? -0.0f : 0.0f;
    if (p == 3 || p == 4) {
        const size_t j = i0 % 32;
        const float a = 0.7f;
        if (j == 0) return p == 3 ? a : -a;
        if (j == 1) return p == 3 ? -a : a;
        return float(int(j % 7) - 3) * 0.03125f;
    }
    if (p == 5) return randf(k * 7 + 11) * (1.0f + float(k % 5));
    return randf(k);
}

static void fill_source(std::vector<unsigned char> & v, const ggml_tensor & t, int p) {
    size_t k = 0;
    for (int64_t i3 = 0; i3 < t.ne[3]; ++i3)
        for (int64_t i2 = 0; i2 < t.ne[2]; ++i2)
            for (int64_t i1 = 0; i1 < t.ne[1]; ++i1) {
                const size_t base = size_t(i1) * t.nb[1] + size_t(i2) * t.nb[2] + size_t(i3) * t.nb[3];
                for (int64_t i0 = 0; i0 < t.ne[0]; ++i0) {
                    const float f = src_value(p, size_t(i0), k++);
                    std::memcpy(v.data() + base + size_t(i0) * t.nb[0], &f, 4);
                }
            }
}

static int64_t gcd64(int64_t a, int64_t b) {
    while (b) { int64_t t = a % b; a = b; b = t; }
    return a;
}

static int64_t perm_step(int64_t cap) {
    if (cap <= 1) return 1;
    int64_t s = 3 % cap;
    if (!s) s = 1;
    while (gcd64(s, cap) != 1) { if (++s >= cap) s = 1; }
    return s;
}

static void fill_indices(std::vector<unsigned char> & v, const ggml_tensor & t, int64_t cap, size_t seed) {
    const int64_t step = perm_step(cap);
    for (int64_t i2 = 0; i2 < t.ne[2]; ++i2)
        for (int64_t i1 = 0; i1 < t.ne[1]; ++i1) {
            const int64_t sh = int64_t((size_t(i1) * 7 + size_t(i2) * 13 + seed) % size_t(cap));
            for (int64_t i0 = 0; i0 < t.ne[0]; ++i0) {
                const int64_t val = (i0 * step + sh) % cap;
                const size_t o = size_t(i0) * t.nb[0] + size_t(i1) * t.nb[1] + size_t(i2) * t.nb[2];
                if (t.type == GGML_TYPE_I64) std::memcpy(v.data() + o, &val, 8);
                else { int32_t x = int32_t(val); std::memcpy(v.data() + o, &x, 4); }
            }
        }
}

static int64_t read_idx(const std::vector<unsigned char> & v, const ggml_tensor & t,
                        int64_t i0, int64_t i1, int64_t i2) {
    const size_t o = size_t(i0) * t.nb[0] + size_t(i1) * t.nb[1] + size_t(i2) * t.nb[2];
    if (t.type == GGML_TYPE_I64) { int64_t x; std::memcpy(&x, v.data() + o, 8); return x; }
    int32_t x; std::memcpy(&x, v.data() + o, 4); return int64_t(x);
}

static void mark_written(std::vector<unsigned char> & w, const std::vector<unsigned char> & ih,
                         const ggml_tensor & im, const ggml_tensor & dm, size_t ci) {
    const size_t row = ggml_row_size(dm.type, dm.ne[0]);
    for (int64_t i3 = 0; i3 < dm.ne[3]; ++i3)
        for (int64_t i2 = 0; i2 < dm.ne[2]; ++i2) {
            const int64_t j1 = i2 % im.ne[1];
            const int64_t j2 = i3 % im.ne[2];
            for (int64_t i1 = 0; i1 < im.ne[0]; ++i1) {
                const int64_t dr = read_idx(ih, im, i1, j1, j2);
                if (dr < 0 || dr >= dm.ne[1]) {
                    std::fprintf(stderr, "INDEX case=%zu row=%lld cap=%lld\n", ci, (long long) dr, (long long) dm.ne[1]);
                    req(false, "index in destination");
                }
                const size_t base = size_t(dr) * dm.nb[1] + size_t(i2) * dm.nb[2] + size_t(i3) * dm.nb[3];
                if (base + row > w.size()) req(false, "written span");
                if (w[base]) {
                    std::fprintf(stderr, "OVERLAP case=%zu row=%lld plane=%lld,%lld\n", ci, (long long) dr, (long long) i2, (long long) i3);
                    req(false, "unique destination rows");
                }
                for (size_t b = 0; b < row; ++b) w[base + b] = 1;
            }
        }
}

static void cmp_bytes(const char * tag, size_t ci, size_t unit,
                      const unsigned char * g, size_t go,
                      const unsigned char * want, size_t wo, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        if (g[go + i] == want[wo + i]) continue;
        std::fprintf(stderr, "MISMATCH case=%zu %s byte=%zu unit=%zu got=0x%02X want=0x%02X\n",
                     ci, tag, i, i / unit, unsigned(g[go + i]), unsigned(want[wo + i]));
        req(false, tag);
    }
    ++g_checks;
}

static void check_guard(const std::vector<unsigned char> & raw, unsigned char f, const char * tag, size_t ci) {
    for (size_t i = 0; i < GUARD; ++i) {
        if (raw[i] != f || raw[raw.size() - 1 - i] != f) {
            std::fprintf(stderr, "GUARD case=%zu %s off=%zu\n", ci, tag, i);
            req(false, "guard bytes");
        }
    }
    ++g_checks;
}

static void check_untouched(const std::vector<unsigned char> & raw, const std::vector<unsigned char> & w,
                            size_t ds, const char * tag, size_t ci) {
    for (size_t i = 0; i < ds; ++i) {
        if (!w[i] && raw[GUARD + i] != DST_FILL) {
            std::fprintf(stderr, "PAD case=%zu %s byte=%zu got=0x%02X\n", ci, tag, i, unsigned(raw[GUARD + i]));
            req(false, "untouched destination");
        }
    }
    ++g_checks;
}

static void validate(const Case & c, const ggml_tensor & s, const ggml_tensor & im, const ggml_tensor & d) {
    req(c.ne0 > 0 && c.caprows > 0 && c.nrows > 0 && c.nhead > 0 && c.nseq > 0, "positive dims");
    req(c.chead > 0 && c.cseq > 0, "positive index dims");
    req(c.nrows <= c.caprows, "nrows <= caprows");
    req(c.nhead % c.chead == 0 && c.nseq % c.cseq == 0, "broadcast divides");
    req(c.dtype == 0 || c.ne0 % 32 == 0, "q4 block multiple");
    const size_t du = c.dtype ? 18 : 4;
    const size_t iu = c.itype ? 8 : 4;
    req(d.nb[1] % du == 0 && d.nb[2] % du == 0 && d.nb[3] % du == 0, "dst stride unit");
    req(s.nb[1] % 4 == 0 && s.nb[2] % 4 == 0 && s.nb[3] % 4 == 0, "src stride unit");
    req(im.nb[1] % iu == 0 && im.nb[2] % iu == 0 && im.nb[3] % iu == 0, "idx stride unit");
    req(d.nb[1] >= ggml_row_size(d.type, d.ne[0]), "dst rows disjoint");
}

static void run_case(ggml_backend_cuda_context & ctx, const Case & c, size_t ci, bool capture, bool rollback) {
    if (c.dtype) req(c.ne0 % 32 == 0, "q4 block multiple");
    const std::array<int64_t, 4> sn = {c.ne0, c.nrows, c.nhead, c.nseq};
    const std::array<int64_t, 4> ine = {c.nrows, c.chead, c.cseq, 1};
    const std::array<int64_t, 4> dn = {c.ne0, c.caprows, c.nhead, c.nseq};

    ggml_tensor sm, im, dm;
    init_tensor(sm, GGML_TYPE_F32, sn, c.ps[0], c.ps[1], c.ps[2]);
    init_tensor(im, idx_type_of(c.itype), ine, c.pi[0], c.pi[1], c.pi[2]);
    init_tensor(dm, dst_type_of(c.dtype), dn, c.pd[0], c.pd[1], c.pd[2]);
    validate(c, sm, im, dm);

    const size_t ss = span(sm);
    const size_t isz = span(im);
    const size_t ds = span(dm);
    std::vector<unsigned char> sh(ss, SRC_FILL), ih(isz, SRC_FILL), dh(ds, DST_FILL);
    fill_source(sh, sm, c.pattern);
    fill_indices(ih, im, c.caprows, ci * 17 + 3);

    std::vector<unsigned char> written(ds, 0);
    mark_written(written, ih, im, dm, ci);

    Buf bsn(ss, SRC_FILL), bin(isz, SRC_FILL), bdn(ds, DST_FILL);
    Buf bsg(ss, SRC_FILL), big(isz, SRC_FILL), bdg(ds, DST_FILL);
    upload(bsn, sh); upload(bin, ih); upload(bdn, dh);
    upload(bsg, sh); upload(big, ih); upload(bdg, dh);
    ck(cudaDeviceSynchronize());

    auto mk = [&](Buf & sb, Buf & ib, Buf & db, ggml_tensor & st, ggml_tensor & it, ggml_tensor & at, ggml_tensor & dt) {
        init_tensor(st, GGML_TYPE_F32, sn, c.ps[0], c.ps[1], c.ps[2]);
        init_tensor(it, idx_type_of(c.itype), ine, c.pi[0], c.pi[1], c.pi[2]);
        init_tensor(at, dst_type_of(c.dtype), dn, c.pd[0], c.pd[1], c.pd[2]);
        st.data = sb.data();
        it.data = ib.data();
        at.data = db.data();
        dt = at;
        dt.src[0] = &st;
        dt.src[1] = &it;
        dt.view_src = &at;
        dt.view_offs = 0;
        dt.op = GGML_OP_SET_ROWS;
    };

    ggml_tensor snt, intt, ant, dnt, sgt, igt, agt, dgt;
    mk(bsn, bin, bdn, snt, intt, ant, dnt);
    mk(bsg, big, bdg, sgt, igt, agt, dgt);

    setenv("GGML_GOOSE_CUDA_SET_ROWS","0",1);
    ggml_cuda_op_set_rows(ctx, &dnt);
    if(rollback) setenv("GGML_GOOSE_CUDA_SET_ROWS","0",1); else unsetenv("GGML_GOOSE_CUDA_SET_ROWS");
    ck(cudaGetLastError());
    ck(cudaStreamSynchronize(ctx.stream()));

    {
        ggml_cuda_op_set_rows(ctx, &dgt);
        ck(cudaGetLastError());
        ck(cudaStreamSynchronize(ctx.stream()));
    }

    const std::vector<unsigned char> nraw = download(bdn);
    const std::vector<unsigned char> snr = download(bsn);
    const std::vector<unsigned char> inr = download(bin);
    check_guard(nraw, DST_FILL, "native-dst", ci);
    check_guard(snr, SRC_FILL, "native-src", ci);
    check_guard(inr, SRC_FILL, "native-idx", ci);
    cmp_bytes("native-src-ro", ci, 4, snr.data(), GUARD, sh.data(), 0, ss);
    cmp_bytes("native-idx-ro", ci, c.itype ? 8 : 4, inr.data(), GUARD, ih.data(), 0, isz);
    check_untouched(nraw, written, ds, "native-untouched", ci);

    {
        const std::vector<unsigned char> graw = download(bdg);
        const std::vector<unsigned char> sgr = download(bsg);
        const std::vector<unsigned char> igr = download(big);
        check_guard(graw, DST_FILL, "goose-dst", ci);
        check_guard(sgr, SRC_FILL, "goose-src", ci);
        check_guard(igr, SRC_FILL, "goose-idx", ci);
        cmp_bytes("goose-src-ro", ci, 4, sgr.data(), GUARD, sh.data(), 0, ss);
        cmp_bytes("goose-idx-ro", ci, c.itype ? 8 : 4, igr.data(), GUARD, ih.data(), 0, isz);
        cmp_bytes("goose-native", ci, c.dtype ? 18 : 4, graw.data(), GUARD, nraw.data(), GUARD, ds);
        check_untouched(graw, written, ds, "goose-untouched", ci);

        if (capture) {
            Buf bdc(ds, DST_FILL);
            upload(bdc, dh);
            ck(cudaDeviceSynchronize());
            ggml_tensor sct, ict, act, dct;
            mk(bsg, big, bdc, sct, ict, act, dct);
            (void) ctx.stream();
            ggml_cuda_op_set_rows(ctx, &dct);
            ck(cudaGetLastError());
            ck(cudaStreamSynchronize(ctx.stream()));

            ck(cudaStreamBeginCapture(ctx.stream(), cudaStreamCaptureModeThreadLocal));
            ggml_cuda_op_set_rows(ctx, &dct);
            cudaGraph_t graph = nullptr;
            ck(cudaStreamEndCapture(ctx.stream(), &graph));
            cudaGraphExec_t exec = nullptr;
            ck(cudaGraphInstantiate(&exec, graph, nullptr, nullptr, 0));
            ck(cudaMemsetAsync(bdc.data(),DST_FILL,ds,ctx.stream()));
            ck(cudaGraphLaunch(exec, ctx.stream()));
            ck(cudaStreamSynchronize(ctx.stream()));
            ck(cudaGraphExecDestroy(exec));
            ck(cudaGraphDestroy(graph));

            const std::vector<unsigned char> craw = download(bdc);
            check_guard(craw, DST_FILL, "capture-dst", ci);
            cmp_bytes("capture-native", ci, c.dtype ? 18 : 4, craw.data(), GUARD, nraw.data(), GUARD, ds);
            check_untouched(craw, written, ds, "capture-untouched", ci);
        }
    }
    ++g_cases;
}

static const Case CASES[] = {
    {1, 1,  64, 16, 8, 3, 2, 3, 2, { 0,  0,  0}, {18, 36, 54}, { 0, 8, 16}, 2},
    {1, 0,  32, 12, 5, 2, 4, 1, 2, {16,  4,  8}, { 0, 18,  0}, { 4, 0,  0}, 3},
    {1, 1,  96, 10, 7, 4, 3, 2, 1, { 8,  0, 16}, {18,  0, 18}, { 8, 8,  0}, 4},
    {1, 0,  32,  4, 4, 1, 1, 1, 1, { 0,  0,  0}, { 0,  0,  0}, { 0, 0,  0}, 0},
    {1, 1,  64,  8, 6, 2, 2, 2, 1, { 0,  0,  0}, { 0,  0,  0}, { 0, 0,  0}, 1},
    {1, 0, 128,  9, 9, 3, 2, 1, 2, { 4,  8,  0}, {36, 18,  0}, { 4, 4,  0}, 5},
    {0, 1,  37, 11, 6, 3, 2, 3, 1, { 4, 12,  0}, { 8,  4, 16}, { 0, 8,  0}, 2},
    {0, 0,  19,  8, 5, 2, 4, 1, 4, { 0,  4,  8}, { 0, 12,  4}, { 4, 0,  8}, 3},
    {0, 1, 257,  9, 7, 2, 2, 2, 1, { 4,  0,  0}, {12,  8,  0}, { 8, 0,  0}, 4},
    {0, 0,   2,  3, 2, 1, 1, 1, 1, { 0,  0,  0}, { 0,  0,  0}, { 0, 0,  0}, 1},
    {0, 1,  64,  6, 6, 2, 2, 1, 2, { 0,  8,  0}, { 4, 16,  8}, { 0, 8,  0}, 0},
    {1, 0,  32,  1, 1, 1, 1, 1, 1, { 0,  0,  0}, { 0,  0,  0}, { 0, 0,  0}, 5},
    {1, 1, 256,4096,4096,2,1,1,1,{0,0,0},{0,0,0},{0,0,0},5},
};


static void invalid_indices(ggml_backend_cuda_context &ctx) {
    for(int q=0;q<2;++q) for(int wide=0;wide<2;++wide) {
        ggml_tensor a,ix,d;
        init_tensor(a,GGML_TYPE_F32,{32,2,1,1},0,0,0);
        init_tensor(ix,wide?GGML_TYPE_I64:GGML_TYPE_I32,{2,1,1,1},0,0,0);
        init_tensor(d,q?GGML_TYPE_Q4_0:GGML_TYPE_F32,{32,2,1,1},0,0,0);
        Buf ab(span(a),SRC_FILL),ib(span(ix),SRC_FILL),db(span(d),DST_FILL);
        std::vector<unsigned char> av(ab.size(),SRC_FILL),iv(ib.size(),SRC_FILL);
        fill_source(av,a,2);
        for(int i=0;i<2;++i) {
            if(wide){int64_t n=i?(1LL<<40):-1;std::memcpy(iv.data()+i*8,&n,8);}
            else{int32_t n=i?2:-1;std::memcpy(iv.data()+i*4,&n,4);}
        }
        upload(ab,av);upload(ib,iv);a.data=ab.data();ix.data=ib.data();d.data=db.data();d.src[0]=&a;d.src[1]=&ix;d.op=GGML_OP_SET_ROWS;
        unsetenv("GGML_GOOSE_CUDA_SET_ROWS");ggml_cuda_op_set_rows(ctx,&d);ck(cudaStreamSynchronize(ctx.stream()));
        auto raw=download(db);for(auto b:raw)req(b==DST_FILL,"invalid indices make no destination writes");
        auto ar=download(ab),ir=download(ib);check_guard(ar,SRC_FILL,"invalid-source",q*2+wide);check_guard(ir,SRC_FILL,"invalid-index",q*2+wide);
        cmp_bytes("invalid-source-readonly",0,4,ar.data(),GUARD,av.data(),0,av.size());cmp_bytes("invalid-index-readonly",0,wide?8:4,ir.data(),GUARD,iv.data(),0,iv.size());
    }
    uint64_t witness[2]={};ck((cudaError_t)fg176r_cuda_device_read(witness));req(witness[0]==15&&witness[1]==132,"invalid-index device guards actually executed");
    std::printf("PASS invalid_index_routes=4 invalid_index_threads=%llu checks=%zu\n",(unsigned long long)witness[1],g_checks);
}

int main(int argc,char**) {
    ck(cudaSetDevice(0));
    ggml_backend_cuda_context ctx(0);
    if(argc>1){invalid_indices(ctx);return 0;}
    const char * env = std::getenv("GGML_GOOSE_CUDA_SET_ROWS");
    const bool rollback = env && std::strcmp(env, "0") == 0;
    const size_t n = sizeof(CASES) / sizeof(CASES[0]);
    for (size_t i = 0; i < n; ++i) run_case(ctx, CASES[i], i, i == 0 || i == 6, rollback);
    std::printf("PASS cases=%zu checks=%zu rollback=%d graph_capture=%d\n",
                g_cases, g_checks, rollback ? 1 : 0, 2);
    uint64_t witness[2]={}; ck((cudaError_t)fg176r_cuda_device_read(witness));
    req(witness[1]==0,"no invalid indices on valid fixtures");
    req(rollback ? witness[0]==0 : witness[0]==15,"actual four Goose device routes");
    req(rollback ? fg176r_cuda_host_seen()==0 : fg176r_cuda_host_seen()==15,"actual host routes");
}
