#pragma once
static __device__ __forceinline__ void fg180qd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg180qd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg180qd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg180qd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg180qd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg180qd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ void fg180qd_gg_function_3(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ int64_t fg180qd_gg_function_5(uint64_t p0, uint64_t p1, int64_t base);
static __device__ __forceinline__ float fg180qd_gg_function_15(float p0, int32_t p1, int64_t base);
static __device__ __forceinline__ float fg180qd_gg_function_12(float p0, float p1, int64_t base);
static __device__ __forceinline__ float fg180qd_gg_function_13(float p0, int32_t p1, int64_t base);
static __device__ __forceinline__ void fg180qd_gg_function_2(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int32_t p18, int32_t p19, int64_t base);
static __device__ __forceinline__ void fg180qd_gg_function_22(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base);
static __device__ __forceinline__ void fg180qd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base);
static __device__ __forceinline__ void fg180qd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg180qd_gg_div(int64_t a, int64_t b) { if (!b) fg180qd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg180qd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg180qd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg180qd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg180qd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg180qd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg180qd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg180qd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg180qd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ void fg180qd_gg_function_3(uint8_t p0, int64_t p1, int64_t base) {
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
fg180qd_gg_done:;
}
static __device__ __forceinline__ int64_t fg180qd_gg_function_5(uint64_t p0, uint64_t p1, int64_t base) {
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
  goto fg180qd_gg_done;
  }
fg180qd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg180qd_gg_function_15(float p0, int32_t p1, int64_t base) {
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
  goto fg180qd_gg_done;
  }
fg180qd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg180qd_gg_function_12(float p0, float p1, int64_t base) {
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
  goto fg180qd_gg_done;
  }
fg180qd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg180qd_gg_function_13(float p0, int32_t p1, int64_t base) {
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
  v2 = fg180qd_gg_function_12(t4, t9, base + 0);
  t10 = ((float)(v2));
  t11 = ((float)(v2));
  t12 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t13 = ((int32_t)(((int64_t)UINT64_C(4))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t14 = fg180q_shfl_xor(t11, t12, t13);
  t15 = ((float)(t14));
  v2 = fg180qd_gg_function_12(t10, t15, base + 0);
  t16 = ((float)(v2));
  t17 = ((float)(v2));
  t18 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t19 = ((int32_t)(((int64_t)UINT64_C(2))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t20 = fg180q_shfl_xor(t17, t18, t19);
  t21 = ((float)(t20));
  v2 = fg180qd_gg_function_12(t16, t21, base + 0);
  t22 = ((float)(v2));
  t23 = ((float)(v2));
  t24 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t25 = ((int32_t)(((int64_t)UINT64_C(1))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t26 = fg180q_shfl_xor(t23, t24, t25);
  t27 = ((float)(t26));
  v2 = fg180qd_gg_function_12(t22, t27, base + 0);
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
  v2 = fg180qd_gg_function_12(t28, t33, base + 0);
  t34 = ((float)(v2));
  t35 = ((float)(v2));
  t36 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t37 = ((int32_t)(((int64_t)UINT64_C(2))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t38 = fg180q_shfl_xor(t35, t36, t37);
  t39 = ((float)(t38));
  v2 = fg180qd_gg_function_12(t34, t39, base + 0);
  t40 = ((float)(v2));
  t41 = ((float)(v2));
  t42 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t43 = ((int32_t)(((int64_t)UINT64_C(1))));
#ifndef __cplusplus
  extern float (fg180q_shfl_xor)(float, uint32_t, int32_t);
#endif
  t44 = fg180q_shfl_xor(t41, t42, t43);
  t45 = ((float)(t44));
  v2 = fg180qd_gg_function_12(t40, t45, base + 0);
  }
  }
  t0 = ((float)(v2));
  goto fg180qd_gg_done;
  }
fg180qd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg180qd_gg_function_2(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int32_t p18, int32_t p19, int64_t base) {
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
  uint64_t t36;
  uint64_t t37;
  uint8_t t38;
  uint64_t t39;
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
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  uint8_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  uint64_t t64;
  uint64_t t65;
  float t66;
  uint64_t t67;
  uint64_t t68;
  float t69;
  uint64_t t70;
  uint64_t t71;
  float t72;
  uint64_t t73;
  uint64_t t74;
  float t75;
  float t76;
  float t77;
  float t78;
  float t79;
  float t80;
  float t81;
  float t82;
  float t83;
  float t84;
  float t85;
  float t86;
  float t87;
  float t88;
  float t89;
  float t90;
  int32_t t91;
  int32_t t92;
  int32_t t93;
  uint8_t t94;
  float t95;
  float t96;
  float t97;
  float t98;
  float t99;
  float t100;
  float t101;
  int32_t t102;
  float t103;
  float t104;
  float t105;
  float t106;
  float t107;
  float t108;
  float t109;
  int32_t t110;
  float t111;
  float t112;
  float t113;
  float t114;
  float t115;
  int32_t t116;
  float t117;
  float t118;
  float t119;
  float t120;
  float t121;
  int32_t t122;
  float t123;
  float t124;
  float t125;
  float t126;
  float t127;
  int32_t t128;
  int32_t t129;
  int32_t t130;
  int32_t t131;
  int32_t t132;
  int32_t t133;
  int32_t t134;
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
  float t149;
  float t150;
  int32_t t151;
  int32_t t152;
  uint8_t t153;
  int32_t t154;
  int32_t t155;
  int32_t t156;
  int32_t t157;
  uint8_t t158;
  int64_t t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  uint64_t t163;
  int64_t t164;
  int64_t t165;
  uint64_t t166;
  int64_t t167;
  int64_t t168;
  int64_t t169;
  int64_t t170;
  int32_t t171;
  int32_t t172;
  int32_t t173;
  int32_t t174;
  int32_t t175;
  int32_t t176;
  int64_t t177;
  int64_t t178;
  int64_t t179;
  int64_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  int64_t t185;
  int64_t t186;
  uint8_t t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  uint64_t t191;
  int64_t t192;
  int64_t t193;
  int64_t t194;
  int64_t t195;
  uint64_t t196;
  int32_t t197;
  int32_t t198;
  int32_t t199;
  uint8_t t200;
  int64_t t201;
  int64_t t202;
  int64_t t203;
  int64_t t204;
  uint8_t t205;
  uint8_t t206;
  int64_t t207;
  int64_t t208;
  uint8_t t209;
  uint64_t t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  int64_t t216;
  int64_t t217;
  int64_t t218;
  int64_t t219;
  int64_t t220;
  uint64_t t221;
  float t222;
  int64_t t223;
  int64_t t224;
  int64_t t225;
  int64_t t226;
  uint8_t t227;
  uint64_t t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
  int64_t t234;
  int64_t t235;
  int64_t t236;
  uint64_t t237;
  float t238;
  int64_t t239;
  int64_t t240;
  int64_t t241;
  int64_t t242;
  uint8_t t243;
  int32_t t244;
  int32_t t245;
  uint8_t t246;
  uint64_t t247;
  int64_t t248;
  int64_t t249;
  int64_t t250;
  int64_t t251;
  int64_t t252;
  int64_t t253;
  uint64_t t254;
  float t255;
  float t256;
  uint64_t t257;
  int64_t t258;
  int64_t t259;
  int64_t t260;
  int64_t t261;
  int64_t t262;
  int64_t t263;
  uint64_t t264;
  float t265;
  int32_t t266;
  int32_t t267;
  uint64_t v0;
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
  fg180qd_gg_function_3(t29, t30, base + 0);
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
  v27 = ((int64_t)(((int64_t)(v24))));
  v28 = ((int64_t)(((int64_t)(v13))));
  t36 = ((uint64_t)(v1));
  t37 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t38 = ((t36) != (t37));
  if (t38) {
  {
  t39 = ((uint64_t)(v1));
  t40 = ((int64_t)(((int64_t)(v13))));
  t41 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t42 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t40) * (uint64_t)(t41)))))));
  v28 = fg180qd_gg_function_5(t39, t42, base + 0);
  }
  }
  t43 = ((int64_t)(v27));
  t44 = ((int64_t)(v6));
  t45 = ((int64_t)(((int64_t)((uint64_t)(t43) * (uint64_t)(t44)))));
  t46 = ((int64_t)(v26));
  t47 = ((int64_t)(v5));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t46) * (uint64_t)(t47)))));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t45) + (uint64_t)(t48)))));
  t50 = ((int64_t)(v28));
  t51 = ((int64_t)(v4));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t50) * (uint64_t)(t51)))));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t49) + (uint64_t)(t52)))));
  }
  }
  t53 = ((int64_t)(v22));
  t54 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(7))))));
  v29 = ((int32_t)(((int32_t)(((t53) >> ((uint64_t)(t54) & 63))))));
  v30 = ((int64_t)(((int64_t)(v29))));
  t55 = ((int64_t)(v22));
  t56 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(127))))));
  v31 = ((int64_t)(((t55) & (t56))));
  v32 = ((float)(fg180qd_gg_double_bits(UINT64_C(0))));
  v33 = ((float)(fg180qd_gg_double_bits(UINT64_C(0))));
  v34 = ((float)(fg180qd_gg_double_bits(UINT64_C(0))));
  v35 = ((float)(fg180qd_gg_double_bits(UINT64_C(0))));
  t57 = ((int64_t)(v22));
  t58 = ((int64_t)(v3));
  t59 = ((t57) < (t58));
  if (t59) {
  {
  t60 = ((int64_t)(v23));
  t61 = ((int64_t)(v22));
  v36 = ((int64_t)(((int64_t)((uint64_t)(t60) + (uint64_t)(t61)))));
  t62 = ((int64_t)(v36));
  t63 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v37 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t62) * (uint64_t)(t63)))))));
  t64 = ((uint64_t)(v0));
  t65 = ((uint64_t)(v37));
