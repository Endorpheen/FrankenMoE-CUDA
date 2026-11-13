#pragma once
static __device__ __forceinline__ void fg272dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg272dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg272dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg272dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg272dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg272dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ int64_t fg272dd_gg_function_14(int64_t p0, int64_t base);
static __device__ __forceinline__ float fg272dd_gg_function_12(uint32_t p0, int64_t base);
static __device__ __forceinline__ void fg272dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg272dd_gg_function_15(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base);
static __device__ __forceinline__ void fg272dd_gg_function_11(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base);
static __device__ __forceinline__ void fg272dd_gg_function_9(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base);
static __device__ __forceinline__ void fg272dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base);
static __device__ __forceinline__ void fg272dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg272dd_gg_div(int64_t a, int64_t b) { if (!b) fg272dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg272dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg272dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg272dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg272dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg272dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg272dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg272dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg272dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ int64_t fg272dd_gg_function_14(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  int64_t t10;
  int64_t t11;
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  int64_t t22;
  int64_t t23;
  uint8_t t24;
  int64_t t25;
  int64_t t26;
  uint8_t t27;
  int64_t t28;
  int64_t t29;
  uint8_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  uint8_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint8_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  uint8_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  uint8_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  uint8_t t50;
  int64_t t51;
  int64_t t52;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto fg272dd_gg_done;
  }
  }
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto fg272dd_gg_done;
  }
  }
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  goto fg272dd_gg_done;
  }
  }
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t12 = ((t10) == (t11));
  if (t12) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  goto fg272dd_gg_done;
  }
  }
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t15 = ((t13) == (t14));
  if (t15) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  goto fg272dd_gg_done;
  }
  }
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t18 = ((t16) == (t17));
  if (t18) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(6))))));
  goto fg272dd_gg_done;
  }
  }
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(6))))));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  goto fg272dd_gg_done;
  }
  }
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(7))))));
  t24 = ((t22) == (t23));
  if (t24) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(12))))));
  goto fg272dd_gg_done;
  }
  }
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t27 = ((t25) == (t26));
  if (t27) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto fg272dd_gg_done;
  }
  }
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(9))))));
  t30 = ((t28) == (t29));
  if (t30) {
  {
  t31 = ((int64_t)(((int64_t)UINT64_C(1))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t31)))));
  goto fg272dd_gg_done;
  }
  }
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(10))))));
  t34 = ((t32) == (t33));
  if (t34) {
  {
  t35 = ((int64_t)(((int64_t)UINT64_C(2))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t35)))));
  goto fg272dd_gg_done;
  }
  }
  t36 = ((int64_t)(v0));
  t37 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(11))))));
  t38 = ((t36) == (t37));
  if (t38) {
  {
  t39 = ((int64_t)(((int64_t)UINT64_C(3))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t39)))));
  goto fg272dd_gg_done;
  }
  }
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(12))))));
  t42 = ((t40) == (t41));
  if (t42) {
  {
  t43 = ((int64_t)(((int64_t)UINT64_C(4))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t43)))));
  goto fg272dd_gg_done;
  }
  }
  t44 = ((int64_t)(v0));
  t45 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(13))))));
  t46 = ((t44) == (t45));
  if (t46) {
  {
  t47 = ((int64_t)(((int64_t)UINT64_C(6))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t47)))));
  goto fg272dd_gg_done;
  }
  }
  t48 = ((int64_t)(v0));
  t49 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(14))))));
  t50 = ((t48) == (t49));
  if (t50) {
  {
  t51 = ((int64_t)(((int64_t)UINT64_C(8))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t51)))));
  goto fg272dd_gg_done;
  }
  }
  t52 = ((int64_t)(((int64_t)UINT64_C(12))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t52)))));
  }
fg272dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg272dd_gg_function_12(uint32_t p0, int64_t base) {
  float t0;
  uint32_t t1;
  uint32_t t2;
  uint8_t t3;
  uint32_t t4;
  uint32_t t5;
  uint32_t t6;
  float t7;
  uint32_t v0;
  uint32_t v1;
  uint32_t v2;
  v0 = p0;
  {
  v1 = ((uint32_t)(((int64_t)UINT64_C(0))));
  v2 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(23))))));
  t1 = ((uint32_t)(v0));
  t2 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  v1 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4194304))))));
  }
  } else {
  {
  t4 = ((uint32_t)(v0));
  t5 = ((uint32_t)(v2));
  v1 = ((uint32_t)(((uint32_t)((uint64_t)(t4) << ((uint64_t)(t5) & 31)))));
  }
  }
  t6 = ((uint32_t)(v1));
#ifndef __cplusplus
  extern float (fg272d_u32f)(uint32_t);
#endif
  t7 = fg272d_u32f(t6);
  t0 = ((float)(t7));
  }
fg272dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg272dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg272d_fail)(int64_t);
#endif
  fg272d_fail(t2);
  }
  }
  }
