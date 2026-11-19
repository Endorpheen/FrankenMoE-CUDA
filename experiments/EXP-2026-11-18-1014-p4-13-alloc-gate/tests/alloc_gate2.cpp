// P4-13 gate part 2: reuse, reset->reuse, rounding, and concurrent alloc/free.
// Only deterministic facts are printed; absolute addresses are compared, not printed.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>
#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
static ggml_context * mkctx(size_t bytes, ggml_tensor ** a, ggml_tensor ** b) {
    ggml_init_params ip{bytes, nullptr, true};
    ggml_context * c = ggml_init(ip);
    *a = ggml_new_tensor_2d(c, GGML_TYPE_F32, 200, 3);   // 2400 B, not 32-aligned
    *b = ggml_new_tensor_2d(c, GGML_TYPE_F32, 64, 3);    // 768 B
    return c;
}
int main(){
    ggml_backend_t cpu = ggml_backend_cpu_init();
    ggml_backend_buffer_type_t buft = ggml_backend_cpu_buffer_type();
    ggml_tensor *a,*b; ggml_context *c = mkctx(16u<<20,&a,&b);
    size_t need = ggml_backend_alloc_ctx_tensors_from_buft_size(c, buft);
    ggml_backend_buffer_t buf1 = ggml_backend_alloc_ctx_tensors_from_buft(c, buft);
    void * base1 = ggml_backend_buffer_get_base(buf1);
    printf("rounding need=%zu a_off=%td b_off=%td a_aligned=%d\n", need,
           (char*)a->data-(char*)base1, (char*)b->data-(char*)base1, ((uintptr_t)a->data%32)==0);
    ggml_backend_buffer_reset(buf1);
    printf("reset_keeps=%d\n", ggml_backend_buffer_get_base(buf1)==base1);
    ggml_backend_buffer_free(buf1);
    ggml_context *c2 = mkctx(16u<<20,&a,&b);
    ggml_backend_buffer_t buf2 = ggml_backend_alloc_ctx_tensors_from_buft(c2, buft);
    printf("reuse_same_size=%d\n", ggml_backend_buffer_get_size(buf2)==need);
    ggml_backend_buffer_free(buf2); ggml_free(c); ggml_free(c2);
    // concurrency: 4 threads, independent contexts and buffers
    std::vector<int> ok(4,0);
    std::vector<std::thread> th;
    for (int t=0;t<4;t++) th.emplace_back([&,t]{
        for (int i=0;i<200;i++){
            ggml_tensor *x,*y; ggml_context *cc = mkctx(4u<<20,&x,&y);
            ggml_backend_buffer_t bb = ggml_backend_alloc_ctx_tensors_from_buft(cc, ggml_backend_cpu_buffer_type());
            if (!bb) { ok[t]=-1; ggml_free(cc); return; }
            memset(x->data, 0, ggml_nbytes(x));
            if (ggml_backend_buffer_get_size(bb) >= ggml_nbytes(x)+ggml_nbytes(y)) ok[t]++;
            ggml_backend_buffer_free(bb); ggml_free(cc);
        }
    });
    for (auto &x: th) x.join();
    printf("threads_ok=%d,%d,%d,%d (expect 200 each)\n", ok[0],ok[1],ok[2],ok[3]);
    ggml_backend_free(cpu);
    return 0;
}
