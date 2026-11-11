#pragma once
static __device__ __forceinline__ void fg282dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg282dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg282dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg282dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg282dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg282dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ float fg282dd_gg_function_19(float p0, int64_t base);
static __device__ __forceinline__ float fg282dd_gg_function_18(float p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base);
static __device__ __forceinline__ float fg282dd_gg_function_13(float p0, int64_t base);
static __device__ __forceinline__ float fg282dd_gg_function_12(float p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base);
static __device__ __forceinline__ void fg282dd_gg_function_4(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t p4, float p5, float p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t base);
static __device__ __forceinline__ float fg282dd_gg_function_2(float p0, int64_t p1, int64_t p2, float p3, float p4, int64_t base);
static __device__ __forceinline__ void fg282dd_gg_function_20(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t p4, float p5, float p6, int64_t p7, float p8, float p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, int64_t p19, int64_t p20, int64_t p21, int64_t p22, int64_t base);
static __device__ __forceinline__ void fg282dd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t p4, float p5, float p6, int64_t p7, float p8, float p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, int64_t p19, int64_t p20, int64_t p21, int64_t p22, int64_t base);
static __device__ __forceinline__ void fg282dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg282dd_gg_div(int64_t a, int64_t b) { if (!b) fg282dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg282dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg282dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg282dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg282dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg282dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg282dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg282dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg282dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ float fg282dd_gg_function_19(float p0, int64_t base) {
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
  extern float (fg282d_shfl_xor)(float, int32_t);
#endif
  t5 = fg282d_shfl_xor(t3, t4);
  v3 = ((float)(t5));
  t6 = ((float)(v1));
  t7 = ((float)(v3));
  v1 = ((float)(((t6) + (t7))));
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  v2 = ((int64_t)(((int64_t)fg282dd_gg_div(t8, t9))));
  }
  goto label1;
label2:;
  t0 = ((float)(v1));
  }
fg282dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg282dd_gg_function_18(float p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base) {
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
  v4 = fg282dd_gg_function_19(t1, base + 0);
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t4 = ((t2) > (t3));
  if (t4) {
  {
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t7 = ((int64_t)(((int64_t)fg282dd_gg_mod(t5, t6))));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t10 = ((uint64_t)(v3));
  t11 = ((int64_t)(v2));
  t12 = ((int64_t)(((int64_t)UINT64_C(32))));
  t13 = ((int64_t)(((int64_t)fg282dd_gg_div(t11, t12))));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t15 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t13) * (uint64_t)(t14)))))));
  t16 = ((uint64_t)(((uint64_t)((uint64_t)(t10) + (uint64_t)(t15)))));
  t17 = ((float)(v4));
#ifndef __cplusplus
  extern void (fg282d_sstore)(uint64_t, float);
#endif
  fg282d_sstore(t16, t17);
  }
  }
#ifndef __cplusplus
  extern void (fg282d_sync)(void);
#endif
  fg282d_sync();
  v5 = ((float)(((float)(fg282dd_gg_double_bits(UINT64_C(0))))));
  t18 = ((int64_t)(v2));
  t19 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t20 = ((int64_t)(((int64_t)fg282dd_gg_mod(t18, t19))));
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t23 = ((int64_t)(((int64_t)fg282dd_gg_div(t21, t22))));
  t24 = ((t20) < (t23));
  if (t24) {
  {
  t25 = ((uint64_t)(v3));
  t26 = ((int64_t)(v2));
  t27 = ((int64_t)(((int64_t)UINT64_C(32))));
  t28 = ((int64_t)(((int64_t)fg282dd_gg_mod(t26, t27))));
  t29 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t30 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))))));
  t31 = ((uint64_t)(((uint64_t)((uint64_t)(t25) + (uint64_t)(t30)))));
#ifndef __cplusplus
  extern float (fg282d_sload)(uint64_t);
#endif
  t32 = fg282d_sload(t31);
  v5 = ((float)(t32));
  }
  }
  t33 = ((float)(v5));
  v4 = fg282dd_gg_function_19(t33, base + 0);
  }
  }
  t0 = ((float)(v4));
  }
