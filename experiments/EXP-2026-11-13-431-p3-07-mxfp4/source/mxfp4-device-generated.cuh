#pragma once
#define FG431D_KERNEL fg431d_function_1
static __device__ __forceinline__ void fg431d_fail(const char*);
static __device__ __forceinline__ int64_t fg431d_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg431d_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg431d_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg431d_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg431d_double_bits(uint64_t bits);
static __device__ __forceinline__ int32_t fg431d_function_14(float p0, float p1, int64_t base);
static __device__ __forceinline__ float fg431d_function_11(int32_t p0, int64_t base);
static __device__ __forceinline__ int32_t fg431d_function_8(float p0, int64_t base);
static __device__ __forceinline__ float fg431d_function_7(float p0, float p1, int64_t base);
static __device__ __forceinline__ float fg431d_function_5(float p0, int64_t base);
static __device__ __forceinline__ int64_t fg431d_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int32_t p18, int64_t base);
static __device__ __forceinline__ void fg431d_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg431d_div(int64_t a, int64_t b) { if (!b) fg431d_fail("division by zero"); if (a==INT64_MIN && b==-1) fg431d_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg431d_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg431d_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg431d_udiv(uint64_t a,uint64_t b) { if(!b)fg431d_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg431d_umod(uint64_t a,uint64_t b) { if(!b)fg431d_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg431d_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ int32_t fg431d_function_14(float p0, float p1, int64_t base) {
  _Alignas(8) int32_t t0;
  _Alignas(8) float t1;
  _Alignas(8) float t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) float t4;
  _Alignas(8) float t5;
  _Alignas(8) float t6;
  _Alignas(8) float t7;
  _Alignas(8) float t8;
  _Alignas(8) float t9;
  _Alignas(8) float t10;
  _Alignas(8) float t11;
  _Alignas(8) float t12;
  _Alignas(8) float t13;
  _Alignas(8) float t14;
  _Alignas(8) float t15;
  _Alignas(8) float t16;
  _Alignas(8) float t17;
  _Alignas(8) uint8_t t18;
  _Alignas(8) float t19;
  _Alignas(8) float t20;
  _Alignas(8) float t21;
  _Alignas(8) float t22;
  _Alignas(8) float t23;
  _Alignas(8) float t24;
  _Alignas(8) float t25;
  _Alignas(8) float t26;
  _Alignas(8) float t27;
  _Alignas(8) float t28;
  _Alignas(8) uint8_t t29;
  _Alignas(8) float t30;
  _Alignas(8) float t31;
  _Alignas(8) float t32;
  _Alignas(8) float t33;
  _Alignas(8) float t34;
  _Alignas(8) float t35;
  _Alignas(8) float t36;
  _Alignas(8) float t37;
  _Alignas(8) float t38;
  _Alignas(8) float t39;
  _Alignas(8) uint8_t t40;
  _Alignas(8) float t41;
  _Alignas(8) float t42;
  _Alignas(8) float t43;
  _Alignas(8) float t44;
  _Alignas(8) float t45;
  _Alignas(8) float t46;
  _Alignas(8) float t47;
  _Alignas(8) float t48;
  _Alignas(8) float t49;
  _Alignas(8) float t50;
  _Alignas(8) uint8_t t51;
  _Alignas(8) float t52;
  _Alignas(8) float t53;
  _Alignas(8) float t54;
  _Alignas(8) float t55;
  _Alignas(8) float t56;
  _Alignas(8) float t57;
  _Alignas(8) float t58;
  _Alignas(8) float t59;
  _Alignas(8) float t60;
  _Alignas(8) float t61;
  _Alignas(8) uint8_t t62;
  _Alignas(8) float t63;
  _Alignas(8) float t64;
  _Alignas(8) float t65;
  _Alignas(8) float t66;
  _Alignas(8) float t67;
  _Alignas(8) float t68;
  _Alignas(8) float t69;
  _Alignas(8) float t70;
  _Alignas(8) float t71;
  _Alignas(8) float t72;
  _Alignas(8) uint8_t t73;
  _Alignas(8) float t74;
  _Alignas(8) float t75;
  _Alignas(8) float t76;
  _Alignas(8) float t77;
  _Alignas(8) float t78;
  _Alignas(8) float t79;
  _Alignas(8) float t80;
  _Alignas(8) float t81;
  _Alignas(8) float t82;
  _Alignas(8) float t83;
  _Alignas(8) uint8_t t84;
  _Alignas(8) int32_t t85;
  _Alignas(8) int32_t t86;
  _Alignas(8) float v0;
  _Alignas(8) float v1;
  _Alignas(8) int32_t v2;
  _Alignas(8) float v3;
  _Alignas(8) int32_t v4;
  _Alignas(8) float v5;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((float)(v0));
  t2 = ((float)(((float)(fg431d_double_bits(UINT64_C(0))))));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  v2 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(8))))));
  }
  } else {
  {
  v2 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  }
  }
  t4 = ((float)(v0));
#ifndef __cplusplus
  extern float (fg431_fabs)(float);
#endif
  t5 = fg431_fabs(t4);
  t6 = ((float)(t5));
  t7 = ((float)(v1));
  v3 = ((float)(((t6) * (t7))));
  v4 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  t8 = ((float)(v3));
  t9 = ((float)(((float)(fg431d_double_bits(UINT64_C(0))))));
  t10 = ((float)(((t8) - (t9))));
#ifndef __cplusplus
  extern float (fg431_fabs)(float);
