#pragma once
static __device__ __forceinline__ void fg249dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg249dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg249dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg249dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg249dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg249dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ int64_t fg249dd_gg_function_7(int64_t p0, int64_t base);
static __device__ __forceinline__ int64_t fg249dd_gg_function_5(uint64_t p0, int64_t p1, int64_t p2, int64_t base);
static __device__ __forceinline__ void fg249dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg249dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base);
static __device__ __forceinline__ void fg249dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg249dd_gg_div(int64_t a, int64_t b) { if (!b) fg249dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg249dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg249dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg249dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg249dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg249dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg249dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg249dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg249dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ int64_t fg249dd_gg_function_7(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) > (t2));
  if (t3) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  goto fg249dd_gg_done;
  }
  }
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  }
fg249dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ int64_t fg249dd_gg_function_5(uint64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t t7;
  uint32_t t8;
  uint32_t t9;
  uint32_t t10;
  uint64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  uint64_t t16;
  uint32_t t17;
  uint32_t t18;
  uint32_t t19;
  uint64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  uint64_t t25;
  uint32_t t26;
  uint64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  uint64_t t32;
  uint32_t t33;
  uint64_t t34;
  int64_t t35;
  int64_t t36;
  uint64_t t37;
  uint32_t t38;
  uint32_t t39;
  uint32_t t40;
  int64_t t41;
  uint32_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint32_t t47;
  int64_t t48;
  int64_t t49;
  uint32_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  uint32_t v5;
  uint32_t v6;
  uint32_t v7;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  v3 = ((int64_t)(((int64_t)UINT64_C(0))));
  v4 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  t4 = ((uint64_t)(v0));
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(v2));
  t7 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t5) + (uint64_t)(t6)))))));
#ifndef __cplusplus
  extern uint32_t (fg249d_ld_u8)(uint64_t, uint64_t);
#endif
  t8 = fg249d_ld_u8(t4, t7);
  t9 = ((uint32_t)(t8));
  t10 = ((uint32_t)(((int64_t)UINT64_C(63))));
  v3 = ((int64_t)(((int64_t)(((t9) & (t10))))));
  t11 = ((uint64_t)(v0));
  t12 = ((int64_t)(v1));
  t13 = ((int64_t)(v2));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t12) + (uint64_t)(t13)))));
  t15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t16 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t14) + (uint64_t)(t15)))))));
#ifndef __cplusplus
  extern uint32_t (fg249d_ld_u8)(uint64_t, uint64_t);
#endif
  t17 = fg249d_ld_u8(t11, t16);
  t18 = ((uint32_t)(t17));
  t19 = ((uint32_t)(((int64_t)UINT64_C(63))));
  v4 = ((int64_t)(((int64_t)(((t18) & (t19))))));
  }
  } else {
  {
  t20 = ((uint64_t)(v0));
  t21 = ((int64_t)(v1));
  t22 = ((int64_t)(v2));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) + (uint64_t)(t22)))));
  t24 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t25 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t23) + (uint64_t)(t24)))))));
#ifndef __cplusplus
  extern uint32_t (fg249d_ld_u8)(uint64_t, uint64_t);
#endif
  t26 = fg249d_ld_u8(t20, t25);
  v5 = ((uint32_t)(t26));
  t27 = ((uint64_t)(v0));
  t28 = ((int64_t)(v1));
  t29 = ((int64_t)(v2));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) + (uint64_t)(t29)))));
  t31 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t32 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t30) - (uint64_t)(t31)))))));
#ifndef __cplusplus
  extern uint32_t (fg249d_ld_u8)(uint64_t, uint64_t);
#endif
  t33 = fg249d_ld_u8(t27, t32);
  v6 = ((uint32_t)(t33));
  t34 = ((uint64_t)(v0));
  t35 = ((int64_t)(v1));
  t36 = ((int64_t)(v2));
  t37 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t35) + (uint64_t)(t36)))))));
