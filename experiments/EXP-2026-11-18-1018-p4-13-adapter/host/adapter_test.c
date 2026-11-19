#include <stdint.h>
#include <stdio.h>
#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
int64_t fg_adapter_dispatch(int64_t,int64_t,int64_t,int64_t,int64_t);
uint64_t fg161_generation(int64_t); int fg161_live_count(void); void * fg161_buffer_base(int64_t);
int main(void){
    ggml_backend_t cpu = ggml_backend_cpu_init();
    int64_t buft = (int64_t)(uintptr_t) ggml_backend_cpu_buffer_type();
    int64_t h = fg_adapter_dispatch(0, buft, 12288, 0, 0);
    printf("alloc=%lld live=%d gen=%llu\n",(long long)h,fg161_live_count(),(unsigned long long)fg161_generation(h));
    struct ggml_init_params ip = { 1u<<20, NULL, true }; struct ggml_context *c = ggml_init(ip);
    struct ggml_tensor *t=ggml_new_tensor_1d(c,GGML_TYPE_F32,1024);
    int64_t ok  = fg_adapter_dispatch(3, h, (int64_t)(uintptr_t)t, (int64_t)(uintptr_t)fg161_buffer_base(h), 0);
    int64_t rs  = fg_adapter_dispatch(2, h, 0, 0, 0);
    int64_t stale = fg_adapter_dispatch(3, h, (int64_t)(uintptr_t)t, (int64_t)(uintptr_t)fg161_buffer_base(h), 0);
    int64_t rel = fg_adapter_dispatch(1, h, 0, 0, 0);
    printf("place_fresh=%lld reset=%lld place_stale=%lld release=%lld live=%d gen=%llu\n",
           (long long)ok,(long long)rs,(long long)stale,(long long)rel,fg161_live_count(),
           (unsigned long long)fg161_generation(h));
    ggml_free(c); ggml_backend_free(cpu); return 0;
}
