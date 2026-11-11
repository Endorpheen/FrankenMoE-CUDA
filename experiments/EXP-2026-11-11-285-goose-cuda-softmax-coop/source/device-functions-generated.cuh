#pragma once
static __device__ __forceinline__ void fg285dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg285dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg285dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg285dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg285dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg285dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ float fg285dd_gg_function_18(float p0, int64_t base);
static __device__ __forceinline__ float fg285dd_gg_function_17(float p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base);
static __device__ __forceinline__ float fg285dd_gg_function_9(float p0, int64_t base);
static __device__ __forceinline__ float fg285dd_gg_function_8(float p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base);
static __device__ __forceinline__ void fg285dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg285dd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t p4, int64_t p5, float p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t base);
static __device__ __forceinline__ void fg285dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg285dd_gg_div(int64_t a, int64_t b) { if (!b) fg285dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg285dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg285dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg285dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg285dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg285dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg285dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg285dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg285dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ float fg285dd_gg_function_18(float p0, int64_t base) {
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
  extern float (fg285d_shfl_xor)(float, int32_t);
#endif
  t5 = fg285d_shfl_xor(t3, t4);
  v3 = ((float)(t5));
  t6 = ((float)(v1));
  t7 = ((float)(v3));
  v1 = ((float)(((t6) + (t7))));
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  v2 = ((int64_t)(((int64_t)fg285dd_gg_div(t8, t9))));
  }
  goto label1;
label2:;
  t0 = ((float)(v1));
  }
fg285dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg285dd_gg_function_17(float p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base) {
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
  v4 = fg285dd_gg_function_18(t1, base + 0);
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t4 = ((t2) > (t3));
  if (t4) {
  {
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t7 = ((int64_t)(((int64_t)fg285dd_gg_mod(t5, t6))));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t10 = ((uint64_t)(v3));
  t11 = ((int64_t)(v2));
  t12 = ((int64_t)(((int64_t)UINT64_C(32))));
  t13 = ((int64_t)(((int64_t)fg285dd_gg_div(t11, t12))));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t15 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t13) * (uint64_t)(t14)))))));
  t16 = ((uint64_t)(((uint64_t)((uint64_t)(t10) + (uint64_t)(t15)))));
  t17 = ((float)(v4));
#ifndef __cplusplus
  extern void (fg285d_sstore)(uint64_t, float);
#endif
  fg285d_sstore(t16, t17);
  }
  }
#ifndef __cplusplus
  extern void (fg285d_sync)(void);
#endif
  fg285d_sync();
  v5 = ((float)(((float)(fg285dd_gg_double_bits(UINT64_C(0))))));
  t18 = ((int64_t)(v2));
  t19 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t20 = ((int64_t)(((int64_t)fg285dd_gg_mod(t18, t19))));
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t23 = ((int64_t)(((int64_t)fg285dd_gg_div(t21, t22))));
  t24 = ((t20) < (t23));
  if (t24) {
  {
  t25 = ((uint64_t)(v3));
  t26 = ((int64_t)(v2));
  t27 = ((int64_t)(((int64_t)UINT64_C(32))));
  t28 = ((int64_t)(((int64_t)fg285dd_gg_mod(t26, t27))));
  t29 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t30 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))))));
  t31 = ((uint64_t)(((uint64_t)((uint64_t)(t25) + (uint64_t)(t30)))));
#ifndef __cplusplus
  extern float (fg285d_sload)(uint64_t);
#endif
  t32 = fg285d_sload(t31);
  v5 = ((float)(t32));
  }
  }
  t33 = ((float)(v5));
  v4 = fg285dd_gg_function_18(t33, base + 0);
  }
  }
  t0 = ((float)(v4));
  }
fg285dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg285dd_gg_function_9(float p0, int64_t base) {
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
  extern float (fg285d_shfl_xor)(float, int32_t);
#endif
  t5 = fg285d_shfl_xor(t3, t4);
  v3 = ((float)(t5));
  t6 = ((float)(v1));
  t7 = ((float)(v3));
#ifndef __cplusplus
  extern float (fg285d_fmaxf)(float, float);
#endif
  t8 = fg285d_fmaxf(t6, t7);
  v1 = ((float)(t8));
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  v2 = ((int64_t)(((int64_t)fg285dd_gg_div(t9, t10))));
  }
  goto label1;
label2:;
  t0 = ((float)(v1));
  }
fg285dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg285dd_gg_function_8(float p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base) {
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
  v4 = fg285dd_gg_function_9(t1, base + 0);
  t2 = ((int64_t)(v1));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t4 = ((t2) > (t3));
  if (t4) {
  {
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t7 = ((int64_t)(((int64_t)fg285dd_gg_mod(t5, t6))));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t10 = ((uint64_t)(v3));
  t11 = ((int64_t)(v2));
  t12 = ((int64_t)(((int64_t)UINT64_C(32))));
  t13 = ((int64_t)(((int64_t)fg285dd_gg_div(t11, t12))));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t15 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t13) * (uint64_t)(t14)))))));
  t16 = ((uint64_t)(((uint64_t)((uint64_t)(t10) + (uint64_t)(t15)))));
  t17 = ((float)(v4));