fg282dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg282dd_gg_function_13(float p0, int64_t base) {
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
  extern float (fg282d_shfl_xor)(float, int32_t);
#endif
  t5 = fg282d_shfl_xor(t3, t4);
  v3 = ((float)(t5));
  t6 = ((float)(v1));
  t7 = ((float)(v3));
#ifndef __cplusplus
  extern float (fg282d_fmaxf)(float, float);
#endif
  t8 = fg282d_fmaxf(t6, t7);
  v1 = ((float)(t8));
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  v2 = ((int64_t)(((int64_t)fg282dd_gg_div(t9, t10))));
  }
  goto label1;
label2:;
  t0 = ((float)(v1));
  }
fg282dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg282dd_gg_function_12(float p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base) {
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
  v4 = fg282dd_gg_function_13(t1, base + 0);
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t4 = ((t2) > (t3));
  if (t4) {
  {
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t7 = ((int64_t)(((int64_t)fg282dd_gg_mod(t5, t6))));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t10 = ((uint64_t)(v3));
  t11 = ((int64_t)(v2));
  t12 = ((int64_t)(((int64_t)UINT64_C(32))));
  t13 = ((int64_t)(((int64_t)fg282dd_gg_div(t11, t12))));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t15 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t13) * (uint64_t)(t14)))))));
  t16 = ((uint64_t)(((uint64_t)((uint64_t)(t10) + (uint64_t)(t15)))));
  t17 = ((float)(v4));
#ifndef __cplusplus
  extern void (fg282d_sstore)(uint64_t, float);
#endif
  fg282d_sstore(t16, t17);
  }
  }
#ifndef __cplusplus
  extern void (fg282d_sync)(void);
#endif
  fg282d_sync();
#ifndef __cplusplus
  extern float (fg282d_ninf)(void);
#endif
  t18 = fg282d_ninf();
  v5 = ((float)(t18));
  t19 = ((int64_t)(v2));
  t20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t21 = ((int64_t)(((int64_t)fg282dd_gg_mod(t19, t20))));
  t22 = ((int64_t)(v1));
  t23 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t24 = ((int64_t)(((int64_t)fg282dd_gg_div(t22, t23))));
  t25 = ((t21) < (t24));
  if (t25) {
  {
  t26 = ((uint64_t)(v3));
  t27 = ((int64_t)(v2));
  t28 = ((int64_t)(((int64_t)UINT64_C(32))));
  t29 = ((int64_t)(((int64_t)fg282dd_gg_mod(t27, t28))));
  t30 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t31 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t29) * (uint64_t)(t30)))))));
  t32 = ((uint64_t)(((uint64_t)((uint64_t)(t26) + (uint64_t)(t31)))));
#ifndef __cplusplus
  extern float (fg282d_sload)(uint64_t);
#endif
  t33 = fg282d_sload(t32);
  v5 = ((float)(t33));
  }
  }
  t34 = ((float)(v5));
  v4 = fg282dd_gg_function_13(t34, base + 0);
  }
  }
  t0 = ((float)(v4));
  }
