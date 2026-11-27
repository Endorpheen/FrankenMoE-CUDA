
typedef uint64_t gg_unaligned_1 GG_ALIGN1;
typedef int64_t gg_unaligned_2 GG_ALIGN1;
typedef float gg_unaligned_3 GG_ALIGN1;
typedef GG_UNALIGNED gg_unaligned_1 *gg_type_4;
typedef gg_type_4 gg_unaligned_4 GG_ALIGN1;
typedef GG_UNALIGNED gg_unaligned_2 *gg_type_5;
typedef gg_type_5 gg_unaligned_5 GG_ALIGN1;
typedef GG_UNALIGNED gg_unaligned_3 *gg_type_7;
typedef gg_type_7 gg_unaligned_7 GG_ALIGN1;
typedef double gg_unaligned_8 GG_ALIGN1;
typedef uint8_t gg_unaligned_10 GG_ALIGN1;

static float gg_function_4(float p0, int64_t p1, int64_t p2, int64_t p3, uint64_t p4, uint64_t p5, float p6, int64_t p7, float p8, int64_t base);
/* ---- prototypes ---- */
static void gg_function_9(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, float p11, int64_t base);
/* ---- prototypes ---- */
static void gg_function_10(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, int64_t base);
/* ---- prototypes ---- */
static void gg_function_11(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, int64_t p5, int64_t base);
/* ---- prototypes ---- */
static float gg_function_4(float p0, int64_t p1, int64_t p2, int64_t p3, uint64_t p4, uint64_t p5, float p6, int64_t p7, float p8, int64_t base) {
  _Alignas(8) float t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) uint8_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) uint8_t t7;
  _Alignas(8) float t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) uint8_t t13;
  _Alignas(8) uint8_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) uint8_t t17;
  _Alignas(8) float t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) uint8_t t21;
  int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) uint64_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) float t28;
  _Alignas(8) float t29;
  _Alignas(8) uint64_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) int64_t t32;
  _Alignas(8) int64_t t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) int64_t t35;
  _Alignas(8) float t36;
  _Alignas(8) float t37;
  _Alignas(8) double t38;
  _Alignas(8) double t39;
  _Alignas(8) float t40;
  _Alignas(8) float t41;
  _Alignas(8) float t42;
  _Alignas(8) float t43;
  _Alignas(8) float t44;
  _Alignas(8) float t45;
  _Alignas(8) float v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) uint64_t v4;
  _Alignas(8) uint64_t v5;
  _Alignas(8) float v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) float v8;
  _Alignas(8) int64_t v9;
  _Alignas(8) double v10;
  _Alignas(8) int64_t v11;
  _Alignas(8) float v12;
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
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) < (t2));
  t4 = t3;
  if (!t4) {
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(v1));
  t4 = ((t5) > (t6));
  }
  t7 = t4;
  if (t7) {
  {
#ifndef __cplusplus
  extern float (i2o_neg_inf)(void);
#endif
  t8 = i2o_neg_inf();
  t0 = ((float)(t8));
  goto gg_done;
  }
  }
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(v2));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t9) - (uint64_t)(t10)))));
  t11 = ((int64_t)(v3));
  t12 = ((int64_t)(((int64_t)UINT64_C(0))));
  t13 = ((t11) != (t12));
  t14 = t13;
  if (t14) {
  t15 = ((int64_t)(v9));
  t16 = ((int64_t)(v3));
  t14 = ((t15) >= (t16));
  }
  t17 = t14;
  if (t17) {
  {
#ifndef __cplusplus
  extern float (i2o_neg_inf)(void);
#endif
  t18 = i2o_neg_inf();
  t0 = ((float)(t18));
  goto gg_done;
  }
  }
  v10 = ((double)(gg_double_bits(UINT64_C(0))));
  t19 = ((int64_t)(v9));
  t20 = ((int64_t)(((int64_t)UINT64_C(1024))));
  t21 = ((t19) < (t20));
  if (t21) {
  {
  t23 = ((int64_t)UINT64_C(16));
  t22 = 0;
label0:;
  if (t22 >= t23) goto label2;
  v11 = t22;
  {
  t24 = ((uint64_t)(v4));
  t25 = ((int64_t)(v7));
  t26 = ((int64_t)(v11));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t25) + (uint64_t)(t26)))));