fg272dd_gg_done:;
}
static __device__ __forceinline__ void fg272dd_gg_function_15(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint64_t t28;
  uint64_t t29;
  uint32_t t30;
  uint32_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  uint64_t t40;
  int64_t t41;
  int64_t t42;
  uint64_t t43;
  uint32_t t44;
  uint32_t t45;
  uint32_t t46;
  int64_t t47;
  uint32_t t48;
  int64_t t49;
  int64_t t50;
  uint64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  uint64_t t56;
  float t57;
  float t58;
  float t59;
  float t60;
  float t61;
  uint64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  uint64_t t69;
  float t70;
  float t71;
  float t72;
  float t73;
  float t74;
  int64_t t75;
  int64_t t76;
  uint64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  uint32_t v9;
  float v10;
  int64_t v11;
  int64_t v12;
  uint32_t v13;
  int64_t v14;
  int64_t v15;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v3));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t3 = ((t4) < (t5));
  }
  t6 = t3;
  t7 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg272dd_gg_function_2(t6, t7, base + 0);
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)UINT64_C(8))));
  v4 = ((int64_t)(((int64_t)fg272dd_gg_mod(t8, t9))));
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(((int64_t)UINT64_C(8))));
  v5 = ((int64_t)(((int64_t)fg272dd_gg_div(t10, t11))));
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)UINT64_C(8))));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  t15 = ((int64_t)(v4));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t14) + (uint64_t)(t15)))));
  t16 = ((int64_t)(v6));
  t17 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(17))))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t17)))));
  t18 = ((int64_t)(v2));
  t19 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(256))))));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))));
  t21 = ((int64_t)(((int64_t)UINT64_C(32))));
  t22 = ((int64_t)(v4));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) * (uint64_t)(t22)))));
  t24 = ((int64_t)(((int64_t)((uint64_t)(t20) + (uint64_t)(t23)))));
  t25 = ((int64_t)(((int64_t)UINT64_C(4))));
  t26 = ((int64_t)(v5));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t25) * (uint64_t)(t26)))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t24) + (uint64_t)(t27)))));
  t28 = ((uint64_t)(v0));
  t29 = ((uint64_t)(((uint64_t)(v7))));
#ifndef __cplusplus
  extern uint32_t (fg272d_ld_u8)(uint64_t, uint64_t);
#endif
  t30 = fg272d_ld_u8(t28, t29);
  v9 = ((uint32_t)(t30));
  t31 = ((uint32_t)(v9));
  v10 = fg272dd_gg_function_12(t31, base + 0);
  t32 = ((int64_t)(v7));
  t33 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) + (uint64_t)(t33)))));
  t35 = ((int64_t)(((int64_t)UINT64_C(4))));
  t36 = ((int64_t)(v5));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) * (uint64_t)(t36)))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t34) + (uint64_t)(t37)))));
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t38 = ((int64_t)(v12));
  t39 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  if (!(((t38) < (t39)))) goto label2;
  {
  t40 = ((uint64_t)(v0));
  t41 = ((int64_t)(v11));
  t42 = ((int64_t)(v12));
  t43 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t41) + (uint64_t)(t42)))))));
#ifndef __cplusplus
  extern uint32_t (fg272d_ld_u8)(uint64_t, uint64_t);
#endif
  t44 = fg272d_ld_u8(t40, t43);
  v13 = ((uint32_t)(t44));
  t45 = ((uint32_t)(v13));
  t46 = ((uint32_t)(((int64_t)UINT64_C(15))));
  t47 = ((int64_t)(((int64_t)(((t45) & (t46))))));
  v14 = fg272dd_gg_function_14(t47, base + 0);
  t48 = ((uint32_t)(v13));
  t49 = ((int64_t)(((int64_t)UINT64_C(4))));
  t50 = ((int64_t)(((int64_t)(((t48) >> ((uint64_t)(t49) & 31))))));
  v15 = fg272dd_gg_function_14(t50, base + 0);
  t51 = ((uint64_t)(v1));
  t52 = ((int64_t)(v8));
  t53 = ((int64_t)(v12));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t53)))));
  t55 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t56 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t54) * (uint64_t)(t55)))))));
  t57 = ((float)(v10));
  t58 = ((float)(((float)(v14))));
  t59 = ((float)(((t57) * (t58))));
  t60 = ((float)(((float)(fg272dd_gg_double_bits(UINT64_C(4602678819172646912))))));
  t61 = ((float)(((t59) * (t60))));
#ifndef __cplusplus
  extern void (fg272d_st_bf16)(uint64_t, uint64_t, float);
#endif
  fg272d_st_bf16(t51, t56, t61);
  t62 = ((uint64_t)(v1));
  t63 = ((int64_t)(v8));
  t64 = ((int64_t)(v12));
  t65 = ((int64_t)(((int64_t)((uint64_t)(t63) + (uint64_t)(t64)))));
  t66 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t67 = ((int64_t)(((int64_t)((uint64_t)(t65) + (uint64_t)(t66)))));
  t68 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t69 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t67) * (uint64_t)(t68)))))));
  t70 = ((float)(v10));
  t71 = ((float)(((float)(v15))));
  t72 = ((float)(((t70) * (t71))));
  t73 = ((float)(((float)(fg272dd_gg_double_bits(UINT64_C(4602678819172646912))))));
  t74 = ((float)(((t72) * (t73))));
#ifndef __cplusplus
  extern void (fg272d_st_bf16)(uint64_t, uint64_t, float);
#endif
  fg272d_st_bf16(t62, t69, t74);
  t75 = ((int64_t)(v12));
  t76 = ((int64_t)(((int64_t)UINT64_C(1))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t75) + (uint64_t)(t76)))));
  }
  goto label1;
label2:;
  }
fg272dd_gg_done:;
}
static __device__ __forceinline__ void fg272dd_gg_function_11(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint64_t t28;
  uint64_t t29;
  uint32_t t30;
  uint32_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  uint64_t t40;
  int64_t t41;
  int64_t t42;
  uint64_t t43;
  uint32_t t44;
  uint32_t t45;
  uint32_t t46;
  int64_t t47;
  uint32_t t48;
  int64_t t49;
  int64_t t50;
  uint64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  uint64_t t56;
  float t57;
  float t58;
  float t59;
  float t60;
  float t61;
  uint64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  uint64_t t69;
  float t70;
  float t71;
  float t72;
  float t73;
  float t74;
  int64_t t75;
  int64_t t76;
  uint64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  uint32_t v9;
  float v10;
  int64_t v11;
  int64_t v12;
  uint32_t v13;
  int64_t v14;
  int64_t v15;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v3));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t3 = ((t4) < (t5));
  }
  t6 = t3;
  t7 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg272dd_gg_function_2(t6, t7, base + 0);
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)UINT64_C(8))));
  v4 = ((int64_t)(((int64_t)fg272dd_gg_mod(t8, t9))));
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(((int64_t)UINT64_C(8))));
  v5 = ((int64_t)(((int64_t)fg272dd_gg_div(t10, t11))));
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)UINT64_C(8))));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  t15 = ((int64_t)(v4));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t14) + (uint64_t)(t15)))));
  t16 = ((int64_t)(v6));
  t17 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(17))))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t17)))));
  t18 = ((int64_t)(v2));
  t19 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(256))))));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))));
  t21 = ((int64_t)(((int64_t)UINT64_C(32))));
  t22 = ((int64_t)(v4));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) * (uint64_t)(t22)))));
  t24 = ((int64_t)(((int64_t)((uint64_t)(t20) + (uint64_t)(t23)))));
  t25 = ((int64_t)(((int64_t)UINT64_C(4))));
  t26 = ((int64_t)(v5));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t25) * (uint64_t)(t26)))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t24) + (uint64_t)(t27)))));
  t28 = ((uint64_t)(v0));
  t29 = ((uint64_t)(((uint64_t)(v7))));
