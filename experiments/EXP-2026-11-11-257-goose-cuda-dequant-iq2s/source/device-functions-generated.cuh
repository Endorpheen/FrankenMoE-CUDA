#pragma once
static __device__ __forceinline__ void fg257dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg257dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg257dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg257dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg257dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg257dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ float fg257dd_gg_function_7(int64_t p0, int64_t base);
static __device__ __forceinline__ void fg257dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg257dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base);
static __device__ __forceinline__ void fg257dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg257dd_gg_div(int64_t a, int64_t b) { if (!b) fg257dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg257dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg257dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg257dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg257dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg257dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg257dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg257dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg257dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ float fg257dd_gg_function_7(int64_t p0, int64_t base) {
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
  t4 = ((float)(fg257dd_gg_double_bits(UINT64_C(4607182418800017408))));
  t0 = ((float)((-(t4))));
  goto fg257dd_gg_done;
  }
  }
  t0 = ((float)(((float)(fg257dd_gg_double_bits(UINT64_C(4607182418800017408))))));
  }
fg257dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg257dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg257d_fail)(int64_t);
#endif
  fg257d_fail(t2);
  }
  }
  }
fg257dd_gg_done:;
}
static __device__ __forceinline__ void fg257dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  uint64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  uint64_t t33;
  uint32_t t34;
  uint64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  uint64_t t40;
  uint32_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  uint32_t t46;
  uint32_t t47;
  uint32_t t48;
  uint32_t t49;
  uint32_t t50;
  uint32_t t51;
  uint64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  uint64_t t57;
  uint32_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  uint64_t t63;
  uint64_t t64;
  float t65;
  float t66;
  float t67;
  uint32_t t68;
  uint32_t t69;
  uint32_t t70;
  uint32_t t71;
  float t72;
  float t73;
  float t74;
  float t75;
  uint64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  uint64_t t85;
  uint32_t t86;
  uint32_t t87;
  uint64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  uint64_t t93;
  uint64_t t94;
  uint64_t t95;
  uint64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  uint64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  uint64_t t106;
  float t107;
  float t108;
  float t109;
  int64_t t110;
  float t111;
  float t112;
  float t113;
  int64_t t114;
  int64_t t115;
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
  uint32_t v11;
  uint32_t v12;
  uint32_t v13;
  uint32_t v14;
  uint32_t v15;
  float v16;
  uint32_t v17;
  uint64_t v18;
  int64_t v19;
  uint64_t v20;
  int64_t v21;
  int64_t v22;
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
  fg257dd_gg_function_2(t6, t7, base + 0);
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)UINT64_C(8))));
  v4 = ((int64_t)(((int64_t)fg257dd_gg_div(t8, t9))));
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(((int64_t)UINT64_C(8))));
  v5 = ((int64_t)(((int64_t)fg257dd_gg_mod(t10, t11))));
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(82))))));
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
  v8 = ((int64_t)(((int64_t)((uint64_t)(t24) + (uint64_t)(t25)))));
  t26 = ((uint64_t)(v0));
  t27 = ((int64_t)(v8));
  t28 = ((int64_t)(((int64_t)UINT64_C(4))));
  t29 = ((int64_t)(v5));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t27) + (uint64_t)(t30)))));
  t32 = ((int64_t)(v4));
  t33 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t32)))))));
#ifndef __cplusplus
  extern uint32_t (fg257d_ld_u8)(uint64_t, uint64_t);
#endif
  t34 = fg257d_ld_u8(t26, t33);
  v9 = ((uint32_t)(t34));
  t35 = ((uint64_t)(v0));
  t36 = ((int64_t)(v6));
  t37 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(66))))));
  t38 = ((int64_t)(((int64_t)((uint64_t)(t36) + (uint64_t)(t37)))));
  t39 = ((int64_t)(v5));
  t40 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t38) + (uint64_t)(t39)))))));
#ifndef __cplusplus
  extern uint32_t (fg257d_ld_u8)(uint64_t, uint64_t);
#endif
  t41 = fg257d_ld_u8(t35, t40);
  v10 = ((uint32_t)(t41));
  t42 = ((int64_t)(((int64_t)UINT64_C(8))));
  t43 = ((int64_t)(((int64_t)UINT64_C(2))));
  t44 = ((int64_t)(v4));
  t45 = ((int64_t)(((int64_t)((uint64_t)(t43) * (uint64_t)(t44)))));
  v11 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t42) - (uint64_t)(t45)))))));
  t46 = ((uint32_t)(v10));
  t47 = ((uint32_t)(v11));
  t48 = ((uint32_t)(((uint32_t)((uint64_t)(t46) << ((uint64_t)(t47) & 31)))));
  t49 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(768))))));
  v12 = ((uint32_t)(((t48) & (t49))));
  t50 = ((uint32_t)(v9));
  t51 = ((uint32_t)(v12));
  v13 = ((uint32_t)(((t50) | (t51))));
  t52 = ((uint64_t)(v0));
  t53 = ((int64_t)(v6));
  t54 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(74))))));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t53) + (uint64_t)(t54)))));
  t56 = ((int64_t)(v5));
  t57 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t55) + (uint64_t)(t56)))))));
