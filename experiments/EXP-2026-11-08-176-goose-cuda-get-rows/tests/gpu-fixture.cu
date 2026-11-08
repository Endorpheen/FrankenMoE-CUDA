// GPU differential fixture for the Goose GET_ROWS replacement.
//
// Contract: the candidate DSO exports ggml_cuda_op_get_rows, which either runs
// the Goose route or, with GGML_GOOSE_CUDA_GET_ROWS=0, the private-renamed
// native one. This fixture allocates real device tensors, builds a real
// ggml_tensor graph node for GGML_OP_GET_ROWS, calls the public entry and
// compares the destination against an independent host reference.
//
// Cases cover both Goose kernels: the 16-byte vector path (same src/dst type,
// aligned, enough blocks) and the element scalar path (non-aligned or too few
// blocks), plus multi-plane src1 indices so the fastdiv decode is exercised.
#include "common.cuh"
void ggml_cuda_op_get_rows(ggml_backend_cuda_context & ctx, ggml_tensor * dst);
#include <cstdio>
#include <cstring>
#include <cuda_runtime.h>
#include <vector>
#include <random>
#include <string>
#include <cstdlib>

static int g_checks, g_fail;
static void check(bool ok, const char * what) {
    ++g_checks;
    if (!ok) { ++g_fail; std::fprintf(stderr, "FAIL %s\n", what); }
}

struct Case {
    int64_t ne00, ne01, ne02, ne03;   // src0 extents
    int64_t ne10, ne11, ne12;         // src1 extents, ne13 = 1
    int64_t pad;                      // extra element padding to misalign strides
    const char * name;
};

// ggml GET_ROWS invariants, as asserted by the CPU reference
// ggml_compute_forward_get_rows_f32:
//   dst->ne[0] == src0->ne[0]
//   dst->ne[2] == src0->ne[2] == src1->ne[1]
//   dst->ne[1] == src1->ne[0]
//   dst->ne[3] == src0->ne[3], src1->ne[3] == 1
// Every case below satisfies them; the earlier cases did not, which is why the
// unchanged native arm failed them too.
static const Case CASES[] = {
    // enough blocks, aligned -> vector path. ne02=1 therefore ne11=1.
    {256, 8, 1, 1, 32, 1, 1, 0, "vec-basic"},
    {1024, 4, 2, 1, 16, 2, 2, 0, "vec-planes"},
    // too few blocks -> scalar path
    {64, 2, 1, 1, 8, 1, 1, 0, "scalar-few-blocks"},
    // ne00 odd: native asserts ne00%2==0 for the quantized path only, the float
    // path allows any ne00; 255 keeps VEC==4 rejected -> scalar path.
    {254, 4, 1, 1, 16, 1, 1, 0, "scalar-odd-row"},
    // larger multi-plane selection, ne02==ne11
    {512, 6, 3, 1, 16, 3, 3, 0, "vec-multiplane"},
};

