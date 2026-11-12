#pragma once
static __device__ __forceinline__ void fg287dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg287dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg287dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg287dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg287dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg287dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ float fg287dd_gg_function_9(float p0, int64_t base);
static __device__ __forceinline__ void fg287dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg287dd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, uint8_t p18, int64_t p19, int64_t p20, int64_t p21, int64_t p22, int64_t p23, int64_t p24, int64_t base);
static __device__ __forceinline__ void fg287dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg287dd_gg_div(int64_t a, int64_t b) { if (!b) fg287dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg287dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg287dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg287dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg287dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg287dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg287dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg287dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg287dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ float fg287dd_gg_function_9(float p0, int64_t base) {
  float t0;
  float t1;
  float t2;
  int32_t t3;
  int32_t t4;
  float t5;
  float t6;
  float t7;
  float t8;
  int32_t t9;
  int32_t t10;
  float t11;
  float t12;
  float t13;
  float t14;
  int32_t t15;
  int32_t t16;
  float t17;
  float t18;
  float t19;
  float t20;
  int32_t t21;
  int32_t t22;
  float t23;
  float t24;
  float t25;
  float t26;
  int32_t t27;
  int32_t t28;
  float t29;
  float t30;
  float v0;
  float v1;
  v0 = p0;
  {
  v1 = ((float)(v0));
  t1 = ((float)(v1));
  t2 = ((float)(v1));
  t3 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(16))))));
  t4 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
#ifndef __cplusplus
  extern float (fg287m_shfl_xor)(float, int32_t, int32_t);
#endif
  t5 = fg287m_shfl_xor(t2, t3, t4);
  t6 = ((float)(t5));
  v1 = ((float)(((t1) + (t6))));
  t7 = ((float)(v1));
  t8 = ((float)(v1));
  t9 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(8))))));
  t10 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
#ifndef __cplusplus
  extern float (fg287m_shfl_xor)(float, int32_t, int32_t);
#endif
  t11 = fg287m_shfl_xor(t8, t9, t10);
  t12 = ((float)(t11));
  v1 = ((float)(((t7) + (t12))));
  t13 = ((float)(v1));
  t14 = ((float)(v1));
  t15 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(4))))));
  t16 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
#ifndef __cplusplus
  extern float (fg287m_shfl_xor)(float, int32_t, int32_t);
#endif
  t17 = fg287m_shfl_xor(t14, t15, t16);
  t18 = ((float)(t17));
  v1 = ((float)(((t13) + (t18))));
  t19 = ((float)(v1));
  t20 = ((float)(v1));
  t21 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(2))))));
  t22 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
#ifndef __cplusplus
  extern float (fg287m_shfl_xor)(float, int32_t, int32_t);
#endif
  t23 = fg287m_shfl_xor(t20, t21, t22);
  t24 = ((float)(t23));
  v1 = ((float)(((t19) + (t24))));
  t25 = ((float)(v1));
  t26 = ((float)(v1));
  t27 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  t28 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
#ifndef __cplusplus
  extern float (fg287m_shfl_xor)(float, int32_t, int32_t);
#endif
  t29 = fg287m_shfl_xor(t26, t27, t28);
  t30 = ((float)(t29));
  v1 = ((float)(((t25) + (t30))));
  t0 = ((float)(v1));
  }
fg287dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg287dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg287m_fail)(int64_t);
#endif
  fg287m_fail(t2);
  }
  }
  }
