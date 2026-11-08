#pragma once
#include <stdint.h>
static __device__ __forceinline__ void fg175fx_gg_fail(const char*);
static __device__ __forceinline__ fg175fx_gg_slice fg175fx_gg_view(void *p, int64_t n);
static __device__ __forceinline__ int64_t fg175fx_gg_index(int64_t i, int64_t len);
static __device__ __forceinline__ int64_t fg175fx_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg175fx_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg175fx_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg175fx_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg175fx_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ float fg175fx_gg_function_15(int64_t base);
static __device__ __forceinline__ void fg175fx_gg_function_18(uint64_t p0, uint64_t p1, int32_t p2, int32_t p3, int32_t p4, int32_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int64_t base);
static __device__ __forceinline__ void fg175fx_gg_function_11(uint64_t p0, uint64_t p1, int32_t p2, int32_t p3, int32_t p4, int32_t p5, int32_t p6, int32_t p7, int64_t p8, int64_t p9, int64_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int64_t base);
static __device__ __forceinline__ int32_t fg175fx_gg_function_7(int32_t p0, int64_t base);
static __device__ __forceinline__ int32_t fg175fx_gg_function_6(uint32_t p0, int64_t base);
static __device__ __forceinline__ void fg175fx_gg_function_1(uint64_t p0, uint64_t p1, int32_t p2, int64_t p3, int64_t p4, int32_t p5, int32_t p6, int32_t p7, int32_t p8, int32_t p9, uint64_t p10, int64_t base);
static __device__ __forceinline__ void fg175fx_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ fg175fx_gg_slice fg175fx_gg_view(void *p, int64_t n) { fg175fx_gg_slice v; v.data=p; v.len=n; return v; }
static __device__ __forceinline__ int64_t fg175fx_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg175fx_gg_fail("array index out of bounds"); return i; }
static __device__ __forceinline__ int64_t fg175fx_gg_div(int64_t a, int64_t b) { if (!b) fg175fx_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg175fx_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg175fx_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg175fx_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg175fx_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg175fx_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg175fx_gg_umod(uint64_t a,uint64_t b) { if(!b)fg175fx_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg175fx_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ float fg175fx_gg_function_15(int64_t base) {
  float t0;
  double t1;
  {
  t1 = fg175fx_gg_double_bits(UINT64_C(4626322717216342016));
  t0 = ((float)((-(t1))));
  goto fg175fx_gg_done;
  }
fg175fx_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg175fx_gg_function_18(uint64_t p0, uint64_t p1, int32_t p2, int32_t p3, int32_t p4, int32_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int64_t base) {
  int32_t t0;
  int32_t t1;
  int32_t t2;
  int32_t t3;
  int32_t t4;
  int32_t t5;
  uint64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  uint64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int32_t t18;
  int32_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  uint8_t t35;
  int64_t t36;
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
  uint8_t t47;
  int32_t t48;
  int32_t t49;
  uint8_t t50;
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
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  uint8_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  uint8_t t90;
  int32_t t91;
  int32_t t92;
  uint8_t t93;
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
  int64_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  uint64_t t124;
  uint64_t t125;
  float t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  uint64_t t133;
  uint64_t t134;
  float t135;
  uint64_t t136;
  int64_t t137;
  int64_t t138;
  uint64_t t139;
  float t140;
  int32_t t141;
  int32_t t142;
  int32_t t143;
  int32_t t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  int64_t t148;
  int64_t t149;
  int64_t t150;
  uint8_t t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  int64_t t159;
  uint64_t t160;
  int64_t t161;
  int64_t t162;
  uint64_t t163;
  float t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  int64_t t169;
  int64_t t170;
  int64_t t171;
  int64_t t172;
  uint64_t t173;
  uint64_t t174;
  float t175;
  uint64_t t176;
  int64_t t177;
  int64_t t178;
  uint64_t t179;
  float t180;
  float t181;
  float t182;
  float t183;
  float t184;
  float t185;
  float t186;
  float t187;
  float t188;
  float t189;
  float t190;
  uint8_t t191;
  float t192;
  float t193;
  float t194;
  float t195;
  float t196;
  uint8_t t197;
  float t198;
  float t199;
  float t200;
  float t201;
  float t202;
  float t203;
  float t204;
  float t205;
  float t206;
  float t207;
  float t208;
  float t209;
  float t210;
  float t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  int64_t t216;
  int64_t t217;
  int64_t t218;
  int64_t t219;
  uint8_t t220;
  int64_t t221;
  int64_t t222;
  uint8_t t223;
  int32_t t224;
  int32_t t225;
  uint8_t t226;
  int32_t t227;
  int32_t t228;
  uint64_t t229;
  uint64_t t230;
  float t231;
  float t232;
  float t233;
  uint64_t v0;
  uint64_t v1;
  int32_t v2;
  int32_t v3;
  int32_t v4;
  int32_t v5;
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
  int64_t v21;
  uint64_t v22;
  int64_t v23;
  int64_t v24;
  int32_t v25;
  int64_t v26;
  int64_t v27;
  int64_t v28;
  int64_t v29;
  int64_t v30;
  int64_t v31;
  int64_t v32;
  int64_t v33;
  int64_t v34;
  int64_t v35;
  int64_t v36;
  int64_t v37;
  int64_t v38;
  int64_t v39;
  int32_t v40;
  int64_t v41;
  uint64_t v42;
  float v43;
  float v44;
  float v45;
  int64_t v46;
  int32_t v47;
  int64_t v48;
  int32_t v49;
  int64_t v50;
  int64_t v51;
  float v52;
  int64_t v53;
  float v54;
  float v55;
  float v56;
  float v57;
  float v58;
  float v59;
  float v60;
  int64_t v61;
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
  {
  t0 = ((int32_t)(v10));
  t1 = ((int32_t)(v11));
  v17 = ((int32_t)(((int32_t)((uint64_t)(t0) * (uint64_t)(t1)))));
  v18 = ((int32_t)(v13));
  v19 = ((int32_t)(v14));
  t2 = ((int32_t)(v18));
  t3 = ((int32_t)(v11));
  t4 = ((int32_t)(((int32_t)((uint64_t)(t2) * (uint64_t)(t3)))));
  t5 = ((int32_t)(v19));
  v20 = ((int32_t)(((int32_t)((uint64_t)(t4) + (uint64_t)(t5)))));
  v21 = ((int64_t)(((int64_t)(v15))));
  t6 = ((uint64_t)(v1));
  t7 = ((int64_t)(v21));
  t8 = ((int64_t)(((int64_t)UINT64_C(4))));
  t9 = ((int64_t)(((int64_t)((uint64_t)(t7) * (uint64_t)(t8)))));
  t10 = ((int64_t)(((int64_t)(v17))));
  t11 = ((int64_t)(((int64_t)((uint64_t)(t9) * (uint64_t)(t10)))));
  t12 = ((int64_t)(((int64_t)UINT64_C(4))));
  t13 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t12)))))));
  v22 = ((uint64_t)(((uint64_t)((uint64_t)(t6) + (uint64_t)(t13)))));
  t14 = ((int64_t)(((int64_t)(v12))));
  t15 = ((int64_t)(((int64_t)(v5))));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))));
  t17 = ((int64_t)(v21));
  v23 = ((int64_t)(((int64_t)fg175fx_gg_div(t16, t17))));
  t18 = ((int32_t)(v12));
  t19 = ((int32_t)(((int64_t)UINT64_C(1))));
  t20 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t18) + (uint64_t)(t19)))))));
  t21 = ((int64_t)(((int64_t)(v5))));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  t23 = ((int64_t)(v21));
  v24 = ((int64_t)(((int64_t)fg175fx_gg_div(t22, t23))));
  v25 = ((int32_t)(((int64_t)UINT64_C(0))));
  t24 = ((int64_t)(v23));
  t25 = ((int64_t)(v24));
  t26 = ((t24) == (t25));
  if (t26) {
  {
  v25 = ((int32_t)(((int64_t)UINT64_C(1))));
  }
  }
  t27 = ((int64_t)(v23));
  t28 = ((int64_t)(v9));
  v26 = ((int64_t)(((int64_t)fg175fx_gg_div(t27, t28))));
  t29 = ((int64_t)(v23));
  t30 = ((int64_t)(v26));
  t31 = ((int64_t)(v9));
  t32 = ((int64_t)(((int64_t)((uint64_t)(t30) * (uint64_t)(t31)))));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t29) - (uint64_t)(t32)))));
  t34 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t35 = ((t33) == (t34));
  if (t35) {
  {
  v25 = ((int32_t)(((int64_t)UINT64_C(1))));
  }
  }
  t36 = ((int64_t)(v24));
  t37 = ((int64_t)(v9));
  v27 = ((int64_t)(((int64_t)fg175fx_gg_div(t36, t37))));
  t38 = ((int64_t)(v26));
  t39 = ((int64_t)(v27));
  t40 = ((t38) == (t39));
  if (t40) {
  {
  t41 = ((int64_t)(v24));
  t42 = ((int64_t)(v27));
  t43 = ((int64_t)(v9));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) * (uint64_t)(t43)))));
  t45 = ((int64_t)(((int64_t)((uint64_t)(t41) - (uint64_t)(t44)))));
  t46 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t47 = ((t45) != (t46));
  if (t47) {
  {
  v25 = ((int32_t)(((int64_t)UINT64_C(1))));
  }
  }
  }
  }
  t48 = ((int32_t)(v25));
  t49 = ((int32_t)(((int64_t)UINT64_C(1))));
  t50 = ((t48) == (t49));
  if (t50) {
  {
  goto fg175fx_gg_done;
  }
  }
  t51 = ((int64_t)(v23));
  t52 = ((int64_t)(v6));
  v28 = ((int64_t)(((int64_t)fg175fx_gg_div(t51, t52))));
  t53 = ((int64_t)(v23));
  t54 = ((int64_t)(v28));
  t55 = ((int64_t)(v6));
  t56 = ((int64_t)(((int64_t)((uint64_t)(t54) * (uint64_t)(t55)))));
  v29 = ((int64_t)(((int64_t)((uint64_t)(t53) - (uint64_t)(t56)))));
  t57 = ((int64_t)(v29));
  t58 = ((int64_t)(v7));
  v30 = ((int64_t)(((int64_t)fg175fx_gg_div(t57, t58))));
  t59 = ((int64_t)(v29));
  t60 = ((int64_t)(v30));
  t61 = ((int64_t)(v7));
  t62 = ((int64_t)(((int64_t)((uint64_t)(t60) * (uint64_t)(t61)))));
  v31 = ((int64_t)(((int64_t)((uint64_t)(t59) - (uint64_t)(t62)))));
  t63 = ((int64_t)(v31));
  t64 = ((int64_t)(v8));
  v32 = ((int64_t)(((int64_t)fg175fx_gg_div(t63, t64))));
  t65 = ((int64_t)(v31));
  t66 = ((int64_t)(v32));
  t67 = ((int64_t)(v8));
  t68 = ((int64_t)(((int64_t)((uint64_t)(t66) * (uint64_t)(t67)))));
  v33 = ((int64_t)(((int64_t)((uint64_t)(t65) - (uint64_t)(t68)))));
  t69 = ((int64_t)(v33));
  t70 = ((int64_t)(v9));
  v34 = ((int64_t)(((int64_t)fg175fx_gg_div(t69, t70))));
  v35 = ((int64_t)(v28));
  v36 = ((int64_t)(v30));
  v37 = ((int64_t)(v32));
  v38 = ((int64_t)(v34));
  t71 = ((int64_t)(v36));
  t72 = ((int64_t)(((int64_t)(v4))));
  t73 = ((int64_t)(((int64_t)((uint64_t)(t71) * (uint64_t)(t72)))));
  t74 = ((int64_t)(v37));
  t75 = ((int64_t)(((int64_t)(v11))));
  t76 = ((int64_t)(((int64_t)((uint64_t)(t74) * (uint64_t)(t75)))));
  v39 = ((int64_t)(((int64_t)((uint64_t)(t73) + (uint64_t)(t76)))));
  v40 = ((int32_t)(((int64_t)UINT64_C(0))));
  t77 = ((int64_t)(v38));
  t78 = ((int64_t)(((int64_t)(v10))));
  t79 = ((int64_t)(((int64_t)((uint64_t)(t77) * (uint64_t)(t78)))));
  t80 = ((int64_t)(((int64_t)(v18))));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t79) + (uint64_t)(t80)))));
  t82 = ((int64_t)(((int64_t)(v2))));
  t83 = ((t81) >= (t82));
  if (t83) {
  {
  v40 = ((int32_t)(((int64_t)UINT64_C(1))));
  }
  }
  t84 = ((int64_t)(v37));
  t85 = ((int64_t)(((int64_t)(v11))));
  t86 = ((int64_t)(((int64_t)((uint64_t)(t84) * (uint64_t)(t85)))));
  t87 = ((int64_t)(((int64_t)(v19))));
  t88 = ((int64_t)(((int64_t)((uint64_t)(t86) + (uint64_t)(t87)))));
  t89 = ((int64_t)(((int64_t)(v4))));
  t90 = ((t88) >= (t89));
  if (t90) {
  {
  v40 = ((int32_t)(((int64_t)UINT64_C(1))));
  }
  }
  t91 = ((int32_t)(v40));
  t92 = ((int32_t)(((int64_t)UINT64_C(1))));
  t93 = ((t91) == (t92));
  if (t93) {
  {
  goto fg175fx_gg_done;
  }
  }
  t94 = ((int64_t)(v35));
  t95 = ((int64_t)(((int64_t)(v3))));
  t96 = ((int64_t)(((int64_t)((uint64_t)(t94) * (uint64_t)(t95)))));
  t97 = ((int64_t)(((int64_t)(v2))));
  t98 = ((int64_t)(((int64_t)((uint64_t)(t96) * (uint64_t)(t97)))));
  t99 = ((int64_t)(((int64_t)UINT64_C(256))));
  t100 = ((int64_t)(((int64_t)((uint64_t)(t98) * (uint64_t)(t99)))));
  t101 = ((int64_t)(v38));
  t102 = ((int64_t)(((int64_t)(v3))));
  t103 = ((int64_t)(((int64_t)((uint64_t)(t101) * (uint64_t)(t102)))));
  t104 = ((int64_t)(((int64_t)(v10))));
  t105 = ((int64_t)(((int64_t)UINT64_C(256))));
  t106 = ((int64_t)(((int64_t)((uint64_t)(t104) * (uint64_t)(t105)))));
  t107 = ((int64_t)(((int64_t)((uint64_t)(t103) * (uint64_t)(t106)))));
  t108 = ((int64_t)(((int64_t)((uint64_t)(t100) + (uint64_t)(t107)))));
  t109 = ((int64_t)(v39));
  t110 = ((int64_t)(((int64_t)UINT64_C(256))));
  t111 = ((int64_t)(((int64_t)((uint64_t)(t109) * (uint64_t)(t110)))));
  t112 = ((int64_t)(((int64_t)((uint64_t)(t108) + (uint64_t)(t111)))));
  t113 = ((int64_t)(((int64_t)(v18))));
  t114 = ((int64_t)(((int64_t)(v3))));
  t115 = ((int64_t)(((int64_t)((uint64_t)(t113) * (uint64_t)(t114)))));
  t116 = ((int64_t)(((int64_t)(v19))));
  t117 = ((int64_t)(((int64_t)((uint64_t)(t115) + (uint64_t)(t116)))));
  t118 = ((int64_t)(((int64_t)UINT64_C(256))));
  t119 = ((int64_t)(((int64_t)((uint64_t)(t117) * (uint64_t)(t118)))));
  t120 = ((int64_t)(((int64_t)((uint64_t)(t112) + (uint64_t)(t119)))));
  t121 = ((int64_t)(((int64_t)(v16))));
  v41 = ((int64_t)(((int64_t)((uint64_t)(t120) + (uint64_t)(t121)))));
  t122 = ((int64_t)(v41));
  t123 = ((int64_t)(((int64_t)UINT64_C(4))));
  v42 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t122) * (uint64_t)(t123)))))));
  v43 = ((float)(fg175fx_gg_double_bits(UINT64_C(0))));
  v44 = ((float)(fg175fx_gg_double_bits(UINT64_C(0))));
  v45 = ((float)(fg175fx_gg_double_bits(UINT64_C(0))));
  fg168f_sync();
  t124 = ((uint64_t)(v0));
  t125 = ((uint64_t)(v42));
  t126 = fg168f_ld_f32(t124, t125);
  v43 = ((float)(t126));
  t127 = ((int64_t)(((int64_t)(v12))));
  t128 = ((int64_t)(((int64_t)(v17))));
  t129 = ((int64_t)(((int64_t)((uint64_t)(t127) * (uint64_t)(t128)))));
  t130 = ((int64_t)(((int64_t)(v20))));
  t131 = ((int64_t)(((int64_t)((uint64_t)(t129) + (uint64_t)(t130)))));
  t132 = ((int64_t)(((int64_t)UINT64_C(8))));
  v46 = ((int64_t)(((int64_t)((uint64_t)(t131) * (uint64_t)(t132)))));
  t133 = ((uint64_t)(v1));
  t134 = ((uint64_t)(((uint64_t)(v46))));
  t135 = fg168f_ld_f32(t133, t134);
  v44 = ((float)(t135));
  t136 = ((uint64_t)(v1));
  t137 = ((int64_t)(v46));
  t138 = ((int64_t)(((int64_t)UINT64_C(4))));
  t139 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t137) + (uint64_t)(t138)))))));
  t140 = fg168f_ld_f32(t136, t139);
  v45 = ((float)(t140));
  t141 = ((int32_t)(v12));
  t142 = ((int32_t)(((int64_t)UINT64_C(1))));
  v47 = ((int32_t)(((int32_t)((uint64_t)(t141) - (uint64_t)(t142)))));
  v48 = ((int64_t)(v23));
  v49 = ((int32_t)(((int64_t)UINT64_C(1))));
