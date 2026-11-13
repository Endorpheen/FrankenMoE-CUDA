#pragma once
static __device__ __forceinline__ void fg248dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg248dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg248dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg248dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg248dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg248dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ void fg248dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg248dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base);
static __device__ __forceinline__ void fg248dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg248dd_gg_div(int64_t a, int64_t b) { if (!b) fg248dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg248dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg248dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg248dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg248dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg248dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg248dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg248dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg248dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ void fg248dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg248d_fail)(int64_t);
#endif
  fg248d_fail(t2);
  }
  }
  }
fg248dd_gg_done:;
}
static __device__ __forceinline__ void fg248dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  uint64_t t30;
  int64_t t31;
  int64_t t32;
  uint64_t t33;
  uint64_t t34;
  float t35;
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
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  uint64_t t54;
  uint64_t t55;
  uint64_t t56;
  uint32_t t57;
  uint64_t t58;
  uint64_t t59;
  uint64_t t60;
  int32_t t61;
  uint64_t t62;
  int64_t t63;
  int64_t t64;
  uint64_t t65;
  uint64_t t66;
  int32_t t67;
  uint64_t t68;
  int64_t t69;
  int64_t t70;
  uint64_t t71;
  uint64_t t72;
  int32_t t73;
  uint64_t t74;
  int64_t t75;
  int64_t t76;
  uint64_t t77;
  uint64_t t78;
  int32_t t79;
  uint64_t t80;
  uint64_t t81;
  uint64_t t82;
  uint32_t t83;
  uint32_t t84;
  uint32_t t85;
  uint32_t t86;
  uint32_t t87;
  int64_t t88;
  uint32_t t89;
  uint32_t t90;
  uint32_t t91;
  int64_t t92;
  uint32_t t93;
  int64_t t94;
  int64_t t95;
  uint64_t t96;
  int64_t t97;
  int64_t t98;
  uint64_t t99;
  uint64_t t100;
  uint32_t t101;
  uint32_t t102;
  uint32_t t103;
  uint32_t t104;
  uint32_t t105;
  int64_t t106;
  uint32_t t107;
  uint32_t t108;
  uint32_t t109;
  int64_t t110;
  uint32_t t111;
  int64_t t112;
  int64_t t113;
  uint64_t t114;
  uint64_t t115;
  uint64_t t116;
  uint32_t t117;
  uint32_t t118;
  int64_t t119;
  uint32_t t120;
  uint32_t t121;
  int64_t t122;
  uint32_t t123;
  uint32_t t124;
  uint32_t t125;
  int64_t t126;
  uint32_t t127;
  int64_t t128;
  int64_t t129;
  uint64_t t130;
  int64_t t131;
  int64_t t132;
  uint64_t t133;
  uint64_t t134;
  uint32_t t135;
  uint32_t t136;
  int64_t t137;
  uint32_t t138;
  uint32_t t139;
  int64_t t140;
  uint32_t t141;
  uint32_t t142;
  uint32_t t143;
  int64_t t144;
  uint32_t t145;
  int64_t t146;
  int64_t t147;
  uint64_t t148;
  int64_t t149;
  int64_t t150;
  uint64_t t151;
  uint64_t t152;
  float t153;
  float t154;
  float t155;
  float t156;
  float t157;
  uint64_t t158;
  int64_t t159;
  int64_t t160;
  int64_t t161;
  int64_t t162;
  uint64_t t163;
  uint64_t t164;
  float t165;
  float t166;
  float t167;
  float t168;
  float t169;
  uint64_t t170;
  int64_t t171;
  int64_t t172;
  int64_t t173;
  int64_t t174;
  uint64_t t175;
  uint64_t t176;
  float t177;
  float t178;
  float t179;
  float t180;
  float t181;
  uint64_t t182;
  int64_t t183;
  int64_t t184;
  int64_t t185;
  int64_t t186;
  uint64_t t187;
  uint64_t t188;
  float t189;
  float t190;
  float t191;
  float t192;
  float t193;
  uint64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  float v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  uint32_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  int64_t v17;
  int64_t v18;
  int64_t v19;
  int64_t v20;
  int64_t v21;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v3));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  t3 = ((t4) < (t5));
  }
  t6 = t3;
  t7 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg248dd_gg_function_2(t6, t7, base + 0);
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)UINT64_C(32))));
  v4 = ((int64_t)(((int64_t)fg248dd_gg_div(t8, t9))));
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(((int64_t)UINT64_C(32))));
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t10) - (uint64_t)(t13)))));
  t14 = ((int64_t)(((int64_t)UINT64_C(8))));
  t15 = ((int64_t)(v4));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))));
  t17 = ((int64_t)(v5));
  t18 = ((int64_t)(((int64_t)UINT64_C(16))));
  t19 = ((int64_t)(((int64_t)fg248dd_gg_div(t17, t18))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t16) + (uint64_t)(t19)))));
  t20 = ((int64_t)(v2));
  t21 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(210))))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  t22 = ((int64_t)(v2));
  t23 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(256))))));
  t24 = ((int64_t)(((int64_t)((uint64_t)(t22) * (uint64_t)(t23)))));
  t25 = ((int64_t)(((int64_t)UINT64_C(128))));
  t26 = ((int64_t)(v4));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t25) * (uint64_t)(t26)))));
  t28 = ((int64_t)(((int64_t)((uint64_t)(t24) + (uint64_t)(t27)))));
  t29 = ((int64_t)(v5));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t28) + (uint64_t)(t29)))));
  t30 = ((uint64_t)(v0));
  t31 = ((int64_t)(v7));
  t32 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(208))))));
  t33 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t32)))))));
  t34 = ((uint64_t)(((uint64_t)((uint64_t)(t30) + (uint64_t)(t33)))));
