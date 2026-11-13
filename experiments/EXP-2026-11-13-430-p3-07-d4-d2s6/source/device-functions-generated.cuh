#pragma once
#define FG180Q_D4_DIRECT fg180qd_gg_function_1
#define FG180Q_D4_SCATTER fg180qd_gg_function_23
#define FG180Q_DS4_DIRECT fg180qd_gg_function_24
#define FG180Q_DS4_SCATTER fg180qd_gg_function_25
#define FG180Q_D2S6_DIRECT fg180qd_gg_function_26
#define FG180Q_D2S6_SCATTER fg180qd_gg_function_27
static __device__ __forceinline__ void fg180qd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg180qd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg180qd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg180qd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg180qd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg180qd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ void fg180qd_gg_function_3(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ int64_t fg180qd_gg_function_5(uint64_t p0, uint64_t p1, int64_t base);
static __device__ __forceinline__ float fg180qd_gg_function_15(float p0, int32_t p1, int64_t base);
static __device__ __forceinline__ float fg180qd_gg_function_12(float p0, float p1, int64_t base);
static __device__ __forceinline__ float fg180qd_gg_function_13(float p0, int32_t p1, int64_t base);
static __device__ __forceinline__ void fg180qd_gg_function_2(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int32_t p18, int32_t p19, int64_t base);
static __device__ __forceinline__ void fg180qd_gg_function_27(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base);
static __device__ __forceinline__ void fg180qd_gg_function_26(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base);
static __device__ __forceinline__ void fg180qd_gg_function_25(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base);
static __device__ __forceinline__ void fg180qd_gg_function_24(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base);
static __device__ __forceinline__ void fg180qd_gg_function_23(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base);
static __device__ __forceinline__ void fg180qd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base);
static __device__ __forceinline__ void fg180qd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg180qd_gg_div(int64_t a, int64_t b) { if (!b) fg180qd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg180qd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg180qd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg180qd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg180qd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg180qd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg180qd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg180qd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg180qd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ void fg180qd_gg_function_3(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg180q_fail)(int64_t);
#endif
  fg180q_fail(t2);
  }
  }
  }
}
static __device__ __forceinline__ int64_t fg180qd_gg_function_5(uint64_t p0, uint64_t p1, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) uint64_t t1;
  _Alignas(8) uint64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) uint64_t v0;
  _Alignas(8) uint64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((uint64_t)(v1));
#ifndef __cplusplus
  extern int64_t (fg180q_ld_i32)(uint64_t, uint64_t);
#endif
  t3 = fg180q_ld_i32(t1, t2);
  t0 = ((int64_t)(t3));
  goto fg180qd_gg_done;
  }
