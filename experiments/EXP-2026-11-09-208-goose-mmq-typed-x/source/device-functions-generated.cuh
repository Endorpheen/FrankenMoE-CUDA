#pragma once
#include "device-types-generated.h"
static __device__ __forceinline__ void fg208qd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg208qd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg208qd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg208qd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg208qd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg208qd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ void fg208qd_gg_function_3(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ int64_t fg208qd_gg_function_5(uint64_t p0, uint64_t p1, int64_t base);
static __device__ __forceinline__ float fg208qd_gg_function_15(float p0, int32_t p1, int64_t base);
static __device__ __forceinline__ float fg208qd_gg_function_12(float p0, float p1, int64_t base);
static __device__ __forceinline__ float fg208qd_gg_function_13(float p0, int32_t p1, int64_t base);
static __device__ __forceinline__ void fg208qd_gg_function_2(fg208qd_gg_type_5 p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int32_t p18, int32_t p19, int64_t base);
static __device__ __forceinline__ void fg208qd_gg_function_22(fg208qd_gg_type_5 p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base);
static __device__ __forceinline__ void fg208qd_gg_function_1(fg208qd_gg_type_5 p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base);
static __device__ __forceinline__ void fg208qd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg208qd_gg_div(int64_t a, int64_t b) { if (!b) fg208qd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg208qd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg208qd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg208qd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg208qd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg208qd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg208qd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg208qd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg208qd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ void fg208qd_gg_function_3(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg180q_fail)(int64_t);
#endif
  fg180q_fail(t2);
  }
  }
  }
fg208qd_gg_done:;
}
static __device__ __forceinline__ int64_t fg208qd_gg_function_5(uint64_t p0, uint64_t p1, int64_t base) {
  int64_t t0;
  uint64_t t1;
  uint64_t t2;
  int64_t t3;
  uint64_t v0;
  uint64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((uint64_t)(v1));
#ifndef __cplusplus
  extern int64_t (fg180q_ld_i32)(uint64_t, uint64_t);
#endif
  t3 = fg180q_ld_i32(t1, t2);
  t0 = ((int64_t)(t3));
  goto fg208qd_gg_done;
  }
