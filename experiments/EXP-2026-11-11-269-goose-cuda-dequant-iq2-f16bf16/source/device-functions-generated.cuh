#pragma once
static __device__ __forceinline__ void fg269dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg269dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg269dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg269dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg269dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg269dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ float fg269dd_gg_function_10(int64_t p0, int64_t base);
static __device__ __forceinline__ void fg269dd_gg_function_13(uint64_t p0, uint32_t p1, float p2, uint64_t p3, int64_t p4, int64_t base);
static __device__ __forceinline__ void fg269dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg269dd_gg_function_17(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base);
static __device__ __forceinline__ void fg269dd_gg_function_9(uint64_t p0, uint32_t p1, float p2, uint64_t p3, int64_t p4, int64_t base);
static __device__ __forceinline__ void fg269dd_gg_function_15(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base);
static __device__ __forceinline__ void fg269dd_gg_function_12(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base);
static __device__ __forceinline__ void fg269dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base);
static __device__ __forceinline__ void fg269dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg269dd_gg_div(int64_t a, int64_t b) { if (!b) fg269dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg269dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg269dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg269dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg269dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg269dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg269dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg269dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg269dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ float fg269dd_gg_function_10(int64_t p0, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  float t4;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) > (t2));
  if (t3) {
  {
  t4 = ((float)(fg269dd_gg_double_bits(UINT64_C(4607182418800017408))));
  t0 = ((float)((-(t4))));
  goto fg269dd_gg_done;
  }
  }
  t0 = ((float)(((float)(fg269dd_gg_double_bits(UINT64_C(4607182418800017408))))));
  }
fg269dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg269dd_gg_function_13(uint64_t p0, uint32_t p1, float p2, uint64_t p3, int64_t p4, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint64_t t6;
  uint64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  uint64_t t17;
  float t18;
  float t19;
  float t20;
  int64_t t21;
  float t22;
  float t23;
  float t24;
  int64_t t25;
  int64_t t26;
  uint64_t v0;
  uint32_t v1;
  float v2;
  uint64_t v3;
  int64_t v4;
  int64_t v5;
  uint64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t0 = ((int64_t)(v5));
  t1 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  if (!(((t0) < (t1)))) goto label2;
  {
  t2 = ((int64_t)(v5));
  t3 = ((int64_t)(((int64_t)UINT64_C(8))));
  v6 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t2) * (uint64_t)(t3)))))));
  t4 = ((uint64_t)(v0));
  t5 = ((uint64_t)(v6));
  t6 = ((uint64_t)(((t4) >> ((uint64_t)(t5) & 63))));
  t7 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(255))))));
  v7 = ((int64_t)(((int64_t)(((t6) & (t7))))));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t9 = ((int64_t)(v5));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t8) << ((uint64_t)(t9) & 63)))));
  t10 = ((int64_t)(((int64_t)(v1))));
  t11 = ((int64_t)(v8));
  v9 = ((int64_t)(((t10) & (t11))));
  t12 = ((uint64_t)(v3));
  t13 = ((int64_t)(v4));
  t14 = ((int64_t)(v5));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t14)))));
  t16 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t17 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t15) * (uint64_t)(t16)))))));
  t18 = ((float)(v2));
  t19 = ((float)(((float)(v7))));
  t20 = ((float)(((t18) * (t19))));
  t21 = ((int64_t)(v9));
  t22 = fg269dd_gg_function_10(t21, base + 0);
  t23 = ((float)(t22));
  t24 = ((float)(((t20) * (t23))));
#ifndef __cplusplus
  extern void (fg269d_st_bf16)(uint64_t, uint64_t, float);
#endif
  fg269d_st_bf16(t12, t17, t24);
  t25 = ((int64_t)(v5));
  t26 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t26)))));
  }
  goto label1;
label2:;
  }
fg269dd_gg_done:;
}
static __device__ __forceinline__ void fg269dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg269d_fail)(int64_t);
#endif
  fg269d_fail(t2);
  }
  }
  }
