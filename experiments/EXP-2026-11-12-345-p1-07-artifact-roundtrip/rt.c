#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ggml.h"
void QUANT(const float *, void *, int64_t);
void DEQUANT(const void *, float *, int64_t);
void GOOSE(const void *, float *, int64_t);
#ifndef QK
#define QK 256
#endif
#ifndef BLK
#define BLK 144
#endif
static uint64_t rng=0x243f6a8885a308d3ull;
static uint32_t nx(void){rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;return (uint32_t)(rng>>32);}
static float fr(void){return (float)((int32_t)(nx()&0xffffff)-0x800000)/1048576.0f;}
int main(void){
#ifdef NEED_INIT
  ggml_quantize_init((enum ggml_type) TYPE_ID);
#endif
  int checks=0, failures=0;
  const int sizes[]={QK, 2*QK, 10*QK};
  for (int s=0;s<3;s++){
    int k=sizes[s], nb=k/QK;
    float *x=malloc((size_t)k*4); uint8_t *q=malloc((size_t)nb*BLK);
    float *a=malloc((size_t)k*4), *b=malloc((size_t)k*4);
    for (int i=0;i<k;i++) x[i]=fr()*3.0f;
    if (s==2) for (int i=0;i<QK;i++) x[i]=0.0f;
    memset(q,0xAA,(size_t)nb*BLK); memset(a,0,(size_t)k*4); memset(b,0,(size_t)k*4);
    QUANT(x,q,k);
    DEQUANT(q,a,k);
    GOOSE(q,b,k);
    for (int i=0;i<k;i++){ checks++; if (memcmp(&a[i],&b[i],4)){ failures++; if (failures<4) printf("FAIL k=%d i=%d native=%g goose=%g\n",k,i,a[i],b[i]); } }
    free(x);free(q);free(a);free(b);
  }
  printf("RT checks=%d failures=%d\n",checks,failures);
  return failures!=0;
}