fg208qd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg208qd_gg_function_15(float p0, int32_t p1, int64_t base) {
  float t0;
  int32_t t1;
  int32_t t2;
  uint8_t t3;
  float t4;
  float t5;
  uint32_t t6;
  int32_t t7;
  float t8;
  float t9;
  float t10;
  float t11;
  uint32_t t12;
  int32_t t13;
  float t14;
  float t15;
  float t16;
  float t17;
  uint32_t t18;
  int32_t t19;
  float t20;
  float t21;
  float t22;
  float t23;
  uint32_t t24;
  int32_t t25;
  float t26;
  float t27;
  float t28;
  float t29;
  uint32_t t30;
  int32_t t31;
  float t32;
  float t33;
  float t34;
  float t35;
  uint32_t t36;
  int32_t t37;
  float t38;
  float t39;
  float t40;
  float t41;
  uint32_t t42;
  int32_t t43;
  float t44;
  float t45;
  float v0;
  int32_t v1;
  float v2;
  v0 = p0;
  v1 = p1;
  {
  v2 = ((float)(v0));
  t1 = ((int32_t)(v1));
  t2 = ((int32_t)(((int64_t)UINT64_C(2))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = ((float)(v2));
  t5 = ((float)(v2));
  t6 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t7 = ((int32_t)(((int64_t)UINT64_C(8))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t8 = fg180q_shfl_xor(t5, t6, t7);
  t9 = ((float)(t8));
  v2 = ((float)(((t4) + (t9))));
  t10 = ((float)(v2));
  t11 = ((float)(v2));
  t12 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t13 = ((int32_t)(((int64_t)UINT64_C(4))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t14 = fg180q_shfl_xor(t11, t12, t13);
  t15 = ((float)(t14));
  v2 = ((float)(((t10) + (t15))));
  t16 = ((float)(v2));
  t17 = ((float)(v2));
  t18 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t19 = ((int32_t)(((int64_t)UINT64_C(2))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t20 = fg180q_shfl_xor(t17, t18, t19);
  t21 = ((float)(t20));
  v2 = ((float)(((t16) + (t21))));
  t22 = ((float)(v2));
  t23 = ((float)(v2));
  t24 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t25 = ((int32_t)(((int64_t)UINT64_C(1))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t26 = fg180q_shfl_xor(t23, t24, t25);
  t27 = ((float)(t26));
  v2 = ((float)(((t22) + (t27))));
  }
  } else {
  {
  t28 = ((float)(v2));
  t29 = ((float)(v2));
  t30 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t31 = ((int32_t)(((int64_t)UINT64_C(4))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t32 = fg180q_shfl_xor(t29, t30, t31);
  t33 = ((float)(t32));
  v2 = ((float)(((t28) + (t33))));
  t34 = ((float)(v2));
  t35 = ((float)(v2));
  t36 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t37 = ((int32_t)(((int64_t)UINT64_C(2))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t38 = fg180q_shfl_xor(t35, t36, t37);
  t39 = ((float)(t38));
  v2 = ((float)(((t34) + (t39))));
  t40 = ((float)(v2));
  t41 = ((float)(v2));
  t42 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t43 = ((int32_t)(((int64_t)UINT64_C(1))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t44 = fg180q_shfl_xor(t41, t42, t43);
  t45 = ((float)(t44));
  v2 = ((float)(((t40) + (t45))));
  }
  }
  t0 = ((float)(v2));
  goto fg208qd_gg_done;
  }
fg208qd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg208qd_gg_function_12(float p0, float p1, int64_t base) {
  float t0;
  float t1;
  float t2;
  uint8_t t3;
  float v0;
  float v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((float)(v0));
  t2 = ((float)(v1));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  t0 = ((float)(v1));
  }
  } else {
  {
  t0 = ((float)(v0));
  }
  }
  goto fg208qd_gg_done;
  }
fg208qd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg208qd_gg_function_13(float p0, int32_t p1, int64_t base) {
  float t0;
  int32_t t1;
  int32_t t2;
  uint8_t t3;
  float t4;
  float t5;
  uint32_t t6;
  int32_t t7;
  float t8;
  float t9;
  float t10;
  float t11;
  uint32_t t12;
  int32_t t13;
  float t14;
  float t15;
  float t16;
  float t17;
  uint32_t t18;
  int32_t t19;
  float t20;
  float t21;
  float t22;
  float t23;
  uint32_t t24;
  int32_t t25;
  float t26;
  float t27;
  float t28;
  float t29;
  uint32_t t30;
  int32_t t31;
  float t32;
  float t33;
  float t34;
  float t35;
  uint32_t t36;
  int32_t t37;
  float t38;
  float t39;
  float t40;
  float t41;
  uint32_t t42;
  int32_t t43;
  float t44;
  float t45;
  float v0;
  int32_t v1;
  float v2;
  v0 = p0;
  v1 = p1;
  {
  v2 = ((float)(v0));
  t1 = ((int32_t)(v1));
  t2 = ((int32_t)(((int64_t)UINT64_C(2))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = ((float)(v2));
  t5 = ((float)(v2));
  t6 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t7 = ((int32_t)(((int64_t)UINT64_C(8))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t8 = fg180q_shfl_xor(t5, t6, t7);
  t9 = ((float)(t8));
  v2 = fg208qd_gg_function_12(t4, t9, base + 0);
  t10 = ((float)(v2));
  t11 = ((float)(v2));
  t12 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t13 = ((int32_t)(((int64_t)UINT64_C(4))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t14 = fg180q_shfl_xor(t11, t12, t13);
  t15 = ((float)(t14));
  v2 = fg208qd_gg_function_12(t10, t15, base + 0);
  t16 = ((float)(v2));
  t17 = ((float)(v2));
  t18 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t19 = ((int32_t)(((int64_t)UINT64_C(2))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t20 = fg180q_shfl_xor(t17, t18, t19);
  t21 = ((float)(t20));
  v2 = fg208qd_gg_function_12(t16, t21, base + 0);
  t22 = ((float)(v2));
  t23 = ((float)(v2));
  t24 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t25 = ((int32_t)(((int64_t)UINT64_C(1))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t26 = fg180q_shfl_xor(t23, t24, t25);
  t27 = ((float)(t26));
  v2 = fg208qd_gg_function_12(t22, t27, base + 0);
  }
  } else {
  {
  t28 = ((float)(v2));
  t29 = ((float)(v2));
  t30 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t31 = ((int32_t)(((int64_t)UINT64_C(4))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t32 = fg180q_shfl_xor(t29, t30, t31);
  t33 = ((float)(t32));
  v2 = fg208qd_gg_function_12(t28, t33, base + 0);
  t34 = ((float)(v2));
  t35 = ((float)(v2));
  t36 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t37 = ((int32_t)(((int64_t)UINT64_C(2))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t38 = fg180q_shfl_xor(t35, t36, t37);
  t39 = ((float)(t38));
  v2 = fg208qd_gg_function_12(t34, t39, base + 0);
  t40 = ((float)(v2));
  t41 = ((float)(v2));
  t42 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t43 = ((int32_t)(((int64_t)UINT64_C(1))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t44 = fg180q_shfl_xor(t41, t42, t43);
  t45 = ((float)(t44));
  v2 = fg208qd_gg_function_12(t40, t45, base + 0);
  }
  }
  t0 = ((float)(v2));
  goto fg208qd_gg_done;
  }
fg208qd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg208qd_gg_function_2(fg208qd_gg_type_5 p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int32_t p18, int32_t p19, int64_t base) {
  int32_t t0;
  int32_t t1;
  uint8_t t2;
  int32_t t3;
  int32_t t4;
  uint8_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  uint8_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  uint8_t t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  int64_t t30;
  int32_t t31;
  int32_t t32;
  uint8_t t33;
  int64_t t34;
  int64_t t35;
  int32_t t36;
  int32_t t37;
  uint8_t t38;
  int32_t t39;
  int32_t t40;
  int32_t t41;
  int32_t t42;
  uint64_t t43;
  uint64_t t44;
  uint8_t t45;
  uint64_t t46;
  int64_t t47;
  int64_t t48;
  uint64_t t49;
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
  uint8_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  fg208qd_gg_type_5 t71;
  uint64_t t72;
  float t73;
  fg208qd_gg_type_5 t74;
  uint64_t t75;
  float t76;
  fg208qd_gg_type_5 t77;
  uint64_t t78;
  float t79;
  fg208qd_gg_type_5 t80;
  uint64_t t81;
  float t82;
  float t83;
  float t84;
  float t85;
  float t86;
  float t87;
  float t88;
  float t89;
  float t90;
  float t91;
  float t92;
  float t93;
  float t94;
  float t95;
  float t96;
  float t97;
  int32_t t98;
  int32_t t99;
  int32_t t100;
  uint8_t t101;
  float t102;
  float t103;
  float t104;
  float t105;
  float t106;
  float t107;
  float t108;
  int32_t t109;
  float t110;
  float t111;
  float t112;
  float t113;
  float t114;
  float t115;
  float t116;
  int32_t t117;
  float t118;
  float t119;
  float t120;
  float t121;
  float t122;
  int32_t t123;
  float t124;
  float t125;
  float t126;
  float t127;
  float t128;
  int32_t t129;
  float t130;
  float t131;
  float t132;
  float t133;
  float t134;
  int32_t t135;
  int32_t t136;
  int32_t t137;
  int32_t t138;
  int32_t t139;
  int32_t t140;
  int32_t t141;
  int32_t t142;
  int32_t t143;
  int32_t t144;
  int32_t t145;
  int32_t t146;
  int32_t t147;
  int32_t t148;
  int32_t t149;
  int32_t t150;
  int32_t t151;
  int32_t t152;
  int32_t t153;
  int32_t t154;
  int32_t t155;
  float t156;
  float t157;
  int32_t t158;
  int32_t t159;
  uint8_t t160;
  int32_t t161;
  int32_t t162;
  int32_t t163;
  int32_t t164;
  uint8_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  int64_t t169;
  uint64_t t170;
  int64_t t171;
  int64_t t172;
  uint64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  int64_t t177;
  int32_t t178;
  int32_t t179;
  int32_t t180;
  int32_t t181;
  int32_t t182;
  int32_t t183;
  int64_t t184;
  int64_t t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  int64_t t193;
  uint8_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  uint64_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  int64_t t202;
  uint64_t t203;
  int32_t t204;
  int32_t t205;
  int32_t t206;
  uint8_t t207;
  int64_t t208;
  int64_t t209;
  int64_t t210;
  int64_t t211;
  uint8_t t212;
  uint8_t t213;
  int64_t t214;
  int64_t t215;
  uint8_t t216;
  uint64_t t217;
  int64_t t218;
  int64_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  int64_t t223;
  int64_t t224;
  int64_t t225;
  int64_t t226;
  int64_t t227;
  uint64_t t228;
  float t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
  uint8_t t234;
  uint64_t t235;
  int64_t t236;
  int64_t t237;
  int64_t t238;
  int64_t t239;
  int64_t t240;
  int64_t t241;
  int64_t t242;
  int64_t t243;
  uint64_t t244;
  float t245;
  int64_t t246;
  int64_t t247;
  int64_t t248;
  int64_t t249;
  uint8_t t250;
  int32_t t251;
  int32_t t252;
  uint8_t t253;
  uint64_t t254;
  int64_t t255;
  int64_t t256;
  int64_t t257;
  int64_t t258;
  int64_t t259;
  int64_t t260;
  uint64_t t261;
  float t262;
  float t263;
  uint64_t t264;
  int64_t t265;
  int64_t t266;
  int64_t t267;
  int64_t t268;
  int64_t t269;
  int64_t t270;
  uint64_t t271;
  float t272;
  int32_t t273;
  int32_t t274;
  fg208qd_gg_type_5 v0;
  uint64_t v1;
  uint64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int32_t v10;
  int32_t v11;
  int32_t v12;
  int32_t v13;
  int32_t v14;
  int32_t v15;
  int32_t v16;
  int32_t v17;
  int32_t v18;
  int32_t v19;
  int32_t v20;
  int32_t v21;
  int64_t v22;
  int64_t v23;
  int32_t v24;
  int32_t v25;
  int64_t v26;
  int64_t v27;
  int64_t v28;
  int32_t v29;
  int64_t v30;
  int64_t v31;
  float v32;
  float v33;
  float v34;
  float v35;
  int64_t v36;
  uint64_t v37;
  float v38;
  float v39;
  float v40;
  int32_t v41;
  int32_t v42;
  int32_t v43;
  int32_t v44;
  int32_t v45;
  int32_t v46;
  float v47;
  int32_t v48;
  int32_t v49;
  int64_t v50;
  int64_t v51;
  int64_t v52;
  int32_t v53;
  int64_t v54;
  int64_t v55;
  int64_t v56;
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
  {
  t0 = ((int32_t)(v11));
  t1 = ((int32_t)(((int64_t)UINT64_C(2))));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  v20 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(64))))));
  }
  } else {
  {
  v20 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
  }
  }
  t3 = ((int32_t)(v11));
  t4 = ((int32_t)(((int64_t)UINT64_C(2))));
  t5 = ((t3) == (t4));
  if (t5) {
  {
  v21 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(16))))));
  }
  } else {
  {
  v21 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
  }
  }
  t6 = ((int64_t)(((int64_t)(v17))));
  t7 = ((int64_t)(((int64_t)(v14))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t6) * (uint64_t)(t7)))));
  t9 = ((int64_t)(((int64_t)(v16))));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  t11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  t12 = ((int64_t)(v22));
  t13 = ((int64_t)(v7));
  t14 = ((t12) < (t13));
  if (t14) {
  {
  t15 = ((int64_t)(v7));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((t15) > (t16));
  t18 = t17;
  if (t18) {
  t19 = ((int64_t)(v8));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t19) > (t20));
  }
  t21 = t18;
  t22 = t21;
  if (t22) {
  t23 = ((int64_t)(v9));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((t23) > (t24));
  }
  t25 = t22;
  t26 = t25;
  if (t26) {
  t27 = ((int64_t)(v3));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((t27) > (t28));
  }
  t29 = t26;
  t30 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg208qd_gg_function_3(t29, t30, base + 0);
  v23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((int32_t)(v12));
  t32 = ((int32_t)(((int64_t)UINT64_C(0))));
  t33 = ((t31) != (t32));
  if (t33) {
  {
  t34 = ((int64_t)(((int64_t)(v13))));
  t35 = ((int64_t)(v5));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t34) * (uint64_t)(t35)))));
  }
  } else {
  {
  v24 = ((int32_t)(v15));
  v25 = ((int32_t)(((int32_t)(v9))));
  v26 = ((int64_t)(((int64_t)UINT64_C(0))));
  v27 = ((int64_t)(((int64_t)UINT64_C(0))));
  t36 = ((int32_t)(v25));
  t37 = ((int32_t)(((int64_t)UINT64_C(1))));
  t38 = ((t36) == (t37));
  if (t38) {
  {
  v26 = ((int64_t)(((int64_t)UINT64_C(0))));
  v27 = ((int64_t)(((int64_t)(v24))));
  }
  } else {
  {
  t39 = ((int32_t)(v24));
  t40 = ((int32_t)(v25));
  v26 = ((int64_t)(((int64_t)(((int32_t)fg208qd_gg_mod(t39, t40))))));
  t41 = ((int32_t)(v24));
  t42 = ((int32_t)(v25));
  v27 = ((int64_t)(((int64_t)(((int32_t)fg208qd_gg_div(t41, t42))))));
  }
  }
  v28 = ((int64_t)(((int64_t)(v13))));
  t43 = ((uint64_t)(v1));
  t44 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t45 = ((t43) != (t44));
  if (t45) {
  {
  t46 = ((uint64_t)(v1));
  t47 = ((int64_t)(((int64_t)(v13))));
  t48 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t49 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t47) * (uint64_t)(t48)))))));
  v28 = fg208qd_gg_function_5(t46, t49, base + 0);
  }
  }
  t50 = ((int64_t)(v27));
  t51 = ((int64_t)(v6));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t50) * (uint64_t)(t51)))));
  t53 = ((int64_t)(v26));
  t54 = ((int64_t)(v5));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t53) * (uint64_t)(t54)))));
  t56 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t55)))));
  t57 = ((int64_t)(v28));
  t58 = ((int64_t)(v4));
  t59 = ((int64_t)(((int64_t)((uint64_t)(t57) * (uint64_t)(t58)))));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t56) + (uint64_t)(t59)))));
  }
  }
  t60 = ((int64_t)(v22));
  t61 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(7))))));
  v29 = ((int32_t)(((int32_t)(((t60) >> ((uint64_t)(t61) & 63))))));
  v30 = ((int64_t)(((int64_t)(v29))));
  t62 = ((int64_t)(v22));
  t63 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(127))))));
  v31 = ((int64_t)(((t62) & (t63))));
  v32 = ((float)(fg208qd_gg_double_bits(UINT64_C(0))));
  v33 = ((float)(fg208qd_gg_double_bits(UINT64_C(0))));
  v34 = ((float)(fg208qd_gg_double_bits(UINT64_C(0))));
  v35 = ((float)(fg208qd_gg_double_bits(UINT64_C(0))));
  t64 = ((int64_t)(v22));
  t65 = ((int64_t)(v3));
  t66 = ((t64) < (t65));
  if (t66) {
  {
  t67 = ((int64_t)(v23));
  t68 = ((int64_t)(v22));
  v36 = ((int64_t)(((int64_t)((uint64_t)(t67) + (uint64_t)(t68)))));
  t69 = ((int64_t)(v36));
  t70 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v37 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t69) * (uint64_t)(t70)))))));
  t71 = v0;
  if ((uintptr_t)t71 % alignof(float)) fg208qd_gg_fail("unaligned foreign reference");
  t72 = ((uint64_t)(v37));
