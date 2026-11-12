#pragma once
static __device__ __forceinline__ void fg279dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg279dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg279dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg279dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg279dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg279dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ float fg279dd_gg_function_8(float p0, int64_t p1, int64_t p2, int64_t base);
static __device__ __forceinline__ void fg279dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg279dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t base);
static __device__ __forceinline__ void fg279dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg279dd_gg_div(int64_t a, int64_t b) { if (!b) fg279dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg279dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg279dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg279dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg279dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg279dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg279dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg279dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg279dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ float fg279dd_gg_function_8(float p0, int64_t p1, int64_t p2, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  float t3;
  int32_t t4;
  float t5;
  int64_t t6;
  int64_t t7;
  uint8_t t8;
  float t9;
  float t10;
  int64_t t11;
  int64_t t12;
  float v0;
  int64_t v1;
  int64_t v2;
  float v3;
  int64_t v4;
  float v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  v3 = ((float)(v0));
  v4 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
label1:;
  t1 = ((int64_t)(v4));
  t2 = ((int64_t)(v2));
  if (!(((t1) < (t2)))) goto label2;
  {
  t3 = ((float)(v3));
  t4 = ((int32_t)(((int32_t)(v4))));
#ifndef __cplusplus
  extern float (fg279d_shfl_up)(float, int32_t);
#endif
  t5 = fg279d_shfl_up(t3, t4);
  v5 = ((float)(t5));
  t6 = ((int64_t)(v1));
  t7 = ((int64_t)(v4));
  t8 = ((t6) >= (t7));
  if (t8) {
  {
  t9 = ((float)(v3));
  t10 = ((float)(v5));
  v3 = ((float)(((t9) + (t10))));
  }
  }
  t11 = ((int64_t)(v4));
  t12 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  }
  goto label1;
label2:;
  t0 = ((float)(v3));
  }
fg279dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg279dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg279d_fail)(int64_t);
#endif
  fg279d_fail(t2);
  }
  }
  }