#ifndef __cplusplus
  extern float (fg248d_ld_half)(uint64_t);
#endif
  t35 = fg248d_ld_half(t34);
  v9 = ((float)(t35));
  t36 = ((int64_t)(v7));
  t37 = ((int64_t)(((int64_t)UINT64_C(64))));
  t38 = ((int64_t)(v4));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t37) * (uint64_t)(t38)))));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t36) + (uint64_t)(t39)))));
  t41 = ((int64_t)(v5));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t40) + (uint64_t)(t41)))));
  t42 = ((int64_t)(v7));
  t43 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(128))))));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) + (uint64_t)(t43)))));
  t45 = ((int64_t)(((int64_t)UINT64_C(32))));
  t46 = ((int64_t)(v4));
  t47 = ((int64_t)(((int64_t)((uint64_t)(t45) * (uint64_t)(t46)))));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t44) + (uint64_t)(t47)))));
  t49 = ((int64_t)(v5));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t48) + (uint64_t)(t49)))));
  t50 = ((int64_t)(v7));
  t51 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(192))))));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t50) + (uint64_t)(t51)))));
  t53 = ((int64_t)(v6));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t53)))));
  t54 = ((uint64_t)(v0));
  t55 = ((uint64_t)(((uint64_t)(v11))));
  t56 = ((uint64_t)(((uint64_t)((uint64_t)(t54) + (uint64_t)(t55)))));
#ifndef __cplusplus
  extern uint32_t (fg248d_ld_u8)(uint64_t);
#endif
  t57 = fg248d_ld_u8(t56);
  v13 = ((uint32_t)(t57));
  t58 = ((uint64_t)(v0));
  t59 = ((uint64_t)(((uint64_t)(v12))));
  t60 = ((uint64_t)(((uint64_t)((uint64_t)(t58) + (uint64_t)(t59)))));
#ifndef __cplusplus
  extern int32_t (fg248d_ld_i8)(uint64_t);
#endif
  t61 = fg248d_ld_i8(t60);
  v14 = ((int64_t)(((int64_t)(t61))));
  t62 = ((uint64_t)(v0));
  t63 = ((int64_t)(v12));
  t64 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t65 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t63) + (uint64_t)(t64)))))));
  t66 = ((uint64_t)(((uint64_t)((uint64_t)(t62) + (uint64_t)(t65)))));
#ifndef __cplusplus
  extern int32_t (fg248d_ld_i8)(uint64_t);
#endif
  t67 = fg248d_ld_i8(t66);
  v15 = ((int64_t)(((int64_t)(t67))));
  t68 = ((uint64_t)(v0));
  t69 = ((int64_t)(v12));
  t70 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t71 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t69) + (uint64_t)(t70)))))));
  t72 = ((uint64_t)(((uint64_t)((uint64_t)(t68) + (uint64_t)(t71)))));
#ifndef __cplusplus
  extern int32_t (fg248d_ld_i8)(uint64_t);
#endif
  t73 = fg248d_ld_i8(t72);
  v16 = ((int64_t)(((int64_t)(t73))));
  t74 = ((uint64_t)(v0));
  t75 = ((int64_t)(v12));
  t76 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(6))))));
  t77 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t75) + (uint64_t)(t76)))))));
  t78 = ((uint64_t)(((uint64_t)((uint64_t)(t74) + (uint64_t)(t77)))));