fg282dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg282dd_gg_function_4(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t p4, float p5, float p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t base) {
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
  uint64_t t16;
  uint64_t t17;
  uint8_t t18;
  uint64_t t19;
  int64_t t20;
  int64_t t21;
  uint64_t t22;
  float t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  uint8_t t30;
  uint64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  uint64_t t36;
  float t37;
  int64_t t38;
  int64_t t39;
  uint8_t t40;
  uint64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  uint64_t t46;
  float t47;
  uint64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  uint64_t t53;
  float t54;
  float t55;
  float t56;
  float t57;
  float t58;
  float t59;
  float t60;
  int64_t t61;
  int64_t t62;
  uint8_t t63;
  uint64_t t64;
  int64_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  uint64_t t69;
  uint64_t t70;
  float t71;
  uint64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  uint64_t t77;
  float t78;
  float t79;
  float t80;
  float t81;
  int64_t t82;
  int64_t t83;
  float t84;
  int64_t t85;
  int64_t t86;
  uint64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  uint8_t t94;
  int64_t t95;
  int64_t t96;
  uint8_t t97;
  uint64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  uint64_t t103;
  uint64_t t104;
  float t105;
  uint64_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  uint64_t t111;
  float t112;
  float t113;
  float t114;
  float t115;
  float t116;
  float t117;
  float t118;
  int64_t t119;
  int64_t t120;
  uint8_t t121;
  uint64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  uint64_t t127;
  uint64_t t128;
  float t129;
  uint64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  uint64_t t135;
  float t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  int64_t t140;
  uint8_t t141;
  float t142;
  int64_t t143;
  int64_t t144;
  uint64_t t145;
  uint64_t t146;
  uint64_t t147;
  uint8_t t148;
  float t149;
  uint64_t t150;
  int64_t t151;
  int64_t t152;
  uint64_t t153;
  float t154;
  float t155;
  float t156;
  float t157;
  float t158;
  float t159;
  float t160;
  float t161;
  int64_t t162;
  int64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  uint8_t t168;
  int64_t t169;
  int64_t t170;
  uint8_t t171;
  uint64_t t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  uint64_t t177;
  uint64_t t178;
  float t179;
  uint64_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  uint64_t t185;
  float t186;
  uint64_t t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  int64_t t191;
  uint64_t t192;
  float t193;
  float t194;
  float t195;
  int64_t t196;
  int64_t t197;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  int64_t v4;
  float v5;
  float v6;
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
  int64_t v19;
  int64_t v20;
  uint64_t v21;
  int64_t v22;
  float v23;
  int64_t v24;
  int64_t v25;
  float v26;
  float v27;
  float v28;
  float v29;
  int64_t v30;
  float v31;
  float v32;
  float v33;
  int64_t v34;
  float v35;
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
  {
  t0 = ((int64_t)(v12));
  t1 = ((int64_t)(v13));
  t2 = ((int64_t)(v15));
  t3 = ((int64_t)(((int64_t)((uint64_t)(t1) * (uint64_t)(t2)))));
  t4 = ((int64_t)(((int64_t)((uint64_t)(t0) + (uint64_t)(t3)))));
  t5 = ((int64_t)(v14));
  t6 = ((int64_t)(v15));
  t7 = ((int64_t)(((int64_t)((uint64_t)(t5) * (uint64_t)(t6)))));
  t8 = ((int64_t)(v16));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) * (uint64_t)(t8)))));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t4) + (uint64_t)(t9)))));
  t10 = ((int64_t)(v18));
  t11 = ((int64_t)(v4));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  t12 = ((int64_t)(v18));
  t13 = ((int64_t)(v4));
  v20 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
#ifndef __cplusplus
  extern uint64_t (fg282d_smem)(void);
#endif
  t14 = fg282d_smem();
  v21 = ((uint64_t)(t14));
  v22 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
#ifndef __cplusplus
  extern float (fg282d_ninf)(void);
#endif
  t15 = fg282d_ninf();
  v23 = ((float)(t15));
  t16 = ((uint64_t)(v2));
  t17 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t18 = ((t16) != (t17));
  if (t18) {
  {
  t19 = ((uint64_t)(v2));
  t20 = ((int64_t)(v9));
  t21 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t22 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))))));
#ifndef __cplusplus
  extern float (fg282d_ld_f32)(uint64_t, uint64_t);
#endif
  t23 = fg282d_ld_f32(t19, t22);
  v23 = ((float)(t23));
  }
  }
  v24 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label1:;
  t24 = ((int64_t)(v24));
  t25 = ((int64_t)(v4));
  if (!(((t24) < (t25)))) goto label2;
  {
  t26 = ((int64_t)(v24));
  t27 = ((int64_t)(v11));
  v25 = ((int64_t)(((int64_t)((uint64_t)(t26) + (uint64_t)(t27)))));
  t28 = ((int64_t)(v25));
  t29 = ((int64_t)(((int64_t)(v4))));
  t30 = ((t28) < (t29));
  if (t30) {
  {
  t31 = ((uint64_t)(v0));
  t32 = ((int64_t)(v19));
  t33 = ((int64_t)(v25));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) + (uint64_t)(t33)))));
  t35 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t36 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t34) * (uint64_t)(t35)))))));
#ifndef __cplusplus
  extern float (fg282d_ld_f32)(uint64_t, uint64_t);
#endif
  t37 = fg282d_ld_f32(t31, t36);
  v26 = ((float)(t37));
  v27 = ((float)(((float)(fg282dd_gg_double_bits(UINT64_C(0))))));
  t38 = ((int64_t)(v8));
  t39 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t40 = ((t38) != (t39));
  if (t40) {
  {
  t41 = ((uint64_t)(v1));
  t42 = ((int64_t)(v7));
  t43 = ((int64_t)(v25));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) + (uint64_t)(t43)))));
  t45 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t46 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t44) * (uint64_t)(t45)))))));
