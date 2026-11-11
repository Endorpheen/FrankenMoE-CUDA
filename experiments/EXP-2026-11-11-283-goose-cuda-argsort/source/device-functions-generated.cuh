#pragma once
static __device__ __forceinline__ void fg283dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg283dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg283dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg283dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg283dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg283dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ void fg283dd_gg_function_7(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base);
static __device__ __forceinline__ void fg283dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg283dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t base);
static __device__ __forceinline__ void fg283dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg283dd_gg_div(int64_t a, int64_t b) { if (!b) fg283dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg283dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg283dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg283dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg283dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg283dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg283dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg283dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg283dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ void fg283dd_gg_function_7(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  uint8_t t4;
  uint64_t t5;
  int64_t t6;
  int64_t t7;
  uint64_t t8;
  uint64_t t9;
  int32_t t10;
  uint64_t t11;
  int64_t t12;
  int64_t t13;
  uint64_t t14;
  uint64_t t15;
  int32_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  int64_t t22;
  int64_t t23;
  uint8_t t24;
  int64_t t25;
  int64_t t26;
  uint8_t t27;
  int64_t t28;
  int64_t t29;
  uint8_t t30;
  uint64_t t31;
  int64_t t32;
  int64_t t33;
  uint64_t t34;
  float t35;
  float t36;
  uint64_t t37;
  int64_t t38;
  int64_t t39;
  uint64_t t40;
  float t41;
  float t42;
  uint8_t t43;
  uint64_t t44;
  int64_t t45;
  int64_t t46;
  uint64_t t47;
  float t48;
  float t49;
  uint64_t t50;
  int64_t t51;
  int64_t t52;
  uint64_t t53;
  float t54;
  float t55;
  uint8_t t56;
  int64_t t57;
  int64_t t58;
  uint8_t t59;
  int64_t t60;
  int64_t t61;
  uint8_t t62;
  int64_t t63;
  int64_t t64;
  uint8_t t65;
  uint64_t t66;
  int64_t t67;
  int64_t t68;
  uint64_t t69;
  float t70;
  float t71;
  uint64_t t72;
  int64_t t73;
  int64_t t74;
  uint64_t t75;
  float t76;
  float t77;
  uint8_t t78;
  uint64_t t79;
  int64_t t80;
  int64_t t81;
  uint64_t t82;
  float t83;
  float t84;
  uint64_t t85;
  int64_t t86;
  int64_t t87;
  uint64_t t88;
  float t89;
  float t90;
  uint8_t t91;
  int64_t t92;
  int64_t t93;
  uint8_t t94;
  uint64_t t95;
  int64_t t96;
  int64_t t97;
  uint64_t t98;
  uint64_t t99;
  int32_t t100;
  uint64_t t101;
  int64_t t102;
  int64_t t103;
  uint64_t t104;
  uint64_t t105;
  int32_t t106;
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  t0 = ((int64_t)(v3));
  t1 = ((int64_t)(v5));
  v7 = ((int64_t)(((t0) ^ (t1))));
  t2 = ((int64_t)(v7));
  t3 = ((int64_t)(((int64_t)(v3))));
  t4 = ((t2) <= (t3));
  if (t4) {
  {
  goto fg283dd_gg_done;
  }
  }
  t5 = ((uint64_t)(v1));
  t6 = ((int64_t)(v3));
  t7 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t8 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t6) * (uint64_t)(t7)))))));
  t9 = ((uint64_t)(((uint64_t)((uint64_t)(t5) + (uint64_t)(t8)))));
#ifndef __cplusplus
  extern int32_t (fg283d_sload_i32)(uint64_t);
#endif
  t10 = fg283d_sload_i32(t9);
  v8 = ((int64_t)(((int64_t)(t10))));
  t11 = ((uint64_t)(v1));
  t12 = ((int64_t)(v7));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t14 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))))));
  t15 = ((uint64_t)(((uint64_t)((uint64_t)(t11) + (uint64_t)(t14)))));
#ifndef __cplusplus
  extern int32_t (fg283d_sload_i32)(uint64_t);
