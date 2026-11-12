#pragma once
static __device__ __forceinline__ void fg281dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg281dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg281dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg281dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg281dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg281dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ float fg281dd_gg_function_19(float p0, int64_t base);
static __device__ __forceinline__ float fg281dd_gg_function_18(float p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base);
static __device__ __forceinline__ float fg281dd_gg_function_13(float p0, int64_t base);
static __device__ __forceinline__ float fg281dd_gg_function_12(float p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base);
static __device__ __forceinline__ void fg281dd_gg_function_4(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, float p4, float p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t base);
static __device__ __forceinline__ float fg281dd_gg_function_2(float p0, int64_t p1, int64_t p2, float p3, float p4, int64_t base);
static __device__ __forceinline__ void fg281dd_gg_function_20(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, float p4, float p5, int64_t p6, float p7, float p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, int64_t p19, int64_t p20, int64_t p21, int64_t base);
static __device__ __forceinline__ void fg281dd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, float p4, float p5, int64_t p6, float p7, float p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, int64_t p19, int64_t p20, int64_t p21, int64_t base);
static __device__ __forceinline__ void fg281dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg281dd_gg_div(int64_t a, int64_t b) { if (!b) fg281dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg281dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg281dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg281dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg281dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg281dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg281dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg281dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg281dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ float fg281dd_gg_function_19(float p0, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  float t3;
  int32_t t4;
  float t5;
  float t6;
  float t7;
  int64_t t8;
  int64_t t9;
  float v0;
  float v1;
  int64_t v2;
  float v3;
  v0 = p0;
  {
  v1 = ((float)(v0));
  v2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
label1:;
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  if (!(((t1) >= (t2)))) goto label2;
  {
  t3 = ((float)(v1));
  t4 = ((int32_t)(((int32_t)(v2))));
#ifndef __cplusplus
  extern float (fg281d_shfl_xor)(float, int32_t);
#endif
  t5 = fg281d_shfl_xor(t3, t4);
  v3 = ((float)(t5));
  t6 = ((float)(v1));
  t7 = ((float)(v3));
  v1 = ((float)(((t6) + (t7))));
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  v2 = ((int64_t)(((int64_t)fg281dd_gg_div(t8, t9))));
  }
  goto label1;
label2:;
  t0 = ((float)(v1));
  }
fg281dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg281dd_gg_function_18(float p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base) {
  float t0;
  float t1;
  int64_t t2;
  int64_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  uint64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  uint64_t t15;
  uint64_t t16;
  float t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  uint8_t t24;
  uint64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  uint64_t t30;
  uint64_t t31;
  float t32;
  float t33;
  float v0;
  int64_t v1;
  int64_t v2;
  uint64_t v3;
  float v4;
  float v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((float)(v0));
  v4 = fg281dd_gg_function_19(t1, base + 0);
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t4 = ((t2) > (t3));
  if (t4) {
  {
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t7 = ((int64_t)(((int64_t)fg281dd_gg_mod(t5, t6))));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t10 = ((uint64_t)(v3));
  t11 = ((int64_t)(v2));
  t12 = ((int64_t)(((int64_t)UINT64_C(32))));
  t13 = ((int64_t)(((int64_t)fg281dd_gg_div(t11, t12))));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t15 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t13) * (uint64_t)(t14)))))));
  t16 = ((uint64_t)(((uint64_t)((uint64_t)(t10) + (uint64_t)(t15)))));
  t17 = ((float)(v4));
#ifndef __cplusplus
  extern void (fg281d_sstore)(uint64_t, float);
#endif
  fg281d_sstore(t16, t17);
  }
  }
#ifndef __cplusplus
  extern void (fg281d_sync)(void);
#endif
  fg281d_sync();
  v5 = ((float)(((float)(fg281dd_gg_double_bits(UINT64_C(0))))));
  t18 = ((int64_t)(v2));
  t19 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t20 = ((int64_t)(((int64_t)fg281dd_gg_mod(t18, t19))));
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t23 = ((int64_t)(((int64_t)fg281dd_gg_div(t21, t22))));
  t24 = ((t20) < (t23));
  if (t24) {
  {
  t25 = ((uint64_t)(v3));
  t26 = ((int64_t)(v2));
  t27 = ((int64_t)(((int64_t)UINT64_C(32))));
  t28 = ((int64_t)(((int64_t)fg281dd_gg_mod(t26, t27))));
  t29 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t30 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))))));
  t31 = ((uint64_t)(((uint64_t)((uint64_t)(t25) + (uint64_t)(t30)))));
