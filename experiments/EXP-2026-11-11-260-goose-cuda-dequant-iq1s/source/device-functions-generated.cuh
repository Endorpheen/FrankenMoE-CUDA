#pragma once
static __device__ __forceinline__ void fg260dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg260dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg260dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg260dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg260dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg260dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ void fg260dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg260dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base);
static __device__ __forceinline__ void fg260dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg260dd_gg_div(int64_t a, int64_t b) { if (!b) fg260dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg260dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg260dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg260dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg260dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg260dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg260dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg260dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg260dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ void fg260dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg260d_fail)(int64_t);
#endif
  fg260d_fail(t2);
  }
  }
  }
fg260dd_gg_done:;
}
static __device__ __forceinline__ void fg260dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  uint64_t t31;
  uint32_t t32;
  uint64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  uint64_t t42;
  uint32_t t43;
  int64_t t44;
  int64_t t45;
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
  float t56;
  uint32_t t57;
  uint32_t t58;
  uint8_t t59;
  float t60;
  uint32_t t61;
  uint32_t t62;
  uint32_t t63;
  uint32_t t64;
  uint64_t t65;
  uint64_t t66;
  float t67;
  float t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  float t73;
  uint32_t t74;
  uint32_t t75;
  uint32_t t76;
  uint32_t t77;
  uint32_t t78;
  uint32_t t79;
  uint32_t t80;
  uint32_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  uint8_t t86;
  int64_t t87;
  int64_t t88;
  uint32_t t89;
  uint32_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  uint32_t t95;
  uint32_t t96;
  uint32_t t97;
  uint32_t t98;
  uint64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  uint64_t t104;
  float t105;
  float t106;
  float t107;
  float t108;
  float t109;
  int64_t t110;
  int64_t t111;
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
  float v15;
  uint32_t v16;
  uint32_t v17;
  uint32_t v18;
  int64_t v19;
  uint32_t v20;
  uint32_t v21;
  uint32_t v22;
  int64_t v23;
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
  fg260dd_gg_function_2(t6, t7, base + 0);
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)UINT64_C(8))));
  v4 = ((int64_t)(((int64_t)fg260dd_gg_div(t8, t9))));
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(((int64_t)UINT64_C(8))));
  v5 = ((int64_t)(((int64_t)fg260dd_gg_mod(t10, t11))));
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(50))))));
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
  t26 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(34))))));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t26)))));
  t28 = ((int64_t)(((int64_t)UINT64_C(2))));
  t29 = ((int64_t)(v5));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  t31 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t27) + (uint64_t)(t30)))))));
#ifndef __cplusplus
  extern uint32_t (fg260d_ld_u16)(uint64_t, uint64_t);
#endif
  t32 = fg260d_ld_u16(t24, t31);
  v8 = ((uint32_t)(t32));
  t33 = ((uint64_t)(v0));
  t34 = ((int64_t)(v6));
  t35 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t34) + (uint64_t)(t35)))));
  t37 = ((int64_t)(((int64_t)UINT64_C(4))));
  t38 = ((int64_t)(v5));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t37) * (uint64_t)(t38)))));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t36) + (uint64_t)(t39)))));
  t41 = ((int64_t)(v4));
  t42 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t40) + (uint64_t)(t41)))))));
#ifndef __cplusplus
  extern uint32_t (fg260d_ld_u8)(uint64_t, uint64_t);
#endif
  t43 = fg260d_ld_u8(t33, t42);
  v9 = ((uint32_t)(t43));
  t44 = ((int64_t)(((int64_t)UINT64_C(3))));
  t45 = ((int64_t)(v4));
  v10 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t44) * (uint64_t)(t45)))))));
  t46 = ((uint32_t)(v9));
  t47 = ((uint32_t)(v8));
  t48 = ((uint32_t)(v10));
  t49 = ((uint32_t)(((t47) >> ((uint64_t)(t48) & 31))));
  t50 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(7))))));
  t51 = ((uint32_t)(((t49) & (t50))));
  t52 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(8))))));
  t53 = ((uint32_t)(((uint32_t)((uint64_t)(t51) << ((uint64_t)(t52) & 31)))));
  v11 = ((uint32_t)(((t46) | (t53))));
  t54 = ((uint32_t)(v8));
  t55 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(32768))))));
  v12 = ((uint32_t)(((t54) & (t55))));
  t56 = ((float)(fg260dd_gg_double_bits(UINT64_C(4606056518893174784))));
  v13 = ((float)((-(t56))));
  t57 = ((uint32_t)(v12));
  t58 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(0))))));
  t59 = ((t57) > (t58));
  if (t59) {
  {
  t60 = ((float)(fg260dd_gg_double_bits(UINT64_C(4607745368753438720))));
  v13 = ((float)((-(t60))));
  }
  }
  t61 = ((uint32_t)(v8));
  t62 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(12))))));
  t63 = ((uint32_t)(((t61) >> ((uint64_t)(t62) & 31))));
  t64 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(7))))));
  v14 = ((int64_t)(((int64_t)(((t63) & (t64))))));
  t65 = ((uint64_t)(v0));
  t66 = ((uint64_t)(((uint64_t)(v6))));