#ifndef __cplusplus
  extern void (fg285d_sstore)(uint64_t, float);
#endif
  fg285d_sstore(t16, t17);
  }
  }
#ifndef __cplusplus
  extern void (fg285d_sync)(void);
#endif
  fg285d_sync();
#ifndef __cplusplus
  extern float (fg285d_ninf)(void);
#endif
  t18 = fg285d_ninf();
  v5 = ((float)(t18));
  t19 = ((int64_t)(v2));
  t20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t21 = ((int64_t)(((int64_t)fg285dd_gg_mod(t19, t20))));
  t22 = ((int64_t)(v1));
  t23 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t24 = ((int64_t)(((int64_t)fg285dd_gg_div(t22, t23))));
  t25 = ((t21) < (t24));
  if (t25) {
  {
  t26 = ((uint64_t)(v3));
  t27 = ((int64_t)(v2));
  t28 = ((int64_t)(((int64_t)UINT64_C(32))));
  t29 = ((int64_t)(((int64_t)fg285dd_gg_mod(t27, t28))));
  t30 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t31 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t29) * (uint64_t)(t30)))))));
  t32 = ((uint64_t)(((uint64_t)((uint64_t)(t26) + (uint64_t)(t31)))));
#ifndef __cplusplus
  extern float (fg285d_sload)(uint64_t);
#endif
  t33 = fg285d_sload(t32);
  v5 = ((float)(t33));
  }
  }
  t34 = ((float)(v5));
  v4 = fg285dd_gg_function_9(t34, base + 0);
  }
  }
  t0 = ((float)(v4));
  }
fg285dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg285dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg285d_fail)(int64_t);
#endif
  fg285d_fail(t2);
  }
  }
  }