fg269dd_gg_done:;
}
static __device__ __forceinline__ void fg269dd_gg_function_17(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  int64_t t28;
  int64_t t29;
  uint64_t t30;
  uint64_t t31;
  float t32;
  uint64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint64_t t38;
  uint32_t t39;
  uint32_t t40;
  uint32_t t41;
  uint64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint64_t t47;
  uint32_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  float t53;
  float t54;
  uint32_t t55;
  uint32_t t56;
  uint32_t t57;
  uint32_t t58;
  float t59;
  float t60;
  float t61;
  float t62;
  uint32_t t63;
  uint32_t t64;
  uint32_t t65;
  uint32_t t66;
  uint32_t t67;
  uint64_t t68;
  uint64_t t69;
  uint32_t t70;
  float t71;
  uint64_t t72;
  int64_t t73;
  uint64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  float v9;
  uint32_t v10;
  uint32_t v11;
  uint32_t v12;
  uint32_t v13;
  float v14;
  uint32_t v15;
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
  fg269dd_gg_function_2(t6, t7, base + 0);
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)UINT64_C(8))));
  v4 = ((int64_t)(((int64_t)fg269dd_gg_div(t8, t9))));
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(((int64_t)UINT64_C(8))));
  v5 = ((int64_t)(((int64_t)fg269dd_gg_mod(t10, t11))));
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(74))))));
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
  t24 = ((int64_t)(v6));
  t25 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) + (uint64_t)(t25)))));
  t27 = ((int64_t)(((int64_t)UINT64_C(8))));
  t28 = ((int64_t)(v5));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) * (uint64_t)(t28)))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t26) + (uint64_t)(t29)))));
  t30 = ((uint64_t)(v0));
  t31 = ((uint64_t)(((uint64_t)(v6))));
#ifndef __cplusplus
  extern float (fg269d_ld_half)(uint64_t, uint64_t);
#endif
  t32 = fg269d_ld_half(t30, t31);
  v9 = ((float)(t32));
  t33 = ((uint64_t)(v0));
  t34 = ((int64_t)(v8));
  t35 = ((int64_t)(((int64_t)UINT64_C(2))));
  t36 = ((int64_t)(v4));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) * (uint64_t)(t36)))));
  t38 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t34) + (uint64_t)(t37)))))));
#ifndef __cplusplus
  extern uint32_t (fg269d_ld_u16)(uint64_t, uint64_t);
#endif
  t39 = fg269d_ld_u16(t33, t38);
  v10 = ((uint32_t)(t39));
  t40 = ((uint32_t)(v10));
  t41 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(511))))));
  v11 = ((uint32_t)(((t40) & (t41))));
  t42 = ((uint64_t)(v0));
  t43 = ((int64_t)(v6));
  t44 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(66))))));
  t45 = ((int64_t)(((int64_t)((uint64_t)(t43) + (uint64_t)(t44)))));
  t46 = ((int64_t)(v5));
  t47 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t45) + (uint64_t)(t46)))))));
#ifndef __cplusplus
  extern uint32_t (fg269d_ld_u8)(uint64_t, uint64_t);
#endif
  t48 = fg269d_ld_u8(t42, t47);
  v12 = ((uint32_t)(t48));
  t49 = ((int64_t)(((int64_t)UINT64_C(4))));
  t50 = ((int64_t)(v4));
  t51 = ((int64_t)(((int64_t)UINT64_C(2))));
  t52 = ((int64_t)(((int64_t)fg269dd_gg_div(t50, t51))));
  v13 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t49) * (uint64_t)(t52)))))));
  t53 = ((float)(v9));
  t54 = ((float)(((float)(fg269dd_gg_double_bits(UINT64_C(4602678819172646912))))));
  t55 = ((uint32_t)(v12));
  t56 = ((uint32_t)(v13));
  t57 = ((uint32_t)(((t55) >> ((uint64_t)(t56) & 31))));
  t58 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(15))))));
  t59 = ((float)(((float)(((t57) & (t58))))));
  t60 = ((float)(((t54) + (t59))));
  t61 = ((float)(((t53) * (t60))));
  t62 = ((float)(((float)(fg269dd_gg_double_bits(UINT64_C(4598175219545276416))))));
  v14 = ((float)(((t61) * (t62))));
  t63 = ((uint32_t)(v10));
  t64 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(9))))));
  t65 = ((uint32_t)(((t63) >> ((uint64_t)(t64) & 31))));