#ifndef __cplusplus
  extern uint32_t (fg272d_ld_u8)(uint64_t, uint64_t);
#endif
  t30 = fg272d_ld_u8(t28, t29);
  v9 = ((uint32_t)(t30));
  t31 = ((uint32_t)(v9));
  v10 = fg272dd_gg_function_12(t31, base + 0);
  t32 = ((int64_t)(v7));
  t33 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) + (uint64_t)(t33)))));
  t35 = ((int64_t)(((int64_t)UINT64_C(4))));
  t36 = ((int64_t)(v5));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) * (uint64_t)(t36)))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t34) + (uint64_t)(t37)))));
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t38 = ((int64_t)(v12));
  t39 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  if (!(((t38) < (t39)))) goto label2;
  {
  t40 = ((uint64_t)(v0));
  t41 = ((int64_t)(v11));
  t42 = ((int64_t)(v12));
  t43 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t41) + (uint64_t)(t42)))))));
#ifndef __cplusplus
  extern uint32_t (fg272d_ld_u8)(uint64_t, uint64_t);
#endif
  t44 = fg272d_ld_u8(t40, t43);
  v13 = ((uint32_t)(t44));
  t45 = ((uint32_t)(v13));
  t46 = ((uint32_t)(((int64_t)UINT64_C(15))));
  t47 = ((int64_t)(((int64_t)(((t45) & (t46))))));
  v14 = fg272dd_gg_function_14(t47, base + 0);
  t48 = ((uint32_t)(v13));
  t49 = ((int64_t)(((int64_t)UINT64_C(4))));
  t50 = ((int64_t)(((int64_t)(((t48) >> ((uint64_t)(t49) & 31))))));
  v15 = fg272dd_gg_function_14(t50, base + 0);
  t51 = ((uint64_t)(v1));
  t52 = ((int64_t)(v8));
  t53 = ((int64_t)(v12));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t53)))));
  t55 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t56 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t54) * (uint64_t)(t55)))))));
  t57 = ((float)(v10));
  t58 = ((float)(((float)(v14))));
  t59 = ((float)(((t57) * (t58))));
  t60 = ((float)(((float)(fg272dd_gg_double_bits(UINT64_C(4602678819172646912))))));
  t61 = ((float)(((t59) * (t60))));
#ifndef __cplusplus
  extern void (fg272d_st_half)(uint64_t, uint64_t, float);
#endif
  fg272d_st_half(t51, t56, t61);
  t62 = ((uint64_t)(v1));
  t63 = ((int64_t)(v8));
  t64 = ((int64_t)(v12));
  t65 = ((int64_t)(((int64_t)((uint64_t)(t63) + (uint64_t)(t64)))));
  t66 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t67 = ((int64_t)(((int64_t)((uint64_t)(t65) + (uint64_t)(t66)))));
  t68 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t69 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t67) * (uint64_t)(t68)))))));
  t70 = ((float)(v10));
  t71 = ((float)(((float)(v15))));
  t72 = ((float)(((t70) * (t71))));
  t73 = ((float)(((float)(fg272dd_gg_double_bits(UINT64_C(4602678819172646912))))));
  t74 = ((float)(((t72) * (t73))));
#ifndef __cplusplus
  extern void (fg272d_st_half)(uint64_t, uint64_t, float);
#endif
  fg272d_st_half(t62, t69, t74);
  t75 = ((int64_t)(v12));
  t76 = ((int64_t)(((int64_t)UINT64_C(1))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t75) + (uint64_t)(t76)))));
  }
  goto label1;
label2:;
  }