#ifndef __cplusplus
  extern uint32_t (fg249d_ld_u8)(uint64_t, uint64_t);
#endif
  t38 = fg249d_ld_u8(t34, t37);
  v7 = ((uint32_t)(t38));
  t39 = ((uint32_t)(v5));
  t40 = ((uint32_t)(((int64_t)UINT64_C(15))));
  t41 = ((int64_t)(((int64_t)(((t39) & (t40))))));
  t42 = ((uint32_t)(v6));
  t43 = ((int64_t)(((int64_t)UINT64_C(6))));
  t44 = ((int64_t)(((int64_t)(((t42) >> ((uint64_t)(t43) & 31))))));
  t45 = ((int64_t)(((int64_t)UINT64_C(4))));
  t46 = ((int64_t)(((int64_t)(((int64_t)((uint64_t)(t44) << ((uint64_t)(t45) & 63)))))));
  v3 = ((int64_t)(((t41) | (t46))));
  t47 = ((uint32_t)(v5));
  t48 = ((int64_t)(((int64_t)UINT64_C(4))));
  t49 = ((int64_t)(((int64_t)(((t47) >> ((uint64_t)(t48) & 31))))));
  t50 = ((uint32_t)(v7));
  t51 = ((int64_t)(((int64_t)UINT64_C(6))));
  t52 = ((int64_t)(((int64_t)(((t50) >> ((uint64_t)(t51) & 31))))));
  t53 = ((int64_t)(((int64_t)UINT64_C(4))));
  t54 = ((int64_t)(((int64_t)(((int64_t)((uint64_t)(t52) << ((uint64_t)(t53) & 63)))))));
  v4 = ((int64_t)(((t49) | (t54))));
  }
  }
  t55 = ((int64_t)(v3));
  t56 = ((int64_t)(((int64_t)UINT64_C(8))));
  t57 = ((int64_t)(((int64_t)((uint64_t)(t55) << ((uint64_t)(t56) & 63)))));
  t58 = ((int64_t)(v4));
  t0 = ((int64_t)(((t57) | (t58))));
  goto fg249dd_gg_done;
  }
fg249dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg249dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg249d_fail)(int64_t);
#endif
  fg249d_fail(t2);
  }
  }
  }
fg249dd_gg_done:;
}
static __device__ __forceinline__ void fg249dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t base) {
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
  uint64_t t26;
  uint64_t t27;
  float t28;
  uint64_t t29;
  int64_t t30;
  int64_t t31;
  uint64_t t32;
  float t33;
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
  uint64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  uint64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  float t62;
  int64_t t63;
  int64_t t64;
  float t65;
  float t66;
  int64_t t67;
  int64_t t68;
  float t69;
  float t70;
  int64_t t71;
  int64_t t72;
  float t73;
  float t74;
  int64_t t75;
  int64_t t76;
  float t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  int64_t t83;
  uint64_t t84;
  uint64_t t85;
  uint32_t t86;
  uint64_t t87;
  int64_t t88;
  int64_t t89;
  uint64_t t90;
  uint32_t t91;
  uint64_t t92;
  uint64_t t93;
  uint32_t t94;
  uint64_t t95;
  int64_t t96;
  int64_t t97;
  uint64_t t98;
  uint32_t t99;
  uint32_t t100;
  uint32_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  uint32_t t108;
  uint32_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  uint32_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  uint32_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  uint64_t t132;
  int64_t t133;
  int64_t t134;
  uint64_t t135;
  float t136;
  float t137;
  float t138;
  float t139;
  float t140;
  uint64_t t141;
  int64_t t142;
  int64_t t143;
  int64_t t144;
  int64_t t145;
  uint64_t t146;
  float t147;
  float t148;
  float t149;
  float t150;
  float t151;
  uint64_t t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  uint64_t t157;
  float t158;
  float t159;
  float t160;
  float t161;
  float t162;
  uint64_t t163;
  int64_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  uint64_t t168;
  float t169;
  float t170;
  float t171;
  float t172;
  float t173;
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
  float v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  float v15;
  float v16;
  float v17;
  float v18;
  int64_t v19;
  int64_t v20;
  uint32_t v21;
  uint32_t v22;
  uint32_t v23;
  uint32_t v24;
  int64_t v25;
  int64_t v26;
  int64_t v27;
  int64_t v28;
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
  fg249dd_gg_function_2(t6, t7, base + 0);
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)UINT64_C(16))));
  v4 = ((int64_t)(((int64_t)fg249dd_gg_div(t8, t9))));
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(((int64_t)UINT64_C(16))));
  v5 = ((int64_t)(((int64_t)fg249dd_gg_mod(t10, t11))));
  t12 = ((int64_t)(((int64_t)UINT64_C(2))));
  t13 = ((int64_t)(v4));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  t14 = ((int64_t)(v2));
  t15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(176))))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))));
  t16 = ((int64_t)(v2));
  t17 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(256))))));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t17)))));
  t19 = ((int64_t)(((int64_t)UINT64_C(64))));
  t20 = ((int64_t)(v4));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t18) + (uint64_t)(t21)))));
  t23 = ((int64_t)(((int64_t)UINT64_C(2))));
  t24 = ((int64_t)(v5));
  t25 = ((int64_t)(((int64_t)((uint64_t)(t23) * (uint64_t)(t24)))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t22) + (uint64_t)(t25)))));
  t26 = ((uint64_t)(v0));
  t27 = ((uint64_t)(((uint64_t)(v7))));
