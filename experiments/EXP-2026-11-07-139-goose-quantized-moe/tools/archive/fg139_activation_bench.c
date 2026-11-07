#define _GNU_SOURCE
#include "ggml.h"
#include "ggml-cpu.h"
#include "ggml-cpu/quants.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sched.h>
void fg139_quantize(int,const float*,void*,int64_t);
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static double measure(int kind,int n,int goose,int iters,float*x,void*y){double start=now();for(int i=0;i<iters;++i){if(goose)fg139_quantize(kind,x,y,n);else if(kind)quantize_row_q8_K(x,y,n);else quantize_row_q8_0(x,y,n);}return(now()-start)/iters;}
static int cmp(const void*a,const void*b){double x=*(const double*)a,y=*(const double*)b;return(x>y)-(x<y);}
int main(void){ggml_cpu_init();cpu_set_t allowed,one;int core=-1;if(!sched_getaffinity(0,sizeof(allowed),&allowed)){for(int i=0;i<CPU_SETSIZE;++i)if(CPU_ISSET(i,&allowed)){core=i;break;}CPU_ZERO(&one);CPU_SET(core,&one);if(sched_setaffinity(0,sizeof(one),&one))abort();}printf("{\"cpu\":%d,\"rounds\":5,\"counter_free\":true,\"cases\":[",core);int sizes[]={32,256,2560,4096},count=0;uint32_t seed=20261107;
for(int kind=0;kind<2;++kind)for(int j=kind;j<4;++j){int n=sizes[j],bytes=n/(kind?256:32)*(kind?292:34);float*x=malloc(n*4);unsigned char*a=malloc(bytes),*b=malloc(bytes);if(!x||!a||!b)abort();for(int i=0;i<n;++i){seed=seed*1664525+1013904223;x[i]=((int)(seed>>24)-128)*0.03125f;}memset(a,0x6b,bytes);memset(b,0x6b,bytes);measure(kind,n,0,1,x,a);measure(kind,n,1,1,x,b);if(memcmp(a,b,bytes))abort();int iters=1;while(measure(kind,n,0,iters,x,a)<0.002/iters&&iters<1048576)iters*=2;double values[2][5];for(int r=0;r<5;++r)for(int k=0;k<2;++k){int arm=(r+k)%2;values[arm][r]=measure(kind,n,arm,iters,x,b);}for(int k=0;k<2;++k)qsort(values[k],5,sizeof(double),cmp);printf("%s{\"kind\":%d,\"values\":%d,\"iters\":%d,\"native_us\":%.6f,\"goose_us\":%.6f,\"ratio\":%.6f}",count++?",":"",kind,n,iters,values[0][2]*1e6,values[1][2]*1e6,values[1][2]/values[0][2]);fflush(stdout);free(x);free(a);free(b);}puts("]}");return 0;}