fg285dd_gg_done:;
}
static __device__ __forceinline__ void fg285dd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t p4, int64_t p5, float p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t t10;
  uint8_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  int64_t t15;
  uint64_t t16;
  uint64_t t17;
  int64_t t18;
  int64_t t19;
  uint64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  uint64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  uint64_t t30;
  uint64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  uint64_t t36;
  float t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  float t44;
  float t45;
  float t46;
  float t47;
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
  int64_t t59;
  uint8_t t60;
  uint64_t t61;
  int64_t t62;
  int64_t t63;
  uint64_t t64;
  float t65;
  int64_t t66;
  int64_t t67;
  uint8_t t68;
  uint64_t t69;
  int64_t t70;
  int64_t t71;
  uint64_t t72;
  float t73;
  int64_t t74;
  int64_t t75;
  uint8_t t76;
  uint64_t t77;
  int64_t t78;
  int64_t t79;
  uint64_t t80;
  float t81;
  int64_t t82;
  int64_t t83;
  uint8_t t84;
  uint64_t t85;
  int64_t t86;
  int64_t t87;
  uint64_t t88;
  float t89;
  float t90;
  float t91;
  float t92;
  float t93;
  float t94;
  float t95;
  float t96;
  float t97;
  float t98;
  float t99;
  float t100;
  float t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  float t106;
  int64_t t107;
  int64_t t108;
  uint64_t t109;
  int64_t t110;
  int64_t t111;
  uint8_t t112;
  uint64_t t113;
  int64_t t114;
  int64_t t115;
  uint64_t t116;
  float t117;
  float t118;
  int64_t t119;
  int64_t t120;
  uint8_t t121;
  uint64_t t122;
  int64_t t123;
  int64_t t124;
  uint64_t t125;
  float t126;
  float t127;
  int64_t t128;
  int64_t t129;
  uint64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
  int64_t t136;
  float t137;
  float t138;
  float t139;
  float t140;
  int64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  uint8_t t153;
  uint64_t t154;
  int64_t t155;
  int64_t t156;
  uint64_t t157;
  float t158;
  int64_t t159;
  int64_t t160;
  uint8_t t161;
  uint64_t t162;
  int64_t t163;
  int64_t t164;
  uint64_t t165;
  float t166;
  int64_t t167;
  int64_t t168;
  uint8_t t169;
  uint64_t t170;
  int64_t t171;
  int64_t t172;
  uint64_t t173;
  float t174;
  int64_t t175;
  int64_t t176;
  uint8_t t177;
  uint64_t t178;
  int64_t t179;
  int64_t t180;
  uint64_t t181;
  float t182;
  int64_t t183;
  int64_t t184;
  uint8_t t185;
  float t186;
  float t187;
  float t188;
  float t189;
  float t190;
  float t191;
  uint64_t t192;
  int64_t t193;
  int64_t t194;
  uint64_t t195;
  float t196;
  int64_t t197;
  int64_t t198;
  uint8_t t199;
  float t200;
  float t201;
  float t202;
  float t203;
  float t204;
  float t205;
  uint64_t t206;
  int64_t t207;
  int64_t t208;
  uint64_t t209;
  float t210;
  int64_t t211;
  int64_t t212;
  uint8_t t213;
  float t214;
  float t215;
  float t216;
  float t217;
  float t218;
  float t219;
  uint64_t t220;
  int64_t t221;
  int64_t t222;
  uint64_t t223;
  float t224;
  int64_t t225;
  int64_t t226;
  uint8_t t227;
  float t228;
  float t229;
  float t230;
  float t231;
  float t232;
  float t233;
  uint64_t t234;
  int64_t t235;
  int64_t t236;
  uint64_t t237;
  float t238;
  int64_t t239;
  int64_t t240;
  int64_t t241;
  int64_t t242;
  float t243;
  int64_t t244;
  int64_t t245;
  uint64_t t246;
  int64_t t247;
  int64_t t248;
  uint8_t t249;
  uint64_t t250;
  int64_t t251;
  int64_t t252;
  uint64_t t253;
  float t254;
  int64_t t255;
  int64_t t256;
  uint8_t t257;
  uint64_t t258;
  int64_t t259;
  int64_t t260;
  uint64_t t261;
  float t262;
  float t263;
  int64_t t264;
  int64_t t265;
  uint64_t t266;
  int64_t t267;
  int64_t t268;
  int64_t t269;
  int64_t t270;
  int64_t t271;
  int64_t t272;
  float t273;
  float t274;
  float t275;
  float t276;
  int64_t t277;
  int64_t t278;
  int64_t t279;
  int64_t t280;
  int64_t t281;
  int64_t t282;
  int64_t t283;
  int64_t t284;
  int64_t t285;
  int64_t t286;
  int64_t t287;
  int64_t t288;
  uint8_t t289;
  uint64_t t290;
  int64_t t291;
  int64_t t292;
  uint64_t t293;
  float t294;
  int64_t t295;
  int64_t t296;
  uint8_t t297;
  uint64_t t298;
  int64_t t299;
  int64_t t300;
  uint64_t t301;
  float t302;
  int64_t t303;
  int64_t t304;
  uint8_t t305;
  uint64_t t306;
  int64_t t307;
  int64_t t308;
  uint64_t t309;
  float t310;
  int64_t t311;
  int64_t t312;
  uint8_t t313;
  uint64_t t314;
  int64_t t315;
  int64_t t316;
  uint64_t t317;
  float t318;
  int64_t t319;
  int64_t t320;
  uint8_t t321;
  uint64_t t322;
  int64_t t323;
  int64_t t324;
  uint64_t t325;
  float t326;
  float t327;
  float t328;
  int64_t t329;
  int64_t t330;
  uint8_t t331;
  uint64_t t332;
  int64_t t333;
  int64_t t334;
  uint64_t t335;
  float t336;
  float t337;
  float t338;
  int64_t t339;
  int64_t t340;
  uint8_t t341;
  uint64_t t342;
  int64_t t343;
  int64_t t344;
  uint64_t t345;
  float t346;
  float t347;
  float t348;
  int64_t t349;
  int64_t t350;
  uint8_t t351;
  uint64_t t352;
  int64_t t353;
  int64_t t354;
  uint64_t t355;
  float t356;
  float t357;
  float t358;
  int64_t t359;
  int64_t t360;
  int64_t t361;
  int64_t t362;
  int64_t t363;
  int64_t t364;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  int64_t v4;
  int64_t v5;
  float v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  uint64_t v11;
  uint64_t v12;
  uint64_t v13;
  int64_t v14;
  int64_t v15;
  uint64_t v16;
  uint64_t v17;
  float v18;
  int64_t v19;
  float v20;
  float v21;
  float v22;
  float v23;
  int64_t v24;
  int64_t v25;
  int64_t v26;
  int64_t v27;
  float v28;
  float v29;
  float v30;
  float v31;
  float v32;
  float v33;
  int64_t v34;
  int64_t v35;
  int64_t v36;
  int64_t v37;
  float v38;
  float v39;
  float v40;
  float v41;
  float v42;
  float v43;
  float v44;
  float v45;
  float v46;
  int64_t v47;
  int64_t v48;
  int64_t v49;
  int64_t v50;
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
  {
  t0 = ((int64_t)(v4));
  t1 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t2 = ((t0) > (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v5));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t4) > (t5));
  }
  t6 = t3;
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)(v7));
  t9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t7 = ((t8) > (t9));
  }
  t10 = t7;
  t11 = t10;
  if (t11) {
  t12 = ((int64_t)(v10));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t11 = ((t12) > (t13));
  }
  t14 = t11;
  t15 = ((int64_t)(((int64_t)UINT64_C(20))));
  fg285dd_gg_function_2(t14, t15, base + 0);
#ifndef __cplusplus
  extern uint64_t (fg285d_smem)(void);