#ifndef __cplusplus
  extern float (fg180q_ld_f4x)(fg208qd_gg_type_5, uint64_t);
#endif
  t73 = fg180q_ld_f4x(t71, t72);
  v32 = ((float)(t73));
  t74 = v0;
  if ((uintptr_t)t74 % alignof(float)) fg208qd_gg_fail("unaligned foreign reference");
  t75 = ((uint64_t)(v37));
#ifndef __cplusplus
  extern float (fg180q_ld_f4y)(fg208qd_gg_type_5, uint64_t);
#endif
  t76 = fg180q_ld_f4y(t74, t75);
  v33 = ((float)(t76));
  t77 = v0;
  if ((uintptr_t)t77 % alignof(float)) fg208qd_gg_fail("unaligned foreign reference");
  t78 = ((uint64_t)(v37));
#ifndef __cplusplus
  extern float (fg180q_ld_f4z)(fg208qd_gg_type_5, uint64_t);
#endif
  t79 = fg180q_ld_f4z(t77, t78);
  v34 = ((float)(t79));
  t80 = v0;
  if ((uintptr_t)t80 % alignof(float)) fg208qd_gg_fail("unaligned foreign reference");
  t81 = ((uint64_t)(v37));
#ifndef __cplusplus
  extern float (fg180q_ld_f4w)(fg208qd_gg_type_5, uint64_t);