#ifndef __cplusplus
  extern float (fg180q_ld_f4x)(uint64_t, uint64_t);
#endif
  t66 = fg180q_ld_f4x(t64, t65);
  v32 = ((float)(t66));
  t67 = ((uint64_t)(v0));
  t68 = ((uint64_t)(v37));
#ifndef __cplusplus
  extern float (fg180q_ld_f4y)(uint64_t, uint64_t);
#endif
  t69 = fg180q_ld_f4y(t67, t68);
  v33 = ((float)(t69));
  t70 = ((uint64_t)(v0));
  t71 = ((uint64_t)(v37));
#ifndef __cplusplus
  extern float (fg180q_ld_f4z)(uint64_t, uint64_t);
#endif
  t72 = fg180q_ld_f4z(t70, t71);
  v34 = ((float)(t72));
  t73 = ((uint64_t)(v0));
  t74 = ((uint64_t)(v37));
#ifndef __cplusplus
  extern float (fg180q_ld_f4w)(uint64_t, uint64_t);
#endif
  t75 = fg180q_ld_f4w(t73, t74);
  v35 = ((float)(t75));
  }
  }
  t76 = ((float)(v32));
#ifndef __cplusplus
  extern float (fg180q_fabs)(float);
#endif
  t77 = fg180q_fabs(t76);
  v38 = ((float)(t77));
  t78 = ((float)(v38));
  t79 = ((float)(v33));
