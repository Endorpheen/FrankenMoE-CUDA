#pragma once
static __device__ __forceinline__ double gg_double_bits(uint64_t bits);
static __device__ __forceinline__ void gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ float gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5, float p6, float p7, int64_t p8, int64_t base);
static __device__ __forceinline__ double gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ void gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg235v_fail)(int64_t);
#endif
  fg235v_fail(t2);
  }
  }
  }
gg_done:;
}
static __device__ __forceinline__ float gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5, float p6, float p7, int64_t p8, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  uint64_t t18;
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
  int64_t t30;
  uint64_t t31;
  int64_t t32;
  int64_t t33;
  uint64_t t34;
  int64_t t35;
  int64_t t36;
  uint64_t t37;
  int64_t t38;
  float t39;
  uint64_t t40;
  int64_t t41;
  float t42;
  float t43;
  float t44;
  float t45;
  int64_t t46;
  float t47;
  float t48;
  float t49;
  float t50;
  float t51;
  float t52;
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
  uint64_t v5;
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
  int64_t v16;
  int64_t v17;
  float v18;
  float v19;
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
  t1 = ((int64_t)(v8));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(1300))));
  gg_function_2(t3, t4, base + 0);
  v9 = ((float)(gg_double_bits(UINT64_C(0))));
  v10 = ((float)(gg_double_bits(UINT64_C(0))));
  v11 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t5 = ((int64_t)(v11));
  t6 = ((int64_t)(v8));
  if (!(((t5) < (t6)))) goto label2;
  {
  v12 = ((int64_t)(((int64_t)UINT64_C(0))));
  v13 = ((int64_t)(((int64_t)UINT64_C(0))));
label4:;
  t7 = ((int64_t)(v13));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  if (!(((t7) < (t8)))) goto label5;
  {
  t9 = ((uint64_t)(v0));
  t10 = ((int64_t)(v13));
#ifndef __cplusplus
  extern int64_t (fg235v_ld_i32)(uint64_t, int64_t);
#endif
  t11 = fg235v_ld_i32(t9, t10);
  t12 = ((int64_t)(t11));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t14 = ((int64_t)(v11));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t13) * (uint64_t)(t14)))));
  t16 = ((int64_t)(((t12) >> ((uint64_t)(t15) & 63))));
  t17 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(252645135))))));
  v14 = ((int64_t)(((t16) & (t17))));
  t18 = ((uint64_t)(v1));
  t19 = ((int64_t)(v11));
  t20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  t22 = ((int64_t)(v13));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) + (uint64_t)(t22)))));
#ifndef __cplusplus
  extern int64_t (fg235v_ld_i32)(uint64_t, int64_t);
#endif
  t24 = fg235v_ld_i32(t18, t23);
  v15 = ((int64_t)(t24));
  t25 = ((int64_t)(v14));
  t26 = ((int64_t)(v15));
  t27 = ((int64_t)(v12));
#ifndef __cplusplus
  extern int64_t (fg235v_dp4a)(int64_t, int64_t, int64_t);
#endif
  t28 = fg235v_dp4a(t25, t26, t27);
  v12 = ((int64_t)(t28));
  t29 = ((int64_t)(v13));
  t30 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t30)))));
  }
  goto label4;
label5:;
  t31 = ((uint64_t)(v2));
  t32 = ((int64_t)(v11));
#ifndef __cplusplus
  extern int64_t (fg235v_ld_u8)(uint64_t, int64_t);
#endif
  t33 = fg235v_ld_u8(t31, t32);
  v16 = ((int64_t)(t33));
  t34 = ((uint64_t)(v3));
  t35 = ((int64_t)(v11));
#ifndef __cplusplus
  extern int64_t (fg235v_ld_u8)(uint64_t, int64_t);
#endif
  t36 = fg235v_ld_u8(t34, t35);
  v17 = ((int64_t)(t36));
  t37 = ((uint64_t)(v4));
  t38 = ((int64_t)(v11));
#ifndef __cplusplus
  extern float (fg235v_ld_f32)(uint64_t, int64_t);
#endif
  t39 = fg235v_ld_f32(t37, t38);
  v18 = ((float)(t39));
  t40 = ((uint64_t)(v5));
  t41 = ((int64_t)(v11));
#ifndef __cplusplus
  extern float (fg235v_ld_f32)(uint64_t, int64_t);
#endif
  t42 = fg235v_ld_f32(t40, t41);
  v19 = ((float)(t42));
  t43 = ((float)(v9));
  t44 = ((float)(v18));
  t45 = ((float)(((float)(v16))));
  t46 = ((int64_t)(v12));
#ifndef __cplusplus
  extern float (fg235v_to_f32)(int64_t);
#endif
  t47 = fg235v_to_f32(t46);
  t48 = ((float)(t47));
  t49 = ((float)(((t45) * (t48))));
  t50 = ((float)(((t44) * (t49))));
  v9 = ((float)(((t43) + (t50))));
  t51 = ((float)(v10));
  t52 = ((float)(v19));
  t53 = ((float)(((float)(v17))));
  t54 = ((float)(((t52) * (t53))));
  v10 = ((float)(((t51) + (t54))));
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
