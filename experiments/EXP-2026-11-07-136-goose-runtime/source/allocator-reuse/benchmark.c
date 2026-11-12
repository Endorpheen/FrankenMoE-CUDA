#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>
struct block {size_t offset,size;};
int64_t fg_selfhost_choose_reuse(const void*,int,size_t,size_t*);
__attribute__((noinline)) static int64_t native(const void *p,int n,size_t size,size_t *available) {
 const struct block*b=p;int64_t best=-1;size_t fit=SIZE_MAX;*available=0;
 for(int i=0;i<n-1;i++){if(b[i].size>*available)*available=b[i].size;if(b[i].size>=size&&b[i].size<=fit){best=i;fit=b[i].size;}}
 return best;
}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(void){struct block states[9][256];int counts[9]={0,1,2,4,8,16,32,128,256};uint64_t seed=20261107;
 for(int j=0;j<9;j++)for(int i=0;i<256;i++){seed=seed*6364136223846793005ULL+1;states[j][i]=(struct block){i*4096,(seed>>32)%65536};}
 for(int pass=0;pass<7;pass++)for(int k=0;k<2;k++){int goose=(pass+k)%2;int64_t(*fn)(const void*,int,size_t,size_t*)=goose?fg_selfhost_choose_reuse:native;uint64_t sum=0;double t=now();for(int i=0;i<300000;i++){int j=i%9;size_t available;int64_t choice=fn(states[j],counts[j],(unsigned)i%65536,&available);sum+=(uint64_t)(choice+1)+available;}double ns=(now()-t)*1e9/300000;printf("{\"pass\":%d,\"goose\":%d,\"ns_call\":%.6f,\"checksum\":%llu,\"preliminary\":true}\n",pass,goose,ns,(unsigned long long)sum);}
}
