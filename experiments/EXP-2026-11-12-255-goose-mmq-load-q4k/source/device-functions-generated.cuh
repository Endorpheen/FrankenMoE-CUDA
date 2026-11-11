#pragma once
static __device__ __forceinline__ void fg255qd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg255qd_gg_index(int64_t i, int64_t len);
static __device__ __forceinline__ int64_t fg255qd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg255qd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg255qd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg255qd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg255qd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ void fg255qd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ uint8_t fg255qd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t base);
static __device__ __forceinline__ void fg255qd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg255qd_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg255qd_gg_fail("array index out of bounds"); return i; }
static __device__ __forceinline__ int64_t fg255qd_gg_div(int64_t a, int64_t b) { if (!b) fg255qd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg255qd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg255qd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg255qd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg255qd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg255qd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg255qd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg255qd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg255qd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ void fg255qd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg255q_fail)(int64_t);
#endif
  fg255q_fail(t2);
  }
  }
  }
}
static __device__ __forceinline__ uint8_t fg255qd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t base) {
  _Alignas(8) uint8_t t0;
  _Alignas(8) uint64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) uint64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) uint8_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) uint8_t t14;
  _Alignas(8) uint8_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) uint8_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) uint64_t t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) int64_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) int64_t t32;
  _Alignas(8) int64_t t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) int64_t t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) int64_t t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) uint64_t t41;
  _Alignas(8) int64_t t42;
  _Alignas(8) int64_t t43;
  _Alignas(8) int64_t t44;
  _Alignas(8) int64_t t45;
  _Alignas(8) int64_t t46;
  _Alignas(8) int64_t t47;
  _Alignas(8) uint64_t t48;
  _Alignas(8) int64_t t49;
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
  _Alignas(8) uint64_t t61;
  _Alignas(8) uint64_t t62;
  _Alignas(8) uint64_t t63;
  _Alignas(8) int64_t t64;
  _Alignas(8) int64_t t65;
  _Alignas(8) int64_t t66;
  _Alignas(8) int64_t t67;
  _Alignas(8) uint64_t t68;
  _Alignas(8) uint64_t t69;
  _Alignas(8) uint64_t t70;
  _Alignas(8) int64_t t71;
  _Alignas(8) int64_t t72;
  _Alignas(8) int64_t t73;
  _Alignas(8) int64_t t74;
  _Alignas(8) uint64_t t75;
  _Alignas(8) int64_t t76;
  _Alignas(8) int64_t t77;
  _Alignas(8) int64_t t78;
  _Alignas(8) int64_t t79;
  _Alignas(8) uint8_t t80;
  _Alignas(8) int64_t t81;
  _Alignas(8) int64_t t82;
  _Alignas(8) int64_t t83;
  _Alignas(8) int64_t t84;
  _Alignas(8) int64_t t85;
  _Alignas(8) int64_t t86;
  _Alignas(8) int64_t t87;
  _Alignas(8) int64_t t88;
  _Alignas(8) int64_t t89;
  _Alignas(8) int64_t t90;
  _Alignas(8) int64_t t91;
  _Alignas(8) int64_t t92;
  _Alignas(8) uint64_t t93;
  _Alignas(8) int64_t t94;
  _Alignas(8) int64_t t95;
  _Alignas(8) int64_t t96;
  _Alignas(8) int64_t t97;
  _Alignas(8) int64_t t98;
  _Alignas(8) int64_t t99;
  _Alignas(8) int64_t t100;
  _Alignas(8) int64_t t101;
  _Alignas(8) int64_t t102;
  _Alignas(8) int64_t t103;
  _Alignas(8) int64_t t104;
  _Alignas(8) int64_t t105;
  _Alignas(8) int64_t t106;
  _Alignas(8) int64_t t107;
  _Alignas(8) int64_t t108;
  _Alignas(8) int64_t t109;
  _Alignas(8) int64_t t110;
  _Alignas(8) int64_t t111;
  _Alignas(8) uint64_t v0;
  _Alignas(8) uint64_t v1;
  _Alignas(8) uint64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) int64_t v8;
  _Alignas(8) uint64_t v9;
  _Alignas(8) int64_t v10;
  _Alignas(8) int64_t v11;
  _Alignas(8) int64_t v12;
  _Alignas(8) int64_t v13;
  _Alignas(8) int64_t v14;
  _Alignas(8) int64_t v15;
  _Alignas(8) int64_t v16;
  _Alignas(8) int64_t v17;
  _Alignas(8) int64_t v18;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  v8 = p8;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((int64_t)(v4));
  t3 = ((int64_t)(v6));
  t4 = ((int64_t)(v5));
  t5 = ((int64_t)(((int64_t)((uint64_t)(t3) * (uint64_t)(t4)))));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t2) + (uint64_t)(t5)))));
  t7 = ((int64_t)(((int64_t)UINT64_C(144))));
  t8 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t6) * (uint64_t)(t7)))))));
  v9 = ((uint64_t)(((uint64_t)((uint64_t)(t1) + (uint64_t)(t8)))));
  t9 = ((int64_t)(v3));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t11 = ((t9) == (t10));
  if (t11) {
  {
  t12 = ((int64_t)(v7));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t14 = ((t12) >= (t13));
  t15 = t14;
  if (t15) {
  t16 = ((int64_t)(v7));
  t17 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t15 = ((t16) < (t17));
  }
  t18 = t15;
  t19 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg255qd_gg_function_2(t18, t19, base + 0);
  t20 = ((uint64_t)(v9));
  t21 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t22 = ((int64_t)(v7));
  t23 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t24 = ((int64_t)(((int64_t)((uint64_t)(t22) * (uint64_t)(t23)))));
  t25 = ((int64_t)(((int64_t)((uint64_t)(t21) + (uint64_t)(t24)))));
#ifndef __cplusplus
  extern int64_t (fg255q_ld_i32)(uint64_t, int64_t);
#endif
  t26 = fg255q_ld_i32(t20, t25);
  v10 = ((int64_t)(t26));
  t27 = ((int64_t)(v6));
  t28 = ((int64_t)(v8));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) * (uint64_t)(t28)))));
  t30 = ((int64_t)(((int64_t)UINT64_C(16))));
  t31 = ((int64_t)(v7));
  t32 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t33 = ((int64_t)(((int64_t)fg255qd_gg_div(t31, t32))));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t30) * (uint64_t)(t33)))));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t34)))));
  t36 = ((int64_t)(v7));
  t37 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t38 = ((int64_t)(((int64_t)fg255qd_gg_mod(t36, t37))));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t35) + (uint64_t)(t38)))));
  t40 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t39) * (uint64_t)(t40)))));
  t41 = ((uint64_t)(v1));
  t42 = ((int64_t)(v11));
  t43 = ((int64_t)(v10));
  t44 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t45 = ((int64_t)(((t43) >> ((uint64_t)(t44) & 63))));
  t46 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(252645135))))));
  t47 = ((int64_t)(((t45) & (t46))));
