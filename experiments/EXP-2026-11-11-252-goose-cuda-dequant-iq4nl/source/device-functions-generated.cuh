#pragma once
static __device__ __forceinline__ void fg252dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg252dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg252dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg252dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg252dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg252dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ int64_t fg252dd_gg_function_6(int64_t p0, int64_t base);
static __device__ __forceinline__ void fg252dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg252dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static __device__ __forceinline__ void fg252dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg252dd_gg_div(int64_t a, int64_t b) { if (!b) fg252dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg252dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg252dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg252dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg252dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg252dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg252dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg252dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg252dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ int64_t fg252dd_gg_function_6(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  uint8_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  uint8_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  uint8_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  uint8_t t35;
  int64_t t36;
  int64_t t37;
  uint8_t t38;
  int64_t t39;
  int64_t t40;
  uint8_t t41;
  int64_t t42;
  int64_t t43;
  uint8_t t44;
  int64_t t45;
  int64_t t46;
  uint8_t t47;
  int64_t t48;
  int64_t t49;
  uint8_t t50;
  int64_t t51;
  int64_t t52;
  uint8_t t53;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = ((int64_t)(((int64_t)UINT64_C(127))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t4)))));
  goto fg252dd_gg_done;
  }
  }
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t7 = ((t5) == (t6));
  if (t7) {
  {
  t8 = ((int64_t)(((int64_t)UINT64_C(104))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t8)))));
  goto fg252dd_gg_done;
  }
  }
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t11 = ((t9) == (t10));
  if (t11) {
  {
  t12 = ((int64_t)(((int64_t)UINT64_C(83))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t12)))));
  goto fg252dd_gg_done;
  }
  }
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t15 = ((t13) == (t14));
  if (t15) {
  {
  t16 = ((int64_t)(((int64_t)UINT64_C(65))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t16)))));
  goto fg252dd_gg_done;
  }
  }
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t19 = ((t17) == (t18));
  if (t19) {
  {
  t20 = ((int64_t)(((int64_t)UINT64_C(49))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t20)))));
  goto fg252dd_gg_done;
  }
  }
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t23 = ((t21) == (t22));
  if (t23) {
  {
  t24 = ((int64_t)(((int64_t)UINT64_C(35))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t24)))));
  goto fg252dd_gg_done;
  }
  }
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(6))))));
  t27 = ((t25) == (t26));
  if (t27) {
  {
  t28 = ((int64_t)(((int64_t)UINT64_C(22))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t28)))));
  goto fg252dd_gg_done;
  }
  }
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(7))))));
  t31 = ((t29) == (t30));
  if (t31) {
  {
  t32 = ((int64_t)(((int64_t)UINT64_C(10))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t32)))));
  goto fg252dd_gg_done;
  }
  }
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t35 = ((t33) == (t34));
  if (t35) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto fg252dd_gg_done;
  }
  }
  t36 = ((int64_t)(v0));
  t37 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(9))))));
  t38 = ((t36) == (t37));
  if (t38) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(13))))));
  goto fg252dd_gg_done;
  }
  }
  t39 = ((int64_t)(v0));
  t40 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(10))))));
  t41 = ((t39) == (t40));
  if (t41) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(25))))));
  goto fg252dd_gg_done;
  }
  }
  t42 = ((int64_t)(v0));
  t43 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(11))))));
  t44 = ((t42) == (t43));
  if (t44) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(38))))));
  goto fg252dd_gg_done;
  }
  }
  t45 = ((int64_t)(v0));
  t46 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(12))))));
  t47 = ((t45) == (t46));
  if (t47) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(53))))));
  goto fg252dd_gg_done;
  }
  }
  t48 = ((int64_t)(v0));
  t49 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(13))))));
  t50 = ((t48) == (t49));
  if (t50) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(69))))));
  goto fg252dd_gg_done;
  }
  }
  t51 = ((int64_t)(v0));
  t52 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(14))))));
  t53 = ((t51) == (t52));
  if (t53) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(89))))));
  goto fg252dd_gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(113))))));
  }
fg252dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg252dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg252d_fail)(int64_t);
#endif
  fg252d_fail(t2);
  }
  }
  }
