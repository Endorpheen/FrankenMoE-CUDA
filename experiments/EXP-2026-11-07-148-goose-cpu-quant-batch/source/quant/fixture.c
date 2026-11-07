#include "ggml.h"
#include "ggml-cpu.h"
#include "ggml-cpu/quants.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
int fg148_encode(int,const float*,void*,int64_t);
static const enum ggml_type types[]={GGML_TYPE_Q1_0,GGML_TYPE_Q2_0,GGML_TYPE_Q4_0,GGML_TYPE_Q4_1,GGML_TYPE_Q5_0,GGML_TYPE_Q5_1,GGML_TYPE_Q8_1,GGML_TYPE_Q8_1,GGML_TYPE_Q8_0,GGML_TYPE_Q8_K};
static atomic_uint_fast64_t cases;
static uint32_t random32(uint32_t*s){*s=*s*1664525u+1013904223u;return*s;}
static void compare(int kind,int n,int pattern,uint32_t seed){
 static void(*fn[])(const float*,void*,int64_t)={quantize_row_q1_0,quantize_row_q2_0,quantize_row_q4_0,quantize_row_q4_1,quantize_row_q5_0,quantize_row_q5_1,quantize_row_q8_1,NULL,quantize_row_q8_0,quantize_row_q8_K};
 static const int widths[]={128,64,32,32,32,32,32,32,32,256},strides[]={18,18,18,20,22,24,36,36,34,292};
 size_t bytes=(size_t)n/widths[kind]*strides[kind];float*x=malloc((size_t)n*4+64);unsigned char*a=malloc(bytes+128),*b=malloc(bytes+128);if(!x||!a||!b)abort();
 for(int i=0;i<n;++i){switch(pattern){case 0:x[i]=0;break;case 1:x[i]=-0.0f;break;case 2:x[i]=1;break;case 3:x[i]=-1;break;case 4:x[i]=(i%2?-1.f:1.f);break;case 5:x[i]=(i%31-15)*.5f;break;case 6:x[i]=(i%255-127)+.5f;break;default:x[i]=((int32_t)(random32(&seed)>>8)-8388608)*.000001f*powf(10.f,(float)(pattern-11));break;}}
 for(size_t i=0;i<bytes+128;++i)a[i]=b[i]=(unsigned char)(i*37+11);
 fn[kind](x,a+64,n);if(!fg148_encode(kind,x,b+64,n))abort();
 if(memcmp(a,b,bytes+128)){size_t at=0;while(at<bytes+128&&a[at]==b[at])++at;fprintf(stderr,"mismatch kind=%d n=%d pattern=%d byte=%zu native=%02x goose=%02x\n",kind,n,pattern,at,a[at],b[at]);abort();}
 for(size_t i=0;i<bytes+128;++i)b[i]=(unsigned char)(i*37+11);
 ggml_get_type_traits_cpu(types[kind])->from_float(x,b+64,n);if(memcmp(a,b,bytes+128)){fprintf(stderr,"trait mismatch kind=%d n=%d pattern=%d\n",kind,n,pattern);abort();}
 atomic_fetch_add(&cases,1);free(x);free(a);free(b);
}
static void*worker(void*id){for(int i=0;i<18;++i){int kind=i%10;if(kind==7)continue;int width=kind==0?128:kind==1?64:kind==9?256:32;compare(kind,width*3,7+i%8,314159u+(uint32_t)((uintptr_t)id*997+i));}return NULL;}
int main(void){ggml_cpu_init();int widths[]={128,64,32,32,32,32,32,32,32,256};int lengths[]={0,1,2,3,17};
 for(int kind=0;kind<10;++kind){if(kind==7)continue;for(int l=0;l<5;++l)for(int pattern=0;pattern<17;++pattern)compare(kind,widths[kind]*lengths[l],pattern,20261107u+kind*73+l*997+pattern);}
 pthread_t threads[8];for(uintptr_t i=0;i<8;++i)if(pthread_create(threads+i,NULL,worker,(void*)i))abort();for(int i=0;i<8;++i)pthread_join(threads[i],NULL);
 printf("{\"status\":\"PASS_NINE_FORMATS_NATIVE_BYTES_CANARIES_AND_CONCURRENT\",\"cases\":%llu,\"threads\":8}\n",(unsigned long long)atomic_load(&cases));return 0;
}
