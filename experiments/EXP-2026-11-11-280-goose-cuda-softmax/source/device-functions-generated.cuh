#pragma once
static __device__ __forceinline__ void fg280dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg280dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg280dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg280dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg280dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg280dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ float fg280dd_gg_function_19(float p0, int64_t base);
static __device__ __forceinline__ float fg280dd_gg_function_18(float p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base);
static __device__ __forceinline__ float fg280dd_gg_function_13(float p0, int64_t base);
static __device__ __forceinline__ float fg280dd_gg_function_12(float p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base);
static __device__ __forceinline__ float fg280dd_gg_function_5(float p0, int64_t p1, int64_t p2, float p3, float p4, int64_t base);
static __device__ __forceinline__ void fg280dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg280dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, float p3, float p4, int64_t p5, float p6, float p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t base);
static __device__ __forceinline__ void fg280dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg280dd_gg_div(int64_t a, int64_t b) { if (!b) fg280dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg280dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg280dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg280dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg280dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg280dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg280dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg280dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg280dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ float fg280dd_gg_function_19(float p0, int64_t base) {
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
  extern float (fg280d_shfl_xor)(float, int32_t);
#endif
  t5 = fg280d_shfl_xor(t3, t4);
  v3 = ((float)(t5));
  t6 = ((float)(v1));
  t7 = ((float)(v3));
  v1 = ((float)(((t6) + (t7))));
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  v2 = ((int64_t)(((int64_t)fg280dd_gg_div(t8, t9))));
  }
  goto label1;
label2:;
  t0 = ((float)(v1));
  }
fg280dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg280dd_gg_function_18(float p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base) {
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
  v4 = fg280dd_gg_function_19(t1, base + 0);
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t4 = ((t2) > (t3));
  if (t4) {
  {
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t7 = ((int64_t)(((int64_t)fg280dd_gg_mod(t5, t6))));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t10 = ((uint64_t)(v3));
  t11 = ((int64_t)(v2));
  t12 = ((int64_t)(((int64_t)UINT64_C(32))));
  t13 = ((int64_t)(((int64_t)fg280dd_gg_div(t11, t12))));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t15 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t13) * (uint64_t)(t14)))))));
  t16 = ((uint64_t)(((uint64_t)((uint64_t)(t10) + (uint64_t)(t15)))));
  t17 = ((float)(v4));
#ifndef __cplusplus
  extern void (fg280d_sstore)(uint64_t, float);
#endif
  fg280d_sstore(t16, t17);
  }
  }
#ifndef __cplusplus
  extern void (fg280d_sync)(void);
#endif
  fg280d_sync();
  v5 = ((float)(((float)(fg280dd_gg_double_bits(UINT64_C(0))))));
  t18 = ((int64_t)(v2));
  t19 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t20 = ((int64_t)(((int64_t)fg280dd_gg_mod(t18, t19))));
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t23 = ((int64_t)(((int64_t)fg280dd_gg_div(t21, t22))));
  t24 = ((t20) < (t23));
  if (t24) {
  {
  t25 = ((uint64_t)(v3));
  t26 = ((int64_t)(v2));
  t27 = ((int64_t)(((int64_t)UINT64_C(32))));
  t28 = ((int64_t)(((int64_t)fg280dd_gg_mod(t26, t27))));
  t29 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t30 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))))));
  t31 = ((uint64_t)(((uint64_t)((uint64_t)(t25) + (uint64_t)(t30)))));
#ifndef __cplusplus
  extern float (fg280d_sload)(uint64_t);
#endif
  t32 = fg280d_sload(t31);
  v5 = ((float)(t32));
  }
  }
  t33 = ((float)(v5));
  v4 = fg280dd_gg_function_19(t33, base + 0);
  }
  }
  t0 = ((float)(v4));
  }
fg280dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg280dd_gg_function_13(float p0, int64_t base) {
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
  extern float (fg280d_shfl_xor)(float, int32_t);
#endif
  t5 = fg280d_shfl_xor(t3, t4);
  v3 = ((float)(t5));
  t6 = ((float)(v1));
  t7 = ((float)(v3));
#ifndef __cplusplus
  extern float (fg280d_fmaxf)(float, float);
#endif
  t8 = fg280d_fmaxf(t6, t7);
  v1 = ((float)(t8));
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  v2 = ((int64_t)(((int64_t)fg280dd_gg_div(t9, t10))));
  }
  goto label1;