fg252dd_gg_done:;
}
static __device__ __forceinline__ void fg252dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
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
  uint8_t t16;
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
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  uint64_t t37;
  uint64_t t38;
  float t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  uint64_t t48;
  int64_t t49;
  int64_t t50;
  uint64_t t51;
  uint32_t t52;
  uint32_t t53;
  uint32_t t54;
  int64_t t55;
  uint32_t t56;
  int64_t t57;
  int64_t t58;
  uint64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  uint64_t t64;
  float t65;
  float t66;
  float t67;
  uint64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  uint64_t t75;
  float t76;
  float t77;
  float t78;
  int64_t t79;
  int64_t t80;
  uint64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  float v11;
  int64_t v12;
  int64_t v13;
  uint32_t v14;
  int64_t v15;
  int64_t v16;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t0 = ((int64_t)(v4));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t3 = ((t4) < (t5));
  }
  t6 = t3;
  t7 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg252dd_gg_function_2(t6, t7, base + 0);
  t8 = ((int64_t)(((int64_t)UINT64_C(8))));
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t8) * (uint64_t)(t9)))));
  t11 = ((int64_t)(v4));
  t12 = ((int64_t)(((int64_t)UINT64_C(8))));
  t13 = ((int64_t)(((int64_t)fg252dd_gg_mod(t11, t12))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t13)))));
  t14 = ((int64_t)(v5));
  t15 = ((int64_t)(v3));
  t16 = ((t14) >= (t15));
  if (t16) {
  {
  goto fg252dd_gg_done;
  }
  }
  t17 = ((int64_t)(v4));
  t18 = ((int64_t)(((int64_t)UINT64_C(8))));
  v6 = ((int64_t)(((int64_t)fg252dd_gg_mod(t17, t18))));
  t19 = ((int64_t)(v4));
  t20 = ((int64_t)(((int64_t)UINT64_C(8))));
  v7 = ((int64_t)(((int64_t)fg252dd_gg_div(t19, t20))));
  t21 = ((int64_t)(v2));
  t22 = ((int64_t)(((int64_t)UINT64_C(8))));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) * (uint64_t)(t22)))));
  t24 = ((int64_t)(v6));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t23) + (uint64_t)(t24)))));
  t25 = ((int64_t)(v8));
  t26 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(18))))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t25) * (uint64_t)(t26)))));
  t27 = ((int64_t)(v2));
  t28 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(256))))));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) * (uint64_t)(t28)))));
  t30 = ((int64_t)(((int64_t)UINT64_C(32))));
  t31 = ((int64_t)(v6));
  t32 = ((int64_t)(((int64_t)((uint64_t)(t30) * (uint64_t)(t31)))));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t32)))));
  t34 = ((int64_t)(((int64_t)UINT64_C(4))));
  t35 = ((int64_t)(v7));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t34) * (uint64_t)(t35)))));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t33) + (uint64_t)(t36)))));
  t37 = ((uint64_t)(v0));
  t38 = ((uint64_t)(((uint64_t)(v9))));
#ifndef __cplusplus
  extern float (fg252d_ld_half)(uint64_t, uint64_t);
#endif
  t39 = fg252d_ld_half(t37, t38);
  v11 = ((float)(t39));
  t40 = ((int64_t)(v9));
  t41 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t42 = ((int64_t)(((int64_t)((uint64_t)(t40) + (uint64_t)(t41)))));
  t43 = ((int64_t)(((int64_t)UINT64_C(4))));
  t44 = ((int64_t)(v7));
  t45 = ((int64_t)(((int64_t)((uint64_t)(t43) * (uint64_t)(t44)))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t42) + (uint64_t)(t45)))));
  v13 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t46 = ((int64_t)(v13));
  t47 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  if (!(((t46) < (t47)))) goto label2;
  {
  t48 = ((uint64_t)(v0));
  t49 = ((int64_t)(v12));
  t50 = ((int64_t)(v13));
  t51 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t49) + (uint64_t)(t50)))))));
#ifndef __cplusplus
  extern uint32_t (fg252d_ld_u8)(uint64_t, uint64_t);
#endif
  t52 = fg252d_ld_u8(t48, t51);
  v14 = ((uint32_t)(t52));
  t53 = ((uint32_t)(v14));
  t54 = ((uint32_t)(((int64_t)UINT64_C(15))));
  t55 = ((int64_t)(((int64_t)(((t53) & (t54))))));
  v15 = fg252dd_gg_function_6(t55, base + 0);
  t56 = ((uint32_t)(v14));
  t57 = ((int64_t)(((int64_t)UINT64_C(4))));
  t58 = ((int64_t)(((int64_t)(((t56) >> ((uint64_t)(t57) & 31))))));
  v16 = fg252dd_gg_function_6(t58, base + 0);
  t59 = ((uint64_t)(v1));
  t60 = ((int64_t)(v10));
  t61 = ((int64_t)(v13));
  t62 = ((int64_t)(((int64_t)((uint64_t)(t60) + (uint64_t)(t61)))));
  t63 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t64 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t62) * (uint64_t)(t63)))))));
  t65 = ((float)(v11));
  t66 = ((float)(((float)(v15))));
  t67 = ((float)(((t65) * (t66))));
#ifndef __cplusplus
  extern void (fg252d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg252d_st_f32(t59, t64, t67);
  t68 = ((uint64_t)(v1));
  t69 = ((int64_t)(v10));
  t70 = ((int64_t)(v13));
  t71 = ((int64_t)(((int64_t)((uint64_t)(t69) + (uint64_t)(t70)))));
  t72 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t73 = ((int64_t)(((int64_t)((uint64_t)(t71) + (uint64_t)(t72)))));
  t74 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t75 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t73) * (uint64_t)(t74)))))));
  t76 = ((float)(v11));
  t77 = ((float)(((float)(v16))));
  t78 = ((float)(((t76) * (t77))));
#ifndef __cplusplus
  extern void (fg252d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg252d_st_f32(t68, t75, t78);
  t79 = ((int64_t)(v13));
  t80 = ((int64_t)(((int64_t)UINT64_C(1))));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t79) + (uint64_t)(t80)))));
  }
  goto label1;
label2:;
  }
fg252dd_gg_done:;
}
