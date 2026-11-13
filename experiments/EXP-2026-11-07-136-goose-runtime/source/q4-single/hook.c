#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#ifndef FG_Q4_TRACE
#define FG_Q4_TRACE 0
#endif
extern float fg_selfhost_q4dot(int,const void*,const void*);
typedef void (*native_dot)(int,float*,size_t,const void*,size_t,const void*,size_t,int);
static native_dot original_dot;
static int enabled=1;
#if FG_Q4_TRACE
static atomic_uint_fast64_t samples=0;
static atomic_uint_fast64_t fallbacks=0;
static _Thread_local uint64_t local_calls;
#endif
__attribute__((constructor)) static void configure(void) {
 original_dot=(native_dot)dlsym(RTLD_NEXT,"ggml_vec_dot_q4_K_q8_K");
 const char *v=getenv("GGML_GOOSE_Q4");enabled=!v||atoi(v)!=0;
 if(!original_dot){fprintf(stderr,"Goose Q4: original symbol unavailable\n");abort();}
}
void ggml_vec_dot_q4_K_q8_K(int n,float*s,size_t bs,const void*x,size_t bx,const void*y,size_t by,int nrc) {
 if(enabled&&n>0&&n%256==0&&nrc==1) {
  *s=fg_selfhost_q4dot(n,x,y);
#if FG_Q4_TRACE
  if((local_calls++&4095u)==0)atomic_fetch_add_explicit(&samples,1,memory_order_relaxed);
#endif
  return;
 }
#if FG_Q4_TRACE
 atomic_fetch_add_explicit(&fallbacks,1,memory_order_relaxed);
#endif
 original_dot(n,s,bs,x,bx,y,by,nrc);
}
#if FG_Q4_TRACE
__attribute__((destructor)) static void report(void) {
 fprintf(stderr,"FG_SELFHOST_Q4_SAMPLE_EVERY_4096=%llu FALLBACK_CALLS=%llu\n",
   (unsigned long long)atomic_load(&samples),(unsigned long long)atomic_load(&fallbacks));
}
#endif