#ifndef __cplusplus
  extern float (fg281d_sload)(uint64_t);
#endif
  t32 = fg281d_sload(t31);
  v5 = ((float)(t32));
  }
  }
  t33 = ((float)(v5));
  v4 = fg281dd_gg_function_19(t33, base + 0);
  }
  }
  t0 = ((float)(v4));
  }
fg281dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg281dd_gg_function_13(float p0, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  float t3;
  int32_t t4;
  float t5;
  float t6;
  float t7;
  float t8;
  int64_t t9;
  int64_t t10;
  float v0;
  float v1;
  int64_t v2;
  float v3;
  v0 = p0;
  {
  v1 = ((float)(v0));
  v2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
label1:;
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  if (!(((t1) >= (t2)))) goto label2;
  {
  t3 = ((float)(v1));
  t4 = ((int32_t)(((int32_t)(v2))));
#ifndef __cplusplus
  extern float (fg281d_shfl_xor)(float, int32_t);
#endif
  t5 = fg281d_shfl_xor(t3, t4);
  v3 = ((float)(t5));
  t6 = ((float)(v1));
  t7 = ((float)(v3));
#ifndef __cplusplus
  extern float (fg281d_fmaxf)(float, float);
#endif
  t8 = fg281d_fmaxf(t6, t7);
  v1 = ((float)(t8));
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  v2 = ((int64_t)(((int64_t)fg281dd_gg_div(t9, t10))));
  }
  goto label1;
label2:;
  t0 = ((float)(v1));
  }
fg281dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg281dd_gg_function_12(float p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base) {
  float t0;
  float t1;
  int64_t t2;
  int64_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  uint64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  uint64_t t15;
  uint64_t t16;
  float t17;
  float t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  uint64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  uint64_t t31;
  uint64_t t32;
  float t33;
  float t34;
  float v0;
  int64_t v1;
  int64_t v2;
  uint64_t v3;
  float v4;
  float v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((float)(v0));
  v4 = fg281dd_gg_function_13(t1, base + 0);
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t4 = ((t2) > (t3));
  if (t4) {
  {
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t7 = ((int64_t)(((int64_t)fg281dd_gg_mod(t5, t6))));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t10 = ((uint64_t)(v3));
  t11 = ((int64_t)(v2));
  t12 = ((int64_t)(((int64_t)UINT64_C(32))));
  t13 = ((int64_t)(((int64_t)fg281dd_gg_div(t11, t12))));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t15 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t13) * (uint64_t)(t14)))))));
  t16 = ((uint64_t)(((uint64_t)((uint64_t)(t10) + (uint64_t)(t15)))));
  t17 = ((float)(v4));
#ifndef __cplusplus
  extern void (fg281d_sstore)(uint64_t, float);
#endif
  fg281d_sstore(t16, t17);
  }
  }
#ifndef __cplusplus
  extern void (fg281d_sync)(void);
#endif
  fg281d_sync();
#ifndef __cplusplus
  extern float (fg281d_ninf)(void);
#endif
  t18 = fg281d_ninf();
  v5 = ((float)(t18));
  t19 = ((int64_t)(v2));
  t20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t21 = ((int64_t)(((int64_t)fg281dd_gg_mod(t19, t20))));
  t22 = ((int64_t)(v1));
  t23 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t24 = ((int64_t)(((int64_t)fg281dd_gg_div(t22, t23))));
  t25 = ((t21) < (t24));
  if (t25) {
  {
  t26 = ((uint64_t)(v3));
  t27 = ((int64_t)(v2));
  t28 = ((int64_t)(((int64_t)UINT64_C(32))));
  t29 = ((int64_t)(((int64_t)fg281dd_gg_mod(t27, t28))));
  t30 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t31 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t29) * (uint64_t)(t30)))))));
  t32 = ((uint64_t)(((uint64_t)((uint64_t)(t26) + (uint64_t)(t31)))));
#ifndef __cplusplus
  extern float (fg281d_sload)(uint64_t);
#endif
  t33 = fg281d_sload(t32);
  v5 = ((float)(t33));
  }
  }
  t34 = ((float)(v5));
  v4 = fg281dd_gg_function_13(t34, base + 0);
  }
  }
  t0 = ((float)(v4));
  }