label1:;
  t143 = ((int32_t)(v49));
  t144 = ((int32_t)(((int64_t)UINT64_C(1))));
  if (!(((t143) == (t144)))) goto label2;
  {
  t145 = ((int64_t)(((int64_t)(v47))));
  t146 = ((int64_t)(((int64_t)(v5))));
  t147 = ((int64_t)(((int64_t)((uint64_t)(t145) * (uint64_t)(t146)))));
  t148 = ((int64_t)(v21));
  v50 = ((int64_t)(((int64_t)fg175fx_gg_div(t147, t148))));
  t149 = ((int64_t)(v50));
  t150 = ((int64_t)(v48));
  t151 = ((t149) != (t150));
  if (t151) {
  {
  t152 = ((int64_t)(((int64_t)(v47))));
  t153 = ((int64_t)(((int64_t)(v17))));
  t154 = ((int64_t)(((int64_t)((uint64_t)(t152) * (uint64_t)(t153)))));
  t155 = ((int64_t)(((int64_t)(v20))));
  t156 = ((int64_t)(((int64_t)((uint64_t)(t154) + (uint64_t)(t155)))));
  t157 = ((int64_t)(((int64_t)UINT64_C(256))));
  t158 = ((int64_t)(((int64_t)((uint64_t)(t156) * (uint64_t)(t157)))));
  t159 = ((int64_t)(((int64_t)(v16))));
  v51 = ((int64_t)(((int64_t)((uint64_t)(t158) + (uint64_t)(t159)))));
  t160 = ((uint64_t)(v22));
  t161 = ((int64_t)(v51));
  t162 = ((int64_t)(((int64_t)UINT64_C(4))));
  t163 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t161) * (uint64_t)(t162)))))));
  t164 = fg168f_ld_f32(t160, t163);
  v52 = ((float)(t164));
  t165 = ((int64_t)(v21));
  t166 = ((int64_t)(((int64_t)(v47))));
  t167 = ((int64_t)(((int64_t)((uint64_t)(t165) + (uint64_t)(t166)))));
  t168 = ((int64_t)(((int64_t)(v17))));
  t169 = ((int64_t)(((int64_t)((uint64_t)(t167) * (uint64_t)(t168)))));
  t170 = ((int64_t)(((int64_t)(v20))));
  t171 = ((int64_t)(((int64_t)((uint64_t)(t169) + (uint64_t)(t170)))));
  t172 = ((int64_t)(((int64_t)UINT64_C(8))));
  v53 = ((int64_t)(((int64_t)((uint64_t)(t171) * (uint64_t)(t172)))));
  t173 = ((uint64_t)(v1));
  t174 = ((uint64_t)(((uint64_t)(v53))));
  t175 = fg168f_ld_f32(t173, t174);
  v54 = ((float)(t175));
  t176 = ((uint64_t)(v1));
  t177 = ((int64_t)(v53));
  t178 = ((int64_t)(((int64_t)UINT64_C(4))));
  t179 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t177) + (uint64_t)(t178)))))));
  t180 = fg168f_ld_f32(t176, t179);
  v55 = ((float)(t180));
  t181 = ((float)(v44));
  t182 = ((float)(v54));
  t183 = fg168f_fmaxf(t181, t182);
  v56 = ((float)(t183));
  t184 = ((float)(v44));
  t185 = ((float)(v56));
  v57 = ((float)(((t184) - (t185))));
  t186 = ((float)(v54));
  t187 = ((float)(v56));
  v58 = ((float)(((t186) - (t187))));
  t188 = ((float)(v57));
  t189 = fg175fx_gg_function_15(base + 0);
  t190 = ((float)(t189));
  t191 = ((t188) >= (t190));
  if (t191) {
  {
  t192 = ((float)(v57));
  t193 = fg168f_expf(t192);
  v59 = ((float)(t193));
  }
  } else {
  {
  v59 = ((float)(((float)(fg175fx_gg_double_bits(UINT64_C(0))))));
  }
  }
  t194 = ((float)(v58));
  t195 = fg175fx_gg_function_15(base + 0);
  t196 = ((float)(t195));
  t197 = ((t194) >= (t196));
  if (t197) {
  {
  t198 = ((float)(v58));
  t199 = fg168f_expf(t198);
  v60 = ((float)(t199));
  }
  } else {
  {
  v60 = ((float)(((float)(fg175fx_gg_double_bits(UINT64_C(0))))));
  }
  }
  t200 = ((float)(v59));
  t201 = ((float)(v43));
  t202 = ((float)(((t200) * (t201))));
  t203 = ((float)(v60));
  t204 = ((float)(v52));
  t205 = ((float)(((t203) * (t204))));
  v43 = ((float)(((t202) + (t205))));
  t206 = ((float)(v59));
  t207 = ((float)(v45));
  t208 = ((float)(((t206) * (t207))));
  t209 = ((float)(v60));
  t210 = ((float)(v55));
  t211 = ((float)(((t209) * (t210))));
  v45 = ((float)(((t208) + (t211))));
  v44 = ((float)(v56));
  t212 = ((int64_t)(v50));
  t213 = ((int64_t)(v9));
  v61 = ((int64_t)(((int64_t)fg175fx_gg_div(t212, t213))));
  t214 = ((int64_t)(v50));
  t215 = ((int64_t)(v61));
  t216 = ((int64_t)(v9));
  t217 = ((int64_t)(((int64_t)((uint64_t)(t215) * (uint64_t)(t216)))));
  t218 = ((int64_t)(((int64_t)((uint64_t)(t214) - (uint64_t)(t217)))));
  t219 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t220 = ((t218) == (t219));
  if (t220) {
  {
  v49 = ((int32_t)(((int64_t)UINT64_C(0))));
  }
  }
  t221 = ((int64_t)(v61));
  t222 = ((int64_t)(v26));
  t223 = ((t221) < (t222));
  if (t223) {
  {
  v49 = ((int32_t)(((int64_t)UINT64_C(0))));
  }
  }
  }
  }
  t224 = ((int32_t)(v49));
  t225 = ((int32_t)(((int64_t)UINT64_C(1))));
  t226 = ((t224) == (t225));
  if (t226) {
  {
  t227 = v47;
  t228 = ((int32_t)(((int64_t)UINT64_C(1))));
  v47 = ((int32_t)((uint64_t)(t227) - (uint64_t)(t228)));
  v48 = ((int64_t)(v50));
  }
  }
  }
  goto label1;