#endif
  t11 = fg431_fabs(t10);
  v5 = ((float)(t11));
  t12 = ((float)(v3));
  t13 = ((float)(((float)(fg431d_double_bits(UINT64_C(4602678819172646912))))));
  t14 = ((float)(((t12) - (t13))));
#ifndef __cplusplus
  extern float (fg431_fabs)(float);
#endif
  t15 = fg431_fabs(t14);
  t16 = ((float)(t15));
  t17 = ((float)(v5));
  t18 = ((t16) < (t17));
  if (t18) {
  {
  v4 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  t19 = ((float)(v3));
  t20 = ((float)(((float)(fg431d_double_bits(UINT64_C(4602678819172646912))))));
  t21 = ((float)(((t19) - (t20))));
#ifndef __cplusplus
  extern float (fg431_fabs)(float);
#endif
  t22 = fg431_fabs(t21);
  v5 = ((float)(t22));
  }
  }
  t23 = ((float)(v3));
  t24 = ((float)(((float)(fg431d_double_bits(UINT64_C(4607182418800017408))))));
  t25 = ((float)(((t23) - (t24))));
#ifndef __cplusplus
  extern float (fg431_fabs)(float);
#endif
  t26 = fg431_fabs(t25);
  t27 = ((float)(t26));
  t28 = ((float)(v5));
  t29 = ((t27) < (t28));
  if (t29) {
  {
  v4 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(2))))));
  t30 = ((float)(v3));
  t31 = ((float)(((float)(fg431d_double_bits(UINT64_C(4607182418800017408))))));
  t32 = ((float)(((t30) - (t31))));
#ifndef __cplusplus
  extern float (fg431_fabs)(float);
#endif
  t33 = fg431_fabs(t32);
  v5 = ((float)(t33));
  }
  }
  t34 = ((float)(v3));
  t35 = ((float)(((float)(fg431d_double_bits(UINT64_C(4609434218613702656))))));
  t36 = ((float)(((t34) - (t35))));
#ifndef __cplusplus
  extern float (fg431_fabs)(float);
#endif
  t37 = fg431_fabs(t36);
  t38 = ((float)(t37));
  t39 = ((float)(v5));
  t40 = ((t38) < (t39));
  if (t40) {
  {
  v4 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(3))))));
  t41 = ((float)(v3));
  t42 = ((float)(((float)(fg431d_double_bits(UINT64_C(4609434218613702656))))));
  t43 = ((float)(((t41) - (t42))));
#ifndef __cplusplus
  extern float (fg431_fabs)(float);
#endif
  t44 = fg431_fabs(t43);
  v5 = ((float)(t44));
  }
  }
  t45 = ((float)(v3));
  t46 = ((float)(((float)(fg431d_double_bits(UINT64_C(4611686018427387904))))));
  t47 = ((float)(((t45) - (t46))));
#ifndef __cplusplus
  extern float (fg431_fabs)(float);
#endif
  t48 = fg431_fabs(t47);
  t49 = ((float)(t48));
  t50 = ((float)(v5));
  t51 = ((t49) < (t50));
  if (t51) {
  {
  v4 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(4))))));
  t52 = ((float)(v3));
  t53 = ((float)(((float)(fg431d_double_bits(UINT64_C(4611686018427387904))))));
  t54 = ((float)(((t52) - (t53))));
#ifndef __cplusplus
  extern float (fg431_fabs)(float);
#endif
  t55 = fg431_fabs(t54);
  v5 = ((float)(t55));
  }
  }
  t56 = ((float)(v3));
  t57 = ((float)(((float)(fg431d_double_bits(UINT64_C(4613937818241073152))))));
  t58 = ((float)(((t56) - (t57))));
#ifndef __cplusplus
  extern float (fg431_fabs)(float);
#endif
  t59 = fg431_fabs(t58);
  t60 = ((float)(t59));
  t61 = ((float)(v5));
  t62 = ((t60) < (t61));
  if (t62) {
  {
  v4 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(5))))));
  t63 = ((float)(v3));
  t64 = ((float)(((float)(fg431d_double_bits(UINT64_C(4613937818241073152))))));
  t65 = ((float)(((t63) - (t64))));
#ifndef __cplusplus
  extern float (fg431_fabs)(float);
#endif
  t66 = fg431_fabs(t65);
  v5 = ((float)(t66));
  }
  }
  t67 = ((float)(v3));
  t68 = ((float)(((float)(fg431d_double_bits(UINT64_C(4616189618054758400))))));
  t69 = ((float)(((t67) - (t68))));
#ifndef __cplusplus
  extern float (fg431_fabs)(float);
#endif
  t70 = fg431_fabs(t69);
  t71 = ((float)(t70));
  t72 = ((float)(v5));
  t73 = ((t71) < (t72));
  if (t73) {
  {
  v4 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(6))))));
  t74 = ((float)(v3));
  t75 = ((float)(((float)(fg431d_double_bits(UINT64_C(4616189618054758400))))));
  t76 = ((float)(((t74) - (t75))));
#ifndef __cplusplus
  extern float (fg431_fabs)(float);
#endif
  t77 = fg431_fabs(t76);
  v5 = ((float)(t77));
  }
  }
  t78 = ((float)(v3));
  t79 = ((float)(((float)(fg431d_double_bits(UINT64_C(4618441417868443648))))));
  t80 = ((float)(((t78) - (t79))));
#ifndef __cplusplus
  extern float (fg431_fabs)(float);