fg272dd_gg_done:;
}
static __device__ __forceinline__ void fg272dd_gg_function_9(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  uint64_t t24;
  int64_t t25;
  int64_t t26;
  uint64_t t27;
  uint32_t t28;
  uint64_t t29;
  int64_t t30;
  int64_t t31;
  uint64_t t32;
  uint32_t t33;
  uint64_t t34;
  int64_t t35;
  int64_t t36;
  uint64_t t37;
  uint32_t t38;
  uint64_t t39;
  int64_t t40;
  int64_t t41;
  uint64_t t42;
  uint32_t t43;
  uint32_t t44;
  uint32_t t45;
  uint32_t t46;
  uint32_t t47;
  uint32_t t48;
  uint32_t t49;
  uint32_t t50;
  uint32_t t51;
  uint32_t t52;
  uint32_t t53;
  uint32_t t54;
  uint32_t t55;
  uint32_t t56;
  uint32_t t57;
  uint32_t t58;
  uint32_t t59;
  uint32_t t60;
  uint32_t t61;
  uint32_t t62;
  float t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  uint64_t t78;
  uint64_t t79;
  uint32_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  uint32_t t85;
  uint32_t t86;
  uint32_t t87;
  uint32_t t88;
  float t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  float t94;
  uint64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  uint64_t t106;
  uint32_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  uint32_t t112;
  uint32_t t113;
  float t114;
  uint32_t t115;
  uint32_t t116;
  uint32_t t117;
  uint32_t t118;
  uint8_t t119;
  float t120;
  uint64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  uint64_t t128;
  uint32_t t129;
  uint32_t t130;
  uint32_t t131;
  uint32_t t132;
  uint32_t t133;
  uint32_t t134;
  uint32_t t135;
  uint32_t t136;
  uint32_t t137;
  uint32_t t138;
  uint32_t t139;
  uint32_t t140;
  uint32_t t141;
  uint32_t t142;
  uint32_t t143;
  uint32_t t144;
  uint32_t t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  uint8_t t150;
  int64_t t151;
  int64_t t152;
  uint32_t t153;
  uint32_t t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  uint32_t t159;
  uint32_t t160;
  uint32_t t161;
  uint32_t t162;
  uint64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  uint64_t t168;
  float t169;
  float t170;
  float t171;
  float t172;
  float t173;
  int64_t t174;
  int64_t t175;
  uint64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  uint32_t v8;
  uint32_t v9;
  uint32_t v10;
  uint32_t v11;
  uint32_t v12;
  float v13;
  int64_t v14;
  int64_t v15;
  uint32_t v16;
  uint32_t v17;
  int64_t v18;
  float v19;
  uint32_t v20;
  uint32_t v21;
  uint32_t v22;
  float v23;
  uint32_t v24;
  uint32_t v25;
  uint32_t v26;
  uint32_t v27;
  int64_t v28;
  uint32_t v29;
  uint32_t v30;
  uint32_t v31;
  int64_t v32;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v3));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t3 = ((t4) < (t5));
  }
  t6 = t3;
  t7 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg272dd_gg_function_2(t6, t7, base + 0);
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)UINT64_C(8))));
  v4 = ((int64_t)(((int64_t)fg272dd_gg_div(t8, t9))));
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(((int64_t)UINT64_C(8))));
  v5 = ((int64_t)(((int64_t)fg272dd_gg_mod(t10, t11))));
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(56))))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  t14 = ((int64_t)(v2));
  t15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(256))))));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))));
  t17 = ((int64_t)(((int64_t)UINT64_C(32))));
  t18 = ((int64_t)(v5));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) * (uint64_t)(t18)))));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t16) + (uint64_t)(t19)))));
  t21 = ((int64_t)(((int64_t)UINT64_C(8))));
  t22 = ((int64_t)(v4));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) * (uint64_t)(t22)))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t20) + (uint64_t)(t23)))));
  t24 = ((uint64_t)(v0));
  t25 = ((int64_t)(v6));
  t26 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(48))))));
  t27 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t26)))))));
#ifndef __cplusplus
  extern uint32_t (fg272d_ld_u16)(uint64_t, uint64_t);
#endif
  t28 = fg272d_ld_u16(t24, t27);
  v8 = ((uint32_t)(t28));
  t29 = ((uint64_t)(v0));
  t30 = ((int64_t)(v6));
  t31 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(50))))));
  t32 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t30) + (uint64_t)(t31)))))));
#ifndef __cplusplus
  extern uint32_t (fg272d_ld_u16)(uint64_t, uint64_t);
#endif
  t33 = fg272d_ld_u16(t29, t32);
  v9 = ((uint32_t)(t33));
  t34 = ((uint64_t)(v0));
  t35 = ((int64_t)(v6));
  t36 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(52))))));
  t37 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t35) + (uint64_t)(t36)))))));
#ifndef __cplusplus
  extern uint32_t (fg272d_ld_u16)(uint64_t, uint64_t);
#endif
  t38 = fg272d_ld_u16(t34, t37);
  v10 = ((uint32_t)(t38));
  t39 = ((uint64_t)(v0));
  t40 = ((int64_t)(v6));
  t41 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(54))))));
  t42 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t40) + (uint64_t)(t41)))))));
#ifndef __cplusplus
  extern uint32_t (fg272d_ld_u16)(uint64_t, uint64_t);
#endif
  t43 = fg272d_ld_u16(t39, t42);
  v11 = ((uint32_t)(t43));
  t44 = ((uint32_t)(v8));
  t45 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(12))))));
  t46 = ((uint32_t)(((t44) >> ((uint64_t)(t45) & 31))));
  t47 = ((uint32_t)(v9));
  t48 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(8))))));
  t49 = ((uint32_t)(((t47) >> ((uint64_t)(t48) & 31))));
  t50 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(240))))));
  t51 = ((uint32_t)(((t49) & (t50))));
  t52 = ((uint32_t)(((t46) | (t51))));
  t53 = ((uint32_t)(v10));
  t54 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4))))));
  t55 = ((uint32_t)(((t53) >> ((uint64_t)(t54) & 31))));
  t56 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(3840))))));
  t57 = ((uint32_t)(((t55) & (t56))));
  t58 = ((uint32_t)(((t52) | (t57))));
  t59 = ((uint32_t)(v11));
  t60 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(61440))))));
  t61 = ((uint32_t)(((t59) & (t60))));
  v12 = ((uint32_t)(((t58) | (t61))));
  t62 = ((uint32_t)(v12));
#ifndef __cplusplus
  extern float (fg272d_f16)(uint32_t);
#endif
  t63 = fg272d_f16(t62);
  v13 = ((float)(t63));
  t64 = ((int64_t)(((int64_t)UINT64_C(2))));
  t65 = ((int64_t)(v5));
  t66 = ((int64_t)(((int64_t)((uint64_t)(t64) * (uint64_t)(t65)))));
  t67 = ((int64_t)(v4));
  t68 = ((int64_t)(((int64_t)UINT64_C(2))));
  t69 = ((int64_t)(((int64_t)fg272dd_gg_div(t67, t68))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t66) + (uint64_t)(t69)))));
  t70 = ((int64_t)(v6));
  t71 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(48))))));
  t72 = ((int64_t)(((int64_t)((uint64_t)(t70) + (uint64_t)(t71)))));
  t73 = ((int64_t)(((int64_t)UINT64_C(2))));
  t74 = ((int64_t)(v14));
  t75 = ((int64_t)(((int64_t)UINT64_C(4))));
  t76 = ((int64_t)(((int64_t)fg272dd_gg_div(t74, t75))));
  t77 = ((int64_t)(((int64_t)((uint64_t)(t73) * (uint64_t)(t76)))));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t72) + (uint64_t)(t77)))));
  t78 = ((uint64_t)(v0));
  t79 = ((uint64_t)(((uint64_t)(v15))));