fg279dd_gg_done:;
}
static __device__ __forceinline__ void fg279dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t base) {
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
  uint64_t t14;
  uint64_t t15;
  int64_t t16;
  int64_t t17;
  uint64_t t18;
  uint64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  uint64_t t24;
  uint64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  uint64_t t32;
  int64_t t33;
  int64_t t34;
  uint8_t t35;
  uint64_t t36;
  float t37;
  int64_t t38;
  int64_t t39;
  uint8_t t40;
  int64_t t41;
  int64_t t42;
  uint8_t t43;
  int64_t t44;
  int64_t t45;
  uint8_t t46;
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
  int64_t t72;
  int64_t t73;
  int64_t t74;
  uint8_t t75;
  uint64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  uint64_t t81;
  float t82;
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  uint8_t t87;
  float t88;
  uint64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  uint64_t t96;
  float t97;
  float t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  uint8_t t103;
  float t104;
  uint64_t t105;
  int64_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  uint64_t t112;
  float t113;
  float t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  uint8_t t119;
  float t120;
  uint64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  uint64_t t128;
  float t129;
  float t130;
  int64_t t131;
  int64_t t132;
  uint8_t t133;
  float t134;
  int64_t t135;
  int64_t t136;
  uint64_t t137;
  int64_t t138;
  int64_t t139;
  uint64_t t140;
  uint64_t t141;
  float t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  uint8_t t147;
  uint64_t t148;
  int64_t t149;
  int64_t t150;
  uint64_t t151;
  uint64_t t152;
  float t153;
  int64_t t154;
  int64_t t155;
  uint8_t t156;
  int64_t t157;
  int64_t t158;
  uint8_t t159;
  uint64_t t160;
  int64_t t161;
  int64_t t162;
  uint64_t t163;
  uint64_t t164;
  float t165;
  float t166;
  int64_t t167;
  int64_t t168;
  int64_t t169;
  int64_t t170;
  uint8_t t171;
  uint64_t t172;
  int64_t t173;
  int64_t t174;
  uint64_t t175;
  uint64_t t176;
  float t177;
  float t178;
  float t179;
  int64_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  uint8_t t184;
  uint64_t t185;
  float t186;
  uint64_t t187;
  float t188;
  uint64_t t189;
  int64_t t190;
  int64_t t191;
  uint64_t t192;
  uint64_t t193;
  float t194;
  uint64_t t195;
  int64_t t196;
  int64_t t197;
  uint64_t t198;
  uint64_t t199;
  float t200;
  float t201;
  float t202;
  float t203;
  float t204;
  float t205;
  float t206;
  int64_t t207;
  int64_t t208;
  uint8_t t209;
  uint64_t t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  uint64_t t215;
  float t216;
  float t217;
  float t218;
  int64_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  uint8_t t223;
  uint64_t t224;
  int64_t t225;
  int64_t t226;
  int64_t t227;
  int64_t t228;
  int64_t t229;
  int64_t t230;
  uint64_t t231;
  float t232;
  float t233;
  float t234;
  int64_t t235;
  int64_t t236;
  int64_t t237;
  int64_t t238;
  uint8_t t239;
  uint64_t t240;
  int64_t t241;
  int64_t t242;
  int64_t t243;
  int64_t t244;
  int64_t t245;
  int64_t t246;
  uint64_t t247;
  float t248;
  float t249;
  float t250;
  int64_t t251;
  int64_t t252;
  int64_t t253;
  int64_t t254;
  uint8_t t255;
  uint64_t t256;
  int64_t t257;
  int64_t t258;
  int64_t t259;
  int64_t t260;
  int64_t t261;
  int64_t t262;
  uint64_t t263;
  float t264;
  float t265;
  float t266;
  int64_t t267;
  int64_t t268;
  uint8_t t269;
  uint64_t t270;
  uint64_t t271;
  float t272;
  float t273;
  uint64_t t274;
  float t275;
  float t276;
  float t277;
  int64_t t278;
  int64_t t279;
  int64_t t280;
  int64_t t281;
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
  uint64_t v22;
  uint64_t v23;
  uint64_t v24;
  uint64_t v25;
  int64_t v26;
  int64_t v27;
  int64_t v28;
  int64_t v29;
  float v30;
  float v31;
  float v32;
  float v33;
  float v34;
  float v35;
  float v36;
  float v37;
  float v38;
  float v39;
  float v40;
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
  t1 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t2 = ((t0) > (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v17));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t4) > (t5));
  }
  t6 = t3;
  t7 = ((int64_t)(((int64_t)UINT64_C(20))));
  fg279dd_gg_function_2(t6, t7, base + 0);
  t8 = ((int64_t)(v13));
  t9 = ((int64_t)(v17));
  v18 = ((int64_t)(((int64_t)fg279dd_gg_mod(t8, t9))));
  t10 = ((int64_t)(v13));
  t11 = ((int64_t)(v17));
  v19 = ((int64_t)(((int64_t)fg279dd_gg_div(t10, t11))));
  t12 = ((int64_t)(v12));
  t13 = ((int64_t)(v17));
  v20 = ((int64_t)(((int64_t)fg279dd_gg_div(t12, t13))));
#ifndef __cplusplus
  extern uint64_t (fg279d_smem)(void);
#endif
  t14 = fg279d_smem();
  v21 = ((uint64_t)(t14));
  v22 = ((uint64_t)(v21));
  t15 = ((uint64_t)(v21));
  t16 = ((int64_t)(v12));
  t17 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t18 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t17)))))));
  v23 = ((uint64_t)(((uint64_t)((uint64_t)(t15) + (uint64_t)(t18)))));
  t19 = ((uint64_t)(v21));
  t20 = ((int64_t)(v12));
  t21 = ((int64_t)(v20));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) + (uint64_t)(t21)))));
  t23 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t24 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t22) * (uint64_t)(t23)))))));
  v24 = ((uint64_t)(((uint64_t)((uint64_t)(t19) + (uint64_t)(t24)))));
  t25 = ((uint64_t)(v21));
  t26 = ((int64_t)(v12));
  t27 = ((int64_t)(v20));
  t28 = ((int64_t)(((int64_t)((uint64_t)(t26) + (uint64_t)(t27)))));
  t29 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) + (uint64_t)(t29)))));
  t31 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t32 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t30) * (uint64_t)(t31)))))));
  v25 = ((uint64_t)(((uint64_t)((uint64_t)(t25) + (uint64_t)(t32)))));
  t33 = ((int64_t)(v13));
  t34 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t35 = ((t33) == (t34));
  if (t35) {
  {
  t36 = ((uint64_t)(v24));
  t37 = ((float)(((float)(fg279dd_gg_double_bits(UINT64_C(0))))));
#ifndef __cplusplus
  extern void (fg279d_sstore)(uint64_t, float);
#endif
  fg279d_sstore(t36, t37);
  }
  }