#ifndef __cplusplus
  extern uint32_t (fg257d_ld_u8)(uint64_t, uint64_t);
#endif
  t58 = fg257d_ld_u8(t52, t57);
  v14 = ((uint32_t)(t58));
  t59 = ((int64_t)(((int64_t)UINT64_C(4))));
  t60 = ((int64_t)(v4));
  t61 = ((int64_t)(((int64_t)UINT64_C(2))));
  t62 = ((int64_t)(((int64_t)fg257dd_gg_div(t60, t61))));
  v15 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t59) * (uint64_t)(t62)))))));
  t63 = ((uint64_t)(v0));
  t64 = ((uint64_t)(((uint64_t)(v6))));
#ifndef __cplusplus
  extern float (fg257d_ld_half)(uint64_t, uint64_t);
#endif
  t65 = fg257d_ld_half(t63, t64);
  t66 = ((float)(t65));
  t67 = ((float)(((float)(fg257dd_gg_double_bits(UINT64_C(4602678819172646912))))));
  t68 = ((uint32_t)(v14));
  t69 = ((uint32_t)(v15));
  t70 = ((uint32_t)(((t68) >> ((uint64_t)(t69) & 31))));
  t71 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(15))))));
  t72 = ((float)(((float)(((t70) & (t71))))));
  t73 = ((float)(((t67) + (t72))));
  t74 = ((float)(((t66) * (t73))));
  t75 = ((float)(((float)(fg257dd_gg_double_bits(UINT64_C(4598175219545276416))))));
  v16 = ((float)(((t74) * (t75))));
  t76 = ((uint64_t)(v0));
  t77 = ((int64_t)(v8));
  t78 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t79 = ((int64_t)(((int64_t)((uint64_t)(t77) + (uint64_t)(t78)))));
  t80 = ((int64_t)(((int64_t)UINT64_C(4))));
  t81 = ((int64_t)(v5));
  t82 = ((int64_t)(((int64_t)((uint64_t)(t80) * (uint64_t)(t81)))));
  t83 = ((int64_t)(((int64_t)((uint64_t)(t79) + (uint64_t)(t82)))));
  t84 = ((int64_t)(v4));
  t85 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t83) + (uint64_t)(t84)))))));
#ifndef __cplusplus
  extern uint32_t (fg257d_ld_u8)(uint64_t, uint64_t);
#endif
  t86 = fg257d_ld_u8(t76, t85);
  v17 = ((uint32_t)(t86));
  t87 = ((uint32_t)(v13));
#ifndef __cplusplus
  extern uint64_t (fg257d_grid)(uint32_t);
#endif
  t88 = fg257d_grid(t87);
  v18 = ((uint64_t)(t88));
  v19 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t89 = ((int64_t)(v19));
  t90 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  if (!(((t89) < (t90)))) goto label2;
  {
  t91 = ((int64_t)(v19));
  t92 = ((int64_t)(((int64_t)UINT64_C(8))));
  v20 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t91) * (uint64_t)(t92)))))));
  t93 = ((uint64_t)(v18));
  t94 = ((uint64_t)(v20));
  t95 = ((uint64_t)(((t93) >> ((uint64_t)(t94) & 63))));
  t96 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(255))))));
  v21 = ((int64_t)(((int64_t)(((t95) & (t96))))));
  t97 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t98 = ((int64_t)(v19));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t97) << ((uint64_t)(t98) & 63)))));
  t99 = ((int64_t)(((int64_t)(v17))));
  t100 = ((int64_t)(v22));
  v23 = ((int64_t)(((t99) & (t100))));
  t101 = ((uint64_t)(v1));
  t102 = ((int64_t)(v7));
  t103 = ((int64_t)(v19));
  t104 = ((int64_t)(((int64_t)((uint64_t)(t102) + (uint64_t)(t103)))));
  t105 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t106 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t104) * (uint64_t)(t105)))))));
  t107 = ((float)(v16));
  t108 = ((float)(((float)(v21))));
  t109 = ((float)(((t107) * (t108))));
  t110 = ((int64_t)(v23));
  t111 = fg257dd_gg_function_7(t110, base + 0);
  t112 = ((float)(t111));
  t113 = ((float)(((t109) * (t112))));
#ifndef __cplusplus
  extern void (fg257d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg257d_st_f32(t101, t106, t113);
  t114 = ((int64_t)(v19));
  t115 = ((int64_t)(((int64_t)UINT64_C(1))));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t114) + (uint64_t)(t115)))));
  }
  goto label1;
label2:;
  }
fg257dd_gg_done:;
}