fg281dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg281dd_gg_function_4(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, float p4, float p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint64_t t14;
  float t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  uint8_t t22;
  uint64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint64_t t28;
  float t29;
  int64_t t30;
  int64_t t31;
  uint8_t t32;
  uint64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint64_t t38;
  float t39;
  uint64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  uint64_t t45;
  float t46;
  float t47;
  float t48;
  float t49;
  float t50;
  float t51;
  float t52;
  int64_t t53;
  int64_t t54;
  uint8_t t55;
  uint64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  uint64_t t61;
  uint64_t t62;
  float t63;
  uint64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  uint64_t t69;
  float t70;
  float t71;
  float t72;
  float t73;
  int64_t t74;
  int64_t t75;
  float t76;
  int64_t t77;
  int64_t t78;
  uint64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  uint8_t t86;
  int64_t t87;
  int64_t t88;
  uint8_t t89;
  uint64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  uint64_t t95;
  uint64_t t96;
  float t97;
  uint64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  uint64_t t103;
  float t104;
  float t105;
  float t106;
  float t107;
  float t108;
  float t109;
  float t110;
  int64_t t111;
  int64_t t112;
  uint8_t t113;
  uint64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  uint64_t t119;
  uint64_t t120;
  float t121;
  uint64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  uint64_t t127;
  float t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  uint8_t t133;
  float t134;
  int64_t t135;
  int64_t t136;
  uint64_t t137;
  float t138;
  float t139;
  int64_t t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  uint8_t t146;
  int64_t t147;
  int64_t t148;
  uint8_t t149;
  uint64_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  uint64_t t155;
  uint64_t t156;
  float t157;
  uint64_t t158;
  int64_t t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  uint64_t t163;
  float t164;
  uint64_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  int64_t t169;
  uint64_t t170;
  float t171;
  float t172;
  float t173;
  int64_t t174;
  int64_t t175;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  int64_t v3;
  float v4;
  float v5;
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
  int64_t v18;
  uint64_t v19;
  int64_t v20;
  float v21;
  int64_t v22;
  int64_t v23;
  float v24;
  float v25;
  float v26;
  float v27;
  int64_t v28;
  float v29;
  float v30;
  float v31;
  int64_t v32;
  float v33;
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
  {
  t0 = ((int64_t)(v10));
  t1 = ((int64_t)(v11));
  t2 = ((int64_t)(v13));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) * (uint64_t)(t2)))));
  t4 = ((int64_t)(((int64_t)((uint64_t)(t0) + (uint64_t)(t3)))));
  t5 = ((int64_t)(v12));
  t6 = ((int64_t)(v13));
  t7 = ((int64_t)(((int64_t)((uint64_t)(t5) * (uint64_t)(t6)))));
  t8 = ((int64_t)(v14));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) * (uint64_t)(t8)))));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t4) + (uint64_t)(t9)))));
  t10 = ((int64_t)(v16));
  t11 = ((int64_t)(v3));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  t12 = ((int64_t)(v16));
  t13 = ((int64_t)(v3));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
#ifndef __cplusplus
  extern uint64_t (fg281d_smem)(void);
#endif
  t14 = fg281d_smem();
  v19 = ((uint64_t)(t14));
  v20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
#ifndef __cplusplus
  extern float (fg281d_ninf)(void);
#endif
  t15 = fg281d_ninf();
  v21 = ((float)(t15));
  v22 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label1:;
  t16 = ((int64_t)(v22));
  t17 = ((int64_t)(v3));
  if (!(((t16) < (t17)))) goto label2;
  {
  t18 = ((int64_t)(v22));
  t19 = ((int64_t)(v9));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t18) + (uint64_t)(t19)))));
  t20 = ((int64_t)(v23));
  t21 = ((int64_t)(((int64_t)(v3))));
  t22 = ((t20) < (t21));
  if (t22) {
  {
  t23 = ((uint64_t)(v0));
  t24 = ((int64_t)(v17));
  t25 = ((int64_t)(v23));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) + (uint64_t)(t25)))));
  t27 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t28 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t26) * (uint64_t)(t27)))))));
#ifndef __cplusplus
  extern float (fg281d_ld_f32)(uint64_t, uint64_t);
