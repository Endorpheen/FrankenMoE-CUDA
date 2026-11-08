extern "C" unsigned long long fg175fa_cuda_host_seen();
extern "C" int fg175fa_cuda_device_read(unsigned long long*);
// EXP175 Apex fixture: FULL public ggml_cuda_flash_attn_ext API plus supported/get_alloc_size queries vs fg175_native_ aliases.
// Q F32 [256,nq,16,ns], K/V Q4_0 [256,nkv,2,ns], mask F16 [nkv,ceil32(nq),1,ns], dst F32 [256,16,nq,ns], GQA 8.
// Same complete inputs to both entry points, no forced kernel route, no timings, no witness ABI; prints counts only.
#include "ggml-cuda/common.cuh"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

void ggml_cuda_flash_attn_ext(ggml_backend_cuda_context & ctx, ggml_tensor * dst);
bool ggml_cuda_flash_attn_ext_supported(int device, const ggml_tensor * dst);
size_t ggml_cuda_flash_attn_ext_get_alloc_size(int device, const ggml_tensor * dst);
void fg175_native_ggml_cuda_flash_attn_ext(ggml_backend_cuda_context & ctx, ggml_tensor * dst);
bool fg175_native_ggml_cuda_flash_attn_ext_supported(int device, const ggml_tensor * dst);
size_t fg175_native_ggml_cuda_flash_attn_ext_get_alloc_size(int device, const ggml_tensor * dst);

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
    static constexpr size_t guard = 128;
    explicit Buffer(std::vector<unsigned char> host) : shadow(std::move(host)) {
        n = shadow.size();
        ck(cudaMalloc(&raw, n));
        ck(cudaMemcpy(raw, shadow.data(), n, cudaMemcpyHostToDevice));
    }
    ~Buffer() { if (raw) { ck(cudaFree(raw)); } }
    void * data() const { return raw + guard; }
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