#endif
  t16 = fg285d_smem();
  v11 = ((uint64_t)(t16));
  v12 = ((uint64_t)(v11));
  t17 = ((uint64_t)(v11));
  t18 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t19 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t20 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))))));
  v13 = ((uint64_t)(((uint64_t)((uint64_t)(t17) + (uint64_t)(t20)))));
  t21 = ((int64_t)(v10));
  t22 = ((int64_t)(v7));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t21) * (uint64_t)(t22)))));
  v15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label1:;
  t23 = ((int64_t)(v15));
  t24 = ((int64_t)(((int64_t)(v5))));
  if (!(((t23) < (t24)))) goto label2;
  {
  t25 = ((uint64_t)(v0));
  t26 = ((int64_t)(v15));
  t27 = ((int64_t)(v4));
  t28 = ((int64_t)(((int64_t)((uint64_t)(t26) * (uint64_t)(t27)))));
  t29 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t30 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))))));
  v16 = ((uint64_t)(((uint64_t)((uint64_t)(t25) + (uint64_t)(t30)))));
  t31 = ((uint64_t)(v1));
  t32 = ((int64_t)(v15));
  t33 = ((int64_t)(v4));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) * (uint64_t)(t33)))));
  t35 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t36 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t34) * (uint64_t)(t35)))))));
  v17 = ((uint64_t)(((uint64_t)((uint64_t)(t31) + (uint64_t)(t36)))));
#ifndef __cplusplus
  extern float (fg285d_ninf)(void);
#endif
  t37 = fg285d_ninf();
  v18 = ((float)(t37));
  t38 = ((int64_t)(v9));
  t39 = ((int64_t)(v7));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t38) * (uint64_t)(t39)))));
  t41 = ((int64_t)(v8));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t40) + (uint64_t)(t41)))));
label4:;
  t42 = ((int64_t)(v19));
  t43 = ((int64_t)(((int64_t)(v4))));
  if (!(((t42) < (t43)))) goto label5;
  {
#ifndef __cplusplus
  extern float (fg285d_ninf)(void);
#endif
  t44 = fg285d_ninf();
  v20 = ((float)(t44));
#ifndef __cplusplus
  extern float (fg285d_ninf)(void);
#endif
  t45 = fg285d_ninf();
  v21 = ((float)(t45));
#ifndef __cplusplus
  extern float (fg285d_ninf)(void);
#endif
  t46 = fg285d_ninf();
  v22 = ((float)(t46));
#ifndef __cplusplus
  extern float (fg285d_ninf)(void);
#endif
  t47 = fg285d_ninf();
  v23 = ((float)(t47));
  v24 = ((int64_t)(v19));
  t48 = ((int64_t)(v19));
  t49 = ((int64_t)(v14));
  v25 = ((int64_t)(((int64_t)((uint64_t)(t48) + (uint64_t)(t49)))));
  t50 = ((int64_t)(v19));
  t51 = ((int64_t)(((int64_t)UINT64_C(2))));
  t52 = ((int64_t)(((int64_t)(v14))));
  t53 = ((int64_t)(((int64_t)((uint64_t)(t51) * (uint64_t)(t52)))));
  v26 = ((int64_t)(((int64_t)((uint64_t)(t50) + (uint64_t)(t53)))));
  t54 = ((int64_t)(v19));
  t55 = ((int64_t)(((int64_t)UINT64_C(3))));
  t56 = ((int64_t)(((int64_t)(v14))));
  t57 = ((int64_t)(((int64_t)((uint64_t)(t55) * (uint64_t)(t56)))));
  v27 = ((int64_t)(((int64_t)((uint64_t)(t54) + (uint64_t)(t57)))));
  t58 = ((int64_t)(v24));
  t59 = ((int64_t)(((int64_t)(v4))));
  t60 = ((t58) < (t59));
  if (t60) {
  {
  t61 = ((uint64_t)(v16));
  t62 = ((int64_t)(v24));
  t63 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t64 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t62) * (uint64_t)(t63)))))));
#ifndef __cplusplus
  extern float (fg285d_ld_f32)(uint64_t, uint64_t);
#endif
  t65 = fg285d_ld_f32(t61, t64);
  v20 = ((float)(t65));
  }
  }
  t66 = ((int64_t)(v25));
  t67 = ((int64_t)(((int64_t)(v4))));
  t68 = ((t66) < (t67));
  if (t68) {
  {
  t69 = ((uint64_t)(v16));
  t70 = ((int64_t)(v25));
  t71 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t72 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t70) * (uint64_t)(t71)))))));
#ifndef __cplusplus
  extern float (fg285d_ld_f32)(uint64_t, uint64_t);
#endif
  t73 = fg285d_ld_f32(t69, t72);
  v21 = ((float)(t73));
  }
  }
  t74 = ((int64_t)(v26));
  t75 = ((int64_t)(((int64_t)(v4))));
  t76 = ((t74) < (t75));
  if (t76) {
  {
  t77 = ((uint64_t)(v16));
  t78 = ((int64_t)(v26));
  t79 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t80 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t78) * (uint64_t)(t79)))))));
#ifndef __cplusplus
  extern float (fg285d_ld_f32)(uint64_t, uint64_t);
#endif
  t81 = fg285d_ld_f32(t77, t80);
  v22 = ((float)(t81));
  }
  }
  t82 = ((int64_t)(v27));
  t83 = ((int64_t)(((int64_t)(v4))));
  t84 = ((t82) < (t83));
  if (t84) {
  {
  t85 = ((uint64_t)(v16));
  t86 = ((int64_t)(v27));
  t87 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t88 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t86) * (uint64_t)(t87)))))));