fg287dd_gg_done:;
}
static __device__ __forceinline__ void fg287dd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, uint8_t p18, int64_t p19, int64_t p20, int64_t p21, int64_t p22, int64_t p23, int64_t p24, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  uint8_t t19;
  int64_t t20;
  int64_t t21;
  uint8_t t22;
  int64_t t23;
  uint8_t t24;
  uint8_t t25;
  int64_t t26;
  int64_t t27;
  uint8_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  uint8_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  uint8_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint64_t t47;
  uint64_t t48;
  int32_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  uint8_t t72;
  int64_t t73;
  int64_t t74;
  uint8_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
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
  uint8_t t90;
  uint64_t t91;
  uint64_t t92;
  float t93;
  uint64_t t94;
  int64_t t95;
  int64_t t96;
  uint64_t t97;
  float t98;
  int64_t t99;
  int64_t t100;
  uint8_t t101;
  uint64_t t102;
  uint64_t t103;
  float t104;
  uint64_t t105;
  int64_t t106;
  int64_t t107;
  uint64_t t108;
  float t109;
  uint64_t t110;
  uint64_t t111;
  float t112;
  uint64_t t113;
  int64_t t114;
  int64_t t115;
  uint64_t t116;
  float t117;
  uint64_t t118;
  uint64_t t119;
  float t120;
  uint64_t t121;
  int64_t t122;
  int64_t t123;
  uint64_t t124;
  float t125;
  float t126;
  float t127;
  float t128;
  float t129;
  float t130;
  float t131;
  float t132;
  float t133;
  int64_t t134;
  int64_t t135;
  float t136;
  int64_t t137;
  int64_t t138;
  uint8_t t139;
  int64_t t140;
  int64_t t141;
  uint8_t t142;
  int32_t t143;
  float t144;
  int64_t t145;
  int64_t t146;
  int32_t t147;
  float t148;
  int64_t t149;
  int64_t t150;
  uint8_t t151;
  int32_t t152;
  float t153;
  float t154;
  int64_t t155;
  int64_t t156;
  uint8_t t157;
  int64_t t158;
  int64_t t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  int64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  uint64_t t168;
  uint64_t t169;
  float t170;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  int64_t v17;
  uint8_t v18;
  int64_t v19;
  int64_t v20;
  int64_t v21;
  int64_t v22;
  int64_t v23;
  int64_t v24;
  int64_t v25;
  int64_t v26;
  int64_t v27;
  int64_t v28;
  int64_t v29;
  int64_t v30;
  int64_t v31;
  int64_t v32;
  int64_t v33;
  int64_t v34;
  float v35;
  int64_t v36;
  int64_t v37;
  int64_t v38;
  float v39;
  float v40;
  float v41;
  float v42;
  int64_t v43;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  v8 = p8;
  v9 = p9;
  v10 = p10;
  v11 = p11;
  v12 = p12;
  v13 = p13;
  v14 = p14;
  v15 = p15;
  v16 = p16;
  v17 = p17;
  v18 = p18;
  v19 = p19;
  v20 = p20;
  v21 = p21;
  v22 = p22;
  v23 = p23;
  v24 = p24;
  {
  t0 = ((int64_t)(v23));
  t1 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t2 = ((t0) > (t1));
  t3 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg287dd_gg_function_2(t2, t3, base + 0);
  t4 = ((int64_t)(v24));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t6 = ((t4) >= (t5));
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)(v24));
  t9 = ((int64_t)(((int64_t)(v23))));
  t7 = ((t8) < (t9));
  }
  t10 = t7;
  t11 = ((int64_t)(((int64_t)UINT64_C(11))));
  fg287dd_gg_function_2(t10, t11, base + 0);
  t12 = ((int64_t)(v5));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t14 = ((t12) >= (t13));
  t15 = ((int64_t)(((int64_t)UINT64_C(12))));
  fg287dd_gg_function_2(t14, t15, base + 0);
  t16 = ((int64_t)(v4));
  t17 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t18 = ((t16) >= (t17));
  t19 = t18;
  if (t19) {
  t20 = ((int64_t)(v4));
  t21 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t19 = ((t20) <= (t21));
  }
  t22 = t19;
  t23 = ((int64_t)(((int64_t)UINT64_C(15))));
  fg287dd_gg_function_2(t22, t23, base + 0);
  v25 = ((int64_t)(v21));
  v26 = ((int64_t)(v21));
  v27 = ((int64_t)(v22));
  t24 = v18;
  t25 = (!(t24));
  if (t25) {
  {
  t26 = ((int64_t)(v16));
  t27 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t28 = ((t26) > (t27));
  t29 = ((int64_t)(((int64_t)UINT64_C(13))));
  fg287dd_gg_function_2(t28, t29, base + 0);
  t30 = ((int64_t)(v21));
  t31 = ((int64_t)(v16));
  v25 = ((int64_t)(((int64_t)fg287dd_gg_div(t30, t31))));
  t32 = ((int64_t)(v21));
  t33 = ((int64_t)(v15));
  v26 = ((int64_t)(((int64_t)fg287dd_gg_mod(t32, t33))));
  t34 = ((int64_t)(v17));
  t35 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t36 = ((t34) > (t35));
  t37 = ((int64_t)(((int64_t)UINT64_C(14))));
  fg287dd_gg_function_2(t36, t37, base + 0);
  t38 = ((int64_t)(v22));
  t39 = ((int64_t)(v17));
  v27 = ((int64_t)(((int64_t)fg287dd_gg_div(t38, t39))));
  }
  }
  t40 = v18;
  if (t40) {
  {
  t41 = ((int64_t)(v21));
  t42 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t43 = ((int64_t)(v19));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) * (uint64_t)(t43)))));
  t45 = ((int64_t)(((int64_t)((uint64_t)(t41) + (uint64_t)(t44)))));
  t46 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v28 = ((int64_t)(((int64_t)((uint64_t)(t45) * (uint64_t)(t46)))));
  t47 = ((uint64_t)(v3));
  t48 = ((uint64_t)(((uint64_t)(((int64_t)(v28))))));
