// P4-13: record the native allocator's observable behaviour (the reference the
// Goose adapter must reproduce bit-for-bit in the differential gate).
#include <cstdio>
#include <cstdint>
#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
int main(){
    ggml_init_params ip{64u*1024*1024, nullptr, true};
    ggml_context * c = ggml_init(ip);
    ggml_tensor * a = ggml_new_tensor_2d(c, GGML_TYPE_F32, 256, 8);
    ggml_tensor * b = ggml_new_tensor_2d(c, GGML_TYPE_F32, 128, 8);
    ggml_backend_t cpu = ggml_backend_cpu_init();
    ggml_backend_buffer_type_t buft = ggml_backend_cpu_buffer_type();
    size_t need = ggml_backend_alloc_ctx_tensors_from_buft_size(c, buft);
    ggml_backend_buffer_t buf = ggml_backend_alloc_ctx_tensors_from_buft(c, buft);
    printf("buft=%s need=%zu buf=%s size=%zu\n", ggml_backend_buft_name(buft), need,
           buf ? ggml_backend_buffer_name(buf) : "null", buf ? ggml_backend_buffer_get_size(buf) : 0);
    printf("a.data=%td b.data=%td base=%p\n",
           (char*)a->data - (char*)ggml_backend_buffer_get_base(buf),
           (char*)b->data - (char*)ggml_backend_buffer_get_base(buf), ggml_backend_buffer_get_base(buf));
    printf("alignment=%zu buft_align=%zu\n",
           (size_t) ggml_backend_buffer_get_alignment(buf), (size_t) ggml_backend_buft_get_alignment(buft));
    ggml_backend_buffer_reset(buf);
    printf("after_reset base=%p size=%zu\n", ggml_backend_buffer_get_base(buf), ggml_backend_buffer_get_size(buf));
    ggml_backend_buffer_free(buf);
    printf("freed ok\n");
    ggml_backend_free(cpu); ggml_free(c);
    return 0;
}