#endif
  t29 = fg281d_ld_f32(t23, t28);
  v24 = ((float)(t29));
  v25 = ((float)(((float)(fg281dd_gg_double_bits(UINT64_C(0))))));
  t30 = ((int64_t)(v7));
  t31 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t32 = ((t30) != (t31));
  if (t32) {
  {
  t33 = ((uint64_t)(v1));
  t34 = ((int64_t)(v6));
  t35 = ((int64_t)(v23));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t34) + (uint64_t)(t35)))));
  t37 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t38 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t36) * (uint64_t)(t37)))))));
#ifndef __cplusplus
  extern float (fg281d_ld_half)(uint64_t, uint64_t);
#endif
  t39 = fg281d_ld_half(t33, t38);
  v25 = ((float)(t39));
  }
  } else {
  {
  t40 = ((uint64_t)(v1));
  t41 = ((int64_t)(v6));
  t42 = ((int64_t)(v23));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t41) + (uint64_t)(t42)))));
  t44 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t45 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t43) * (uint64_t)(t44)))))));
#ifndef __cplusplus
  extern float (fg281d_ld_f32)(uint64_t, uint64_t);
#endif
  t46 = fg281d_ld_f32(t40, t45);
  v25 = ((float)(t46));
  }
  }
  t47 = ((float)(v24));
  t48 = ((float)(v4));
  t49 = ((float)(((t47) * (t48))));
  t50 = ((float)(v5));
  t51 = ((float)(v25));
  t52 = ((float)(((t50) * (t51))));
  v26 = ((float)(((t49) + (t52))));
  t53 = ((int64_t)(v15));
  t54 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t55 = ((t53) != (t54));
  if (t55) {
  {
  t56 = ((uint64_t)(v19));
  t57 = ((int64_t)(v20));
  t58 = ((int64_t)(v23));
  t59 = ((int64_t)(((int64_t)((uint64_t)(t57) + (uint64_t)(t58)))));
  t60 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t61 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t59) * (uint64_t)(t60)))))));
  t62 = ((uint64_t)(((uint64_t)((uint64_t)(t56) + (uint64_t)(t61)))));
  t63 = ((float)(v26));
#ifndef __cplusplus
  extern void (fg281d_sstore)(uint64_t, float);
#endif
  fg281d_sstore(t62, t63);
  }
  } else {
  {
  t64 = ((uint64_t)(v2));
  t65 = ((int64_t)(v18));
  t66 = ((int64_t)(v23));
  t67 = ((int64_t)(((int64_t)((uint64_t)(t65) + (uint64_t)(t66)))));
  t68 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t69 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t67) * (uint64_t)(t68)))))));
  t70 = ((float)(v26));
#ifndef __cplusplus
  extern void (fg281d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg281d_st_f32(t64, t69, t70);
  }
  }
  t71 = ((float)(v21));
  t72 = ((float)(v26));
#ifndef __cplusplus
  extern float (fg281d_fmaxf)(float, float);
#endif
  t73 = fg281d_fmaxf(t71, t72);
  v21 = ((float)(t73));
  }
  }
  t74 = ((int64_t)(v22));
  t75 = ((int64_t)(v8));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t74) + (uint64_t)(t75)))));
  }
  goto label1;
label2:;
  t76 = ((float)(v21));
  t77 = ((int64_t)(v8));
  t78 = ((int64_t)(v9));
  t79 = ((uint64_t)(v19));
  v21 = fg281dd_gg_function_12(t76, t77, t78, t79, base + 0);
  v27 = ((float)(((float)(fg281dd_gg_double_bits(UINT64_C(0))))));
  v22 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label4:;
  t80 = ((int64_t)(v22));
  t81 = ((int64_t)(v3));
  if (!(((t80) < (t81)))) goto label5;
  {
  t82 = ((int64_t)(v22));
  t83 = ((int64_t)(v9));
  v28 = ((int64_t)(((int64_t)((uint64_t)(t82) + (uint64_t)(t83)))));
  t84 = ((int64_t)(v28));
  t85 = ((int64_t)(((int64_t)(v3))));
  t86 = ((t84) < (t85));
  if (t86) {
  {
  v29 = ((float)(((float)(fg281dd_gg_double_bits(UINT64_C(0))))));
  t87 = ((int64_t)(v15));
  t88 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t89 = ((t87) != (t88));
  if (t89) {
  {
  t90 = ((uint64_t)(v19));
  t91 = ((int64_t)(v20));
  t92 = ((int64_t)(v28));
  t93 = ((int64_t)(((int64_t)((uint64_t)(t91) + (uint64_t)(t92)))));
  t94 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t95 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t93) * (uint64_t)(t94)))))));
  t96 = ((uint64_t)(((uint64_t)((uint64_t)(t90) + (uint64_t)(t95)))));