#ifndef __cplusplus
  extern int32_t (fg287m_ld_i32)(uint64_t, uint64_t);
#endif
  t49 = fg287m_ld_i32(t47, t48);
  v25 = ((int64_t)(((int64_t)(t49))));
  t50 = ((int64_t)(v21));
  t51 = ((int64_t)(v15));
  v26 = ((int64_t)(((int64_t)fg287dd_gg_mod(t50, t51))));
  v27 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  }
  }
  t52 = ((int64_t)(v27));
  t53 = ((int64_t)(v12));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t52) * (uint64_t)(t53)))));
  t55 = ((int64_t)(v25));
  t56 = ((int64_t)(v9));
  t57 = ((int64_t)(((int64_t)((uint64_t)(t55) * (uint64_t)(t56)))));
  t58 = ((int64_t)(((int64_t)((uint64_t)(t54) + (uint64_t)(t57)))));
  t59 = ((int64_t)(v20));
  t60 = ((int64_t)(v6));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t59) * (uint64_t)(t60)))));
  v29 = ((int64_t)(((int64_t)((uint64_t)(t58) + (uint64_t)(t61)))));
  t62 = ((int64_t)(v22));
  t63 = ((int64_t)(v13));
  t64 = ((int64_t)(((int64_t)((uint64_t)(t62) * (uint64_t)(t63)))));
  t65 = ((int64_t)(v26));
  t66 = ((int64_t)(v10));
  t67 = ((int64_t)(((int64_t)((uint64_t)(t65) * (uint64_t)(t66)))));
  v30 = ((int64_t)(((int64_t)((uint64_t)(t64) + (uint64_t)(t67)))));
  t68 = ((int64_t)(v30));
  t69 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v31 = ((int64_t)(((int64_t)((uint64_t)(t68) * (uint64_t)(t69)))));
  v32 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t70 = ((int64_t)(v4));
  t71 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t72 = ((t70) != (t71));
  if (t72) {
  {
  v32 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  }
  }
  v33 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t73 = ((int64_t)(v4));
  t74 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t75 = ((t73) != (t74));
  if (t75) {
  {
  v33 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  }
  }
  t76 = ((int64_t)(v29));
  t77 = ((int64_t)(v32));
  v34 = ((int64_t)(((int64_t)((uint64_t)(t76) * (uint64_t)(t77)))));
  v35 = ((float)(((float)(fg287dd_gg_double_bits(UINT64_C(0))))));
  v36 = ((int64_t)(v24));