#endif
  t81 = fg431_fabs(t80);
  t82 = ((float)(t81));
  t83 = ((float)(v5));
  t84 = ((t82) < (t83));
  if (t84) {
  {
  v4 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(7))))));
  }
  }
  t85 = ((int32_t)(v4));
  t86 = ((int32_t)(v2));
  t0 = ((int32_t)(((t85) | (t86))));
  goto fg431d_done;
  }
fg431d_done:;
  return t0;
}
static __device__ __forceinline__ float fg431d_function_11(int32_t p0, int64_t base) {
  _Alignas(8) float t0;
  _Alignas(8) int32_t t1;
  _Alignas(8) int32_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) uint32_t t4;
  _Alignas(8) uint32_t t5;
  _Alignas(8) uint32_t t6;
  _Alignas(8) float t7;
  _Alignas(8) int32_t v0;
  _Alignas(8) uint32_t v1;
  v0 = p0;
  {
  v1 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(0))))));
  t1 = ((int32_t)(v0));
  t2 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  v1 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4194304))))));
  }
  } else {
  {
  t4 = ((uint32_t)(((uint32_t)(v0))));
  t5 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(23))))));
  v1 = ((uint32_t)(((uint32_t)((uint64_t)(t4) << ((uint64_t)(t5) & 31)))));
  }
  }
  t6 = ((uint32_t)(v1));
#ifndef __cplusplus
  extern float (fg431_bits_f32)(uint32_t);
#endif
  t7 = fg431_bits_f32(t6);
  t0 = ((float)(t7));
  goto fg431d_done;
  }
fg431d_done:;
  return t0;
}
static __device__ __forceinline__ int32_t fg431d_function_8(float p0, int64_t base) {
  _Alignas(8) int32_t t0;
  _Alignas(8) float t1;
  _Alignas(8) float t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) uint8_t t4;
  _Alignas(8) float t5;
  _Alignas(8) float t6;
  _Alignas(8) float t7;
  _Alignas(8) int32_t t8;
  _Alignas(8) int32_t t9;
  _Alignas(8) int32_t t10;
  _Alignas(8) int32_t t11;
  _Alignas(8) int32_t t12;
  _Alignas(8) int32_t t13;
  _Alignas(8) int32_t t14;
  _Alignas(8) uint8_t t15;
  _Alignas(8) int32_t t16;
  _Alignas(8) int32_t t17;
  _Alignas(8) uint8_t t18;
  _Alignas(8) float v0;
  _Alignas(8) float v1;
  _Alignas(8) int32_t v2;
  _Alignas(8) int32_t v3;
  v0 = p0;
  {
  t1 = ((float)(v0));
  t2 = ((float)(((float)(fg431d_double_bits(UINT64_C(0))))));
  t3 = ((t1) > (t2));
  t4 = (!(t3));
  if (t4) {
  {
  t0 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  goto fg431d_done;
  }
  }
  t5 = ((float)(v0));
#ifndef __cplusplus
  extern float (fg431_log2)(float);
#endif
  t6 = fg431_log2(t5);
  v1 = ((float)(t6));
  t7 = ((float)(v1));
#ifndef __cplusplus
  extern int32_t (fg431_rint)(float);
#endif
  t8 = fg431_rint(t7);
  v2 = ((int32_t)(t8));
  t9 = ((int32_t)(v2));
  t10 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(2))))));
  t11 = ((int32_t)(((int32_t)((uint64_t)(t9) - (uint64_t)(t10)))));
  t12 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(127))))));
  v3 = ((int32_t)(((int32_t)((uint64_t)(t11) + (uint64_t)(t12)))));
  t13 = ((int32_t)(v3));
  t14 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  t15 = ((t13) < (t14));
  if (t15) {
  {
  v3 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  }
  }
  t16 = ((int32_t)(v3));
  t17 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(254))))));
  t18 = ((t16) > (t17));
  if (t18) {
  {
  v3 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(254))))));
  }
  }
  t0 = ((int32_t)(v3));
  goto fg431d_done;
  }
fg431d_done:;
  return t0;
}
static __device__ __forceinline__ float fg431d_function_7(float p0, float p1, int64_t base) {
  _Alignas(8) float t0;
  _Alignas(8) float t1;
  _Alignas(8) float t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) float v0;
  _Alignas(8) float v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((float)(v0));
  t2 = ((float)(v1));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  t0 = ((float)(v1));
  }
  } else {
  {
  t0 = ((float)(v0));
  }
  }
  goto fg431d_done;
  }
fg431d_done:;
  return t0;
}
static __device__ __forceinline__ float fg431d_function_5(float p0, int64_t base) {
  _Alignas(8) float t0;
  _Alignas(8) float t1;
  _Alignas(8) float t2;
  _Alignas(8) uint32_t t3;
  _Alignas(8) int32_t t4;
  _Alignas(8) float t5;
  _Alignas(8) float t6;
  _Alignas(8) float t7;
  _Alignas(8) float t8;
  _Alignas(8) uint32_t t9;
  _Alignas(8) int32_t t10;
  _Alignas(8) float t11;
  _Alignas(8) float t12;
  _Alignas(8) float t13;
  _Alignas(8) float t14;
  _Alignas(8) uint32_t t15;
  _Alignas(8) int32_t t16;
  _Alignas(8) float t17;
  _Alignas(8) float t18;
  _Alignas(8) float t19;
  _Alignas(8) float t20;
  _Alignas(8) uint32_t t21;
  _Alignas(8) int32_t t22;
  _Alignas(8) float t23;
  _Alignas(8) float t24;
  _Alignas(8) float t25;
  _Alignas(8) float t26;
  _Alignas(8) uint32_t t27;
  _Alignas(8) int32_t t28;
  _Alignas(8) float t29;
  _Alignas(8) float t30;
  _Alignas(8) float v0;
  _Alignas(8) float v1;
  v0 = p0;
  {
  v1 = ((float)(v0));
  t1 = ((float)(v1));
  t2 = ((float)(v1));
  t3 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t4 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(16))))));