#ifndef __cplusplus
  extern void (fg279d_sync)(void);
#endif
  fg279d_sync();
  t38 = ((int64_t)(v16));
  t39 = ((int64_t)(((int64_t)(v5))));
  t40 = ((t38) >= (t39));
  if (t40) {
  {
  goto fg279dd_gg_done;
  }
  }
  t41 = ((int64_t)(v15));
  t42 = ((int64_t)(((int64_t)(v4))));
  t43 = ((t41) >= (t42));
  if (t43) {
  {
  goto fg279dd_gg_done;
  }
  }
  t44 = ((int64_t)(v14));
  t45 = ((int64_t)(((int64_t)(v3))));
  t46 = ((t44) >= (t45));
  if (t46) {
  {
  goto fg279dd_gg_done;
  }
  }
  t47 = ((int64_t)(v14));
  t48 = ((int64_t)(v6));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t47) * (uint64_t)(t48)))));
  t50 = ((int64_t)(v15));
  t51 = ((int64_t)(v7));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t50) * (uint64_t)(t51)))));
  t53 = ((int64_t)(((int64_t)((uint64_t)(t49) + (uint64_t)(t52)))));
  t54 = ((int64_t)(v16));
  t55 = ((int64_t)(v8));
  t56 = ((int64_t)(((int64_t)((uint64_t)(t54) * (uint64_t)(t55)))));
  v26 = ((int64_t)(((int64_t)((uint64_t)(t53) + (uint64_t)(t56)))));
  t57 = ((int64_t)(v14));
  t58 = ((int64_t)(v9));
  t59 = ((int64_t)(((int64_t)((uint64_t)(t57) * (uint64_t)(t58)))));
  t60 = ((int64_t)(v15));
  t61 = ((int64_t)(v10));
  t62 = ((int64_t)(((int64_t)((uint64_t)(t60) * (uint64_t)(t61)))));
  t63 = ((int64_t)(((int64_t)((uint64_t)(t59) + (uint64_t)(t62)))));
  t64 = ((int64_t)(v16));
  t65 = ((int64_t)(v11));
  t66 = ((int64_t)(((int64_t)((uint64_t)(t64) * (uint64_t)(t65)))));
  v27 = ((int64_t)(((int64_t)((uint64_t)(t63) + (uint64_t)(t66)))));
  v28 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label1:;
  t67 = ((int64_t)(v28));
  t68 = ((int64_t)(v2));
  if (!(((t67) < (t68)))) goto label2;
  {
  t69 = ((int64_t)(v28));
  t70 = ((int64_t)(v13));
  t71 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t72 = ((int64_t)(((int64_t)((uint64_t)(t70) * (uint64_t)(t71)))));
  v29 = ((int64_t)(((int64_t)((uint64_t)(t69) + (uint64_t)(t72)))));
  v30 = ((float)(((float)(fg279dd_gg_double_bits(UINT64_C(0))))));
  t73 = ((int64_t)(v29));
  t74 = ((int64_t)(((int64_t)(v2))));
  t75 = ((t73) < (t74));
  if (t75) {
  {
  t76 = ((uint64_t)(v0));
  t77 = ((int64_t)(v26));
  t78 = ((int64_t)(v29));
  t79 = ((int64_t)(((int64_t)((uint64_t)(t77) + (uint64_t)(t78)))));
  t80 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t81 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t79) * (uint64_t)(t80)))))));
#ifndef __cplusplus
  extern float (fg279d_ld_f32)(uint64_t, uint64_t);
#endif
  t82 = fg279d_ld_f32(t76, t81);
  v30 = ((float)(t82));
  }
  }
  v31 = ((float)(v30));
  t83 = ((int64_t)(v29));
  t84 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t85 = ((int64_t)(((int64_t)((uint64_t)(t83) + (uint64_t)(t84)))));
  t86 = ((int64_t)(((int64_t)(v2))));
  t87 = ((t85) < (t86));
  if (t87) {
  {
  t88 = ((float)(v31));
  t89 = ((uint64_t)(v0));
  t90 = ((int64_t)(v26));
  t91 = ((int64_t)(v29));
  t92 = ((int64_t)(((int64_t)((uint64_t)(t90) + (uint64_t)(t91)))));
  t93 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t94 = ((int64_t)(((int64_t)((uint64_t)(t92) + (uint64_t)(t93)))));
  t95 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t96 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t94) * (uint64_t)(t95)))))));
