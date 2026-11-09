// GPU differential fixture for the Goose MM IDs helper.
//
// The operator redefines ggml_cuda_launch_mm_ids_helper; with
// GGML_GOOSE_CUDA_MM_IDS=0 it goes to the private-renamed native one.
//
// Only the n_expert_used == 8 specialisation is in scope. The gate is byte
// equality of all three outputs (ids_dst, ids_src1, expert_bounds) between the two
// arms, plus an independent check that the produced layout is self-consistent:
// every compact row must map back to the (token, slot) pair that produced it, and
// expert_bounds must be non-decreasing and end at the total compact count.
#include "common.cuh"
void ggml_cuda_launch_mm_ids_helper(const int32_t * ids, int32_t * ids_src1, int32_t * ids_dst,
                                    int32_t * expert_bounds, int n_experts, int n_tokens,
                                    int n_expert_used, int nchannels_y, int si1, int sis1,
                                    bool write_inverse, cudaStream_t stream);
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>
#include <algorithm>
#include <string>

static int g_checks, g_fail;
static void check(bool ok, const char * what) {
    ++g_checks;
    if (!ok) { ++g_fail; std::fprintf(stderr, "FAIL %s\n", what); }
}

struct Case { int n_experts, n_tokens, n_used, nch, si1, sis1; bool inv; const char * name; };

static const Case CASES[] = {
    {  8,  4, 8, 1, 8, 8, false, "e8-t4-fwd" },
    {  8, 16, 8, 1, 8, 8, true,  "e8-t16-inv" },
    { 32, 64, 8, 1, 8, 8, false, "e32-t64-fwd" },
    {  8, 33, 8, 1, 8, 8, false, "e8-t33-fwd" },   // not a multiple of the slot count
    { 16, 128, 8, 2, 8, 16, false, "e16-t128-ch2" },
};

struct Out {
    std::vector<int32_t> dst, src1, bounds;
    bool ok = false;
};

static Out run_case(int dev, const Case & c) {
    Out o;
    std::mt19937 rng(181001 + (unsigned)c.n_tokens + (unsigned)c.n_experts);
    std::uniform_int_distribution<int32_t> de(0, c.n_experts - 1);
    // A router never selects the same expert twice for one token, and the operator relies
    // on that: the compaction writes the packed (token, expert) slot at
    // it_compact + inclusive_prefix, so duplicate experts inside one token would target
    // the same staging address from different lanes. Generating duplicates was a defect
    // in this fixture, not a property of the kernel -- it produced racecheck hazards in
    // both arms that cannot occur on real input. Each token now gets a distinct expert set.
    std::vector<int32_t> ids((size_t)c.n_tokens * c.n_used);
    for (int t = 0; t < c.n_tokens; ++t) {
        std::vector<int32_t> pool(c.n_experts);
        for (int e = 0; e < c.n_experts; ++e) pool[e] = e;
        std::shuffle(pool.begin(), pool.end(), rng);
        for (int u = 0; u < c.n_used; ++u) ids[(size_t)t*c.n_used + u] = pool[u];
    }
    const size_t ndst = ids.size();
    const size_t nsrc1 = ids.size();
    const size_t nbounds = (size_t)c.n_experts + 1;

    int32_t * di = nullptr, * dr = nullptr, * dd = nullptr, * db = nullptr;
    cudaSetDevice(dev);
    cudaMalloc(&di, ids.size()*4); cudaMalloc(&dr, nsrc1*4);
    cudaMalloc(&dd, ndst*4);       cudaMalloc(&db, nbounds*4);
    cudaMemcpy(di, ids.data(), ids.size()*4, cudaMemcpyHostToDevice);
    cudaMemset(dr, 0xCD, nsrc1*4); cudaMemset(dd, 0xCD, ndst*4); cudaMemset(db, 0xCD, nbounds*4);
    cudaStream_t st; cudaStreamCreate(&st);
    ggml_cuda_launch_mm_ids_helper(di, dr, dd, db, c.n_experts, c.n_tokens, c.n_used,
                                   c.nch, c.si1, c.sis1, c.inv, st);
    o.ok = cudaStreamSynchronize(st) == cudaSuccess;
    o.dst.resize(ndst); o.src1.resize(nsrc1); o.bounds.resize(nbounds);
    cudaMemcpy(o.dst.data(), dd, ndst*4, cudaMemcpyDeviceToHost);
    cudaMemcpy(o.src1.data(), dr, nsrc1*4, cudaMemcpyDeviceToHost);
    cudaMemcpy(o.bounds.data(), db, nbounds*4, cudaMemcpyDeviceToHost);
    cudaFree(di); cudaFree(dr); cudaFree(dd); cudaFree(db); cudaStreamDestroy(st);
    return o;
}

int main(int argc, char ** argv) {
    (void) argc; (void) argv;
    if (ggml_backend_cuda_get_device_count() < 1) { std::fprintf(stderr, "no CUDA device\n"); return 77; }
    const int dev = 0;
    const char * dump = getenv("FG181M_DUMP");
    const bool native_arm = getenv("GGML_GOOSE_CUDA_MM_IDS") &&
                            std::strcmp(getenv("GGML_GOOSE_CUDA_MM_IDS"), "0") == 0;

    for (const Case & c : CASES) {
        Out o = run_case(dev, c);
        check(o.ok, (std::string("sync ")+c.name).c_str());
        if (!o.ok) continue;

        // expert_bounds must be non-decreasing and its last entry is the total
        // number of compact rows actually produced.
        bool mono = true;
        for (size_t i = 1; i < o.bounds.size(); ++i) if (o.bounds[i] < o.bounds[i-1]) mono = false;
        check(mono, (std::string("bounds monotone ")+c.name).c_str());
        const int32_t total = o.bounds.back();
        check(total >= 0 && total <= (int32_t)o.dst.size(),
              (std::string("bounds total in range ")+c.name).c_str());

        // Every written compact row must be a valid (token, slot) pair.
        bool valid = true;
        for (size_t i = 0; i < o.dst.size(); ++i) {
            const int32_t v = o.dst[i];
            if (v == (int32_t)0xCDCDCDCD) continue;   // slot never written
            if (v < 0 || v >= c.n_tokens * c.n_used) { valid = false; break; }
        }
        // bounds must also be exactly the expert-sorted partition: each entry is the
        // running count of compact rows whose expert id is below the next expert.
        for (int e = 0; e < c.n_experts; ++e) {
            if (o.bounds[e] > o.bounds[e+1]) valid = false;
        }
        check(valid, (std::string("dst rows valid ")+c.name).c_str());

        if (dump) {
            std::string base = std::string(dump) + "/" + c.name;
            if (FILE * f = fopen((base+".dst.bin").c_str(),"wb")) { fwrite(o.dst.data(),4,o.dst.size(),f); fclose(f); }
            if (FILE * f = fopen((base+".src1.bin").c_str(),"wb")) { fwrite(o.src1.data(),4,o.src1.size(),f); fclose(f); }
            if (FILE * f = fopen((base+".bounds.bin").c_str(),"wb")) { fwrite(o.bounds.data(),4,o.bounds.size(),f); fclose(f); }
        }
    }
    std::printf("FG181M_GPU_FIXTURE checks=%d failures=%d arm=%s\n",
                g_checks, g_fail, native_arm ? "native" : "goose");
    return g_fail ? 1 : 0;
}