#ifndef __cplusplus
  extern float (i2o_load)(uint64_t, int64_t);
#endif
  t28 = i2o_load(t24, t27);
  t29 = ((float)(t28));
  t30 = ((uint64_t)(v5));
  t31 = ((int64_t)(v9));
  t32 = ((int64_t)(((int64_t)UINT64_C(16))));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) * (uint64_t)(t32)))));
  t34 = ((int64_t)(v11));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t33) + (uint64_t)(t34)))));
#ifndef __cplusplus
  extern float (i2o_load)(uint64_t, int64_t);
#endif
  t36 = i2o_load(t30, t35);
  t37 = ((float)(t36));
  v12 = ((float)(((t29) * (t37))));
  t38 = v10;
  t39 = ((double)(((double)(v12))));
  v10 = ((t38) + (t39));
  }
label1:;
  t22++; goto label0;
label2:;
  }
  }
  t40 = ((float)(v8));
  t41 = ((float)(v0));
  t42 = ((float)(((t40) * (t41))));
  t43 = ((float)(((float)(v10))));
  t44 = ((float)(v6));
  t45 = ((float)(((t43) - (t44))));
  t0 = ((float)(((t42) + (t45))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static void gg_function_0(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5, uint64_t p6, uint64_t p7, uint64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, float p15, int64_t base) {
  _Alignas(8) uint64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) float t2;
  _Alignas(8) uint64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) float t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) uint8_t t12;
  _Alignas(8) uint64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) float t17;
  _Alignas(8) float t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) uint64_t t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) int64_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) uint64_t t27;
  _Alignas(8) uint64_t t28;
  _Alignas(8) float t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) int64_t t32;
  _Alignas(8) float t33;
  _Alignas(8) float t34;
  _Alignas(8) uint64_t t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) int64_t t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) float t39;
  _Alignas(8) float t40;
  _Alignas(8) float t41;
  _Alignas(8) uint8_t t42;
  _Alignas(8) float t43;
  _Alignas(8) float t44;
  _Alignas(8) float t45;
  _Alignas(8) float t46;
  _Alignas(8) uint64_t t47;
  _Alignas(8) int64_t t48;
  _Alignas(8) float t49;
  _Alignas(8) float t50;
  _Alignas(8) float t51;
  int64_t t52;
  _Alignas(8) int64_t t53;
  _Alignas(8) uint64_t t54;
  _Alignas(8) int64_t t55;
  _Alignas(8) int64_t t56;
  _Alignas(8) int64_t t57;
  _Alignas(8) float t58;
  _Alignas(8) float t59;
  _Alignas(8) float t60;
  _Alignas(8) float t61;
  _Alignas(8) float t62;
  _Alignas(8) uint64_t t63;
  _Alignas(8) int64_t t64;
  _Alignas(8) int64_t t65;
  _Alignas(8) int64_t t66;
  _Alignas(8) float t67;
  _Alignas(8) float t68;
  _Alignas(8) float t69;
  _Alignas(8) uint64_t t70;
  _Alignas(8) int64_t t71;
  _Alignas(8) float t72;
  _Alignas(8) uint64_t t73;
  _Alignas(8) int64_t t74;
  _Alignas(8) float t75;
  _Alignas(8) uint64_t t76;
  _Alignas(8) int64_t t77;
  _Alignas(8) float t78;
  _Alignas(8) uint64_t v0;
  _Alignas(8) uint64_t v1;
  _Alignas(8) uint64_t v2;
  _Alignas(8) uint64_t v3;
  _Alignas(8) uint64_t v4;
  _Alignas(8) uint64_t v5;
  _Alignas(8) uint64_t v6;
  _Alignas(8) uint64_t v7;
  _Alignas(8) uint64_t v8;
  _Alignas(8) int64_t v9;
  _Alignas(8) int64_t v10;
  _Alignas(8) int64_t v11;
  _Alignas(8) int64_t v12;
  _Alignas(8) int64_t v13;
  _Alignas(8) int64_t v14;
  _Alignas(8) float v15;
  _Alignas(8) float v16;
  _Alignas(8) float v17;
  _Alignas(8) float v18;
  _Alignas(8) int64_t v19;
  _Alignas(8) int64_t v20;
  _Alignas(8) float v21;
  _Alignas(8) float v22;
  _Alignas(8) float v23;
  _Alignas(8) int64_t v24;
  _Alignas(8) float v25;
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
  t0 = ((uint64_t)(v2));
  t1 = ((int64_t)(v13));