#ifndef __cplusplus
  extern uint32_t (fg272d_ld_u16)(uint64_t, uint64_t);
#endif
  t80 = fg272d_ld_u16(t78, t79);
  v16 = ((uint32_t)(t80));
  t81 = ((int64_t)(((int64_t)UINT64_C(3))));
  t82 = ((int64_t)(v14));
  t83 = ((int64_t)(((int64_t)UINT64_C(4))));
  t84 = ((int64_t)(((int64_t)fg272dd_gg_mod(t82, t83))));
  v17 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t81) * (uint64_t)(t84)))))));
  t85 = ((uint32_t)(v16));
  t86 = ((uint32_t)(v17));
  t87 = ((uint32_t)(((t85) >> ((uint64_t)(t86) & 31))));
  t88 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(7))))));
  v18 = ((int64_t)(((int64_t)(((t87) & (t88))))));
  t89 = ((float)(v13));
  t90 = ((int64_t)(((int64_t)UINT64_C(2))));
  t91 = ((int64_t)(v18));
  t92 = ((int64_t)(((int64_t)((uint64_t)(t90) * (uint64_t)(t91)))));
  t93 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t94 = ((float)(((float)(((int64_t)((uint64_t)(t92) + (uint64_t)(t93)))))));
  v19 = ((float)(((t89) * (t94))));
  t95 = ((uint64_t)(v0));
  t96 = ((int64_t)(v6));
  t97 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t98 = ((int64_t)(((int64_t)((uint64_t)(t96) + (uint64_t)(t97)))));
  t99 = ((int64_t)(((int64_t)UINT64_C(2))));
  t100 = ((int64_t)(v5));
  t101 = ((int64_t)(((int64_t)((uint64_t)(t99) * (uint64_t)(t100)))));
  t102 = ((int64_t)(((int64_t)((uint64_t)(t98) + (uint64_t)(t101)))));
  t103 = ((int64_t)(v4));
  t104 = ((int64_t)(((int64_t)UINT64_C(2))));
  t105 = ((int64_t)(((int64_t)fg272dd_gg_div(t103, t104))));
  t106 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t102) + (uint64_t)(t105)))))));
#ifndef __cplusplus
  extern uint32_t (fg272d_ld_u8)(uint64_t, uint64_t);
#endif
  t107 = fg272d_ld_u8(t95, t106);
  v20 = ((uint32_t)(t107));
  t108 = ((int64_t)(((int64_t)UINT64_C(4))));
  t109 = ((int64_t)(v4));
  t110 = ((int64_t)(((int64_t)UINT64_C(2))));
  t111 = ((int64_t)(((int64_t)fg272dd_gg_mod(t109, t110))));
  v21 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t108) * (uint64_t)(t111)))))));
  t112 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(8))))));
  t113 = ((uint32_t)(v21));
  v22 = ((uint32_t)(((uint32_t)((uint64_t)(t112) << ((uint64_t)(t113) & 31)))));
  t114 = ((float)(fg272dd_gg_double_bits(UINT64_C(4606056518893174784))));
  v23 = ((float)((-(t114))));
  t115 = ((uint32_t)(v20));
  t116 = ((uint32_t)(v22));
  t117 = ((uint32_t)(((t115) & (t116))));
  t118 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(0))))));
  t119 = ((t117) > (t118));
  if (t119) {
  {
  t120 = ((float)(fg272dd_gg_double_bits(UINT64_C(4607745368753438720))));
  v23 = ((float)((-(t120))));
  }
  }
  t121 = ((uint64_t)(v0));
  t122 = ((int64_t)(v6));
  t123 = ((int64_t)(((int64_t)UINT64_C(4))));
  t124 = ((int64_t)(v5));
  t125 = ((int64_t)(((int64_t)((uint64_t)(t123) * (uint64_t)(t124)))));
  t126 = ((int64_t)(((int64_t)((uint64_t)(t122) + (uint64_t)(t125)))));
  t127 = ((int64_t)(v4));
  t128 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t126) + (uint64_t)(t127)))))));
#ifndef __cplusplus
  extern uint32_t (fg272d_ld_u8)(uint64_t, uint64_t);
#endif
  t129 = fg272d_ld_u8(t121, t128);
  t130 = ((uint32_t)(t129));
  t131 = ((uint32_t)(v20));
  t132 = ((uint32_t)(v21));
  t133 = ((uint32_t)(((t131) >> ((uint64_t)(t132) & 31))));
  t134 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(7))))));
  t135 = ((uint32_t)(((t133) & (t134))));
  t136 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(8))))));
  t137 = ((uint32_t)(((uint32_t)((uint64_t)(t135) << ((uint64_t)(t136) & 31)))));
  v24 = ((uint32_t)(((t130) | (t137))));
  t138 = ((uint32_t)(v24));
#ifndef __cplusplus
  extern uint32_t (fg272d_grid_1)(uint32_t);
