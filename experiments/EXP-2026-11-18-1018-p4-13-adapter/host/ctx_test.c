#include <stdint.h>
#include <stdio.h>
#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
int64_t fg_adapter_dispatch(int64_t,int64_t,int64_t,int64_t,int64_t);
size_t fg161_buffer_size(int64_t); int fg161_live_count(void);
int main(void){
    ggml_backend_t cpu = ggml_backend_cpu_init();
    ggml_backend_buffer_type_t bt = ggml_backend_cpu_buffer_type();
    struct ggml_init_params ip = { 16u<<20, NULL, true };
    struct ggml_context * c = ggml_init(ip);
    struct ggml_tensor * a = ggml_new_tensor_2d(c, GGML_TYPE_F32, 256, 8);
    struct ggml_tensor * b = ggml_new_tensor_2d(c, GGML_TYPE_F32, 128, 8);
    size_t native = ggml_backend_alloc_ctx_tensors_from_buft_size(c, bt);
    int64_t h = fg_adapter_dispatch(4, (int64_t)(uintptr_t) bt, (int64_t)(uintptr_t) c, 0, 0);
    char * base = (char *) ggml_backend_buffer_get_base(a->buffer);
    printf("ctx: native=%zu goose=%zu a_off=%td b_off=%td live=%d\n", native, fg161_buffer_size(h),
           (char*)a->data-base, (char*)b->data-base, fg161_live_count());
    fg_adapter_dispatch(1, h, 0,0,0);
    printf("after_release live=%d\n", fg161_live_count());
    ggml_free(c); ggml_backend_free(cpu); return 0;
}