#ifndef __cplusplus
  extern float (i2o_shared_read)(uint64_t, int64_t);
#endif
  t2 = i2o_shared_read(t0, t1);
  v16 = ((float)(t2));
  t3 = ((uint64_t)(v8));
  t4 = ((int64_t)(v12));
#ifndef __cplusplus
  extern float (i2o_load)(uint64_t, int64_t);
#endif
  t5 = i2o_load(t3, t4);
  v17 = ((float)(t5));
  v18 = ((float)(v16));
  t6 = ((int64_t)(v13));
  t7 = ((int64_t)(((int64_t)UINT64_C(32))));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t6) * (uint64_t)(t7)))));
  t9 = ((int64_t)UINT64_C(32));
  t8 = 0;
label0:;
  if (t8 >= t9) goto label2;
  v20 = t8;
  {
  t10 = ((int64_t)(v20));
  t11 = ((int64_t)(v9));
  t12 = ((t10) < (t11));
  if (t12) {
  {
  t13 = ((uint64_t)(v0));
  t14 = ((int64_t)(v19));
  t15 = ((int64_t)(v20));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) + (uint64_t)(t15)))));
#ifndef __cplusplus
  extern float (i2o_shared_read)(uint64_t, int64_t);
#endif
  t17 = i2o_shared_read(t13, t16);
  t18 = ((float)(t17));
  t19 = ((int64_t)(v11));
  t20 = ((uint64_t)(v5));
  t21 = ((int64_t)(v10));
  t22 = ((int64_t)(v20));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) + (uint64_t)(t22)))));
#ifndef __cplusplus
  extern int64_t (i2o_position)(uint64_t, int64_t);
#endif
  t24 = i2o_position(t20, t23);
  t25 = ((int64_t)(t24));
  t26 = ((int64_t)(v14));
  t27 = ((uint64_t)(v6));
  t28 = ((uint64_t)(v7));
  t29 = ((float)(v17));
  t30 = ((int64_t)(v12));
  t31 = ((int64_t)(((int64_t)UINT64_C(16))));
  t32 = ((int64_t)(((int64_t)((uint64_t)(t30) * (uint64_t)(t31)))));
  t33 = ((float)(v15));
  v21 = gg_function_4(t18, t19, t25, t26, t27, t28, t29, t32, t33, base + 0);
  }
  } else {
  {
#ifndef __cplusplus
  extern float (i2o_neg_inf)(void);
#endif
  t34 = i2o_neg_inf();
  v21 = ((float)(t34));
  }
  }
  t35 = ((uint64_t)(v0));
  t36 = ((int64_t)(v19));
  t37 = ((int64_t)(v20));
  t38 = ((int64_t)(((int64_t)((uint64_t)(t36) + (uint64_t)(t37)))));
  t39 = ((float)(v21));
#ifndef __cplusplus
  extern void (i2o_shared_write)(uint64_t, int64_t, float);
#endif
  i2o_shared_write(t35, t38, t39);
  t40 = ((float)(v21));
  t41 = ((float)(v18));
  t42 = ((t40) > (t41));
  if (t42) {
  {
  v18 = ((float)(v21));
  }
  }
  }
label1:;
  t8++; goto label0;
label2:;
  t43 = ((float)(v16));
  t44 = ((float)(v18));
  t45 = ((float)(((t43) - (t44))));
#ifndef __cplusplus
  extern float (i2o_exp)(float);
#endif
  t46 = i2o_exp(t45);
  v22 = ((float)(t46));
  t47 = ((uint64_t)(v3));
  t48 = ((int64_t)(v13));
#ifndef __cplusplus
  extern float (i2o_shared_read)(uint64_t, int64_t);
#endif
  t49 = i2o_shared_read(t47, t48);
  t50 = ((float)(t49));
  t51 = ((float)(v22));
  v23 = ((float)(((t50) * (t51))));
  t53 = ((int64_t)UINT64_C(32));
  t52 = 0;
