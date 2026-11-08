#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <immintrin.h>
static uint32_t fg_q8k_float_bits(float x){uint32_t u;memcpy(&u,&x,4);return u;}
static float fg139_float_from_bits(uint32_t u){float x;memcpy(&x,&u,4);return x;}
static uint16_t fg139_float_half(float x){return _cvtss_sh(x,_MM_FROUND_TO_NEAREST_INT);}
static atomic_uint_fast64_t calls[10],blocks[10];
static void fg139_quant_witness(int64_t kind,int64_t count){assert(kind>=0&&kind<10&&count>=0);atomic_fetch_add_explicit(calls+kind,1,memory_order_relaxed);atomic_fetch_add_explicit(blocks+kind,count,memory_order_relaxed);}
#define main fg148_unused_main
#include "../../../generated/cpu-quant.c"
#undef main
int fg148_encode(int kind,const float*x,void*y,int64_t n){
 const int width[]={128,64,32,32,32,32,32,32,32,256};const int stride[]={18,18,18,20,22,24,36,36,34,292};
 assert(kind>=0&&kind<10&&n>=0&&n%width[kind]==0);assert((uint64_t)(n/width[kind])<=SIZE_MAX/(unsigned)stride[kind]);
 return gg_function_1(kind,gg_view((void*)x,kind<8?n:0),gg_view(y,kind<8?(n/width[kind])*stride[kind]:0),gg_view((void*)x,kind==8?n/32:0),gg_view((void*)x,kind==9?n/256:0),gg_view(y,kind==8?n/32:0),gg_view(y,kind==9?n/256:0),0);
}
__attribute__((destructor))static void report(void){for(int i=0;i<10;++i){uint64_t n=atomic_load(calls+i);if(n)fprintf(stderr,"FG148_QUANT kind=%d calls=%llu values=%llu\n",i,(unsigned long long)n,(unsigned long long)atomic_load(blocks+i));}}

static int enabled=1;
__attribute__((constructor))static void config(void){const char*v=getenv("GGML_GOOSE_CPU_QUANT");enabled=!v||strcmp(v,"0");}
extern void quantize_row_q1_0(const float*,void*,int64_t);
void fg148_row_q1_0(const float*x,void*y,int64_t n){if(!enabled||!fg148_encode(0,x,y,n))quantize_row_q1_0(x,y,n);}
extern void quantize_row_q2_0(const float*,void*,int64_t);
void fg148_row_q2_0(const float*x,void*y,int64_t n){if(!enabled||!fg148_encode(1,x,y,n))quantize_row_q2_0(x,y,n);}
extern void quantize_row_q4_0(const float*,void*,int64_t);
void fg148_row_q4_0(const float*x,void*y,int64_t n){if(!enabled||!fg148_encode(2,x,y,n))quantize_row_q4_0(x,y,n);}
extern void quantize_row_q4_1(const float*,void*,int64_t);
void fg148_row_q4_1(const float*x,void*y,int64_t n){if(!enabled||!fg148_encode(3,x,y,n))quantize_row_q4_1(x,y,n);}
extern void quantize_row_q5_0(const float*,void*,int64_t);
void fg148_row_q5_0(const float*x,void*y,int64_t n){if(!enabled||!fg148_encode(4,x,y,n))quantize_row_q5_0(x,y,n);}
extern void quantize_row_q5_1(const float*,void*,int64_t);
void fg148_row_q5_1(const float*x,void*y,int64_t n){if(!enabled||!fg148_encode(5,x,y,n))quantize_row_q5_1(x,y,n);}
extern void quantize_row_q8_1(const float*,void*,int64_t);
void fg148_row_q8_1(const float*x,void*y,int64_t n){if(!enabled||!fg148_encode(6,x,y,n))quantize_row_q8_1(x,y,n);}
extern void quantize_row_q8_0(const float*,void*,int64_t);
void fg148_row_q8_0(const float*x,void*y,int64_t n){if(!enabled||!fg148_encode(8,x,y,n))quantize_row_q8_0(x,y,n);}
extern void quantize_row_q8_K(const float*,void*,int64_t);
void fg148_row_q8_K(const float*x,void*y,int64_t n){if(!enabled||!fg148_encode(9,x,y,n))quantize_row_q8_K(x,y,n);}
extern void fg_selfhost_quant_q8k(const float*,void*,int64_t);
extern void fg139_quantize_q8_0(const float*,void*,int64_t);
void fg148_moe_q8k(const float*x,void*y,int64_t n){if(!enabled){fg_selfhost_quant_q8k(x,y,n);return;}assert(fg148_encode(9,x,y,n));}
void fg148_moe_q80(const float*x,void*y,int64_t n){if(!enabled){fg139_quantize_q8_0(x,y,n);return;}assert(fg148_encode(8,x,y,n));}
