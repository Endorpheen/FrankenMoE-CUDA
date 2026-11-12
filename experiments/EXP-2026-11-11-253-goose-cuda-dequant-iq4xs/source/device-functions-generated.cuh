#pragma once
static __device__ __forceinline__ void fg253dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg253dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg253dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg253dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg253dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg253dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ int64_t fg253dd_gg_function_7(int64_t p0, int64_t base);
static __device__ __forceinline__ void fg253dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg253dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base);
static __device__ __forceinline__ void fg253dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg253dd_gg_div(int64_t a, int64_t b) { if (!b) fg253dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg253dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg253dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg253dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg253dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg253dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg253dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg253dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg253dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ int64_t fg253dd_gg_function_7(int64_t p0, int64_t base) {
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
  goto fg253dd_gg_done;
  }
  }
  t5 = ((int64_t)(v0));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t7 = ((t5) == (t6));
  if (t7) {
  {
  t8 = ((int64_t)(((int64_t)UINT64_C(104))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t8)))));
  goto fg253dd_gg_done;
  }
  }
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t11 = ((t9) == (t10));
  if (t11) {
  {
  t12 = ((int64_t)(((int64_t)UINT64_C(83))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t12)))));
  goto fg253dd_gg_done;
  }
  }
  t13 = ((int64_t)(v0));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t15 = ((t13) == (t14));
  if (t15) {
  {
  t16 = ((int64_t)(((int64_t)UINT64_C(65))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t16)))));
  goto fg253dd_gg_done;
  }
  }
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t19 = ((t17) == (t18));
  if (t19) {
  {
  t20 = ((int64_t)(((int64_t)UINT64_C(49))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t20)))));
  goto fg253dd_gg_done;
  }
  }
  t21 = ((int64_t)(v0));
  t22 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t23 = ((t21) == (t22));
  if (t23) {
  {
  t24 = ((int64_t)(((int64_t)UINT64_C(35))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t24)))));
  goto fg253dd_gg_done;
  }
  }
  t25 = ((int64_t)(v0));
  t26 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(6))))));
  t27 = ((t25) == (t26));
  if (t27) {
  {
  t28 = ((int64_t)(((int64_t)UINT64_C(22))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t28)))));
  goto fg253dd_gg_done;
  }
  }
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(7))))));
  t31 = ((t29) == (t30));
  if (t31) {
  {
  t32 = ((int64_t)(((int64_t)UINT64_C(10))));
  t0 = ((int64_t)(((int64_t)((uint64_t)0 - (uint64_t)(t32)))));
  goto fg253dd_gg_done;
  }
  }
  t33 = ((int64_t)(v0));
  t34 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t35 = ((t33) == (t34));
  if (t35) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto fg253dd_gg_done;
  }
  }
  t36 = ((int64_t)(v0));
  t37 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(9))))));
  t38 = ((t36) == (t37));
  if (t38) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(13))))));
  goto fg253dd_gg_done;
  }
  }
  t39 = ((int64_t)(v0));
  t40 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(10))))));
  t41 = ((t39) == (t40));
  if (t41) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(25))))));
  goto fg253dd_gg_done;
  }
  }
  t42 = ((int64_t)(v0));
  t43 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(11))))));
  t44 = ((t42) == (t43));
  if (t44) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(38))))));
  goto fg253dd_gg_done;
  }
  }
  t45 = ((int64_t)(v0));
  t46 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(12))))));
  t47 = ((t45) == (t46));
  if (t47) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(53))))));
  goto fg253dd_gg_done;
  }
  }
  t48 = ((int64_t)(v0));
  t49 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(13))))));
  t50 = ((t48) == (t49));
  if (t50) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(69))))));
  goto fg253dd_gg_done;
  }
  }
  t51 = ((int64_t)(v0));
  t52 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(14))))));
  t53 = ((t51) == (t52));
  if (t53) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(89))))));
  goto fg253dd_gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(113))))));
  }
fg253dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg253dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg253d_fail)(int64_t);
#endif
  fg253d_fail(t2);
  }
  }
  }
