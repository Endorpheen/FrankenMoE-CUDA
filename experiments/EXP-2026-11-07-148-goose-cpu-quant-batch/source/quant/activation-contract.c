#include "ggml.h"
#include "ggml-cpu.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
extern void quantize_row_q8_0(const float*,void*,int64_t);
extern void quantize_row_q8_K(const float*,void*,int64_t);
int fg148_encode(int,const float*,void*,int64_t);
static void fg139_quantize(int kind,const float*x,void*y,int64_t n){if(!fg148_encode(kind?9:8,x,y,n))abort();}
static atomic_uint_fast64_t calls[2],values[2];
static uint32_t next_random(uint32_t*s){*s=*s*1664525u+1013904223u;return*s;}
static void compare(int kind,int n,int pattern,uint32_t seed){
 size_t size=kind?(size_t)n/256*292:(size_t)n/32*34;
 unsigned char*xraw=malloc((size_t)n*4+132),*a=malloc(size+128),*b=malloc(size+128),*before=malloc((size_t)n*4+132);if(!xraw||!a||!b||!before)abort();
 memset(xraw,0x5a,(size_t)n*4+132);float*x=(float*)(xraw+68);
 static float ties[]={127,-127,0.5f,1.5f,2.5f,-0.5f,-1.5f,-2.5f,126.5f,-126.5f};
 for(int i=0;i<n;++i){
  if(pattern==0)x[i]=0;else if(pattern==1)x[i]=-0.0f;else if(pattern==2)x[i]=127;else if(pattern==3)x[i]=-127;
  else if(pattern==4)x[i]=i%2?-1:1;else if(pattern==5)x[i]=(i%255-127)*0.5f;else if(pattern==6)x[i]=ties[i%10];
  else x[i]=((int32_t)(next_random(&seed)>>8)-8388608)*0.000001f*powf(10,(float)(pattern-13));
 }
 if(pattern>=100){uint32_t bits[]={0x7fc12345,0xffc23456,0x7f812345,0x7f800000,0xff800000,1,0x80000001,0x00800000,0x80000000};for(int i=0;i<n;++i)x[i]=1;uint32_t u=bits[(pattern-100)/32];memcpy(x+(pattern-100)%32,&u,4);}
 memcpy(before,xraw,(size_t)n*4+132);for(size_t i=0;i<size+128;++i)a[i]=b[i]=(unsigned char)(i*37+11);
 if(kind)quantize_row_q8_K(x,a+64,n);else quantize_row_q8_0(x,a+64,n);
 fg139_quantize(kind,x,b+64,n);
 if(memcmp(a,b,size+128)){fprintf(stderr,"mismatch kind=%d n=%d pattern=%d\n",kind,n,pattern);abort();}
 if(memcmp(before,xraw,(size_t)n*4+132))abort();
 atomic_fetch_add(&calls[kind],1);atomic_fetch_add(&values[kind],n);
 free(before);free(b);free(a);free(xraw);
}
static void*worker(void*arg){intptr_t id=(intptr_t)arg;for(int i=0;i<32;++i)compare((int)(i%2),i%2?2048:2560,7+(i%12),20261107u+(uint32_t)(id*997+i));return NULL;}
int main(void){
 ggml_cpu_init();int zero[]={0,32,64,96,256,640,1024,2560,16384};int large[]={0,256,512,1024,2048,4096,16384};
 for(int k=0;k<2;++k)for(int j=0;j<(k?7:9);++j)for(int p=0;p<(k?18:19);++p)compare(k,k?large[j]:zero[j],p,20261107u+j*193u+p*313u);
 for(int special=0;special<9;++special)for(int pos=0;pos<32;++pos)compare(0,32,100+special*32+pos,1);
 pthread_t threads[8];for(intptr_t i=0;i<8;++i)if(pthread_create(threads+i,NULL,worker,(void*)i))abort();for(int i=0;i<8;++i)pthread_join(threads[i],NULL);
 printf("{\"status\":\"PASS_NATIVE_QUANT_BYTES_AND_CANARIES\",\"q8_0_cases\":%llu,\"q8_K_cases\":%llu,\"q8_0_values\":%llu,\"q8_K_values\":%llu,\"concurrent_threads\":8}\n",(unsigned long long)atomic_load(&calls[0]),(unsigned long long)atomic_load(&calls[1]),(unsigned long long)atomic_load(&values[0]),(unsigned long long)atomic_load(&values[1]));return 0;
}