#ifndef __cplusplus
  extern int32_t (fg248d_ld_i8)(uint64_t);
#endif
  t79 = fg248d_ld_i8(t78);
  v17 = ((int64_t)(((int64_t)(t79))));
  t80 = ((uint64_t)(v0));
  t81 = ((uint64_t)(((uint64_t)(v10))));
  t82 = ((uint64_t)(((uint64_t)((uint64_t)(t80) + (uint64_t)(t81)))));
#ifndef __cplusplus
  extern uint32_t (fg248d_ld_u8)(uint64_t);
#endif
  t83 = fg248d_ld_u8(t82);
  t84 = ((uint32_t)(t83));
  t85 = ((uint32_t)(((int64_t)UINT64_C(15))));
  t86 = ((uint32_t)(((t84) & (t85))));
  t87 = ((uint32_t)(v13));
  t88 = ((int64_t)(((int64_t)UINT64_C(0))));
  t89 = ((uint32_t)(((t87) >> ((uint64_t)(t88) & 31))));
  t90 = ((uint32_t)(((int64_t)UINT64_C(3))));
  t91 = ((uint32_t)(((t89) & (t90))));
  t92 = ((int64_t)(((int64_t)UINT64_C(4))));
  t93 = ((uint32_t)(((uint32_t)((uint64_t)(t91) << ((uint64_t)(t92) & 31)))));
  t94 = ((int64_t)(((int64_t)(((t86) | (t93))))));
  t95 = ((int64_t)(((int64_t)UINT64_C(32))));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t94) - (uint64_t)(t95)))));
  t96 = ((uint64_t)(v0));
  t97 = ((int64_t)(v10));
  t98 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t99 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t97) + (uint64_t)(t98)))))));
  t100 = ((uint64_t)(((uint64_t)((uint64_t)(t96) + (uint64_t)(t99)))));
#ifndef __cplusplus
  extern uint32_t (fg248d_ld_u8)(uint64_t);
#endif
  t101 = fg248d_ld_u8(t100);
  t102 = ((uint32_t)(t101));
  t103 = ((uint32_t)(((int64_t)UINT64_C(15))));
  t104 = ((uint32_t)(((t102) & (t103))));
  t105 = ((uint32_t)(v13));
  t106 = ((int64_t)(((int64_t)UINT64_C(2))));
  t107 = ((uint32_t)(((t105) >> ((uint64_t)(t106) & 31))));
  t108 = ((uint32_t)(((int64_t)UINT64_C(3))));
  t109 = ((uint32_t)(((t107) & (t108))));
  t110 = ((int64_t)(((int64_t)UINT64_C(4))));
  t111 = ((uint32_t)(((uint32_t)((uint64_t)(t109) << ((uint64_t)(t110) & 31)))));
  t112 = ((int64_t)(((int64_t)(((t104) | (t111))))));
  t113 = ((int64_t)(((int64_t)UINT64_C(32))));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t112) - (uint64_t)(t113)))));
  t114 = ((uint64_t)(v0));
  t115 = ((uint64_t)(((uint64_t)(v10))));
  t116 = ((uint64_t)(((uint64_t)((uint64_t)(t114) + (uint64_t)(t115)))));
#ifndef __cplusplus
  extern uint32_t (fg248d_ld_u8)(uint64_t);
#endif
  t117 = fg248d_ld_u8(t116);
  t118 = ((uint32_t)(t117));
  t119 = ((int64_t)(((int64_t)UINT64_C(4))));
  t120 = ((uint32_t)(((t118) >> ((uint64_t)(t119) & 31))));
  t121 = ((uint32_t)(v13));
  t122 = ((int64_t)(((int64_t)UINT64_C(4))));
  t123 = ((uint32_t)(((t121) >> ((uint64_t)(t122) & 31))));
  t124 = ((uint32_t)(((int64_t)UINT64_C(3))));
  t125 = ((uint32_t)(((t123) & (t124))));
  t126 = ((int64_t)(((int64_t)UINT64_C(4))));
  t127 = ((uint32_t)(((uint32_t)((uint64_t)(t125) << ((uint64_t)(t126) & 31)))));
  t128 = ((int64_t)(((int64_t)(((t120) | (t127))))));
  t129 = ((int64_t)(((int64_t)UINT64_C(32))));
  v20 = ((int64_t)(((int64_t)((uint64_t)(t128) - (uint64_t)(t129)))));
  t130 = ((uint64_t)(v0));
  t131 = ((int64_t)(v10));
  t132 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t133 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t131) + (uint64_t)(t132)))))));
  t134 = ((uint64_t)(((uint64_t)((uint64_t)(t130) + (uint64_t)(t133)))));
