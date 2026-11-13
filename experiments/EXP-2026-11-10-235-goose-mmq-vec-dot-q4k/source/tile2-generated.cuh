#pragma once
static __device__ __forceinline__ double gg_double_bits(uint64_t bits);
static __device__ __forceinline__ float gg_function_6(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, int64_t p5, float p6, float p7, int64_t p8, int64_t base);
static __device__ __forceinline__ void gg236_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ int64_t gg236_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t base);
static __device__ __forceinline__ double gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ float gg_function_6(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, int64_t p5, float p6, float p7, int64_t p8, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  uint64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  uint64_t t15;
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
  uint64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  float t31;
  uint64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  float t36;
  float t37;
  float t38;
  uint64_t t39;
  int64_t t40;
  int64_t t41;
  float t42;
  int64_t t43;
  float t44;
  float t45;
  float t46;
  float t47;
  float t48;
  float t49;
  uint64_t t50;
  int64_t t51;
  int64_t t52;
  float t53;
  float t54;
  int64_t t55;
  int64_t t56;
  float t57;
  float t58;
  float t59;
  float t60;
  float t61;
  float t62;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  uint64_t v4;
  int64_t v5;
  float v6;
  float v7;
  int64_t v8;
  float v9;
  float v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  float v16;
  float v17;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  v8 = p8;
  {
  v9 = ((float)(gg_double_bits(UINT64_C(0))));
  v10 = ((float)(gg_double_bits(UINT64_C(0))));
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t1 = ((int64_t)(v11));
  t2 = ((int64_t)(v8));
  if (!(((t1) < (t2)))) goto label2;
  {
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
  v13 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t3 = ((int64_t)(v13));
  t4 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  if (!(((t3) < (t4)))) goto label5;
  {
  t5 = ((uint64_t)(v0));
  t6 = ((int64_t)(v13));
#ifndef __cplusplus
  extern int64_t (fg236t_ld_i32)(uint64_t, int64_t);
#endif
  t7 = fg236t_ld_i32(t5, t6);
  v14 = ((int64_t)(t7));
  t8 = ((int64_t)(v14));
  t9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t10 = ((int64_t)(v11));
  t11 = ((int64_t)(((int64_t)((uint64_t)(t9) * (uint64_t)(t10)))));
  t12 = ((int64_t)(((t8) >> ((uint64_t)(t11) & 63))));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(252645135))))));
  v15 = ((int64_t)(((t12) & (t13))));
  t14 = ((int64_t)(v15));
  t15 = ((uint64_t)(v1));
  t16 = ((int64_t)(v11));
  t17 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t17)))));
  t19 = ((int64_t)(v13));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t18) + (uint64_t)(t19)))));
#ifndef __cplusplus
  extern int64_t (fg236t_ld_i32)(uint64_t, int64_t);
#endif
  t21 = fg236t_ld_i32(t15, t20);
  t22 = ((int64_t)(t21));
  t23 = ((int64_t)(v12));
#ifndef __cplusplus
  extern int64_t (fg236t_dp4a)(int64_t, int64_t, int64_t);
#endif
  t24 = fg236t_dp4a(t14, t22, t23);
  v12 = ((int64_t)(t24));
  t25 = ((int64_t)(v13));
  t26 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t26)))));
  }
  goto label4;
label5:;
  t27 = ((uint64_t)(v4));
  t28 = ((int64_t)(v5));
  t29 = ((int64_t)(v11));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) + (uint64_t)(t29)))));
#ifndef __cplusplus
  extern float (fg236t_dm_x)(uint64_t, int64_t);
#endif
  t31 = fg236t_dm_x(t27, t30);
  v16 = ((float)(t31));
  t32 = ((uint64_t)(v4));
  t33 = ((int64_t)(v5));
  t34 = ((int64_t)(v11));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t33) + (uint64_t)(t34)))));
#ifndef __cplusplus
  extern float (fg236t_dm_y)(uint64_t, int64_t);