fg180qd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg180qd_gg_function_15(float p0, int32_t p1, int64_t base) {
  _Alignas(8) float t0;
  _Alignas(8) int32_t t1;
  _Alignas(8) int32_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) float t4;
  _Alignas(8) float t5;
  _Alignas(8) uint32_t t6;
  _Alignas(8) int32_t t7;
  _Alignas(8) float t8;
  _Alignas(8) float t9;
  _Alignas(8) float t10;
  _Alignas(8) float t11;
  _Alignas(8) uint32_t t12;
  _Alignas(8) int32_t t13;
  _Alignas(8) float t14;
  _Alignas(8) float t15;
  _Alignas(8) float t16;
  _Alignas(8) float t17;
  _Alignas(8) uint32_t t18;
  _Alignas(8) int32_t t19;
  _Alignas(8) float t20;
  _Alignas(8) float t21;
  _Alignas(8) float t22;
  _Alignas(8) float t23;
  _Alignas(8) uint32_t t24;
  _Alignas(8) int32_t t25;
  _Alignas(8) float t26;
  _Alignas(8) float t27;
  _Alignas(8) float t28;
  _Alignas(8) float t29;
  _Alignas(8) uint32_t t30;
  _Alignas(8) int32_t t31;
  _Alignas(8) float t32;
  _Alignas(8) float t33;
  _Alignas(8) float v0;
  _Alignas(8) int32_t v1;
  _Alignas(8) float v2;
  v0 = p0;
  v1 = p1;
  {
  v2 = ((float)(v0));
  t1 = ((int32_t)(v1));
  t2 = ((int32_t)(((int64_t)UINT64_C(2))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = ((float)(v2));
  t5 = ((float)(v2));
  t6 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t7 = ((int32_t)(((int64_t)UINT64_C(2))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t8 = fg180q_shfl_xor(t5, t6, t7);
  t9 = ((float)(t8));
  v2 = ((float)(((t4) + (t9))));
  t10 = ((float)(v2));
  t11 = ((float)(v2));
  t12 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t13 = ((int32_t)(((int64_t)UINT64_C(1))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t14 = fg180q_shfl_xor(t11, t12, t13);
  t15 = ((float)(t14));
  v2 = ((float)(((t10) + (t15))));
  }
  } else {
  {
  t16 = ((float)(v2));
  t17 = ((float)(v2));
  t18 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t19 = ((int32_t)(((int64_t)UINT64_C(4))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t20 = fg180q_shfl_xor(t17, t18, t19);
  t21 = ((float)(t20));
  v2 = ((float)(((t16) + (t21))));
  t22 = ((float)(v2));
  t23 = ((float)(v2));
  t24 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t25 = ((int32_t)(((int64_t)UINT64_C(2))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t26 = fg180q_shfl_xor(t23, t24, t25);
  t27 = ((float)(t26));
  v2 = ((float)(((t22) + (t27))));
  t28 = ((float)(v2));
  t29 = ((float)(v2));
  t30 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t31 = ((int32_t)(((int64_t)UINT64_C(1))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t32 = fg180q_shfl_xor(t29, t30, t31);
  t33 = ((float)(t32));
  v2 = ((float)(((t28) + (t33))));
  }
  }
  t0 = ((float)(v2));
  goto fg180qd_gg_done;
  }
fg180qd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg180qd_gg_function_12(float p0, float p1, int64_t base) {
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
  goto fg180qd_gg_done;
  }
fg180qd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg180qd_gg_function_13(float p0, int32_t p1, int64_t base) {
  _Alignas(8) float t0;
  _Alignas(8) int32_t t1;
  _Alignas(8) int32_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) float t4;
  _Alignas(8) float t5;
  _Alignas(8) uint32_t t6;
  _Alignas(8) int32_t t7;
  _Alignas(8) float t8;
  _Alignas(8) float t9;
  _Alignas(8) float t10;
  _Alignas(8) float t11;
  _Alignas(8) uint32_t t12;
  _Alignas(8) int32_t t13;
  _Alignas(8) float t14;
  _Alignas(8) float t15;
  _Alignas(8) float t16;
  _Alignas(8) float t17;
  _Alignas(8) uint32_t t18;
  _Alignas(8) int32_t t19;
  _Alignas(8) float t20;
  _Alignas(8) float t21;
  _Alignas(8) float t22;
  _Alignas(8) float t23;
  _Alignas(8) uint32_t t24;
  _Alignas(8) int32_t t25;
  _Alignas(8) float t26;
  _Alignas(8) float t27;
  _Alignas(8) float t28;
  _Alignas(8) float t29;
  _Alignas(8) uint32_t t30;
  _Alignas(8) int32_t t31;
  _Alignas(8) float t32;
  _Alignas(8) float t33;
  _Alignas(8) float t34;
  _Alignas(8) float t35;
  _Alignas(8) uint32_t t36;
  _Alignas(8) int32_t t37;
  _Alignas(8) float t38;
  _Alignas(8) float t39;
  _Alignas(8) float t40;
  _Alignas(8) float t41;
  _Alignas(8) uint32_t t42;
  _Alignas(8) int32_t t43;
  _Alignas(8) float t44;
  _Alignas(8) float t45;
  _Alignas(8) float v0;
  _Alignas(8) int32_t v1;
  _Alignas(8) float v2;
  v0 = p0;
  v1 = p1;
  {
  v2 = ((float)(v0));
  t1 = ((int32_t)(v1));
  t2 = ((int32_t)(((int64_t)UINT64_C(2))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = ((float)(v2));
  t5 = ((float)(v2));
  t6 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t7 = ((int32_t)(((int64_t)UINT64_C(8))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t8 = fg180q_shfl_xor(t5, t6, t7);
  t9 = ((float)(t8));
  v2 = fg180qd_gg_function_12(t4, t9, base + 0);
  t10 = ((float)(v2));
  t11 = ((float)(v2));
  t12 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t13 = ((int32_t)(((int64_t)UINT64_C(4))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t14 = fg180q_shfl_xor(t11, t12, t13);
  t15 = ((float)(t14));
  v2 = fg180qd_gg_function_12(t10, t15, base + 0);
  t16 = ((float)(v2));
  t17 = ((float)(v2));
  t18 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t19 = ((int32_t)(((int64_t)UINT64_C(2))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t20 = fg180q_shfl_xor(t17, t18, t19);
  t21 = ((float)(t20));
  v2 = fg180qd_gg_function_12(t16, t21, base + 0);
  t22 = ((float)(v2));
  t23 = ((float)(v2));
  t24 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t25 = ((int32_t)(((int64_t)UINT64_C(1))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t26 = fg180q_shfl_xor(t23, t24, t25);
  t27 = ((float)(t26));
  v2 = fg180qd_gg_function_12(t22, t27, base + 0);
  }
  } else {
  {
  t28 = ((float)(v2));
  t29 = ((float)(v2));
  t30 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t31 = ((int32_t)(((int64_t)UINT64_C(4))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t32 = fg180q_shfl_xor(t29, t30, t31);
  t33 = ((float)(t32));
  v2 = fg180qd_gg_function_12(t28, t33, base + 0);
  t34 = ((float)(v2));
  t35 = ((float)(v2));
  t36 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t37 = ((int32_t)(((int64_t)UINT64_C(2))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t38 = fg180q_shfl_xor(t35, t36, t37);
  t39 = ((float)(t38));
  v2 = fg180qd_gg_function_12(t34, t39, base + 0);
  t40 = ((float)(v2));
  t41 = ((float)(v2));
  t42 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t43 = ((int32_t)(((int64_t)UINT64_C(1))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t44 = fg180q_shfl_xor(t41, t42, t43);
  t45 = ((float)(t44));
  v2 = fg180qd_gg_function_12(t40, t45, base + 0);
  }
  }
  t0 = ((float)(v2));
  goto fg180qd_gg_done;
  }
fg180qd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg180qd_gg_function_2(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int32_t p18, int32_t p19, int64_t base) {
  _Alignas(8) int32_t t0;
  _Alignas(8) int32_t t1;
  _Alignas(8) uint8_t t2;
  _Alignas(8) int32_t t3;
  _Alignas(8) int32_t t4;
  _Alignas(8) uint8_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) uint8_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) uint8_t t17;
  _Alignas(8) uint8_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) uint8_t t21;
  _Alignas(8) uint8_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) int64_t t24;
  _Alignas(8) uint8_t t25;
  _Alignas(8) uint8_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) uint8_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) int32_t t31;
  _Alignas(8) int32_t t32;
  _Alignas(8) uint8_t t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) int64_t t35;
  _Alignas(8) int32_t t36;
  _Alignas(8) int32_t t37;
  _Alignas(8) uint8_t t38;
  _Alignas(8) int32_t t39;
  _Alignas(8) int32_t t40;
  _Alignas(8) int32_t t41;
  _Alignas(8) int32_t t42;
  _Alignas(8) uint64_t t43;
  _Alignas(8) uint64_t t44;
  _Alignas(8) uint8_t t45;
  _Alignas(8) uint64_t t46;
  _Alignas(8) int64_t t47;
  _Alignas(8) int64_t t48;
  _Alignas(8) uint64_t t49;
  _Alignas(8) int64_t t50;
  _Alignas(8) int64_t t51;
  _Alignas(8) int64_t t52;
  _Alignas(8) int64_t t53;
  _Alignas(8) int64_t t54;
  _Alignas(8) int64_t t55;
  _Alignas(8) int64_t t56;
  _Alignas(8) int64_t t57;
  _Alignas(8) int64_t t58;
  _Alignas(8) int64_t t59;
  _Alignas(8) int64_t t60;
  _Alignas(8) int64_t t61;
  _Alignas(8) int64_t t62;
  _Alignas(8) int64_t t63;
  _Alignas(8) int64_t t64;
  _Alignas(8) int64_t t65;
  _Alignas(8) uint8_t t66;
  _Alignas(8) int64_t t67;
  _Alignas(8) int64_t t68;
  _Alignas(8) int64_t t69;
  _Alignas(8) int64_t t70;
  _Alignas(8) uint64_t t71;
  _Alignas(8) uint64_t t72;
  _Alignas(8) float t73;
  _Alignas(8) uint64_t t74;
  _Alignas(8) uint64_t t75;
  _Alignas(8) float t76;
  _Alignas(8) uint64_t t77;
  _Alignas(8) uint64_t t78;
  _Alignas(8) float t79;
  _Alignas(8) uint64_t t80;
  _Alignas(8) uint64_t t81;
  _Alignas(8) float t82;
  _Alignas(8) float t83;
  _Alignas(8) float t84;
  _Alignas(8) float t85;
  _Alignas(8) float t86;
  _Alignas(8) float t87;
  _Alignas(8) float t88;
  _Alignas(8) float t89;
  _Alignas(8) float t90;
  _Alignas(8) float t91;
  _Alignas(8) float t92;
  _Alignas(8) float t93;
  _Alignas(8) float t94;
  _Alignas(8) float t95;
  _Alignas(8) float t96;
  _Alignas(8) float t97;
  _Alignas(8) int32_t t98;
  _Alignas(8) int32_t t99;
  _Alignas(8) int32_t t100;
  _Alignas(8) uint8_t t101;
  _Alignas(8) float t102;
  _Alignas(8) float t103;
  _Alignas(8) float t104;
  _Alignas(8) float t105;
  _Alignas(8) float t106;
  _Alignas(8) float t107;
  _Alignas(8) float t108;
  _Alignas(8) int32_t t109;
  _Alignas(8) float t110;
  _Alignas(8) float t111;
  _Alignas(8) float t112;
  _Alignas(8) float t113;
  _Alignas(8) float t114;
  _Alignas(8) float t115;
  _Alignas(8) float t116;
  _Alignas(8) int32_t t117;
  _Alignas(8) float t118;
  _Alignas(8) float t119;
  _Alignas(8) float t120;
  _Alignas(8) float t121;
  _Alignas(8) float t122;
  _Alignas(8) int32_t t123;
  _Alignas(8) float t124;
  _Alignas(8) float t125;
  _Alignas(8) float t126;
  _Alignas(8) float t127;
  _Alignas(8) float t128;
  _Alignas(8) int32_t t129;
  _Alignas(8) float t130;
  _Alignas(8) float t131;
  _Alignas(8) float t132;
  _Alignas(8) float t133;
  _Alignas(8) float t134;
  _Alignas(8) int32_t t135;
  _Alignas(8) int32_t t136;
  _Alignas(8) int32_t t137;
  _Alignas(8) int32_t t138;
  _Alignas(8) int32_t t139;
  _Alignas(8) int32_t t140;
  _Alignas(8) int32_t t141;
  _Alignas(8) int32_t t142;
  _Alignas(8) int32_t t143;
  _Alignas(8) int32_t t144;
  _Alignas(8) int32_t t145;
  _Alignas(8) int32_t t146;
  _Alignas(8) int32_t t147;
  _Alignas(8) int32_t t148;
  _Alignas(8) int32_t t149;
  _Alignas(8) int32_t t150;
  _Alignas(8) int32_t t151;
  _Alignas(8) int32_t t152;
  _Alignas(8) int32_t t153;
  _Alignas(8) int32_t t154;
  _Alignas(8) int32_t t155;
  _Alignas(8) float t156;
  _Alignas(8) float t157;
  _Alignas(8) int32_t t158;
  _Alignas(8) int32_t t159;
  _Alignas(8) uint8_t t160;
  _Alignas(8) int32_t t161;
  _Alignas(8) int32_t t162;
  _Alignas(8) uint8_t t163;
  _Alignas(8) int32_t t164;
  _Alignas(8) int32_t t165;
  _Alignas(8) uint8_t t166;
  _Alignas(8) int64_t t167;
  _Alignas(8) int64_t t168;
  _Alignas(8) int64_t t169;
  _Alignas(8) int64_t t170;
  _Alignas(8) uint64_t t171;
  _Alignas(8) int64_t t172;
  _Alignas(8) int64_t t173;
  _Alignas(8) uint64_t t174;
  _Alignas(8) int64_t t175;
  _Alignas(8) int64_t t176;
  _Alignas(8) int64_t t177;
  _Alignas(8) int64_t t178;
  _Alignas(8) int32_t t179;
  _Alignas(8) int32_t t180;
  _Alignas(8) int32_t t181;
  _Alignas(8) int32_t t182;
  _Alignas(8) int32_t t183;
  _Alignas(8) int32_t t184;
  _Alignas(8) int64_t t185;
  _Alignas(8) int64_t t186;
  _Alignas(8) int64_t t187;
  _Alignas(8) int64_t t188;
  _Alignas(8) int64_t t189;
  _Alignas(8) int64_t t190;
  _Alignas(8) int64_t t191;
  _Alignas(8) int64_t t192;
  _Alignas(8) int64_t t193;
  _Alignas(8) int64_t t194;
  _Alignas(8) uint8_t t195;
  _Alignas(8) int64_t t196;
  _Alignas(8) int64_t t197;
  _Alignas(8) int64_t t198;
  _Alignas(8) uint64_t t199;
  _Alignas(8) int64_t t200;
  _Alignas(8) int64_t t201;
  _Alignas(8) int64_t t202;
  _Alignas(8) int64_t t203;
  _Alignas(8) uint64_t t204;
  _Alignas(8) int32_t t205;
  _Alignas(8) int32_t t206;
  _Alignas(8) int32_t t207;
  _Alignas(8) uint8_t t208;
  _Alignas(8) int64_t t209;
  _Alignas(8) int64_t t210;
  _Alignas(8) int64_t t211;
  _Alignas(8) int64_t t212;
  _Alignas(8) uint8_t t213;
  _Alignas(8) uint8_t t214;
  _Alignas(8) int64_t t215;
  _Alignas(8) int64_t t216;
  _Alignas(8) uint8_t t217;
  _Alignas(8) uint64_t t218;
  _Alignas(8) int64_t t219;
  _Alignas(8) int64_t t220;
  _Alignas(8) int64_t t221;
  _Alignas(8) int64_t t222;
  _Alignas(8) int64_t t223;
  _Alignas(8) int64_t t224;
  _Alignas(8) int64_t t225;
  _Alignas(8) int64_t t226;
  _Alignas(8) uint64_t t227;
  _Alignas(8) float t228;
  _Alignas(8) int64_t t229;
  _Alignas(8) int64_t t230;
  _Alignas(8) int64_t t231;
  _Alignas(8) int64_t t232;
  _Alignas(8) uint8_t t233;
  _Alignas(8) uint64_t t234;
  _Alignas(8) int64_t t235;
  _Alignas(8) int64_t t236;
  _Alignas(8) int64_t t237;
  _Alignas(8) int64_t t238;
  _Alignas(8) int64_t t239;
  _Alignas(8) int64_t t240;
  _Alignas(8) uint64_t t241;
  _Alignas(8) float t242;
  _Alignas(8) int64_t t243;
  _Alignas(8) int64_t t244;
  _Alignas(8) int64_t t245;
  _Alignas(8) int64_t t246;
  _Alignas(8) uint8_t t247;
  _Alignas(8) int32_t t248;
  _Alignas(8) int32_t t249;
  _Alignas(8) uint8_t t250;
  _Alignas(8) uint64_t t251;
  _Alignas(8) int64_t t252;
  _Alignas(8) int64_t t253;
  _Alignas(8) int64_t t254;
  _Alignas(8) int64_t t255;
  _Alignas(8) int64_t t256;
  _Alignas(8) int64_t t257;
  _Alignas(8) uint64_t t258;
  _Alignas(8) float t259;
  _Alignas(8) float t260;
  _Alignas(8) uint64_t t261;
  _Alignas(8) int64_t t262;
  _Alignas(8) int64_t t263;
  _Alignas(8) int64_t t264;
  _Alignas(8) int64_t t265;
  _Alignas(8) int64_t t266;
  _Alignas(8) int64_t t267;
  _Alignas(8) uint64_t t268;
  _Alignas(8) float t269;
  _Alignas(8) int32_t t270;
  _Alignas(8) int32_t t271;
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
  _Alignas(8) int32_t v19;
  _Alignas(8) int32_t v20;
  _Alignas(8) int32_t v21;
  _Alignas(8) int64_t v22;
  _Alignas(8) int64_t v23;
  _Alignas(8) int32_t v24;
  _Alignas(8) int32_t v25;
  _Alignas(8) int64_t v26;
  _Alignas(8) int64_t v27;
  _Alignas(8) int64_t v28;
  _Alignas(8) int32_t v29;
  _Alignas(8) int64_t v30;
  _Alignas(8) int64_t v31;
  _Alignas(8) float v32;
  _Alignas(8) float v33;
  _Alignas(8) float v34;
  _Alignas(8) float v35;
  _Alignas(8) int64_t v36;
  _Alignas(8) uint64_t v37;
  _Alignas(8) float v38;
  _Alignas(8) float v39;
  _Alignas(8) float v40;
  _Alignas(8) int32_t v41;
  _Alignas(8) int32_t v42;
  _Alignas(8) int32_t v43;
  _Alignas(8) int32_t v44;
  _Alignas(8) int32_t v45;
  _Alignas(8) int32_t v46;
  _Alignas(8) float v47;
  _Alignas(8) int32_t v48;
  _Alignas(8) int32_t v49;
  _Alignas(8) int64_t v50;
  _Alignas(8) int64_t v51;
  _Alignas(8) int64_t v52;
  _Alignas(8) int32_t v53;
  _Alignas(8) int64_t v54;
  _Alignas(8) int64_t v55;
  _Alignas(8) int64_t v56;
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
  v19 = p19;
  {
  t0 = ((int32_t)(v11));
  t1 = ((int32_t)(((int64_t)UINT64_C(2))));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  v20 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(64))))));
  }
  } else {
  {
  v20 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
  }
  }
  t3 = ((int32_t)(v11));
  t4 = ((int32_t)(((int64_t)UINT64_C(2))));
  t5 = ((t3) == (t4));
  if (t5) {
  {
  v21 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(16))))));
  }
  } else {
  {
  v21 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
  }
  }
  t6 = ((int64_t)(((int64_t)(v17))));
  t7 = ((int64_t)(((int64_t)(v14))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t6) * (uint64_t)(t7)))));
  t9 = ((int64_t)(((int64_t)(v16))));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  t11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  t12 = ((int64_t)(v22));
  t13 = ((int64_t)(v7));
  t14 = ((t12) < (t13));
  if (t14) {
  {
  t15 = ((int64_t)(v7));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((t15) > (t16));
  t18 = t17;
  if (t18) {
  t19 = ((int64_t)(v8));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t19) > (t20));
  }
  t21 = t18;
  t22 = t21;
  if (t22) {
  t23 = ((int64_t)(v9));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((t23) > (t24));
  }
  t25 = t22;
  t26 = t25;
  if (t26) {
  t27 = ((int64_t)(v3));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((t27) > (t28));
  }
  t29 = t26;
  t30 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg180qd_gg_function_3(t29, t30, base + 0);
  v23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((int32_t)(v12));
  t32 = ((int32_t)(((int64_t)UINT64_C(0))));
  t33 = ((t31) != (t32));
  if (t33) {
  {
  t34 = ((int64_t)(((int64_t)(v13))));
  t35 = ((int64_t)(v5));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t34) * (uint64_t)(t35)))));
  }
  } else {
  {
  v24 = ((int32_t)(v15));
  v25 = ((int32_t)(((int32_t)(v9))));
  v26 = ((int64_t)(((int64_t)UINT64_C(0))));
  v27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = ((int32_t)(v25));
  t37 = ((int32_t)(((int64_t)UINT64_C(1))));
  t38 = ((t36) == (t37));
  if (t38) {
  {
  v26 = ((int64_t)(((int64_t)UINT64_C(0))));
  v27 = ((int64_t)(((int64_t)(v24))));
  }
  } else {
  {
  t39 = ((int32_t)(v24));
  t40 = ((int32_t)(v25));
  v26 = ((int64_t)(((int64_t)(((int32_t)fg180qd_gg_mod(t39, t40))))));
  t41 = ((int32_t)(v24));
  t42 = ((int32_t)(v25));
  v27 = ((int64_t)(((int64_t)(((int32_t)fg180qd_gg_div(t41, t42))))));
  }
  }
  v28 = ((int64_t)(((int64_t)(v13))));
  t43 = ((uint64_t)(v1));
  t44 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t45 = ((t43) != (t44));
  if (t45) {
  {
  t46 = ((uint64_t)(v1));
  t47 = ((int64_t)(((int64_t)(v13))));
  t48 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t49 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t47) * (uint64_t)(t48)))))));
  v28 = fg180qd_gg_function_5(t46, t49, base + 0);
  }
  }
  t50 = ((int64_t)(v27));
  t51 = ((int64_t)(v6));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t50) * (uint64_t)(t51)))));
  t53 = ((int64_t)(v26));
  t54 = ((int64_t)(v5));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t53) * (uint64_t)(t54)))));
  t56 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t55)))));
  t57 = ((int64_t)(v28));
  t58 = ((int64_t)(v4));
  t59 = ((int64_t)(((int64_t)((uint64_t)(t57) * (uint64_t)(t58)))));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t56) + (uint64_t)(t59)))));
  }
  }
  t60 = ((int64_t)(v22));
  t61 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(7))))));
  v29 = ((int32_t)(((int32_t)(((t60) >> ((uint64_t)(t61) & 63))))));
  v30 = ((int64_t)(((int64_t)(v29))));
  t62 = ((int64_t)(v22));
  t63 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(127))))));
  v31 = ((int64_t)(((t62) & (t63))));
  v32 = ((float)(fg180qd_gg_double_bits(UINT64_C(0))));
  v33 = ((float)(fg180qd_gg_double_bits(UINT64_C(0))));
  v34 = ((float)(fg180qd_gg_double_bits(UINT64_C(0))));
  v35 = ((float)(fg180qd_gg_double_bits(UINT64_C(0))));
  t64 = ((int64_t)(v22));
  t65 = ((int64_t)(v3));
  t66 = ((t64) < (t65));
  if (t66) {
  {
  t67 = ((int64_t)(v23));
  t68 = ((int64_t)(v22));
  v36 = ((int64_t)(((int64_t)((uint64_t)(t67) + (uint64_t)(t68)))));
  t69 = ((int64_t)(v36));
  t70 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v37 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t69) * (uint64_t)(t70)))))));
  t71 = ((uint64_t)(v0));
  t72 = ((uint64_t)(v37));