#endif
  t16 = fg283d_sload_i32(t15);
  v9 = ((int64_t)(((int64_t)(t16))));
  v10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t17 = ((int64_t)(v3));
  t18 = ((int64_t)(v4));
  t19 = ((int64_t)(((t17) & (t18))));
  t20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t21 = ((t19) == (t20));
  if (t21) {
  {
  t22 = ((int64_t)(v8));
  t23 = ((int64_t)(((int64_t)(v2))));
  t24 = ((t22) >= (t23));
  if (t24) {
  {
  v10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t25 = ((int64_t)(v9));
  t26 = ((int64_t)(((int64_t)(v2))));
  t27 = ((t25) < (t26));
  if (t27) {
  {
  t28 = ((int64_t)(v6));
  t29 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t30 = ((t28) == (t29));
  if (t30) {
  {
  t31 = ((uint64_t)(v0));
  t32 = ((int64_t)(v8));
  t33 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t34 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t32) * (uint64_t)(t33)))))));
#ifndef __cplusplus
  extern float (fg283d_ld_f32)(uint64_t, uint64_t);
#endif
  t35 = fg283d_ld_f32(t31, t34);
  t36 = ((float)(t35));
  t37 = ((uint64_t)(v0));
  t38 = ((int64_t)(v9));
  t39 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t40 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t38) * (uint64_t)(t39)))))));
#ifndef __cplusplus
  extern float (fg283d_ld_f32)(uint64_t, uint64_t);
#endif
  t41 = fg283d_ld_f32(t37, t40);
  t42 = ((float)(t41));
  t43 = ((t36) > (t42));
  if (t43) {
  {
  v10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  }
  }
  }
  } else {
  {
  t44 = ((uint64_t)(v0));
  t45 = ((int64_t)(v8));
  t46 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t47 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t45) * (uint64_t)(t46)))))));
#ifndef __cplusplus
  extern float (fg283d_ld_f32)(uint64_t, uint64_t);
#endif
  t48 = fg283d_ld_f32(t44, t47);
  t49 = ((float)(t48));
  t50 = ((uint64_t)(v0));
  t51 = ((int64_t)(v9));
  t52 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t53 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t51) * (uint64_t)(t52)))))));
#ifndef __cplusplus
  extern float (fg283d_ld_f32)(uint64_t, uint64_t);
#endif
  t54 = fg283d_ld_f32(t50, t53);
  t55 = ((float)(t54));
  t56 = ((t49) < (t55));
  if (t56) {
  {
  v10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  }
  }
  }
  }
  }
  }
  }
  }
  }
  } else {
  {
  t57 = ((int64_t)(v9));
  t58 = ((int64_t)(((int64_t)(v2))));
  t59 = ((t57) >= (t58));
  if (t59) {
  {
  v10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  }
  } else {
  {
  t60 = ((int64_t)(v8));
  t61 = ((int64_t)(((int64_t)(v2))));
  t62 = ((t60) < (t61));
  if (t62) {
  {
  t63 = ((int64_t)(v6));
  t64 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t65 = ((t63) == (t64));
  if (t65) {
  {
  t66 = ((uint64_t)(v0));
  t67 = ((int64_t)(v8));
  t68 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t69 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t67) * (uint64_t)(t68)))))));
#ifndef __cplusplus
  extern float (fg283d_ld_f32)(uint64_t, uint64_t);
#endif
  t70 = fg283d_ld_f32(t66, t69);
  t71 = ((float)(t70));
  t72 = ((uint64_t)(v0));
  t73 = ((int64_t)(v9));
  t74 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t75 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t73) * (uint64_t)(t74)))))));
#ifndef __cplusplus
  extern float (fg283d_ld_f32)(uint64_t, uint64_t);
#endif
  t76 = fg283d_ld_f32(t72, t75);
  t77 = ((float)(t76));
  t78 = ((t71) < (t77));
  if (t78) {
  {
  v10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  }
  }
  }
  } else {
  {
  t79 = ((uint64_t)(v0));
  t80 = ((int64_t)(v8));
  t81 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t82 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t80) * (uint64_t)(t81)))))));
#ifndef __cplusplus
  extern float (fg283d_ld_f32)(uint64_t, uint64_t);