#ifndef __cplusplus
  extern float (fg249d_ld_half)(uint64_t, uint64_t);
#endif
  t28 = fg249d_ld_half(t26, t27);
  v9 = ((float)(t28));
  t29 = ((uint64_t)(v0));
  t30 = ((int64_t)(v7));
  t31 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t32 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t30) + (uint64_t)(t31)))))));
#ifndef __cplusplus
  extern float (fg249d_ld_half)(uint64_t, uint64_t);
#endif
  t33 = fg249d_ld_half(t29, t32);
  v10 = ((float)(t33));
  t34 = ((int64_t)(v7));
  t35 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(48))))));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t34) + (uint64_t)(t35)))));
  t37 = ((int64_t)(((int64_t)UINT64_C(32))));
  t38 = ((int64_t)(v4));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t37) * (uint64_t)(t38)))));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t36) + (uint64_t)(t39)))));
  t41 = ((int64_t)(((int64_t)UINT64_C(2))));
  t42 = ((int64_t)(v5));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t41) * (uint64_t)(t42)))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t40) + (uint64_t)(t43)))));
  t44 = ((int64_t)(v7));
  t45 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) + (uint64_t)(t45)))));
  t47 = ((int64_t)(((int64_t)UINT64_C(2))));
  t48 = ((int64_t)(v5));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t47) * (uint64_t)(t48)))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t46) + (uint64_t)(t49)))));
  t50 = ((uint64_t)(v0));
  t51 = ((int64_t)(v7));
  t52 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t53 = ((int64_t)(((int64_t)((uint64_t)(t51) + (uint64_t)(t52)))));
  t54 = ((int64_t)(v6));
  v13 = fg249dd_gg_function_5(t50, t53, t54, base + 0);
  t55 = ((uint64_t)(v0));
  t56 = ((int64_t)(v7));
  t57 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t58 = ((int64_t)(((int64_t)((uint64_t)(t56) + (uint64_t)(t57)))));
  t59 = ((int64_t)(v6));
  t60 = ((int64_t)(((int64_t)UINT64_C(1))));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t59) + (uint64_t)(t60)))));
  v14 = fg249dd_gg_function_5(t55, t58, t61, base + 0);
  t62 = ((float)(v9));
  t63 = ((int64_t)(v13));
  t64 = ((int64_t)(((int64_t)UINT64_C(8))));
  t65 = ((float)(((float)(((t63) >> ((uint64_t)(t64) & 63))))));
  v15 = ((float)(((t62) * (t65))));
  t66 = ((float)(v10));
  t67 = ((int64_t)(v13));
  t68 = ((int64_t)(((int64_t)UINT64_C(255))));
  t69 = ((float)(((float)(((t67) & (t68))))));
  v16 = ((float)(((t66) * (t69))));
  t70 = ((float)(v9));
  t71 = ((int64_t)(v14));
  t72 = ((int64_t)(((int64_t)UINT64_C(8))));
  t73 = ((float)(((float)(((t71) >> ((uint64_t)(t72) & 63))))));
  v17 = ((float)(((t70) * (t73))));
  t74 = ((float)(v10));
  t75 = ((int64_t)(v14));
  t76 = ((int64_t)(((int64_t)UINT64_C(255))));
  t77 = ((float)(((float)(((t75) & (t76))))));
  v18 = ((float)(((t74) * (t77))));
  t78 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t79 = ((int64_t)(((int64_t)UINT64_C(2))));
  t80 = ((int64_t)(v4));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t79) * (uint64_t)(t80)))));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t78) << ((uint64_t)(t81) & 63)))));
  t82 = ((int64_t)(v19));
  t83 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v20 = ((int64_t)(((int64_t)((uint64_t)(t82) << ((uint64_t)(t83) & 63)))));
  t84 = ((uint64_t)(v0));
  t85 = ((uint64_t)(((uint64_t)(v11))));
