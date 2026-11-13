// Typed GPU differential fixture for the Goose CONCAT replacement.
//
// The float fixture (gpu-fixture.cu) covers the route/branch matrix. This one
// pins the copy width each element type selects, because the Goose body is
// width-parameterised and the native entry picks the width from the type:
//   1 byte  -> I8/U8 here; the quantized byte view is covered separately by
//              gpu-quant-fixture.cu, which indexes whole rows exactly as the
//              native code does (row bytes = ne[0]*type_size/block_size).
//   2 bytes -> F16, BF16, I16
//   4 bytes -> F32, I32
//   8 bytes -> F64, I64
// Every case compares against a byte-level host reference built from the same
// raw input bytes, so a width or stride mistake shows as a byte mismatch.
#include "common.cuh"
void ggml_cuda_op_concat(ggml_backend_cuda_context & ctx, ggml_tensor * dst);
#include <cstdio>
#include <cstring>
#include <cuda_runtime.h>
#include <string>
#include <vector>
#include <random>

static int g_checks, g_fail;
static void check(bool ok, const char * what) {
    ++g_checks;
    if (!ok) { ++g_fail; std::fprintf(stderr, "FAIL %s\n", what); }
}

struct TCase {
    ggml_type type;
    int64_t nea[4], neb[4], neo[4];
    int dim;
    const char * name;
};

static const TCase TCASES[] = {
    {GGML_TYPE_I32,  {8, 4, 2, 1},  {8, 4, 2, 1},  {16, 4, 2, 1}, 0, "i32-dim0-nc"},
    {GGML_TYPE_I32,  {5, 6, 2, 1},  {5, 3, 2, 1},  {5, 9, 2, 1},  1, "i32-dim1-nc"},
    {GGML_TYPE_I16,  {6, 3, 2, 2},  {6, 3, 2, 2},  {12, 3, 2, 2}, 0, "i16-dim0-cont"},
    {GGML_TYPE_F16,  {5, 4, 3, 1},  {5, 4, 3, 1},  {5, 8, 3, 1},  1, "f16-dim1-cont"},
    {GGML_TYPE_F64,  {4, 3, 2, 1},  {4, 3, 2, 1},  {8, 3, 2, 1},  0, "f64-dim0-cont"},
    {GGML_TYPE_I8,   {7, 5, 2, 1},  {7, 5, 2, 1},  {14, 5, 2, 1}, 0, "i8-dim0-cont"},
    {GGML_TYPE_F16,  {4, 3, 2, 2},  {4, 3, 2, 3},  {4, 3, 2, 5},  3, "f16-dim3-memcpy"},
};