label2:;
  t0 = ((float)(v1));
  }
fg280dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg280dd_gg_function_12(float p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base) {
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
  v4 = fg280dd_gg_function_13(t1, base + 0);
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t4 = ((t2) > (t3));
  if (t4) {
  {
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t7 = ((int64_t)(((int64_t)fg280dd_gg_mod(t5, t6))));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t10 = ((uint64_t)(v3));
  t11 = ((int64_t)(v2));
  t12 = ((int64_t)(((int64_t)UINT64_C(32))));
  t13 = ((int64_t)(((int64_t)fg280dd_gg_div(t11, t12))));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t15 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t13) * (uint64_t)(t14)))))));
  t16 = ((uint64_t)(((uint64_t)((uint64_t)(t10) + (uint64_t)(t15)))));
  t17 = ((float)(v4));
#ifndef __cplusplus
  extern void (fg280d_sstore)(uint64_t, float);
#endif
  fg280d_sstore(t16, t17);
  }
  }
#ifndef __cplusplus
  extern void (fg280d_sync)(void);
#endif
  fg280d_sync();
#ifndef __cplusplus
  extern float (fg280d_ninf)(void);
#endif
  t18 = fg280d_ninf();
  v5 = ((float)(t18));
  t19 = ((int64_t)(v2));
  t20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t21 = ((int64_t)(((int64_t)fg280dd_gg_mod(t19, t20))));
  t22 = ((int64_t)(v1));
  t23 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t24 = ((int64_t)(((int64_t)fg280dd_gg_div(t22, t23))));
  t25 = ((t21) < (t24));
  if (t25) {
  {
  t26 = ((uint64_t)(v3));
  t27 = ((int64_t)(v2));
  t28 = ((int64_t)(((int64_t)UINT64_C(32))));
  t29 = ((int64_t)(((int64_t)fg280dd_gg_mod(t27, t28))));
  t30 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t31 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t29) * (uint64_t)(t30)))))));
  t32 = ((uint64_t)(((uint64_t)((uint64_t)(t26) + (uint64_t)(t31)))));
#ifndef __cplusplus
  extern float (fg280d_sload)(uint64_t);
#endif
  t33 = fg280d_sload(t32);
  v5 = ((float)(t33));
  }
  }
  t34 = ((float)(v5));
  v4 = fg280dd_gg_function_13(t34, base + 0);
  }
  }
  t0 = ((float)(v4));
  }
fg280dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg280dd_gg_function_5(float p0, int64_t p1, int64_t p2, float p3, float p4, int64_t base) {
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
  t2 = ((float)(((float)(fg280dd_gg_double_bits(UINT64_C(0))))));
  t3 = ((t1) <= (t2));
  if (t3) {
  {
  t0 = ((float)(((float)(fg280dd_gg_double_bits(UINT64_C(4607182418800017408))))));
  goto fg280dd_gg_done;
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
  extern float (fg280d_powf)(float, float);
#endif
  t20 = fg280d_powf(t18, t19);
  t0 = ((float)(t20));
  }
fg280dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg280dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg280d_fail)(int64_t);
#endif
  fg280d_fail(t2);
  }
  }
  }
