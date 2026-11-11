#pragma once
static __device__ __forceinline__ void fg275dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg275dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg275dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg275dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg275dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg275dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ void fg275dd_gg_function_3(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ int64_t fg275dd_gg_function_2(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static __device__ __forceinline__ void fg275dd_gg_function_7(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t base);
static __device__ __forceinline__ void fg275dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t base);
static __device__ __forceinline__ void fg275dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg275dd_gg_div(int64_t a, int64_t b) { if (!b) fg275dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg275dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg275dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg275dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg275dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg275dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg275dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg275dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg275dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ void fg275dd_gg_function_3(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg275d_fail)(int64_t);
#endif
  fg275d_fail(t2);
  }
  }
  }
fg275dd_gg_done:;
}
static __device__ __forceinline__ int64_t fg275dd_gg_function_2(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
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
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) > (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v2));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t4 = ((t5) > (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v3));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t8 = ((t9) > (t10));
  }
  t11 = t8;
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(v4));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t12 = ((t13) > (t14));
  }
  t15 = t12;
  t16 = ((int64_t)(((int64_t)UINT64_C(20))));
  fg275dd_gg_function_3(t15, t16, base + 0);
  t17 = ((int64_t)(v0));
  t18 = ((int64_t)(v1));
  v5 = ((int64_t)(((int64_t)fg275dd_gg_mod(t17, t18))));
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(v1));
  t21 = ((int64_t)(((int64_t)fg275dd_gg_div(t19, t20))));
  t22 = ((int64_t)(v2));
  v6 = ((int64_t)(((int64_t)fg275dd_gg_mod(t21, t22))));
  t23 = ((int64_t)(v0));
  t24 = ((int64_t)(v1));
  t25 = ((int64_t)(v2));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t25)))));
  t27 = ((int64_t)(((int64_t)fg275dd_gg_div(t23, t26))));
  t28 = ((int64_t)(v3));
  v7 = ((int64_t)(((int64_t)fg275dd_gg_mod(t27, t28))));
  t29 = ((int64_t)(v0));
  t30 = ((int64_t)(v1));
  t31 = ((int64_t)(v2));
  t32 = ((int64_t)(((int64_t)((uint64_t)(t30) * (uint64_t)(t31)))));
  t33 = ((int64_t)(v3));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) * (uint64_t)(t33)))));
  v8 = ((int64_t)(((int64_t)fg275dd_gg_div(t29, t34))));
  t35 = ((int64_t)(v8));
  t36 = ((int64_t)(v3));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) * (uint64_t)(t36)))));
  t38 = ((int64_t)(v7));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t37) + (uint64_t)(t38)))));
  t40 = ((int64_t)(v2));
  t41 = ((int64_t)(((int64_t)((uint64_t)(t39) * (uint64_t)(t40)))));
  t42 = ((int64_t)(v6));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t41) + (uint64_t)(t42)))));
  t44 = ((int64_t)(v1));
  t45 = ((int64_t)(((int64_t)((uint64_t)(t43) * (uint64_t)(t44)))));
  t46 = ((int64_t)(v5));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t45) + (uint64_t)(t46)))));
  }
fg275dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg275dd_gg_function_7(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
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
  uint8_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  uint64_t t37;
  int64_t t38;
  int64_t t39;
  uint64_t t40;
  uint64_t t41;
  int64_t t42;
  int64_t t43;
  uint64_t t44;
  uint32_t t45;
  uint32_t t46;
  uint64_t t47;
  int64_t t48;
  int64_t t49;
  uint64_t t50;
  uint32_t t51;
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  {
  t0 = ((int64_t)(v6));
  t1 = ((int64_t)(((int64_t)(v7))));
  t2 = ((t0) >= (t1));
  if (t2) {
  {
  goto fg275dd_gg_done;
  }
  }
  t3 = ((int64_t)(v6));
  t4 = ((int64_t)(v2));
  v8 = ((int64_t)(((int64_t)fg275dd_gg_mod(t3, t4))));
  t5 = ((int64_t)(v6));
  t6 = ((int64_t)(v2));
  t7 = ((int64_t)(((int64_t)fg275dd_gg_div(t5, t6))));
  t8 = ((int64_t)(v3));
  v9 = ((int64_t)(((int64_t)fg275dd_gg_mod(t7, t8))));
  t9 = ((int64_t)(v6));
  t10 = ((int64_t)(v2));
  t11 = ((int64_t)(v3));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  t13 = ((int64_t)(v4));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  v10 = ((int64_t)(((int64_t)fg275dd_gg_div(t9, t14))));
  t15 = ((int64_t)(v6));
  t16 = ((int64_t)(v2));
  t17 = ((int64_t)(v3));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t17)))));
  t19 = ((int64_t)(((int64_t)fg275dd_gg_div(t15, t18))));
  t20 = ((int64_t)(v4));
  v11 = ((int64_t)(((int64_t)fg275dd_gg_mod(t19, t20))));
  t21 = ((int64_t)(v6));
  t22 = ((int64_t)(v2));
  t23 = ((int64_t)(v3));
  t24 = ((int64_t)(v4));
  t25 = ((int64_t)(v5));
  v12 = fg275dd_gg_function_2(t21, t22, t23, t24, t25, base + 0);
  t26 = ((int64_t)(v8));
  t27 = ((int64_t)(v9));
  t28 = ((t26) == (t27));
  if (t28) {
  {
  t29 = ((int64_t)(v10));
  t30 = ((int64_t)(v4));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t29) * (uint64_t)(t30)))));
  t32 = ((int64_t)(v11));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t32)))));
  t33 = ((int64_t)(v13));
  t34 = ((int64_t)(v2));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t33) * (uint64_t)(t34)))));
  t36 = ((int64_t)(v8));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t35) + (uint64_t)(t36)))));
  t37 = ((uint64_t)(v0));
  t38 = ((int64_t)(v12));
  t39 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t40 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t38) * (uint64_t)(t39)))))));
  t41 = ((uint64_t)(v1));
  t42 = ((int64_t)(v14));
  t43 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t44 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t42) * (uint64_t)(t43)))))));