#endif
  t83 = fg283d_ld_f32(t79, t82);
  t84 = ((float)(t83));
  t85 = ((uint64_t)(v0));
  t86 = ((int64_t)(v9));
  t87 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t88 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t86) * (uint64_t)(t87)))))));
#ifndef __cplusplus
  extern float (fg283d_ld_f32)(uint64_t, uint64_t);
#endif
  t89 = fg283d_ld_f32(t85, t88);
  t90 = ((float)(t89));
  t91 = ((t84) > (t90));
  if (t91) {
  {
  v10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  t92 = ((int64_t)(v10));
  t93 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t94 = ((t92) != (t93));
  if (t94) {
  {
  t95 = ((uint64_t)(v1));
  t96 = ((int64_t)(v3));
  t97 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t98 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t96) * (uint64_t)(t97)))))));
  t99 = ((uint64_t)(((uint64_t)((uint64_t)(t95) + (uint64_t)(t98)))));
  t100 = ((int32_t)(((int32_t)(v9))));
#ifndef __cplusplus
  extern void (fg283d_sstore_i32)(uint64_t, int32_t);
#endif
  fg283d_sstore_i32(t99, t100);
  t101 = ((uint64_t)(v1));
  t102 = ((int64_t)(v7));
  t103 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t104 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t102) * (uint64_t)(t103)))))));
  t105 = ((uint64_t)(((uint64_t)((uint64_t)(t101) + (uint64_t)(t104)))));
  t106 = ((int32_t)(((int32_t)(v8))));
#ifndef __cplusplus
  extern void (fg283d_sstore_i32)(uint64_t, int32_t);
#endif
  fg283d_sstore_i32(t105, t106);
  }
  }
  }
fg283dd_gg_done:;
}
static __device__ __forceinline__ void fg283dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg283d_fail)(int64_t);
#endif
  fg283d_fail(t2);
  }
  }
  }
fg283dd_gg_done:;
}
static __device__ __forceinline__ void fg283dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t base) {
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
  uint8_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  int64_t t15;
  uint64_t t16;
  int64_t t17;
  int64_t t18;
  uint64_t t19;
  int64_t t20;
  int64_t t21;
  uint64_t t22;
  uint64_t t23;
  int32_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  uint64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint64_t t38;
  int64_t t39;
  int64_t t40;
  uint64_t t41;
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
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  uint64_t t56;
  int64_t t57;
  int64_t t58;
  uint64_t t59;
  uint64_t t60;
  int32_t t61;
  uint64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  uint64_t t69;
  int32_t t70;
  int64_t t71;
  int64_t t72;
  uint64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  uint64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  uint64_t v12;
  int32_t v13;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  {
  t0 = ((int64_t)(v2));
  t1 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t2 = ((t0) > (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(((int64_t)(v2))));
  t3 = ((t4) >= (t5));
  }
  t6 = t3;
  t7 = ((int64_t)(((int64_t)UINT64_C(20))));
  fg283dd_gg_function_2(t6, t7, base + 0);
  t8 = ((int64_t)(v5));
  t9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t10 = ((t8) > (t9));
  t11 = t10;
  if (t11) {
  t12 = ((int64_t)(v6));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t11 = ((t12) >= (t13));
  }
  t14 = t11;
  t15 = ((int64_t)(((int64_t)UINT64_C(21))));
  fg283dd_gg_function_2(t14, t15, base + 0);
#ifndef __cplusplus
  extern uint64_t (fg283d_smem)(void);
#endif
  t16 = fg283d_smem();
  v8 = ((uint64_t)(t16));
  v9 = ((int64_t)(v6));
label1:;
  t17 = ((int64_t)(v9));
  t18 = ((int64_t)(((int64_t)(v3))));
  if (!(((t17) < (t18)))) goto label2;
  {
  t19 = ((uint64_t)(v8));
  t20 = ((int64_t)(v9));
  t21 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t22 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))))));
  t23 = ((uint64_t)(((uint64_t)((uint64_t)(t19) + (uint64_t)(t22)))));
  t24 = ((int32_t)(((int32_t)(v9))));