#ifndef __cplusplus
  extern float (fg431_shfl_f32)(float, uint32_t, int32_t);
#endif
  t5 = fg431_shfl_f32(t2, t3, t4);
  t6 = ((float)(t5));
  v1 = fg431d_function_7(t1, t6, base + 0);
  t7 = ((float)(v1));
  t8 = ((float)(v1));
  t9 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t10 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(8))))));
#ifndef __cplusplus
  extern float (fg431_shfl_f32)(float, uint32_t, int32_t);
#endif
  t11 = fg431_shfl_f32(t8, t9, t10);
  t12 = ((float)(t11));
  v1 = fg431d_function_7(t7, t12, base + 0);
  t13 = ((float)(v1));
  t14 = ((float)(v1));
  t15 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t16 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(4))))));
#ifndef __cplusplus
  extern float (fg431_shfl_f32)(float, uint32_t, int32_t);
#endif
  t17 = fg431_shfl_f32(t14, t15, t16);
  t18 = ((float)(t17));
  v1 = fg431d_function_7(t13, t18, base + 0);
  t19 = ((float)(v1));
  t20 = ((float)(v1));
  t21 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t22 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(2))))));
#ifndef __cplusplus
  extern float (fg431_shfl_f32)(float, uint32_t, int32_t);
#endif
  t23 = fg431_shfl_f32(t20, t21, t22);
  t24 = ((float)(t23));
  v1 = fg431d_function_7(t19, t24, base + 0);
  t25 = ((float)(v1));
  t26 = ((float)(v1));
  t27 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t28 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
#ifndef __cplusplus
  extern float (fg431_shfl_f32)(float, uint32_t, int32_t);
#endif
  t29 = fg431_shfl_f32(t26, t27, t28);
  t30 = ((float)(t29));
  v1 = fg431d_function_7(t25, t30, base + 0);
  t0 = ((float)(v1));
  goto fg431d_done;
  }