#ifndef __cplusplus
  extern uint32_t (fg275d_ld_u16)(uint64_t, uint64_t);
#endif
  t45 = fg275d_ld_u16(t41, t44);
  t46 = ((uint32_t)(t45));
#ifndef __cplusplus
  extern void (fg275d_st_u16)(uint64_t, uint64_t, uint32_t);
#endif
  fg275d_st_u16(t37, t40, t46);
  }
  } else {
  {
  t47 = ((uint64_t)(v0));
  t48 = ((int64_t)(v12));
  t49 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t50 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t48) * (uint64_t)(t49)))))));
  t51 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(0))))));
#ifndef __cplusplus
  extern void (fg275d_st_u16)(uint64_t, uint64_t, uint32_t);
#endif
  fg275d_st_u16(t47, t50, t51);
  }
  }
  }
fg275dd_gg_done:;
}
static __device__ __forceinline__ void fg275dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
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
  uint8_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  uint64_t t37;
  int64_t t38;
  int64_t t39;
  uint64_t t40;
  uint64_t t41;
  int64_t t42;
  int64_t t43;
  uint64_t t44;
  float t45;
  float t46;
  uint64_t t47;
  int64_t t48;
  int64_t t49;
  uint64_t t50;
  float t51;
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
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  {
  t0 = ((int64_t)(v6));
  t1 = ((int64_t)(((int64_t)(v7))));
  t2 = ((t0) >= (t1));
  if (t2) {
  {
  goto fg275dd_gg_done;
  }
  }
  t3 = ((int64_t)(v6));
  t4 = ((int64_t)(v2));
  v8 = ((int64_t)(((int64_t)fg275dd_gg_mod(t3, t4))));
  t5 = ((int64_t)(v6));
  t6 = ((int64_t)(v2));
  t7 = ((int64_t)(((int64_t)fg275dd_gg_div(t5, t6))));
  t8 = ((int64_t)(v3));
  v9 = ((int64_t)(((int64_t)fg275dd_gg_mod(t7, t8))));
  t9 = ((int64_t)(v6));
  t10 = ((int64_t)(v2));
  t11 = ((int64_t)(v3));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  t13 = ((int64_t)(v4));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  v10 = ((int64_t)(((int64_t)fg275dd_gg_div(t9, t14))));
  t15 = ((int64_t)(v6));
  t16 = ((int64_t)(v2));
  t17 = ((int64_t)(v3));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t17)))));
  t19 = ((int64_t)(((int64_t)fg275dd_gg_div(t15, t18))));
  t20 = ((int64_t)(v4));
  v11 = ((int64_t)(((int64_t)fg275dd_gg_mod(t19, t20))));
  t21 = ((int64_t)(v6));
  t22 = ((int64_t)(v2));
  t23 = ((int64_t)(v3));
  t24 = ((int64_t)(v4));
  t25 = ((int64_t)(v5));
  v12 = fg275dd_gg_function_2(t21, t22, t23, t24, t25, base + 0);
  t26 = ((int64_t)(v8));
  t27 = ((int64_t)(v9));
  t28 = ((t26) == (t27));
  if (t28) {
  {
  t29 = ((int64_t)(v10));
  t30 = ((int64_t)(v4));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t29) * (uint64_t)(t30)))));
  t32 = ((int64_t)(v11));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t32)))));
  t33 = ((int64_t)(v13));
  t34 = ((int64_t)(v2));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t33) * (uint64_t)(t34)))));
  t36 = ((int64_t)(v8));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t35) + (uint64_t)(t36)))));
  t37 = ((uint64_t)(v0));
  t38 = ((int64_t)(v12));
  t39 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t40 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t38) * (uint64_t)(t39)))))));
  t41 = ((uint64_t)(v1));
  t42 = ((int64_t)(v14));
  t43 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t44 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t42) * (uint64_t)(t43)))))));
#ifndef __cplusplus
  extern float (fg275d_ld_f32)(uint64_t, uint64_t);
#endif
  t45 = fg275d_ld_f32(t41, t44);
  t46 = ((float)(t45));
#ifndef __cplusplus
  extern void (fg275d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg275d_st_f32(t37, t40, t46);
  }
  } else {
  {
  t47 = ((uint64_t)(v0));
  t48 = ((int64_t)(v12));
  t49 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t50 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t48) * (uint64_t)(t49)))))));
  t51 = ((float)(((float)(fg275dd_gg_double_bits(UINT64_C(0))))));
#ifndef __cplusplus
  extern void (fg275d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg275d_st_f32(t47, t50, t51);
  }
  }
  }
fg275dd_gg_done:;
}