#ifndef __cplusplus
  extern float (fg282d_ld_half)(uint64_t, uint64_t);
#endif
  t47 = fg282d_ld_half(t41, t46);
  v27 = ((float)(t47));
  }
  } else {
  {
  t48 = ((uint64_t)(v1));
  t49 = ((int64_t)(v7));
  t50 = ((int64_t)(v25));
  t51 = ((int64_t)(((int64_t)((uint64_t)(t49) + (uint64_t)(t50)))));
  t52 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t53 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t51) * (uint64_t)(t52)))))));
#ifndef __cplusplus
  extern float (fg282d_ld_f32)(uint64_t, uint64_t);
#endif
  t54 = fg282d_ld_f32(t48, t53);
  v27 = ((float)(t54));
  }
  }
  t55 = ((float)(v26));
  t56 = ((float)(v5));
  t57 = ((float)(((t55) * (t56))));
  t58 = ((float)(v6));
  t59 = ((float)(v27));
  t60 = ((float)(((t58) * (t59))));
  v28 = ((float)(((t57) + (t60))));
  t61 = ((int64_t)(v17));
  t62 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t63 = ((t61) != (t62));
  if (t63) {
  {
  t64 = ((uint64_t)(v21));
  t65 = ((int64_t)(v22));
  t66 = ((int64_t)(v25));
  t67 = ((int64_t)(((int64_t)((uint64_t)(t65) + (uint64_t)(t66)))));
  t68 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t69 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t67) * (uint64_t)(t68)))))));
  t70 = ((uint64_t)(((uint64_t)((uint64_t)(t64) + (uint64_t)(t69)))));
  t71 = ((float)(v28));
#ifndef __cplusplus
  extern void (fg282d_sstore)(uint64_t, float);
#endif
  fg282d_sstore(t70, t71);
  }
  } else {
  {
  t72 = ((uint64_t)(v3));
  t73 = ((int64_t)(v20));
  t74 = ((int64_t)(v25));
  t75 = ((int64_t)(((int64_t)((uint64_t)(t73) + (uint64_t)(t74)))));
  t76 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t77 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t75) * (uint64_t)(t76)))))));
  t78 = ((float)(v28));
#ifndef __cplusplus
  extern void (fg282d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg282d_st_f32(t72, t77, t78);
  }
  }
  t79 = ((float)(v23));
  t80 = ((float)(v28));
#ifndef __cplusplus
  extern float (fg282d_fmaxf)(float, float);
#endif
  t81 = fg282d_fmaxf(t79, t80);
  v23 = ((float)(t81));
  }
  }
  t82 = ((int64_t)(v24));
  t83 = ((int64_t)(v10));
  v24 = ((int64_t)(((int64_t)((uint64_t)(t82) + (uint64_t)(t83)))));
  }
  goto label1;
label2:;
  t84 = ((float)(v23));
  t85 = ((int64_t)(v10));
  t86 = ((int64_t)(v11));
  t87 = ((uint64_t)(v21));
  v23 = fg282dd_gg_function_12(t84, t85, t86, t87, base + 0);
  v29 = ((float)(((float)(fg282dd_gg_double_bits(UINT64_C(0))))));
  v24 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label4:;
  t88 = ((int64_t)(v24));
  t89 = ((int64_t)(v4));
  if (!(((t88) < (t89)))) goto label5;
  {
  t90 = ((int64_t)(v24));
  t91 = ((int64_t)(v11));
  v30 = ((int64_t)(((int64_t)((uint64_t)(t90) + (uint64_t)(t91)))));
  t92 = ((int64_t)(v30));
  t93 = ((int64_t)(((int64_t)(v4))));
  t94 = ((t92) < (t93));
  if (t94) {
  {
  v31 = ((float)(((float)(fg282dd_gg_double_bits(UINT64_C(0))))));
  t95 = ((int64_t)(v17));
  t96 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t97 = ((t95) != (t96));
  if (t97) {
  {
  t98 = ((uint64_t)(v21));
  t99 = ((int64_t)(v22));
  t100 = ((int64_t)(v30));
  t101 = ((int64_t)(((int64_t)((uint64_t)(t99) + (uint64_t)(t100)))));
  t102 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t103 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t101) * (uint64_t)(t102)))))));
  t104 = ((uint64_t)(((uint64_t)((uint64_t)(t98) + (uint64_t)(t103)))));