#ifndef __cplusplus
  extern float (fg180q_ld_f4x)(uint64_t, uint64_t);
#endif
  t73 = fg180q_ld_f4x(t71, t72);
  v32 = ((float)(t73));
  t74 = ((uint64_t)(v0));
  t75 = ((uint64_t)(v37));
#ifndef __cplusplus
  extern float (fg180q_ld_f4y)(uint64_t, uint64_t);
#endif
  t76 = fg180q_ld_f4y(t74, t75);
  v33 = ((float)(t76));
  t77 = ((uint64_t)(v0));
  t78 = ((uint64_t)(v37));
#ifndef __cplusplus
  extern float (fg180q_ld_f4z)(uint64_t, uint64_t);
#endif
  t79 = fg180q_ld_f4z(t77, t78);
  v34 = ((float)(t79));
  t80 = ((uint64_t)(v0));
  t81 = ((uint64_t)(v37));
#ifndef __cplusplus
  extern float (fg180q_ld_f4w)(uint64_t, uint64_t);
#endif
  t82 = fg180q_ld_f4w(t80, t81);
  v35 = ((float)(t82));
  }
  }
  t83 = ((float)(v32));
#ifndef __cplusplus
  extern float (fg180q_fabs)(float);
#endif
  t84 = fg180q_fabs(t83);
  v38 = ((float)(t84));
  t85 = ((float)(v38));
  t86 = ((float)(v33));