label2:;
  t229 = ((uint64_t)(v0));
  t230 = ((uint64_t)(v42));
  t231 = ((float)(v43));
  t232 = ((float)(v45));
  t233 = ((float)(((t231) / (t232))));
  fg168f_st_f32(t229, t230, t233);
  }
fg175fx_gg_done:;
}
static __device__ __forceinline__ void fg175fx_gg_function_11(uint64_t p0, uint64_t p1, int32_t p2, int32_t p3, int32_t p4, int32_t p5, int32_t p6, int32_t p7, int64_t p8, int64_t p9, int64_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int64_t base) {
  int32_t t0;
  int32_t t1;
  int32_t t2;
  int32_t t3;
  int32_t t4;
  int32_t t5;
  int32_t t6;
  int32_t t7;
  int32_t t8;
  int32_t t9;
  int32_t t10;
  int32_t t11;
  uint64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  uint64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
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
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  uint8_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  uint8_t t57;
  int32_t t58;
  int32_t t59;
  uint8_t t60;
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
  int64_t t75;
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
  int64_t t90;
  uint64_t t91;
  uint64_t t92;
  float t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  uint64_t t100;
  uint64_t t101;
  float t102;
  uint64_t t103;
  int64_t t104;
  int64_t t105;
  uint64_t t106;
  float t107;
  int32_t t108;
  int32_t t109;
  int32_t t110;
  int32_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  uint64_t t120;
  int64_t t121;
  int64_t t122;
  uint64_t t123;
  float t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  uint64_t t133;
  uint64_t t134;
  float t135;
  uint64_t t136;
  int64_t t137;
  int64_t t138;
  uint64_t t139;
  float t140;
  float t141;
  float t142;
  float t143;
  float t144;
  float t145;
  float t146;
  float t147;
  float t148;
  float t149;
  float t150;
  uint8_t t151;
  float t152;
  float t153;
  float t154;
  float t155;
  float t156;
  uint8_t t157;
  float t158;
  float t159;
  float t160;
  float t161;
  float t162;
  float t163;
  float t164;
  float t165;
  float t166;
  float t167;
  float t168;
  float t169;
  float t170;
  float t171;
  int32_t t172;
  int32_t t173;
  uint64_t t174;
  uint64_t t175;
  float t176;
  float t177;
  float t178;
  uint64_t v0;
  uint64_t v1;
  int32_t v2;
  int32_t v3;
  int32_t v4;
  int32_t v5;
  int32_t v6;
  int32_t v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
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
  int32_t v22;
  uint64_t v23;
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
  int32_t v35;
  int64_t v36;
  uint64_t v37;
  float v38;
  int64_t v39;
  float v40;
  float v41;
  int32_t v42;
  int64_t v43;
  float v44;
  int64_t v45;
  float v46;
  float v47;
  float v48;
  float v49;
  float v50;
  float v51;
  float v52;
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
  {
  fg168f_lc();
  t0 = ((int32_t)(v11));
  t1 = ((int32_t)(v12));
  v17 = ((int32_t)(((int32_t)((uint64_t)(t0) * (uint64_t)(t1)))));
  v18 = ((int32_t)(v14));
  v19 = ((int32_t)(v15));
  t2 = ((int32_t)(v18));
  t3 = ((int32_t)(v12));
  t4 = ((int32_t)(((int32_t)((uint64_t)(t2) * (uint64_t)(t3)))));
  t5 = ((int32_t)(v19));
  v20 = ((int32_t)(((int32_t)((uint64_t)(t4) + (uint64_t)(t5)))));
  t6 = ((int32_t)(v13));
  t7 = ((int32_t)(v7));
  v21 = ((int32_t)(((int32_t)((uint64_t)(t6) * (uint64_t)(t7)))));
  t8 = ((int32_t)(v21));
  t9 = ((int32_t)(v7));
  t10 = ((int32_t)(((int32_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  t11 = ((int32_t)(((int64_t)UINT64_C(1))));
  v22 = ((int32_t)(((int32_t)((uint64_t)(t10) - (uint64_t)(t11)))));
  t12 = ((uint64_t)(v1));
  t13 = ((int64_t)(((int64_t)(v5))));
  t14 = ((int64_t)(((int64_t)UINT64_C(4))));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t13) * (uint64_t)(t14)))));
  t16 = ((int64_t)(((int64_t)(v17))));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t15) * (uint64_t)(t16)))));
  t18 = ((int64_t)(((int64_t)UINT64_C(4))));
  t19 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t17) * (uint64_t)(t18)))))));
  v23 = ((uint64_t)(((uint64_t)((uint64_t)(t12) + (uint64_t)(t19)))));
  t20 = ((int64_t)(((int64_t)(v13))));
  t21 = ((int64_t)(v8));
  v24 = ((int64_t)(((int64_t)fg175fx_gg_div(t20, t21))));
  t22 = ((int64_t)(((int64_t)(v13))));
  t23 = ((int64_t)(v24));
  t24 = ((int64_t)(v8));
  t25 = ((int64_t)(((int64_t)((uint64_t)(t23) * (uint64_t)(t24)))));
  v25 = ((int64_t)(((int64_t)((uint64_t)(t22) - (uint64_t)(t25)))));
  t26 = ((int64_t)(v25));
  t27 = ((int64_t)(v9));
  v26 = ((int64_t)(((int64_t)fg175fx_gg_div(t26, t27))));
  t28 = ((int64_t)(v25));
  t29 = ((int64_t)(v26));
  t30 = ((int64_t)(v9));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t29) * (uint64_t)(t30)))));
  v27 = ((int64_t)(((int64_t)((uint64_t)(t28) - (uint64_t)(t31)))));
  t32 = ((int64_t)(v27));
  t33 = ((int64_t)(v10));
  v28 = ((int64_t)(((int64_t)fg175fx_gg_div(t32, t33))));
  t34 = ((int64_t)(v27));
  t35 = ((int64_t)(v28));
  t36 = ((int64_t)(v10));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) * (uint64_t)(t36)))));
  v29 = ((int64_t)(((int64_t)((uint64_t)(t34) - (uint64_t)(t37)))));
  v30 = ((int64_t)(v24));
  v31 = ((int64_t)(v26));
  v32 = ((int64_t)(v28));
  v33 = ((int64_t)(v29));
  t38 = ((int64_t)(v31));
  t39 = ((int64_t)(((int64_t)(v6))));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t38) * (uint64_t)(t39)))));
  t41 = ((int64_t)(v32));
  t42 = ((int64_t)(((int64_t)(v12))));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t41) * (uint64_t)(t42)))));
  v34 = ((int64_t)(((int64_t)((uint64_t)(t40) + (uint64_t)(t43)))));
  v35 = ((int32_t)(((int64_t)UINT64_C(0))));
  t44 = ((int64_t)(v33));
  t45 = ((int64_t)(((int64_t)(v11))));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) * (uint64_t)(t45)))));
  t47 = ((int64_t)(((int64_t)(v18))));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t46) + (uint64_t)(t47)))));
  t49 = ((int64_t)(((int64_t)(v2))));
  t50 = ((t48) >= (t49));
  if (t50) {
  {
  v35 = ((int32_t)(((int64_t)UINT64_C(1))));
  }
  }
  t51 = ((int64_t)(v32));
  t52 = ((int64_t)(((int64_t)(v12))));
  t53 = ((int64_t)(((int64_t)((uint64_t)(t51) * (uint64_t)(t52)))));
  t54 = ((int64_t)(((int64_t)(v19))));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t53) + (uint64_t)(t54)))));
  t56 = ((int64_t)(((int64_t)(v6))));
  t57 = ((t55) >= (t56));
  if (t57) {
  {
  v35 = ((int32_t)(((int64_t)UINT64_C(1))));
  }
  }
  t58 = ((int32_t)(v35));
  t59 = ((int32_t)(((int64_t)UINT64_C(1))));
  t60 = ((t58) == (t59));
  if (t60) {
  {
  goto fg175fx_gg_done;
  }
  }
  t61 = ((int64_t)(v30));
  t62 = ((int64_t)(((int64_t)(v3))));
  t63 = ((int64_t)(((int64_t)((uint64_t)(t61) * (uint64_t)(t62)))));
  t64 = ((int64_t)(((int64_t)(v2))));
  t65 = ((int64_t)(((int64_t)((uint64_t)(t63) * (uint64_t)(t64)))));
  t66 = ((int64_t)(((int64_t)UINT64_C(256))));
  t67 = ((int64_t)(((int64_t)((uint64_t)(t65) * (uint64_t)(t66)))));
  t68 = ((int64_t)(v33));
  t69 = ((int64_t)(((int64_t)(v3))));
  t70 = ((int64_t)(((int64_t)((uint64_t)(t68) * (uint64_t)(t69)))));
  t71 = ((int64_t)(((int64_t)(v11))));
  t72 = ((int64_t)(((int64_t)UINT64_C(256))));
  t73 = ((int64_t)(((int64_t)((uint64_t)(t71) * (uint64_t)(t72)))));
  t74 = ((int64_t)(((int64_t)((uint64_t)(t70) * (uint64_t)(t73)))));
  t75 = ((int64_t)(((int64_t)((uint64_t)(t67) + (uint64_t)(t74)))));
  t76 = ((int64_t)(v34));
  t77 = ((int64_t)(((int64_t)UINT64_C(256))));
  t78 = ((int64_t)(((int64_t)((uint64_t)(t76) * (uint64_t)(t77)))));
  t79 = ((int64_t)(((int64_t)((uint64_t)(t75) + (uint64_t)(t78)))));
  t80 = ((int64_t)(((int64_t)(v18))));
  t81 = ((int64_t)(((int64_t)(v3))));
  t82 = ((int64_t)(((int64_t)((uint64_t)(t80) * (uint64_t)(t81)))));
  t83 = ((int64_t)(((int64_t)(v19))));
  t84 = ((int64_t)(((int64_t)((uint64_t)(t82) + (uint64_t)(t83)))));
  t85 = ((int64_t)(((int64_t)UINT64_C(256))));
  t86 = ((int64_t)(((int64_t)((uint64_t)(t84) * (uint64_t)(t85)))));
  t87 = ((int64_t)(((int64_t)((uint64_t)(t79) + (uint64_t)(t86)))));
  t88 = ((int64_t)(((int64_t)(v16))));
  v36 = ((int64_t)(((int64_t)((uint64_t)(t87) + (uint64_t)(t88)))));
  t89 = ((int64_t)(v36));
  t90 = ((int64_t)(((int64_t)UINT64_C(4))));
  v37 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t89) * (uint64_t)(t90)))))));
  fg168f_sync();
  t91 = ((uint64_t)(v0));
  t92 = ((uint64_t)(v37));
  t93 = fg168f_ld_f32(t91, t92);
  v38 = ((float)(t93));
  t94 = ((int64_t)(((int64_t)(v22))));
  t95 = ((int64_t)(((int64_t)(v17))));
  t96 = ((int64_t)(((int64_t)((uint64_t)(t94) * (uint64_t)(t95)))));
  t97 = ((int64_t)(((int64_t)(v20))));
  t98 = ((int64_t)(((int64_t)((uint64_t)(t96) + (uint64_t)(t97)))));
  t99 = ((int64_t)(((int64_t)UINT64_C(8))));
  v39 = ((int64_t)(((int64_t)((uint64_t)(t98) * (uint64_t)(t99)))));
  t100 = ((uint64_t)(v1));
  t101 = ((uint64_t)(((uint64_t)(v39))));
  t102 = fg168f_ld_f32(t100, t101);
  v40 = ((float)(t102));
  t103 = ((uint64_t)(v1));
  t104 = ((int64_t)(v39));
  t105 = ((int64_t)(((int64_t)UINT64_C(4))));
  t106 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t104) + (uint64_t)(t105)))))));
  t107 = fg168f_ld_f32(t103, t106);
  v41 = ((float)(t107));
  t108 = ((int32_t)(v22));
  t109 = ((int32_t)(((int64_t)UINT64_C(1))));
  v42 = ((int32_t)(((int32_t)((uint64_t)(t108) - (uint64_t)(t109)))));
