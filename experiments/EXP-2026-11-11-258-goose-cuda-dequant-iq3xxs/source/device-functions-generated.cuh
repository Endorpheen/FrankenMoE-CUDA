#pragma once
static __device__ __forceinline__ void fg258dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg258dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg258dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg258dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg258dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg258dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ float fg258dd_gg_function_9(int64_t p0, int64_t base);
static __device__ __forceinline__ void fg258dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg258dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base);
static __device__ __forceinline__ void fg258dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg258dd_gg_div(int64_t a, int64_t b) { if (!b) fg258dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg258dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg258dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg258dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg258dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg258dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg258dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg258dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg258dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ float fg258dd_gg_function_9(int64_t p0, int64_t base) {
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
  t4 = ((float)(fg258dd_gg_double_bits(UINT64_C(4607182418800017408))));
  t0 = ((float)((-(t4))));
  goto fg258dd_gg_done;
  }
  }
  t0 = ((float)(((float)(fg258dd_gg_double_bits(UINT64_C(4607182418800017408))))));
  }
fg258dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg258dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg258d_fail)(int64_t);
#endif
  fg258d_fail(t2);
  }
  }
  }
fg258dd_gg_done:;
}
static __device__ __forceinline__ void fg258dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  uint64_t t35;
  uint32_t t36;
  uint64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  uint64_t t44;
  uint32_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  uint64_t t52;
  uint64_t t53;
  uint32_t t54;
  uint64_t t55;
  int64_t t56;
  int64_t t57;
  uint64_t t58;
  uint32_t t59;
  uint32_t t60;
  uint32_t t61;
  uint32_t t62;
  uint32_t t63;
  uint32_t t64;
  uint32_t t65;
  uint64_t t66;
  uint64_t t67;
  float t68;
  float t69;
  float t70;
  float t71;
  float t72;
  float t73;
  float t74;
  int64_t t75;
  int64_t t76;
  uint32_t t77;
  uint32_t t78;
  uint32_t t79;
  uint32_t t80;
  uint32_t t81;
  uint32_t t82;
  uint32_t t83;
  uint32_t t84;
  uint32_t t85;
  uint32_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  uint32_t t91;
  uint64_t t92;
  uint32_t t93;
  uint32_t t94;
  uint32_t t95;
  uint64_t t96;
  uint32_t t97;
  uint32_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  int64_t t108;
  uint64_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  uint64_t t114;
  float t115;
  float t116;
  float t117;
  int64_t t118;
  float t119;
  float t120;
  float t121;
  uint64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  uint64_t t129;
  float t130;
  float t131;
  float t132;
  int64_t t133;
  float t134;
  float t135;
  float t136;
  int64_t t137;
  int64_t t138;
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
  uint32_t v10;
  int64_t v11;
  uint32_t v12;
  uint32_t v13;
  uint32_t v14;
  uint32_t v15;
  float v16;
  uint32_t v17;
  uint32_t v18;
  uint32_t v19;
  uint32_t v20;
  int64_t v21;
  uint64_t v22;
  int64_t v23;
  int64_t v24;
  int64_t v25;
  int64_t v26;
  int64_t v27;
  int64_t v28;
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
  fg258dd_gg_function_2(t6, t7, base + 0);
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)UINT64_C(8))));
  v4 = ((int64_t)(((int64_t)fg258dd_gg_div(t8, t9))));
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(((int64_t)UINT64_C(8))));
  v5 = ((int64_t)(((int64_t)fg258dd_gg_mod(t10, t11))));
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(98))))));
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
  t31 = ((int64_t)(v8));
  t32 = ((int64_t)(((int64_t)UINT64_C(2))));
  t33 = ((int64_t)(v4));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) * (uint64_t)(t33)))));
  t35 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t34)))))));
#ifndef __cplusplus
  extern uint32_t (fg258d_ld_u8)(uint64_t, uint64_t);