#ifndef __cplusplus
  extern float (fg180q_fabs)(float);
#endif
  t87 = fg180q_fabs(t86);
  t88 = ((float)(t87));
  v38 = fg180qd_gg_function_12(t85, t88, base + 0);
  t89 = ((float)(v38));
  t90 = ((float)(v34));
#ifndef __cplusplus
  extern float (fg180q_fabs)(float);
#endif
  t91 = fg180q_fabs(t90);
  t92 = ((float)(t91));
  v38 = fg180qd_gg_function_12(t89, t92, base + 0);
  t93 = ((float)(v38));
  t94 = ((float)(v35));
#ifndef __cplusplus
  extern float (fg180q_fabs)(float);
#endif
  t95 = fg180q_fabs(t94);
  t96 = ((float)(t95));
  v38 = fg180qd_gg_function_12(t93, t96, base + 0);
  t97 = ((float)(v38));
  t98 = ((int32_t)(v11));
  v38 = fg180qd_gg_function_13(t97, t98, base + 0);
  v39 = ((float)(fg180qd_gg_double_bits(UINT64_C(0))));
  t99 = ((int32_t)(v11));
  t100 = ((int32_t)(((int64_t)UINT64_C(0))));
  t101 = ((t99) != (t100));
  if (t101) {
  {
  t102 = ((float)(v32));
  t103 = ((float)(v33));
  v39 = ((float)(((t102) + (t103))));
  t104 = ((float)(v39));
  t105 = ((float)(v34));
  v39 = ((float)(((t104) + (t105))));
  t106 = ((float)(v39));
  t107 = ((float)(v35));
  v39 = ((float)(((t106) + (t107))));
  t108 = ((float)(v39));
  t109 = ((int32_t)(v11));
  v39 = fg180qd_gg_function_15(t108, t109, base + 0);
  }
  }
  t110 = ((float)(fg180qd_gg_double_bits(UINT64_C(4638637247447433216))));
  t111 = ((float)(v38));
  v40 = ((float)(((t110) / (t111))));