#ifndef __cplusplus
  extern float (fg281d_sload)(uint64_t);
#endif
  t97 = fg281d_sload(t96);
  v29 = ((float)(t97));
  }
  } else {
  {
  t98 = ((uint64_t)(v2));
  t99 = ((int64_t)(v18));
  t100 = ((int64_t)(v28));
  t101 = ((int64_t)(((int64_t)((uint64_t)(t99) + (uint64_t)(t100)))));
  t102 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t103 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t101) * (uint64_t)(t102)))))));
#ifndef __cplusplus
  extern float (fg281d_ld_f32)(uint64_t, uint64_t);
#endif
  t104 = fg281d_ld_f32(t98, t103);
  v29 = ((float)(t104));
  }
  }
  t105 = ((float)(v29));
  t106 = ((float)(v21));
  t107 = ((float)(((t105) - (t106))));
#ifndef __cplusplus
  extern float (fg281d_expf)(float);
#endif
  t108 = fg281d_expf(t107);
  v30 = ((float)(t108));
  t109 = ((float)(v27));
  t110 = ((float)(v30));
  v27 = ((float)(((t109) + (t110))));
  t111 = ((int64_t)(v15));
  t112 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t113 = ((t111) != (t112));
  if (t113) {
  {
  t114 = ((uint64_t)(v19));
  t115 = ((int64_t)(v20));
  t116 = ((int64_t)(v28));
  t117 = ((int64_t)(((int64_t)((uint64_t)(t115) + (uint64_t)(t116)))));
  t118 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t119 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t117) * (uint64_t)(t118)))))));
  t120 = ((uint64_t)(((uint64_t)((uint64_t)(t114) + (uint64_t)(t119)))));
  t121 = ((float)(v30));
#ifndef __cplusplus
  extern void (fg281d_sstore)(uint64_t, float);
#endif
  fg281d_sstore(t120, t121);
  }
  } else {
  {
  t122 = ((uint64_t)(v2));
  t123 = ((int64_t)(v18));
  t124 = ((int64_t)(v28));
  t125 = ((int64_t)(((int64_t)((uint64_t)(t123) + (uint64_t)(t124)))));
  t126 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t127 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t125) * (uint64_t)(t126)))))));
  t128 = ((float)(v30));
#ifndef __cplusplus
  extern void (fg281d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg281d_st_f32(t122, t127, t128);
  }
  }
  }
  }
  t129 = ((int64_t)(v22));
  t130 = ((int64_t)(v8));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t129) + (uint64_t)(t130)))));
  }
  goto label4;
label5:;
  t131 = ((int64_t)(v8));
  t132 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t133 = ((t131) > (t132));
  if (t133) {
  {
#ifndef __cplusplus
  extern void (fg281d_sync)(void);
#endif
  fg281d_sync();
  }
  }
  t134 = ((float)(v27));
  t135 = ((int64_t)(v8));
  t136 = ((int64_t)(v9));
  t137 = ((uint64_t)(v19));
  v27 = fg281dd_gg_function_18(t134, t135, t136, t137, base + 0);
  t138 = ((float)(((float)(fg281dd_gg_double_bits(UINT64_C(4607182418800017408))))));
  t139 = ((float)(v27));
  v31 = ((float)(((t138) / (t139))));
  v22 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label7:;
  t140 = ((int64_t)(v22));
  t141 = ((int64_t)(v3));
  if (!(((t140) < (t141)))) goto label8;
  {
  t142 = ((int64_t)(v22));
  t143 = ((int64_t)(v9));
  v32 = ((int64_t)(((int64_t)((uint64_t)(t142) + (uint64_t)(t143)))));
  t144 = ((int64_t)(v32));
  t145 = ((int64_t)(((int64_t)(v3))));
  t146 = ((t144) < (t145));
  if (t146) {
  {
  v33 = ((float)(((float)(fg281dd_gg_double_bits(UINT64_C(0))))));
  t147 = ((int64_t)(v15));
  t148 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t149 = ((t147) != (t148));
  if (t149) {
  {
  t150 = ((uint64_t)(v19));
  t151 = ((int64_t)(v20));
  t152 = ((int64_t)(v32));
  t153 = ((int64_t)(((int64_t)((uint64_t)(t151) + (uint64_t)(t152)))));
  t154 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t155 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t153) * (uint64_t)(t154)))))));
  t156 = ((uint64_t)(((uint64_t)((uint64_t)(t150) + (uint64_t)(t155)))));