#ifndef __cplusplus
  extern uint32_t (fg269d_ksign)(uint32_t);
#endif
  t66 = fg269d_ksign(t65);
  v15 = ((uint32_t)(t66));
  t67 = ((uint32_t)(v11));
#ifndef __cplusplus
  extern uint64_t (fg269d_grid_xs)(uint32_t);
#endif
  t68 = fg269d_grid_xs(t67);
  t69 = ((uint64_t)(t68));
  t70 = ((uint32_t)(v15));
  t71 = ((float)(v14));
  t72 = ((uint64_t)(v1));
  t73 = ((int64_t)(v7));
  fg269dd_gg_function_13(t69, t70, t71, t72, t73, base + 0);
  }
fg269dd_gg_done:;
}
static __device__ __forceinline__ void fg269dd_gg_function_9(uint64_t p0, uint32_t p1, float p2, uint64_t p3, int64_t p4, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint64_t t6;
  uint64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  uint64_t t17;
  float t18;
  float t19;
  float t20;
  int64_t t21;
  float t22;
  float t23;
  float t24;
  int64_t t25;
  int64_t t26;
  uint64_t v0;
  uint32_t v1;
  float v2;
  uint64_t v3;
  int64_t v4;
  int64_t v5;
  uint64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t0 = ((int64_t)(v5));
  t1 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  if (!(((t0) < (t1)))) goto label2;
  {
  t2 = ((int64_t)(v5));
  t3 = ((int64_t)(((int64_t)UINT64_C(8))));
  v6 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t2) * (uint64_t)(t3)))))));
  t4 = ((uint64_t)(v0));
  t5 = ((uint64_t)(v6));
  t6 = ((uint64_t)(((t4) >> ((uint64_t)(t5) & 63))));
  t7 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(255))))));
  v7 = ((int64_t)(((int64_t)(((t6) & (t7))))));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t9 = ((int64_t)(v5));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t8) << ((uint64_t)(t9) & 63)))));
  t10 = ((int64_t)(((int64_t)(v1))));
  t11 = ((int64_t)(v8));
  v9 = ((int64_t)(((t10) & (t11))));
  t12 = ((uint64_t)(v3));
  t13 = ((int64_t)(v4));
  t14 = ((int64_t)(v5));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t14)))));
  t16 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t17 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t15) * (uint64_t)(t16)))))));
  t18 = ((float)(v2));
  t19 = ((float)(((float)(v7))));
  t20 = ((float)(((t18) * (t19))));
  t21 = ((int64_t)(v9));
  t22 = fg269dd_gg_function_10(t21, base + 0);
  t23 = ((float)(t22));
  t24 = ((float)(((t20) * (t23))));
#ifndef __cplusplus
  extern void (fg269d_st_half)(uint64_t, uint64_t, float);
#endif
  fg269d_st_half(t12, t17, t24);
  t25 = ((int64_t)(v5));
  t26 = ((int64_t)(((int64_t)UINT64_C(1))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t26)))));
  }
  goto label1;
label2:;
  }