label1:;
  t110 = ((int32_t)(v42));
  t111 = ((int32_t)(v21));
  if (!(((t110) >= (t111)))) goto label2;
  {
  t112 = ((int64_t)(((int64_t)(v42))));
  t113 = ((int64_t)(((int64_t)(v17))));
  t114 = ((int64_t)(((int64_t)((uint64_t)(t112) * (uint64_t)(t113)))));
  t115 = ((int64_t)(((int64_t)(v20))));
  t116 = ((int64_t)(((int64_t)((uint64_t)(t114) + (uint64_t)(t115)))));
  t117 = ((int64_t)(((int64_t)UINT64_C(256))));
  t118 = ((int64_t)(((int64_t)((uint64_t)(t116) * (uint64_t)(t117)))));
  t119 = ((int64_t)(((int64_t)(v16))));
  v43 = ((int64_t)(((int64_t)((uint64_t)(t118) + (uint64_t)(t119)))));
  t120 = ((uint64_t)(v23));
  t121 = ((int64_t)(v43));
  t122 = ((int64_t)(((int64_t)UINT64_C(4))));
  t123 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t121) * (uint64_t)(t122)))))));
  t124 = fg168f_ld_f32(t120, t123);
  v44 = ((float)(t124));
  t125 = ((int64_t)(((int64_t)(v5))));
  t126 = ((int64_t)(((int64_t)(v42))));
  t127 = ((int64_t)(((int64_t)((uint64_t)(t125) + (uint64_t)(t126)))));
  t128 = ((int64_t)(((int64_t)(v17))));
  t129 = ((int64_t)(((int64_t)((uint64_t)(t127) * (uint64_t)(t128)))));
  t130 = ((int64_t)(((int64_t)(v20))));
  t131 = ((int64_t)(((int64_t)((uint64_t)(t129) + (uint64_t)(t130)))));
  t132 = ((int64_t)(((int64_t)UINT64_C(8))));
  v45 = ((int64_t)(((int64_t)((uint64_t)(t131) * (uint64_t)(t132)))));
  t133 = ((uint64_t)(v1));
  t134 = ((uint64_t)(((uint64_t)(v45))));
  t135 = fg168f_ld_f32(t133, t134);
  v46 = ((float)(t135));
  t136 = ((uint64_t)(v1));
  t137 = ((int64_t)(v45));
  t138 = ((int64_t)(((int64_t)UINT64_C(4))));
  t139 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t137) + (uint64_t)(t138)))))));
  t140 = fg168f_ld_f32(t136, t139);
  v47 = ((float)(t140));
  t141 = ((float)(v40));
  t142 = ((float)(v46));
  t143 = fg168f_fmaxf(t141, t142);
  v48 = ((float)(t143));
  t144 = ((float)(v40));
  t145 = ((float)(v48));
  v49 = ((float)(((t144) - (t145))));
  t146 = ((float)(v46));
  t147 = ((float)(v48));
  v50 = ((float)(((t146) - (t147))));
  t148 = ((float)(v49));
  t149 = fg175fx_gg_function_15(base + 0);
  t150 = ((float)(t149));
  t151 = ((t148) >= (t150));
  if (t151) {
  {
  t152 = ((float)(v49));
  t153 = fg168f_expf(t152);
  v51 = ((float)(t153));
  }
  } else {
  {
  v51 = ((float)(((float)(fg175fx_gg_double_bits(UINT64_C(0))))));
  }
  }
  t154 = ((float)(v50));
  t155 = fg175fx_gg_function_15(base + 0);
  t156 = ((float)(t155));
  t157 = ((t154) >= (t156));
  if (t157) {
  {
  t158 = ((float)(v50));
  t159 = fg168f_expf(t158);
  v52 = ((float)(t159));
  }
  } else {
  {
  v52 = ((float)(((float)(fg175fx_gg_double_bits(UINT64_C(0))))));
  }
  }
  t160 = ((float)(v51));
  t161 = ((float)(v38));
  t162 = ((float)(((t160) * (t161))));
  t163 = ((float)(v52));
  t164 = ((float)(v44));
  t165 = ((float)(((t163) * (t164))));
  v38 = ((float)(((t162) + (t165))));
  t166 = ((float)(v51));
  t167 = ((float)(v41));
  t168 = ((float)(((t166) * (t167))));
  t169 = ((float)(v52));
  t170 = ((float)(v47));
  t171 = ((float)(((t169) * (t170))));
  v41 = ((float)(((t168) + (t171))));
  v40 = ((float)(v48));
  t172 = v42;
  t173 = ((int32_t)(((int64_t)UINT64_C(1))));
  v42 = ((int32_t)((uint64_t)(t172) - (uint64_t)(t173)));
  }
  goto label1;
