#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls, g_bytes;
__attribute__((destructor)) static void fg217c_report(void){
 if(!atomic_load(&g_calls))return;
 fprintf(stderr,"FG217C_UNICODE_CPTS calls=%llu bytes=%llu\n",
   atomic_load(&g_calls),atomic_load(&g_bytes));}
static void g_tick(void) { const unsigned long long n = atomic_fetch_add(&g_calls, 1) + 1;
    if (n == 1) fprintf(stderr, "FG217C_UNICODE_CPTS first call calls=1\n");
    if ((n % 1000) == 0) fprintf(stderr, "FG217C_UNICODE_CPTS calls=%llu\n", n); }
static void g_fail(int64_t c){fprintf(stderr,"FG217C validation code %lld\n",(long long)c);abort();}
static int64_t g_ld_u8(uint64_t d,int64_t b){return (int64_t)*(const uint8_t*)(uintptr_t)(d+(uint64_t)b);}
static void g_st_i32(uint64_t d,int64_t i,int64_t v){((int32_t*)(uintptr_t)d)[i]=(int32_t)v;}
static int64_t g_ld_i32(uint64_t d,int64_t i){return (int64_t)((const int32_t*)(uintptr_t)d)[i];}
#define fg217c_ld_u8 g_ld_u8
#define fg217c_st_i32 g_st_i32
#define fg217c_ld_i32 g_ld_i32
#define fg217c_fail  g_fail
#define gg_init_globals fg217c_init_globals
#define main fg217c_generated_main
#include "cpts-generated.c"
#undef main
int64_t fg217c_cpts_from_utf8(const char*utf8,int64_t size,int32_t*out){ g_tick();
 // the scratch slot holds the consumed byte count for one decode step; it is per-call state, so
 // it lives on this stack frame rather than being shared across threads
 int32_t scratch[1]={0};
 atomic_fetch_add(&g_calls,1);
 atomic_fetch_add(&g_bytes,(unsigned long long)(size>0?size:0));
 return gg_function_1((uint64_t)(uintptr_t)utf8,size,(uint64_t)(uintptr_t)out,
                      (uint64_t)(uintptr_t)scratch,0);
}
int fg217c_enabled(void){const char*f=getenv("GGML_GOOSE_UNICODE_CPTS");return !(f&&strcmp(f,"0")==0);}
