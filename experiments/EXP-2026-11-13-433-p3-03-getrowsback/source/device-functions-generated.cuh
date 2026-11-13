#pragma once
static __device__ __forceinline__ void fg433b_fail(const char*);
static __device__ __forceinline__ int64_t fg433b_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg433b_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg433b_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg433b_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg433b_double_bits(uint64_t bits);
static __device__ __forceinline__ int64_t fg433b_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t base);
static __device__ __forceinline__ void fg433b_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg433b_div(int64_t a, int64_t b) { if (!b) fg433b_fail("division by zero"); if (a==INT64_MIN && b==-1) fg433b_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg433b_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg433b_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg433b_udiv(uint64_t a,uint64_t b) { if(!b)fg433b_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg433b_umod(uint64_t a,uint64_t b) { if(!b)fg433b_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg433b_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ int64_t fg433b_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) uint8_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) uint8_t t9;
  _Alignas(8) uint64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) uint64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) uint8_t t17;
  _Alignas(8) float t18;
  _Alignas(8) uint64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) int64_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) uint64_t t26;
  _Alignas(8) float t27;
  _Alignas(8) float t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) uint64_t t31;
  _Alignas(8) int64_t t32;
  _Alignas(8) int64_t t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) int64_t t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) int64_t t37;
  _Alignas(8) uint64_t t38;
  _Alignas(8) float t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) int64_t t41;
  _Alignas(8) uint64_t v0;
  _Alignas(8) uint64_t v1;
  _Alignas(8) uint64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) int64_t v8;
  _Alignas(8) int64_t v9;
  _Alignas(8) float v10;
  _Alignas(8) int64_t v11;
  _Alignas(8) int64_t v12;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  v8 = p8;
  {
  t1 = ((int64_t)(v6));
  t2 = ((int64_t)(v3));
  t3 = ((t1) >= (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto fg433b_done;
  }
  }
  v9 = ((int64_t)(v7));
label1:;
  t4 = ((int64_t)(v9));
  t5 = ((int64_t)(v5));
  t6 = ((t4) < (t5));
  if (!(t6)) goto label2;
  {
  v10 = ((float)(fg433b_double_bits(UINT64_C(0))));
  v11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label4:;
  t7 = ((int64_t)(v11));
  t8 = ((int64_t)(v4));
  t9 = ((t7) < (t8));
  if (!(t9)) goto label5;
  {
  t10 = ((uint64_t)(v1));
  t11 = ((int64_t)(v11));
  t12 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t13 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t12)))))));
#ifndef __cplusplus
  extern int64_t (fg433b_ld_i32)(uint64_t, uint64_t);
#endif
  t14 = fg433b_ld_i32(t10, t13);
  v12 = ((int64_t)(t14));
  t15 = ((int64_t)(v12));
  t16 = ((int64_t)(v9));
  t17 = ((t15) == (t16));
  if (t17) {
  {
  t18 = ((float)(v10));
  t19 = ((uint64_t)(v0));
  t20 = ((int64_t)(v11));
  t21 = ((int64_t)(v3));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  t23 = ((int64_t)(v6));
  t24 = ((int64_t)(((int64_t)((uint64_t)(t22) + (uint64_t)(t23)))));
  t25 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t26 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t25)))))));
#ifndef __cplusplus
  extern float (fg433b_ld_f32)(uint64_t, uint64_t);
#endif
  t27 = fg433b_ld_f32(t19, t26);
  t28 = ((float)(t27));
  v10 = ((float)(((t18) + (t28))));
  }
  }
  t29 = ((int64_t)(v11));
  t30 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t30)))));
  }
  goto label4;
label5:;
  t31 = ((uint64_t)(v2));
  t32 = ((int64_t)(v9));
  t33 = ((int64_t)(v3));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) * (uint64_t)(t33)))));
  t35 = ((int64_t)(v6));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t34) + (uint64_t)(t35)))));
  t37 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t38 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t36) * (uint64_t)(t37)))))));
  t39 = ((float)(v10));
#ifndef __cplusplus
  extern void (fg433b_st_f32)(uint64_t, uint64_t, float);
#endif
  fg433b_st_f32(t31, t38, t39);
  t40 = ((int64_t)(v9));
  t41 = ((int64_t)(v8));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t40) + (uint64_t)(t41)))));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto fg433b_done;
  }
fg433b_done:;
  return t0;
}