int main(int argc, char ** argv) {
    (void) argc; (void) argv;
    int dev = 0;
    if (ggml_backend_cuda_get_device_count() < 1) { std::fprintf(stderr, "no CUDA device\n"); return 77; }
    cudaSetDevice(dev);
    std::mt19937 rng(176001);

    const char * only = getenv("FG176G_CASE");
    for (const Case & c : CASES) {
        if (only && std::strcmp(only, c.name) != 0) continue;
        const int64_t ne00 = c.ne00, ne01 = c.ne01, ne02 = c.ne02, ne03 = c.ne03;
        const int64_t ne10 = c.ne10, ne11 = c.ne11, ne12 = c.ne12;
        const int64_t ns0 = ne00 * ne01 * ne02 * ne03;
        const int64_t ns1 = ne10 * ne11 * ne12;
        const int64_t nd  = ne10 * ne11 * ne12 * ne00;   // dst is [ne00, ne10, ne11, ne12]
        std::vector<float> h0(ns0), h1i(ns1), ref(nd, -777.0f);
        std::uniform_real_distribution<float> d0(-3.0f, 3.0f);
        for (auto & v : h0) v = d0(rng);
        // src1 holds row indices in [0, ne01) with duplicates, as a real model does.
        std::uniform_int_distribution<int32_t> di(0, (int32_t)ne01 - 1);
        std::vector<int32_t> idx(ns1);
        for (auto & v : idx) v = di(rng);

        // Independent reference: dst[i00, i10, i11, i12] = src0[i00, idx[i10,i11,i12], i11, i12].
        for (int64_t i12 = 0; i12 < ne12; ++i12)
        for (int64_t i11 = 0; i11 < ne11; ++i11)
        for (int64_t i10 = 0; i10 < ne10; ++i10) {
            const int64_t r = idx[(i12*ne11 + i11)*ne10 + i10];
            for (int64_t i00 = 0; i00 < ne00; ++i00) {
                const int64_t s = ((i12*ne02 + i11)*ne01 + r)*ne00 + i00;
                const int64_t t = ((i12*ne11 + i11)*ne10 + i10)*ne00 + i00;
                ref[t] = h0[s];
            }
        }

        float * d0p = nullptr, * ddp = nullptr;
        int32_t * d1p = nullptr;
        cudaMalloc(&d0p, ns0*sizeof(float));
        cudaMalloc(&d1p, ns1*sizeof(int32_t));
        cudaMalloc(&ddp, nd*sizeof(float));
        cudaMemcpy(d0p, h0.data(), ns0*sizeof(float), cudaMemcpyHostToDevice);
        cudaMemcpy(d1p, idx.data(), ns1*sizeof(int32_t), cudaMemcpyHostToDevice);
        cudaMemset(ddp, 0xFF, nd*sizeof(float));

        ggml_tensor t0{}, t1{}, td{};
        const int64_t ne[4]  = {ne00, ne01, ne02, ne03};
        const int64_t ni[4]  = {ne10, ne11, ne12, 1};
        const int64_t ndm[4] = {ne00, ne10, ne11, ne12};
        for (int i = 0; i < 4; ++i) { t0.ne[i] = ne[i]; t1.ne[i] = ni[i]; td.ne[i] = ndm[i]; }
        t0.type = td.type = GGML_TYPE_F32;
        t1.type = GGML_TYPE_I32;
        t0.data = d0p; t1.data = d1p; td.data = ddp;
        t0.nb[0] = sizeof(float);
        t1.nb[0] = sizeof(int32_t);
        td.nb[0] = sizeof(float);
        t0.nb[1] = t0.nb[0]*t0.ne[0];
        t1.nb[1] = t1.nb[0]*t1.ne[0];
        td.nb[1] = td.nb[0]*td.ne[0];
        t0.nb[2] = t0.nb[1]*t0.ne[1];
        t1.nb[2] = t1.nb[1]*t1.ne[1];
        td.nb[2] = td.nb[1]*td.ne[1];
        t0.nb[3] = t0.nb[2]*t0.ne[2];
        t1.nb[3] = t1.nb[2]*t1.ne[2];
        td.nb[3] = td.nb[2]*td.ne[2];
        td.op = GGML_OP_GET_ROWS;
        td.src[0] = &t0; td.src[1] = &t1;

        ggml_backend_cuda_context ctx(dev);
        ggml_cuda_op_get_rows(ctx, &td);
        check(cudaDeviceSynchronize() == cudaSuccess, (std::string("sync ")+c.name).c_str());

        if (getenv("FG176G_VERBOSE")) {
            std::fprintf(stderr, "SETUP %s src0.ne={%lld,%lld,%lld,%lld} nb1=%zu nb2=%zu nb3=%zu\n",
                c.name,(long long)t0.ne[0],(long long)t0.ne[1],(long long)t0.ne[2],(long long)t0.ne[3],
                t0.nb[1],t0.nb[2],t0.nb[3]);
            std::fprintf(stderr, "SETUP %s src1.ne={%lld,%lld,%lld,%lld} nb1=%zu nb2=%zu\n",
                c.name,(long long)t1.ne[0],(long long)t1.ne[1],(long long)t1.ne[2],(long long)t1.ne[3],
                t1.nb[1],t1.nb[2]);
            std::fprintf(stderr, "SETUP %s dst.ne={%lld,%lld,%lld,%lld} nb1=%zu nb2=%zu nb3=%zu\n",
                c.name,(long long)td.ne[0],(long long)td.ne[1],(long long)td.ne[2],(long long)td.ne[3],
                td.nb[1],td.nb[2],td.nb[3]);
            std::fprintf(stderr, "SETUP %s idx[0..7]=", c.name);
            for (int q = 0; q < 8 && q < ns1; ++q) std::fprintf(stderr, "%d ", idx[q]);
            std::fprintf(stderr, "\n");
        }
        std::vector<float> got(nd);
        if (const char * dir = getenv("FG176G_DUMP")) {
            std::string fn = std::string(dir) + "/" + c.name + ".bin";
            if (FILE * f = fopen(fn.c_str(), "wb")) { fwrite(got.data(), 1, 0, f); fclose(f); }
        }
        cudaMemcpy(got.data(), ddp, nd*sizeof(float), cudaMemcpyDeviceToHost);
        if (const char * dir = getenv("FG176G_DUMP")) {
            std::string fn = std::string(dir) + "/" + c.name + ".bin";
            if (FILE * f = fopen(fn.c_str(), "wb")) { fwrite(got.data(), 1, got.size()*sizeof(float), f); fclose(f); }
        }
        int64_t bad = 0;
        for (int64_t i = 0; i < nd; ++i) if (std::memcmp(&got[i], &ref[i], sizeof(float)) != 0) ++bad;
        if (bad) {
            int64_t first = -1;
            for (int64_t i = 0; i < nd; ++i) if (std::memcmp(&got[i], &ref[i], sizeof(float)) != 0) { first = i; break; }
            std::fprintf(stderr, "DIAG %s first_bad=%lld got=%f want=%f\n", c.name,
                         (long long)first, got[first], ref[first]);
        }
        check(bad == 0, (std::string("bytes ")+c.name).c_str());
        std::vector<float> back0(ns0);
        cudaMemcpy(back0.data(), d0p, ns0*sizeof(float), cudaMemcpyDeviceToHost);
        check(std::memcmp(back0.data(), h0.data(), ns0*sizeof(float)) == 0, (std::string("src0 preserved ")+c.name).c_str());
        cudaFree(d0p); cudaFree(d1p); cudaFree(ddp);
    }
    std::printf("FG176G_GPU_FIXTURE checks=%d failures=%d cases=%zu\n",
                g_checks, g_fail, sizeof(CASES)/sizeof(CASES[0]));
    return g_fail ? 1 : 0;
}