fg431d_done:;
  return t0;
}
static __device__ __forceinline__ int64_t fg431d_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int32_t p18, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) uint8_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int32_t t16;
  _Alignas(8) int32_t t17;
  _Alignas(8) int32_t t18;
  _Alignas(8) int32_t t19;
  _Alignas(8) int32_t t20;
  _Alignas(8) int32_t t21;
  _Alignas(8) int32_t t22;
  _Alignas(8) int32_t t23;
  _Alignas(8) uint8_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) uint64_t t31;
  _Alignas(8) uint64_t t32;
  _Alignas(8) uint8_t t33;
  _Alignas(8) uint64_t t34;
  _Alignas(8) int64_t t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) uint64_t t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) int64_t t41;
  _Alignas(8) int64_t t42;
  _Alignas(8) int64_t t43;
  _Alignas(8) int64_t t44;
  _Alignas(8) int64_t t45;
  _Alignas(8) int64_t t46;
  _Alignas(8) int64_t t47;
  _Alignas(8) int64_t t48;
  _Alignas(8) int32_t t49;
  _Alignas(8) int32_t t50;
  _Alignas(8) uint8_t t51;
  _Alignas(8) int64_t t52;
  _Alignas(8) int64_t t53;
  _Alignas(8) int64_t t54;
  _Alignas(8) int64_t t55;
  _Alignas(8) int64_t t56;
  _Alignas(8) int64_t t57;
  _Alignas(8) int64_t t58;
  _Alignas(8) int64_t t59;
  _Alignas(8) uint8_t t60;
  _Alignas(8) uint64_t t61;
  _Alignas(8) int64_t t62;
  _Alignas(8) int64_t t63;
  _Alignas(8) int64_t t64;
  _Alignas(8) int64_t t65;
  _Alignas(8) uint64_t t66;
  _Alignas(8) float t67;
  _Alignas(8) float t68;
  _Alignas(8) float t69;
  _Alignas(8) float t70;
  _Alignas(8) float t71;
  _Alignas(8) float t72;
  _Alignas(8) float t73;
  _Alignas(8) uint8_t t74;
  _Alignas(8) int32_t t75;
  _Alignas(8) float t76;
  _Alignas(8) float t77;
  _Alignas(8) float t78;
  _Alignas(8) float t79;
  _Alignas(8) int32_t t80;
  _Alignas(8) uint32_t t81;
  _Alignas(8) int32_t t82;
  _Alignas(8) int32_t t83;
  _Alignas(8) int32_t t84;
  _Alignas(8) uint32_t t85;
  _Alignas(8) int32_t t86;
  _Alignas(8) int32_t t87;
  _Alignas(8) int32_t t88;
  _Alignas(8) int32_t t89;
  _Alignas(8) int32_t t90;
  _Alignas(8) uint32_t t91;
  _Alignas(8) int32_t t92;
  _Alignas(8) int32_t t93;
  _Alignas(8) int32_t t94;
  _Alignas(8) int32_t t95;
  _Alignas(8) int32_t t96;
  _Alignas(8) uint32_t t97;
  _Alignas(8) int32_t t98;
  _Alignas(8) int32_t t99;
  _Alignas(8) int32_t t100;
  _Alignas(8) int32_t t101;
  _Alignas(8) int32_t t102;
  _Alignas(8) int32_t t103;
  _Alignas(8) int32_t t104;
  _Alignas(8) int32_t t105;
  _Alignas(8) int32_t t106;
  _Alignas(8) int32_t t107;
  _Alignas(8) int32_t t108;
  _Alignas(8) int32_t t109;
  _Alignas(8) int32_t t110;
  _Alignas(8) int32_t t111;
  _Alignas(8) int32_t t112;
  _Alignas(8) int32_t t113;
  _Alignas(8) int32_t t114;
  _Alignas(8) int32_t t115;
  _Alignas(8) uint8_t t116;
  _Alignas(8) int32_t t117;
  _Alignas(8) int32_t t118;
  _Alignas(8) int32_t t119;
  _Alignas(8) int32_t t120;
  _Alignas(8) uint8_t t121;
  _Alignas(8) int32_t t122;
  _Alignas(8) int32_t t123;
  _Alignas(8) uint8_t t124;
  _Alignas(8) int32_t t125;
  _Alignas(8) int32_t t126;
  _Alignas(8) uint8_t t127;
  _Alignas(8) int64_t t128;
  _Alignas(8) int64_t t129;
  _Alignas(8) int64_t t130;
  _Alignas(8) int64_t t131;
  _Alignas(8) uint64_t t132;
  _Alignas(8) int64_t t133;
  _Alignas(8) int64_t t134;
  _Alignas(8) uint64_t t135;
  _Alignas(8) int64_t t136;
  _Alignas(8) int64_t t137;
  _Alignas(8) int64_t t138;
  _Alignas(8) int64_t t139;
  _Alignas(8) int64_t t140;
  _Alignas(8) int64_t t141;
  _Alignas(8) int64_t t142;
  _Alignas(8) int64_t t143;
  _Alignas(8) int64_t t144;
  _Alignas(8) int64_t t145;
  _Alignas(8) int64_t t146;
  _Alignas(8) int64_t t147;
  _Alignas(8) int64_t t148;
  _Alignas(8) int64_t t149;
  _Alignas(8) int64_t t150;
  _Alignas(8) int64_t t151;
  _Alignas(8) int64_t t152;
  _Alignas(8) int64_t t153;
  _Alignas(8) int64_t t154;
  _Alignas(8) int32_t t155;
  _Alignas(8) int32_t t156;
  _Alignas(8) uint8_t t157;
  _Alignas(8) uint64_t t158;
  _Alignas(8) int64_t t159;
  _Alignas(8) int64_t t160;
  _Alignas(8) int64_t t161;
  _Alignas(8) int64_t t162;
  _Alignas(8) int64_t t163;
  _Alignas(8) int64_t t164;
  _Alignas(8) int64_t t165;
  _Alignas(8) int64_t t166;
  _Alignas(8) int64_t t167;
  _Alignas(8) int64_t t168;
  _Alignas(8) uint64_t t169;
  _Alignas(8) int32_t t170;
  _Alignas(8) uint64_t t171;
  _Alignas(8) int64_t t172;
  _Alignas(8) int64_t t173;
  _Alignas(8) int64_t t174;
  _Alignas(8) int64_t t175;
  _Alignas(8) int64_t t176;
  _Alignas(8) int64_t t177;
  _Alignas(8) int64_t t178;
  _Alignas(8) int64_t t179;
  _Alignas(8) int64_t t180;
  _Alignas(8) int64_t t181;
  _Alignas(8) int64_t t182;
  _Alignas(8) int64_t t183;
  _Alignas(8) uint64_t t184;
  _Alignas(8) int32_t t185;
  _Alignas(8) int32_t t186;
  _Alignas(8) int32_t t187;
  _Alignas(8) uint8_t t188;
  _Alignas(8) uint64_t t189;
  _Alignas(8) int64_t t190;
  _Alignas(8) int64_t t191;
  _Alignas(8) int64_t t192;
  _Alignas(8) int64_t t193;
  _Alignas(8) uint64_t t194;
  _Alignas(8) int32_t t195;
  _Alignas(8) int32_t t196;
  _Alignas(8) int32_t t197;
  _Alignas(8) int32_t t198;
  _Alignas(8) int32_t t199;
  _Alignas(8) int32_t t200;
  _Alignas(8) int32_t t201;
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
  _Alignas(8) int32_t v10;
  _Alignas(8) int32_t v11;
  _Alignas(8) int32_t v12;
  _Alignas(8) int32_t v13;
  _Alignas(8) int32_t v14;
  _Alignas(8) int32_t v15;
  _Alignas(8) int32_t v16;
  _Alignas(8) int32_t v17;
  _Alignas(8) int32_t v18;
  _Alignas(8) int64_t v19;
  _Alignas(8) int64_t v20;
  _Alignas(8) int64_t v21;
  _Alignas(8) int32_t v22;
  _Alignas(8) int32_t v23;
  _Alignas(8) int32_t v24;
  _Alignas(8) int64_t v25;
  _Alignas(8) int64_t v26;
  _Alignas(8) int64_t v27;
  _Alignas(8) int64_t v28;
  _Alignas(8) int32_t v29;
  _Alignas(8) int32_t v30;
  _Alignas(8) int32_t v31;
  _Alignas(8) int32_t v32;
  _Alignas(8) int32_t v33;
  _Alignas(8) int64_t v34;
  _Alignas(8) float v35;
  _Alignas(8) float v36;
  _Alignas(8) int32_t v37;
  _Alignas(8) float v38;
  _Alignas(8) int32_t v39;
  _Alignas(8) int32_t v40;
  _Alignas(8) int32_t v41;
  _Alignas(8) int32_t v42;
  _Alignas(8) int32_t v43;
  _Alignas(8) int32_t v44;
  _Alignas(8) int32_t v45;
  _Alignas(8) int32_t v46;
  _Alignas(8) int64_t v47;
  _Alignas(8) int64_t v48;
  _Alignas(8) int64_t v49;
  _Alignas(8) int64_t v50;
  _Alignas(8) int64_t v51;
  _Alignas(8) int64_t v52;
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
  v12 = p12;
  v13 = p13;
  v14 = p14;
  v15 = p15;
  v16 = p16;
  v17 = p17;
  v18 = p18;
  {
  t1 = ((int64_t)(((int64_t)(v12))));
  t2 = ((int64_t)(((int64_t)(v17))));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) * (uint64_t)(t2)))));
  t4 = ((int64_t)(((int64_t)(v15))));
  t5 = ((int64_t)(((int64_t)((uint64_t)(t3) + (uint64_t)(t4)))));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t5) * (uint64_t)(t6)))));
  t7 = ((int64_t)(v19));
  t8 = ((int64_t)(v7));
  t9 = ((t7) >= (t8));
  if (t9) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto fg431d_done;
  }
  }
  t10 = ((int64_t)(v19));
  t11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(256))))));
  v20 = ((int64_t)(((int64_t)fg431d_div(t10, t11))));
  t12 = ((int64_t)(v19));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(256))))));
  t14 = ((int64_t)(((int64_t)fg431d_mod(t12, t13))));
  t15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  v21 = ((int64_t)(((int64_t)fg431d_div(t14, t15))));
  t16 = ((int32_t)(v14));
  t17 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(4))))));
  v22 = ((int32_t)(((int32_t)fg431d_div(t16, t17))));
  t18 = ((int32_t)(v14));
  t19 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(4))))));
  v23 = ((int32_t)(((int32_t)fg431d_mod(t18, t19))));
  t20 = ((int32_t)(v22));
  t21 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(2))))));
  v24 = ((int32_t)(((int32_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  v25 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t22 = ((int32_t)(v18));
  t23 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  t24 = ((t22) != (t23));
  if (t24) {
  {
  t25 = ((int64_t)(((int64_t)(v11))));
  t26 = ((int64_t)(v5));
  v25 = ((int64_t)(((int64_t)((uint64_t)(t25) * (uint64_t)(t26)))));
  }
  } else {
  {
  t27 = ((int64_t)(((int64_t)(v13))));
  t28 = ((int64_t)(v9));
  v26 = ((int64_t)(((int64_t)fg431d_mod(t27, t28))));
  t29 = ((int64_t)(((int64_t)(v13))));
  t30 = ((int64_t)(v9));
  v27 = ((int64_t)(((int64_t)fg431d_div(t29, t30))));
  v28 = ((int64_t)(((int64_t)(v11))));
  t31 = ((uint64_t)(v1));
  t32 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t33 = ((t31) != (t32));
  if (t33) {
  {
  t34 = ((uint64_t)(v1));
  t35 = ((int64_t)(((int64_t)(v11))));
  t36 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t37 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t35) * (uint64_t)(t36)))))));
#ifndef __cplusplus
  extern int64_t (fg431_ld_i32)(uint64_t, uint64_t);
#endif
  t38 = fg431_ld_i32(t34, t37);
  v28 = ((int64_t)(t38));
  }
  }
  t39 = ((int64_t)(v27));
  t40 = ((int64_t)(v6));
  t41 = ((int64_t)(((int64_t)((uint64_t)(t39) * (uint64_t)(t40)))));
  t42 = ((int64_t)(v26));
  t43 = ((int64_t)(v5));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) * (uint64_t)(t43)))));
  t45 = ((int64_t)(((int64_t)((uint64_t)(t41) + (uint64_t)(t44)))));
  t46 = ((int64_t)(v28));
  t47 = ((int64_t)(v4));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t46) * (uint64_t)(t47)))));
  v25 = ((int64_t)(((int64_t)((uint64_t)(t45) + (uint64_t)(t48)))));
  }
  }
  v29 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  v30 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  v31 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  v32 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  v33 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
