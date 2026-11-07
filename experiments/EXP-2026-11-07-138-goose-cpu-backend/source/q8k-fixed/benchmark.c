#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <string.h>
extern void fg_selfhost_quant_q8k(const float *,void *,int64_t);
extern void quantize_row_q8_K(const float *,void *,int64_t);
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static double timed(int goose,int n,const float*x,void*y){int repeat=3000;double t=now();for(int i=0;i<repeat;++i){if(goose)fg_selfhost_quant_q8k(x,y,n);else quantize_row_q8_K(x,y,n);}return (now()-t)*1e9/repeat;}
int main(void){for(int n=256;n<=16384;n*=4){float*x=malloc(n*4);void*y=calloc((size_t)n/256,292);for(int i=0;i<n;++i)x[i]=(float)((i*31337ULL)%200003)/1024.0f-97.0f;timed(0,n,x,y);timed(1,n,x,y);for(int r=0;r<5;++r){double native,goose;if(r%2){goose=timed(1,n,x,y);native=timed(0,n,x,y);}else{native=timed(0,n,x,y);goose=timed(1,n,x,y);}printf("{\"n\":%d,\"repeat\":%d,\"native_ns\":%.9g,\"goose_ns\":%.9g,\"preliminary\":true}\n",n,r,native,goose);}free(x);free(y);}return 0;}
