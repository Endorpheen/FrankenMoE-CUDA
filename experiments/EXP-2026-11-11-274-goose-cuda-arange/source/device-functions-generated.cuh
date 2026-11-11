#pragma once
static __device__ __forceinline__ void fg274dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg274dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg274dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg274dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg274dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg274dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ void fg274dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg274dd_gg_function_1(uint64_t p0, int64_t p1, float p2, float p3, int64_t p4, int64_t base);
static __device__ __forceinline__ void fg274dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg274dd_gg_div(int64_t a, int64_t b) { if (!b) fg274dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg274dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg274dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg274dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg274dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg274dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg274dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg274dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg274dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ void fg274dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
  uint8_t t0;
  uint8_t t1;
  int64_t t2;
  uint8_t v0;
  int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t0 = v0;
  t1 = (!(t0));
  if (t1) {
  {
  t2 = ((int64_t)(v1));
#ifndef __cplusplus
  extern void (fg274d_fail)(int64_t);
#endif
  fg274d_fail(t2);
  }
  }
  }
fg274dd_gg_done:;
}
static __device__ __forceinline__ void fg274dd_gg_function_1(uint64_t p0, int64_t p1, float p2, float p3, int64_t p4, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  int64_t t6;
  uint64_t t7;
  int64_t t8;
  int64_t t9;
  uint64_t t10;
  float t11;
  float t12;
  float t13;
  float t14;
  float t15;
  uint64_t v0;
  int64_t v1;
  float v2;
  float v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t0 = ((int64_t)(v4));
  t1 = ((int64_t)(((int64_t)(v1))));
  t2 = ((t0) >= (t1));
  if (t2) {
  {
  goto fg274dd_gg_done;
  }
  }
  t3 = ((int64_t)(v4));
  t4 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t5 = ((t3) >= (t4));
  t6 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg274dd_gg_function_2(t5, t6, base + 0);
  t7 = ((uint64_t)(v0));
  t8 = ((int64_t)(v4));
  t9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t10 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t8) * (uint64_t)(t9)))))));
  t11 = ((float)(v2));
  t12 = ((float)(v3));
  t13 = ((float)(((float)(v4))));
  t14 = ((float)(((t12) * (t13))));
  t15 = ((float)(((t11) + (t14))));
#ifndef __cplusplus
  extern void (fg274d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg274d_st_f32(t7, t10, t15);
  }
fg274dd_gg_done:;
}
