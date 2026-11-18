#include <stdint.h>
#include <stdio.h>
#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
int64_t fg161_bulk(int64_t,int64_t,int64_t); size_t fg161_buffer_size(int64_t); void fg161_free_buffer(int64_t);
static void case_run(const char * name, int64_t n0, int64_t n1, int64_t rows) {
    struct ggml_init_params ip = { 16u<<20, NULL, true };
    struct ggml_context * c = ggml_init(ip);
    struct ggml_tensor * a = ggml_new_tensor_2d(c, GGML_TYPE_F32, n0, rows);
    struct ggml_tensor * b = ggml_new_tensor_2d(c, GGML_TYPE_F32, n1, rows);
    ggml_backend_buffer_type_t bt = ggml_backend_cpu_buffer_type();
    size_t native_need = ggml_backend_alloc_ctx_tensors_from_buft_size(c, bt);
    int64_t h = fg161_bulk((int64_t)(uintptr_t) bt, (int64_t)(uintptr_t) c, 0);
    printf("%s native_need=%zu goose_size=%zu a_off=%td b_off=%td\n", name, native_need,
           fg161_buffer_size(h), (char*)a->data - (char*)ggml_backend_buffer_get_base(a->buffer),
           (char*)b->data - (char*)ggml_backend_buffer_get_base(b->buffer));
    fg161_free_buffer(h); ggml_free(c);
}
int main(void){
    ggml_backend_t cpu = ggml_backend_cpu_init();
    case_run("200x3+64x3", 200, 64, 3);
    case_run("256x8+128x8", 256, 128, 8);
    case_run("200x3+64x3", 200, 64, 3);
    ggml_backend_free(cpu); return 0;
}