label3:;
  if (t52 >= t53) goto label5;
  v24 = t52;
  {
  t54 = ((uint64_t)(v0));
  t55 = ((int64_t)(v19));
  t56 = ((int64_t)(v24));
  t57 = ((int64_t)(((int64_t)((uint64_t)(t55) + (uint64_t)(t56)))));
#ifndef __cplusplus
  extern float (i2o_shared_read)(uint64_t, int64_t);
#endif
  t58 = i2o_shared_read(t54, t57);
  t59 = ((float)(t58));
  t60 = ((float)(v18));
  t61 = ((float)(((t59) - (t60))));
#ifndef __cplusplus
  extern float (i2o_exp)(float);
#endif
  t62 = i2o_exp(t61);
  v25 = ((float)(t62));
  t63 = ((uint64_t)(v1));
  t64 = ((int64_t)(v19));
  t65 = ((int64_t)(v24));
  t66 = ((int64_t)(((int64_t)((uint64_t)(t64) + (uint64_t)(t65)))));
  t67 = ((float)(v25));
#ifndef __cplusplus
  extern void (i2o_probability_write)(uint64_t, int64_t, float);
#endif
  i2o_probability_write(t63, t66, t67);
  t68 = v23;
  t69 = ((float)(v25));
  v23 = ((t68) + (t69));
  }
label4:;
  t52++; goto label3;
label5:;
  t70 = ((uint64_t)(v2));
  t71 = ((int64_t)(v13));
  t72 = ((float)(v18));
#ifndef __cplusplus
  extern void (i2o_shared_write)(uint64_t, int64_t, float);
#endif
  i2o_shared_write(t70, t71, t72);
  t73 = ((uint64_t)(v3));
  t74 = ((int64_t)(v13));
  t75 = ((float)(v23));
#ifndef __cplusplus
  extern void (i2o_shared_write)(uint64_t, int64_t, float);
#endif
  i2o_shared_write(t73, t74, t75);
  t76 = ((uint64_t)(v4));
  t77 = ((int64_t)(v13));
  t78 = ((float)(v22));
#ifndef __cplusplus
  extern void (i2o_shared_write)(uint64_t, int64_t, float);
#endif
  i2o_shared_write(t76, t77, t78);
  }
}
static void gg_function_9(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, float p11, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) uint8_t t2;
  _Alignas(8) uint64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) float t5;
  _Alignas(8) float t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) uint64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) uint64_t t15;
  _Alignas(8) uint64_t t16;
  _Alignas(8) uint64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) float t19;
  _Alignas(8) float t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) float t24;
  _Alignas(8) float t25;
  _Alignas(8) uint64_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) float t28;
  _Alignas(8) uint64_t v0;
  _Alignas(8) uint64_t v1;
  _Alignas(8) uint64_t v2;
  _Alignas(8) uint64_t v3;
  _Alignas(8) uint64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) int64_t v8;
  _Alignas(8) int64_t v9;
  _Alignas(8) int64_t v10;
  _Alignas(8) float v11;
  _Alignas(8) float v12;
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
  {
  t0 = ((int64_t)(v9));
  t1 = ((int64_t)(v5));
  t2 = ((t0) < (t1));
  if (t2) {
  {
  t3 = ((uint64_t)(v0));
  t4 = ((int64_t)(v9));
#ifndef __cplusplus
  extern float (i2o_shared_read)(uint64_t, int64_t);
#endif
  t5 = i2o_shared_read(t3, t4);
  t6 = ((float)(t5));
  t7 = ((int64_t)(v7));
  t8 = ((uint64_t)(v1));
  t9 = ((int64_t)(v6));
  t10 = ((int64_t)(v9));
  t11 = ((int64_t)(((int64_t)((uint64_t)(t9) + (uint64_t)(t10)))));
#ifndef __cplusplus
  extern int64_t (i2o_position)(uint64_t, int64_t);
#endif
  t12 = i2o_position(t8, t11);
  t13 = ((int64_t)(t12));
  t14 = ((int64_t)(v10));
  t15 = ((uint64_t)(v2));
  t16 = ((uint64_t)(v3));
  t17 = ((uint64_t)(v4));
  t18 = ((int64_t)(v8));
#ifndef __cplusplus
  extern float (i2o_load)(uint64_t, int64_t);
#endif
  t19 = i2o_load(t17, t18);
  t20 = ((float)(t19));
  t21 = ((int64_t)(v8));
  t22 = ((int64_t)(((int64_t)UINT64_C(16))));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) * (uint64_t)(t22)))));
  t24 = ((float)(v11));
  v12 = gg_function_4(t6, t7, t13, t14, t15, t16, t20, t23, t24, base + 0);
  }
  } else {
  {
#ifndef __cplusplus
  extern float (i2o_neg_inf)(void);
#endif
  t25 = i2o_neg_inf();
  v12 = ((float)(t25));
  }
  }
  t26 = ((uint64_t)(v0));
  t27 = ((int64_t)(v9));
  t28 = ((float)(v12));