#ifndef __cplusplus
  extern void (fg180q_pdl_sync)(void);
#endif
  fg180q_pdl_sync();
  t112 = ((float)(v32));
  t113 = ((float)(v40));
  t114 = ((float)(((t112) * (t113))));
#ifndef __cplusplus
  extern float (fg180q_round)(float);
#endif
  t115 = fg180q_round(t114);
  t116 = ((float)(t115));
#ifndef __cplusplus
  extern int32_t (fg180q_i32)(float);
#endif
  t117 = fg180q_i32(t116);
  v41 = ((int32_t)(t117));
  t118 = ((float)(v33));
  t119 = ((float)(v40));
  t120 = ((float)(((t118) * (t119))));
#ifndef __cplusplus
  extern float (fg180q_round)(float);
#endif
  t121 = fg180q_round(t120);
  t122 = ((float)(t121));
#ifndef __cplusplus
  extern int32_t (fg180q_i32)(float);
#endif
  t123 = fg180q_i32(t122);
  v42 = ((int32_t)(t123));
  t124 = ((float)(v34));
  t125 = ((float)(v40));
  t126 = ((float)(((t124) * (t125))));
#ifndef __cplusplus
  extern float (fg180q_round)(float);
#endif
  t127 = fg180q_round(t126);
  t128 = ((float)(t127));