#ifndef __cplusplus
  extern float (fg281d_sload)(uint64_t);
#endif
  t157 = fg281d_sload(t156);
  v33 = ((float)(t157));
  }
  } else {
  {
  t158 = ((uint64_t)(v2));
  t159 = ((int64_t)(v18));
  t160 = ((int64_t)(v32));
  t161 = ((int64_t)(((int64_t)((uint64_t)(t159) + (uint64_t)(t160)))));
  t162 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t163 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t161) * (uint64_t)(t162)))))));
#ifndef __cplusplus
  extern float (fg281d_ld_f32)(uint64_t, uint64_t);
#endif
  t164 = fg281d_ld_f32(t158, t163);
  v33 = ((float)(t164));
  }
  }
  t165 = ((uint64_t)(v2));
  t166 = ((int64_t)(v18));
  t167 = ((int64_t)(v32));
  t168 = ((int64_t)(((int64_t)((uint64_t)(t166) + (uint64_t)(t167)))));
  t169 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t170 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t168) * (uint64_t)(t169)))))));
  t171 = ((float)(v33));
  t172 = ((float)(v31));
  t173 = ((float)(((t171) * (t172))));
#ifndef __cplusplus
  extern void (fg281d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg281d_st_f32(t165, t170, t173);
  }
  }
  t174 = ((int64_t)(v22));
  t175 = ((int64_t)(v8));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t174) + (uint64_t)(t175)))));
  }
  goto label7;
label8:;
  }
fg281dd_gg_done:;
}
static __device__ __forceinline__ float fg281dd_gg_function_2(float p0, int64_t p1, int64_t p2, float p3, float p4, int64_t base) {
  float t0;
  float t1;
  float t2;
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
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  float t18;
  float t19;
  float t20;
  float v0;
  int64_t v1;
  int64_t v2;
  float v3;
  float v4;
  float v5;
  int64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((float)(v0));
  t2 = ((float)(((float)(fg281dd_gg_double_bits(UINT64_C(0))))));
  t3 = ((t1) <= (t2));
  if (t3) {
  {
  t0 = ((float)(((float)(fg281dd_gg_double_bits(UINT64_C(4607182418800017408))))));
  goto fg281dd_gg_done;
  }
  }
  v5 = ((float)(v4));
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)(v2))));
  t6 = ((t4) < (t5));
  if (t6) {
  {
  v5 = ((float)(v3));
  }
  }
  t7 = ((int64_t)(((int64_t)UINT64_C(2))));
  t8 = ((int64_t)(v1));
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t8) - (uint64_t)(t9)))));
  t11 = ((int64_t)(((int64_t)((uint64_t)(t7) * (uint64_t)(t10)))));
  t12 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t11) + (uint64_t)(t12)))));
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(((int64_t)(v2))));
  t15 = ((t13) < (t14));
  if (t15) {
  {
  t16 = ((int64_t)(v1));
  t17 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t16) + (uint64_t)(t17)))));
  }
  }
  t18 = ((float)(v5));
  t19 = ((float)(((float)(v6))));
#ifndef __cplusplus
  extern float (fg281d_powf)(float, float);
#endif
  t20 = fg281d_powf(t18, t19);
  t0 = ((float)(t20));
  }