#ifndef __cplusplus
  extern float (fg279d_ld_f32)(uint64_t, uint64_t);
#endif
  t97 = fg279d_ld_f32(t89, t96);
  t98 = ((float)(t97));
  v31 = ((float)(((t88) + (t98))));
  }
  }
  v32 = ((float)(v31));
  t99 = ((int64_t)(v29));
  t100 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t101 = ((int64_t)(((int64_t)((uint64_t)(t99) + (uint64_t)(t100)))));
  t102 = ((int64_t)(((int64_t)(v2))));
  t103 = ((t101) < (t102));
  if (t103) {
  {
  t104 = ((float)(v32));
  t105 = ((uint64_t)(v0));
  t106 = ((int64_t)(v26));
  t107 = ((int64_t)(v29));
  t108 = ((int64_t)(((int64_t)((uint64_t)(t106) + (uint64_t)(t107)))));
  t109 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t110 = ((int64_t)(((int64_t)((uint64_t)(t108) + (uint64_t)(t109)))));
  t111 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t112 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t110) * (uint64_t)(t111)))))));
#ifndef __cplusplus
  extern float (fg279d_ld_f32)(uint64_t, uint64_t);
#endif
  t113 = fg279d_ld_f32(t105, t112);
  t114 = ((float)(t113));
  v32 = ((float)(((t104) + (t114))));
  }
  }
  v33 = ((float)(v32));
  t115 = ((int64_t)(v29));
  t116 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t117 = ((int64_t)(((int64_t)((uint64_t)(t115) + (uint64_t)(t116)))));
  t118 = ((int64_t)(((int64_t)(v2))));
  t119 = ((t117) < (t118));
  if (t119) {
  {
  t120 = ((float)(v33));
  t121 = ((uint64_t)(v0));
  t122 = ((int64_t)(v26));
  t123 = ((int64_t)(v29));
  t124 = ((int64_t)(((int64_t)((uint64_t)(t122) + (uint64_t)(t123)))));
  t125 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t126 = ((int64_t)(((int64_t)((uint64_t)(t124) + (uint64_t)(t125)))));
  t127 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t128 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t126) * (uint64_t)(t127)))))));
#ifndef __cplusplus
  extern float (fg279d_ld_f32)(uint64_t, uint64_t);
#endif
  t129 = fg279d_ld_f32(t121, t128);
  t130 = ((float)(t129));
  v33 = ((float)(((t120) + (t130))));
  }
  }
  v34 = ((float)(((float)(fg279dd_gg_double_bits(UINT64_C(0))))));
  t131 = ((int64_t)(v29));
  t132 = ((int64_t)(((int64_t)(v2))));
  t133 = ((t131) < (t132));
  if (t133) {
  {
  v34 = ((float)(v33));
  }
  }
  t134 = ((float)(v34));
  t135 = ((int64_t)(v18));
  t136 = ((int64_t)(v17));
  v34 = fg279dd_gg_function_8(t134, t135, t136, base + 0);
  t137 = ((uint64_t)(v22));
  t138 = ((int64_t)(v13));
  t139 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t140 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t138) * (uint64_t)(t139)))))));
  t141 = ((uint64_t)(((uint64_t)((uint64_t)(t137) + (uint64_t)(t140)))));
  t142 = ((float)(v34));
#ifndef __cplusplus
  extern void (fg279d_sstore)(uint64_t, float);
#endif
  fg279d_sstore(t141, t142);
  t143 = ((int64_t)(v18));
  t144 = ((int64_t)(v17));
  t145 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t146 = ((int64_t)(((int64_t)((uint64_t)(t144) - (uint64_t)(t145)))));
  t147 = ((t143) == (t146));
  if (t147) {
  {
  t148 = ((uint64_t)(v23));
  t149 = ((int64_t)(v19));
  t150 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t151 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t149) * (uint64_t)(t150)))))));
  t152 = ((uint64_t)(((uint64_t)((uint64_t)(t148) + (uint64_t)(t151)))));
  t153 = ((float)(v34));
#ifndef __cplusplus
  extern void (fg279d_sstore)(uint64_t, float);
#endif
  fg279d_sstore(t152, t153);
  }
  }
#ifndef __cplusplus
  extern void (fg279d_sync)(void);