#endif
  t139 = fg272d_grid_1(t138);
  v25 = ((uint32_t)(t139));
  t140 = ((uint32_t)(v25));
  t141 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(252645135))))));
  v26 = ((uint32_t)(((t140) & (t141))));
  t142 = ((uint32_t)(v25));
  t143 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4))))));
  t144 = ((uint32_t)(((t142) >> ((uint64_t)(t143) & 31))));
  t145 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(252645135))))));
  v27 = ((uint32_t)(((t144) & (t145))));
  v28 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t146 = ((int64_t)(v28));
  t147 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  if (!(((t146) < (t147)))) goto label2;
  {
  v29 = ((uint32_t)(((int64_t)UINT64_C(0))));
  t148 = ((int64_t)(v28));
  t149 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t150 = ((t148) < (t149));
  if (t150) {
  {
  t151 = ((int64_t)(v28));
  t152 = ((int64_t)(((int64_t)UINT64_C(8))));
  v30 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t151) * (uint64_t)(t152)))))));
  t153 = ((uint32_t)(v26));
  t154 = ((uint32_t)(v30));
  v29 = ((uint32_t)(((t153) >> ((uint64_t)(t154) & 31))));
  }
  } else {
  {
  t155 = ((int64_t)(v28));
  t156 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t157 = ((int64_t)(((int64_t)((uint64_t)(t155) - (uint64_t)(t156)))));
  t158 = ((int64_t)(((int64_t)UINT64_C(8))));
  v31 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t157) * (uint64_t)(t158)))))));
  t159 = ((uint32_t)(v27));
  t160 = ((uint32_t)(v31));
  v29 = ((uint32_t)(((t159) >> ((uint64_t)(t160) & 31))));
  }
  }
  t161 = ((uint32_t)(v29));
  t162 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(255))))));
  v32 = ((int64_t)(((int64_t)(((t161) & (t162))))));
  t163 = ((uint64_t)(v1));
  t164 = ((int64_t)(v7));
  t165 = ((int64_t)(v28));
  t166 = ((int64_t)(((int64_t)((uint64_t)(t164) + (uint64_t)(t165)))));
  t167 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t168 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t166) * (uint64_t)(t167)))))));
  t169 = ((float)(v19));
  t170 = ((float)(((float)(v32))));
  t171 = ((float)(v23));
  t172 = ((float)(((t170) + (t171))));
  t173 = ((float)(((t169) * (t172))));
#ifndef __cplusplus
  extern void (fg272d_st_bf16)(uint64_t, uint64_t, float);
#endif
  fg272d_st_bf16(t163, t168, t173);
  t174 = ((int64_t)(v28));
  t175 = ((int64_t)(((int64_t)UINT64_C(1))));
  v28 = ((int64_t)(((int64_t)((uint64_t)(t174) + (uint64_t)(t175)))));
  }
  goto label1;
label2:;
  }
fg272dd_gg_done:;
}
static __device__ __forceinline__ void fg272dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  uint64_t t24;
  int64_t t25;
  int64_t t26;
  uint64_t t27;
  uint32_t t28;
  uint64_t t29;
  int64_t t30;
  int64_t t31;
  uint64_t t32;
  uint32_t t33;
  uint64_t t34;
  int64_t t35;
  int64_t t36;
  uint64_t t37;
  uint32_t t38;
  uint64_t t39;
  int64_t t40;
  int64_t t41;
  uint64_t t42;
  uint32_t t43;
  uint32_t t44;
  uint32_t t45;
  uint32_t t46;
  uint32_t t47;
  uint32_t t48;
  uint32_t t49;
  uint32_t t50;
  uint32_t t51;
  uint32_t t52;
  uint32_t t53;
  uint32_t t54;
  uint32_t t55;
  uint32_t t56;
  uint32_t t57;
  uint32_t t58;
  uint32_t t59;
  uint32_t t60;
  uint32_t t61;
  uint32_t t62;
  float t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  uint64_t t78;
  uint64_t t79;
  uint32_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  uint32_t t85;
  uint32_t t86;
  uint32_t t87;
  uint32_t t88;
  float t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  float t94;
  uint64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  uint64_t t106;
  uint32_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  uint32_t t112;
  uint32_t t113;
  float t114;
  uint32_t t115;
  uint32_t t116;
  uint32_t t117;
  uint32_t t118;
  uint8_t t119;
  float t120;
  uint64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  uint64_t t128;
  uint32_t t129;
  uint32_t t130;
  uint32_t t131;
  uint32_t t132;
  uint32_t t133;
  uint32_t t134;
  uint32_t t135;
  uint32_t t136;
  uint32_t t137;
  uint32_t t138;
  uint32_t t139;
  uint32_t t140;
  uint32_t t141;
  uint32_t t142;
  uint32_t t143;
  uint32_t t144;
  uint32_t t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  uint8_t t150;
  int64_t t151;
  int64_t t152;
  uint32_t t153;
  uint32_t t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  uint32_t t159;
  uint32_t t160;
  uint32_t t161;
  uint32_t t162;
  uint64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  uint64_t t168;
  float t169;
  float t170;
  float t171;
  float t172;
  float t173;
  int64_t t174;
  int64_t t175;
  uint64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  uint32_t v8;
  uint32_t v9;
  uint32_t v10;
  uint32_t v11;
  uint32_t v12;
  float v13;
  int64_t v14;
  int64_t v15;
  uint32_t v16;
  uint32_t v17;
  int64_t v18;
  float v19;
  uint32_t v20;
  uint32_t v21;
  uint32_t v22;
  float v23;
  uint32_t v24;
  uint32_t v25;
  uint32_t v26;
  uint32_t v27;
  int64_t v28;
  uint32_t v29;
  uint32_t v30;
  uint32_t v31;
  int64_t v32;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v3));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t3 = ((t4) < (t5));
  }
  t6 = t3;
  t7 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg272dd_gg_function_2(t6, t7, base + 0);
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)UINT64_C(8))));
  v4 = ((int64_t)(((int64_t)fg272dd_gg_div(t8, t9))));
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(((int64_t)UINT64_C(8))));
  v5 = ((int64_t)(((int64_t)fg272dd_gg_mod(t10, t11))));
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(56))))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  t14 = ((int64_t)(v2));
  t15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(256))))));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))));
  t17 = ((int64_t)(((int64_t)UINT64_C(32))));
  t18 = ((int64_t)(v5));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) * (uint64_t)(t18)))));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t16) + (uint64_t)(t19)))));
  t21 = ((int64_t)(((int64_t)UINT64_C(8))));
  t22 = ((int64_t)(v4));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) * (uint64_t)(t22)))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t20) + (uint64_t)(t23)))));
  t24 = ((uint64_t)(v0));
  t25 = ((int64_t)(v6));
  t26 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(48))))));
  t27 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t26)))))));