fg280dd_gg_done:;
}
static __device__ __forceinline__ void fg280dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, float p3, float p4, int64_t p5, float p6, float p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t base) {
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
  uint64_t t22;
  float t23;
  int64_t t24;
  int64_t t25;
  float t26;
  float t27;
  float t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  uint8_t t35;
  uint64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  uint64_t t41;
  float t42;
  float t43;
  float t44;
  float t45;
  float t46;
  int64_t t47;
  int64_t t48;
  uint8_t t49;
  uint64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  uint64_t t55;
  uint64_t t56;
  float t57;
  uint64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  uint64_t t63;
  float t64;
  float t65;
  float t66;
  float t67;
  int64_t t68;
  int64_t t69;
  float t70;
  int64_t t71;
  int64_t t72;
  uint64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  uint8_t t80;
  int64_t t81;
  int64_t t82;
  uint8_t t83;
  uint64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  uint64_t t89;
  uint64_t t90;
  float t91;
  uint64_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  uint64_t t97;
  float t98;
  float t99;
  float t100;
  float t101;
  float t102;
  float t103;
  float t104;
  int64_t t105;
  int64_t t106;
  uint8_t t107;
  uint64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  uint64_t t113;
  uint64_t t114;
  float t115;
  uint64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  uint64_t t121;
  float t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  uint8_t t127;
  float t128;
  int64_t t129;
  int64_t t130;
  uint64_t t131;
  float t132;
  float t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  uint8_t t140;
  int64_t t141;
  int64_t t142;
  uint8_t t143;
  uint64_t t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  uint64_t t149;
  uint64_t t150;
  float t151;
  uint64_t t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  uint64_t t157;
  float t158;
  uint64_t t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  int64_t t163;
  uint64_t t164;
  float t165;
  float t166;
  float t167;
  int64_t t168;
  int64_t t169;
  uint64_t v0;
  uint64_t v1;
  int64_t v2;
  float v3;
  float v4;
  int64_t v5;
  float v6;
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
  int64_t v17;
  int64_t v18;
  uint64_t v19;
  int64_t v20;
  float v21;
  float v22;
  int64_t v23;
  int64_t v24;
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
  t0 = ((int64_t)(v2));
  t1 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t2 = ((t0) > (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v8));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t4) > (t5));
  }
  t6 = t3;
  t7 = ((int64_t)(((int64_t)UINT64_C(20))));
  fg280dd_gg_function_2(t6, t7, base + 0);
  t8 = ((int64_t)(v10));
  t9 = ((int64_t)(v11));
  t10 = ((int64_t)(v13));
  t11 = ((int64_t)(((int64_t)((uint64_t)(t9) * (uint64_t)(t10)))));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t8) + (uint64_t)(t11)))));
  t13 = ((int64_t)(v12));
  t14 = ((int64_t)(v13));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t13) * (uint64_t)(t14)))));
  t16 = ((int64_t)(v14));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t15) * (uint64_t)(t16)))));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t12) + (uint64_t)(t17)))));
  t18 = ((int64_t)(v16));
  t19 = ((int64_t)(v2));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))));
  t20 = ((int64_t)(v16));
  t21 = ((int64_t)(v2));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
#ifndef __cplusplus
  extern uint64_t (fg280d_smem)(void);
#endif
  t22 = fg280d_smem();
  v19 = ((uint64_t)(t22));
  v20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t23 = ((float)(v4));
  t24 = ((int64_t)(v11));
  t25 = ((int64_t)(v5));
  t26 = ((float)(v6));
  t27 = ((float)(v7));
  v21 = fg280dd_gg_function_5(t23, t24, t25, t26, t27, base + 0);
#ifndef __cplusplus
  extern float (fg280d_ninf)(void);
#endif
  t28 = fg280d_ninf();
  v22 = ((float)(t28));
  v23 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label1:;
  t29 = ((int64_t)(v23));
  t30 = ((int64_t)(v2));
  if (!(((t29) < (t30)))) goto label2;
  {
  t31 = ((int64_t)(v23));
  t32 = ((int64_t)(v9));
  v24 = ((int64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t32)))));
  t33 = ((int64_t)(v24));
  t34 = ((int64_t)(((int64_t)(v2))));
  t35 = ((t33) < (t34));
  if (t35) {
  {
  t36 = ((uint64_t)(v0));
  t37 = ((int64_t)(v17));
  t38 = ((int64_t)(v24));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t37) + (uint64_t)(t38)))));
  t40 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t41 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t39) * (uint64_t)(t40)))))));
#ifndef __cplusplus
  extern float (fg280d_ld_f32)(uint64_t, uint64_t);
#endif
  t42 = fg280d_ld_f32(t36, t41);
  v25 = ((float)(t42));
  t43 = ((float)(v25));
  t44 = ((float)(v3));
  t45 = ((float)(((t43) * (t44))));
  t46 = ((float)(((float)(fg280dd_gg_double_bits(UINT64_C(0))))));
  v26 = ((float)(((t45) + (t46))));
  t47 = ((int64_t)(v15));
  t48 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t49 = ((t47) != (t48));
  if (t49) {
  {
  t50 = ((uint64_t)(v19));
  t51 = ((int64_t)(v20));
  t52 = ((int64_t)(v24));
  t53 = ((int64_t)(((int64_t)((uint64_t)(t51) + (uint64_t)(t52)))));
  t54 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t55 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t53) * (uint64_t)(t54)))))));
  t56 = ((uint64_t)(((uint64_t)((uint64_t)(t50) + (uint64_t)(t55)))));
  t57 = ((float)(v26));
