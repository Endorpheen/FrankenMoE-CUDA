#include <stdio.h>
#include <time.h>
#include <nvtx3/nvToolsExt.h>
static volatile long sink;
static long tns(struct timespec*a,struct timespec*b){return (b->tv_sec-a->tv_sec)*1000000000L+(b->tv_nsec-a->tv_nsec);}
int main(){ int N=2000000; struct timespec a,b; long B=0,C=0,D=0;
  clock_gettime(CLOCK_MONOTONIC,&a); for(int i=0;i<N;i++){ sink+=i; } clock_gettime(CLOCK_MONOTONIC,&b); B=tns(&a,&b);
  clock_gettime(CLOCK_MONOTONIC,&a); for(int i=0;i<N;i++){ struct timespec t0,t1; clock_gettime(CLOCK_MONOTONIC,&t0); sink+=i; clock_gettime(CLOCK_MONOTONIC,&t1); sink+=t1.tv_nsec&1; } clock_gettime(CLOCK_MONOTONIC,&b); C=tns(&a,&b);
  clock_gettime(CLOCK_MONOTONIC,&a); for(int i=0;i<N;i++){ nvtxRangePushA("g"); struct timespec t0,t1; clock_gettime(CLOCK_MONOTONIC,&t0); sink+=i; clock_gettime(CLOCK_MONOTONIC,&t1); nvtxRangePop(); sink+=t1.tv_nsec&1; } clock_gettime(CLOCK_MONOTONIC,&b); D=tns(&a,&b);
  printf("clock pair = %.1f ns/op\nnvtx pair = %.1f ns/op\ntotal instrumentation = %.1f ns/op\n",
     (double)(C-B)/N,(double)(D-C)/N,(double)(D-B)/N);
  double tot=(double)(D-B)/N;
  printf("model request upper bound (<=3*17886 instrumented ops) = %.2f ms CPU-side\n", tot*3*17886/1e6);
  return 0;
}
