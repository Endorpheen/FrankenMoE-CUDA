#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
int64_t fg143_ex_meta(int64_t,int64_t,int64_t);
int64_t fg143_ex_abort_value(int64_t);
void fg143_ex_compute(int64_t,int64_t);
void fg143_ex_fused(int64_t,int64_t,int64_t);
uint8_t fg143_ex_abort_requested(int64_t);
void fg143_ex_abort_at(int64_t,int64_t);
void fg143_ex_barrier(int64_t);
static atomic_uint_fast64_t steps,fusions,graphs;
static void fg143_ex_witness(int64_t n,int64_t f){atomic_fetch_add_explicit(&steps,n,memory_order_relaxed);atomic_fetch_add_explicit(&fusions,f,memory_order_relaxed);atomic_fetch_add_explicit(&graphs,1,memory_order_relaxed);}
#define main fg143_executor_unused_main
#include "../../../generated/cpu-executor.c"
#undef main
int64_t fg143_execute_graph(int64_t c,int64_t n,int64_t ith,int64_t nth,int64_t ref,int64_t fuse,int64_t compute,int64_t output){return gg_function_1(c,n,ith,nth,ref,fuse,compute,output,0);}
__attribute__((destructor))static void fg143_executor_report(void){fprintf(stderr,"FG143_EXECUTOR worker_graphs=%llu worker_steps=%llu worker_fusions=%llu\n",(unsigned long long)atomic_load(&graphs),(unsigned long long)atomic_load(&steps),(unsigned long long)atomic_load(&fusions));}
