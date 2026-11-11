#pragma once
static __device__ __forceinline__ void fg254dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg254dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg254dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg254dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg254dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg254dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ int64_t fg254dd_gg_function_7(int64_t p0, int64_t base);
static __device__ __forceinline__ float fg254dd_gg_function_5(uint32_t p0, int64_t base);
static __device__ __forceinline__ void fg254dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg254dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base);
static __device__ __forceinline__ void fg254dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg254dd_gg_div(int64_t a, int64_t b) { if (!b) fg254dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg254dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg254dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg254dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg254dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg254dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg254dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg254dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg254dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ int64_t fg254dd_gg_function_7(int64_t p0, int64_t base) {
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
  goto fg254dd_gg_done;
  }
  }
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t6 = ((t4) == (t5));
  if (t6) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto fg254dd_gg_done;
  }
  }
  t7 = ((int64_t)(v0));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  goto fg254dd_gg_done;
  }
  }
  t10 = ((int64_t)(v0));
  t11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t12 = ((t10) == (t11));
  if (t12) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  goto fg254dd_gg_done;
  }
  }
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t15 = ((t13) == (t14));
  if (t15) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  goto fg254dd_gg_done;
  }
  }
  t16 = ((int64_t)(v0));
  t17 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t18 = ((t16) == (t17));
  if (t18) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(6))))));
  goto fg254dd_gg_done;
  }
  }
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(6))))));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  goto fg254dd_gg_done;
  }
  }
  t22 = ((int64_t)(v0));
  t23 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(7))))));
  t24 = ((t22) == (t23));
  if (t24) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(12))))));
  goto fg254dd_gg_done;
  }
  }
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t27 = ((t25) == (t26));
  if (t27) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto fg254dd_gg_done;
  }
  }
  t28 = ((int64_t)(v0));
  t29 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(9))))));
  t30 = ((t28) == (t29));
  if (t30) {
  {
  t31 = ((int64_t)(((int64_t)UINT64_C(1))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t31)))));
  goto fg254dd_gg_done;
  }
  }
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(10))))));
  t34 = ((t32) == (t33));
  if (t34) {
  {
  t35 = ((int64_t)(((int64_t)UINT64_C(2))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t35)))));
  goto fg254dd_gg_done;
  }
  }
  t36 = ((int64_t)(v0));
  t37 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(11))))));
  t38 = ((t36) == (t37));
  if (t38) {
  {
  t39 = ((int64_t)(((int64_t)UINT64_C(3))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t39)))));
  goto fg254dd_gg_done;
  }
  }
  t40 = ((int64_t)(v0));
  t41 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(12))))));
  t42 = ((t40) == (t41));
  if (t42) {
  {
  t43 = ((int64_t)(((int64_t)UINT64_C(4))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t43)))));
  goto fg254dd_gg_done;
  }
  }
  t44 = ((int64_t)(v0));
  t45 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(13))))));
  t46 = ((t44) == (t45));
  if (t46) {
  {
  t47 = ((int64_t)(((int64_t)UINT64_C(6))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t47)))));
  goto fg254dd_gg_done;
  }
  }
  t48 = ((int64_t)(v0));
  t49 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(14))))));
  t50 = ((t48) == (t49));
  if (t50) {
  {
  t51 = ((int64_t)(((int64_t)UINT64_C(8))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t51)))));
  goto fg254dd_gg_done;
  }
  }
  t52 = ((int64_t)(((int64_t)UINT64_C(12))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t52)))));
  }
fg254dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg254dd_gg_function_5(uint32_t p0, int64_t base) {
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
  extern float (fg254d_u32_as_f32)(uint32_t);
#endif
  t7 = fg254d_u32_as_f32(t6);
  t0 = ((float)(t7));
  }
fg254dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg254dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg254d_fail)(int64_t);
#endif
  fg254d_fail(t2);
  }
  }
  }
fg254dd_gg_done:;
}
static __device__ __forceinline__ void fg254dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  fg254dd_gg_function_2(t6, t7, base + 0);
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)UINT64_C(8))));
  v4 = ((int64_t)(((int64_t)fg254dd_gg_mod(t8, t9))));
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(((int64_t)UINT64_C(8))));
  v5 = ((int64_t)(((int64_t)fg254dd_gg_div(t10, t11))));
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
  extern uint32_t (fg254d_ld_u8)(uint64_t, uint64_t);
#endif
  t30 = fg254d_ld_u8(t28, t29);
  v9 = ((uint32_t)(t30));
  t31 = ((uint32_t)(v9));
  v10 = fg254dd_gg_function_5(t31, base + 0);
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
  extern uint32_t (fg254d_ld_u8)(uint64_t, uint64_t);
#endif
  t44 = fg254d_ld_u8(t40, t43);
  v13 = ((uint32_t)(t44));
  t45 = ((uint32_t)(v13));
  t46 = ((uint32_t)(((int64_t)UINT64_C(15))));
  t47 = ((int64_t)(((int64_t)(((t45) & (t46))))));
  v14 = fg254dd_gg_function_7(t47, base + 0);
  t48 = ((uint32_t)(v13));
  t49 = ((int64_t)(((int64_t)UINT64_C(4))));
  t50 = ((int64_t)(((int64_t)(((t48) >> ((uint64_t)(t49) & 31))))));
  v15 = fg254dd_gg_function_7(t50, base + 0);
  t51 = ((uint64_t)(v1));
  t52 = ((int64_t)(v8));
  t53 = ((int64_t)(v12));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t53)))));
  t55 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t56 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t54) * (uint64_t)(t55)))))));
  t57 = ((float)(v10));
  t58 = ((float)(((float)(v14))));
  t59 = ((float)(((t57) * (t58))));
  t60 = ((float)(((float)(fg254dd_gg_double_bits(UINT64_C(4602678819172646912))))));
  t61 = ((float)(((t59) * (t60))));
#ifndef __cplusplus
  extern void (fg254d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg254d_st_f32(t51, t56, t61);
  t62 = ((uint64_t)(v1));
  t63 = ((int64_t)(v8));
  t64 = ((int64_t)(v12));
  t65 = ((int64_t)(((int64_t)((uint64_t)(t63) + (uint64_t)(t64)))));
  t66 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t67 = ((int64_t)(((int64_t)((uint64_t)(t65) + (uint64_t)(t66)))));
  t68 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t69 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t67) * (uint64_t)(t68)))))));
  t70 = ((float)(v10));
  t71 = ((float)(((float)(v15))));
  t72 = ((float)(((t70) * (t71))));
  t73 = ((float)(((float)(fg254dd_gg_double_bits(UINT64_C(4602678819172646912))))));
  t74 = ((float)(((t72) * (t73))));
#ifndef __cplusplus
  extern void (fg254d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg254d_st_f32(t62, t69, t74);
  t75 = ((int64_t)(v12));
  t76 = ((int64_t)(((int64_t)UINT64_C(1))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t75) + (uint64_t)(t76)))));
  }
  goto label1;
label2:;
  }
fg254dd_gg_done:;
}
