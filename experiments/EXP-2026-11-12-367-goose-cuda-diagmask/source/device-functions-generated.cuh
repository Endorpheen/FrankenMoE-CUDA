#pragma once
static __device__ __forceinline__ void fg367dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg367dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg367dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg367dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg367dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg367dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ float fg367dd_gg_function_6(int64_t base);
static __device__ __forceinline__ int64_t fg367dd_gg_function_4(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base);
static __device__ __forceinline__ void fg367dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ int64_t fg367dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t base);
static __device__ __forceinline__ void fg367dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg367dd_gg_div(int64_t a, int64_t b) { if (!b) fg367dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg367dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg367dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg367dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg367dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg367dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg367dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg367dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg367dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ float fg367dd_gg_function_6(int64_t base) {
  _Alignas(8) float t0;
  {
  t0 = ((float)(fg367dd_gg_double_bits(UINT64_C(5183643170566569984))));
  goto fg367dd_gg_done;
  }
fg367dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ int64_t fg367dd_gg_function_4(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) uint64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) uint64_t t15;
  _Alignas(8) float t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) uint8_t t19;
  _Alignas(8) float t20;
  _Alignas(8) float t21;
  _Alignas(8) float t22;
  _Alignas(8) uint64_t t23;
  _Alignas(8) int64_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) uint64_t t26;
  _Alignas(8) float t27;
  _Alignas(8) uint64_t v0;
  _Alignas(8) uint64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) int64_t v8;
  _Alignas(8) float v9;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  t1 = ((int64_t)(v6));
  t2 = ((int64_t)(v2));
  t3 = ((t1) >= (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto fg367dd_gg_done;
  }
  }
  t4 = ((int64_t)(v5));
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  t7 = ((int64_t)(v6));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t6) + (uint64_t)(t7)))));
  t8 = ((int64_t)(v4));
  t9 = ((int64_t)(v5));
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(((int64_t)fg367dd_gg_mod(t9, t10))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t8) + (uint64_t)(t11)))));
  t12 = ((uint64_t)(v0));
  t13 = ((int64_t)(v7));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t15 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t13) * (uint64_t)(t14)))))));
#ifndef __cplusplus
  extern float (fg367d_ld_f32)(uint64_t, uint64_t);
#endif
  t16 = fg367d_ld_f32(t12, t15);
  v9 = ((float)(((float)(t16))));
  t17 = ((int64_t)(v6));
  t18 = ((int64_t)(v8));
  t19 = ((t17) > (t18));
  if (t19) {
  {
  t20 = ((float)(v9));
  t21 = fg367dd_gg_function_6(base + 0);
  t22 = ((float)(t21));
  v9 = ((float)(((float)(((t20) - (t22))))));
  }
  }
  t23 = ((uint64_t)(v1));
  t24 = ((int64_t)(v7));
  t25 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t26 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t25)))))));
  t27 = ((float)(v9));
#ifndef __cplusplus
  extern void (fg367d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg367d_st_f32(t23, t26, t27);
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto fg367dd_gg_done;
  }
fg367dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg367dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
  _Alignas(8) uint8_t t0;
  _Alignas(8) uint8_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t v0;
  _Alignas(8) int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t0 = v0;
  t1 = (!(t0));
  if (t1) {
  {
  t2 = ((int64_t)(v1));
#ifndef __cplusplus
  extern void (fg367d_fail)(int64_t);
#endif
  fg367d_fail(t2);
  }
  }
  }
}
static __device__ __forceinline__ int64_t fg367dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) uint8_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) uint8_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) uint64_t t21;
  _Alignas(8) uint64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) int64_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) uint64_t v0;
  _Alignas(8) uint64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) int64_t v8;
  _Alignas(8) int64_t v9;
  _Alignas(8) int64_t v10;
  _Alignas(8) int64_t v11;
  _Alignas(8) int64_t v12;
  _Alignas(8) int64_t v13;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  v8 = p8;
  v9 = p9;
  v10 = p10;
  v11 = p11;
  {
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) > (t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(20))));
  fg367dd_gg_function_2(t3, t4, base + 0);
  t5 = ((int64_t)(v3));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t7 = ((t5) > (t6));
  t8 = ((int64_t)(((int64_t)UINT64_C(21))));
  fg367dd_gg_function_2(t7, t8, base + 0);
  t9 = ((int64_t)(v4));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t11 = ((t9) > (t10));
  t12 = ((int64_t)(((int64_t)UINT64_C(22))));
  fg367dd_gg_function_2(t11, t12, base + 0);
  t13 = ((int64_t)(v11));
  t14 = ((int64_t)(v7));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t13) * (uint64_t)(t14)))));
  t16 = ((int64_t)(v9));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t15) + (uint64_t)(t16)))));
  t17 = ((int64_t)(v10));
  t18 = ((int64_t)(v6));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) * (uint64_t)(t18)))));
  t20 = ((int64_t)(v8));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t19) + (uint64_t)(t20)))));
  t21 = ((uint64_t)(v0));
  t22 = ((uint64_t)(v1));
  t23 = ((int64_t)(v2));
  t24 = ((int64_t)(v4));
  t25 = ((int64_t)(v5));
  t26 = ((int64_t)(v13));
  t27 = ((int64_t)(v12));
  t0 = fg367dd_gg_function_4(t21, t22, t23, t24, t25, t26, t27, base + 0);
  goto fg367dd_gg_done;
  }
fg367dd_gg_done:;
  return t0;
}
