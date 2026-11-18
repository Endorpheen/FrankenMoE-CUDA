#include <stdint.h>
#include <stdio.h>
#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
int64_t fg_adapter_allocate(int64_t,uint64_t); int64_t fg_adapter_release(int64_t); int64_t fg_adapter_reset(int64_t);
uint64_t fg161_generation(int64_t); int fg161_live_count(void);
int main(void){
    ggml_backend_t cpu = ggml_backend_cpu_init();
    int64_t buft = (int64_t)(uintptr_t) ggml_backend_cpu_buffer_type();
    int64_t h = fg_adapter_allocate(buft, 12288);
    printf("goose_alloc=%lld live=%d gen=%llu\n",(long long)h,fg161_live_count(),(unsigned long long)fg161_generation(h));
    int64_t g = fg_adapter_reset(h);
    printf("goose_reset_ret=%lld gen=%llu\n",(long long)g,(unsigned long long)fg161_generation(h));
    int64_t r = fg_adapter_release(h);
    printf("goose_release_ret=%lld live=%d\n",(long long)r,fg161_live_count());
    ggml_backend_free(cpu); return 0;
}