fg281dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg281dd_gg_function_20(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, float p4, float p5, int64_t p6, float p7, float p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, int64_t p19, int64_t p20, int64_t p21, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
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
  float t16;
  int64_t t17;
  int64_t t18;
  float t19;
  float t20;
  uint64_t t21;
  uint64_t t22;
  uint64_t t23;
  int64_t t24;
  float t25;
  float t26;
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
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  int64_t v3;
  float v4;
  float v5;
  int64_t v6;
  float v7;
  float v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  int64_t v17;
  int64_t v18;
  int64_t v19;
  int64_t v20;
  int64_t v21;
  int64_t v22;
  int64_t v23;
  int64_t v24;
  float v25;
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
  {
  t0 = ((int64_t)(v17));
  t1 = ((int64_t)(v9));
  v22 = ((int64_t)(((int64_t)fg281dd_gg_mod(t0, t1))));
  t2 = ((int64_t)(v18));
  t3 = ((int64_t)(v10));
  v23 = ((int64_t)(((int64_t)fg281dd_gg_mod(t2, t3))));
  t4 = ((int64_t)(v16));
  t5 = ((int64_t)(v11));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  t7 = ((int64_t)(v22));
  t8 = ((int64_t)(v12));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) * (uint64_t)(t8)))));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t6) + (uint64_t)(t9)))));
  t11 = ((int64_t)(v23));
  t12 = ((int64_t)(v13));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t13)))));
  t15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  v24 = ((int64_t)(((int64_t)fg281dd_gg_div(t14, t15))));
  t16 = ((float)(v5));
  t17 = ((int64_t)(v17));
  t18 = ((int64_t)(v6));
  t19 = ((float)(v7));
  t20 = ((float)(v8));
  v25 = fg281dd_gg_function_2(t16, t17, t18, t19, t20, base + 0);
  t21 = ((uint64_t)(v0));
  t22 = ((uint64_t)(v1));
  t23 = ((uint64_t)(v2));
  t24 = ((int64_t)(v3));
  t25 = ((float)(v4));
  t26 = ((float)(v25));
  t27 = ((int64_t)(v24));
  t28 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t29 = ((int64_t)(v14));
  t30 = ((int64_t)(v15));
  t31 = ((int64_t)(v16));
  t32 = ((int64_t)(v17));
  t33 = ((int64_t)(v18));
  t34 = ((int64_t)(v19));
  t35 = ((int64_t)(v20));
  t36 = ((int64_t)(v21));
  fg281dd_gg_function_4(t21, t22, t23, t24, t25, t26, t27, t28, t29, t30, t31, t32, t33, t34, t35, t36, base + 0);
  }
fg281dd_gg_done:;
}
static __device__ __forceinline__ void fg281dd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, float p4, float p5, int64_t p6, float p7, float p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, int64_t p19, int64_t p20, int64_t p21, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
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
  float t16;
  int64_t t17;
  int64_t t18;
  float t19;
  float t20;
  uint64_t t21;
  uint64_t t22;
  uint64_t t23;
  int64_t t24;
  float t25;
  float t26;
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
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  int64_t v3;
  float v4;
  float v5;
  int64_t v6;
  float v7;
  float v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  int64_t v17;
  int64_t v18;
  int64_t v19;
  int64_t v20;
  int64_t v21;
  int64_t v22;
  int64_t v23;
  int64_t v24;
  float v25;
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
  {
  t0 = ((int64_t)(v17));
  t1 = ((int64_t)(v9));
  v22 = ((int64_t)(((int64_t)fg281dd_gg_mod(t0, t1))));
  t2 = ((int64_t)(v18));
  t3 = ((int64_t)(v10));
  v23 = ((int64_t)(((int64_t)fg281dd_gg_mod(t2, t3))));
  t4 = ((int64_t)(v16));
  t5 = ((int64_t)(v11));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  t7 = ((int64_t)(v22));
  t8 = ((int64_t)(v12));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) * (uint64_t)(t8)))));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t6) + (uint64_t)(t9)))));
  t11 = ((int64_t)(v23));
  t12 = ((int64_t)(v13));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t13)))));
  t15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v24 = ((int64_t)(((int64_t)fg281dd_gg_div(t14, t15))));
  t16 = ((float)(v5));
  t17 = ((int64_t)(v17));
  t18 = ((int64_t)(v6));
  t19 = ((float)(v7));
  t20 = ((float)(v8));
  v25 = fg281dd_gg_function_2(t16, t17, t18, t19, t20, base + 0);
  t21 = ((uint64_t)(v0));
  t22 = ((uint64_t)(v1));
  t23 = ((uint64_t)(v2));
  t24 = ((int64_t)(v3));
  t25 = ((float)(v4));
  t26 = ((float)(v25));
  t27 = ((int64_t)(v24));
  t28 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t29 = ((int64_t)(v14));
  t30 = ((int64_t)(v15));
  t31 = ((int64_t)(v16));
  t32 = ((int64_t)(v17));
  t33 = ((int64_t)(v18));
  t34 = ((int64_t)(v19));
  t35 = ((int64_t)(v20));
  t36 = ((int64_t)(v21));
  fg281dd_gg_function_4(t21, t22, t23, t24, t25, t26, t27, t28, t29, t30, t31, t32, t33, t34, t35, t36, base + 0);
  }
fg281dd_gg_done:;
}