#ifndef __cplusplus
  extern float (fg180q_fabs)(float);
#endif
  t80 = fg180q_fabs(t79);
  t81 = ((float)(t80));
  v38 = fg180qd_gg_function_12(t78, t81, base + 0);
  t82 = ((float)(v38));
  t83 = ((float)(v34));
#ifndef __cplusplus
  extern float (fg180q_fabs)(float);
#endif
  t84 = fg180q_fabs(t83);
  t85 = ((float)(t84));
  v38 = fg180qd_gg_function_12(t82, t85, base + 0);
  t86 = ((float)(v38));
  t87 = ((float)(v35));
#ifndef __cplusplus
  extern float (fg180q_fabs)(float);
#endif
  t88 = fg180q_fabs(t87);
  t89 = ((float)(t88));
  v38 = fg180qd_gg_function_12(t86, t89, base + 0);
  t90 = ((float)(v38));
  t91 = ((int32_t)(v11));
  v38 = fg180qd_gg_function_13(t90, t91, base + 0);
  v39 = ((float)(fg180qd_gg_double_bits(UINT64_C(0))));
  t92 = ((int32_t)(v11));
  t93 = ((int32_t)(((int64_t)UINT64_C(0))));
  t94 = ((t92) != (t93));
  if (t94) {
  {
  t95 = ((float)(v32));
  t96 = ((float)(v33));
  v39 = ((float)(((t95) + (t96))));
  t97 = ((float)(v39));
  t98 = ((float)(v34));
  v39 = ((float)(((t97) + (t98))));
  t99 = ((float)(v39));
  t100 = ((float)(v35));
  v39 = ((float)(((t99) + (t100))));
  t101 = ((float)(v39));
  t102 = ((int32_t)(v11));
  v39 = fg180qd_gg_function_15(t101, t102, base + 0);
  }
  }
  t103 = ((float)(fg180qd_gg_double_bits(UINT64_C(4638637247447433216))));
  t104 = ((float)(v38));
  v40 = ((float)(((t103) / (t104))));