#ifndef __cplusplus
  extern int32_t (fg180q_i32)(float);
#endif
  t129 = fg180q_i32(t128);
  v43 = ((int32_t)(t129));
  t130 = ((float)(v35));
  t131 = ((float)(v40));
  t132 = ((float)(((t130) * (t131))));
#ifndef __cplusplus
  extern float (fg180q_round)(float);
#endif
  t133 = fg180q_round(t132);
  t134 = ((float)(t133));
#ifndef __cplusplus
  extern int32_t (fg180q_i32)(float);
#endif
  t135 = fg180q_i32(t134);
  v44 = ((int32_t)(t135));
  v45 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(255))))));
  t136 = ((int32_t)(v41));
  t137 = ((int32_t)(v45));
  t138 = ((int32_t)(((t136) & (t137))));
  t139 = ((int32_t)(v42));
  t140 = ((int32_t)(v45));
  t141 = ((int32_t)(((t139) & (t140))));
  t142 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(8))))));
  t143 = ((int32_t)(((int32_t)((uint64_t)(t141) << ((uint64_t)(t142) & 31)))));
  t144 = ((int32_t)(((t138) | (t143))));
  t145 = ((int32_t)(v43));
  t146 = ((int32_t)(v45));
  t147 = ((int32_t)(((t145) & (t146))));
  t148 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(16))))));
  t149 = ((int32_t)(((int32_t)((uint64_t)(t147) << ((uint64_t)(t148) & 31)))));
  t150 = ((int32_t)(((t144) | (t149))));
  t151 = ((int32_t)(v44));
  t152 = ((int32_t)(v45));
  t153 = ((int32_t)(((t151) & (t152))));
  t154 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(24))))));
  t155 = ((int32_t)(((int32_t)((uint64_t)(t153) << ((uint64_t)(t154) & 31)))));
  v46 = ((int32_t)(((t150) | (t155))));
  t156 = ((float)(fg180qd_gg_double_bits(UINT64_C(4607182418800017408))));
  t157 = ((float)(v40));
  v47 = ((float)(((t156) / (t157))));
  t158 = ((int32_t)(v12));
  t159 = ((int32_t)(((int64_t)UINT64_C(0))));
  t160 = ((t158) != (t159));
  if (t160) {
  {
  v48 = ((int32_t)(v10));
  }
  } else {
  {
  v48 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  }
  }
  v49 = ((int32_t)(((int64_t)UINT64_C(0))));
label1:;
  t161 = ((int32_t)(v49));
  t162 = ((int32_t)(v48));
  t163 = ((t161) < (t162));
  if (!(t163)) goto label2;
  {
  v50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t164 = ((int32_t)(v12));
  t165 = ((int32_t)(((int64_t)UINT64_C(0))));
  t166 = ((t164) != (t165));
  if (t166) {
  {
  t167 = ((int64_t)(((int64_t)(v13))));
  t168 = ((int64_t)(((int64_t)(v10))));
  t169 = ((int64_t)(((int64_t)((uint64_t)(t167) * (uint64_t)(t168)))));
  t170 = ((int64_t)(((int64_t)(v49))));
  v51 = ((int64_t)(((int64_t)((uint64_t)(t169) + (uint64_t)(t170)))));
  t171 = ((uint64_t)(v1));
  t172 = ((int64_t)(v51));
  t173 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t174 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t172) * (uint64_t)(t173)))))));
  v52 = fg180qd_gg_function_5(t171, t174, base + 0);
  t175 = ((int64_t)(v30));
  t176 = ((int64_t)(v8));
  t177 = ((int64_t)(((int64_t)((uint64_t)(t175) * (uint64_t)(t176)))));
  t178 = ((int64_t)(v52));
  v50 = ((int64_t)(((int64_t)((uint64_t)(t177) + (uint64_t)(t178)))));
  }
  } else {
  {
  t179 = ((int32_t)(v18));
  t180 = ((int32_t)(v19));
  t181 = ((int32_t)(((int32_t)((uint64_t)(t179) * (uint64_t)(t180)))));
  t182 = ((int32_t)(v17));
  t183 = ((int32_t)(((int32_t)((uint64_t)(t181) * (uint64_t)(t182)))));
  t184 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(7))))));
  v53 = ((int32_t)(((t183) >> ((uint64_t)(t184) & 31))));
  v54 = ((int64_t)(((int64_t)(v53))));
  t185 = ((int64_t)(((int64_t)(v15))));
  t186 = ((int64_t)(v54));
  v55 = ((int64_t)(((int64_t)((uint64_t)(t185) * (uint64_t)(t186)))));
  t187 = ((int64_t)(v55));
  t188 = ((int64_t)(v30));
  t189 = ((int64_t)(v8));
  t190 = ((int64_t)(((int64_t)((uint64_t)(t188) * (uint64_t)(t189)))));
  t191 = ((int64_t)(((int64_t)((uint64_t)(t187) + (uint64_t)(t190)))));
  t192 = ((int64_t)(((int64_t)(v13))));
  v50 = ((int64_t)(((int64_t)((uint64_t)(t191) + (uint64_t)(t192)))));
  }
  }
  t193 = ((int64_t)(v50));
  t194 = ((int64_t)(((int64_t)UINT64_C(0))));
  t195 = ((t193) >= (t194));
  t196 = ((int64_t)(((int64_t)UINT64_C(11))));
  fg180qd_gg_function_3(t195, t196, base + 0);
  t197 = ((int64_t)(v50));
  t198 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(144))))));
  v56 = ((int64_t)(((int64_t)((uint64_t)(t197) * (uint64_t)(t198)))));
  t199 = ((uint64_t)(v2));
  t200 = ((int64_t)(v56));
  t201 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t202 = ((int64_t)(((int64_t)((uint64_t)(t200) + (uint64_t)(t201)))));
  t203 = ((int64_t)(v31));
  t204 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t202) + (uint64_t)(t203)))))));
  t205 = ((int32_t)(v46));
#ifndef __cplusplus
  extern void (fg180q_st_i32)(uint64_t, uint64_t, int32_t);