#ifndef __cplusplus
  extern float (fg285d_ld_f32)(uint64_t, uint64_t);
#endif
  t89 = fg285d_ld_f32(t85, t88);
  v23 = ((float)(t89));
  }
  }
  t90 = ((float)(v18));
  t91 = ((float)(v20));
#ifndef __cplusplus
  extern float (fg285d_fmaxf)(float, float);
#endif
  t92 = fg285d_fmaxf(t90, t91);
  v18 = ((float)(t92));
  t93 = ((float)(v18));
  t94 = ((float)(v21));
#ifndef __cplusplus
  extern float (fg285d_fmaxf)(float, float);
#endif
  t95 = fg285d_fmaxf(t93, t94);
  v18 = ((float)(t95));
  t96 = ((float)(v18));
  t97 = ((float)(v22));
#ifndef __cplusplus
  extern float (fg285d_fmaxf)(float, float);
#endif
  t98 = fg285d_fmaxf(t96, t97);
  v18 = ((float)(t98));
  t99 = ((float)(v18));
  t100 = ((float)(v23));
#ifndef __cplusplus
  extern float (fg285d_fmaxf)(float, float);
#endif
  t101 = fg285d_fmaxf(t99, t100);
  v18 = ((float)(t101));
  t102 = ((int64_t)(v19));
  t103 = ((int64_t)(v14));
  t104 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t105 = ((int64_t)(((int64_t)((uint64_t)(t103) * (uint64_t)(t104)))));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t102) + (uint64_t)(t105)))));
  }
  goto label4;
label5:;
  t106 = ((float)(v18));
  t107 = ((int64_t)(v7));
  t108 = ((int64_t)(v8));
  t109 = ((uint64_t)(v12));
  v18 = fg285dd_gg_function_8(t106, t107, t108, t109, base + 0);
  t110 = ((int64_t)(v8));
  t111 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t112 = ((t110) == (t111));
  if (t112) {
  {
  t113 = ((uint64_t)(v2));
  t114 = ((int64_t)(v9));
  t115 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t116 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t114) * (uint64_t)(t115)))))));
  t117 = ((float)(v18));
#ifndef __cplusplus
  extern void (fg285d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg285d_st_f32(t113, t116, t117);
  }
  }
#ifndef __cplusplus
  extern void (fg285d_gridsync)(void);
#endif
  fg285d_gridsync();
#ifndef __cplusplus
  extern float (fg285d_ninf)(void);
#endif
  t118 = fg285d_ninf();
  v28 = ((float)(t118));
  t119 = ((int64_t)(v8));
  t120 = ((int64_t)(((int64_t)(v10))));
  t121 = ((t119) < (t120));
  if (t121) {
  {
  t122 = ((uint64_t)(v2));
  t123 = ((int64_t)(v8));
  t124 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t125 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t123) * (uint64_t)(t124)))))));
#ifndef __cplusplus
  extern float (fg285d_ld_f32)(uint64_t, uint64_t);
#endif
  t126 = fg285d_ld_f32(t122, t125);
  v28 = ((float)(t126));
  }
  }
  t127 = ((float)(v28));
  t128 = ((int64_t)(v7));
  t129 = ((int64_t)(v8));
  t130 = ((uint64_t)(v12));
  v28 = fg285dd_gg_function_8(t127, t128, t129, t130, base + 0);
  v29 = ((float)(((float)(fg285dd_gg_double_bits(UINT64_C(0))))));
  t131 = ((int64_t)(v9));
  t132 = ((int64_t)(v7));
  t133 = ((int64_t)(((int64_t)((uint64_t)(t131) * (uint64_t)(t132)))));
  t134 = ((int64_t)(v8));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t133) + (uint64_t)(t134)))));
label7:;
  t135 = ((int64_t)(v19));
  t136 = ((int64_t)(((int64_t)(v4))));
  if (!(((t135) < (t136)))) goto label8;
  {
#ifndef __cplusplus
  extern float (fg285d_ninf)(void);
#endif
  t137 = fg285d_ninf();
  v30 = ((float)(t137));
#ifndef __cplusplus
  extern float (fg285d_ninf)(void);
#endif
  t138 = fg285d_ninf();
  v31 = ((float)(t138));
#ifndef __cplusplus
  extern float (fg285d_ninf)(void);
#endif
  t139 = fg285d_ninf();
  v32 = ((float)(t139));
#ifndef __cplusplus
  extern float (fg285d_ninf)(void);
#endif
  t140 = fg285d_ninf();
  v33 = ((float)(t140));
  v34 = ((int64_t)(v19));
  t141 = ((int64_t)(v19));
  t142 = ((int64_t)(v14));
  v35 = ((int64_t)(((int64_t)((uint64_t)(t141) + (uint64_t)(t142)))));
  t143 = ((int64_t)(v19));
  t144 = ((int64_t)(((int64_t)UINT64_C(2))));
  t145 = ((int64_t)(((int64_t)(v14))));
  t146 = ((int64_t)(((int64_t)((uint64_t)(t144) * (uint64_t)(t145)))));
  v36 = ((int64_t)(((int64_t)((uint64_t)(t143) + (uint64_t)(t146)))));
  t147 = ((int64_t)(v19));
  t148 = ((int64_t)(((int64_t)UINT64_C(3))));
  t149 = ((int64_t)(((int64_t)(v14))));
  t150 = ((int64_t)(((int64_t)((uint64_t)(t148) * (uint64_t)(t149)))));
  v37 = ((int64_t)(((int64_t)((uint64_t)(t147) + (uint64_t)(t150)))));
  t151 = ((int64_t)(v34));
  t152 = ((int64_t)(((int64_t)(v4))));
  t153 = ((t151) < (t152));
  if (t153) {
  {
  t154 = ((uint64_t)(v16));
  t155 = ((int64_t)(v34));
  t156 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t157 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t155) * (uint64_t)(t156)))))));
