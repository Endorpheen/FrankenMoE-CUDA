#pragma once
static __device__ __forceinline__ void fg434c_fail(const char*);
static __device__ __forceinline__ int64_t fg434c_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg434c_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg434c_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg434c_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg434c_double_bits(uint64_t bits);
static __device__ __forceinline__ int64_t fg434c_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, uint64_t p10, int64_t base);
static __device__ __forceinline__ void fg434c_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg434c_div(int64_t a, int64_t b) { if (!b) fg434c_fail("division by zero"); if (a==INT64_MIN && b==-1) fg434c_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg434c_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg434c_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg434c_udiv(uint64_t a,uint64_t b) { if(!b)fg434c_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg434c_umod(uint64_t a,uint64_t b) { if(!b)fg434c_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg434c_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ int64_t fg434c_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, uint64_t p10, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) uint8_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) uint8_t t7;
  _Alignas(8) uint8_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) uint8_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) int64_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) int64_t t32;
  _Alignas(8) int64_t t33;
  _Alignas(8) uint8_t t34;
  _Alignas(8) int64_t t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) int64_t t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) uint8_t t41;
  _Alignas(8) int64_t t42;
  _Alignas(8) int64_t t43;
  _Alignas(8) uint8_t t44;
  _Alignas(8) int64_t t45;
  _Alignas(8) int64_t t46;
  _Alignas(8) uint8_t t47;
  _Alignas(8) uint8_t t48;
  _Alignas(8) int64_t t49;
  _Alignas(8) int64_t t50;
  _Alignas(8) int64_t t51;
  _Alignas(8) int64_t t52;
  _Alignas(8) uint8_t t53;
  _Alignas(8) int64_t t54;
  _Alignas(8) int64_t t55;
  _Alignas(8) int64_t t56;
  _Alignas(8) int64_t t57;
  _Alignas(8) int64_t t58;
  _Alignas(8) int64_t t59;
  _Alignas(8) int64_t t60;
  _Alignas(8) int64_t t61;
  _Alignas(8) int64_t t62;
  _Alignas(8) int64_t t63;
  _Alignas(8) int64_t t64;
  _Alignas(8) int64_t t65;
  _Alignas(8) int64_t t66;
  _Alignas(8) int64_t t67;
  _Alignas(8) int64_t t68;
  _Alignas(8) int64_t t69;
  _Alignas(8) int64_t t70;
  _Alignas(8) int64_t t71;
  _Alignas(8) int64_t t72;
  _Alignas(8) int64_t t73;
  _Alignas(8) uint64_t t74;
  _Alignas(8) int64_t t75;
  _Alignas(8) int64_t t76;
  _Alignas(8) uint64_t t77;
  _Alignas(8) uint64_t t78;
  _Alignas(8) int64_t t79;
  _Alignas(8) int64_t t80;
  _Alignas(8) uint64_t t81;
  _Alignas(8) float t82;
  _Alignas(8) float t83;
  _Alignas(8) int64_t t84;
  _Alignas(8) int64_t t85;
  _Alignas(8) int64_t t86;
  _Alignas(8) int64_t t87;
  _Alignas(8) uint8_t t88;
  _Alignas(8) int64_t t89;
  _Alignas(8) int64_t t90;
  _Alignas(8) int64_t t91;
  _Alignas(8) int64_t t92;
  _Alignas(8) uint8_t t93;
  _Alignas(8) uint8_t t94;
  _Alignas(8) int64_t t95;
  _Alignas(8) int64_t t96;
  _Alignas(8) uint8_t t97;
  _Alignas(8) int64_t t98;
  _Alignas(8) int64_t t99;
  _Alignas(8) int64_t t100;
  _Alignas(8) int64_t t101;
  _Alignas(8) int64_t t102;
  _Alignas(8) int64_t t103;
  _Alignas(8) int64_t t104;
  _Alignas(8) int64_t t105;
  _Alignas(8) int64_t t106;
  _Alignas(8) int64_t t107;
  _Alignas(8) int64_t t108;
  _Alignas(8) int64_t t109;
  _Alignas(8) int64_t t110;
  _Alignas(8) int64_t t111;
  _Alignas(8) int64_t t112;
  _Alignas(8) int64_t t113;
  _Alignas(8) int64_t t114;
  _Alignas(8) int64_t t115;
  _Alignas(8) int64_t t116;
  _Alignas(8) int64_t t117;
  _Alignas(8) uint64_t t118;
  _Alignas(8) int64_t t119;
  _Alignas(8) int64_t t120;
  _Alignas(8) uint64_t t121;
  _Alignas(8) uint64_t t122;
  _Alignas(8) int64_t t123;
  _Alignas(8) int64_t t124;
  _Alignas(8) uint64_t t125;
  _Alignas(8) float t126;
  _Alignas(8) float t127;
  _Alignas(8) int64_t t128;
  _Alignas(8) int64_t t129;
  _Alignas(8) int64_t t130;
  _Alignas(8) int64_t t131;
  _Alignas(8) int64_t t132;
  _Alignas(8) int64_t t133;
  _Alignas(8) uint64_t v0;
  _Alignas(8) uint64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) int64_t v8;
  _Alignas(8) int64_t v9;
  _Alignas(8) uint64_t v10;
  _Alignas(8) int64_t v11;
  _Alignas(8) int64_t v12;
  _Alignas(8) int64_t v13;
  _Alignas(8) int64_t v14;
  _Alignas(8) int64_t v15;
  _Alignas(8) int64_t v16;
  _Alignas(8) int64_t v17;
  _Alignas(8) int64_t v18;
  _Alignas(8) int64_t v19;
  _Alignas(8) int64_t v20;
  _Alignas(8) int64_t v21;
  _Alignas(8) int64_t v22;
  _Alignas(8) int64_t v23;
  _Alignas(8) int64_t v24;
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
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) <= (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(v3));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t4 = ((t5) <= (t6));
  }
  t7 = t4;
  t8 = t7;
  if (!t8) {
  t9 = ((int64_t)(v4));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t8 = ((t9) <= (t10));
  }
  t11 = t8;
  if (t11) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto fg434c_done;
  }
  }
  t12 = ((int64_t)(v2));
  t13 = ((int64_t)(v3));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  t14 = ((int64_t)(v4));
  t15 = ((int64_t)(v11));
  v12 = ((int64_t)(((int64_t)fg434c_div(t14, t15))));
  t16 = ((int64_t)(v5));
  t17 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t17)))));
  t19 = ((int64_t)(v8));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t18) + (uint64_t)(t19)))));
  t20 = ((int64_t)(v6));
  t21 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  t23 = ((int64_t)(v9));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t22) + (uint64_t)(t23)))));
  t24 = ((int64_t)(v6));
  t25 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t25)))));
  t27 = ((int64_t)(v8));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t26) + (uint64_t)(t27)))));
  t28 = ((int64_t)(v5));
  t29 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  t31 = ((int64_t)(v9));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t30) + (uint64_t)(t31)))));
  v17 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  v18 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label1:;
  t32 = ((int64_t)(v18));
  t33 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t34 = ((t32) < (t33));
  if (!(t34)) goto label2;
  {
  t35 = ((int64_t)(v7));
  t36 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) * (uint64_t)(t36)))));
  t38 = ((int64_t)(v18));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t37) + (uint64_t)(t38)))));
  t39 = ((int64_t)(v19));
  t40 = ((int64_t)(v12));
  t41 = ((t39) >= (t40));
  if (t41) {
  {
  v18 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  }
  } else {
  {
  v20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label4:;
  t42 = ((int64_t)(v20));
  t43 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t44 = ((t42) < (t43));
  if (!(t44)) goto label5;
  {
  t45 = ((int64_t)(v13));
  t46 = ((int64_t)(v3));
  t47 = ((t45) < (t46));
  t48 = t47;
  if (t48) {
  t49 = ((int64_t)(v14));
  t50 = ((int64_t)(v20));
  t51 = ((int64_t)(((int64_t)((uint64_t)(t49) + (uint64_t)(t50)))));
  t52 = ((int64_t)(v2));
  t48 = ((t51) < (t52));
  }
  t53 = t48;
  if (t53) {
  {
  t54 = ((int64_t)(v17));
  t55 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1056))))));
  t56 = ((int64_t)(((int64_t)((uint64_t)(t54) * (uint64_t)(t55)))));
  t57 = ((int64_t)(v9));
  t58 = ((int64_t)(v20));
  t59 = ((int64_t)(((int64_t)((uint64_t)(t57) + (uint64_t)(t58)))));
  t60 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(33))))));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t59) * (uint64_t)(t60)))));
  t62 = ((int64_t)(((int64_t)((uint64_t)(t56) + (uint64_t)(t61)))));
  t63 = ((int64_t)(v8));
  v21 = ((int64_t)(((int64_t)((uint64_t)(t62) + (uint64_t)(t63)))));
  t64 = ((int64_t)(v19));
  t65 = ((int64_t)(v11));
  t66 = ((int64_t)(((int64_t)((uint64_t)(t64) * (uint64_t)(t65)))));
  t67 = ((int64_t)(v14));
  t68 = ((int64_t)(v20));
  t69 = ((int64_t)(((int64_t)((uint64_t)(t67) + (uint64_t)(t68)))));
  t70 = ((int64_t)(v3));
  t71 = ((int64_t)(((int64_t)((uint64_t)(t69) * (uint64_t)(t70)))));
  t72 = ((int64_t)(((int64_t)((uint64_t)(t66) + (uint64_t)(t71)))));
  t73 = ((int64_t)(v13));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t72) + (uint64_t)(t73)))));
  t74 = ((uint64_t)(v10));
  t75 = ((int64_t)(v21));
  t76 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t77 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t75) * (uint64_t)(t76)))))));
  t78 = ((uint64_t)(v0));
  t79 = ((int64_t)(v22));
  t80 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t81 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t79) * (uint64_t)(t80)))))));