label1:;
  t49 = ((int32_t)(v33));
  t50 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(2))))));
  t51 = ((t49) < (t50));
  if (!(t51)) goto label2;
  {
  t52 = ((int64_t)(v19));
  t53 = ((int64_t)(((int64_t)(v33))));
  t54 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t53) * (uint64_t)(t54)))));
  t56 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t55)))));
  t57 = ((int64_t)(((int64_t)(v14))));
  v34 = ((int64_t)(((int64_t)((uint64_t)(t56) + (uint64_t)(t57)))));
  v35 = ((float)(fg431d_double_bits(UINT64_C(0))));
  t58 = ((int64_t)(v34));
  t59 = ((int64_t)(v3));
  t60 = ((t58) < (t59));
  if (t60) {
  {
  t61 = ((uint64_t)(v0));
  t62 = ((int64_t)(v25));
  t63 = ((int64_t)(v34));
  t64 = ((int64_t)(((int64_t)((uint64_t)(t62) + (uint64_t)(t63)))));
  t65 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t66 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t64) * (uint64_t)(t65)))))));
#ifndef __cplusplus
  extern float (fg431_ld_f32)(uint64_t, uint64_t);
#endif
  t67 = fg431_ld_f32(t61, t66);
  v35 = ((float)(t67));
  }
  }
  t68 = ((float)(v35));