#ifndef __cplusplus
  extern void (i2o_shared_write)(uint64_t, int64_t, float);
#endif
  i2o_shared_write(t26, t27, t28);
  }
}
static void gg_function_10(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, int64_t base) {
  _Alignas(8) uint64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) float t2;
  int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) uint64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) float t7;
  _Alignas(8) float t8;
  _Alignas(8) float t9;
  _Alignas(8) uint8_t t10;
  _Alignas(8) float t11;
  _Alignas(8) float t12;
  _Alignas(8) float t13;
  _Alignas(8) float t14;
  _Alignas(8) uint64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) float t17;
  _Alignas(8) float t18;
  _Alignas(8) float t19;
  int64_t t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) uint64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) int64_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) float t26;
  _Alignas(8) float t27;
  _Alignas(8) float t28;
  _Alignas(8) float t29;
  _Alignas(8) float t30;
  _Alignas(8) uint64_t t31;
  _Alignas(8) int64_t t32;
  _Alignas(8) int64_t t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) float t35;
  _Alignas(8) float t36;
  _Alignas(8) float t37;
  _Alignas(8) uint64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) float t40;
  _Alignas(8) uint64_t t41;
  _Alignas(8) int64_t t42;
  _Alignas(8) float t43;
  _Alignas(8) uint64_t t44;
  _Alignas(8) int64_t t45;
  _Alignas(8) float t46;
  _Alignas(8) uint64_t v0;
  _Alignas(8) uint64_t v1;
  _Alignas(8) uint64_t v2;
  _Alignas(8) uint64_t v3;
  _Alignas(8) uint64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) float v7;
  _Alignas(8) float v8;
  _Alignas(8) int64_t v9;
  _Alignas(8) float v10;
  _Alignas(8) float v11;
  _Alignas(8) float v12;
  _Alignas(8) int64_t v13;
  _Alignas(8) float v14;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  v5 = ((int64_t)(((int64_t)UINT64_C(0))));
  v6 = ((int64_t)(((int64_t)UINT64_C(0))));
  t0 = ((uint64_t)(v2));
  t1 = ((int64_t)(v5));
#ifndef __cplusplus
  extern float (i2o_shared_read)(uint64_t, int64_t);
#endif
  t2 = i2o_shared_read(t0, t1);
  v7 = ((float)(t2));
  v8 = ((float)(v7));
  t4 = ((int64_t)UINT64_C(32));
  t3 = 0;
label0:;
  if (t3 >= t4) goto label2;
  v9 = t3;
  {
  t5 = ((uint64_t)(v0));
  t6 = ((int64_t)(v9));
#ifndef __cplusplus
  extern float (i2o_shared_read)(uint64_t, int64_t);
#endif
  t7 = i2o_shared_read(t5, t6);
  v10 = ((float)(t7));
  t8 = ((float)(v10));
  t9 = ((float)(v8));
  t10 = ((t8) > (t9));
  if (t10) {
  {
  v8 = ((float)(v10));
  }
  }
  }
label1:;
  t3++; goto label0;
label2:;
  t11 = ((float)(v7));
  t12 = ((float)(v8));
  t13 = ((float)(((t11) - (t12))));
#ifndef __cplusplus
  extern float (i2o_exp)(float);
#endif
  t14 = i2o_exp(t13);
  v11 = ((float)(t14));
  t15 = ((uint64_t)(v3));
  t16 = ((int64_t)(v5));
#ifndef __cplusplus
  extern float (i2o_shared_read)(uint64_t, int64_t);
#endif
  t17 = i2o_shared_read(t15, t16);
  t18 = ((float)(t17));
  t19 = ((float)(v11));
  v12 = ((float)(((t18) * (t19))));
  t21 = ((int64_t)UINT64_C(32));
  t20 = 0;