#ifndef __cplusplus
  extern uint32_t (fg249d_ld_u8)(uint64_t, uint64_t);
#endif
  t86 = fg249d_ld_u8(t84, t85);
  v21 = ((uint32_t)(t86));
  t87 = ((uint64_t)(v0));
  t88 = ((int64_t)(v11));
  t89 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t90 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t88) + (uint64_t)(t89)))))));
#ifndef __cplusplus
  extern uint32_t (fg249d_ld_u8)(uint64_t, uint64_t);
#endif
  t91 = fg249d_ld_u8(t87, t90);
  v22 = ((uint32_t)(t91));
  t92 = ((uint64_t)(v0));
  t93 = ((uint64_t)(((uint64_t)(v12))));
#ifndef __cplusplus
  extern uint32_t (fg249d_ld_u8)(uint64_t, uint64_t);
#endif
  t94 = fg249d_ld_u8(t92, t93);
  v23 = ((uint32_t)(t94));
  t95 = ((uint64_t)(v0));
  t96 = ((int64_t)(v12));
  t97 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t98 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t96) + (uint64_t)(t97)))))));
#ifndef __cplusplus
  extern uint32_t (fg249d_ld_u8)(uint64_t, uint64_t);
#endif
  t99 = fg249d_ld_u8(t95, t98);
  v24 = ((uint32_t)(t99));
  t100 = ((uint32_t)(v21));
  t101 = ((uint32_t)(((int64_t)UINT64_C(15))));
  t102 = ((int64_t)(((int64_t)(((t100) & (t101))))));
  t103 = ((int64_t)(((int64_t)(v23))));
  t104 = ((int64_t)(v19));
  t105 = ((int64_t)(((t103) & (t104))));
  t106 = fg249dd_gg_function_7(t105, base + 0);
  t107 = ((int64_t)(t106));
  v25 = ((int64_t)(((int64_t)((uint64_t)(t102) + (uint64_t)(t107)))));
  t108 = ((uint32_t)(v22));
  t109 = ((uint32_t)(((int64_t)UINT64_C(15))));
  t110 = ((int64_t)(((int64_t)(((t108) & (t109))))));
  t111 = ((int64_t)(((int64_t)(v24))));
  t112 = ((int64_t)(v19));
  t113 = ((int64_t)(((t111) & (t112))));
  t114 = fg249dd_gg_function_7(t113, base + 0);
  t115 = ((int64_t)(t114));
  v26 = ((int64_t)(((int64_t)((uint64_t)(t110) + (uint64_t)(t115)))));
  t116 = ((uint32_t)(v21));
  t117 = ((int64_t)(((int64_t)UINT64_C(4))));
  t118 = ((int64_t)(((int64_t)(((t116) >> ((uint64_t)(t117) & 31))))));
  t119 = ((int64_t)(((int64_t)(v23))));
  t120 = ((int64_t)(v20));
  t121 = ((int64_t)(((t119) & (t120))));
  t122 = fg249dd_gg_function_7(t121, base + 0);
  t123 = ((int64_t)(t122));
  v27 = ((int64_t)(((int64_t)((uint64_t)(t118) + (uint64_t)(t123)))));
  t124 = ((uint32_t)(v22));
  t125 = ((int64_t)(((int64_t)UINT64_C(4))));
  t126 = ((int64_t)(((int64_t)(((t124) >> ((uint64_t)(t125) & 31))))));
  t127 = ((int64_t)(((int64_t)(v24))));
  t128 = ((int64_t)(v20));
  t129 = ((int64_t)(((t127) & (t128))));
  t130 = fg249dd_gg_function_7(t129, base + 0);
  t131 = ((int64_t)(t130));
  v28 = ((int64_t)(((int64_t)((uint64_t)(t126) + (uint64_t)(t131)))));
  t132 = ((uint64_t)(v1));
  t133 = ((int64_t)(v8));
  t134 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t135 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t133) * (uint64_t)(t134)))))));
  t136 = ((float)(v15));
  t137 = ((float)(((float)(v25))));
  t138 = ((float)(((t136) * (t137))));
  t139 = ((float)(v16));
  t140 = ((float)(((t138) - (t139))));