static void fill_q(std::vector<unsigned char> & v, const int64_t ne[4], const size_t nb[4]) {
    size_t lin = 0;
    for (int64_t i3 = 0; i3 < ne[3]; i3++) for (int64_t i2 = 0; i2 < ne[2]; i2++) for (int64_t i1 = 0; i1 < ne[1]; i1++) for (int64_t i0 = 0; i0 < ne[0]; i0++) {
        put_f32(v, i0*nb[0] + i1*nb[1] + i2*nb[2] + i3*nb[3], float(int((lin*31337ull) % 50003ull) - 25001) / 4096.0f);
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
// Causal with prefix offset: valid query i1 sees keys i0 <= (nkv >= nq ? nkv - nq + i1 : i1 % nkv), key 0 always visible.
// Padded mask rows i1 >= nq are all -inf; valid rows hold finite 0 or -inf; row stride stays nkv*2 bytes.
static void fill_mask(std::vector<unsigned char> & v, int64_t kvlen, int64_t nq, int64_t ne1, int64_t ns, const size_t nb[4]) {
    for (int64_t i3 = 0; i3 < ns; i3++) for (int64_t i1 = 0; i1 < ne1; i1++) for (int64_t i0 = 0; i0 < kvlen; i0++) {
        const int64_t lim = kvlen >= nq ? kvlen - nq + i1 : i1 % kvlen;
        put_u16(v, i3*nb[3] + i0*nb[0] + i1*nb[1], (i1 < nq && i0 <= lim) ? 0x0000 : 0xfc00);
    }
}
static void tset(ggml_tensor & t, ggml_type ty, const int64_t ne[4], const size_t nb[4], void * data) {
    std::memset(&t, 0, sizeof t);
    t.type = ty;
    for (int i = 0; i < 4; i++) { t.ne[i] = ne[i]; t.nb[i] = nb[i]; }
    t.data = data;
}
static void wire_dst(ggml_tensor & td, const int64_t ne[4], const size_t nb[4], void * data, ggml_tensor & tq, ggml_tensor & tk, ggml_tensor & tv, ggml_tensor * tm) {
    tset(td, GGML_TYPE_F32, ne, nb, data);
    td.op = GGML_OP_FLASH_ATTN_EXT;
    const float op[3] = {1.0f/16.0f, 0.0f, 0.0f};
    std::memcpy(td.op_params, op, sizeof op);
    td.src[0] = &tq; td.src[1] = &tk; td.src[2] = &tv; td.src[3] = tm;
}

struct Case { int nq, kv, qpad, kvpad, ns, capture; };
static const Case case_table[8] = {
    {1, 256, 32, 18, 1, 0}, {2, 512, 32, 18, 1, 1}, {3, 256, 32, 18, 2, 0}, {4, 512, 32, 18, 1, 0},
    {5, 1024, 32, 18, 1, 0}, {13, 256, 32, 18, 1, 0}, {1024, 512, 32, 18, 1, 0}, {2048, 2048, 32, 18, 1, 0},
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
    const int nq = c.nq, kv = c.kv, ns = c.ns;
    const int64_t ne1 = (nq + 31) / 32 * 32;
    const int64_t qne[4] = {256, nq, 16, ns}, kne[4] = {256, kv, 2, ns}, mne[4] = {kv, ne1, 1, ns}, dne[4] = {256, 16, nq, ns};
    size_t qnb[4] = {4, size_t(1024 + c.qpad), 0, 0};
    qnb[2] = qnb[1]*size_t(nq) + c.qpad; qnb[3] = qnb[2]*16;
    size_t knb[4] = {18, size_t(144 + c.kvpad), 0, 0};
    knb[2] = knb[1]*size_t(kv) + c.kvpad; knb[3] = knb[2]*2 + c.kvpad;
    const size_t mnb[4] = {2, size_t(kv)*2, size_t(kv)*2*ne1, size_t(kv)*2*ne1};
    const size_t dnb[4] = {4, 1024, size_t(1024)*16, size_t(1024)*16*size_t(nq)};
    const size_t qbytes = (ns-1)*qnb[3] + 15*qnb[2] + (nq-1)*qnb[1] + 1024;
    const size_t kbytes = (ns-1)*knb[3] + knb[2] + (kv-1)*knb[1] + 144;
    const size_t mbytes = (ns-1)*mnb[3] + (ne1-1)*mnb[1] + size_t(kv)*2;
    const size_t dbytes = (ns-1)*dnb[3] + (nq-1)*dnb[2] + 15*dnb[1] + 1024;
    snprintf(cur_desc, sizeof cur_desc, "nq=%d kv=%d ns=%d qpad=%d kvpad=%d", nq, kv, ns, c.qpad, c.kvpad);

    std::vector<unsigned char> qh = stage(qbytes), kh = stage(kbytes), vh = stage(kbytes), mh = stage(mbytes);
    fill_q(qh, qne, qnb); fill_kv(kh, kne, knb, 1); fill_kv(vh, kne, knb, 3); fill_mask(mh, kv, nq, ne1, ns, mnb);
    Buffer qa(qh), qb(qh), ka(kh), kb(kh), va(vh), vb(vh), ma(mh), mb(mh);

    ggml_tensor tq{}, tk{}, tv{}, tm{}, tqb{}, tkb{}, tvb{}, tmb{}, td{}, tdb{};
    tset(tq, GGML_TYPE_F32, qne, qnb, qa.data());
    tset(tk, GGML_TYPE_Q4_0, kne, knb, ka.data());
    tset(tv, GGML_TYPE_Q4_0, kne, knb, va.data());
    tset(tm, GGML_TYPE_F16, mne, mnb, ma.data());
    tset(tqb, GGML_TYPE_F32, qne, qnb, qb.data());
    tset(tkb, GGML_TYPE_Q4_0, kne, knb, kb.data());
    tset(tvb, GGML_TYPE_Q4_0, kne, knb, vb.data());
    tset(tmb, GGML_TYPE_F16, mne, mnb, mb.data());
    wire_dst(td, dne, dnb, nullptr, tq, tk, tv, &tm);
    wire_dst(tdb, dne, dnb, nullptr, tqb, tkb, tvb, &tmb);

    // Sizing/support queries with data = NULL, then a 128-aligned fake base; public must equal the native alias.
    req(ggml_cuda_flash_attn_ext_supported(ctx.device, &td) && fg175_native_ggml_cuda_flash_attn_ext_supported(ctx.device, &td), "supported NULL data");
    const size_t alloc_n = ggml_cuda_flash_attn_ext_get_alloc_size(ctx.device, &td);
    req(alloc_n >= dbytes, "alloc covers dst payload");
    req(fg175_native_ggml_cuda_flash_attn_ext_get_alloc_size(ctx.device, &td) == alloc_n, "alias alloc NULL data");
    td.data = (void *) 0x100000;
    req(ggml_cuda_flash_attn_ext_supported(ctx.device, &td) && fg175_native_ggml_cuda_flash_attn_ext_supported(ctx.device, &td), "supported fake base");
    req(ggml_cuda_flash_attn_ext_get_alloc_size(ctx.device, &td) == alloc_n, "public alloc fake base");
    req(fg175_native_ggml_cuda_flash_attn_ext_get_alloc_size(ctx.device, &td) == alloc_n, "alias alloc fake base");

    // Full get_alloc_size payload (dst plus legitimate f16 K/V stage region) under 64B canaries; staged f16 writes are expected.
    Buffer da(stage(alloc_n)), db(stage(alloc_n));
    td.data = da.data();
    tdb.data = db.data();
    req(ggml_cuda_flash_attn_ext_get_alloc_size(ctx.device, &td) == alloc_n, "public alloc real base");
    req(fg175_native_ggml_cuda_flash_attn_ext_get_alloc_size(ctx.device, &td) == alloc_n, "alias alloc real base");

    ck(cudaDeviceSynchronize());
    fg175_native_ggml_cuda_flash_attn_ext(ctx, &td);
    ck(cudaGetLastError());
    ck(cudaStreamSynchronize(ctx.stream()));
    ggml_cuda_flash_attn_ext(ctx, &tdb);
    ck(cudaGetLastError());
    ck(cudaStreamSynchronize(ctx.stream()));
    const std::vector<unsigned char> ref = da.fetch();
    compare_dst(ref, db.fetch(), dne, dnb);
    req(qa.intact() && qb.intact(), "q unchanged");
    req(ka.intact() && kb.intact(), "k unchanged");
    req(va.intact() && vb.intact(), "v unchanged");
    req(ma.intact() && mb.intact(), "mask unchanged");
    if (c.capture) {
        ck(cudaMemsetAsync(db.raw, 0xA5, db.n, ctx.stream()));
        ck(cudaStreamSynchronize(ctx.stream()));
        cudaGraph_t graph = nullptr;
        ck(cudaStreamBeginCapture(ctx.stream(), cudaStreamCaptureModeThreadLocal));
        ggml_cuda_flash_attn_ext(ctx, &tdb);
        ck(cudaStreamEndCapture(ctx.stream(), &graph));
        size_t nodes = 0;
        ck(cudaGraphGetNodes(graph, nullptr, &nodes));
        req(nodes >= 1, "capture recorded kernels");
        cudaGraphExec_t exec = nullptr;
        ck(cudaGraphInstantiateWithFlags(&exec, graph, 0));
        ck(cudaGraphLaunch(exec, ctx.stream()));
        ck(cudaStreamSynchronize(ctx.stream()));
        compare_dst(ref, db.fetch(), dne, dnb);
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
    std::memcpy(cur_desc, "totals", 7);
    ck(cudaDeviceSynchronize());
    unsigned long long witness[3]={};req(fg175fa_cuda_device_read(witness)==0,"FA witness readable");const char*flag=getenv("GGML_GOOSE_CUDA_FA");const bool rollback=flag&&strcmp(flag,"0")==0;
    if(rollback){req(fg175fa_cuda_host_seen()==0&&witness[0]==0&&witness[1]==0&&witness[2]==0,"rollback bypasses all Goose FA paths");}
    else{req((fg175fa_cuda_host_seen()&15)==15,"all host roles/routes executed");req((witness[0]&7)==7,"vector N1/N2/combine executed");req((witness[2]&(4|8|32|64))==(4|8|32|64),"MMA N4/N8,strided stage and mask executed");}
    printf("WITNESS host%llx vec%llx features%llx prefill%llx\n",fg175fa_cuda_host_seen(),witness[0],witness[1],witness[2]);
    printf("PASS cases=%zu checks=%zu captures=%zu\n", cases, checks, captures);
    return 0;
}