fg253dd_gg_done:;
}
static __device__ __forceinline__ void fg253dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  uint64_t t25;
  float t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  uint64_t t33;
  uint64_t t34;
  uint32_t t35;
  uint64_t t36;
  int64_t t37;
  int64_t t38;
  uint64_t t39;
  uint32_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint32_t t47;
  uint32_t t48;
  uint32_t t49;
  uint32_t t50;
  uint32_t t51;
  uint32_t t52;
  uint32_t t53;
  uint32_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  float t59;
  int64_t t60;
  int64_t t61;
  float t62;
  int64_t t63;
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
  uint64_t t75;
  int64_t t76;
  int64_t t77;
  uint64_t t78;
  uint32_t t79;
  uint32_t t80;
  uint32_t t81;
  int64_t t82;
  uint32_t t83;
  int64_t t84;
  int64_t t85;
  uint64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  uint64_t t91;
  float t92;
  float t93;
  float t94;
  uint64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  uint64_t t102;
  float t103;
  float t104;
  float t105;
  int64_t t106;
  int64_t t107;
  uint64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  float v8;
  int64_t v9;
  uint32_t v10;
  uint32_t v11;
  uint32_t v12;
  uint32_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  float v17;
  int64_t v18;
  int64_t v19;
  uint32_t v20;
  int64_t v21;
  int64_t v22;
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
  fg253dd_gg_function_2(t6, t7, base + 0);
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)UINT64_C(8))));
  v4 = ((int64_t)(((int64_t)fg253dd_gg_mod(t8, t9))));
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(((int64_t)UINT64_C(8))));
  v5 = ((int64_t)(((int64_t)fg253dd_gg_div(t10, t11))));
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(136))))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  t14 = ((int64_t)(v2));
  t15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(256))))));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))));
  t17 = ((int64_t)(((int64_t)UINT64_C(32))));
  t18 = ((int64_t)(v4));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) * (uint64_t)(t18)))));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t16) + (uint64_t)(t19)))));
  t21 = ((int64_t)(((int64_t)UINT64_C(4))));
  t22 = ((int64_t)(v5));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) * (uint64_t)(t22)))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t20) + (uint64_t)(t23)))));
  t24 = ((uint64_t)(v0));
  t25 = ((uint64_t)(((uint64_t)(v6))));
#ifndef __cplusplus
  extern float (fg253d_ld_half)(uint64_t, uint64_t);
#endif
  t26 = fg253d_ld_half(t24, t25);
  v8 = ((float)(t26));
  t27 = ((int64_t)(v6));
  t28 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) + (uint64_t)(t28)))));
  t30 = ((int64_t)(v4));
  t31 = ((int64_t)(((int64_t)UINT64_C(2))));
  t32 = ((int64_t)(((int64_t)fg253dd_gg_div(t30, t31))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t32)))));
  t33 = ((uint64_t)(v0));
  t34 = ((uint64_t)(((uint64_t)(v9))));
#ifndef __cplusplus
  extern uint32_t (fg253d_ld_u8)(uint64_t, uint64_t);
#endif
  t35 = fg253d_ld_u8(t33, t34);
  v10 = ((uint32_t)(t35));
  t36 = ((uint64_t)(v0));
  t37 = ((int64_t)(v6));
  t38 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t39 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t37) + (uint64_t)(t38)))))));
#ifndef __cplusplus
  extern uint32_t (fg253d_ld_u16)(uint64_t, uint64_t);