#ifndef __cplusplus
  extern float (fg285d_ld_f32)(uint64_t, uint64_t);
#endif
  t158 = fg285d_ld_f32(t154, t157);
  v30 = ((float)(t158));
  }
  }
  t159 = ((int64_t)(v35));
  t160 = ((int64_t)(((int64_t)(v4))));
  t161 = ((t159) < (t160));
  if (t161) {
  {
  t162 = ((uint64_t)(v16));
  t163 = ((int64_t)(v35));
  t164 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t165 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t163) * (uint64_t)(t164)))))));
#ifndef __cplusplus
  extern float (fg285d_ld_f32)(uint64_t, uint64_t);
#endif
  t166 = fg285d_ld_f32(t162, t165);
  v31 = ((float)(t166));
  }
  }
  t167 = ((int64_t)(v36));
  t168 = ((int64_t)(((int64_t)(v4))));
  t169 = ((t167) < (t168));
  if (t169) {
  {
  t170 = ((uint64_t)(v16));
  t171 = ((int64_t)(v36));
  t172 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t173 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t171) * (uint64_t)(t172)))))));
#ifndef __cplusplus
  extern float (fg285d_ld_f32)(uint64_t, uint64_t);
#endif
  t174 = fg285d_ld_f32(t170, t173);
  v32 = ((float)(t174));
  }
  }
  t175 = ((int64_t)(v37));
  t176 = ((int64_t)(((int64_t)(v4))));
  t177 = ((t175) < (t176));
  if (t177) {
  {
  t178 = ((uint64_t)(v16));
  t179 = ((int64_t)(v37));
  t180 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t181 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t179) * (uint64_t)(t180)))))));
#ifndef __cplusplus
  extern float (fg285d_ld_f32)(uint64_t, uint64_t);
#endif
  t182 = fg285d_ld_f32(t178, t181);
  v33 = ((float)(t182));
  }
  }
  t183 = ((int64_t)(v34));
  t184 = ((int64_t)(((int64_t)(v4))));
  t185 = ((t183) < (t184));
  if (t185) {
  {
  t186 = ((float)(v30));
  t187 = ((float)(v28));
  t188 = ((float)(((t186) - (t187))));
#ifndef __cplusplus
  extern float (fg285d_expf)(float);
#endif
  t189 = fg285d_expf(t188);
  v38 = ((float)(t189));
  t190 = ((float)(v29));
  t191 = ((float)(v38));
  v29 = ((float)(((t190) + (t191))));
  t192 = ((uint64_t)(v17));
  t193 = ((int64_t)(v34));
  t194 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t195 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t193) * (uint64_t)(t194)))))));
  t196 = ((float)(v38));
#ifndef __cplusplus
  extern void (fg285d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg285d_st_f32(t192, t195, t196);
  }
  }
  t197 = ((int64_t)(v35));
  t198 = ((int64_t)(((int64_t)(v4))));
  t199 = ((t197) < (t198));
  if (t199) {
  {
  t200 = ((float)(v31));
  t201 = ((float)(v28));
  t202 = ((float)(((t200) - (t201))));
#ifndef __cplusplus
  extern float (fg285d_expf)(float);
#endif
  t203 = fg285d_expf(t202);
  v39 = ((float)(t203));
  t204 = ((float)(v29));
  t205 = ((float)(v39));
  v29 = ((float)(((t204) + (t205))));
  t206 = ((uint64_t)(v17));
  t207 = ((int64_t)(v35));
  t208 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t209 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t207) * (uint64_t)(t208)))))));
  t210 = ((float)(v39));
#ifndef __cplusplus
  extern void (fg285d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg285d_st_f32(t206, t209, t210);
  }
  }
  t211 = ((int64_t)(v36));
  t212 = ((int64_t)(((int64_t)(v4))));
  t213 = ((t211) < (t212));
  if (t213) {
  {
  t214 = ((float)(v32));
  t215 = ((float)(v28));
  t216 = ((float)(((t214) - (t215))));
#ifndef __cplusplus
  extern float (fg285d_expf)(float);
#endif
  t217 = fg285d_expf(t216);
  v40 = ((float)(t217));
  t218 = ((float)(v29));
  t219 = ((float)(v40));
  v29 = ((float)(((t218) + (t219))));
  t220 = ((uint64_t)(v17));
  t221 = ((int64_t)(v36));
  t222 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t223 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t221) * (uint64_t)(t222)))))));
  t224 = ((float)(v40));