#endif
  t36 = fg258d_ld_u8(t30, t35);
  v9 = ((uint32_t)(t36));
  t37 = ((uint64_t)(v0));
  t38 = ((int64_t)(v8));
  t39 = ((int64_t)(((int64_t)UINT64_C(2))));
  t40 = ((int64_t)(v4));
  t41 = ((int64_t)(((int64_t)((uint64_t)(t39) * (uint64_t)(t40)))));
  t42 = ((int64_t)(((int64_t)((uint64_t)(t38) + (uint64_t)(t41)))));
  t43 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t44 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t42) + (uint64_t)(t43)))))));
#ifndef __cplusplus
  extern uint32_t (fg258d_ld_u8)(uint64_t, uint64_t);
#endif
  t45 = fg258d_ld_u8(t37, t44);
  v10 = ((uint32_t)(t45));
  t46 = ((int64_t)(v6));
  t47 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(66))))));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t46) + (uint64_t)(t47)))));
  t49 = ((int64_t)(((int64_t)UINT64_C(4))));
  t50 = ((int64_t)(v5));
  t51 = ((int64_t)(((int64_t)((uint64_t)(t49) * (uint64_t)(t50)))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t48) + (uint64_t)(t51)))));
  t52 = ((uint64_t)(v0));
  t53 = ((uint64_t)(((uint64_t)(v11))));
#ifndef __cplusplus
  extern uint32_t (fg258d_ld_u16)(uint64_t, uint64_t);
#endif
  t54 = fg258d_ld_u16(t52, t53);
  v12 = ((uint32_t)(t54));
  t55 = ((uint64_t)(v0));
  t56 = ((int64_t)(v11));
  t57 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t58 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t56) + (uint64_t)(t57)))))));
#ifndef __cplusplus
  extern uint32_t (fg258d_ld_u16)(uint64_t, uint64_t);
#endif
  t59 = fg258d_ld_u16(t55, t58);
  v13 = ((uint32_t)(t59));
  t60 = ((uint32_t)(v12));
  t61 = ((uint32_t)(v13));
  t62 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(16))))));
  t63 = ((uint32_t)(((uint32_t)((uint64_t)(t61) << ((uint64_t)(t62) & 31)))));
  v14 = ((uint32_t)(((t60) | (t63))));
  t64 = ((uint32_t)(v14));
  t65 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(28))))));
  v15 = ((uint32_t)(((t64) >> ((uint64_t)(t65) & 31))));
  t66 = ((uint64_t)(v0));
  t67 = ((uint64_t)(((uint64_t)(v6))));
#ifndef __cplusplus
  extern float (fg258d_ld_half)(uint64_t, uint64_t);
#endif
  t68 = fg258d_ld_half(t66, t67);
  t69 = ((float)(t68));
  t70 = ((float)(((float)(fg258dd_gg_double_bits(UINT64_C(4602678819172646912))))));
  t71 = ((float)(((float)(v15))));
  t72 = ((float)(((t70) + (t71))));
  t73 = ((float)(((t69) * (t72))));
  t74 = ((float)(((float)(fg258dd_gg_double_bits(UINT64_C(4602678819172646912))))));
  v16 = ((float)(((t73) * (t74))));
  t75 = ((int64_t)(((int64_t)UINT64_C(7))));
  t76 = ((int64_t)(v4));
  v17 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t75) * (uint64_t)(t76)))))));
  t77 = ((uint32_t)(v14));
  t78 = ((uint32_t)(v17));
  t79 = ((uint32_t)(((t77) >> ((uint64_t)(t78) & 31))));
  t80 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(127))))));
  t81 = ((uint32_t)(((t79) & (t80))));
#ifndef __cplusplus
  extern uint32_t (fg258d_ksign)(uint32_t);
#endif
  t82 = fg258d_ksign(t81);
  v18 = ((uint32_t)(t82));
  t83 = ((uint32_t)(v9));
#ifndef __cplusplus
  extern uint32_t (fg258d_grid)(uint32_t);
#endif
  t84 = fg258d_grid(t83);
  v19 = ((uint32_t)(t84));
  t85 = ((uint32_t)(v10));