#endif
  t36 = fg236t_dm_y(t32, t35);
  v17 = ((float)(t36));
  t37 = ((float)(v9));
  t38 = ((float)(v16));
  t39 = ((uint64_t)(v2));
  t40 = ((int64_t)(v11));
#ifndef __cplusplus
  extern int64_t (fg236t_ld_u8)(uint64_t, int64_t);
#endif
  t41 = fg236t_ld_u8(t39, t40);
  t42 = ((float)(((float)(t41))));
  t43 = ((int64_t)(v12));
#ifndef __cplusplus
  extern float (fg236t_to_f32)(int64_t);
#endif
  t44 = fg236t_to_f32(t43);
  t45 = ((float)(t44));
  t46 = ((float)(((t42) * (t45))));
  t47 = ((float)(((t38) * (t46))));
  v9 = ((float)(((t37) + (t47))));
  t48 = ((float)(v10));
  t49 = ((float)(v17));
  t50 = ((uint64_t)(v3));
  t51 = ((int64_t)(v11));
#ifndef __cplusplus
  extern int64_t (fg236t_ld_u8)(uint64_t, int64_t);
#endif
  t52 = fg236t_ld_u8(t50, t51);
  t53 = ((float)(((float)(t52))));
  t54 = ((float)(((t49) * (t53))));
  v10 = ((float)(((t48) + (t54))));
  t55 = ((int64_t)(v11));
  t56 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t55) + (uint64_t)(t56)))));
  }
  goto label1;
label2:;
  t57 = ((float)(v6));
  t58 = ((float)(v9));
  t59 = ((float)(((t57) * (t58))));
  t60 = ((float)(v7));
  t61 = ((float)(v10));
  t62 = ((float)(((t60) * (t61))));
  t0 = ((float)(((t59) - (t62))));
  }
gg_done:;
  return t0;
}
static __device__ __forceinline__ void gg236_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg236t_fail)(int64_t);
#endif
  fg236t_fail(t2);
  }
  }
  }