#endif
  t82 = fg180q_ld_f4w(t80, t81);
  v35 = ((float)(t82));
  }
  }
  t83 = ((float)(v32));
#ifndef __cplusplus
  extern float (fg180q_fabs)(float);
#endif
  t84 = fg180q_fabs(t83);
  v38 = ((float)(t84));
  t85 = ((float)(v38));
  t86 = ((float)(v33));
#ifndef __cplusplus
  extern float (fg180q_fabs)(float);
#endif
  t87 = fg180q_fabs(t86);
  t88 = ((float)(t87));
  v38 = fg208qd_gg_function_12(t85, t88, base + 0);
  t89 = ((float)(v38));
  t90 = ((float)(v34));
#ifndef __cplusplus
  extern float (fg180q_fabs)(float);
#endif
  t91 = fg180q_fabs(t90);
  t92 = ((float)(t91));
  v38 = fg208qd_gg_function_12(t89, t92, base + 0);
  t93 = ((float)(v38));
  t94 = ((float)(v35));
#ifndef __cplusplus
  extern float (fg180q_fabs)(float);
#endif
  t95 = fg180q_fabs(t94);
  t96 = ((float)(t95));
  v38 = fg208qd_gg_function_12(t93, t96, base + 0);
  t97 = ((float)(v38));
  t98 = ((int32_t)(v11));
  v38 = fg208qd_gg_function_13(t97, t98, base + 0);
  v39 = ((float)(fg208qd_gg_double_bits(UINT64_C(0))));
  t99 = ((int32_t)(v11));
  t100 = ((int32_t)(((int64_t)UINT64_C(0))));
  t101 = ((t99) != (t100));
  if (t101) {
  {
  t102 = ((float)(v32));
  t103 = ((float)(v33));
  v39 = ((float)(((t102) + (t103))));
  t104 = ((float)(v39));
  t105 = ((float)(v34));
  v39 = ((float)(((t104) + (t105))));
  t106 = ((float)(v39));
  t107 = ((float)(v35));
  v39 = ((float)(((t106) + (t107))));
  t108 = ((float)(v39));
  t109 = ((int32_t)(v11));
  v39 = fg208qd_gg_function_15(t108, t109, base + 0);
  }
  }
  t110 = ((float)(fg208qd_gg_double_bits(UINT64_C(4638637247447433216))));
  t111 = ((float)(v38));
  v40 = ((float)(((t110) / (t111))));
