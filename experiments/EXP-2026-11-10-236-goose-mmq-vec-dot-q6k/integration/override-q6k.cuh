#pragma once
// Force-included override for the Q6_K inner product (EXP236).
//
// Why this shape: `mmq-vec-dot.cuh` does `#include "vecdotq.cuh"`, and a quoted include resolves in the
// includer's own directory first, so an -I copy is silently ignored -- two attempts produced byte-identical
// cubins before I checked the hashes. The working approach is to rename the native symbol with -D and
// define the real name here, which -include puts in front of everything.
#ifndef GGML_GOOSE_Q6K_OVERRIDE
#define GGML_GOOSE_Q6K_OVERRIDE
#include <stdint.h>

static __device__ __forceinline__ void    fg236k_fail(int64_t){ asm("trap;"); }
static __device__ __forceinline__ int64_t fg236k_ld_i32(uint64_t b,int64_t i){ return (int64_t)((const int32_t*)(uintptr_t)b)[i]; }
static __device__ __forceinline__ float   fg236k_ld_f32(uint64_t b,int64_t i){ return ((const float*)(uintptr_t)b)[i]; }
static __device__ __forceinline__ float   fg236k_to_f32(int64_t v){ return (float)v; }
static __device__ __forceinline__ int64_t fg236k_dp4a(int64_t a,int64_t b,int64_t c){ return (int64_t)__dp4a((int)a,(int)b,(int)c); }
static __device__ __forceinline__ int64_t gg_div(int64_t a,int64_t b){ if(!b) fg236k_fail(1); return a/b; }
static __device__ __forceinline__ int64_t gg_mod(int64_t a,int64_t b){ if(!b) fg236k_fail(1); return a%b; }

static __device__ __forceinline__ double gg_double_bits(uint64_t bits);
static __device__ __forceinline__ int64_t gg_function_6(int64_t p0, int64_t base);
static __device__ __forceinline__ void gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ float gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, float p3, uint64_t p4, int64_t p5, int64_t base);
static __device__ __forceinline__ double gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ int64_t gg_function_6(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(128))))));
  t3 = ((t1) >= (t2));
  if (t3) {
  {
  t4 = ((int64_t)(v0));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(256))))));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t4) - (uint64_t)(t5)))));
  goto gg_done;
  }
  }
  t0 = ((int64_t)(v0));
  }
gg_done:;
  return t0;
}
static __device__ __forceinline__ void gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg236k_fail)(int64_t);
// The replacement itself. -Dvec_dot_q6_K_q8_1_impl_mmq=..._native renames the native definition and every
// call site; this defines the original name so the call sites bind here instead.
static __device__ __forceinline__ float vec_dot_q6_K_q8_1_impl_mmq(
    const int * __restrict__ v, const int * __restrict__ u, const int8_t * __restrict__ sc,
    const float & d6, const float * __restrict__ d8) {
    return (float) gg_function_1((uint64_t)(uintptr_t) v, (uint64_t)(uintptr_t) u,
                                 (uint64_t)(uintptr_t) sc, d6, (uint64_t)(uintptr_t) d8, 2, 0);
}

#endif
  fg236k_fail(t2);
  }
  }
  }
gg_done:;
}
static __device__ __forceinline__ float gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, float p3, uint64_t p4, int64_t p5, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  uint64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint64_t t16;
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
  int64_t t31;
  int64_t t32;
  int64_t t33;
  uint64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  uint64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  uint64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  uint64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  uint64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  uint64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
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
  int64_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  float t118;
  uint64_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  float t123;
  float t124;
  int64_t t125;
  float t126;
  float t127;
  float t128;
  int64_t t129;
  int64_t t130;
  float t131;
  float t132;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  float v3;
  uint64_t v4;
  int64_t v5;
  int64_t v6;
  float v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t1 = ((int64_t)(v5));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(1600))));
  gg_function_2(t3, t4, base + 0);
  t5 = ((uint64_t)(v2));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
#ifndef __cplusplus
  extern int64_t (fg236k_ld_i32)(uint64_t, int64_t);
