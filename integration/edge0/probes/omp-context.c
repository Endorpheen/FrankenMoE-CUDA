// Diagnostic interposition only; never linked into the shipped runtime.
#define _GNU_SOURCE
#include "ggml-cpu.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdatomic.h>
static atomic_uint calls;
enum ggml_status ggml_graph_compute_with_ctx(struct ggml_context * c, struct ggml_cgraph * g, int threads) {
    typedef enum ggml_status (*compute_fn)(struct ggml_context *,struct ggml_cgraph *,int);
    compute_fn real=(compute_fn)dlsym(RTLD_NEXT,"ggml_graph_compute_with_ctx");
    typedef int (*query_fn)(void);
    query_fn num=(query_fn)dlsym(RTLD_DEFAULT,"omp_get_num_threads");
    query_fn level=(query_fn)dlsym(RTLD_DEFAULT,"omp_get_active_level");
    query_fn max=(query_fn)dlsym(RTLD_DEFAULT,"omp_get_max_active_levels");
    unsigned int k=atomic_fetch_add(&calls,1);
    if(k<100)fprintf(stderr,"EDGE0_OMP call=%u requested=%d outer_threads=%d active_level=%d max_active=%d\n",k,threads,num?num():-1,level?level():-1,max?max():-1);
    return real(c,g,threads);
}