#ifndef __cplusplus
  extern void (fg280d_sstore)(uint64_t, float);
#endif
  fg280d_sstore(t56, t57);
  }
  } else {
  {
  t58 = ((uint64_t)(v1));
  t59 = ((int64_t)(v18));
  t60 = ((int64_t)(v24));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t59) + (uint64_t)(t60)))));
  t62 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t63 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t61) * (uint64_t)(t62)))))));
  t64 = ((float)(v26));
#ifndef __cplusplus
  extern void (fg280d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg280d_st_f32(t58, t63, t64);
  }
  }
  t65 = ((float)(v22));
  t66 = ((float)(v26));
#ifndef __cplusplus
  extern float (fg280d_fmaxf)(float, float);
#endif
  t67 = fg280d_fmaxf(t65, t66);
  v22 = ((float)(t67));
  }
  }
  t68 = ((int64_t)(v23));
  t69 = ((int64_t)(v8));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t68) + (uint64_t)(t69)))));
  }
  goto label1;
label2:;
  t70 = ((float)(v22));
  t71 = ((int64_t)(v8));
  t72 = ((int64_t)(v9));
  t73 = ((uint64_t)(v19));
  v22 = fg280dd_gg_function_12(t70, t71, t72, t73, base + 0);
  v27 = ((float)(((float)(fg280dd_gg_double_bits(UINT64_C(0))))));
  v23 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label4:;
  t74 = ((int64_t)(v23));
  t75 = ((int64_t)(v2));
  if (!(((t74) < (t75)))) goto label5;
  {
  t76 = ((int64_t)(v23));
  t77 = ((int64_t)(v9));
  v28 = ((int64_t)(((int64_t)((uint64_t)(t76) + (uint64_t)(t77)))));
  t78 = ((int64_t)(v28));
  t79 = ((int64_t)(((int64_t)(v2))));
  t80 = ((t78) < (t79));
  if (t80) {
  {
  v29 = ((float)(((float)(fg280dd_gg_double_bits(UINT64_C(0))))));
  t81 = ((int64_t)(v15));
  t82 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t83 = ((t81) != (t82));
  if (t83) {
  {
  t84 = ((uint64_t)(v19));
  t85 = ((int64_t)(v20));
  t86 = ((int64_t)(v28));
  t87 = ((int64_t)(((int64_t)((uint64_t)(t85) + (uint64_t)(t86)))));
  t88 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t89 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t87) * (uint64_t)(t88)))))));
  t90 = ((uint64_t)(((uint64_t)((uint64_t)(t84) + (uint64_t)(t89)))));
#ifndef __cplusplus
  extern float (fg280d_sload)(uint64_t);
#endif
  t91 = fg280d_sload(t90);
  v29 = ((float)(t91));
  }
  } else {
  {
  t92 = ((uint64_t)(v1));
  t93 = ((int64_t)(v18));
  t94 = ((int64_t)(v28));
  t95 = ((int64_t)(((int64_t)((uint64_t)(t93) + (uint64_t)(t94)))));
  t96 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t97 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t95) * (uint64_t)(t96)))))));
#ifndef __cplusplus
  extern float (fg280d_ld_f32)(uint64_t, uint64_t);
#endif
  t98 = fg280d_ld_f32(t92, t97);
  v29 = ((float)(t98));
  }
  }
  t99 = ((float)(v29));
  t100 = ((float)(v22));
  t101 = ((float)(((t99) - (t100))));
#ifndef __cplusplus
  extern float (fg280d_expf)(float);
#endif
  t102 = fg280d_expf(t101);
  v30 = ((float)(t102));
  t103 = ((float)(v27));
  t104 = ((float)(v30));
  v27 = ((float)(((t103) + (t104))));
  t105 = ((int64_t)(v15));
  t106 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t107 = ((t105) != (t106));
  if (t107) {
  {
  t108 = ((uint64_t)(v19));
  t109 = ((int64_t)(v20));
  t110 = ((int64_t)(v28));
  t111 = ((int64_t)(((int64_t)((uint64_t)(t109) + (uint64_t)(t110)))));
  t112 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t113 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t111) * (uint64_t)(t112)))))));
  t114 = ((uint64_t)(((uint64_t)((uint64_t)(t108) + (uint64_t)(t113)))));
  t115 = ((float)(v30));