label2:;
  t174 = ((uint64_t)(v0));
  t175 = ((uint64_t)(v37));
  t176 = ((float)(v38));
  t177 = ((float)(v41));
  t178 = ((float)(((t176) / (t177))));
  fg168f_st_f32(t174, t175, t178);
  }
fg175fx_gg_done:;
}
static __device__ __forceinline__ int32_t fg175fx_gg_function_7(int32_t p0, int64_t base) {
  int32_t t0;
  int32_t t1;
  int32_t t2;
  int32_t t3;
  int32_t t4;
  int32_t t5;
  int32_t t6;
  int32_t t7;
  int32_t t8;
  int32_t t9;
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
  int32_t t20;
  int32_t t21;
  int32_t t22;
  int32_t t23;
  int32_t t24;
  int32_t t25;
  int32_t v0;
  int32_t v1;
  v0 = p0;
  {
  v1 = ((int32_t)(v0));
  t1 = ((int32_t)(v1));
  t2 = ((int32_t)(v1));
  t3 = ((int32_t)(((int64_t)UINT64_C(16))));
  t4 = fg168f_shfl_xor_i32(t2, t3);
  t5 = ((int32_t)(t4));
  v1 = ((int32_t)(((t1) & (t5))));
  t6 = ((int32_t)(v1));
  t7 = ((int32_t)(v1));
  t8 = ((int32_t)(((int64_t)UINT64_C(8))));
  t9 = fg168f_shfl_xor_i32(t7, t8);
  t10 = ((int32_t)(t9));
  v1 = ((int32_t)(((t6) & (t10))));
  t11 = ((int32_t)(v1));
  t12 = ((int32_t)(v1));
  t13 = ((int32_t)(((int64_t)UINT64_C(4))));
  t14 = fg168f_shfl_xor_i32(t12, t13);
  t15 = ((int32_t)(t14));
  v1 = ((int32_t)(((t11) & (t15))));
  t16 = ((int32_t)(v1));
  t17 = ((int32_t)(v1));
  t18 = ((int32_t)(((int64_t)UINT64_C(2))));
  t19 = fg168f_shfl_xor_i32(t17, t18);
  t20 = ((int32_t)(t19));
  v1 = ((int32_t)(((t16) & (t20))));
  t21 = ((int32_t)(v1));
  t22 = ((int32_t)(v1));
  t23 = ((int32_t)(((int64_t)UINT64_C(1))));
  t24 = fg168f_shfl_xor_i32(t22, t23);
  t25 = ((int32_t)(t24));
  v1 = ((int32_t)(((t21) & (t25))));
  t0 = ((int32_t)(v1));
  goto fg175fx_gg_done;
  }
fg175fx_gg_done:;
  return t0;
}
static __device__ __forceinline__ int32_t fg175fx_gg_function_6(uint32_t p0, int64_t base) {
  int32_t t0;
  uint32_t t1;
  uint32_t t2;
  uint32_t t3;
  uint32_t t4;
  uint8_t t5;
  uint32_t v0;
  v0 = p0;
  {
  t1 = ((uint32_t)(v0));
  t2 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(32767))))));
  t3 = ((uint32_t)(((t1) & (t2))));
  t4 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(31744))))));
  t5 = ((t3) == (t4));
  if (t5) {
  {
  t0 = ((int32_t)(((int64_t)UINT64_C(1))));
  goto fg175fx_gg_done;
  }
  }
  t0 = ((int32_t)(((int64_t)UINT64_C(0))));
  goto fg175fx_gg_done;
  }