#endif
  fg279d_sync();
  t154 = ((int64_t)(v19));
  t155 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t156 = ((t154) == (t155));
  if (t156) {
  {
  v35 = ((float)(((float)(fg279dd_gg_double_bits(UINT64_C(0))))));
  t157 = ((int64_t)(v13));
  t158 = ((int64_t)(((int64_t)(v20))));
  t159 = ((t157) < (t158));
  if (t159) {
  {
  t160 = ((uint64_t)(v23));
  t161 = ((int64_t)(v13));
  t162 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t163 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t161) * (uint64_t)(t162)))))));
  t164 = ((uint64_t)(((uint64_t)((uint64_t)(t160) + (uint64_t)(t163)))));
#ifndef __cplusplus
  extern float (fg279d_sload)(uint64_t);
#endif
  t165 = fg279d_sload(t164);
  v35 = ((float)(t165));
  }
  }
  v36 = ((float)(v35));
  t166 = ((float)(v36));
  t167 = ((int64_t)(v18));
  t168 = ((int64_t)(v17));
  v36 = fg279dd_gg_function_8(t166, t167, t168, base + 0);
  t169 = ((int64_t)(v13));
  t170 = ((int64_t)(((int64_t)(v20))));
  t171 = ((t169) < (t170));
  if (t171) {
  {
  t172 = ((uint64_t)(v23));
  t173 = ((int64_t)(v13));
  t174 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t175 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t173) * (uint64_t)(t174)))))));
  t176 = ((uint64_t)(((uint64_t)((uint64_t)(t172) + (uint64_t)(t175)))));
  t177 = ((float)(v36));
  t178 = ((float)(v35));
  t179 = ((float)(((t177) - (t178))));
#ifndef __cplusplus
  extern void (fg279d_sstore)(uint64_t, float);
#endif
  fg279d_sstore(t176, t179);
  }
  }
  t180 = ((int64_t)(v13));
  t181 = ((int64_t)(v20));
  t182 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t183 = ((int64_t)(((int64_t)((uint64_t)(t181) - (uint64_t)(t182)))));
  t184 = ((t180) == (t183));
  if (t184) {
  {
  t185 = ((uint64_t)(v25));
  t186 = ((float)(v36));
#ifndef __cplusplus
  extern void (fg279d_sstore)(uint64_t, float);
#endif
  fg279d_sstore(t185, t186);
  }
  }
  }
  }
#ifndef __cplusplus
  extern void (fg279d_sync)(void);
#endif
  fg279d_sync();
  t187 = ((uint64_t)(v24));
#ifndef __cplusplus
  extern float (fg279d_sload)(uint64_t);
#endif
  t188 = fg279d_sload(t187);
  v37 = ((float)(t188));
  t189 = ((uint64_t)(v22));
  t190 = ((int64_t)(v13));
  t191 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t192 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t190) * (uint64_t)(t191)))))));
  t193 = ((uint64_t)(((uint64_t)((uint64_t)(t189) + (uint64_t)(t192)))));
#ifndef __cplusplus
  extern float (fg279d_sload)(uint64_t);
#endif
  t194 = fg279d_sload(t193);
  v38 = ((float)(t194));
  t195 = ((uint64_t)(v23));
  t196 = ((int64_t)(v19));
  t197 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t198 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t196) * (uint64_t)(t197)))))));
  t199 = ((uint64_t)(((uint64_t)((uint64_t)(t195) + (uint64_t)(t198)))));
#ifndef __cplusplus
  extern float (fg279d_sload)(uint64_t);
#endif
  t200 = fg279d_sload(t199);
  v39 = ((float)(t200));
  t201 = ((float)(v38));
  t202 = ((float)(v39));
  t203 = ((float)(((t201) + (t202))));
  t204 = ((float)(v37));
  t205 = ((float)(((t203) + (t204))));
  t206 = ((float)(v33));
  v40 = ((float)(((t205) - (t206))));
  t207 = ((int64_t)(v29));
  t208 = ((int64_t)(((int64_t)(v2))));
  t209 = ((t207) < (t208));
  if (t209) {
  {
  t210 = ((uint64_t)(v1));
  t211 = ((int64_t)(v27));
  t212 = ((int64_t)(v29));
  t213 = ((int64_t)(((int64_t)((uint64_t)(t211) + (uint64_t)(t212)))));
  t214 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t215 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t213) * (uint64_t)(t214)))))));
  t216 = ((float)(v30));
  t217 = ((float)(v40));
  t218 = ((float)(((t216) + (t217))));