#ifndef __cplusplus
  extern void (fg280d_sstore)(uint64_t, float);
#endif
  fg280d_sstore(t114, t115);
  }
  } else {
  {
  t116 = ((uint64_t)(v1));
  t117 = ((int64_t)(v18));
  t118 = ((int64_t)(v28));
  t119 = ((int64_t)(((int64_t)((uint64_t)(t117) + (uint64_t)(t118)))));
  t120 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t121 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t119) * (uint64_t)(t120)))))));
  t122 = ((float)(v30));
#ifndef __cplusplus
  extern void (fg280d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg280d_st_f32(t116, t121, t122);
  }
  }
  }
  }
  t123 = ((int64_t)(v23));
  t124 = ((int64_t)(v8));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t123) + (uint64_t)(t124)))));
  }
  goto label4;
label5:;
  t125 = ((int64_t)(v8));
  t126 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t127 = ((t125) > (t126));
  if (t127) {
  {
#ifndef __cplusplus
  extern void (fg280d_sync)(void);
#endif
  fg280d_sync();
  }
  }
  t128 = ((float)(v27));
  t129 = ((int64_t)(v8));
  t130 = ((int64_t)(v9));
  t131 = ((uint64_t)(v19));
  v27 = fg280dd_gg_function_18(t128, t129, t130, t131, base + 0);
  t132 = ((float)(((float)(fg280dd_gg_double_bits(UINT64_C(4607182418800017408))))));
  t133 = ((float)(v27));
  v31 = ((float)(((t132) / (t133))));
  v23 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label7:;
  t134 = ((int64_t)(v23));
  t135 = ((int64_t)(v2));
  if (!(((t134) < (t135)))) goto label8;
  {
  t136 = ((int64_t)(v23));
  t137 = ((int64_t)(v9));
  v32 = ((int64_t)(((int64_t)((uint64_t)(t136) + (uint64_t)(t137)))));
  t138 = ((int64_t)(v32));
  t139 = ((int64_t)(((int64_t)(v2))));
  t140 = ((t138) < (t139));
  if (t140) {
  {
  v33 = ((float)(((float)(fg280dd_gg_double_bits(UINT64_C(0))))));
  t141 = ((int64_t)(v15));
  t142 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t143 = ((t141) != (t142));
  if (t143) {
  {
  t144 = ((uint64_t)(v19));
  t145 = ((int64_t)(v20));
  t146 = ((int64_t)(v32));
  t147 = ((int64_t)(((int64_t)((uint64_t)(t145) + (uint64_t)(t146)))));
  t148 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t149 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t147) * (uint64_t)(t148)))))));
  t150 = ((uint64_t)(((uint64_t)((uint64_t)(t144) + (uint64_t)(t149)))));
#ifndef __cplusplus
  extern float (fg280d_sload)(uint64_t);
#endif
  t151 = fg280d_sload(t150);
  v33 = ((float)(t151));
  }
  } else {
  {
  t152 = ((uint64_t)(v1));
  t153 = ((int64_t)(v18));
  t154 = ((int64_t)(v32));
  t155 = ((int64_t)(((int64_t)((uint64_t)(t153) + (uint64_t)(t154)))));
  t156 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t157 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t155) * (uint64_t)(t156)))))));
#ifndef __cplusplus
  extern float (fg280d_ld_f32)(uint64_t, uint64_t);
#endif
  t158 = fg280d_ld_f32(t152, t157);
  v33 = ((float)(t158));
  }
  }
  t159 = ((uint64_t)(v1));
  t160 = ((int64_t)(v18));
  t161 = ((int64_t)(v32));
  t162 = ((int64_t)(((int64_t)((uint64_t)(t160) + (uint64_t)(t161)))));
  t163 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t164 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t162) * (uint64_t)(t163)))))));
  t165 = ((float)(v33));
  t166 = ((float)(v31));
  t167 = ((float)(((t165) * (t166))));
#ifndef __cplusplus
  extern void (fg280d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg280d_st_f32(t159, t164, t167);
  }
  }
  t168 = ((int64_t)(v23));
  t169 = ((int64_t)(v8));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t168) + (uint64_t)(t169)))));
  }
  goto label7;
label8:;
  }
fg280dd_gg_done:;
}