fg269dd_gg_done:;
}
static __device__ __forceinline__ void fg269dd_gg_function_15(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  int64_t t28;
  int64_t t29;
  uint64_t t30;
  uint64_t t31;
  float t32;
  uint64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint64_t t38;
  uint32_t t39;
  uint32_t t40;
  uint32_t t41;
  uint64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint64_t t47;
  uint32_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  float t53;
  float t54;
  uint32_t t55;
  uint32_t t56;
  uint32_t t57;
  uint32_t t58;
  float t59;
  float t60;
  float t61;
  float t62;
  uint32_t t63;
  uint32_t t64;
  uint32_t t65;
  uint32_t t66;
  uint32_t t67;
  uint64_t t68;
  uint64_t t69;
  uint32_t t70;
  float t71;
  uint64_t t72;
  int64_t t73;
  uint64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  float v9;
  uint32_t v10;
  uint32_t v11;
  uint32_t v12;
  uint32_t v13;
  float v14;
  uint32_t v15;
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
  fg269dd_gg_function_2(t6, t7, base + 0);
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)UINT64_C(8))));
  v4 = ((int64_t)(((int64_t)fg269dd_gg_div(t8, t9))));
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(((int64_t)UINT64_C(8))));
  v5 = ((int64_t)(((int64_t)fg269dd_gg_mod(t10, t11))));
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(74))))));
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
  t24 = ((int64_t)(v6));
  t25 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) + (uint64_t)(t25)))));
  t27 = ((int64_t)(((int64_t)UINT64_C(8))));
  t28 = ((int64_t)(v5));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) * (uint64_t)(t28)))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t26) + (uint64_t)(t29)))));
  t30 = ((uint64_t)(v0));
  t31 = ((uint64_t)(((uint64_t)(v6))));
#ifndef __cplusplus
  extern float (fg269d_ld_half)(uint64_t, uint64_t);
#endif
  t32 = fg269d_ld_half(t30, t31);
  v9 = ((float)(t32));
  t33 = ((uint64_t)(v0));
  t34 = ((int64_t)(v8));
  t35 = ((int64_t)(((int64_t)UINT64_C(2))));
  t36 = ((int64_t)(v4));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) * (uint64_t)(t36)))));
  t38 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t34) + (uint64_t)(t37)))))));
#ifndef __cplusplus
  extern uint32_t (fg269d_ld_u16)(uint64_t, uint64_t);
#endif
  t39 = fg269d_ld_u16(t33, t38);
  v10 = ((uint32_t)(t39));
  t40 = ((uint32_t)(v10));
  t41 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(511))))));
  v11 = ((uint32_t)(((t40) & (t41))));
  t42 = ((uint64_t)(v0));
  t43 = ((int64_t)(v6));
  t44 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(66))))));
  t45 = ((int64_t)(((int64_t)((uint64_t)(t43) + (uint64_t)(t44)))));
  t46 = ((int64_t)(v5));
  t47 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t45) + (uint64_t)(t46)))))));
#ifndef __cplusplus
  extern uint32_t (fg269d_ld_u8)(uint64_t, uint64_t);
#endif
  t48 = fg269d_ld_u8(t42, t47);
  v12 = ((uint32_t)(t48));
  t49 = ((int64_t)(((int64_t)UINT64_C(4))));
  t50 = ((int64_t)(v4));
  t51 = ((int64_t)(((int64_t)UINT64_C(2))));
  t52 = ((int64_t)(((int64_t)fg269dd_gg_div(t50, t51))));
  v13 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t49) * (uint64_t)(t52)))))));
  t53 = ((float)(v9));
  t54 = ((float)(((float)(fg269dd_gg_double_bits(UINT64_C(4602678819172646912))))));
  t55 = ((uint32_t)(v12));
  t56 = ((uint32_t)(v13));
  t57 = ((uint32_t)(((t55) >> ((uint64_t)(t56) & 31))));
  t58 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(15))))));
  t59 = ((float)(((float)(((t57) & (t58))))));
  t60 = ((float)(((t54) + (t59))));
  t61 = ((float)(((t53) * (t60))));
  t62 = ((float)(((float)(fg269dd_gg_double_bits(UINT64_C(4598175219545276416))))));
  v14 = ((float)(((t61) * (t62))));
  t63 = ((uint32_t)(v10));
  t64 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(9))))));
  t65 = ((uint32_t)(((t63) >> ((uint64_t)(t64) & 31))));
#ifndef __cplusplus
  extern uint32_t (fg269d_ksign)(uint32_t);
