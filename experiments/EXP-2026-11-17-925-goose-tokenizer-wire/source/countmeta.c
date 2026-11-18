#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <execinfo.h>
static int64_t nk=0, na=0, nn=0;
int gguf_get_kv_type(const void * c, int64_t i) { static int (*f)(const void*, int64_t); if(!f) f=dlsym(RTLD_NEXT,"gguf_get_kv_type");
  if (nk < 2) { void * bt[6]; int n = backtrace(bt,6); fprintf(stderr,"COUNT kv_type from:\n"); backtrace_symbols_fd(bt,n,2); }
  nk++; return f(c,i); }
int gguf_get_arr_type(const void * c, int64_t i) { static int (*f)(const void*, int64_t); if(!f) f=dlsym(RTLD_NEXT,"gguf_get_arr_type"); na++; return f(c,i); }
int gguf_get_arr_n(const void * c, int64_t i) { static int (*f)(const void*, int64_t); if(!f) f=dlsym(RTLD_NEXT,"gguf_get_arr_n"); nn++; return f(c,i); }
__attribute__((destructor)) static void rep(void){ fprintf(stderr,"COUNT kv_type=%lld arr_type=%lld arr_n=%lld\n",(long long)nk,(long long)na,(long long)nn); }
