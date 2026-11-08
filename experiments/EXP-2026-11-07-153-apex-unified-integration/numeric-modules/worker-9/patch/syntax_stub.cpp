// Strict C++17 syntax check for the integration header. The real header is
// compiled inside ggml-cuda translation units; here it is compiled against
// minimal stand-ins for the ggml/CUDA declarations it consumes.
#include <cstdint>
#include <cstddef>
#include <cstring>

struct CUstream_st;
typedef struct CUstream_st * cudaStream_t;

enum ggml_type { GGML_TYPE_F32 = 0, GGML_TYPE_F16 = 1, GGML_TYPE_I32 = 2 };

struct ggml_tensor {
    ggml_type type;
    void * data;
    int64_t ne[4];
    int64_t nb[4];
    int32_t op_params[16];
    ggml_tensor * src[4];
};

struct ggml_backend_cuda_context {
    cudaStream_t s;
    cudaStream_t stream() const { return s; }
};

static inline bool ggml_is_contiguous(const ggml_tensor *) { return true; }
static inline bool ggml_is_contiguous_1(const ggml_tensor *) { return true; }
static inline bool ggml_is_permuted(const ggml_tensor *) { return false; }
static inline int64_t ggml_nelements(const ggml_tensor *) { return 1; }
static inline int64_t ggml_nrows(const ggml_tensor *) { return 1; }
static inline size_t ggml_element_size(const ggml_tensor *) { return 4; }
static inline size_t ggml_type_size(ggml_type) { return 4; }

#define GGML_ROPE_TYPE_IMROPE 40

#include "apex-cuda-goose-ops.h"

int main() {
    ggml_backend_cuda_context ctx;
    ggml_tensor t{};
    unsigned sink = 0;
    sink += apex_cuda_goose::unary(ctx, &t, 0);
    sink += apex_cuda_goose::swiglu(ctx, &t);
    sink += apex_cuda_goose::bin_bcast(ctx, &t, 0);
    sink += apex_cuda_goose::norm(ctx, &t);
    sink += apex_cuda_goose::group_norm(ctx, &t);
    sink += apex_cuda_goose::rms_norm(ctx, &t);
    sink += apex_cuda_goose::l2_norm(ctx, &t);
    sink += apex_cuda_goose::soft_max(ctx, &t);
    sink += apex_cuda_goose::rope(ctx, &t);
    return (int) sink;
}