#ifndef __cplusplus
  extern float (fg434c_ld_f32)(uint64_t, uint64_t);
#endif
  t82 = fg434c_ld_f32(t78, t81);
  t83 = ((float)(t82));
#ifndef __cplusplus
  extern void (fg434c_st_f32)(uint64_t, uint64_t, float);
#endif
  fg434c_st_f32(t74, t77, t83);
  }
  }
  t84 = ((int64_t)(v20));
  t85 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  v20 = ((int64_t)(((int64_t)((uint64_t)(t84) + (uint64_t)(t85)))));
  }
  goto label4;
label5:;
#ifndef __cplusplus
  extern void (fg434c_sync)(void);
#endif
  fg434c_sync();
  v20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label7:;
  t86 = ((int64_t)(v20));
  t87 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t88 = ((t86) < (t87));
  if (!(t88)) goto label8;
  {
  t89 = ((int64_t)(v16));
  t90 = ((int64_t)(v20));
  t91 = ((int64_t)(((int64_t)((uint64_t)(t89) + (uint64_t)(t90)))));
  t92 = ((int64_t)(v3));
  t93 = ((t91) < (t92));
  t94 = t93;
  if (t94) {
  t95 = ((int64_t)(v15));
  t96 = ((int64_t)(v2));
  t94 = ((t95) < (t96));
  }
  t97 = t94;
  if (t97) {
  {
  t98 = ((int64_t)(v17));
  t99 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1056))))));
  t100 = ((int64_t)(((int64_t)((uint64_t)(t98) * (uint64_t)(t99)))));
  t101 = ((int64_t)(v8));
  t102 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(33))))));
  t103 = ((int64_t)(((int64_t)((uint64_t)(t101) * (uint64_t)(t102)))));
  t104 = ((int64_t)(((int64_t)((uint64_t)(t100) + (uint64_t)(t103)))));
  t105 = ((int64_t)(v9));
  t106 = ((int64_t)(v20));
  t107 = ((int64_t)(((int64_t)((uint64_t)(t105) + (uint64_t)(t106)))));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t104) + (uint64_t)(t107)))));
  t108 = ((int64_t)(v19));
  t109 = ((int64_t)(v11));
  t110 = ((int64_t)(((int64_t)((uint64_t)(t108) * (uint64_t)(t109)))));
  t111 = ((int64_t)(v16));
  t112 = ((int64_t)(v20));
  t113 = ((int64_t)(((int64_t)((uint64_t)(t111) + (uint64_t)(t112)))));
  t114 = ((int64_t)(v2));
  t115 = ((int64_t)(((int64_t)((uint64_t)(t113) * (uint64_t)(t114)))));
  t116 = ((int64_t)(((int64_t)((uint64_t)(t110) + (uint64_t)(t115)))));
  t117 = ((int64_t)(v15));
  v24 = ((int64_t)(((int64_t)((uint64_t)(t116) + (uint64_t)(t117)))));
  t118 = ((uint64_t)(v1));
  t119 = ((int64_t)(v24));
  t120 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t121 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t119) * (uint64_t)(t120)))))));
  t122 = ((uint64_t)(v10));
  t123 = ((int64_t)(v23));
  t124 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t125 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t123) * (uint64_t)(t124)))))));
#ifndef __cplusplus
  extern float (fg434c_ld_f32)(uint64_t, uint64_t);
#endif
  t126 = fg434c_ld_f32(t122, t125);
  t127 = ((float)(t126));
#ifndef __cplusplus
  extern void (fg434c_st_f32)(uint64_t, uint64_t, float);
#endif
  fg434c_st_f32(t118, t121, t127);
  }
  }
  t128 = ((int64_t)(v20));
  t129 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  v20 = ((int64_t)(((int64_t)((uint64_t)(t128) + (uint64_t)(t129)))));
  }
  goto label7;
label8:;
#ifndef __cplusplus
  extern void (fg434c_sync)(void);
#endif
  fg434c_sync();
  t130 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t131 = ((int64_t)(v17));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t130) - (uint64_t)(t131)))));
  t132 = ((int64_t)(v18));
  t133 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t132) + (uint64_t)(t133)))));
  }
  }
  }
  goto label1;
label2:;
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto fg434c_done;
  }
fg434c_done:;
  return t0;
}
