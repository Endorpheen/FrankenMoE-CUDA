#pragma once
static __device__ __forceinline__ void fg258df_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg258df_gg_index(int64_t i, int64_t len);
static __device__ __forceinline__ int64_t fg258df_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg258df_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg258df_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg258df_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg258df_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ uint8_t fg258df_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base);
static __device__ __forceinline__ void fg258df_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg258df_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg258df_gg_fail("array index out of bounds"); return i; }
static __device__ __forceinline__ int64_t fg258df_gg_div(int64_t a, int64_t b) { if (!b) fg258df_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg258df_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg258df_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg258df_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg258df_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg258df_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg258df_gg_umod(uint64_t a,uint64_t b) { if(!b)fg258df_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg258df_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ uint8_t fg258df_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base) {
  _Alignas(8) uint8_t t0;
  _Alignas(8) uint64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) uint64_t t8;
  _Alignas(8) uint64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) uint64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) float t17;
  _Alignas(8) float t18;
  _Alignas(8) uint64_t v0;
  _Alignas(8) uint64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) uint64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((int64_t)(v2));
  t3 = ((int64_t)(v4));
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(((int64_t)((uint64_t)(t3) * (uint64_t)(t4)))));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t2) + (uint64_t)(t5)))));
  t7 = ((int64_t)(((int64_t)UINT64_C(210))));
  t8 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t6) * (uint64_t)(t7)))))));
  v6 = ((uint64_t)(((uint64_t)((uint64_t)(t1) + (uint64_t)(t8)))));
  t9 = ((uint64_t)(v1));
  t10 = ((int64_t)(v4));
  t11 = ((int64_t)(v5));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  t15 = ((uint64_t)(v6));
  t16 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(208))))));
#ifndef __cplusplus
  extern float (fg258df_ld_f16_f32)(uint64_t, int64_t);
#endif
  t17 = fg258df_ld_f16_f32(t15, t16);
  t18 = ((float)(t17));
#ifndef __cplusplus
  extern void (fg258df_st_f32)(uint64_t, int64_t, float);
#endif
  fg258df_st_f32(t9, t14, t18);
  t0 = ((uint8_t)UINT64_C(1));
  }
  return t0;
}
static __device__ __forceinline__ uint8_t fg258df_run(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5){return fg258df_gg_function_1(p0, p1, p2, p3, p4, p5, 0);}