#endif
  t66 = fg269d_ksign(t65);
  v15 = ((uint32_t)(t66));
  t67 = ((uint32_t)(v11));
#ifndef __cplusplus
  extern uint64_t (fg269d_grid_xs)(uint32_t);
#endif
  t68 = fg269d_grid_xs(t67);
  t69 = ((uint64_t)(t68));
  t70 = ((uint32_t)(v15));
  t71 = ((float)(v14));
  t72 = ((uint64_t)(v1));
  t73 = ((int64_t)(v7));
  fg269dd_gg_function_9(t69, t70, t71, t72, t73, base + 0);
  }
fg269dd_gg_done:;
}
static __device__ __forceinline__ void fg269dd_gg_function_12(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  int64_t t28;
  int64_t t29;
  uint64_t t30;
  uint64_t t31;
  float t32;
  uint64_t t33;
  int64_t t34;
  int64_t t35;
  uint64_t t36;
  uint32_t t37;
  uint64_t t38;
  int64_t t39;
  int64_t t40;
  uint64_t t41;
  uint32_t t42;
  uint64_t t43;
  int64_t t44;
  int64_t t45;
  uint64_t t46;
  uint32_t t47;
  uint32_t t48;
  uint32_t t49;
  uint32_t t50;
  uint32_t t51;
  uint32_t t52;
  uint32_t t53;
  float t54;
  float t55;
  float t56;
  float t57;
  float t58;
  float t59;
  int64_t t60;
  int64_t t61;
  uint32_t t62;
  uint32_t t63;
  uint32_t t64;
  uint32_t t65;
  uint32_t t66;
  uint32_t t67;
  uint32_t t68;
  uint64_t t69;
  uint64_t t70;
  uint32_t t71;
  float t72;
  uint64_t t73;
  int64_t t74;
  uint64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  float v9;
  uint32_t v10;
  uint32_t v11;
  uint32_t v12;
  uint32_t v13;
  uint32_t v14;
  float v15;
  uint32_t v16;
  uint32_t v17;
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
  fg269dd_gg_function_2(t6, t7, base + 0);
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)UINT64_C(8))));
  v4 = ((int64_t)(((int64_t)fg269dd_gg_div(t8, t9))));
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(((int64_t)UINT64_C(8))));
  v5 = ((int64_t)(((int64_t)fg269dd_gg_mod(t10, t11))));
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(66))))));
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
  t24 = ((int64_t)(v6));
  t25 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) + (uint64_t)(t25)))));
  t27 = ((int64_t)(((int64_t)UINT64_C(8))));
  t28 = ((int64_t)(v5));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) * (uint64_t)(t28)))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t26) + (uint64_t)(t29)))));
  t30 = ((uint64_t)(v0));
  t31 = ((uint64_t)(((uint64_t)(v6))));
#ifndef __cplusplus
  extern float (fg269d_ld_half)(uint64_t, uint64_t);
#endif
  t32 = fg269d_ld_half(t30, t31);
  v9 = ((float)(t32));
  t33 = ((uint64_t)(v0));
  t34 = ((int64_t)(v8));
  t35 = ((int64_t)(v4));
  t36 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t34) + (uint64_t)(t35)))))));
#ifndef __cplusplus
  extern uint32_t (fg269d_ld_u8)(uint64_t, uint64_t);
#endif
  t37 = fg269d_ld_u8(t33, t36);
  v10 = ((uint32_t)(t37));
  t38 = ((uint64_t)(v0));
  t39 = ((int64_t)(v8));
  t40 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t41 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t39) + (uint64_t)(t40)))))));
#ifndef __cplusplus
  extern uint32_t (fg269d_ld_u16)(uint64_t, uint64_t);
#endif
  t42 = fg269d_ld_u16(t38, t41);
  v11 = ((uint32_t)(t42));
  t43 = ((uint64_t)(v0));
  t44 = ((int64_t)(v8));
  t45 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(6))))));
  t46 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t44) + (uint64_t)(t45)))))));
#ifndef __cplusplus
  extern uint32_t (fg269d_ld_u16)(uint64_t, uint64_t);