gg_done:;
}
static __device__ __forceinline__ int64_t gg236_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  uint8_t t4;
  int64_t t5;
  int64_t t6;
  uint8_t t7;
  uint8_t t8;
  int64_t t9;
  int64_t t10;
  uint8_t t11;
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  uint8_t t20;
  int64_t t21;
  int64_t t22;
  uint8_t t23;
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
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  uint64_t t79;
  int64_t t80;
  float t81;
  uint64_t t82;
  int64_t t83;
  float t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  uint64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  uint64_t t94;
  int64_t t95;
  int64_t t96;
  uint64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  uint64_t t102;
  uint64_t t103;
  int64_t t104;
  float t105;
  float t106;
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
  uint64_t t118;
  int64_t t119;
  uint64_t t120;
  int64_t t121;
  float t122;
  float t123;
  float t124;
  float t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  uint64_t v4;
  uint64_t v5;
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
  int64_t v21;
  int64_t v22;
  int64_t v23;
  int64_t v24;
  int64_t v25;
  int64_t v26;
  int64_t v27;
  float v28;
  float v29;
  float v30;
  int64_t v31;
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
  {
  t1 = ((int64_t)(v7));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v9));
  t6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = ((t5) >= (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v10));
  t10 = ((int64_t)(((int64_t)UINT64_C(0))));
  t8 = ((t9) > (t10));
  }
  t11 = t8;
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(v8));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t12 = ((t13) > (t14));
  }
  t15 = t12;
  t16 = ((int64_t)(((int64_t)UINT64_C(1500))));
  gg236_function_2(t15, t16, base + 0);
  t17 = ((int64_t)(v11));
  t18 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((t17) > (t18));
  t20 = t19;
  if (t20) {
  t21 = ((int64_t)(v12));
  t22 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((t21) > (t22));
  }
  t23 = t20;
  t24 = ((int64_t)(((int64_t)UINT64_C(1501))));
  gg236_function_2(t23, t24, base + 0);
  t25 = ((int64_t)(v11));
  t26 = ((int64_t)(v11));
  t27 = ((int64_t)(v12));
  t28 = ((int64_t)(((int64_t)gg_div(t26, t27))));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t28)))));
  v16 = ((int64_t)(((int64_t)UINT64_C(0))));
  v17 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t29 = ((int64_t)(v17));
  t30 = ((int64_t)(v11));
  if (!(((t29) < (t30)))) goto label2;
  {
  t31 = ((int64_t)(v6));
  t32 = ((int64_t)(v17));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t32)))));
  v19 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t33 = ((int64_t)(v19));
  t34 = ((int64_t)(v7));
  if (!(((t33) < (t34)))) goto label5;
  {
  t35 = ((int64_t)(v19));
  t36 = ((int64_t)(v14));
  v20 = ((int64_t)(((int64_t)((uint64_t)(t35) + (uint64_t)(t36)))));
  v21 = ((int64_t)(((int64_t)UINT64_C(0))));
label7:;
  t37 = ((int64_t)(v21));
  t38 = ((int64_t)(v9));
  if (!(((t37) < (t38)))) goto label8;
  {
  t39 = ((int64_t)(v21));
  t40 = ((int64_t)(v13));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t39) + (uint64_t)(t40)))));
  t41 = ((int64_t)(v22));
  t42 = ((int64_t)(v11));
  t43 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t44 = ((int64_t)(((int64_t)gg_div(t42, t43))));
  t45 = ((int64_t)(((int64_t)((uint64_t)(t41) * (uint64_t)(t44)))));
  t46 = ((int64_t)(v22));
  t47 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t48 = ((int64_t)(((int64_t)gg_div(t46, t47))));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t45) + (uint64_t)(t48)))));
  t50 = ((int64_t)(v18));
  t51 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t52 = ((int64_t)(((int64_t)gg_div(t50, t51))));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t49) + (uint64_t)(t52)))));
  t53 = ((int64_t)(v23));
  t54 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t53) * (uint64_t)(t54)))));
  t56 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t57 = ((int64_t)(v17));
  t58 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t59 = ((int64_t)(((int64_t)gg_div(t57, t58))));
  t60 = ((int64_t)(((int64_t)((uint64_t)(t56) * (uint64_t)(t59)))));
  v24 = ((int64_t)(((int64_t)((uint64_t)(t55) + (uint64_t)(t60)))));
  t61 = ((int64_t)(v22));
  t62 = ((int64_t)(v11));
  t63 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t64 = ((int64_t)(((int64_t)((uint64_t)(t62) + (uint64_t)(t63)))));
  t65 = ((int64_t)(((int64_t)((uint64_t)(t61) * (uint64_t)(t64)))));
  t66 = ((int64_t)(v18));
  t67 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t68 = ((int64_t)(((int64_t)gg_div(t66, t67))));
  v25 = ((int64_t)(((int64_t)((uint64_t)(t65) + (uint64_t)(t68)))));
  t69 = ((int64_t)(v20));
  t70 = ((int64_t)(v15));
  t71 = ((int64_t)(((int64_t)((uint64_t)(t69) * (uint64_t)(t70)))));
  t72 = ((int64_t)(v17));
  v26 = ((int64_t)(((int64_t)((uint64_t)(t71) + (uint64_t)(t72)))));
  t73 = ((int64_t)(v20));
  t74 = ((int64_t)(v15));
  t75 = ((int64_t)(((int64_t)((uint64_t)(t73) * (uint64_t)(t74)))));
  t76 = ((int64_t)(v17));
  t77 = ((int64_t)(v12));
  t78 = ((int64_t)(((int64_t)gg_div(t76, t77))));
  v27 = ((int64_t)(((int64_t)((uint64_t)(t75) + (uint64_t)(t78)))));
  t79 = ((uint64_t)(v1));
  t80 = ((int64_t)(v22));