#endif
  t40 = fg253d_ld_u16(t36, t39);
  v11 = ((uint32_t)(t40));
  t41 = ((int64_t)(((int64_t)UINT64_C(4))));
  t42 = ((int64_t)(v4));
  t43 = ((int64_t)(((int64_t)UINT64_C(2))));
  t44 = ((int64_t)(((int64_t)fg253dd_gg_mod(t42, t43))));
  v12 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t41) * (uint64_t)(t44)))))));
  t45 = ((int64_t)(((int64_t)UINT64_C(2))));
  t46 = ((int64_t)(v4));
  v13 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t45) * (uint64_t)(t46)))))));
  t47 = ((uint32_t)(v10));
  t48 = ((uint32_t)(v12));
  t49 = ((uint32_t)(((t47) >> ((uint64_t)(t48) & 31))));
  t50 = ((uint32_t)(((int64_t)UINT64_C(15))));
  v14 = ((int64_t)(((int64_t)(((t49) & (t50))))));
  t51 = ((uint32_t)(v11));
  t52 = ((uint32_t)(v13));
  t53 = ((uint32_t)(((t51) >> ((uint64_t)(t52) & 31))));
  t54 = ((uint32_t)(((int64_t)UINT64_C(3))));
  t55 = ((int64_t)(((int64_t)(((t53) & (t54))))));
  t56 = ((int64_t)(((int64_t)UINT64_C(4))));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t55) << ((uint64_t)(t56) & 63)))));
  t57 = ((int64_t)(v14));
  t58 = ((int64_t)(v15));
  v16 = ((int64_t)(((t57) | (t58))));
  t59 = ((float)(v8));
  t60 = ((int64_t)(v16));
  t61 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t62 = ((float)(((float)(((int64_t)((uint64_t)(t60) - (uint64_t)(t61)))))));
  v17 = ((float)(((t59) * (t62))));
  t63 = ((int64_t)(v6));
  t64 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t65 = ((int64_t)(((int64_t)((uint64_t)(t63) + (uint64_t)(t64)))));
  t66 = ((int64_t)(((int64_t)UINT64_C(16))));
  t67 = ((int64_t)(v4));
  t68 = ((int64_t)(((int64_t)((uint64_t)(t66) * (uint64_t)(t67)))));
  t69 = ((int64_t)(((int64_t)((uint64_t)(t65) + (uint64_t)(t68)))));
  t70 = ((int64_t)(((int64_t)UINT64_C(4))));
  t71 = ((int64_t)(v5));
  t72 = ((int64_t)(((int64_t)((uint64_t)(t70) * (uint64_t)(t71)))));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t69) + (uint64_t)(t72)))));
  v19 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t73 = ((int64_t)(v19));
  t74 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  if (!(((t73) < (t74)))) goto label2;
  {
  t75 = ((uint64_t)(v0));
  t76 = ((int64_t)(v18));
  t77 = ((int64_t)(v19));
  t78 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t76) + (uint64_t)(t77)))))));
#ifndef __cplusplus
  extern uint32_t (fg253d_ld_u8)(uint64_t, uint64_t);
#endif
  t79 = fg253d_ld_u8(t75, t78);
  v20 = ((uint32_t)(t79));
  t80 = ((uint32_t)(v20));
  t81 = ((uint32_t)(((int64_t)UINT64_C(15))));
  t82 = ((int64_t)(((int64_t)(((t80) & (t81))))));
  v21 = fg253dd_gg_function_7(t82, base + 0);
  t83 = ((uint32_t)(v20));
  t84 = ((int64_t)(((int64_t)UINT64_C(4))));
  t85 = ((int64_t)(((int64_t)(((t83) >> ((uint64_t)(t84) & 31))))));
  v22 = fg253dd_gg_function_7(t85, base + 0);
  t86 = ((uint64_t)(v1));
  t87 = ((int64_t)(v7));
  t88 = ((int64_t)(v19));
  t89 = ((int64_t)(((int64_t)((uint64_t)(t87) + (uint64_t)(t88)))));
  t90 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t91 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t89) * (uint64_t)(t90)))))));
  t92 = ((float)(v17));
  t93 = ((float)(((float)(v21))));
  t94 = ((float)(((t92) * (t93))));
#ifndef __cplusplus
  extern void (fg253d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg253d_st_f32(t86, t91, t94);
  t95 = ((uint64_t)(v1));
  t96 = ((int64_t)(v7));
  t97 = ((int64_t)(v19));
  t98 = ((int64_t)(((int64_t)((uint64_t)(t96) + (uint64_t)(t97)))));
  t99 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t100 = ((int64_t)(((int64_t)((uint64_t)(t98) + (uint64_t)(t99)))));
  t101 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t102 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t100) * (uint64_t)(t101)))))));
  t103 = ((float)(v17));
  t104 = ((float)(((float)(v22))));
  t105 = ((float)(((t103) * (t104))));
#ifndef __cplusplus
  extern void (fg253d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg253d_st_f32(t95, t102, t105);
  t106 = ((int64_t)(v19));
  t107 = ((int64_t)(((int64_t)UINT64_C(1))));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t106) + (uint64_t)(t107)))));
  }
  goto label1;
label2:;
  }
fg253dd_gg_done:;
}