#endif
  fg180q_st_i32(t199, t204, t205);
  t206 = ((int32_t)(v11));
  t207 = ((int32_t)(((int64_t)UINT64_C(2))));
  t208 = ((t206) == (t207));
  if (t208) {
  {
  t209 = ((int64_t)(v31));
  t210 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(15))))));
  t211 = ((int64_t)(((t209) & (t210))));
  t212 = ((int64_t)(((int64_t)UINT64_C(0))));
  t213 = ((t211) == (t212));
  t214 = t213;
  if (t214) {
  t215 = ((int64_t)(v31));
  t216 = ((int64_t)(((int64_t)UINT64_C(96))));
  t214 = ((t215) < (t216));
  }
  t217 = t214;
  if (t217) {
  {
  t218 = ((uint64_t)(v2));
  t219 = ((int64_t)(v56));
  t220 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t221 = ((int64_t)(v31));
  t222 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t223 = ((int64_t)(((t221) >> ((uint64_t)(t222) & 63))));
  t224 = ((int64_t)(((int64_t)((uint64_t)(t220) + (uint64_t)(t223)))));
  t225 = ((int64_t)(((int64_t)UINT64_C(2))));
  t226 = ((int64_t)(((int64_t)((uint64_t)(t224) * (uint64_t)(t225)))));
  t227 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t219) + (uint64_t)(t226)))))));
  t228 = ((float)(v39));
#ifndef __cplusplus
  extern void (fg180q_st_half)(uint64_t, uint64_t, float);
#endif
  fg180q_st_half(t218, t227, t228);
  t229 = ((int64_t)(v31));
  t230 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(63))))));
  t231 = ((int64_t)(((t229) & (t230))));
  t232 = ((int64_t)(((int64_t)UINT64_C(0))));
  t233 = ((t231) == (t232));
  if (t233) {
  {
  t234 = ((uint64_t)(v2));
  t235 = ((int64_t)(v56));
  t236 = ((int64_t)(v31));
  t237 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(6))))));
  t238 = ((int64_t)(((t236) >> ((uint64_t)(t237) & 63))));
  t239 = ((int64_t)(((int64_t)UINT64_C(2))));
  t240 = ((int64_t)(((int64_t)((uint64_t)(t238) * (uint64_t)(t239)))));
  t241 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t235) + (uint64_t)(t240)))))));
  t242 = ((float)(v47));
#ifndef __cplusplus
  extern void (fg180q_st_half)(uint64_t, uint64_t, float);
#endif
  fg180q_st_half(t234, t241, t242);
  }
  }
  }
  }
  }
  } else {
  {
  t243 = ((int64_t)(v31));
  t244 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(31))))));
  t245 = ((int64_t)(((t243) & (t244))));
  t246 = ((int64_t)(((int64_t)UINT64_C(0))));
  t247 = ((t245) == (t246));
  if (t247) {
  {
  t248 = ((int32_t)(v11));
  t249 = ((int32_t)(((int64_t)UINT64_C(1))));
  t250 = ((t248) == (t249));
  if (t250) {
  {
  t251 = ((uint64_t)(v2));
  t252 = ((int64_t)(v56));
  t253 = ((int64_t)(v31));
  t254 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t255 = ((int64_t)(((t253) >> ((uint64_t)(t254) & 63))));
  t256 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t257 = ((int64_t)(((int64_t)((uint64_t)(t255) * (uint64_t)(t256)))));
  t258 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t252) + (uint64_t)(t257)))))));
  t259 = ((float)(v47));
  t260 = ((float)(v39));
#ifndef __cplusplus
  extern void (fg180q_st_half2)(uint64_t, uint64_t, float, float);
#endif
  fg180q_st_half2(t251, t258, t259, t260);
  }
  } else {
  {
  t261 = ((uint64_t)(v2));
  t262 = ((int64_t)(v56));
  t263 = ((int64_t)(v31));
  t264 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t265 = ((int64_t)(((t263) >> ((uint64_t)(t264) & 63))));
  t266 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t267 = ((int64_t)(((int64_t)((uint64_t)(t265) * (uint64_t)(t266)))));
  t268 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t262) + (uint64_t)(t267)))))));
  t269 = ((float)(v47));
#ifndef __cplusplus
  extern void (fg180q_st_f32)(uint64_t, uint64_t, float);
#endif
  fg180q_st_f32(t261, t268, t269);
  }
  }
  }
  }
  }
  }
  t270 = v49;
  t271 = ((int32_t)(((int64_t)UINT64_C(1))));
  v49 = ((int32_t)((uint64_t)(t270) + (uint64_t)(t271)));
  }
  goto label1;