#ifndef __cplusplus
  extern void (fg255q_st_i32)(uint64_t, int64_t, int64_t);
#endif
  fg255q_st_i32(t41, t42, t47);
  t48 = ((uint64_t)(v1));
  t49 = ((int64_t)(v11));
  t50 = ((int64_t)(((int64_t)UINT64_C(8))));
  t51 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t50) * (uint64_t)(t51)))));
  t53 = ((int64_t)(((int64_t)((uint64_t)(t49) + (uint64_t)(t52)))));
  t54 = ((int64_t)(v10));
  t55 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t56 = ((int64_t)(((t54) >> ((uint64_t)(t55) & 63))));
  t57 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(252645135))))));
  t58 = ((int64_t)(((t56) & (t57))));
#ifndef __cplusplus
  extern void (fg255q_st_i32)(uint64_t, int64_t, int64_t);
#endif
  fg255q_st_i32(t48, t53, t58);
  t0 = ((uint8_t)UINT64_C(1));
  goto fg255qd_gg_done;
  }
  }
  t59 = ((int64_t)(v7));
  t60 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  v12 = ((int64_t)(((int64_t)fg255qd_gg_mod(t59, t60))));
  t61 = ((uint64_t)(v9));
  t62 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t63 = ((uint64_t)(((uint64_t)((uint64_t)(t61) + (uint64_t)(t62)))));
  t64 = ((int64_t)(v12));
  t65 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t66 = ((int64_t)(((int64_t)((uint64_t)(t64) + (uint64_t)(t65)))));