#ifndef __cplusplus
  extern float (fg282d_sload)(uint64_t);
#endif
  t105 = fg282d_sload(t104);
  v31 = ((float)(t105));
  }
  } else {
  {
  t106 = ((uint64_t)(v3));
  t107 = ((int64_t)(v20));
  t108 = ((int64_t)(v30));
  t109 = ((int64_t)(((int64_t)((uint64_t)(t107) + (uint64_t)(t108)))));
  t110 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t111 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t109) * (uint64_t)(t110)))))));
#ifndef __cplusplus
  extern float (fg282d_ld_f32)(uint64_t, uint64_t);
#endif
  t112 = fg282d_ld_f32(t106, t111);
  v31 = ((float)(t112));
  }
  }
  t113 = ((float)(v31));
  t114 = ((float)(v23));
  t115 = ((float)(((t113) - (t114))));
#ifndef __cplusplus
  extern float (fg282d_expf)(float);
#endif
  t116 = fg282d_expf(t115);
  v32 = ((float)(t116));
  t117 = ((float)(v29));
  t118 = ((float)(v32));
  v29 = ((float)(((t117) + (t118))));
  t119 = ((int64_t)(v17));
  t120 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t121 = ((t119) != (t120));
  if (t121) {
  {
  t122 = ((uint64_t)(v21));
  t123 = ((int64_t)(v22));
  t124 = ((int64_t)(v30));
  t125 = ((int64_t)(((int64_t)((uint64_t)(t123) + (uint64_t)(t124)))));
  t126 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t127 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t125) * (uint64_t)(t126)))))));
  t128 = ((uint64_t)(((uint64_t)((uint64_t)(t122) + (uint64_t)(t127)))));
  t129 = ((float)(v32));
#ifndef __cplusplus
  extern void (fg282d_sstore)(uint64_t, float);
#endif
  fg282d_sstore(t128, t129);
  }
  } else {
  {
  t130 = ((uint64_t)(v3));
  t131 = ((int64_t)(v20));
  t132 = ((int64_t)(v30));
  t133 = ((int64_t)(((int64_t)((uint64_t)(t131) + (uint64_t)(t132)))));
  t134 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t135 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t133) * (uint64_t)(t134)))))));
  t136 = ((float)(v32));
#ifndef __cplusplus
  extern void (fg282d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg282d_st_f32(t130, t135, t136);
  }
  }
  }
  }
  t137 = ((int64_t)(v24));
  t138 = ((int64_t)(v10));
  v24 = ((int64_t)(((int64_t)((uint64_t)(t137) + (uint64_t)(t138)))));
  }
  goto label4;
label5:;
  t139 = ((int64_t)(v10));
  t140 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t141 = ((t139) > (t140));
  if (t141) {
  {
#ifndef __cplusplus
  extern void (fg282d_sync)(void);
#endif
  fg282d_sync();
  }
  }
  t142 = ((float)(v29));
  t143 = ((int64_t)(v10));
  t144 = ((int64_t)(v11));
  t145 = ((uint64_t)(v21));
  v29 = fg282dd_gg_function_18(t142, t143, t144, t145, base + 0);
  t146 = ((uint64_t)(v2));
  t147 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t148 = ((t146) != (t147));
  if (t148) {
  {
  t149 = ((float)(v29));
  t150 = ((uint64_t)(v2));
  t151 = ((int64_t)(v9));
  t152 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t153 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t151) * (uint64_t)(t152)))))));
#ifndef __cplusplus
  extern float (fg282d_ld_f32)(uint64_t, uint64_t);
#endif
  t154 = fg282d_ld_f32(t150, t153);
  t155 = ((float)(t154));
  t156 = ((float)(v23));
  t157 = ((float)(((t155) - (t156))));
#ifndef __cplusplus
  extern float (fg282d_expf)(float);