#ifndef __cplusplus
  extern void (fg283d_sstore_i32)(uint64_t, int32_t);
#endif
  fg283d_sstore_i32(t23, t24);
  t25 = ((int64_t)(v9));
  t26 = ((int64_t)(v5));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t26)))));
  }
  goto label1;
label2:;
#ifndef __cplusplus
  extern void (fg283d_sync)(void);
#endif
  fg283d_sync();
  v10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
label4:;
  t27 = ((int64_t)(v10));
  t28 = ((int64_t)(((int64_t)(v3))));
  if (!(((t27) <= (t28)))) goto label5;
  {
  t29 = ((int64_t)(v10));
  t30 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  v11 = ((int64_t)(((int64_t)fg283dd_gg_div(t29, t30))));
label7:;
  t31 = ((int64_t)(v11));
  t32 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  if (!(((t31) > (t32)))) goto label8;
  {
  v9 = ((int64_t)(v6));
  t33 = ((uint64_t)(v0));
  t34 = ((int64_t)(v7));
  t35 = ((int64_t)(v2));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t34) * (uint64_t)(t35)))));
  t37 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t38 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t36) * (uint64_t)(t37)))))));
  v12 = ((uint64_t)(((uint64_t)((uint64_t)(t33) + (uint64_t)(t38)))));
label10:;
  t39 = ((int64_t)(v9));
  t40 = ((int64_t)(((int64_t)(v3))));
  if (!(((t39) < (t40)))) goto label11;
  {
  t41 = ((uint64_t)(v12));
  t42 = ((uint64_t)(v8));
  t43 = ((int64_t)(v2));
  t44 = ((int64_t)(v9));
  t45 = ((int64_t)(v10));
  t46 = ((int64_t)(v11));
  t47 = ((int64_t)(v4));
  fg283dd_gg_function_7(t41, t42, t43, t44, t45, t46, t47, base + 0);
  t48 = ((int64_t)(v9));
  t49 = ((int64_t)(v5));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t48) + (uint64_t)(t49)))));
  }
  goto label10;
label11:;
#ifndef __cplusplus
  extern void (fg283d_sync)(void);
#endif
  fg283d_sync();
  t50 = ((int64_t)(v11));
  t51 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  v11 = ((int64_t)(((int64_t)fg283dd_gg_div(t50, t51))));
  }
  goto label7;
label8:;
  t52 = ((int64_t)(v10));
  t53 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t52) * (uint64_t)(t53)))));
  }
  goto label4;
label5:;
  v9 = ((int64_t)(v6));
label13:;
  t54 = ((int64_t)(v9));
  t55 = ((int64_t)(((int64_t)(v2))));
  if (!(((t54) < (t55)))) goto label14;
  {
  t56 = ((uint64_t)(v8));
  t57 = ((int64_t)(v9));
  t58 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t59 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t57) * (uint64_t)(t58)))))));
  t60 = ((uint64_t)(((uint64_t)((uint64_t)(t56) + (uint64_t)(t59)))));
#ifndef __cplusplus
  extern int32_t (fg283d_sload_i32)(uint64_t);
#endif
  t61 = fg283d_sload_i32(t60);
  v13 = ((int32_t)(t61));
  t62 = ((uint64_t)(v1));
  t63 = ((int64_t)(v7));
  t64 = ((int64_t)(v2));
  t65 = ((int64_t)(((int64_t)((uint64_t)(t63) * (uint64_t)(t64)))));
  t66 = ((int64_t)(v9));
  t67 = ((int64_t)(((int64_t)((uint64_t)(t65) + (uint64_t)(t66)))));
  t68 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t69 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t67) * (uint64_t)(t68)))))));
  t70 = ((int32_t)(v13));
#ifndef __cplusplus
  extern void (fg283d_st_i32)(uint64_t, uint64_t, int32_t);
#endif
  fg283d_st_i32(t62, t69, t70);
  t71 = ((int64_t)(v9));
  t72 = ((int64_t)(v5));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t71) + (uint64_t)(t72)))));
  }
  goto label13;
label14:;
  }
fg283dd_gg_done:;
}
