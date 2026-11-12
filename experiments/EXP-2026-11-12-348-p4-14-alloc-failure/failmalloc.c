#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
static void *(*real_malloc)(size_t);
static void *(*real_calloc)(size_t,size_t);
static unsigned long n_calls, fail_at;
static int inited;
static void init(void){
    real_malloc = dlsym(RTLD_NEXT, "malloc");
    real_calloc = dlsym(RTLD_NEXT, "calloc");
    const char *s = getenv("FAIL_AT");
    fail_at = s ? strtoul(s, NULL, 10) : 0;
    inited = 1;
}
void *malloc(size_t n){ if(!inited) init(); n_calls++; if(fail_at && n_calls==fail_at){ return NULL; } return real_malloc(n); }
void *calloc(size_t a,size_t b){ if(!inited) init(); n_calls++; if(fail_at && n_calls==fail_at){ return NULL; } return real_calloc(a,b); }