label2:;
  }
  }
  }
}
static __device__ __forceinline__ void fg180qd_gg_function_27(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base) {
  _Alignas(8) uint64_t t0;
  _Alignas(8) uint64_t t1;
  _Alignas(8) uint64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int32_t t10;
  _Alignas(8) int32_t t11;
  _Alignas(8) int32_t t12;
  _Alignas(8) int32_t t13;
  _Alignas(8) int32_t t14;
  _Alignas(8) int32_t t15;
  _Alignas(8) int32_t t16;
  _Alignas(8) int32_t t17;
  _Alignas(8) int32_t t18;
  _Alignas(8) int32_t t19;
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
  {
  t0 = ((uint64_t)(v0));
  t1 = ((uint64_t)(v1));
  t2 = ((uint64_t)(v2));
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(v5));
  t6 = ((int64_t)(v6));
  t7 = ((int64_t)(v7));
  t8 = ((int64_t)(v8));
  t9 = ((int64_t)(v9));
  t10 = ((int32_t)(v10));
  t11 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(2))))));
  t12 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  t13 = ((int32_t)(v11));
  t14 = ((int32_t)(v12));
  t15 = ((int32_t)(v13));
  t16 = ((int32_t)(v14));
  t17 = ((int32_t)(v15));
  t18 = ((int32_t)(v16));
  t19 = ((int32_t)(v17));
  fg180qd_gg_function_2(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, t19, base + 0);
  }
}
static __device__ __forceinline__ void fg180qd_gg_function_26(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base) {
  _Alignas(8) uint64_t t0;
  _Alignas(8) uint64_t t1;
  _Alignas(8) uint64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int32_t t10;
  _Alignas(8) int32_t t11;
  _Alignas(8) int32_t t12;
  _Alignas(8) int32_t t13;
  _Alignas(8) int32_t t14;
  _Alignas(8) int32_t t15;
  _Alignas(8) int32_t t16;
  _Alignas(8) int32_t t17;
  _Alignas(8) int32_t t18;
  _Alignas(8) int32_t t19;
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
  {
  t0 = ((uint64_t)(v0));
  t1 = ((uint64_t)(v1));
  t2 = ((uint64_t)(v2));
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(v5));
  t6 = ((int64_t)(v6));
  t7 = ((int64_t)(v7));
  t8 = ((int64_t)(v8));
  t9 = ((int64_t)(v9));
  t10 = ((int32_t)(v10));
  t11 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(2))))));
  t12 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  t13 = ((int32_t)(v11));
  t14 = ((int32_t)(v12));
  t15 = ((int32_t)(v13));
  t16 = ((int32_t)(v14));
  t17 = ((int32_t)(v15));
  t18 = ((int32_t)(v16));
  t19 = ((int32_t)(v17));
  fg180qd_gg_function_2(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, t19, base + 0);
  }
}
static __device__ __forceinline__ void fg180qd_gg_function_25(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base) {
  _Alignas(8) uint64_t t0;
  _Alignas(8) uint64_t t1;
  _Alignas(8) uint64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int32_t t10;
  _Alignas(8) int32_t t11;
  _Alignas(8) int32_t t12;
  _Alignas(8) int32_t t13;
  _Alignas(8) int32_t t14;
  _Alignas(8) int32_t t15;
  _Alignas(8) int32_t t16;
  _Alignas(8) int32_t t17;
  _Alignas(8) int32_t t18;
  _Alignas(8) int32_t t19;
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
  {
  t0 = ((uint64_t)(v0));
  t1 = ((uint64_t)(v1));
  t2 = ((uint64_t)(v2));
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(v5));
  t6 = ((int64_t)(v6));
  t7 = ((int64_t)(v7));
  t8 = ((int64_t)(v8));
  t9 = ((int64_t)(v9));
  t10 = ((int32_t)(v10));
  t11 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  t12 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  t13 = ((int32_t)(v11));
  t14 = ((int32_t)(v12));
  t15 = ((int32_t)(v13));
  t16 = ((int32_t)(v14));
  t17 = ((int32_t)(v15));
  t18 = ((int32_t)(v16));
  t19 = ((int32_t)(v17));
  fg180qd_gg_function_2(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, t19, base + 0);
  }
}
static __device__ __forceinline__ void fg180qd_gg_function_24(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base) {
  _Alignas(8) uint64_t t0;
  _Alignas(8) uint64_t t1;
  _Alignas(8) uint64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int32_t t10;
  _Alignas(8) int32_t t11;
  _Alignas(8) int32_t t12;
  _Alignas(8) int32_t t13;
  _Alignas(8) int32_t t14;
  _Alignas(8) int32_t t15;
  _Alignas(8) int32_t t16;
  _Alignas(8) int32_t t17;
  _Alignas(8) int32_t t18;
  _Alignas(8) int32_t t19;
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
  {
  t0 = ((uint64_t)(v0));
  t1 = ((uint64_t)(v1));
  t2 = ((uint64_t)(v2));
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(v5));
  t6 = ((int64_t)(v6));
  t7 = ((int64_t)(v7));
  t8 = ((int64_t)(v8));
  t9 = ((int64_t)(v9));
  t10 = ((int32_t)(v10));
  t11 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  t12 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  t13 = ((int32_t)(v11));
  t14 = ((int32_t)(v12));
  t15 = ((int32_t)(v13));
  t16 = ((int32_t)(v14));
  t17 = ((int32_t)(v15));
  t18 = ((int32_t)(v16));
  t19 = ((int32_t)(v17));
  fg180qd_gg_function_2(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, t19, base + 0);
  }
}
static __device__ __forceinline__ void fg180qd_gg_function_23(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base) {
  _Alignas(8) uint64_t t0;
  _Alignas(8) uint64_t t1;
  _Alignas(8) uint64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int32_t t10;
  _Alignas(8) int32_t t11;
  _Alignas(8) int32_t t12;
  _Alignas(8) int32_t t13;
  _Alignas(8) int32_t t14;
  _Alignas(8) int32_t t15;
  _Alignas(8) int32_t t16;
  _Alignas(8) int32_t t17;
  _Alignas(8) int32_t t18;
  _Alignas(8) int32_t t19;
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
  {
  t0 = ((uint64_t)(v0));
  t1 = ((uint64_t)(v1));
  t2 = ((uint64_t)(v2));
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(v5));
  t6 = ((int64_t)(v6));
  t7 = ((int64_t)(v7));
  t8 = ((int64_t)(v8));
  t9 = ((int64_t)(v9));
  t10 = ((int32_t)(v10));
  t11 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  t12 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  t13 = ((int32_t)(v11));
  t14 = ((int32_t)(v12));
  t15 = ((int32_t)(v13));
  t16 = ((int32_t)(v14));
  t17 = ((int32_t)(v15));
  t18 = ((int32_t)(v16));
  t19 = ((int32_t)(v17));
  fg180qd_gg_function_2(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, t19, base + 0);
  }
}
static __device__ __forceinline__ void fg180qd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base) {
  _Alignas(8) uint64_t t0;
  _Alignas(8) uint64_t t1;
  _Alignas(8) uint64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int32_t t10;
  _Alignas(8) int32_t t11;
  _Alignas(8) int32_t t12;
  _Alignas(8) int32_t t13;
  _Alignas(8) int32_t t14;
  _Alignas(8) int32_t t15;
  _Alignas(8) int32_t t16;
  _Alignas(8) int32_t t17;
  _Alignas(8) int32_t t18;
  _Alignas(8) int32_t t19;
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
  {
  t0 = ((uint64_t)(v0));
  t1 = ((uint64_t)(v1));
  t2 = ((uint64_t)(v2));
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(v5));
  t6 = ((int64_t)(v6));
  t7 = ((int64_t)(v7));
  t8 = ((int64_t)(v8));
  t9 = ((int64_t)(v9));
  t10 = ((int32_t)(v10));
  t11 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  t12 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  t13 = ((int32_t)(v11));
  t14 = ((int32_t)(v12));
  t15 = ((int32_t)(v13));
  t16 = ((int32_t)(v14));
  t17 = ((int32_t)(v15));
  t18 = ((int32_t)(v16));
  t19 = ((int32_t)(v17));
  fg180qd_gg_function_2(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, t19, base + 0);
  }
}