#ifndef __cplusplus
  extern void (fg180q_pdl_sync)(void);
#endif
  fg180q_pdl_sync();
  t105 = ((float)(v32));
  t106 = ((float)(v40));
  t107 = ((float)(((t105) * (t106))));
#ifndef __cplusplus
  extern float (fg180q_round)(float);
#endif
  t108 = fg180q_round(t107);
  t109 = ((float)(t108));
#ifndef __cplusplus
  extern int32_t (fg180q_i32)(float);
#endif
  t110 = fg180q_i32(t109);
  v41 = ((int32_t)(t110));
  t111 = ((float)(v33));
  t112 = ((float)(v40));
  t113 = ((float)(((t111) * (t112))));
#ifndef __cplusplus
  extern float (fg180q_round)(float);
#endif
  t114 = fg180q_round(t113);
  t115 = ((float)(t114));
#ifndef __cplusplus
  extern int32_t (fg180q_i32)(float);
#endif
  t116 = fg180q_i32(t115);
  v42 = ((int32_t)(t116));
  t117 = ((float)(v34));
  t118 = ((float)(v40));
  t119 = ((float)(((t117) * (t118))));
#ifndef __cplusplus
  extern float (fg180q_round)(float);
#endif
  t120 = fg180q_round(t119);
  t121 = ((float)(t120));
#ifndef __cplusplus
  extern int32_t (fg180q_i32)(float);
#endif
  t122 = fg180q_i32(t121);
  v43 = ((int32_t)(t122));
  t123 = ((float)(v35));
  t124 = ((float)(v40));
  t125 = ((float)(((t123) * (t124))));
#ifndef __cplusplus
  extern float (fg180q_round)(float);
#endif
  t126 = fg180q_round(t125);
  t127 = ((float)(t126));
#ifndef __cplusplus
  extern int32_t (fg180q_i32)(float);