#ifndef __cplusplus
  extern void (fg180q_pdl_sync)(void);
#endif
  fg180q_pdl_sync();
  t112 = ((float)(v32));
  t113 = ((float)(v40));
  t114 = ((float)(((t112) * (t113))));
#ifndef __cplusplus
  extern float (fg180q_round)(float);
#endif
  t115 = fg180q_round(t114);
  t116 = ((float)(t115));
#ifndef __cplusplus
  extern int32_t (fg180q_i32)(float);
#endif
  t117 = fg180q_i32(t116);
  v41 = ((int32_t)(t117));
  t118 = ((float)(v33));
  t119 = ((float)(v40));
  t120 = ((float)(((t118) * (t119))));
#ifndef __cplusplus
  extern float (fg180q_round)(float);
#endif
  t121 = fg180q_round(t120);
  t122 = ((float)(t121));
#ifndef __cplusplus
  extern int32_t (fg180q_i32)(float);
#endif
  t123 = fg180q_i32(t122);
  v42 = ((int32_t)(t123));
  t124 = ((float)(v34));
  t125 = ((float)(v40));
  t126 = ((float)(((t124) * (t125))));
#ifndef __cplusplus
  extern float (fg180q_round)(float);
#endif
  t127 = fg180q_round(t126);
  t128 = ((float)(t127));
