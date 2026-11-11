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
  _Alignas(8) uint8_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) uint8_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) uint64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) int64_t t20;
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
  _Alignas(8) uint64_t t36;
  _Alignas(8) int64_t t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) int64_t t41;
  _Alignas(8) int64_t t42;
  _Alignas(8) uint64_t t43;
  _Alignas(8) int64_t t44;
  _Alignas(8) int64_t t45;
  _Alignas(8) int64_t t46;
  _Alignas(8) int64_t t47;
  _Alignas(8) int64_t t48;
  _Alignas(8) int64_t t49;
  _Alignas(8) int64_t t50;
  _Alignas(8) int64_t t51;
  _Alignas(8) uint64_t t52;
  _Alignas(8) uint64_t t53;
  _Alignas(8) uint64_t t54;
  _Alignas(8) int64_t t55;
  _Alignas(8) int64_t t56;
  _Alignas(8) int64_t t57;
  _Alignas(8) int64_t t58;
  _Alignas(8) uint64_t t59;
  _Alignas(8) uint64_t t60;
  _Alignas(8) uint64_t t61;
  _Alignas(8) int64_t t62;
  _Alignas(8) int64_t t63;
  _Alignas(8) int64_t t64;
  _Alignas(8) int64_t t65;
  _Alignas(8) uint64_t t66;
  _Alignas(8) int64_t t67;
  _Alignas(8) int64_t t68;
  _Alignas(8) int64_t t69;
  _Alignas(8) int64_t t70;
  _Alignas(8) uint8_t t71;
  _Alignas(8) int64_t t72;
  _Alignas(8) int64_t t73;
  _Alignas(8) int64_t t74;
  _Alignas(8) int64_t t75;
  _Alignas(8) int64_t t76;
  _Alignas(8) int64_t t77;
  _Alignas(8) int64_t t78;
  _Alignas(8) int64_t t79;
  _Alignas(8) int64_t t80;
  _Alignas(8) int64_t t81;
  _Alignas(8) int64_t t82;
  _Alignas(8) int64_t t83;
  _Alignas(8) uint64_t t84;
  _Alignas(8) int64_t t85;
  _Alignas(8) int64_t t86;
  _Alignas(8) int64_t t87;
  _Alignas(8) int64_t t88;
  _Alignas(8) int64_t t89;
  _Alignas(8) int64_t t90;
  _Alignas(8) int64_t t91;
  _Alignas(8) int64_t t92;
  _Alignas(8) int64_t t93;
  _Alignas(8) int64_t t94;
  _Alignas(8) int64_t t95;
  _Alignas(8) int64_t t96;
  _Alignas(8) int64_t t97;
  _Alignas(8) int64_t t98;
  _Alignas(8) int64_t t99;
  _Alignas(8) int64_t t100;
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
  t2 = ((int64_t)(v3));
  t3 = ((int64_t)(v5));
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(((int64_t)((uint64_t)(t3) * (uint64_t)(t4)))));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t2) + (uint64_t)(t5)))));
  t7 = ((int64_t)(((int64_t)UINT64_C(144))));
  t8 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t6) * (uint64_t)(t7)))))));
  v9 = ((uint64_t)(((uint64_t)((uint64_t)(t1) + (uint64_t)(t8)))));
  t9 = ((int64_t)(v6));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t11 = ((t9) >= (t10));
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(v6));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t12 = ((t13) < (t14));
  }
  t15 = t12;
  t16 = ((int64_t)(((int64_t)UINT64_C(1))));
  fg255qd_gg_function_2(t15, t16, base + 0);
  t17 = ((uint64_t)(v9));
  t18 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t19 = ((int64_t)(v6));
  t20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t18) + (uint64_t)(t21)))));
#ifndef __cplusplus
  extern int64_t (fg255q_ld_i32)(uint64_t, int64_t);
#endif
  t23 = fg255q_ld_i32(t17, t22);
  v10 = ((int64_t)(t23));
  t24 = ((int64_t)(v5));
  t25 = ((int64_t)(v7));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t25)))));
  t27 = ((int64_t)(((int64_t)UINT64_C(16))));
  t28 = ((int64_t)(v6));
  t29 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t30 = ((int64_t)(((int64_t)fg255qd_gg_div(t28, t29))));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t27) * (uint64_t)(t30)))));
  t32 = ((int64_t)(((int64_t)((uint64_t)(t26) + (uint64_t)(t31)))));
  t33 = ((int64_t)(v6));
  t34 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t35 = ((int64_t)(((int64_t)fg255qd_gg_mod(t33, t34))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t32) + (uint64_t)(t35)))));
  t36 = ((uint64_t)(v1));
  t37 = ((int64_t)(v11));
  t38 = ((int64_t)(v10));
  t39 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t40 = ((int64_t)(((t38) >> ((uint64_t)(t39) & 63))));
  t41 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(252645135))))));
  t42 = ((int64_t)(((t40) & (t41))));