label3:;
  if (t20 >= t21) goto label5;
  v13 = t20;
  {
  t22 = ((uint64_t)(v0));
  t23 = ((int64_t)(v6));
  t24 = ((int64_t)(v13));
  t25 = ((int64_t)(((int64_t)((uint64_t)(t23) + (uint64_t)(t24)))));
#ifndef __cplusplus
  extern float (i2o_shared_read)(uint64_t, int64_t);
#endif
  t26 = i2o_shared_read(t22, t25);
  t27 = ((float)(t26));
  t28 = ((float)(v8));
  t29 = ((float)(((t27) - (t28))));
#ifndef __cplusplus
  extern float (i2o_exp)(float);
#endif
  t30 = i2o_exp(t29);
  v14 = ((float)(t30));
  t31 = ((uint64_t)(v1));
  t32 = ((int64_t)(v6));
  t33 = ((int64_t)(v13));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) + (uint64_t)(t33)))));
  t35 = ((float)(v14));
#ifndef __cplusplus
  extern void (i2o_probability_write)(uint64_t, int64_t, float);
#endif
  i2o_probability_write(t31, t34, t35);
  t36 = v12;
  t37 = ((float)(v14));
  v12 = ((t36) + (t37));
  }
label4:;
  t20++; goto label3;
label5:;
  t38 = ((uint64_t)(v2));
  t39 = ((int64_t)(v5));
  t40 = ((float)(v8));
#ifndef __cplusplus
  extern void (i2o_shared_write)(uint64_t, int64_t, float);
#endif
  i2o_shared_write(t38, t39, t40);
  t41 = ((uint64_t)(v3));
  t42 = ((int64_t)(v5));
  t43 = ((float)(v12));
#ifndef __cplusplus
  extern void (i2o_shared_write)(uint64_t, int64_t, float);
#endif
  i2o_shared_write(t41, t42, t43);
  t44 = ((uint64_t)(v4));
  t45 = ((int64_t)(v5));
  t46 = ((float)(v11));
#ifndef __cplusplus
  extern void (i2o_shared_write)(uint64_t, int64_t, float);
#endif
  i2o_shared_write(t44, t45, t46);
  }
}
static void gg_function_11(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, int64_t p5, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) uint64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) float t4;
  int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) uint64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) float t11;
  _Alignas(8) float t12;
  _Alignas(8) float t13;
  _Alignas(8) uint8_t t14;
  _Alignas(8) float t15;
  _Alignas(8) float t16;
  _Alignas(8) float t17;
  _Alignas(8) float t18;
  _Alignas(8) uint64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) float t21;
  _Alignas(8) float t22;
  _Alignas(8) float t23;
  int64_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) uint64_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) float t30;
  _Alignas(8) float t31;
  _Alignas(8) float t32;
  _Alignas(8) float t33;
  _Alignas(8) float t34;
  _Alignas(8) uint64_t t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) int64_t t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) float t39;
  _Alignas(8) float t40;
  _Alignas(8) float t41;
  _Alignas(8) uint64_t t42;
  _Alignas(8) int64_t t43;
  _Alignas(8) float t44;
  _Alignas(8) uint64_t t45;
  _Alignas(8) int64_t t46;
  _Alignas(8) float t47;
  _Alignas(8) uint64_t t48;
  _Alignas(8) int64_t t49;
  _Alignas(8) float t50;
  _Alignas(8) uint64_t v0;
  _Alignas(8) uint64_t v1;
  _Alignas(8) uint64_t v2;
  _Alignas(8) uint64_t v3;
  _Alignas(8) uint64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) float v7;
  _Alignas(8) float v8;
  _Alignas(8) int64_t v9;
  _Alignas(8) float v10;
  _Alignas(8) float v11;
  _Alignas(8) float v12;
  _Alignas(8) int64_t v13;
  _Alignas(8) float v14;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t0 = ((int64_t)(v5));
  t1 = ((int64_t)(((int64_t)UINT64_C(32))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t0) * (uint64_t)(t1)))));
  t2 = ((uint64_t)(v2));
  t3 = ((int64_t)(v5));
#ifndef __cplusplus
  extern float (i2o_shared_read)(uint64_t, int64_t);