#ifndef __cplusplus
  extern uint32_t (fg272d_ld_u16)(uint64_t, uint64_t);
#endif
  t28 = fg272d_ld_u16(t24, t27);
  v8 = ((uint32_t)(t28));
  t29 = ((uint64_t)(v0));
  t30 = ((int64_t)(v6));
  t31 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(50))))));
  t32 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t30) + (uint64_t)(t31)))))));
#ifndef __cplusplus
  extern uint32_t (fg272d_ld_u16)(uint64_t, uint64_t);
#endif
  t33 = fg272d_ld_u16(t29, t32);
  v9 = ((uint32_t)(t33));
  t34 = ((uint64_t)(v0));
  t35 = ((int64_t)(v6));
  t36 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(52))))));
  t37 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t35) + (uint64_t)(t36)))))));
#ifndef __cplusplus
  extern uint32_t (fg272d_ld_u16)(uint64_t, uint64_t);
#endif
  t38 = fg272d_ld_u16(t34, t37);
  v10 = ((uint32_t)(t38));
  t39 = ((uint64_t)(v0));
  t40 = ((int64_t)(v6));
  t41 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(54))))));
  t42 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t40) + (uint64_t)(t41)))))));
#ifndef __cplusplus
  extern uint32_t (fg272d_ld_u16)(uint64_t, uint64_t);
#endif
  t43 = fg272d_ld_u16(t39, t42);
  v11 = ((uint32_t)(t43));
  t44 = ((uint32_t)(v8));
  t45 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(12))))));
  t46 = ((uint32_t)(((t44) >> ((uint64_t)(t45) & 31))));
  t47 = ((uint32_t)(v9));
  t48 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(8))))));
  t49 = ((uint32_t)(((t47) >> ((uint64_t)(t48) & 31))));
  t50 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(240))))));
  t51 = ((uint32_t)(((t49) & (t50))));
  t52 = ((uint32_t)(((t46) | (t51))));
  t53 = ((uint32_t)(v10));
  t54 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4))))));
  t55 = ((uint32_t)(((t53) >> ((uint64_t)(t54) & 31))));
  t56 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(3840))))));
  t57 = ((uint32_t)(((t55) & (t56))));
  t58 = ((uint32_t)(((t52) | (t57))));
  t59 = ((uint32_t)(v11));
  t60 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(61440))))));
  t61 = ((uint32_t)(((t59) & (t60))));
  v12 = ((uint32_t)(((t58) | (t61))));
  t62 = ((uint32_t)(v12));
#ifndef __cplusplus
  extern float (fg272d_f16)(uint32_t);
#endif
  t63 = fg272d_f16(t62);
  v13 = ((float)(t63));
  t64 = ((int64_t)(((int64_t)UINT64_C(2))));
  t65 = ((int64_t)(v5));
  t66 = ((int64_t)(((int64_t)((uint64_t)(t64) * (uint64_t)(t65)))));
  t67 = ((int64_t)(v4));
  t68 = ((int64_t)(((int64_t)UINT64_C(2))));
  t69 = ((int64_t)(((int64_t)fg272dd_gg_div(t67, t68))));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t66) + (uint64_t)(t69)))));
  t70 = ((int64_t)(v6));
  t71 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(48))))));
  t72 = ((int64_t)(((int64_t)((uint64_t)(t70) + (uint64_t)(t71)))));
  t73 = ((int64_t)(((int64_t)UINT64_C(2))));
  t74 = ((int64_t)(v14));
  t75 = ((int64_t)(((int64_t)UINT64_C(4))));
  t76 = ((int64_t)(((int64_t)fg272dd_gg_div(t74, t75))));
  t77 = ((int64_t)(((int64_t)((uint64_t)(t73) * (uint64_t)(t76)))));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t72) + (uint64_t)(t77)))));
  t78 = ((uint64_t)(v0));
  t79 = ((uint64_t)(((uint64_t)(v15))));
#ifndef __cplusplus
  extern uint32_t (fg272d_ld_u16)(uint64_t, uint64_t);
#endif
  t80 = fg272d_ld_u16(t78, t79);
  v16 = ((uint32_t)(t80));
  t81 = ((int64_t)(((int64_t)UINT64_C(3))));
  t82 = ((int64_t)(v14));
  t83 = ((int64_t)(((int64_t)UINT64_C(4))));
  t84 = ((int64_t)(((int64_t)fg272dd_gg_mod(t82, t83))));
  v17 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t81) * (uint64_t)(t84)))))));
  t85 = ((uint32_t)(v16));
  t86 = ((uint32_t)(v17));
  t87 = ((uint32_t)(((t85) >> ((uint64_t)(t86) & 31))));
  t88 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(7))))));
  v18 = ((int64_t)(((int64_t)(((t87) & (t88))))));
  t89 = ((float)(v13));
  t90 = ((int64_t)(((int64_t)UINT64_C(2))));
  t91 = ((int64_t)(v18));
  t92 = ((int64_t)(((int64_t)((uint64_t)(t90) * (uint64_t)(t91)))));
  t93 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t94 = ((float)(((float)(((int64_t)((uint64_t)(t92) + (uint64_t)(t93)))))));
  v19 = ((float)(((t89) * (t94))));
  t95 = ((uint64_t)(v0));
  t96 = ((int64_t)(v6));
  t97 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t98 = ((int64_t)(((int64_t)((uint64_t)(t96) + (uint64_t)(t97)))));
  t99 = ((int64_t)(((int64_t)UINT64_C(2))));
  t100 = ((int64_t)(v5));
  t101 = ((int64_t)(((int64_t)((uint64_t)(t99) * (uint64_t)(t100)))));
  t102 = ((int64_t)(((int64_t)((uint64_t)(t98) + (uint64_t)(t101)))));
  t103 = ((int64_t)(v4));
  t104 = ((int64_t)(((int64_t)UINT64_C(2))));
  t105 = ((int64_t)(((int64_t)fg272dd_gg_div(t103, t104))));
  t106 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t102) + (uint64_t)(t105)))))));
