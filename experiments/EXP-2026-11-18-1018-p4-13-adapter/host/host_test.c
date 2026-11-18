#include <stdint.h>
#include <stdio.h>
#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
int64_t fg161_alloc_buffer(int64_t,uint64_t); void fg161_free_buffer(int64_t);
void fg161_reset_buffer(int64_t); void fg161_set_usage(int64_t,int64_t);
uint64_t fg161_generation(int64_t); int fg161_live_count(void);
int main(void){
    ggml_backend_t cpu = ggml_backend_cpu_init();
    int64_t buft = (int64_t)(uintptr_t) ggml_backend_cpu_buffer_type();
    int64_t h = fg161_alloc_buffer(buft, 12288);
    printf("alloc=%lld gen=%llu live=%d\n",(long long)h,(unsigned long long)fg161_generation(h),fg161_live_count());
    fg161_set_usage(h, 64);
    fg161_reset_buffer(h);
    printf("after_reset gen=%llu\n",(unsigned long long)fg161_generation(h));
    fg161_free_buffer(h);
    printf("after_free live=%d gen_stale=%llu\n", fg161_live_count(), (unsigned long long)fg161_generation(h));
    int64_t h2 = fg161_alloc_buffer(buft, 3168);
    printf("realloc=%lld live=%d\n",(long long)h2, fg161_live_count());
    fg161_free_buffer(h2); ggml_backend_free(cpu); return 0;
}
