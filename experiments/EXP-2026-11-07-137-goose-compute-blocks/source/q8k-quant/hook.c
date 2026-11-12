#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <stdatomic.h>
extern void fg_selfhost_quant_q8k(const float *,void *,int64_t);
typedef void (*quant_fn)(const float *,void *,int64_t);
static quant_fn native_quant;
static int enabled;
#ifdef FG_Q8K_TRACE
static atomic_uint_fast64_t calls,blocks,checked;
static _Thread_local uint64_t local_calls;
#endif
__attribute__((constructor)) static void configure(void) {
 native_quant=(quant_fn)dlsym(RTLD_NEXT,"quantize_row_q8_K");
 const char *value=getenv("GGML_GOOSE_Q8K");enabled=!value||strcmp(value,"0");
 if(!native_quant){fprintf(stderr,"Goose Q8_K native symbol missing\n");abort();}
}
void quantize_row_q8_K(const float *x,void *y,int64_t n) {
 if(!enabled||n<0||n%256){native_quant(x,y,n);return;}
#ifdef FG_Q8K_TRACE
 void *shadow=NULL;size_t bytes=(size_t)(n/256)*292;
 if(n&&((local_calls++&4095u)==0)){shadow=malloc(bytes);if(!shadow)abort();memcpy(shadow,y,bytes);native_quant(x,shadow,n);}
#endif
 fg_selfhost_quant_q8k(x,y,n);
#ifdef FG_Q8K_TRACE
 atomic_fetch_add_explicit(&calls,1,memory_order_relaxed);atomic_fetch_add_explicit(&blocks,n/256,memory_order_relaxed);
 if(shadow){if(memcmp(shadow,y,bytes)){fprintf(stderr,"Goose Q8_K shadow mismatch\n");abort();}free(shadow);atomic_fetch_add_explicit(&checked,1,memory_order_relaxed);}
#endif
}
#ifdef FG_Q8K_TRACE
__attribute__((destructor)) static void report(void) {
 fprintf(stderr,"FG_SELFHOST_Q8K calls=%llu blocks=%llu shadow_checks=%llu enabled=%d\n",(unsigned long long)atomic_load(&calls),(unsigned long long)atomic_load(&blocks),(unsigned long long)atomic_load(&checked),enabled);
}
#endif