#ifndef __cplusplus
  extern uint32_t (fg258d_grid)(uint32_t);
#endif
  t86 = fg258d_grid(t85);
  v20 = ((uint32_t)(t86));
  v21 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t87 = ((int64_t)(v21));
  t88 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  if (!(((t87) < (t88)))) goto label2;
  {
  t89 = ((int64_t)(v21));
  t90 = ((int64_t)(((int64_t)UINT64_C(8))));
  v22 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t89) * (uint64_t)(t90)))))));
  t91 = ((uint32_t)(v19));
  t92 = ((uint64_t)(v22));
  t93 = ((uint32_t)(((t91) >> ((uint64_t)(t92) & 31))));
  t94 = ((uint32_t)(((uint64_t)(((int64_t)UINT64_C(255))))));
  v23 = ((int64_t)(((int64_t)(((t93) & (t94))))));
  t95 = ((uint32_t)(v20));
  t96 = ((uint64_t)(v22));
  t97 = ((uint32_t)(((t95) >> ((uint64_t)(t96) & 31))));
  t98 = ((uint32_t)(((uint64_t)(((int64_t)UINT64_C(255))))));
  v24 = ((int64_t)(((int64_t)(((t97) & (t98))))));
  t99 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t100 = ((int64_t)(v21));
  v25 = ((int64_t)(((int64_t)((uint64_t)(t99) << ((uint64_t)(t100) & 63)))));
  t101 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t102 = ((int64_t)(v21));
  t103 = ((int64_t)(((int64_t)UINT64_C(4))));
  t104 = ((int64_t)(((int64_t)((uint64_t)(t102) + (uint64_t)(t103)))));
  v26 = ((int64_t)(((int64_t)((uint64_t)(t101) << ((uint64_t)(t104) & 63)))));
  t105 = ((int64_t)(((int64_t)(v18))));
  t106 = ((int64_t)(v25));
  v27 = ((int64_t)(((t105) & (t106))));
  t107 = ((int64_t)(((int64_t)(v18))));
  t108 = ((int64_t)(v26));
  v28 = ((int64_t)(((t107) & (t108))));
  t109 = ((uint64_t)(v1));
  t110 = ((int64_t)(v7));
  t111 = ((int64_t)(v21));
  t112 = ((int64_t)(((int64_t)((uint64_t)(t110) + (uint64_t)(t111)))));
  t113 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t114 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t112) * (uint64_t)(t113)))))));
  t115 = ((float)(v16));
  t116 = ((float)(((float)(v23))));
  t117 = ((float)(((t115) * (t116))));
  t118 = ((int64_t)(v27));
  t119 = fg258dd_gg_function_9(t118, base + 0);
  t120 = ((float)(t119));
  t121 = ((float)(((t117) * (t120))));
#ifndef __cplusplus
  extern void (fg258d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg258d_st_f32(t109, t114, t121);
  t122 = ((uint64_t)(v1));
  t123 = ((int64_t)(v7));
  t124 = ((int64_t)(v21));
  t125 = ((int64_t)(((int64_t)((uint64_t)(t123) + (uint64_t)(t124)))));
  t126 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t127 = ((int64_t)(((int64_t)((uint64_t)(t125) + (uint64_t)(t126)))));
  t128 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t129 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t127) * (uint64_t)(t128)))))));
  t130 = ((float)(v16));
  t131 = ((float)(((float)(v24))));
  t132 = ((float)(((t130) * (t131))));
  t133 = ((int64_t)(v28));
  t134 = fg258dd_gg_function_9(t133, base + 0);
  t135 = ((float)(t134));
  t136 = ((float)(((t132) * (t135))));
#ifndef __cplusplus
  extern void (fg258d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg258d_st_f32(t122, t129, t136);
  t137 = ((int64_t)(v21));
  t138 = ((int64_t)(((int64_t)UINT64_C(1))));
  v21 = ((int64_t)(((int64_t)((uint64_t)(t137) + (uint64_t)(t138)))));
  }
  goto label1;
label2:;
  }
fg258dd_gg_done:;
}