label1:;
  t78 = ((int64_t)(v36));
  t79 = ((int64_t)(((int64_t)(v5))));
  if (!(((t78) < (t79)))) goto label2;
  {
  t80 = ((int64_t)(v34));
  t81 = ((int64_t)(v36));
  t82 = ((int64_t)(v33));
  t83 = ((int64_t)(((int64_t)((uint64_t)(t81) * (uint64_t)(t82)))));
  v37 = ((int64_t)(((int64_t)((uint64_t)(t80) + (uint64_t)(t83)))));
  t84 = ((int64_t)(v31));
  t85 = ((int64_t)(v36));
  t86 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t87 = ((int64_t)(((int64_t)((uint64_t)(t85) * (uint64_t)(t86)))));
  v38 = ((int64_t)(((int64_t)((uint64_t)(t84) + (uint64_t)(t87)))));
  v39 = ((float)(((float)(fg287dd_gg_double_bits(UINT64_C(0))))));
  v40 = ((float)(((float)(fg287dd_gg_double_bits(UINT64_C(0))))));
  t88 = ((int64_t)(v4));
  t89 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t90 = ((t88) == (t89));
  if (t90) {
  {
  t91 = ((uint64_t)(v0));
  t92 = ((uint64_t)(((uint64_t)(v37))));
#ifndef __cplusplus
  extern float (fg287m_ld_f32)(uint64_t, uint64_t);
#endif
  t93 = fg287m_ld_f32(t91, t92);
  v39 = ((float)(t93));
  t94 = ((uint64_t)(v0));
  t95 = ((int64_t)(v37));
  t96 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t97 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t95) + (uint64_t)(t96)))))));
#ifndef __cplusplus
  extern float (fg287m_ld_f32)(uint64_t, uint64_t);
#endif
  t98 = fg287m_ld_f32(t94, t97);
  v40 = ((float)(t98));
  }
  } else {
  {
  t99 = ((int64_t)(v4));
  t100 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t101 = ((t99) == (t100));
  if (t101) {
  {
  t102 = ((uint64_t)(v0));
  t103 = ((uint64_t)(((uint64_t)(v37))));
#ifndef __cplusplus
  extern float (fg287m_ld_half)(uint64_t, uint64_t);
#endif
  t104 = fg287m_ld_half(t102, t103);
  v39 = ((float)(t104));
  t105 = ((uint64_t)(v0));
  t106 = ((int64_t)(v37));
  t107 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t108 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t106) + (uint64_t)(t107)))))));
#ifndef __cplusplus
  extern float (fg287m_ld_half)(uint64_t, uint64_t);
#endif
  t109 = fg287m_ld_half(t105, t108);
  v40 = ((float)(t109));
  }
  } else {
  {
  t110 = ((uint64_t)(v0));
  t111 = ((uint64_t)(((uint64_t)(v37))));
#ifndef __cplusplus
  extern float (fg287m_ld_bf16)(uint64_t, uint64_t);
#endif
  t112 = fg287m_ld_bf16(t110, t111);
  v39 = ((float)(t112));
  t113 = ((uint64_t)(v0));
  t114 = ((int64_t)(v37));
  t115 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t116 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t114) + (uint64_t)(t115)))))));
#ifndef __cplusplus
  extern float (fg287m_ld_bf16)(uint64_t, uint64_t);
#endif
  t117 = fg287m_ld_bf16(t113, t116);
  v40 = ((float)(t117));
  }
  }
  }
  }
  t118 = ((uint64_t)(v1));
  t119 = ((uint64_t)(((uint64_t)(v38))));
#ifndef __cplusplus
  extern float (fg287m_ld_f32)(uint64_t, uint64_t);
#endif
  t120 = fg287m_ld_f32(t118, t119);
  v41 = ((float)(t120));
  t121 = ((uint64_t)(v1));
  t122 = ((int64_t)(v38));
  t123 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t124 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t122) + (uint64_t)(t123)))))));
#ifndef __cplusplus
  extern float (fg287m_ld_f32)(uint64_t, uint64_t);