fg175fx_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg175fx_gg_function_1(uint64_t p0, uint64_t p1, int32_t p2, int64_t p3, int64_t p4, int32_t p5, int32_t p6, int32_t p7, int32_t p8, int32_t p9, uint64_t p10, int64_t base) {
  int32_t t0;
  int32_t t1;
  uint8_t t2;
  uint64_t t3;
  uint64_t t4;
  int32_t t5;
  int32_t t6;
  int32_t t7;
  int32_t t8;
  int32_t t9;
  int32_t t10;
  int32_t t11;
  int32_t t12;
  int32_t t13;
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int32_t t29;
  int32_t t30;
  uint64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint64_t t38;
  uint32_t t39;
  uint32_t t40;
  uint32_t t41;
  uint32_t t42;
  uint32_t t43;
  int32_t t44;
  uint32_t t45;
  int32_t t46;
  int32_t t47;
  int32_t t48;
  uint32_t t49;
  int32_t t50;
  int32_t t51;
  int32_t t52;
  int32_t t53;
  int32_t t54;
  int32_t t55;
  int32_t t56;
  int32_t t57;
  int32_t t58;
  uint8_t t59;
  uint64_t t60;
  int32_t t61;
  int32_t t62;
  uint64_t t63;
  int32_t t64;
  uint64_t t65;
  int32_t t66;
  int32_t t67;
  uint64_t t68;
  int32_t t69;
  int32_t t70;
  int32_t t71;
  int32_t t72;
  uint8_t t73;
  int32_t t74;
  int32_t t75;
  uint8_t t76;
  int32_t t77;
  int32_t t78;
  int32_t t79;
  int32_t t80;
  int32_t t81;
  int32_t t82;
  uint8_t t83;
  uint64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  uint64_t t91;
  int32_t t92;
  uint64_t v0;
  uint64_t v1;
  int32_t v2;
  int64_t v3;
  int64_t v4;
  int32_t v5;
  int32_t v6;
  int32_t v7;
  int32_t v8;
  int32_t v9;
  uint64_t v10;
  int32_t v11;
  int32_t v12;
  int32_t v13;
  int32_t v14;
  int32_t v15;
  int64_t v16;
  int32_t v17;
  uint32_t v18;
  uint32_t v19;
  uint32_t v20;
  int32_t v21;
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
  v11 = ((int32_t)(v7));
  v12 = ((int32_t)(v6));
  t0 = ((int32_t)(v9));
  t1 = ((int32_t)(((int64_t)UINT64_C(32))));
  t2 = ((t0) < (t1));
  if (t2) {
  {
  t3 = ((uint64_t)(v10));
  t4 = ((uint64_t)(((uint64_t)(v9))));
  t5 = ((int32_t)(((int64_t)UINT64_C(1))));
  fg168f_sm_st_i32(t3, t4, t5);
  }
  }
  fg168f_sync();
  fg168f_barrier();
  t6 = ((int32_t)(v2));
  t7 = ((int32_t)(((int64_t)UINT64_C(1))));
  t8 = ((int32_t)(((int32_t)((uint64_t)(t6) - (uint64_t)(t7)))));
  t9 = ((int32_t)(((int64_t)UINT64_C(256))));
  v13 = ((int32_t)(((int32_t)((uint64_t)(t8) * (uint64_t)(t9)))));
  v14 = ((int32_t)(((int64_t)UINT64_C(1))));
label1:;
  t10 = ((int32_t)(v14));
  t11 = ((int32_t)(((int64_t)UINT64_C(1))));
  if (!(((t10) == (t11)))) goto label2;
  {
  t12 = ((int32_t)(v13));
  t13 = ((int32_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) < (t13));
  if (t14) {
  {
  v14 = ((int32_t)(((int64_t)UINT64_C(0))));
  }
  } else {
  {
  v15 = ((int32_t)(((int64_t)UINT64_C(1))));
  t15 = ((int64_t)(((int64_t)(v11))));
  t16 = ((int64_t)(v4));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t15) * (uint64_t)(t16)))));
  t18 = ((int64_t)(((int64_t)(v12))));
  t19 = ((int64_t)(((int64_t)(v5))));
  t20 = ((int64_t)(v3));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t21)))));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t22)))));
  t24 = ((int64_t)(((int64_t)(v13))));
  t25 = ((int64_t)(((int64_t)UINT64_C(2))));
  t26 = ((int64_t)(((int64_t)fg175fx_gg_div(t24, t25))));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t23) + (uint64_t)(t26)))));
  t28 = ((int64_t)(((int64_t)(v9))));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t27) + (uint64_t)(t28)))));
  v17 = ((int32_t)(((int64_t)UINT64_C(0))));