#endif
  t128 = fg180q_i32(t127);
  v44 = ((int32_t)(t128));
  v45 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(255))))));
  t129 = ((int32_t)(v41));
  t130 = ((int32_t)(v45));
  t131 = ((int32_t)(((t129) & (t130))));
  t132 = ((int32_t)(v42));
  t133 = ((int32_t)(v45));
  t134 = ((int32_t)(((t132) & (t133))));
  t135 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(8))))));
  t136 = ((int32_t)(((int32_t)((uint64_t)(t134) << ((uint64_t)(t135) & 31)))));
  t137 = ((int32_t)(((t131) | (t136))));
  t138 = ((int32_t)(v43));
  t139 = ((int32_t)(v45));
  t140 = ((int32_t)(((t138) & (t139))));
  t141 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(16))))));
  t142 = ((int32_t)(((int32_t)((uint64_t)(t140) << ((uint64_t)(t141) & 31)))));
  t143 = ((int32_t)(((t137) | (t142))));
  t144 = ((int32_t)(v44));
  t145 = ((int32_t)(v45));
  t146 = ((int32_t)(((t144) & (t145))));
  t147 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(24))))));
  t148 = ((int32_t)(((int32_t)((uint64_t)(t146) << ((uint64_t)(t147) & 31)))));
  v46 = ((int32_t)(((t143) | (t148))));
  t149 = ((float)(fg180qd_gg_double_bits(UINT64_C(4607182418800017408))));
  t150 = ((float)(v40));
  v47 = ((float)(((t149) / (t150))));
  t151 = ((int32_t)(v12));
  t152 = ((int32_t)(((int64_t)UINT64_C(0))));
  t153 = ((t151) != (t152));
  if (t153) {
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
  t154 = ((int32_t)(v49));
  t155 = ((int32_t)(v48));
  if (!(((t154) < (t155)))) goto label2;
  {
  v50 = ((int64_t)(((int64_t)UINT64_C(0))));
  t156 = ((int32_t)(v12));
  t157 = ((int32_t)(((int64_t)UINT64_C(0))));
  t158 = ((t156) != (t157));
  if (t158) {
  {
  t159 = ((int64_t)(((int64_t)(v13))));
  t160 = ((int64_t)(((int64_t)(v10))));
  t161 = ((int64_t)(((int64_t)((uint64_t)(t159) * (uint64_t)(t160)))));
  t162 = ((int64_t)(((int64_t)(v49))));
  v51 = ((int64_t)(((int64_t)((uint64_t)(t161) + (uint64_t)(t162)))));
  t163 = ((uint64_t)(v1));
  t164 = ((int64_t)(v51));
  t165 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t166 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t164) * (uint64_t)(t165)))))));
  v52 = fg180qd_gg_function_5(t163, t166, base + 0);
  t167 = ((int64_t)(v30));
  t168 = ((int64_t)(v8));
  t169 = ((int64_t)(((int64_t)((uint64_t)(t167) * (uint64_t)(t168)))));
  t170 = ((int64_t)(v52));
  v50 = ((int64_t)(((int64_t)((uint64_t)(t169) + (uint64_t)(t170)))));
  }
  } else {
  {
  t171 = ((int32_t)(v18));
  t172 = ((int32_t)(v19));
  t173 = ((int32_t)(((int32_t)((uint64_t)(t171) * (uint64_t)(t172)))));
  t174 = ((int32_t)(v17));
  t175 = ((int32_t)(((int32_t)((uint64_t)(t173) * (uint64_t)(t174)))));
  t176 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(7))))));
  v53 = ((int32_t)(((t175) >> ((uint64_t)(t176) & 31))));
  v54 = ((int64_t)(((int64_t)(v53))));
  t177 = ((int64_t)(((int64_t)(v15))));
  t178 = ((int64_t)(v54));
  v55 = ((int64_t)(((int64_t)((uint64_t)(t177) * (uint64_t)(t178)))));
  t179 = ((int64_t)(v55));
  t180 = ((int64_t)(v30));
  t181 = ((int64_t)(v8));
  t182 = ((int64_t)(((int64_t)((uint64_t)(t180) * (uint64_t)(t181)))));
  t183 = ((int64_t)(((int64_t)((uint64_t)(t179) + (uint64_t)(t182)))));
  t184 = ((int64_t)(((int64_t)(v13))));
  v50 = ((int64_t)(((int64_t)((uint64_t)(t183) + (uint64_t)(t184)))));
  }
  }
  t185 = ((int64_t)(v50));
  t186 = ((int64_t)(((int64_t)UINT64_C(0))));
  t187 = ((t185) >= (t186));
  t188 = ((int64_t)(((int64_t)UINT64_C(11))));
  fg180qd_gg_function_3(t187, t188, base + 0);
  t189 = ((int64_t)(v50));
  t190 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(144))))));
  v56 = ((int64_t)(((int64_t)((uint64_t)(t189) * (uint64_t)(t190)))));
  t191 = ((uint64_t)(v2));
  t192 = ((int64_t)(v56));
  t193 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t194 = ((int64_t)(((int64_t)((uint64_t)(t192) + (uint64_t)(t193)))));
  t195 = ((int64_t)(v31));
  t196 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t194) + (uint64_t)(t195)))))));
  t197 = ((int32_t)(v46));
#ifndef __cplusplus
  extern void (fg180q_st_i32)(uint64_t, uint64_t, int32_t);
#endif
  fg180q_st_i32(t191, t196, t197);
  t198 = ((int32_t)(v11));
  t199 = ((int32_t)(((int64_t)UINT64_C(2))));
  t200 = ((t198) == (t199));
  if (t200) {
  {
  t201 = ((int64_t)(v31));
  t202 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(15))))));
  t203 = ((int64_t)(((t201) & (t202))));
  t204 = ((int64_t)(((int64_t)UINT64_C(0))));
  t205 = ((t203) == (t204));
  t206 = t205;
  if (t206) {
  t207 = ((int64_t)(v31));
  t208 = ((int64_t)(((int64_t)UINT64_C(96))));
  t206 = ((t207) < (t208));
  }
  t209 = t206;
  if (t209) {
  {
  t210 = ((uint64_t)(v2));
  t211 = ((int64_t)(v56));
  t212 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t213 = ((int64_t)(((int64_t)((uint64_t)(t211) + (uint64_t)(t212)))));
  t214 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t215 = ((int64_t)(v31));
  t216 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t217 = ((int64_t)(((t215) >> ((uint64_t)(t216) & 63))));
  t218 = ((int64_t)(((int64_t)((uint64_t)(t214) + (uint64_t)(t217)))));
  t219 = ((int64_t)(((int64_t)UINT64_C(2))));
  t220 = ((int64_t)(((int64_t)((uint64_t)(t218) * (uint64_t)(t219)))));
  t221 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t213) + (uint64_t)(t220)))))));
  t222 = ((float)(v39));