#ifndef __cplusplus
  extern int32_t (fg180q_i32)(float);
#endif
  t129 = fg180q_i32(t128);
  v43 = ((int32_t)(t129));
  t130 = ((float)(v35));
  t131 = ((float)(v40));
  t132 = ((float)(((t130) * (t131))));
#ifndef __cplusplus
  extern float (fg180q_round)(float);
#endif
  t133 = fg180q_round(t132);
  t134 = ((float)(t133));
#ifndef __cplusplus
  extern int32_t (fg180q_i32)(float);
#endif
  t135 = fg180q_i32(t134);
  v44 = ((int32_t)(t135));
  v45 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(255))))));
  t136 = ((int32_t)(v41));
  t137 = ((int32_t)(v45));
  t138 = ((int32_t)(((t136) & (t137))));
  t139 = ((int32_t)(v42));
  t140 = ((int32_t)(v45));
  t141 = ((int32_t)(((t139) & (t140))));
  t142 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(8))))));
  t143 = ((int32_t)(((int32_t)((uint64_t)(t141) << ((uint64_t)(t142) & 31)))));
  t144 = ((int32_t)(((t138) | (t143))));
  t145 = ((int32_t)(v43));
  t146 = ((int32_t)(v45));
  t147 = ((int32_t)(((t145) & (t146))));
  t148 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(16))))));
  t149 = ((int32_t)(((int32_t)((uint64_t)(t147) << ((uint64_t)(t148) & 31)))));
  t150 = ((int32_t)(((t144) | (t149))));
  t151 = ((int32_t)(v44));
  t152 = ((int32_t)(v45));
  t153 = ((int32_t)(((t151) & (t152))));
  t154 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(24))))));
  t155 = ((int32_t)(((int32_t)((uint64_t)(t153) << ((uint64_t)(t154) & 31)))));
  v46 = ((int32_t)(((t150) | (t155))));
  t156 = ((float)(fg208qd_gg_double_bits(UINT64_C(4607182418800017408))));
  t157 = ((float)(v40));
  v47 = ((float)(((t156) / (t157))));
  t158 = ((int32_t)(v12));
  t159 = ((int32_t)(((int64_t)UINT64_C(0))));
  t160 = ((t158) != (t159));
  if (t160) {
  {
  v48 = ((int32_t)(v10));
  }
  } else {
  {
  v48 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  }
  }
  v49 = ((int32_t)(((int64_t)UINT64_C(0))));
