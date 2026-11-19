#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
typedef void (*init_t)(int, char **);
typedef int64_t (*fn_t)(const void*, int64_t, const void*, int64_t, int64_t, void*, int64_t);
int main(void){
    void *h=dlopen("/home/end0/AI/FrankenMoE-CUDA/builds/flint-acc/bin/libllama.so.0",RTLD_LAZY);
    if(!h){printf("dlopen: %s\n",dlerror());return 2;}
    init_t init=(init_t)dlsym(h,"goose_init");
    printf("goose_init=%p\n",(void*)init);
    if(init) init(0,0);
    fn_t fn=(fn_t)dlsym(h,"fg_sp_prefix");
    int64_t draft[4]={5,6,7,8}, target[4]={5,6,9,9}, out[2]={-1,-1};
    printf("before\n"); fflush(stdout);
    int64_t r=fn(draft,4,target,4,4,out,2);
    printf("after: r=%lld out0=%lld\n",(long long)r,(long long)out[0]);
    return 0;
}