#ifndef __cplusplus
  extern uint32_t (fg248d_ld_u8)(uint64_t);
#endif
  t135 = fg248d_ld_u8(t134);
  t136 = ((uint32_t)(t135));
  t137 = ((int64_t)(((int64_t)UINT64_C(4))));
  t138 = ((uint32_t)(((t136) >> ((uint64_t)(t137) & 31))));
  t139 = ((uint32_t)(v13));
  t140 = ((int64_t)(((int64_t)UINT64_C(6))));
  t141 = ((uint32_t)(((t139) >> ((uint64_t)(t140) & 31))));
  t142 = ((uint32_t)(((int64_t)UINT64_C(3))));
  t143 = ((uint32_t)(((t141) & (t142))));
  t144 = ((int64_t)(((int64_t)UINT64_C(4))));
  t145 = ((uint32_t)(((uint32_t)((uint64_t)(t143) << ((uint64_t)(t144) & 31)))));
  t146 = ((int64_t)(((int64_t)(((t138) | (t145))))));
  t147 = ((int64_t)(((int64_t)UINT64_C(32))));
  v21 = ((int64_t)(((int64_t)((uint64_t)(t146) - (uint64_t)(t147)))));
  t148 = ((uint64_t)(v1));
  t149 = ((int64_t)(v8));
  t150 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t151 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t149) * (uint64_t)(t150)))))));
  t152 = ((uint64_t)(((uint64_t)((uint64_t)(t148) + (uint64_t)(t151)))));
  t153 = ((float)(v9));
  t154 = ((float)(((float)(v14))));
  t155 = ((float)(((t153) * (t154))));
  t156 = ((float)(((float)(v18))));
  t157 = ((float)(((t155) * (t156))));
#ifndef __cplusplus
  extern void (fg248d_st_f32)(uint64_t, float);
#endif
  fg248d_st_f32(t152, t157);
  t158 = ((uint64_t)(v1));
  t159 = ((int64_t)(v8));
  t160 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t161 = ((int64_t)(((int64_t)((uint64_t)(t159) + (uint64_t)(t160)))));
  t162 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t163 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t161) * (uint64_t)(t162)))))));
  t164 = ((uint64_t)(((uint64_t)((uint64_t)(t158) + (uint64_t)(t163)))));
  t165 = ((float)(v9));
  t166 = ((float)(((float)(v15))));
  t167 = ((float)(((t165) * (t166))));
  t168 = ((float)(((float)(v19))));
  t169 = ((float)(((t167) * (t168))));
#ifndef __cplusplus
  extern void (fg248d_st_f32)(uint64_t, float);
#endif
  fg248d_st_f32(t164, t169);
  t170 = ((uint64_t)(v1));
  t171 = ((int64_t)(v8));
  t172 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(64))))));
  t173 = ((int64_t)(((int64_t)((uint64_t)(t171) + (uint64_t)(t172)))));
  t174 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t175 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t173) * (uint64_t)(t174)))))));
  t176 = ((uint64_t)(((uint64_t)((uint64_t)(t170) + (uint64_t)(t175)))));
  t177 = ((float)(v9));
  t178 = ((float)(((float)(v16))));
  t179 = ((float)(((t177) * (t178))));
  t180 = ((float)(((float)(v20))));
  t181 = ((float)(((t179) * (t180))));
#ifndef __cplusplus
  extern void (fg248d_st_f32)(uint64_t, float);
#endif
  fg248d_st_f32(t176, t181);
  t182 = ((uint64_t)(v1));
  t183 = ((int64_t)(v8));
  t184 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(96))))));
  t185 = ((int64_t)(((int64_t)((uint64_t)(t183) + (uint64_t)(t184)))));
  t186 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t187 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t185) * (uint64_t)(t186)))))));
  t188 = ((uint64_t)(((uint64_t)((uint64_t)(t182) + (uint64_t)(t187)))));
  t189 = ((float)(v9));
  t190 = ((float)(((float)(v17))));
  t191 = ((float)(((t189) * (t190))));
  t192 = ((float)(((float)(v21))));
  t193 = ((float)(((t191) * (t192))));
#ifndef __cplusplus
  extern void (fg248d_st_f32)(uint64_t, float);
#endif
  fg248d_st_f32(t188, t193);
  }
fg248dd_gg_done:;
}