#endif
  t158 = fg282d_expf(t157);
  t159 = ((float)(t158));
  v29 = ((float)(((t149) + (t159))));
  }
  }
  t160 = ((float)(((float)(fg282dd_gg_double_bits(UINT64_C(4607182418800017408))))));
  t161 = ((float)(v29));
  v33 = ((float)(((t160) / (t161))));
  v24 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label7:;
  t162 = ((int64_t)(v24));
  t163 = ((int64_t)(v4));
  if (!(((t162) < (t163)))) goto label8;
  {
  t164 = ((int64_t)(v24));
  t165 = ((int64_t)(v11));
  v34 = ((int64_t)(((int64_t)((uint64_t)(t164) + (uint64_t)(t165)))));
  t166 = ((int64_t)(v34));
  t167 = ((int64_t)(((int64_t)(v4))));
  t168 = ((t166) < (t167));
  if (t168) {
  {
  v35 = ((float)(((float)(fg282dd_gg_double_bits(UINT64_C(0))))));
  t169 = ((int64_t)(v17));
  t170 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t171 = ((t169) != (t170));
  if (t171) {
  {
  t172 = ((uint64_t)(v21));
  t173 = ((int64_t)(v22));
  t174 = ((int64_t)(v34));
  t175 = ((int64_t)(((int64_t)((uint64_t)(t173) + (uint64_t)(t174)))));
  t176 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t177 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t175) * (uint64_t)(t176)))))));
  t178 = ((uint64_t)(((uint64_t)((uint64_t)(t172) + (uint64_t)(t177)))));
#ifndef __cplusplus
  extern float (fg282d_sload)(uint64_t);
#endif
  t179 = fg282d_sload(t178);
  v35 = ((float)(t179));
  }
  } else {
  {
  t180 = ((uint64_t)(v3));
  t181 = ((int64_t)(v20));
  t182 = ((int64_t)(v34));
  t183 = ((int64_t)(((int64_t)((uint64_t)(t181) + (uint64_t)(t182)))));
  t184 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t185 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t183) * (uint64_t)(t184)))))));
#ifndef __cplusplus
  extern float (fg282d_ld_f32)(uint64_t, uint64_t);
#endif
  t186 = fg282d_ld_f32(t180, t185);
  v35 = ((float)(t186));
  }
  }
  t187 = ((uint64_t)(v3));
  t188 = ((int64_t)(v20));
  t189 = ((int64_t)(v34));
  t190 = ((int64_t)(((int64_t)((uint64_t)(t188) + (uint64_t)(t189)))));
  t191 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t192 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t190) * (uint64_t)(t191)))))));
  t193 = ((float)(v35));
  t194 = ((float)(v33));
  t195 = ((float)(((t193) * (t194))));
#ifndef __cplusplus
  extern void (fg282d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg282d_st_f32(t187, t192, t195);
  }
  }
  t196 = ((int64_t)(v24));
  t197 = ((int64_t)(v10));
  v24 = ((int64_t)(((int64_t)((uint64_t)(t196) + (uint64_t)(t197)))));
  }
  goto label7;
label8:;
  }