#ifndef __cplusplus
  extern void (fg249d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg249d_st_f32(t132, t135, t140);
  t141 = ((uint64_t)(v1));
  t142 = ((int64_t)(v8));
  t143 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t144 = ((int64_t)(((int64_t)((uint64_t)(t142) + (uint64_t)(t143)))));
  t145 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t146 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t144) * (uint64_t)(t145)))))));
  t147 = ((float)(v15));
  t148 = ((float)(((float)(v26))));
  t149 = ((float)(((t147) * (t148))));
  t150 = ((float)(v16));
  t151 = ((float)(((t149) - (t150))));
#ifndef __cplusplus
  extern void (fg249d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg249d_st_f32(t141, t146, t151);
  t152 = ((uint64_t)(v1));
  t153 = ((int64_t)(v8));
  t154 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t155 = ((int64_t)(((int64_t)((uint64_t)(t153) + (uint64_t)(t154)))));
  t156 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t157 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t155) * (uint64_t)(t156)))))));
  t158 = ((float)(v17));
  t159 = ((float)(((float)(v27))));
  t160 = ((float)(((t158) * (t159))));
  t161 = ((float)(v18));
  t162 = ((float)(((t160) - (t161))));
#ifndef __cplusplus
  extern void (fg249d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg249d_st_f32(t152, t157, t162);
  t163 = ((uint64_t)(v1));
  t164 = ((int64_t)(v8));
  t165 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(33))))));
  t166 = ((int64_t)(((int64_t)((uint64_t)(t164) + (uint64_t)(t165)))));
  t167 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t168 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t166) * (uint64_t)(t167)))))));
  t169 = ((float)(v17));
  t170 = ((float)(((float)(v28))));
  t171 = ((float)(((t169) * (t170))));
  t172 = ((float)(v18));
  t173 = ((float)(((t171) - (t172))));
#ifndef __cplusplus
  extern void (fg249d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg249d_st_f32(t163, t168, t173);
  }
fg249dd_gg_done:;
}