#endif
  t7 = fg236k_ld_i32(t5, t6);
  v6 = ((int64_t)(t7));
  v7 = ((float)(gg_double_bits(UINT64_C(0))));
  v8 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t8 = ((int64_t)(v8));
  t9 = ((int64_t)(v5));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t11 = ((int64_t)(((int64_t)((uint64_t)(t9) * (uint64_t)(t10)))));
  if (!(((t8) < (t11)))) goto label2;
  {
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
  v10 = ((int64_t)(((int64_t)UINT64_C(0))));
  v11 = ((int64_t)(v8));
label4:;
  t12 = ((int64_t)(v11));
  t13 = ((int64_t)(v8));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t14)))));
  if (!(((t12) < (t15)))) goto label5;
  {
  t16 = ((uint64_t)(v0));
  t17 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t18 = ((int64_t)(v11));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) * (uint64_t)(t18)))));
  t20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) + (uint64_t)(t20)))));
#ifndef __cplusplus
  extern int64_t (fg236k_ld_i32)(uint64_t, int64_t);
#endif
  t22 = fg236k_ld_i32(t16, t21);
  t23 = ((int64_t)(t22));
  t24 = ((uint64_t)(v1));
  t25 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t26 = ((int64_t)(v11));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t25) * (uint64_t)(t26)))));
  t28 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) + (uint64_t)(t28)))));
#ifndef __cplusplus
  extern int64_t (fg236k_ld_i32)(uint64_t, int64_t);
#endif
  t30 = fg236k_ld_i32(t24, t29);
  t31 = ((int64_t)(t30));
  t32 = ((int64_t)(v9));
#ifndef __cplusplus
  extern int64_t (fg236k_dp4a)(int64_t, int64_t, int64_t);
#endif
  t33 = fg236k_dp4a(t23, t31, t32);
  v9 = ((int64_t)(t33));
  t34 = ((uint64_t)(v0));
  t35 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t36 = ((int64_t)(v11));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) * (uint64_t)(t36)))));
  t38 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t37) + (uint64_t)(t38)))));
#ifndef __cplusplus
  extern int64_t (fg236k_ld_i32)(uint64_t, int64_t);
#endif
  t40 = fg236k_ld_i32(t34, t39);
  t41 = ((int64_t)(t40));
  t42 = ((uint64_t)(v1));
  t43 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t44 = ((int64_t)(v11));
  t45 = ((int64_t)(((int64_t)((uint64_t)(t43) * (uint64_t)(t44)))));
  t46 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t47 = ((int64_t)(((int64_t)((uint64_t)(t45) + (uint64_t)(t46)))));
#ifndef __cplusplus
  extern int64_t (fg236k_ld_i32)(uint64_t, int64_t);
#endif
  t48 = fg236k_ld_i32(t42, t47);
  t49 = ((int64_t)(t48));
  t50 = ((int64_t)(v9));
#ifndef __cplusplus
  extern int64_t (fg236k_dp4a)(int64_t, int64_t, int64_t);
#endif
  t51 = fg236k_dp4a(t41, t49, t50);
  v9 = ((int64_t)(t51));
  t52 = ((uint64_t)(v0));
  t53 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t54 = ((int64_t)(v11));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t53) * (uint64_t)(t54)))));
  t56 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t57 = ((int64_t)(((int64_t)((uint64_t)(t55) + (uint64_t)(t56)))));
#ifndef __cplusplus
  extern int64_t (fg236k_ld_i32)(uint64_t, int64_t);
#endif
  t58 = fg236k_ld_i32(t52, t57);
  t59 = ((int64_t)(t58));
  t60 = ((uint64_t)(v1));
  t61 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t62 = ((int64_t)(v11));
  t63 = ((int64_t)(((int64_t)((uint64_t)(t61) * (uint64_t)(t62)))));
  t64 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t65 = ((int64_t)(((int64_t)((uint64_t)(t63) + (uint64_t)(t64)))));
#ifndef __cplusplus
  extern int64_t (fg236k_ld_i32)(uint64_t, int64_t);
#endif
  t66 = fg236k_ld_i32(t60, t65);
  t67 = ((int64_t)(t66));
  t68 = ((int64_t)(v10));
#ifndef __cplusplus
  extern int64_t (fg236k_dp4a)(int64_t, int64_t, int64_t);