label1:;
  t161 = ((int32_t)(v49));
  t162 = ((int32_t)(v48));
  if (!(((t161) < (t162)))) goto label2;
  {
  v50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t163 = ((int32_t)(v12));
  t164 = ((int32_t)(((int64_t)UINT64_C(0))));
  t165 = ((t163) != (t164));
  if (t165) {
  {
  t166 = ((int64_t)(((int64_t)(v13))));
  t167 = ((int64_t)(((int64_t)(v10))));
  t168 = ((int64_t)(((int64_t)((uint64_t)(t166) * (uint64_t)(t167)))));
  t169 = ((int64_t)(((int64_t)(v49))));
  v51 = ((int64_t)(((int64_t)((uint64_t)(t168) + (uint64_t)(t169)))));
  t170 = ((uint64_t)(v1));
  t171 = ((int64_t)(v51));
  t172 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t173 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t171) * (uint64_t)(t172)))))));
  v52 = fg208qd_gg_function_5(t170, t173, base + 0);
  t174 = ((int64_t)(v30));
  t175 = ((int64_t)(v8));
  t176 = ((int64_t)(((int64_t)((uint64_t)(t174) * (uint64_t)(t175)))));
  t177 = ((int64_t)(v52));
  v50 = ((int64_t)(((int64_t)((uint64_t)(t176) + (uint64_t)(t177)))));
  }
  } else {
  {
  t178 = ((int32_t)(v18));
  t179 = ((int32_t)(v19));
  t180 = ((int32_t)(((int32_t)((uint64_t)(t178) * (uint64_t)(t179)))));
  t181 = ((int32_t)(v17));
  t182 = ((int32_t)(((int32_t)((uint64_t)(t180) * (uint64_t)(t181)))));
  t183 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(7))))));
  v53 = ((int32_t)(((t182) >> ((uint64_t)(t183) & 31))));
  v54 = ((int64_t)(((int64_t)(v53))));
  t184 = ((int64_t)(((int64_t)(v15))));
  t185 = ((int64_t)(v54));
  v55 = ((int64_t)(((int64_t)((uint64_t)(t184) * (uint64_t)(t185)))));
  t186 = ((int64_t)(v55));
  t187 = ((int64_t)(v30));
  t188 = ((int64_t)(v8));
  t189 = ((int64_t)(((int64_t)((uint64_t)(t187) * (uint64_t)(t188)))));
  t190 = ((int64_t)(((int64_t)((uint64_t)(t186) + (uint64_t)(t189)))));
  t191 = ((int64_t)(((int64_t)(v13))));
  v50 = ((int64_t)(((int64_t)((uint64_t)(t190) + (uint64_t)(t191)))));
  }
  }
  t192 = ((int64_t)(v50));
  t193 = ((int64_t)(((int64_t)UINT64_C(0))));
  t194 = ((t192) >= (t193));
  t195 = ((int64_t)(((int64_t)UINT64_C(11))));
  fg208qd_gg_function_3(t194, t195, base + 0);
  t196 = ((int64_t)(v50));
  t197 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(144))))));
  v56 = ((int64_t)(((int64_t)((uint64_t)(t196) * (uint64_t)(t197)))));
  t198 = ((uint64_t)(v2));
  t199 = ((int64_t)(v56));
  t200 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t201 = ((int64_t)(((int64_t)((uint64_t)(t199) + (uint64_t)(t200)))));
  t202 = ((int64_t)(v31));
  t203 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t201) + (uint64_t)(t202)))))));
  t204 = ((int32_t)(v46));
#ifndef __cplusplus
  extern void (fg180q_st_i32)(uint64_t, uint64_t, int32_t);
#endif
  fg180q_st_i32(t198, t203, t204);
  t205 = ((int32_t)(v11));
  t206 = ((int32_t)(((int64_t)UINT64_C(2))));
  t207 = ((t205) == (t206));
  if (t207) {
  {
  t208 = ((int64_t)(v31));
  t209 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(15))))));
  t210 = ((int64_t)(((t208) & (t209))));
  t211 = ((int64_t)(((int64_t)UINT64_C(0))));
  t212 = ((t210) == (t211));
  t213 = t212;
  if (t213) {
  t214 = ((int64_t)(((int64_t)UINT64_C(96))));
  t215 = ((int64_t)(v31));
  t213 = ((t214) > (t215));
  }
  t216 = t213;
  if (t216) {
  {
  t217 = ((uint64_t)(v2));
  t218 = ((int64_t)(v56));
  t219 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t220 = ((int64_t)(((int64_t)((uint64_t)(t218) + (uint64_t)(t219)))));
  t221 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t222 = ((int64_t)(v31));
  t223 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t224 = ((int64_t)(((t222) >> ((uint64_t)(t223) & 63))));
  t225 = ((int64_t)(((int64_t)((uint64_t)(t221) + (uint64_t)(t224)))));
  t226 = ((int64_t)(((int64_t)UINT64_C(2))));
  t227 = ((int64_t)(((int64_t)((uint64_t)(t225) * (uint64_t)(t226)))));
  t228 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t220) + (uint64_t)(t227)))))));
  t229 = ((float)(v39));
#ifndef __cplusplus
  extern void (fg180q_st_f32)(uint64_t, uint64_t, float);
#endif
  fg180q_st_f32(t217, t228, t229);
  t230 = ((int64_t)(v31));
  t231 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(63))))));
  t232 = ((int64_t)(((t230) & (t231))));
  t233 = ((int64_t)(((int64_t)UINT64_C(0))));
  t234 = ((t232) == (t233));
  if (t234) {
  {
  t235 = ((uint64_t)(v2));
  t236 = ((int64_t)(v56));
  t237 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t238 = ((int64_t)(((int64_t)((uint64_t)(t236) + (uint64_t)(t237)))));
  t239 = ((int64_t)(v31));
  t240 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(6))))));
  t241 = ((int64_t)(((t239) >> ((uint64_t)(t240) & 63))));
  t242 = ((int64_t)(((int64_t)UINT64_C(2))));
  t243 = ((int64_t)(((int64_t)((uint64_t)(t241) * (uint64_t)(t242)))));
  t244 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t238) + (uint64_t)(t243)))))));
  t245 = ((float)(v47));
#ifndef __cplusplus
  extern void (fg180q_st_f32)(uint64_t, uint64_t, float);