#ifndef __cplusplus
  extern void (fg180q_st_f32)(uint64_t, uint64_t, float);
#endif
  fg180q_st_f32(t210, t221, t222);
  t223 = ((int64_t)(v31));
  t224 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(63))))));
  t225 = ((int64_t)(((t223) & (t224))));
  t226 = ((int64_t)(((int64_t)UINT64_C(0))));
  t227 = ((t225) == (t226));
  if (t227) {
  {
  t228 = ((uint64_t)(v2));
  t229 = ((int64_t)(v56));
  t230 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t231 = ((int64_t)(((int64_t)((uint64_t)(t229) + (uint64_t)(t230)))));
  t232 = ((int64_t)(v31));
  t233 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(6))))));
  t234 = ((int64_t)(((t232) >> ((uint64_t)(t233) & 63))));
  t235 = ((int64_t)(((int64_t)UINT64_C(2))));
  t236 = ((int64_t)(((int64_t)((uint64_t)(t234) * (uint64_t)(t235)))));
  t237 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t231) + (uint64_t)(t236)))))));
  t238 = ((float)(v47));
#ifndef __cplusplus
  extern void (fg180q_st_f32)(uint64_t, uint64_t, float);
#endif
  fg180q_st_f32(t228, t237, t238);
  }
  }
  }
  }
  }
  } else {
  {
  t239 = ((int64_t)(v31));
  t240 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(31))))));
  t241 = ((int64_t)(((t239) & (t240))));
  t242 = ((int64_t)(((int64_t)UINT64_C(0))));
  t243 = ((t241) == (t242));
  if (t243) {
  {
  t244 = ((int32_t)(v11));
  t245 = ((int32_t)(((int64_t)UINT64_C(1))));
  t246 = ((t244) == (t245));
  if (t246) {
  {
  t247 = ((uint64_t)(v2));
  t248 = ((int64_t)(v56));
  t249 = ((int64_t)(v31));
  t250 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t251 = ((int64_t)(((t249) >> ((uint64_t)(t250) & 63))));
  t252 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t253 = ((int64_t)(((int64_t)((uint64_t)(t251) * (uint64_t)(t252)))));
  t254 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t248) + (uint64_t)(t253)))))));
  t255 = ((float)(v47));
  t256 = ((float)(v39));
#ifndef __cplusplus
  extern void (fg180q_st_half2)(uint64_t, uint64_t, float, float);
#endif
  fg180q_st_half2(t247, t254, t255, t256);
  }
  } else {
  {
  t257 = ((uint64_t)(v2));
  t258 = ((int64_t)(v56));
  t259 = ((int64_t)(v31));
  t260 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(5))))));
  t261 = ((int64_t)(((t259) >> ((uint64_t)(t260) & 63))));
  t262 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t263 = ((int64_t)(((int64_t)((uint64_t)(t261) * (uint64_t)(t262)))));
  t264 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t258) + (uint64_t)(t263)))))));
  t265 = ((float)(v47));
#ifndef __cplusplus
  extern void (fg180q_st_f32)(uint64_t, uint64_t, float);
#endif
  fg180q_st_f32(t257, t264, t265);
  }
  }
  }
  }
  }
  }
  t266 = v49;
  t267 = ((int32_t)(((int64_t)UINT64_C(1))));
  v49 = ((int32_t)((uint64_t)(t266) + (uint64_t)(t267)));
  }
  goto label1;
label2:;
  }
  }
  }
fg180qd_gg_done:;
}
static __device__ __forceinline__ void fg180qd_gg_function_22(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base) {
  uint64_t t0;
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
  uint64_t v0;
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
  t0 = ((uint64_t)(v0));
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
  fg180qd_gg_function_2(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, t19, base + 0);
  }
fg180qd_gg_done:;
}
static __device__ __forceinline__ void fg180qd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base) {
  uint64_t t0;
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
  uint64_t v0;
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
  t0 = ((uint64_t)(v0));
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
  fg180qd_gg_function_2(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, t19, base + 0);
  }
fg180qd_gg_done:;
}