#ifndef __cplusplus
  extern float (fg260d_ld_half)(uint64_t, uint64_t);
#endif
  t67 = fg260d_ld_half(t65, t66);
  t68 = ((float)(t67));
  t69 = ((int64_t)(((int64_t)UINT64_C(2))));
  t70 = ((int64_t)(v14));
  t71 = ((int64_t)(((int64_t)((uint64_t)(t69) * (uint64_t)(t70)))));
  t72 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t73 = ((float)(((float)(((int64_t)((uint64_t)(t71) + (uint64_t)(t72)))))));
  v15 = ((float)(((t68) * (t73))));
  t74 = ((uint32_t)(v11));
#ifndef __cplusplus
  extern uint32_t (fg260d_grid)(uint32_t);
#endif
  t75 = fg260d_grid(t74);
  v16 = ((uint32_t)(t75));
  t76 = ((uint32_t)(v16));
  t77 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(252645135))))));
  v17 = ((uint32_t)(((t76) & (t77))));
  t78 = ((uint32_t)(v16));
  t79 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4))))));
  t80 = ((uint32_t)(((t78) >> ((uint64_t)(t79) & 31))));
  t81 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(252645135))))));
  v18 = ((uint32_t)(((t80) & (t81))));
  v19 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t82 = ((int64_t)(v19));
  t83 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  if (!(((t82) < (t83)))) goto label2;
  {
  v20 = ((uint32_t)(((int64_t)UINT64_C(0))));
  t84 = ((int64_t)(v19));
  t85 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t86 = ((t84) < (t85));
  if (t86) {
  {
  t87 = ((int64_t)(v19));
  t88 = ((int64_t)(((int64_t)UINT64_C(8))));
  v21 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t87) * (uint64_t)(t88)))))));
  t89 = ((uint32_t)(v17));
  t90 = ((uint32_t)(v21));
  v20 = ((uint32_t)(((t89) >> ((uint64_t)(t90) & 31))));
  }
  } else {
  {
  t91 = ((int64_t)(v19));
  t92 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t93 = ((int64_t)(((int64_t)((uint64_t)(t91) - (uint64_t)(t92)))));
  t94 = ((int64_t)(((int64_t)UINT64_C(8))));
  v22 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t93) * (uint64_t)(t94)))))));
  t95 = ((uint32_t)(v18));
  t96 = ((uint32_t)(v22));
  v20 = ((uint32_t)(((t95) >> ((uint64_t)(t96) & 31))));
  }
  }
  t97 = ((uint32_t)(v20));
  t98 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(255))))));
  v23 = ((int64_t)(((int64_t)(((t97) & (t98))))));
  t99 = ((uint64_t)(v1));
  t100 = ((int64_t)(v7));
  t101 = ((int64_t)(v19));
  t102 = ((int64_t)(((int64_t)((uint64_t)(t100) + (uint64_t)(t101)))));
  t103 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t104 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t102) * (uint64_t)(t103)))))));
  t105 = ((float)(v15));
  t106 = ((float)(((float)(v23))));
  t107 = ((float)(v13));
  t108 = ((float)(((t106) + (t107))));
  t109 = ((float)(((t105) * (t108))));
#ifndef __cplusplus
  extern void (fg260d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg260d_st_f32(t99, t104, t109);
  t110 = ((int64_t)(v19));
  t111 = ((int64_t)(((int64_t)UINT64_C(1))));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t110) + (uint64_t)(t111)))));
  }
  goto label1;
label2:;
  }
fg260dd_gg_done:;
}