label4:;
  t29 = ((int32_t)(v17));
  t30 = ((int32_t)(v5));
  if (!(((t29) < (t30)))) goto label5;
  {
  t31 = ((uint64_t)(v0));
  t32 = ((int64_t)(v16));
  t33 = ((int64_t)(((int64_t)(v17))));
  t34 = ((int64_t)(v3));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t33) * (uint64_t)(t34)))));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t32) + (uint64_t)(t35)))));
  t37 = ((int64_t)(((int64_t)UINT64_C(4))));
  t38 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t36) * (uint64_t)(t37)))))));
  t39 = fg168f_ld_u32(t31, t38);
  v18 = ((uint32_t)(t39));
  t40 = ((uint32_t)(v18));
  t41 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(65535))))));
  v19 = ((uint32_t)(((t40) & (t41))));
  t42 = ((uint32_t)(v18));
  t43 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(16))))));
  v20 = ((uint32_t)(((t42) >> ((uint64_t)(t43) & 31))));
  t44 = ((int32_t)(v15));
  t45 = ((uint32_t)(v19));
  t46 = fg175fx_gg_function_6(t45, base + 0);
  t47 = ((int32_t)(t46));
  t48 = ((int32_t)(((int32_t)((uint64_t)(t44) * (uint64_t)(t47)))));
  t49 = ((uint32_t)(v20));
  t50 = fg175fx_gg_function_6(t49, base + 0);
  t51 = ((int32_t)(t50));
  v15 = ((int32_t)(((int32_t)((uint64_t)(t48) * (uint64_t)(t51)))));
  t52 = v17;
  t53 = ((int32_t)(((int64_t)UINT64_C(1))));
  v17 = ((int32_t)((uint64_t)(t52) + (uint64_t)(t53)));
  }
  goto label4;