#ifndef __cplusplus
  extern int64_t (fg255q_unpack_scales)(uint64_t, int64_t);
#endif
  t67 = fg255q_unpack_scales(t63, t66);
  v13 = ((int64_t)(t67));
  t68 = ((uint64_t)(v9));
  t69 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t70 = ((uint64_t)(((uint64_t)((uint64_t)(t68) + (uint64_t)(t69)))));
  t71 = ((int64_t)(v12));
  t72 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t73 = ((int64_t)(((int64_t)((uint64_t)(t71) + (uint64_t)(t72)))));
#ifndef __cplusplus
  extern int64_t (fg255q_unpack_scales)(uint64_t, int64_t);
#endif
  t74 = fg255q_unpack_scales(t70, t73);
  v14 = ((int64_t)(t74));
  t75 = ((uint64_t)(v9));
  t76 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
#ifndef __cplusplus
  extern int64_t (fg255q_ld_i32)(uint64_t, int64_t);
#endif
  t77 = fg255q_ld_i32(t75, t76);
  v15 = ((int64_t)(t77));
  v16 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label1:;
  t78 = ((int64_t)(v16));
  t79 = ((int64_t)(((int64_t)UINT64_C(4))));
  t80 = ((t78) < (t79));
  if (!(t80)) goto label2;
  {
  t81 = ((int64_t)(v13));
  t82 = ((int64_t)(v16));
  t83 = ((int64_t)(((int64_t)UINT64_C(8))));
  t84 = ((int64_t)(((int64_t)(((int64_t)((uint64_t)(t82) * (uint64_t)(t83)))))));
  t85 = ((int64_t)(((t81) >> ((uint64_t)(t84) & 63))));
  t86 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(255))))));
  v17 = ((int64_t)(((t85) & (t86))));
  t87 = ((int64_t)(v14));
  t88 = ((int64_t)(v16));
  t89 = ((int64_t)(((int64_t)UINT64_C(8))));
  t90 = ((int64_t)(((int64_t)(((int64_t)((uint64_t)(t88) * (uint64_t)(t89)))))));
  t91 = ((int64_t)(((t87) >> ((uint64_t)(t90) & 63))));
  t92 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(255))))));
  v18 = ((int64_t)(((t91) & (t92))));
  t93 = ((uint64_t)(v2));
  t94 = ((int64_t)(v6));
  t95 = ((int64_t)(v8));
  t96 = ((int64_t)(((int64_t)((uint64_t)(t94) * (uint64_t)(t95)))));
  t97 = ((int64_t)(((int64_t)UINT64_C(4))));
  t98 = ((int64_t)(((int64_t)(v12))));
  t99 = ((int64_t)(((int64_t)((uint64_t)(t97) * (uint64_t)(t98)))));
  t100 = ((int64_t)(((int64_t)((uint64_t)(t96) + (uint64_t)(t99)))));
  t101 = ((int64_t)(v16));
  t102 = ((int64_t)(((int64_t)((uint64_t)(t100) + (uint64_t)(t101)))));
  t103 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t104 = ((int64_t)(((int64_t)((uint64_t)(t102) * (uint64_t)(t103)))));
  t105 = ((int64_t)(v15));
  t106 = ((int64_t)(v17));
  t107 = ((int64_t)(v18));
#ifndef __cplusplus
  extern int64_t (fg255q_dm_scale)(int64_t, int64_t, int64_t);
#endif
  t108 = fg255q_dm_scale(t105, t106, t107);
  t109 = ((int64_t)(t108));
#ifndef __cplusplus
  extern void (fg255q_st_i32)(uint64_t, int64_t, int64_t);
#endif
  fg255q_st_i32(t93, t104, t109);
  t110 = ((int64_t)(v16));
  t111 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t110) + (uint64_t)(t111)))));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  }
fg255qd_gg_done:;
  return t0;
}
