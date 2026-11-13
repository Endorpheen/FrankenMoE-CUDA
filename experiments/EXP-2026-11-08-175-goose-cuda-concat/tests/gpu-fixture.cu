// GPU differential fixture for the Goose CONCAT replacement.
//
// Contract: the candidate DSO exports ggml_cuda_op_concat, which either runs the
// Goose route or, with GGML_GOOSE_CUDA_CONCAT=0, the private-renamed native one.
// This fixture allocates real device tensors, builds a real ggml_tensor graph
// node for GGML_OP_CONCAT, calls the public entry, and compares the destination
// against an independent host reference computed from the same inputs.
//
// It deliberately does not use the ggml scheduler, so no graph allocator, no
// CUDA Graph capture and no other Goose module participates. That isolates the
// operator exactly as the native/candidate switch does.
#include "common.cuh"
// Public CUDA entry under test; declared here because concat.cuh pulls the
// native kernel templates this fixture intentionally does not compile.
void ggml_cuda_op_concat(ggml_backend_cuda_context & ctx, ggml_tensor * dst);
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>
#include <string>

static int g_checks, g_fail;
static void check(bool ok, const char * what) {
    ++g_checks;
    if (!ok) { ++g_fail; std::fprintf(stderr, "FAIL %s\n", what); }
}

// Host reference: flattened dense concat along `dim`, matching ggml semantics.
template <typename T>
static void ref_concat(const T * a, const int64_t * nea, const T * b, const int64_t * neb,
                       T * out, const int64_t * neo, int dim) {
    int64_t total = neo[0] * neo[1] * neo[2] * neo[3];
    for (int64_t i3 = 0; i3 < neo[3]; ++i3)
    for (int64_t i2 = 0; i2 < neo[2]; ++i2)
    for (int64_t i1 = 0; i1 < neo[1]; ++i1)
    for (int64_t i0 = 0; i0 < neo[0]; ++i0) {
        bool from_a = (dim == 0 ? i0 < nea[0] : dim == 1 ? i1 < nea[1] : dim == 2 ? i2 < nea[2] : i3 < nea[3]);
        int64_t so = from_a
            ? (((i3 * nea[2] + i2) * nea[1] + i1) * nea[0] + i0)
            : ((((i3 - (dim == 3 ? nea[3] : 0)) * neb[2] + (i2 - (dim == 2 ? nea[2] : 0))) * neb[1]
                 + (i1 - (dim == 1 ? nea[1] : 0))) * neb[0] + (i0 - (dim == 0 ? nea[0] : 0)));
        int64_t doff = ((i3 * neo[2] + i2) * neo[1] + i1) * neo[0] + i0;
        out[doff] = from_a ? a[so] : b[so];
    }
    (void) total;
}

struct Case {
    int64_t nea[4], neb[4], neo[4];
    int dim;
    const char * name;
};

static const Case CASES[] = {
    // contiguous-to-3 operands along dim 0 -> Goose route 1 (one launch per i3)
    {{16, 4, 2, 2}, {16, 4, 2, 2}, {32, 4, 2, 2}, 0, "dim0-cont32"},
    {{8, 3, 2, 1},  {4, 3, 2, 1},  {12, 3, 2, 1}, 0, "dim0-cont-mixed"},
    {{6, 5, 1, 1},  {6, 7, 1, 1},  {6, 12, 1, 1}, 1, "dim1-cont"},
    {{5, 4, 3, 1},  {5, 4, 2, 1},  {5, 4, 5, 1},  2, "dim2-cont"},
    // dim 3 contiguous -> Goose route 2 (two cudaMemcpyAsync, transport only)
    {{4, 3, 2, 2},  {4, 3, 2, 3},  {4, 3, 2, 5},  3, "dim3-cont-memcpy"},
    // non-contiguous shapes -> Goose route 0 (concat_non_cont)
    {{5, 7, 2, 3},  {5, 4, 2, 3},  {5, 11, 2, 3}, 1, "dim1-noncont"},
    {{9, 3, 4, 2},  {7, 3, 4, 2},  {16, 3, 4, 2}, 0, "dim0-noncont"},
    {{3, 3, 5, 2},  {3, 3, 2, 2},  {3, 3, 7, 2},  2, "dim2-noncont"},
};