#ifndef __cplusplus
  extern float (fg236t_dm_x)(uint64_t, int64_t);
#endif
  t81 = fg236t_dm_x(t79, t80);
  v28 = ((float)(t81));
  t82 = ((uint64_t)(v1));
  t83 = ((int64_t)(v22));
#ifndef __cplusplus
  extern float (fg236t_dm_y)(uint64_t, int64_t);
#endif
  t84 = fg236t_dm_y(t82, t83);
  v29 = ((float)(t84));
  t85 = ((int64_t)(((int64_t)(v0))));
  t86 = ((int64_t)(v25));
  t87 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t88 = ((int64_t)(((int64_t)((uint64_t)(t86) * (uint64_t)(t87)))));
  t89 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t85) + (uint64_t)(t88)))))));
  t90 = ((int64_t)(((int64_t)(v3))));
  t91 = ((int64_t)(v26));
  t92 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t93 = ((int64_t)(((int64_t)((uint64_t)(t91) * (uint64_t)(t92)))));
  t94 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t90) + (uint64_t)(t93)))))));
  t95 = ((int64_t)(((int64_t)(v2))));
  t96 = ((int64_t)(v24));
  t97 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t95) + (uint64_t)(t96)))))));
  t98 = ((int64_t)(((int64_t)(v2))));
  t99 = ((int64_t)(v24));
  t100 = ((int64_t)(((int64_t)((uint64_t)(t98) + (uint64_t)(t99)))));
  t101 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t102 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t100) + (uint64_t)(t101)))))));
  t103 = ((uint64_t)(v4));
  t104 = ((int64_t)(v27));
  t105 = ((float)(v28));
  t106 = ((float)(v29));
  t107 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  v30 = gg_function_6(t89, t94, t97, t102, t103, t104, t105, t106, t107, base + 0);
  t108 = ((int64_t)(v19));
  t109 = ((int64_t)(v8));
  t110 = ((int64_t)(((int64_t)gg_div(t108, t109))));
  t111 = ((int64_t)(v9));
  t112 = ((int64_t)(((int64_t)((uint64_t)(t110) * (uint64_t)(t111)))));
  t113 = ((int64_t)(v10));
  t114 = ((int64_t)(((int64_t)gg_div(t112, t113))));
  t115 = ((int64_t)(v21));
  t116 = ((int64_t)(v10));
  t117 = ((int64_t)(((int64_t)gg_div(t115, t116))));
  v31 = ((int64_t)(((int64_t)((uint64_t)(t114) + (uint64_t)(t117)))));
  t118 = ((uint64_t)(v5));
  t119 = ((int64_t)(v31));
  t120 = ((uint64_t)(v5));
  t121 = ((int64_t)(v31));
#ifndef __cplusplus
  extern float (fg236t_ld_f32)(uint64_t, int64_t);
#endif
  t122 = fg236t_ld_f32(t120, t121);
  t123 = ((float)(t122));
  t124 = ((float)(v30));
  t125 = ((float)(((t123) + (t124))));
#ifndef __cplusplus
  extern void (fg236t_st_f32)(uint64_t, int64_t, float);
#endif
  fg236t_st_f32(t118, t119, t125);
  t126 = ((int64_t)(v16));
  t127 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t126) + (uint64_t)(t127)))));
  t128 = ((int64_t)(v21));
  t129 = ((int64_t)(v10));
  v21 = ((int64_t)(((int64_t)((uint64_t)(t128) + (uint64_t)(t129)))));
  }
  goto label7;
label8:;
  t130 = ((int64_t)(v19));
  t131 = ((int64_t)(v8));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t130) + (uint64_t)(t131)))));
  }
  goto label4;
label5:;
  t132 = ((int64_t)(v17));
  t133 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t132) + (uint64_t)(t133)))));
  }
  goto label1;
label2:;
  t0 = ((int64_t)(v16));
  }
gg_done:;
  return t0;
}