#ifndef __cplusplus
  extern void (fg255q_st_i32)(uint64_t, int64_t, int64_t);
#endif
  fg255q_st_i32(t36, t37, t42);
  t43 = ((uint64_t)(v1));
  t44 = ((int64_t)(v11));
  t45 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) + (uint64_t)(t45)))));
  t47 = ((int64_t)(v10));
  t48 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t49 = ((int64_t)(((t47) >> ((uint64_t)(t48) & 63))));
  t50 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(252645135))))));
  t51 = ((int64_t)(((t49) & (t50))));
#ifndef __cplusplus
  extern void (fg255q_st_i32)(uint64_t, int64_t, int64_t);
#endif
  fg255q_st_i32(t43, t46, t51);
  t52 = ((uint64_t)(v9));
  t53 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t54 = ((uint64_t)(((uint64_t)((uint64_t)(t52) + (uint64_t)(t53)))));
  t55 = ((int64_t)(v8));
  t56 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t57 = ((int64_t)(((int64_t)((uint64_t)(t55) + (uint64_t)(t56)))));
#ifndef __cplusplus
  extern int64_t (fg255q_unpack_scales)(uint64_t, int64_t);
#endif
  t58 = fg255q_unpack_scales(t54, t57);
  v12 = ((int64_t)(t58));
  t59 = ((uint64_t)(v9));
  t60 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t61 = ((uint64_t)(((uint64_t)((uint64_t)(t59) + (uint64_t)(t60)))));
  t62 = ((int64_t)(v8));
  t63 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t64 = ((int64_t)(((int64_t)((uint64_t)(t62) + (uint64_t)(t63)))));
#ifndef __cplusplus
  extern int64_t (fg255q_unpack_scales)(uint64_t, int64_t);
#endif
  t65 = fg255q_unpack_scales(t61, t64);
  v13 = ((int64_t)(t65));
  t66 = ((uint64_t)(v9));
  t67 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
#ifndef __cplusplus
  extern int64_t (fg255q_ld_i32)(uint64_t, int64_t);
#endif
  t68 = fg255q_ld_i32(t66, t67);
  v14 = ((int64_t)(t68));
  v15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label1:;
  t69 = ((int64_t)(v15));
  t70 = ((int64_t)(((int64_t)UINT64_C(4))));
  t71 = ((t69) < (t70));
  if (!(t71)) goto label2;
  {
  t72 = ((int64_t)(v12));
  t73 = ((int64_t)(v15));
  t74 = ((int64_t)(((int64_t)UINT64_C(8))));
  t75 = ((int64_t)(((int64_t)(((int64_t)((uint64_t)(t73) * (uint64_t)(t74)))))));
  t76 = ((int64_t)(((t72) >> ((uint64_t)(t75) & 63))));
  t77 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(255))))));
  v16 = ((int64_t)(((t76) & (t77))));
  t78 = ((int64_t)(v13));
  t79 = ((int64_t)(v15));
  t80 = ((int64_t)(((int64_t)UINT64_C(8))));
  t81 = ((int64_t)(((int64_t)(((int64_t)((uint64_t)(t79) * (uint64_t)(t80)))))));
  t82 = ((int64_t)(((t78) >> ((uint64_t)(t81) & 63))));
  t83 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(255))))));
  v17 = ((int64_t)(((t82) & (t83))));
  t84 = ((uint64_t)(v2));
  t85 = ((int64_t)(v5));
  t86 = ((int64_t)(v7));
  t87 = ((int64_t)(((int64_t)((uint64_t)(t85) * (uint64_t)(t86)))));
  t88 = ((int64_t)(((int64_t)UINT64_C(4))));
  t89 = ((int64_t)(((int64_t)(v8))));
  t90 = ((int64_t)(((int64_t)((uint64_t)(t88) * (uint64_t)(t89)))));
  t91 = ((int64_t)(((int64_t)((uint64_t)(t87) + (uint64_t)(t90)))));
  t92 = ((int64_t)(v15));
  t93 = ((int64_t)(((int64_t)((uint64_t)(t91) + (uint64_t)(t92)))));
  t94 = ((int64_t)(v14));
  t95 = ((int64_t)(v16));
  t96 = ((int64_t)(v17));
#ifndef __cplusplus
  extern int64_t (fg255q_dm_scale)(int64_t, int64_t, int64_t);
#endif
  t97 = fg255q_dm_scale(t94, t95, t96);
  t98 = ((int64_t)(t97));
#ifndef __cplusplus
  extern void (fg255q_st_i32)(uint64_t, int64_t, int64_t);
#endif
  fg255q_st_i32(t84, t93, t98);
  t99 = ((int64_t)(v15));
  t100 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t99) + (uint64_t)(t100)))));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  }
  return t0;
}