#ifndef __cplusplus
  extern uint32_t (fg272d_ld_u8)(uint64_t, uint64_t);
#endif
  t107 = fg272d_ld_u8(t95, t106);
  v20 = ((uint32_t)(t107));
  t108 = ((int64_t)(((int64_t)UINT64_C(4))));
  t109 = ((int64_t)(v4));
  t110 = ((int64_t)(((int64_t)UINT64_C(2))));
  t111 = ((int64_t)(((int64_t)fg272dd_gg_mod(t109, t110))));
  v21 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t108) * (uint64_t)(t111)))))));
  t112 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(8))))));
  t113 = ((uint32_t)(v21));
  v22 = ((uint32_t)(((uint32_t)((uint64_t)(t112) << ((uint64_t)(t113) & 31)))));
  t114 = ((float)(fg272dd_gg_double_bits(UINT64_C(4606056518893174784))));
  v23 = ((float)((-(t114))));
  t115 = ((uint32_t)(v20));
  t116 = ((uint32_t)(v22));
  t117 = ((uint32_t)(((t115) & (t116))));
  t118 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(0))))));
  t119 = ((t117) > (t118));
  if (t119) {
  {
  t120 = ((float)(fg272dd_gg_double_bits(UINT64_C(4607745368753438720))));
  v23 = ((float)((-(t120))));
  }
  }
  t121 = ((uint64_t)(v0));
  t122 = ((int64_t)(v6));
  t123 = ((int64_t)(((int64_t)UINT64_C(4))));
  t124 = ((int64_t)(v5));
  t125 = ((int64_t)(((int64_t)((uint64_t)(t123) * (uint64_t)(t124)))));
  t126 = ((int64_t)(((int64_t)((uint64_t)(t122) + (uint64_t)(t125)))));
  t127 = ((int64_t)(v4));
  t128 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t126) + (uint64_t)(t127)))))));
#ifndef __cplusplus
  extern uint32_t (fg272d_ld_u8)(uint64_t, uint64_t);
#endif
  t129 = fg272d_ld_u8(t121, t128);
  t130 = ((uint32_t)(t129));
  t131 = ((uint32_t)(v20));
  t132 = ((uint32_t)(v21));
  t133 = ((uint32_t)(((t131) >> ((uint64_t)(t132) & 31))));
  t134 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(7))))));
  t135 = ((uint32_t)(((t133) & (t134))));
  t136 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(8))))));
  t137 = ((uint32_t)(((uint32_t)((uint64_t)(t135) << ((uint64_t)(t136) & 31)))));
  v24 = ((uint32_t)(((t130) | (t137))));
  t138 = ((uint32_t)(v24));
#ifndef __cplusplus
  extern uint32_t (fg272d_grid_1)(uint32_t);
#endif
  t139 = fg272d_grid_1(t138);
  v25 = ((uint32_t)(t139));
  t140 = ((uint32_t)(v25));
  t141 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(252645135))))));
  v26 = ((uint32_t)(((t140) & (t141))));
  t142 = ((uint32_t)(v25));
  t143 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4))))));
  t144 = ((uint32_t)(((t142) >> ((uint64_t)(t143) & 31))));
  t145 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(252645135))))));
  v27 = ((uint32_t)(((t144) & (t145))));
  v28 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t146 = ((int64_t)(v28));
  t147 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  if (!(((t146) < (t147)))) goto label2;
  {
  v29 = ((uint32_t)(((int64_t)UINT64_C(0))));
  t148 = ((int64_t)(v28));
  t149 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t150 = ((t148) < (t149));
  if (t150) {
  {
  t151 = ((int64_t)(v28));
  t152 = ((int64_t)(((int64_t)UINT64_C(8))));
  v30 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t151) * (uint64_t)(t152)))))));
  t153 = ((uint32_t)(v26));
  t154 = ((uint32_t)(v30));
  v29 = ((uint32_t)(((t153) >> ((uint64_t)(t154) & 31))));
  }
  } else {
  {
  t155 = ((int64_t)(v28));
  t156 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t157 = ((int64_t)(((int64_t)((uint64_t)(t155) - (uint64_t)(t156)))));
  t158 = ((int64_t)(((int64_t)UINT64_C(8))));
  v31 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t157) * (uint64_t)(t158)))))));
  t159 = ((uint32_t)(v27));
  t160 = ((uint32_t)(v31));
  v29 = ((uint32_t)(((t159) >> ((uint64_t)(t160) & 31))));
  }
  }
  t161 = ((uint32_t)(v29));
  t162 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(255))))));
  v32 = ((int64_t)(((int64_t)(((t161) & (t162))))));
  t163 = ((uint64_t)(v1));
  t164 = ((int64_t)(v7));
  t165 = ((int64_t)(v28));
  t166 = ((int64_t)(((int64_t)((uint64_t)(t164) + (uint64_t)(t165)))));
  t167 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t168 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t166) * (uint64_t)(t167)))))));
  t169 = ((float)(v19));
  t170 = ((float)(((float)(v32))));
  t171 = ((float)(v23));
  t172 = ((float)(((t170) + (t171))));
  t173 = ((float)(((t169) * (t172))));
#ifndef __cplusplus
  extern void (fg272d_st_half)(uint64_t, uint64_t, float);
#endif
  fg272d_st_half(t163, t168, t173);
  t174 = ((int64_t)(v28));
  t175 = ((int64_t)(((int64_t)UINT64_C(1))));
  v28 = ((int64_t)(((int64_t)((uint64_t)(t174) + (uint64_t)(t175)))));
  }
  goto label1;
label2:;
  }
fg272dd_gg_done:;
}
