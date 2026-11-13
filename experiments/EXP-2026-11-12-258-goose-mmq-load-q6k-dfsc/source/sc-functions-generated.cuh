#pragma once
static __device__ __forceinline__ void fg258sc_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg258sc_gg_index(int64_t i, int64_t len);
static __device__ __forceinline__ int64_t fg258sc_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg258sc_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg258sc_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg258sc_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg258sc_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ uint8_t fg258sc_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t base);
static __device__ __forceinline__ void fg258sc_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg258sc_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg258sc_gg_fail("array index out of bounds"); return i; }
static __device__ __forceinline__ int64_t fg258sc_gg_div(int64_t a, int64_t b) { if (!b) fg258sc_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg258sc_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg258sc_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg258sc_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg258sc_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg258sc_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg258sc_gg_umod(uint64_t a,uint64_t b) { if(!b)fg258sc_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg258sc_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ uint8_t fg258sc_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t base) {
  _Alignas(8) uint8_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) uint64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) uint64_t t16;
  _Alignas(8) uint64_t t17;
  _Alignas(8) uint64_t t18;
  _Alignas(8) uint64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) uint64_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) int64_t t32;
  _Alignas(8) int64_t t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) uint64_t v0;
  _Alignas(8) uint64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) int64_t v8;
  _Alignas(8) uint64_t v9;
  _Alignas(8) int64_t v10;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  {
  t1 = ((int64_t)(v6));
  t2 = ((int64_t)(v7));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t4 = ((int64_t)(((int64_t)fg258sc_gg_div(t2, t3))));
  v8 = ((int64_t)(((int64_t)fg258sc_gg_mod(t1, t4))));
  t5 = ((uint64_t)(v0));
  t6 = ((int64_t)(v2));
  t7 = ((int64_t)(v4));
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) * (uint64_t)(t8)))));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t6) + (uint64_t)(t9)))));
  t11 = ((int64_t)(v8));
  t12 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t13 = ((int64_t)(((int64_t)fg258sc_gg_div(t11, t12))));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t13)))));
  t15 = ((int64_t)(((int64_t)UINT64_C(210))));
  t16 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))))));
  v9 = ((uint64_t)(((uint64_t)((uint64_t)(t5) + (uint64_t)(t16)))));
  t17 = ((uint64_t)(v9));
  t18 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(192))))));
  t19 = ((uint64_t)(((uint64_t)((uint64_t)(t17) + (uint64_t)(t18)))));
  t20 = ((int64_t)(v8));
  t21 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
#ifndef __cplusplus
  extern int64_t (fg258sc_ld_i32)(uint64_t, int64_t);
#endif
  t23 = fg258sc_ld_i32(t19, t22);
  v10 = ((int64_t)(t23));
  t24 = ((uint64_t)(v1));
  t25 = ((int64_t)(v4));
  t26 = ((int64_t)(v5));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t25) * (uint64_t)(t26)))));
  t28 = ((int64_t)(v6));
  t29 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t30 = ((int64_t)(((int64_t)fg258sc_gg_mod(t28, t29))));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t27) + (uint64_t)(t30)))));
  t32 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) * (uint64_t)(t32)))));
  t34 = ((int64_t)(v10));
#ifndef __cplusplus
  extern void (fg258sc_st_i32)(uint64_t, int64_t, int64_t);
#endif
  fg258sc_st_i32(t24, t33, t34);
  t0 = ((uint8_t)UINT64_C(1));
  }
  return t0;
}
static __device__ __forceinline__ uint8_t fg258sc_run(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7){return fg258sc_gg_function_1(p0, p1, p2, p3, p4, p5, p6, p7, 0);}