#ifndef __cplusplus
  extern void (fg285d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg285d_st_f32(t220, t223, t224);
  }
  }
  t225 = ((int64_t)(v37));
  t226 = ((int64_t)(((int64_t)(v4))));
  t227 = ((t225) < (t226));
  if (t227) {
  {
  t228 = ((float)(v33));
  t229 = ((float)(v28));
  t230 = ((float)(((t228) - (t229))));
#ifndef __cplusplus
  extern float (fg285d_expf)(float);
#endif
  t231 = fg285d_expf(t230);
  v41 = ((float)(t231));
  t232 = ((float)(v29));
  t233 = ((float)(v41));
  v29 = ((float)(((t232) + (t233))));
  t234 = ((uint64_t)(v17));
  t235 = ((int64_t)(v37));
  t236 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t237 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t235) * (uint64_t)(t236)))))));
  t238 = ((float)(v41));
#ifndef __cplusplus
  extern void (fg285d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg285d_st_f32(t234, t237, t238);
  }
  }
  t239 = ((int64_t)(v19));
  t240 = ((int64_t)(v14));
  t241 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t242 = ((int64_t)(((int64_t)((uint64_t)(t240) * (uint64_t)(t241)))));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t239) + (uint64_t)(t242)))));
  }
  goto label7;
label8:;
  t243 = ((float)(v29));
  t244 = ((int64_t)(v7));
  t245 = ((int64_t)(v8));
  t246 = ((uint64_t)(v13));
  v29 = fg285dd_gg_function_17(t243, t244, t245, t246, base + 0);
  t247 = ((int64_t)(v8));
  t248 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t249 = ((t247) == (t248));
  if (t249) {
  {
  t250 = ((uint64_t)(v3));
  t251 = ((int64_t)(v9));
  t252 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t253 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t251) * (uint64_t)(t252)))))));
  t254 = ((float)(v29));
#ifndef __cplusplus
  extern void (fg285d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg285d_st_f32(t250, t253, t254);
  }
  }
#ifndef __cplusplus
  extern void (fg285d_gridsync)(void);
#endif
  fg285d_gridsync();
  v42 = ((float)(((float)(fg285dd_gg_double_bits(UINT64_C(0))))));
  t255 = ((int64_t)(v8));
  t256 = ((int64_t)(((int64_t)(v10))));
  t257 = ((t255) < (t256));
  if (t257) {
  {
  t258 = ((uint64_t)(v3));
  t259 = ((int64_t)(v8));
  t260 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t261 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t259) * (uint64_t)(t260)))))));
#ifndef __cplusplus
  extern float (fg285d_ld_f32)(uint64_t, uint64_t);
#endif
  t262 = fg285d_ld_f32(t258, t261);
  v42 = ((float)(t262));
  }
  }
  t263 = ((float)(v42));
  t264 = ((int64_t)(v7));
  t265 = ((int64_t)(v8));
  t266 = ((uint64_t)(v13));
  v42 = fg285dd_gg_function_17(t263, t264, t265, t266, base + 0);
  t267 = ((int64_t)(v9));
  t268 = ((int64_t)(v7));
  t269 = ((int64_t)(((int64_t)((uint64_t)(t267) * (uint64_t)(t268)))));
  t270 = ((int64_t)(v8));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t269) + (uint64_t)(t270)))));
label10:;
  t271 = ((int64_t)(v19));
  t272 = ((int64_t)(((int64_t)(v4))));
  if (!(((t271) < (t272)))) goto label11;
  {
#ifndef __cplusplus
  extern float (fg285d_ninf)(void);
#endif
  t273 = fg285d_ninf();
  v43 = ((float)(t273));
#ifndef __cplusplus
  extern float (fg285d_ninf)(void);
#endif
  t274 = fg285d_ninf();
  v44 = ((float)(t274));
#ifndef __cplusplus
  extern float (fg285d_ninf)(void);
#endif
  t275 = fg285d_ninf();
  v45 = ((float)(t275));
#ifndef __cplusplus
  extern float (fg285d_ninf)(void);
#endif
  t276 = fg285d_ninf();
  v46 = ((float)(t276));
  v47 = ((int64_t)(v19));
  t277 = ((int64_t)(v19));
  t278 = ((int64_t)(v14));
  v48 = ((int64_t)(((int64_t)((uint64_t)(t277) + (uint64_t)(t278)))));
  t279 = ((int64_t)(v19));
  t280 = ((int64_t)(((int64_t)UINT64_C(2))));
  t281 = ((int64_t)(((int64_t)(v14))));
  t282 = ((int64_t)(((int64_t)((uint64_t)(t280) * (uint64_t)(t281)))));
  v49 = ((int64_t)(((int64_t)((uint64_t)(t279) + (uint64_t)(t282)))));
  t283 = ((int64_t)(v19));
  t284 = ((int64_t)(((int64_t)UINT64_C(3))));
  t285 = ((int64_t)(((int64_t)(v14))));
  t286 = ((int64_t)(((int64_t)((uint64_t)(t284) * (uint64_t)(t285)))));
  v50 = ((int64_t)(((int64_t)((uint64_t)(t283) + (uint64_t)(t286)))));
  t287 = ((int64_t)(v47));
  t288 = ((int64_t)(((int64_t)(v4))));
  t289 = ((t287) < (t288));
  if (t289) {
  {
  t290 = ((uint64_t)(v17));
  t291 = ((int64_t)(v47));
  t292 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t293 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t291) * (uint64_t)(t292)))))));