#ifndef __cplusplus
  extern float (fg431_fabs)(float);
#endif
  t69 = fg431_fabs(t68);
  t70 = ((float)(t69));
  v36 = fg431d_function_5(t70, base + 0);
  t71 = ((float)(v36));
  v37 = fg431d_function_8(t71, base + 0);
  v38 = ((float)(fg431d_double_bits(UINT64_C(0))));
  t72 = ((float)(v36));
  t73 = ((float)(((float)(fg431d_double_bits(UINT64_C(0))))));
  t74 = ((t72) > (t73));
  if (t74) {
  {
  t75 = ((int32_t)(v37));
  t76 = fg431d_function_11(t75, base + 0);
#ifndef __cplusplus
  extern float (fg431_rcp)(float);
#endif
  t77 = fg431_rcp(t76);
  v38 = ((float)(t77));
  }
  }
  t78 = ((float)(v35));
  t79 = ((float)(v38));
  v39 = fg431d_function_14(t78, t79, base + 0);
  t80 = ((int32_t)(v39));
  t81 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t82 = ((int32_t)(v24));
#ifndef __cplusplus
  extern int32_t (fg431_shfl_i32)(int32_t, uint32_t, int32_t);
#endif
  t83 = fg431_shfl_i32(t80, t81, t82);
  v40 = ((int32_t)(t83));
  t84 = ((int32_t)(v39));
  t85 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t86 = ((int32_t)(v24));
  t87 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  t88 = ((int32_t)(((int32_t)((uint64_t)(t86) + (uint64_t)(t87)))));
#ifndef __cplusplus
  extern int32_t (fg431_shfl_i32)(int32_t, uint32_t, int32_t);
#endif
  t89 = fg431_shfl_i32(t84, t85, t88);
  v41 = ((int32_t)(t89));
  t90 = ((int32_t)(v39));
  t91 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t92 = ((int32_t)(v24));
  t93 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(16))))));
  t94 = ((int32_t)(((int32_t)((uint64_t)(t92) + (uint64_t)(t93)))));
#ifndef __cplusplus
  extern int32_t (fg431_shfl_i32)(int32_t, uint32_t, int32_t);
#endif
  t95 = fg431_shfl_i32(t90, t91, t94);
  v42 = ((int32_t)(t95));
  t96 = ((int32_t)(v39));
  t97 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t98 = ((int32_t)(v24));
  t99 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(17))))));
  t100 = ((int32_t)(((int32_t)((uint64_t)(t98) + (uint64_t)(t99)))));
#ifndef __cplusplus
  extern int32_t (fg431_shfl_i32)(int32_t, uint32_t, int32_t);
#endif
  t101 = fg431_shfl_i32(t96, t97, t100);
  v43 = ((int32_t)(t101));
  t102 = ((int32_t)(v42));
  t103 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(4))))));
  t104 = ((int32_t)(((int32_t)((uint64_t)(t102) << ((uint64_t)(t103) & 31)))));
  t105 = ((int32_t)(v40));
  t106 = ((int32_t)(((t104) | (t105))));
  t107 = ((int32_t)(v43));
  t108 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(4))))));
  t109 = ((int32_t)(((int32_t)((uint64_t)(t107) << ((uint64_t)(t108) & 31)))));
  t110 = ((int32_t)(v41));
  t111 = ((int32_t)(((t109) | (t110))));
  t112 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(8))))));
  t113 = ((int32_t)(((int32_t)((uint64_t)(t111) << ((uint64_t)(t112) & 31)))));
  v44 = ((int32_t)(((t106) | (t113))));
  t114 = ((int32_t)(v33));
  t115 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  t116 = ((t114) == (t115));
  if (t116) {
  {
  v29 = ((int32_t)(v37));
  v31 = ((int32_t)(v44));
  }
  } else {
  {
  v30 = ((int32_t)(v37));
  v32 = ((int32_t)(v44));
  }
  }
  t117 = ((int32_t)(v33));
  t118 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  v33 = ((int32_t)(((int32_t)((uint64_t)(t117) + (uint64_t)(t118)))));
  }
  goto label1;
label2:;
  t119 = ((int32_t)(v18));
  t120 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  t121 = ((t119) != (t120));
  if (t121) {
  {
  v45 = ((int32_t)(v10));
  }
  } else {
  {
  v45 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  }
  }
  v46 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
label4:;
  t122 = ((int32_t)(v46));
  t123 = ((int32_t)(v45));
  t124 = ((t122) < (t123));
  if (!(t124)) goto label5;
  {
  v47 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t125 = ((int32_t)(v18));
  t126 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  t127 = ((t125) != (t126));
  if (t127) {
  {
  t128 = ((int64_t)(((int64_t)(v11))));
  t129 = ((int64_t)(((int64_t)(v10))));
  t130 = ((int64_t)(((int64_t)((uint64_t)(t128) * (uint64_t)(t129)))));
  t131 = ((int64_t)(((int64_t)(v46))));
  v48 = ((int64_t)(((int64_t)((uint64_t)(t130) + (uint64_t)(t131)))));
  t132 = ((uint64_t)(v1));
  t133 = ((int64_t)(v48));
  t134 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t135 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t133) * (uint64_t)(t134)))))));
