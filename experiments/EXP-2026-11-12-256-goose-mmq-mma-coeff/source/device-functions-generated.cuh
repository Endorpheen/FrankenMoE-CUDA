#pragma once
static __device__ __forceinline__ void fg256d_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg256d_gg_index(int64_t i, int64_t len);
static __device__ __forceinline__ int64_t fg256d_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg256d_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg256d_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg256d_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg256d_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ uint8_t fg256d_gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, uint64_t p4, int64_t p5, int64_t base);
static __device__ __forceinline__ void fg256d_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg256d_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg256d_gg_fail("array index out of bounds"); return i; }
static __device__ __forceinline__ int64_t fg256d_gg_div(int64_t a, int64_t b) { if (!b) fg256d_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg256d_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg256d_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg256d_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg256d_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg256d_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg256d_gg_umod(uint64_t a,uint64_t b) { if(!b)fg256d_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg256d_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ uint8_t fg256d_gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t p3, uint64_t p4, int64_t p5, int64_t base) {
  _Alignas(8) uint8_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) float t2;
  _Alignas(8) float t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) float t5;
  _Alignas(8) float t6;
  _Alignas(8) float t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) float t9;
  _Alignas(8) float t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) float t12;
  _Alignas(8) float t13;
  _Alignas(8) float t14;
  _Alignas(8) uint64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) float t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) uint64_t t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) float t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) int64_t v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) uint64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) float v6;
  _Alignas(8) float v7;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t1 = ((int64_t)(v0));
#ifndef __cplusplus
  extern float (fg256d_b2f)(int64_t);
#endif
  t2 = fg256d_b2f(t1);
  t3 = ((float)(t2));
  t4 = ((int64_t)(v2));
#ifndef __cplusplus
  extern float (fg256d_b2f)(int64_t);
#endif
  t5 = fg256d_b2f(t4);
  t6 = ((float)(t5));
#ifndef __cplusplus
  extern float (fg256d_mul)(float, float);
#endif
  t7 = fg256d_mul(t3, t6);
  v6 = ((float)(t7));
  t8 = ((int64_t)(v1));
#ifndef __cplusplus
  extern float (fg256d_b2f)(int64_t);
#endif
  t9 = fg256d_b2f(t8);
  t10 = ((float)(t9));
  t11 = ((int64_t)(v3));
#ifndef __cplusplus
  extern float (fg256d_b2f)(int64_t);
#endif
  t12 = fg256d_b2f(t11);
  t13 = ((float)(t12));
#ifndef __cplusplus
  extern float (fg256d_mul)(float, float);
#endif
  t14 = fg256d_mul(t10, t13);
  v7 = ((float)(t14));
  t15 = ((uint64_t)(v4));
  t16 = ((int64_t)(v5));
  t17 = ((float)(v6));
#ifndef __cplusplus
  extern int64_t (fg256d_f2b)(float);
#endif
  t18 = fg256d_f2b(t17);
  t19 = ((int64_t)(t18));
#ifndef __cplusplus
  extern void (fg256d_st)(uint64_t, int64_t, int64_t);
#endif
  fg256d_st(t15, t16, t19);
  t20 = ((uint64_t)(v4));
  t21 = ((int64_t)(v5));
  t22 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) + (uint64_t)(t22)))));
  t24 = ((float)(v7));
#ifndef __cplusplus
  extern int64_t (fg256d_f2b)(float);
#endif
  t25 = fg256d_f2b(t24);
  t26 = ((int64_t)(t25));
#ifndef __cplusplus
  extern void (fg256d_st)(uint64_t, int64_t, int64_t);
#endif
  fg256d_st(t20, t23, t26);
  t0 = ((uint8_t)UINT64_C(1));
  }
  return t0;
}