int main(int argc, char ** argv) {
    (void) argc; (void) argv;
    int dev = 0;
    if (ggml_backend_cuda_get_device_count() < 1) { std::fprintf(stderr, "no CUDA device\n"); return 77; }
    cudaSetDevice(dev);
    std::mt19937 rng(777001);

    for (const TCase & c : TCASES) {
        const size_t es = ggml_type_size(c.type);
        const int64_t na = c.nea[0]*c.nea[1]*c.nea[2]*c.nea[3];
        const int64_t nb = c.neb[0]*c.neb[1]*c.neb[2]*c.neb[3];
        const int64_t no = c.neo[0]*c.neo[1]*c.neo[2]*c.neo[3];
        std::vector<uint8_t> ba(na*es), bb(nb*es), ref(no*es);
        for (auto & v : ba) v = (uint8_t)rng();
        for (auto & v : bb) v = (uint8_t)rng();
        std::memset(ref.data(), 0xCD, ref.size());

        // Reference over whole elements of es bytes, using the same index map as
        // the float fixture but in element units.
        for (int64_t i3 = 0; i3 < c.neo[3]; ++i3)
        for (int64_t i2 = 0; i2 < c.neo[2]; ++i2)
        for (int64_t i1 = 0; i1 < c.neo[1]; ++i1)
        for (int64_t i0 = 0; i0 < c.neo[0]; ++i0) {
            bool a = (c.dim == 0 ? i0 < c.nea[0] : c.dim == 1 ? i1 < c.nea[1] : c.dim == 2 ? i2 < c.nea[2] : i3 < c.nea[3]);
            int64_t src;
            if (a) src = ((i3*c.nea[2] + i2)*c.nea[1] + i1)*c.nea[0] + i0;
            else src = (((i3 - (c.dim==3?c.nea[3]:0))*c.neb[2] + (i2 - (c.dim==2?c.nea[2]:0)))*c.neb[1]
                        + (i1 - (c.dim==1?c.nea[1]:0)))*c.neb[0] + (i0 - (c.dim==0?c.nea[0]:0));
            int64_t dst = ((i3*c.neo[2] + i2)*c.neo[1] + i1)*c.neo[0] + i0;
            std::memcpy(ref.data() + dst*es, (a ? ba : bb).data() + src*es, es);
        }

        void * da = nullptr, * db = nullptr, * dd = nullptr;
        cudaMalloc(&da, ba.size()); cudaMalloc(&db, bb.size()); cudaMalloc(&dd, ref.size());
        cudaMemcpy(da, ba.data(), ba.size(), cudaMemcpyHostToDevice);
        cudaMemcpy(db, bb.data(), bb.size(), cudaMemcpyHostToDevice);
        cudaMemset(dd, 0xCD, ref.size());

        ggml_tensor ta{}, tb{}, td{};
        for (int i = 0; i < 4; ++i) { ta.ne[i]=c.nea[i]; tb.ne[i]=c.neb[i]; td.ne[i]=c.neo[i]; }
        ta.type = tb.type = td.type = c.type;
        ta.data = da; tb.data = db; td.data = dd;
        const size_t blck=(size_t)ggml_blck_size(c.type);
        const size_t rowb=(size_t)ggml_row_size(c.type,c.neb[0]);
        const size_t rowa=(size_t)ggml_row_size(c.type,c.nea[0]);
        const size_t rowo=(size_t)ggml_row_size(c.type,c.neo[0]);
        ta.nb[0]=es; tb.nb[0]=es; td.nb[0]=es;
        ta.nb[1]=rowa; tb.nb[1]=rowb; td.nb[1]=rowo;
        (void)blck;
        ta.nb[2]=(size_t)ta.nb[1]*ta.ne[1]; tb.nb[2]=(size_t)tb.nb[1]*tb.ne[1]; td.nb[2]=(size_t)td.nb[1]*td.ne[1];
        ta.nb[3]=(size_t)ta.nb[2]*ta.ne[2]; tb.nb[3]=(size_t)tb.nb[2]*tb.ne[2]; td.nb[3]=(size_t)td.nb[2]*td.ne[2];
        td.op = GGML_OP_CONCAT;
        td.src[0] = &ta; td.src[1] = &tb;
        ((int32_t *) td.op_params)[0] = c.dim;

        ggml_backend_cuda_context ctx(dev);
        ggml_cuda_op_concat(ctx, &td);
        check(cudaDeviceSynchronize() == cudaSuccess, (std::string("sync ")+c.name).c_str());

        std::vector<uint8_t> got(ref.size());
        cudaMemcpy(got.data(), dd, ref.size(), cudaMemcpyDeviceToHost);
        check(std::memcmp(got.data(), ref.data(), ref.size()) == 0, (std::string("bytes ")+c.name).c_str());
        std::vector<uint8_t> ra(ba.size()), rb(bb.size());
        cudaMemcpy(ra.data(), da, ba.size(), cudaMemcpyDeviceToHost);
        cudaMemcpy(rb.data(), db, bb.size(), cudaMemcpyDeviceToHost);
        check(std::memcmp(ra.data(), ba.data(), ba.size()) == 0, (std::string("src0 preserved ")+c.name).c_str());
        check(std::memcmp(rb.data(), bb.data(), bb.size()) == 0, (std::string("src1 preserved ")+c.name).c_str());
        cudaFree(da); cudaFree(db); cudaFree(dd);
    }
    std::printf("FG175C_TYPED_FIXTURE checks=%d failures=%d cases=%zu\n",
                g_checks, g_fail, sizeof(TCASES)/sizeof(TCASES[0]));
    return g_fail ? 1 : 0;
}