#endif
  fg180q_st_f32(t235, t244, t245);
  }
  }
  }
  }
  }
  } else {
  {
  t246 = ((int64_t)(v31));
  t247 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(31))))));
  t248 = ((int64_t)(((t246) & (t247))));
  t249 = ((int64_t)(((int64_t)UINT64_C(0))));
  t250 = ((t248) == (t249));
  if (t250) {
  {
  t251 = ((int32_t)(v11));
  t252 = ((int32_t)(((int64_t)UINT64_C(1))));
  t253 = ((t251) == (t252));
  if (t253) {
  {
  t254 = ((uint64_t)(v2));
  t255 = ((int64_t)(v56));
  t256 = ((int64_t)(v31));
  t257 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t258 = ((int64_t)(((t256) >> ((uint64_t)(t257) & 63))));
  t259 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t260 = ((int64_t)(((int64_t)((uint64_t)(t258) * (uint64_t)(t259)))));
  t261 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t255) + (uint64_t)(t260)))))));
  t262 = ((float)(v47));
  t263 = ((float)(v39));
#ifndef __cplusplus
  extern void (fg180q_st_half2)(uint64_t, uint64_t, float, float);
#endif
  fg180q_st_half2(t254, t261, t262, t263);
  }
  } else {
  {
  t264 = ((uint64_t)(v2));
  t265 = ((int64_t)(v56));
  t266 = ((int64_t)(v31));
  t267 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t268 = ((int64_t)(((t266) >> ((uint64_t)(t267) & 63))));
  t269 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t270 = ((int64_t)(((int64_t)((uint64_t)(t268) * (uint64_t)(t269)))));
  t271 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t265) + (uint64_t)(t270)))))));
  t272 = ((float)(v47));
#ifndef __cplusplus
  extern void (fg180q_st_f32)(uint64_t, uint64_t, float);
#endif
  fg180q_st_f32(t264, t271, t272);
  }
  }
  }
  }
  }
  }
  t273 = v49;
  t274 = ((int32_t)(((int64_t)UINT64_C(1))));
  v49 = ((int32_t)((uint64_t)(t273) + (uint64_t)(t274)));
  }
  goto label1;
label2:;
  }
  }
  }
fg208qd_gg_done:;
}
static __device__ __forceinline__ void fg208qd_gg_function_22(fg208qd_gg_type_5 p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base) {
  fg208qd_gg_type_5 t0;
  uint64_t t1;
  uint64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int32_t t10;
  int32_t t11;
  int32_t t12;
  int32_t t13;
  int32_t t14;
  int32_t t15;
  int32_t t16;
  int32_t t17;
  int32_t t18;
  int32_t t19;
  fg208qd_gg_type_5 v0;
  uint64_t v1;
  uint64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int32_t v10;
  int32_t v11;
  int32_t v12;
  int32_t v13;
  int32_t v14;
  int32_t v15;
  int32_t v16;
  int32_t v17;
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
  t0 = v0;
  t1 = ((uint64_t)(v1));
  t2 = ((uint64_t)(v2));
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(v5));
  t6 = ((int64_t)(v6));
  t7 = ((int64_t)(v7));
  t8 = ((int64_t)(v8));
  t9 = ((int64_t)(v9));
  t10 = ((int32_t)(v10));
  t11 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  t12 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  t13 = ((int32_t)(v11));
  t14 = ((int32_t)(v12));
  t15 = ((int32_t)(v13));
  t16 = ((int32_t)(v14));
  t17 = ((int32_t)(v15));
  t18 = ((int32_t)(v16));
  t19 = ((int32_t)(v17));
  fg208qd_gg_function_2(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, t19, base + 0);
  }
fg208qd_gg_done:;
}
static __device__ __forceinline__ void fg208qd_gg_function_1(fg208qd_gg_type_5 p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base) {
  fg208qd_gg_type_5 t0;
  uint64_t t1;
  uint64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int32_t t10;
  int32_t t11;
  int32_t t12;
  int32_t t13;
  int32_t t14;
  int32_t t15;
  int32_t t16;
  int32_t t17;
  int32_t t18;
  int32_t t19;
  fg208qd_gg_type_5 v0;
  uint64_t v1;
  uint64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int32_t v10;
  int32_t v11;
  int32_t v12;
  int32_t v13;
  int32_t v14;
  int32_t v15;
  int32_t v16;
  int32_t v17;
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
  t0 = v0;
  t1 = ((uint64_t)(v1));
  t2 = ((uint64_t)(v2));
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(v5));
  t6 = ((int64_t)(v6));
  t7 = ((int64_t)(v7));
  t8 = ((int64_t)(v8));
  t9 = ((int64_t)(v9));
  t10 = ((int32_t)(v10));
  t11 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  t12 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  t13 = ((int32_t)(v11));
  t14 = ((int32_t)(v12));
  t15 = ((int32_t)(v13));
  t16 = ((int32_t)(v14));
  t17 = ((int32_t)(v15));
  t18 = ((int32_t)(v16));
  t19 = ((int32_t)(v17));
  fg208qd_gg_function_2(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, t19, base + 0);
  }
fg208qd_gg_done:;
}