#ifndef __cplusplus
  extern int64_t (fg431_ld_i32)(uint64_t, uint64_t);
#endif
  t136 = fg431_ld_i32(t132, t135);
  v49 = ((int64_t)(t136));
  t137 = ((int64_t)(v20));
  t138 = ((int64_t)(v8));
  t139 = ((int64_t)(((int64_t)((uint64_t)(t137) * (uint64_t)(t138)))));
  t140 = ((int64_t)(v49));
  v47 = ((int64_t)(((int64_t)((uint64_t)(t139) + (uint64_t)(t140)))));
  }
  } else {
  {
  t141 = ((int64_t)(v7));
  t142 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(256))))));
  v50 = ((int64_t)(((int64_t)fg431d_div(t141, t142))));
  t143 = ((int64_t)(((int64_t)(v13))));
  t144 = ((int64_t)(v8));
  t145 = ((int64_t)(v50));
  t146 = ((int64_t)(((int64_t)((uint64_t)(t144) * (uint64_t)(t145)))));
  v51 = ((int64_t)(((int64_t)((uint64_t)(t143) * (uint64_t)(t146)))));
  t147 = ((int64_t)(v51));
  t148 = ((int64_t)(v20));
  t149 = ((int64_t)(v8));
  t150 = ((int64_t)(((int64_t)((uint64_t)(t148) * (uint64_t)(t149)))));
  t151 = ((int64_t)(((int64_t)((uint64_t)(t147) + (uint64_t)(t150)))));
  t152 = ((int64_t)(((int64_t)(v11))));
  v47 = ((int64_t)(((int64_t)((uint64_t)(t151) + (uint64_t)(t152)))));
  }
  }
  t153 = ((int64_t)(v47));
  t154 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(144))))));
  v52 = ((int64_t)(((int64_t)((uint64_t)(t153) * (uint64_t)(t154)))));
  t155 = ((int32_t)(v23));
  t156 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  t157 = ((t155) == (t156));
  if (t157) {
  {
  t158 = ((uint64_t)(v2));
  t159 = ((int64_t)(v52));
  t160 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t161 = ((int64_t)(((int64_t)((uint64_t)(t159) + (uint64_t)(t160)))));
  t162 = ((int64_t)(v21));
  t163 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t164 = ((int64_t)(((int64_t)((uint64_t)(t162) * (uint64_t)(t163)))));
  t165 = ((int64_t)(((int64_t)(v22))));
  t166 = ((int64_t)(((int64_t)((uint64_t)(t164) + (uint64_t)(t165)))));
  t167 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t168 = ((int64_t)(((int64_t)((uint64_t)(t166) * (uint64_t)(t167)))));
  t169 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t161) + (uint64_t)(t168)))))));
  t170 = ((int32_t)(v31));
#ifndef __cplusplus
  extern void (fg431_st_u16)(uint64_t, uint64_t, int32_t);
#endif
  fg431_st_u16(t158, t169, t170);
  t171 = ((uint64_t)(v2));
  t172 = ((int64_t)(v52));
  t173 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t174 = ((int64_t)(((int64_t)((uint64_t)(t172) + (uint64_t)(t173)))));
  t175 = ((int64_t)(v21));
  t176 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t177 = ((int64_t)(((int64_t)((uint64_t)(t175) * (uint64_t)(t176)))));
  t178 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t179 = ((int64_t)(((int64_t)((uint64_t)(t177) + (uint64_t)(t178)))));
  t180 = ((int64_t)(((int64_t)(v22))));
  t181 = ((int64_t)(((int64_t)((uint64_t)(t179) + (uint64_t)(t180)))));
  t182 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t183 = ((int64_t)(((int64_t)((uint64_t)(t181) * (uint64_t)(t182)))));
  t184 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t174) + (uint64_t)(t183)))))));
  t185 = ((int32_t)(v32));
#ifndef __cplusplus
  extern void (fg431_st_u16)(uint64_t, uint64_t, int32_t);
#endif
  fg431_st_u16(t171, t184, t185);
  }
  }
  t186 = ((int32_t)(v14));
  t187 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  t188 = ((t186) == (t187));
  if (t188) {
  {
  t189 = ((uint64_t)(v2));
  t190 = ((int64_t)(v52));
  t191 = ((int64_t)(v21));
  t192 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t193 = ((int64_t)(((int64_t)((uint64_t)(t191) * (uint64_t)(t192)))));
  t194 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t190) + (uint64_t)(t193)))))));
  t195 = ((int32_t)(v30));
  t196 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(8))))));
  t197 = ((int32_t)(((int32_t)((uint64_t)(t195) << ((uint64_t)(t196) & 31)))));
  t198 = ((int32_t)(v29));
  t199 = ((int32_t)(((t197) | (t198))));
#ifndef __cplusplus
  extern void (fg431_st_i32)(uint64_t, uint64_t, int32_t);
#endif
  fg431_st_i32(t189, t194, t199);
  }
  }
  t200 = ((int32_t)(v46));
  t201 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  v46 = ((int32_t)(((int32_t)((uint64_t)(t200) + (uint64_t)(t201)))));
  }
  goto label4;
label5:;
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto fg431d_done;
  }
fg431d_done:;
  return t0;
}