label5:;
  t54 = ((int32_t)(v15));
  v15 = fg175fx_gg_function_7(t54, base + 0);
  t55 = ((int32_t)(v9));
  t56 = ((int32_t)(((int64_t)UINT64_C(32))));
  t57 = ((int32_t)(((int32_t)fg175fx_gg_mod(t55, t56))));
  t58 = ((int32_t)(((int64_t)UINT64_C(0))));
  t59 = ((t57) == (t58));
  if (t59) {
  {
  t60 = ((uint64_t)(v10));
  t61 = ((int32_t)(v9));
  t62 = ((int32_t)(((int64_t)UINT64_C(32))));
  t63 = ((uint64_t)(((uint64_t)(((int32_t)fg175fx_gg_div(t61, t62))))));
  t64 = ((int32_t)(v15));
  fg168f_sm_st_i32(t60, t63, t64);
  }
  }
  fg168f_barrier();
  t65 = ((uint64_t)(v10));
  t66 = ((int32_t)(v9));
  t67 = ((int32_t)(((int64_t)UINT64_C(32))));
  t68 = ((uint64_t)(((uint64_t)(((int32_t)fg175fx_gg_mod(t66, t67))))));
  t69 = fg168f_sm_i32(t65, t68);
  v15 = ((int32_t)(t69));
  fg168f_barrier();
  t70 = ((int32_t)(v15));
  v15 = fg175fx_gg_function_7(t70, base + 0);
  t71 = ((int32_t)(v15));
  t72 = ((int32_t)(((int64_t)UINT64_C(0))));
  t73 = ((t71) == (t72));
  if (t73) {
  {
  v14 = ((int32_t)(((int64_t)UINT64_C(0))));
  }
  }
  t74 = ((int32_t)(v14));
  t75 = ((int32_t)(((int64_t)UINT64_C(1))));
  t76 = ((t74) == (t75));
  if (t76) {
  {
  t77 = v13;
  t78 = ((int32_t)(((int64_t)UINT64_C(256))));
  v13 = ((int32_t)((uint64_t)(t77) - (uint64_t)(t78)));
  }
  }
  }
  }
  }
  goto label1;
label2:;
  t79 = ((int32_t)(v13));
  t80 = ((int32_t)(((int64_t)UINT64_C(256))));
  v21 = ((int32_t)(((int32_t)((uint64_t)(t79) + (uint64_t)(t80)))));
  t81 = ((int32_t)(v9));
  t82 = ((int32_t)(((int64_t)UINT64_C(0))));
  t83 = ((t81) == (t82));
  if (t83) {
  {
  t84 = ((uint64_t)(v1));
  t85 = ((int64_t)(((int64_t)(v11))));
  t86 = ((int64_t)(((int64_t)(v8))));
  t87 = ((int64_t)(((int64_t)((uint64_t)(t85) * (uint64_t)(t86)))));
  t88 = ((int64_t)(((int64_t)(v12))));
  t89 = ((int64_t)(((int64_t)((uint64_t)(t87) + (uint64_t)(t88)))));
  t90 = ((int64_t)(((int64_t)UINT64_C(4))));
  t91 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t89) * (uint64_t)(t90)))))));
  t92 = ((int32_t)(v21));
  fg168f_st_i32(t84, t91, t92);
  }
  }
  }
fg175fx_gg_done:;
}
#define FG175F_MASK fg175fx_gg_function_1
#define FG175F_UNIFORM fg175fx_gg_function_11
#define FG175F_GENERAL fg175fx_gg_function_18
