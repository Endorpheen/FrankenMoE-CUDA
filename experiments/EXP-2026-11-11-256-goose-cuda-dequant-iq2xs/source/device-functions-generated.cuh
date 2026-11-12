#pragma once
static __device__ __forceinline__ void fg256dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg256dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg256dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg256dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg256dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg256dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ float fg256dd_gg_function_9(int64_t p0, int64_t base);
static __device__ __forceinline__ void fg256dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg256dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base);
static __device__ __forceinline__ void fg256dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg256dd_gg_div(int64_t a, int64_t b) { if (!b) fg256dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg256dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg256dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg256dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg256dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg256dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg256dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg256dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg256dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ float fg256dd_gg_function_9(int64_t p0, int64_t base) {
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
  t4 = ((float)(fg256dd_gg_double_bits(UINT64_C(4607182418800017408))));
  t0 = ((float)((-(t4))));
  goto fg256dd_gg_done;
  }
  }
  t0 = ((float)(((float)(fg256dd_gg_double_bits(UINT64_C(4607182418800017408))))));
  }
fg256dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg256dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg256d_fail)(int64_t);
#endif
  fg256d_fail(t2);
  }
  }
  }
fg256dd_gg_done:;
}
static __device__ __forceinline__ void fg256dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  uint64_t t73;
  uint64_t t74;
  uint64_t t75;
  uint64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  uint64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  uint64_t t86;
  float t87;
  float t88;
  float t89;
  int64_t t90;
  float t91;
  float t92;
  float t93;
  int64_t t94;
  int64_t t95;
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
  uint64_t v16;
  int64_t v17;
  uint64_t v18;
  int64_t v19;
  int64_t v20;
  int64_t v21;
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
  fg256dd_gg_function_2(t6, t7, base + 0);
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)UINT64_C(8))));
  v4 = ((int64_t)(((int64_t)fg256dd_gg_div(t8, t9))));
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(((int64_t)UINT64_C(8))));
  v5 = ((int64_t)(((int64_t)fg256dd_gg_mod(t10, t11))));
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
  extern float (fg256d_ld_half)(uint64_t, uint64_t);
#endif
  t32 = fg256d_ld_half(t30, t31);
  v9 = ((float)(t32));
  t33 = ((uint64_t)(v0));
  t34 = ((int64_t)(v8));
  t35 = ((int64_t)(((int64_t)UINT64_C(2))));
  t36 = ((int64_t)(v4));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) * (uint64_t)(t36)))));
  t38 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t34) + (uint64_t)(t37)))))));
#ifndef __cplusplus
  extern uint32_t (fg256d_ld_u16)(uint64_t, uint64_t);
#endif
  t39 = fg256d_ld_u16(t33, t38);
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
  extern uint32_t (fg256d_ld_u8)(uint64_t, uint64_t);
#endif
  t48 = fg256d_ld_u8(t42, t47);
  v12 = ((uint32_t)(t48));
  t49 = ((int64_t)(((int64_t)UINT64_C(4))));
  t50 = ((int64_t)(v4));
  t51 = ((int64_t)(((int64_t)UINT64_C(2))));
  t52 = ((int64_t)(((int64_t)fg256dd_gg_div(t50, t51))));
  v13 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t49) * (uint64_t)(t52)))))));
  t53 = ((float)(v9));
  t54 = ((float)(((float)(fg256dd_gg_double_bits(UINT64_C(4602678819172646912))))));
  t55 = ((uint32_t)(v12));
  t56 = ((uint32_t)(v13));
  t57 = ((uint32_t)(((t55) >> ((uint64_t)(t56) & 31))));
  t58 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(15))))));
  t59 = ((float)(((float)(((t57) & (t58))))));
  t60 = ((float)(((t54) + (t59))));
  t61 = ((float)(((t53) * (t60))));
  t62 = ((float)(((float)(fg256dd_gg_double_bits(UINT64_C(4598175219545276416))))));
  v14 = ((float)(((t61) * (t62))));
  t63 = ((uint32_t)(v10));
  t64 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(9))))));
  t65 = ((uint32_t)(((t63) >> ((uint64_t)(t64) & 31))));
#ifndef __cplusplus
  extern uint32_t (fg256d_ksign)(uint32_t);
#endif
  t66 = fg256d_ksign(t65);
  v15 = ((uint32_t)(t66));
  t67 = ((uint32_t)(v11));
#ifndef __cplusplus
  extern uint64_t (fg256d_grid)(uint32_t);
#endif
  t68 = fg256d_grid(t67);
  v16 = ((uint64_t)(t68));
  v17 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t69 = ((int64_t)(v17));
  t70 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  if (!(((t69) < (t70)))) goto label2;
  {
  t71 = ((int64_t)(v17));
  t72 = ((int64_t)(((int64_t)UINT64_C(8))));
  v18 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t71) * (uint64_t)(t72)))))));
  t73 = ((uint64_t)(v16));
  t74 = ((uint64_t)(v18));
  t75 = ((uint64_t)(((t73) >> ((uint64_t)(t74) & 63))));
  t76 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(255))))));
  v19 = ((int64_t)(((int64_t)(((t75) & (t76))))));
  t77 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t78 = ((int64_t)(v17));
  v20 = ((int64_t)(((int64_t)((uint64_t)(t77) << ((uint64_t)(t78) & 63)))));
  t79 = ((int64_t)(((int64_t)(v15))));
  t80 = ((int64_t)(v20));
  v21 = ((int64_t)(((t79) & (t80))));
  t81 = ((uint64_t)(v1));
  t82 = ((int64_t)(v7));
  t83 = ((int64_t)(v17));
  t84 = ((int64_t)(((int64_t)((uint64_t)(t82) + (uint64_t)(t83)))));
  t85 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t86 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t84) * (uint64_t)(t85)))))));
  t87 = ((float)(v14));
  t88 = ((float)(((float)(v19))));
  t89 = ((float)(((t87) * (t88))));
  t90 = ((int64_t)(v21));
  t91 = fg256dd_gg_function_9(t90, base + 0);
  t92 = ((float)(t91));
  t93 = ((float)(((t89) * (t92))));
#ifndef __cplusplus
  extern void (fg256d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg256d_st_f32(t81, t86, t93);
  t94 = ((int64_t)(v17));
  t95 = ((int64_t)(((int64_t)UINT64_C(1))));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t94) + (uint64_t)(t95)))));
  }
  goto label1;
label2:;
  }
fg256dd_gg_done:;
}