int main(int argc, char ** argv) {
    (void) argc; (void) argv;
    int dev = 0;
    if (ggml_backend_cuda_get_device_count() < 1) { std::fprintf(stderr, "no CUDA device\n"); return 77; }
    cudaSetDevice(dev);
    std::mt19937 rng(20261108);

    for (const Case & c : CASES) {
        const int64_t na = c.nea[0]*c.nea[1]*c.nea[2]*c.nea[3];
        const int64_t nb = c.neb[0]*c.neb[1]*c.neb[2]*c.neb[3];
        const int64_t no = c.neo[0]*c.neo[1]*c.neo[2]*c.neo[3];
        std::vector<float> ha(na), hb(nb), href(no, -12345.0f);
        std::uniform_real_distribution<float> dist(-4.0f, 4.0f);
        for (auto & v : ha) v = dist(rng);
        for (auto & v : hb) v = dist(rng);
        ref_concat(ha.data(), c.nea, hb.data(), c.neb, href.data(), c.neo, c.dim);

        float * da = nullptr, * db = nullptr, * dd = nullptr;
        cudaMalloc(&da, na*sizeof(float)); cudaMalloc(&db, nb*sizeof(float)); cudaMalloc(&dd, no*sizeof(float));
        cudaMemcpy(da, ha.data(), na*sizeof(float), cudaMemcpyHostToDevice);
        cudaMemcpy(db, hb.data(), nb*sizeof(float), cudaMemcpyHostToDevice);
        cudaMemset(dd, 0xFF, no*sizeof(float));

        ggml_tensor ta{}, tb{}, td{};
        for (int i = 0; i < 4; ++i) { ta.ne[i] = c.nea[i]; tb.ne[i] = c.neb[i]; td.ne[i] = c.neo[i]; }
        ta.type = tb.type = td.type = GGML_TYPE_F32;
        ta.data = da; tb.data = db; td.data = dd;
        size_t es = sizeof(float);
        ta.nb[0]=es; tb.nb[0]=es; td.nb[0]=es;
        ta.nb[1]=ta.nb[0]*ta.ne[0]; tb.nb[1]=tb.nb[0]*tb.ne[0]; td.nb[1]=td.nb[0]*td.ne[0];
        ta.nb[2]=ta.nb[1]*ta.ne[1]; tb.nb[2]=tb.nb[1]*tb.ne[1]; td.nb[2]=td.nb[1]*td.ne[1];
        ta.nb[3]=ta.nb[2]*ta.ne[2]; tb.nb[3]=tb.nb[2]*tb.ne[2]; td.nb[3]=td.nb[2]*td.ne[2];
        td.op = GGML_OP_CONCAT;
        td.src[0] = &ta; td.src[1] = &tb;
        ((int32_t *) td.op_params)[0] = c.dim;

        ggml_backend_cuda_context ctx(dev);
        ggml_cuda_op_concat(ctx, &td);
        cudaError_t e = cudaDeviceSynchronize();
        check(e == cudaSuccess, (std::string("sync ")+c.name).c_str());

        std::vector<float> got(no);
        cudaMemcpy(got.data(), dd, no*sizeof(float), cudaMemcpyDeviceToHost);
        int64_t bad = 0;
        for (int64_t i = 0; i < no; ++i) if (std::memcmp(&got[i], &href[i], sizeof(float)) != 0) ++bad;
        check(bad == 0, (std::string("bytes ")+c.name).c_str());
        // input preservation
        std::vector<float> ra(na), rb(nb);
        cudaMemcpy(ra.data(), da, na*sizeof(float), cudaMemcpyDeviceToHost);
        cudaMemcpy(rb.data(), db, nb*sizeof(float), cudaMemcpyDeviceToHost);
        check(std::memcmp(ra.data(), ha.data(), na*sizeof(float)) == 0, (std::string("src0 preserved ")+c.name).c_str());
        check(std::memcmp(rb.data(), hb.data(), nb*sizeof(float)) == 0, (std::string("src1 preserved ")+c.name).c_str());
        cudaFree(da); cudaFree(db); cudaFree(dd);
    }
    std::printf("FG175C_GPU_FIXTURE checks=%d failures=%d cases=%zu\n", g_checks, g_fail, sizeof(CASES)/sizeof(CASES[0]));
    return g_fail ? 1 : 0;
}
