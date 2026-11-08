// Quantized-type GPU differential fixture for the Goose CONCAT replacement.
//
// The native entry treats a quantized type as a byte tensor: the copy unit is
// one byte, ggml_row_size(type, ne[0]) is the row length, and ne[0] must be a
// multiple of the block size. This fixture therefore indexes bytes and derives
// every stride from ggml_row_size, which is exactly the contract the Goose host
// plan recomputes as row_units().
#include "common.cuh"
void ggml_cuda_op_concat(ggml_backend_cuda_context & ctx, ggml_tensor * dst);
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

struct QCase {
    ggml_type type;
    int64_t nea[4], neb[4], neo[4];
    int dim;
    const char * name;
};

static const QCase QCASES[] = {
    {GGML_TYPE_Q4_0, {32, 4, 1, 1},  {32, 4, 1, 1},  {64, 4, 1, 1},  0, "q4_0-dim0"},
    {GGML_TYPE_Q8_0, {32, 4, 2, 1},  {32, 4, 2, 1},  {32, 8, 2, 1},  1, "q8_0-dim1"},
    {GGML_TYPE_Q4_0, {32, 3, 2, 1},  {32, 3, 3, 1},  {32, 3, 5, 1},  2, "q4_0-dim2"},
    {GGML_TYPE_Q8_0, {32, 2, 2, 2},  {32, 2, 2, 3},  {32, 2, 2, 5},  3, "q8_0-dim3"},
};

int main(int argc, char ** argv) {
    (void) argc; (void) argv;
    int dev = 0;
    if (ggml_backend_cuda_get_device_count() < 1) { std::fprintf(stderr, "no CUDA device\n"); return 77; }
    cudaSetDevice(dev);
    std::mt19937 rng(424242);

    for (const QCase & c : QCASES) {
        const size_t rowa = ggml_row_size(c.type, c.nea[0]);
        const size_t rowb = ggml_row_size(c.type, c.neb[0]);
        const size_t rowo = ggml_row_size(c.type, c.neo[0]);
        const int64_t na = (int64_t)rowa*c.nea[1]*c.nea[2]*c.nea[3];
        const int64_t nb = (int64_t)rowb*c.neb[1]*c.neb[2]*c.neb[3];
        const int64_t no = (int64_t)rowo*c.neo[1]*c.neo[2]*c.neo[3];
        std::vector<uint8_t> ba(na), bb(nb), ref(no);
        for (auto & v : ba) v = (uint8_t)rng();
        for (auto & v : bb) v = (uint8_t)rng();
        std::memset(ref.data(), 0xCD, ref.size());

        // Byte-level reference over whole rows: row r of the destination takes
        // row r of src0 while the concatenation index is inside src0, else the
        // corresponding src1 row. A row is rowa/rowb/rowo bytes respectively.
        for (int64_t i3 = 0; i3 < c.neo[3]; ++i3)
        for (int64_t i2 = 0; i2 < c.neo[2]; ++i2)
        for (int64_t i1 = 0; i1 < c.neo[1]; ++i1) {
            bool a = (c.dim == 0 || (c.dim == 1 ? i1 < c.nea[1] : c.dim == 2 ? i2 < c.nea[2] : i3 < c.nea[3]));
            // dim 0 splits within a row, so it is handled bytewise below.
            if (c.dim == 0) continue;
            const uint8_t * src;
            size_t rown;
            if (a) {
                src = ba.data() + ((i3*c.nea[2] + i2)*c.nea[1] + i1)*rowa;
                rown = rowa;
            } else {
                const int64_t j3 = i3 - (c.dim == 3 ? c.nea[3] : 0);
                const int64_t j2 = i2 - (c.dim == 2 ? c.nea[2] : 0);
                const int64_t j1 = i1 - (c.dim == 1 ? c.nea[1] : 0);
                src = bb.data() + ((j3*c.neb[2] + j2)*c.neb[1] + j1)*rowb;
                rown = rowb;
            }
            std::memcpy(ref.data() + ((i3*c.neo[2] + i2)*c.neo[1] + i1)*rowo, src, rown);
        }
        if (c.dim == 0) {
            // dst row = src0 row ++ src1 row, byte concatenation of two rows.
            for (int64_t r = 0; r < c.neo[1]*c.neo[2]*c.neo[3]; ++r) {
                const int64_t a = c.nea[0]*(size_t)ggml_type_size(c.type)/(size_t)ggml_blck_size(c.type);
                const int64_t b = c.neb[0]*(size_t)ggml_type_size(c.type)/(size_t)ggml_blck_size(c.type);
                std::memcpy(ref.data() + r*rowo,       ba.data() + r*rowa, (size_t)a);
                std::memcpy(ref.data() + r*rowo + a,   bb.data() + r*rowb, (size_t)b);
            }
        }

        void * da = nullptr, * db = nullptr, * dd = nullptr;
        cudaMalloc(&da, ba.size()); cudaMalloc(&db, bb.size()); cudaMalloc(&dd, ref.size());
        cudaMemcpy(da, ba.data(), ba.size(), cudaMemcpyHostToDevice);
        cudaMemcpy(db, bb.data(), bb.size(), cudaMemcpyHostToDevice);
        cudaMemset(dd, 0xCD, ref.size());

        ggml_tensor ta{}, tb{}, td{};
        const size_t tsz = ggml_type_size(c.type);
        for (int i = 0; i < 4; ++i) { ta.ne[i]=c.nea[i]; tb.ne[i]=c.neb[i]; td.ne[i]=c.neo[i]; }
        ta.type = tb.type = td.type = c.type;
        ta.data = da; tb.data = db; td.data = dd;
        ta.nb[0]=tsz; tb.nb[0]=tsz; td.nb[0]=tsz;
        ta.nb[1]=rowa; tb.nb[1]=rowb; td.nb[1]=rowo;
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
        // Dump the actual destination bytes so Goose and native runs can be
        // compared byte-for-byte against each other, independently of the
        // hand-written reference below.
        if (const char * dir = getenv("FG175C_DUMP")) {
            std::string fn = std::string(dir) + "/" + c.name + ".bin";
            if (FILE * f = fopen(fn.c_str(), "wb")) { fwrite(got.data(), 1, got.size(), f); fclose(f); }
        }
        check(std::memcmp(got.data(), ref.data(), ref.size()) == 0, (std::string("bytes ")+c.name).c_str());
        std::vector<uint8_t> ra(ba.size()), rb(bb.size());
        cudaMemcpy(ra.data(), da, ba.size(), cudaMemcpyDeviceToHost);
        cudaMemcpy(rb.data(), db, bb.size(), cudaMemcpyDeviceToHost);
        check(std::memcmp(ra.data(), ba.data(), ba.size()) == 0, (std::string("src0 preserved ")+c.name).c_str());
        check(std::memcmp(rb.data(), bb.data(), bb.size()) == 0, (std::string("src1 preserved ")+c.name).c_str());
        cudaFree(da); cudaFree(db); cudaFree(dd);
    }
    std::printf("FG175C_QUANT_FIXTURE checks=%d failures=%d cases=%zu\n",
                g_checks, g_fail, sizeof(QCASES)/sizeof(QCASES[0]));
    return g_fail ? 1 : 0;
}
