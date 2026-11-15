#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
typedef struct { const uint8_t *data; int64_t len; } fb;
typedef struct { uint16_t d; uint16_t dmin; uint8_t scales[12]; uint8_t qs[128]; } block_q4_K;
typedef struct { float d; int8_t qs[256]; int16_t bsums[16]; } block_q8_K;
extern "C" {
void quantize_row_q4_K_ref(const float*,void*,int64_t);
void quantize_row_q8_K_ref(const float*,void*,int64_t);
void ggml_vec_dot_q4_K_q8_K_generic(int,float*,size_t,const void*,size_t,const void*,size_t,int);
int32_t fg_cpu_simd_q4_k_q8_k(fb,fb,int32_t*,float*);
}
static uint32_t st=0x9e3779b9u;
static uint32_t nx(void){st^=st<<13;st^=st>>17;st^=st<<5;return st;}
int main(int argc,char**argv){
  int nb=argc>1?atoi(argv[1]):2; int iters=argc>2?atoi(argv[2]):128;
  int diff=0, maxulp=0;
  float *src=(float*)malloc(sizeof(float)*256*nb);
  block_q4_K *x=(block_q4_K*)malloc(sizeof(block_q4_K)*nb);
  block_q8_K *y=(block_q8_K*)malloc(sizeof(block_q8_K)*nb);
  double maxrel=0;
  for(int it=0;it<iters;it++){
    for(int i=0;i<256*nb;i++) src[i]=(float)((int32_t)(nx()%4096)-2048)/1024.0f;
    quantize_row_q4_K_ref(src,x,256*nb); quantize_row_q8_K_ref(src,y,256*nb);
    float native=0; ggml_vec_dot_q4_K_q8_K_generic(256*nb,&native,0,x,0,y,0,1);
    float sum=0.0f;
    for(int b=0;b<nb;b++){int32_t m=0;float v=0;fg_cpu_simd_q4_k_q8_k((fb){(uint8_t*)&x[b],144},(fb){(uint8_t*)&y[b],292},&m,&v);sum+=v;}
    if(native!=sum){
      diff++;
      int32_t a,b32; memcpy(&a,&native,4); memcpy(&b32,&sum,4);
      int32_t d = a>b32? a-b32 : b32-a; if(d>maxulp) maxulp=d;
      double rel = native!=0 ? fabs((double)(sum-native)/(double)native) : 0;
      if(rel>maxrel) maxrel=rel;
    }
  }
  printf("nb=%d iters=%d differs=%d max_ulp=%d max_rel=%.3g\n",nb,iters,diff,maxulp,maxrel);
  return 0;
}