#endif
  t4 = i2o_shared_read(t2, t3);
  v7 = ((float)(t4));
  v8 = ((float)(v7));
  t6 = ((int64_t)UINT64_C(32));
  t5 = 0;
label0:;
  if (t5 >= t6) goto label2;
  v9 = t5;
  {
  t7 = ((uint64_t)(v0));
  t8 = ((int64_t)(v6));
  t9 = ((int64_t)(v9));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
#ifndef __cplusplus
  extern float (i2o_shared_read)(uint64_t, int64_t);
#endif
  t11 = i2o_shared_read(t7, t10);
  v10 = ((float)(t11));
  t12 = ((float)(v10));
  t13 = ((float)(v8));
  t14 = ((t12) > (t13));
  if (t14) {
  {
  v8 = ((float)(v10));
  }
  }
  }
label1:;
  t5++; goto label0;
label2:;
  t15 = ((float)(v7));
  t16 = ((float)(v8));
  t17 = ((float)(((t15) - (t16))));
#ifndef __cplusplus
  extern float (i2o_exp)(float);
#endif
  t18 = i2o_exp(t17);
  v11 = ((float)(t18));
  t19 = ((uint64_t)(v3));
  t20 = ((int64_t)(v5));
#ifndef __cplusplus
  extern float (i2o_shared_read)(uint64_t, int64_t);
#endif
  t21 = i2o_shared_read(t19, t20);
  t22 = ((float)(t21));
  t23 = ((float)(v11));
  v12 = ((float)(((t22) * (t23))));
  t25 = ((int64_t)UINT64_C(32));
  t24 = 0;
label3:;
  if (t24 >= t25) goto label5;
  v13 = t24;
  {
  t26 = ((uint64_t)(v0));
  t27 = ((int64_t)(v6));
  t28 = ((int64_t)(v13));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) + (uint64_t)(t28)))));
#ifndef __cplusplus
  extern float (i2o_shared_read)(uint64_t, int64_t);
#endif
  t30 = i2o_shared_read(t26, t29);
  t31 = ((float)(t30));
  t32 = ((float)(v8));
  t33 = ((float)(((t31) - (t32))));
#ifndef __cplusplus
  extern float (i2o_exp)(float);
#endif
  t34 = i2o_exp(t33);
  v14 = ((float)(t34));
  t35 = ((uint64_t)(v1));
  t36 = ((int64_t)(v6));
  t37 = ((int64_t)(v13));
  t38 = ((int64_t)(((int64_t)((uint64_t)(t36) + (uint64_t)(t37)))));
  t39 = ((float)(v14));
#ifndef __cplusplus
  extern void (i2o_probability_write)(uint64_t, int64_t, float);
#endif
  i2o_probability_write(t35, t38, t39);
  t40 = v12;
  t41 = ((float)(v14));
  v12 = ((t40) + (t41));
  }
label4:;
  t24++; goto label3;
label5:;
  t42 = ((uint64_t)(v2));
  t43 = ((int64_t)(v5));
  t44 = ((float)(v8));
#ifndef __cplusplus
  extern void (i2o_shared_write)(uint64_t, int64_t, float);
#endif
  i2o_shared_write(t42, t43, t44);
  t45 = ((uint64_t)(v3));
  t46 = ((int64_t)(v5));
  t47 = ((float)(v12));
#ifndef __cplusplus
  extern void (i2o_shared_write)(uint64_t, int64_t, float);
#endif
  i2o_shared_write(t45, t46, t47);
  t48 = ((uint64_t)(v4));
  t49 = ((int64_t)(v5));
  t50 = ((float)(v11));
#ifndef __cplusplus
  extern void (i2o_shared_write)(uint64_t, int64_t, float);
#endif
  i2o_shared_write(t48, t49, t50);
  }
}
#if defined(__OPENCL_C_VERSION__)
static void i2_online_row_g(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5, uint64_t p6, uint64_t p7, uint64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, float p15) { gg_function_0(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, 0); }
static void i2_online_adjust_key_g(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, float p11) { gg_function_9(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, 0); }
static void i2_online_prepared_row_g(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4) { gg_function_10(p0, p1, p2, p3, p4, 0); }
static void i2_online_prepared_row_at_g(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, int64_t p5) { gg_function_11(p0, p1, p2, p3, p4, p5, 0); }
#endif