fg282dd_gg_done:;
}
static __device__ __forceinline__ float fg282dd_gg_function_2(float p0, int64_t p1, int64_t p2, float p3, float p4, int64_t base) {
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
  t2 = ((float)(((float)(fg282dd_gg_double_bits(UINT64_C(0))))));
  t3 = ((t1) <= (t2));
  if (t3) {
  {
  t0 = ((float)(((float)(fg282dd_gg_double_bits(UINT64_C(4607182418800017408))))));
  goto fg282dd_gg_done;
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
  extern float (fg282d_powf)(float, float);
#endif
  t20 = fg282d_powf(t18, t19);
  t0 = ((float)(t20));
  }
fg282dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg282dd_gg_function_20(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t p4, float p5, float p6, int64_t p7, float p8, float p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, int64_t p19, int64_t p20, int64_t p21, int64_t p22, int64_t base) {
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
  uint64_t t24;
  int64_t t25;
  float t26;
  float t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  int64_t v4;
  float v5;
  float v6;
  int64_t v7;
  float v8;
  float v9;
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
  int64_t v25;
  float v26;
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
  {
  t0 = ((int64_t)(v18));
  t1 = ((int64_t)(v10));
  v23 = ((int64_t)(((int64_t)fg282dd_gg_mod(t0, t1))));
  t2 = ((int64_t)(v19));
  t3 = ((int64_t)(v11));
  v24 = ((int64_t)(((int64_t)fg282dd_gg_mod(t2, t3))));
  t4 = ((int64_t)(v17));
  t5 = ((int64_t)(v12));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  t7 = ((int64_t)(v23));
  t8 = ((int64_t)(v13));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) * (uint64_t)(t8)))));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t6) + (uint64_t)(t9)))));
  t11 = ((int64_t)(v24));
  t12 = ((int64_t)(v14));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t13)))));
  t15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  v25 = ((int64_t)(((int64_t)fg282dd_gg_div(t14, t15))));
  t16 = ((float)(v6));
  t17 = ((int64_t)(v18));
  t18 = ((int64_t)(v7));
  t19 = ((float)(v8));
  t20 = ((float)(v9));
  v26 = fg282dd_gg_function_2(t16, t17, t18, t19, t20, base + 0);
  t21 = ((uint64_t)(v0));
  t22 = ((uint64_t)(v1));
  t23 = ((uint64_t)(v2));
  t24 = ((uint64_t)(v3));
  t25 = ((int64_t)(v4));
  t26 = ((float)(v5));
  t27 = ((float)(v26));
  t28 = ((int64_t)(v25));
  t29 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t30 = ((int64_t)(v18));
  t31 = ((int64_t)(v15));
  t32 = ((int64_t)(v16));
  t33 = ((int64_t)(v17));
  t34 = ((int64_t)(v18));
  t35 = ((int64_t)(v19));
  t36 = ((int64_t)(v20));
  t37 = ((int64_t)(v21));
  t38 = ((int64_t)(v22));
  fg282dd_gg_function_4(t21, t22, t23, t24, t25, t26, t27, t28, t29, t30, t31, t32, t33, t34, t35, t36, t37, t38, base + 0);
  }
fg282dd_gg_done:;
}
static __device__ __forceinline__ void fg282dd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t p4, float p5, float p6, int64_t p7, float p8, float p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, int64_t p19, int64_t p20, int64_t p21, int64_t p22, int64_t base) {
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
  uint64_t t24;
  int64_t t25;
  float t26;
  float t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  int64_t v4;
  float v5;
  float v6;
  int64_t v7;
  float v8;
  float v9;
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
  int64_t v25;
  float v26;
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
  {
  t0 = ((int64_t)(v18));
  t1 = ((int64_t)(v10));
  v23 = ((int64_t)(((int64_t)fg282dd_gg_mod(t0, t1))));
  t2 = ((int64_t)(v19));
  t3 = ((int64_t)(v11));
  v24 = ((int64_t)(((int64_t)fg282dd_gg_mod(t2, t3))));
  t4 = ((int64_t)(v17));
  t5 = ((int64_t)(v12));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  t7 = ((int64_t)(v23));
  t8 = ((int64_t)(v13));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) * (uint64_t)(t8)))));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t6) + (uint64_t)(t9)))));
  t11 = ((int64_t)(v24));
  t12 = ((int64_t)(v14));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t13)))));
  t15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v25 = ((int64_t)(((int64_t)fg282dd_gg_div(t14, t15))));
  t16 = ((float)(v6));
  t17 = ((int64_t)(v18));
  t18 = ((int64_t)(v7));
  t19 = ((float)(v8));
  t20 = ((float)(v9));
  v26 = fg282dd_gg_function_2(t16, t17, t18, t19, t20, base + 0);
  t21 = ((uint64_t)(v0));
  t22 = ((uint64_t)(v1));
  t23 = ((uint64_t)(v2));
  t24 = ((uint64_t)(v3));
  t25 = ((int64_t)(v4));
  t26 = ((float)(v5));
  t27 = ((float)(v26));
  t28 = ((int64_t)(v25));
  t29 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t30 = ((int64_t)(v18));
  t31 = ((int64_t)(v15));
  t32 = ((int64_t)(v16));
  t33 = ((int64_t)(v17));
  t34 = ((int64_t)(v18));
  t35 = ((int64_t)(v19));
  t36 = ((int64_t)(v20));
  t37 = ((int64_t)(v21));
  t38 = ((int64_t)(v22));
  fg282dd_gg_function_4(t21, t22, t23, t24, t25, t26, t27, t28, t29, t30, t31, t32, t33, t34, t35, t36, t37, t38, base + 0);
  }
fg282dd_gg_done:;
}