#ifndef __cplusplus
  extern void (fg279d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg279d_st_f32(t210, t215, t218);
  }
  }
  t219 = ((int64_t)(v29));
  t220 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t221 = ((int64_t)(((int64_t)((uint64_t)(t219) + (uint64_t)(t220)))));
  t222 = ((int64_t)(((int64_t)(v2))));
  t223 = ((t221) < (t222));
  if (t223) {
  {
  t224 = ((uint64_t)(v1));
  t225 = ((int64_t)(v27));
  t226 = ((int64_t)(v29));
  t227 = ((int64_t)(((int64_t)((uint64_t)(t225) + (uint64_t)(t226)))));
  t228 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t229 = ((int64_t)(((int64_t)((uint64_t)(t227) + (uint64_t)(t228)))));
  t230 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t231 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t229) * (uint64_t)(t230)))))));
  t232 = ((float)(v31));
  t233 = ((float)(v40));
  t234 = ((float)(((t232) + (t233))));
#ifndef __cplusplus
  extern void (fg279d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg279d_st_f32(t224, t231, t234);
  }
  }
  t235 = ((int64_t)(v29));
  t236 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t237 = ((int64_t)(((int64_t)((uint64_t)(t235) + (uint64_t)(t236)))));
  t238 = ((int64_t)(((int64_t)(v2))));
  t239 = ((t237) < (t238));
  if (t239) {
  {
  t240 = ((uint64_t)(v1));
  t241 = ((int64_t)(v27));
  t242 = ((int64_t)(v29));
  t243 = ((int64_t)(((int64_t)((uint64_t)(t241) + (uint64_t)(t242)))));
  t244 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t245 = ((int64_t)(((int64_t)((uint64_t)(t243) + (uint64_t)(t244)))));
  t246 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t247 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t245) * (uint64_t)(t246)))))));
  t248 = ((float)(v32));
  t249 = ((float)(v40));
  t250 = ((float)(((t248) + (t249))));
#ifndef __cplusplus
  extern void (fg279d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg279d_st_f32(t240, t247, t250);
  }
  }
  t251 = ((int64_t)(v29));
  t252 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t253 = ((int64_t)(((int64_t)((uint64_t)(t251) + (uint64_t)(t252)))));
  t254 = ((int64_t)(((int64_t)(v2))));
  t255 = ((t253) < (t254));
  if (t255) {
  {
  t256 = ((uint64_t)(v1));
  t257 = ((int64_t)(v27));
  t258 = ((int64_t)(v29));
  t259 = ((int64_t)(((int64_t)((uint64_t)(t257) + (uint64_t)(t258)))));
  t260 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t261 = ((int64_t)(((int64_t)((uint64_t)(t259) + (uint64_t)(t260)))));
  t262 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t263 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t261) * (uint64_t)(t262)))))));
  t264 = ((float)(v33));
  t265 = ((float)(v40));
  t266 = ((float)(((t264) + (t265))));
#ifndef __cplusplus
  extern void (fg279d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg279d_st_f32(t256, t263, t266);
  }
  }
#ifndef __cplusplus
  extern void (fg279d_sync)(void);
#endif
  fg279d_sync();
  t267 = ((int64_t)(v13));
  t268 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t269 = ((t267) == (t268));
  if (t269) {
  {
  t270 = ((uint64_t)(v24));
  t271 = ((uint64_t)(v24));
#ifndef __cplusplus
  extern float (fg279d_sload)(uint64_t);
#endif
  t272 = fg279d_sload(t271);
  t273 = ((float)(t272));
  t274 = ((uint64_t)(v25));
#ifndef __cplusplus
  extern float (fg279d_sload)(uint64_t);
#endif
  t275 = fg279d_sload(t274);
  t276 = ((float)(t275));
  t277 = ((float)(((t273) + (t276))));
#ifndef __cplusplus
  extern void (fg279d_sstore)(uint64_t, float);
#endif
  fg279d_sstore(t270, t277);
  }
  }
  t278 = ((int64_t)(v28));
  t279 = ((int64_t)(((int64_t)UINT64_C(4))));
  t280 = ((int64_t)(((int64_t)(v12))));
  t281 = ((int64_t)(((int64_t)((uint64_t)(t279) * (uint64_t)(t280)))));
  v28 = ((int64_t)(((int64_t)((uint64_t)(t278) + (uint64_t)(t281)))));
  }
  goto label1;
label2:;
  }
fg279dd_gg_done:;
}