#ifndef __cplusplus
  extern float (fg285d_ld_f32)(uint64_t, uint64_t);
#endif
  t294 = fg285d_ld_f32(t290, t293);
  v43 = ((float)(t294));
  }
  }
  t295 = ((int64_t)(v48));
  t296 = ((int64_t)(((int64_t)(v4))));
  t297 = ((t295) < (t296));
  if (t297) {
  {
  t298 = ((uint64_t)(v17));
  t299 = ((int64_t)(v48));
  t300 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t301 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t299) * (uint64_t)(t300)))))));
#ifndef __cplusplus
  extern float (fg285d_ld_f32)(uint64_t, uint64_t);
#endif
  t302 = fg285d_ld_f32(t298, t301);
  v44 = ((float)(t302));
  }
  }
  t303 = ((int64_t)(v49));
  t304 = ((int64_t)(((int64_t)(v4))));
  t305 = ((t303) < (t304));
  if (t305) {
  {
  t306 = ((uint64_t)(v17));
  t307 = ((int64_t)(v49));
  t308 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t309 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t307) * (uint64_t)(t308)))))));
#ifndef __cplusplus
  extern float (fg285d_ld_f32)(uint64_t, uint64_t);
#endif
  t310 = fg285d_ld_f32(t306, t309);
  v45 = ((float)(t310));
  }
  }
  t311 = ((int64_t)(v50));
  t312 = ((int64_t)(((int64_t)(v4))));
  t313 = ((t311) < (t312));
  if (t313) {
  {
  t314 = ((uint64_t)(v17));
  t315 = ((int64_t)(v50));
  t316 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t317 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t315) * (uint64_t)(t316)))))));
#ifndef __cplusplus
  extern float (fg285d_ld_f32)(uint64_t, uint64_t);
#endif
  t318 = fg285d_ld_f32(t314, t317);
  v46 = ((float)(t318));
  }
  }
  t319 = ((int64_t)(v47));
  t320 = ((int64_t)(((int64_t)(v4))));
  t321 = ((t319) < (t320));
  if (t321) {
  {
  t322 = ((uint64_t)(v17));
  t323 = ((int64_t)(v47));
  t324 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t325 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t323) * (uint64_t)(t324)))))));
  t326 = ((float)(v43));
  t327 = ((float)(v42));
  t328 = ((float)(((t326) / (t327))));
#ifndef __cplusplus
  extern void (fg285d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg285d_st_f32(t322, t325, t328);
  }
  }
  t329 = ((int64_t)(v48));
  t330 = ((int64_t)(((int64_t)(v4))));
  t331 = ((t329) < (t330));
  if (t331) {
  {
  t332 = ((uint64_t)(v17));
  t333 = ((int64_t)(v48));
  t334 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t335 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t333) * (uint64_t)(t334)))))));
  t336 = ((float)(v44));
  t337 = ((float)(v42));
  t338 = ((float)(((t336) / (t337))));
#ifndef __cplusplus
  extern void (fg285d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg285d_st_f32(t332, t335, t338);
  }
  }
  t339 = ((int64_t)(v49));
  t340 = ((int64_t)(((int64_t)(v4))));
  t341 = ((t339) < (t340));
  if (t341) {
  {
  t342 = ((uint64_t)(v17));
  t343 = ((int64_t)(v49));
  t344 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t345 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t343) * (uint64_t)(t344)))))));
  t346 = ((float)(v45));
  t347 = ((float)(v42));
  t348 = ((float)(((t346) / (t347))));
#ifndef __cplusplus
  extern void (fg285d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg285d_st_f32(t342, t345, t348);
  }
  }
  t349 = ((int64_t)(v50));
  t350 = ((int64_t)(((int64_t)(v4))));
  t351 = ((t349) < (t350));
  if (t351) {
  {
  t352 = ((uint64_t)(v17));
  t353 = ((int64_t)(v50));
  t354 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t355 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t353) * (uint64_t)(t354)))))));
  t356 = ((float)(v46));
  t357 = ((float)(v42));
  t358 = ((float)(((t356) / (t357))));
#ifndef __cplusplus
  extern void (fg285d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg285d_st_f32(t352, t355, t358);
  }
  }
  t359 = ((int64_t)(v19));
  t360 = ((int64_t)(v14));
  t361 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t362 = ((int64_t)(((int64_t)((uint64_t)(t360) * (uint64_t)(t361)))));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t359) + (uint64_t)(t362)))));
  }
  goto label10;
label11:;
  t363 = ((int64_t)(v15));
  t364 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t363) + (uint64_t)(t364)))));
  }
  goto label1;
label2:;
  }
fg285dd_gg_done:;
}
