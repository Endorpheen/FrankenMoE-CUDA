#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
struct block {size_t offset,size;};
int fg_native_release_block(void*,int,size_t,size_t);
int fg_selfhost_release_block(void*,int,size_t,size_t);
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(void){struct block states[16][256]={0},work[16][256];int counts[16]={0,1,2,4,8,16,32,64,128,255,2,2,1,1,4,32};size_t offsets[16];
 for(int j=0;j<16;j++){for(int i=0;i<counts[j];i++)states[j][i]=(struct block){i*64,32};offsets[j]=32;}
 offsets[0]=0;offsets[1]=64;offsets[12]=32;states[13][0].offset=32;offsets[13]=0;
 for(int pass=0;pass<7;pass++)for(int k=0;k<2;k++){int goose=(pass+k)%2;int(*fn)(void*,int,size_t,size_t)=goose?fg_selfhost_release_block:fg_native_release_block;uint64_t sum=0;double elapsed=0;int output[16];
 for(int iteration=0;iteration<12000;iteration++){for(int j=0;j<16;j++)memcpy(work[j],states[j],(size_t)counts[j]*sizeof(struct block));double start=now();for(int j=0;j<16;j++)output[j]=fn(work[j],counts[j],offsets[j],32);elapsed+=now()-start;for(int j=0;j<16;j++)sum+=(uint64_t)output[j]+work[j][0].size;}
 printf("{\"pass\":%d,\"goose\":%d,\"ns_release_call\":%.6f,\"checksum\":%llu,\"preliminary\":true}\n",pass,goose,elapsed*1e9/(12000*16),(unsigned long long)sum);}
}