#endif
  t69 = fg236k_dp4a(t59, t67, t68);
  v10 = ((int64_t)(t69));
  t70 = ((uint64_t)(v0));
  t71 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t72 = ((int64_t)(v11));
  t73 = ((int64_t)(((int64_t)((uint64_t)(t71) * (uint64_t)(t72)))));
  t74 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t75 = ((int64_t)(((int64_t)((uint64_t)(t73) + (uint64_t)(t74)))));
#ifndef __cplusplus
  extern int64_t (fg236k_ld_i32)(uint64_t, int64_t);
#endif
  t76 = fg236k_ld_i32(t70, t75);
  t77 = ((int64_t)(t76));
  t78 = ((uint64_t)(v1));
  t79 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t80 = ((int64_t)(v11));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t79) * (uint64_t)(t80)))));
  t82 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t83 = ((int64_t)(((int64_t)((uint64_t)(t81) + (uint64_t)(t82)))));
#ifndef __cplusplus
  extern int64_t (fg236k_ld_i32)(uint64_t, int64_t);
#endif
  t84 = fg236k_ld_i32(t78, t83);
  t85 = ((int64_t)(t84));
  t86 = ((int64_t)(v10));
#ifndef __cplusplus
  extern int64_t (fg236k_dp4a)(int64_t, int64_t, int64_t);
#endif
  t87 = fg236k_dp4a(t77, t85, t86);
  v10 = ((int64_t)(t87));
  t88 = ((int64_t)(v11));
  t89 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t88) + (uint64_t)(t89)))));
  }
  goto label4;
label5:;
  t90 = ((int64_t)(v6));
  t91 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t92 = ((int64_t)(v8));
  t93 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t94 = ((int64_t)(((int64_t)gg_div(t92, t93))));
  t95 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t96 = ((int64_t)(((int64_t)((uint64_t)(t94) + (uint64_t)(t95)))));
  t97 = ((int64_t)(((int64_t)((uint64_t)(t91) * (uint64_t)(t96)))));
  t98 = ((int64_t)(((t90) >> ((uint64_t)(t97) & 63))));
  t99 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(255))))));
  v12 = ((int64_t)(((t98) & (t99))));
  t100 = ((int64_t)(v6));
  t101 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t102 = ((int64_t)(v8));
  t103 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t104 = ((int64_t)(((int64_t)gg_div(t102, t103))));
  t105 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t106 = ((int64_t)(((int64_t)((uint64_t)(t104) + (uint64_t)(t105)))));
  t107 = ((int64_t)(((int64_t)((uint64_t)(t101) * (uint64_t)(t106)))));
  t108 = ((int64_t)(((t100) >> ((uint64_t)(t107) & 63))));
  t109 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(255))))));
  v13 = ((int64_t)(((t108) & (t109))));
  t110 = ((int64_t)(v12));
  v14 = gg_function_6(t110, base + 0);
  t111 = ((int64_t)(v13));
  v15 = gg_function_6(t111, base + 0);
  t112 = ((int64_t)(v14));
  t113 = ((int64_t)(v9));
  t114 = ((int64_t)(((int64_t)((uint64_t)(t112) * (uint64_t)(t113)))));
  t115 = ((int64_t)(v15));
  t116 = ((int64_t)(v10));
  t117 = ((int64_t)(((int64_t)((uint64_t)(t115) * (uint64_t)(t116)))));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t114) + (uint64_t)(t117)))));
  t118 = ((float)(v7));
  t119 = ((uint64_t)(v4));
  t120 = ((int64_t)(v8));
  t121 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t122 = ((int64_t)(((int64_t)gg_div(t120, t121))));
#ifndef __cplusplus
  extern float (fg236k_ld_f32)(uint64_t, int64_t);
#endif
  t123 = fg236k_ld_f32(t119, t122);
  t124 = ((float)(t123));
  t125 = ((int64_t)(v16));
#ifndef __cplusplus
  extern float (fg236k_to_f32)(int64_t);
#endif
  t126 = fg236k_to_f32(t125);
  t127 = ((float)(t126));
  t128 = ((float)(((t124) * (t127))));
  v7 = ((float)(((t118) + (t128))));
  t129 = ((int64_t)(v8));
  t130 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t129) + (uint64_t)(t130)))));
  }
  goto label1;
label2:;
  t131 = ((float)(v3));
  t132 = ((float)(v7));
  t0 = ((float)(((t131) * (t132))));
  }
gg_done:;
  return t0;
}

#endif