#endif
  t47 = fg269d_ld_u16(t43, t46);
  v12 = ((uint32_t)(t47));
  t48 = ((uint32_t)(v11));
  t49 = ((uint32_t)(v12));
  t50 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(16))))));
  t51 = ((uint32_t)(((uint32_t)((uint64_t)(t49) << ((uint64_t)(t50) & 31)))));
  v13 = ((uint32_t)(((t48) | (t51))));
  t52 = ((uint32_t)(v13));
  t53 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(28))))));
  v14 = ((uint32_t)(((t52) >> ((uint64_t)(t53) & 31))));
  t54 = ((float)(v9));
  t55 = ((float)(((float)(fg269dd_gg_double_bits(UINT64_C(4602678819172646912))))));
  t56 = ((float)(((float)(v14))));
  t57 = ((float)(((t55) + (t56))));
  t58 = ((float)(((t54) * (t57))));
  t59 = ((float)(((float)(fg269dd_gg_double_bits(UINT64_C(4598175219545276416))))));
  v15 = ((float)(((t58) * (t59))));
  t60 = ((int64_t)(((int64_t)UINT64_C(7))));
  t61 = ((int64_t)(v4));
  v16 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t60) * (uint64_t)(t61)))))));
  t62 = ((uint32_t)(v13));
  t63 = ((uint32_t)(v16));
  t64 = ((uint32_t)(((t62) >> ((uint64_t)(t63) & 31))));
  t65 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(127))))));
  t66 = ((uint32_t)(((t64) & (t65))));
#ifndef __cplusplus
  extern uint32_t (fg269d_ksign)(uint32_t);
#endif
  t67 = fg269d_ksign(t66);
  v17 = ((uint32_t)(t67));
  t68 = ((uint32_t)(v10));
#ifndef __cplusplus
  extern uint64_t (fg269d_grid_xxs)(uint32_t);
#endif
  t69 = fg269d_grid_xxs(t68);
  t70 = ((uint64_t)(t69));
  t71 = ((uint32_t)(v17));
  t72 = ((float)(v15));
  t73 = ((uint64_t)(v1));
  t74 = ((int64_t)(v7));
  fg269dd_gg_function_13(t70, t71, t72, t73, t74, base + 0);
  }
fg269dd_gg_done:;
}
static __device__ __forceinline__ void fg269dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  int64_t t28;
  int64_t t29;
  uint64_t t30;
  uint64_t t31;
  float t32;
  uint64_t t33;
  int64_t t34;
  int64_t t35;
  uint64_t t36;
  uint32_t t37;
  uint64_t t38;
  int64_t t39;
  int64_t t40;
  uint64_t t41;
  uint32_t t42;
  uint64_t t43;
  int64_t t44;
  int64_t t45;
  uint64_t t46;
  uint32_t t47;
  uint32_t t48;
  uint32_t t49;
  uint32_t t50;
  uint32_t t51;
  uint32_t t52;
  uint32_t t53;
  float t54;
  float t55;
  float t56;
  float t57;
  float t58;
  float t59;
  int64_t t60;
  int64_t t61;
  uint32_t t62;
  uint32_t t63;
  uint32_t t64;
  uint32_t t65;
  uint32_t t66;
  uint32_t t67;
  uint32_t t68;
  uint64_t t69;
  uint64_t t70;
  uint32_t t71;
  float t72;
  uint64_t t73;
  int64_t t74;
  uint64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  float v9;
  uint32_t v10;
  uint32_t v11;
  uint32_t v12;
  uint32_t v13;
  uint32_t v14;
  float v15;
  uint32_t v16;
  uint32_t v17;
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
  fg269dd_gg_function_2(t6, t7, base + 0);
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)UINT64_C(8))));
  v4 = ((int64_t)(((int64_t)fg269dd_gg_div(t8, t9))));
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(((int64_t)UINT64_C(8))));
  v5 = ((int64_t)(((int64_t)fg269dd_gg_mod(t10, t11))));
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(66))))));
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
  t24 = ((int64_t)(v6));
  t25 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) + (uint64_t)(t25)))));
  t27 = ((int64_t)(((int64_t)UINT64_C(8))));
  t28 = ((int64_t)(v5));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) * (uint64_t)(t28)))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t26) + (uint64_t)(t29)))));
  t30 = ((uint64_t)(v0));
  t31 = ((uint64_t)(((uint64_t)(v6))));