#endif
  t125 = fg287m_ld_f32(t121, t124);
  v42 = ((float)(t125));
  t126 = ((float)(v35));
  t127 = ((float)(v39));
  t128 = ((float)(v41));
#ifndef __cplusplus
  extern float (fg287m_mad)(float, float, float);
#endif
  t129 = fg287m_mad(t126, t127, t128);
  v35 = ((float)(t129));
  t130 = ((float)(v35));
  t131 = ((float)(v40));
  t132 = ((float)(v42));
#ifndef __cplusplus
  extern float (fg287m_mad)(float, float, float);
#endif
  t133 = fg287m_mad(t130, t131, t132);
  v35 = ((float)(t133));
  t134 = ((int64_t)(v36));
  t135 = ((int64_t)(v23));
  v36 = ((int64_t)(((int64_t)((uint64_t)(t134) + (uint64_t)(t135)))));
  }
  goto label1;
label2:;
  t136 = ((float)(v35));
  v35 = fg287dd_gg_function_9(t136, base + 0);
  t137 = ((int64_t)(v23));
  t138 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t139 = ((t137) > (t138));
  if (t139) {
  {
  t140 = ((int64_t)(v24));
  t141 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t142 = ((t140) < (t141));
  if (t142) {
  {
  t143 = ((int32_t)(((int32_t)(v24))));
  t144 = ((float)(((float)(fg287dd_gg_double_bits(UINT64_C(0))))));
#ifndef __cplusplus
  extern void (fg287m_shm_put)(int32_t, float);
#endif
  fg287m_shm_put(t143, t144);
  }
  }
#ifndef __cplusplus
  extern void (fg287m_barrier)(void);
#endif
  fg287m_barrier();
  t145 = ((int64_t)(v24));
  t146 = ((int64_t)(((int64_t)UINT64_C(32))));
  t147 = ((int32_t)(((int32_t)(((int64_t)fg287dd_gg_div(t145, t146))))));
  t148 = ((float)(v35));
#ifndef __cplusplus
  extern void (fg287m_shm_put)(int32_t, float);
#endif
  fg287m_shm_put(t147, t148);
#ifndef __cplusplus
  extern void (fg287m_barrier)(void);
#endif
  fg287m_barrier();
  t149 = ((int64_t)(v24));
  t150 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t151 = ((t149) < (t150));
  if (t151) {
  {
  t152 = ((int32_t)(((int32_t)(v24))));
#ifndef __cplusplus
  extern float (fg287m_shm_get)(int32_t);
#endif
  t153 = fg287m_shm_get(t152);
  t154 = ((float)(t153));
  v35 = fg287dd_gg_function_9(t154, base + 0);
  }
  }
  }
  }
  t155 = ((int64_t)(v24));
  t156 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t157 = ((t155) < (t156));
  if (t157) {
  {
  t158 = ((int64_t)(v22));
  t159 = ((int64_t)(v14));
  t160 = ((int64_t)(((int64_t)((uint64_t)(t158) * (uint64_t)(t159)))));
  t161 = ((int64_t)(v21));
  t162 = ((int64_t)(v11));
  t163 = ((int64_t)(((int64_t)((uint64_t)(t161) * (uint64_t)(t162)))));
  t164 = ((int64_t)(((int64_t)((uint64_t)(t160) + (uint64_t)(t163)))));
  t165 = ((int64_t)(v20));
  t166 = ((int64_t)(((int64_t)((uint64_t)(t164) + (uint64_t)(t165)))));
  t167 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v43 = ((int64_t)(((int64_t)((uint64_t)(t166) * (uint64_t)(t167)))));
  t168 = ((uint64_t)(v2));
  t169 = ((uint64_t)(((uint64_t)(v43))));
  t170 = ((float)(v35));
#ifndef __cplusplus
  extern void (fg287m_st_f32)(uint64_t, uint64_t, float);
#endif
  fg287m_st_f32(t168, t169, t170);
  }
  }
  }
fg287dd_gg_done:;
}