#ifndef __cplusplus
  extern float (fg269d_ld_half)(uint64_t, uint64_t);
#endif
  t32 = fg269d_ld_half(t30, t31);
  v9 = ((float)(t32));
  t33 = ((uint64_t)(v0));
  t34 = ((int64_t)(v8));
  t35 = ((int64_t)(v4));
  t36 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t34) + (uint64_t)(t35)))))));
#ifndef __cplusplus
  extern uint32_t (fg269d_ld_u8)(uint64_t, uint64_t);
#endif
  t37 = fg269d_ld_u8(t33, t36);
  v10 = ((uint32_t)(t37));
  t38 = ((uint64_t)(v0));
  t39 = ((int64_t)(v8));
  t40 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t41 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t39) + (uint64_t)(t40)))))));
#ifndef __cplusplus
  extern uint32_t (fg269d_ld_u16)(uint64_t, uint64_t);
#endif
  t42 = fg269d_ld_u16(t38, t41);
  v11 = ((uint32_t)(t42));
  t43 = ((uint64_t)(v0));
  t44 = ((int64_t)(v8));
  t45 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(6))))));
  t46 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t44) + (uint64_t)(t45)))))));
#ifndef __cplusplus
  extern uint32_t (fg269d_ld_u16)(uint64_t, uint64_t);
#endif
  t47 = fg269d_ld_u16(t43, t46);
  v12 = ((uint32_t)(t47));
  t48 = ((uint32_t)(v11));
  t49 = ((uint32_t)(v12));
  t50 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(16))))));
  t51 = ((uint32_t)(((uint32_t)((uint64_t)(t49) << ((uint64_t)(t50) & 31)))));
  v13 = ((uint32_t)(((t48) | (t51))));
  t52 = ((uint32_t)(v13));
  t53 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(28))))));
  v14 = ((uint32_t)(((t52) >> ((uint64_t)(t53) & 31))));
  t54 = ((float)(v9));
  t55 = ((float)(((float)(fg269dd_gg_double_bits(UINT64_C(4602678819172646912))))));
  t56 = ((float)(((float)(v14))));
  t57 = ((float)(((t55) + (t56))));
  t58 = ((float)(((t54) * (t57))));
  t59 = ((float)(((float)(fg269dd_gg_double_bits(UINT64_C(4598175219545276416))))));
  v15 = ((float)(((t58) * (t59))));
  t60 = ((int64_t)(((int64_t)UINT64_C(7))));
  t61 = ((int64_t)(v4));
  v16 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t60) * (uint64_t)(t61)))))));
  t62 = ((uint32_t)(v13));
  t63 = ((uint32_t)(v16));
  t64 = ((uint32_t)(((t62) >> ((uint64_t)(t63) & 31))));
  t65 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(127))))));
  t66 = ((uint32_t)(((t64) & (t65))));
#ifndef __cplusplus
  extern uint32_t (fg269d_ksign)(uint32_t);
#endif
  t67 = fg269d_ksign(t66);
  v17 = ((uint32_t)(t67));
  t68 = ((uint32_t)(v10));
#ifndef __cplusplus
  extern uint64_t (fg269d_grid_xxs)(uint32_t);
#endif
  t69 = fg269d_grid_xxs(t68);
  t70 = ((uint64_t)(t69));
  t71 = ((uint32_t)(v17));
  t72 = ((float)(v15));
  t73 = ((uint64_t)(v1));
  t74 = ((int64_t)(v7));
  fg269dd_gg_function_9(t70, t71, t72, t73, t74, base + 0);
  }
fg269dd_gg_done:;
}
