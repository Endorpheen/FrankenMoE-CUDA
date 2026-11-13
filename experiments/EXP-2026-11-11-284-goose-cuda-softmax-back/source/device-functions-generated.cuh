#pragma once
static __device__ __forceinline__ void fg284dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg284dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg284dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg284dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg284dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg284dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ float fg284dd_gg_function_5(float p0, int64_t base);
static __device__ __forceinline__ void fg284dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg284dd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, float p4, int64_t p5, int64_t p6, int64_t base);
static __device__ __forceinline__ void fg284dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg284dd_gg_div(int64_t a, int64_t b) { if (!b) fg284dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg284dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg284dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg284dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg284dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg284dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg284dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg284dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg284dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ float fg284dd_gg_function_5(float p0, int64_t base) {
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
  extern float (fg284d_shfl_xor)(float, int32_t);
#endif
  t5 = fg284d_shfl_xor(t3, t4);
  v3 = ((float)(t5));
  t6 = ((float)(v1));
  t7 = ((float)(v3));
  v1 = ((float)(((t6) + (t7))));
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  v2 = ((int64_t)(((int64_t)fg284dd_gg_div(t8, t9))));
  }
  goto label1;
label2:;
  t0 = ((float)(v1));
  }
fg284dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg284dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg284d_fail)(int64_t);
#endif
  fg284d_fail(t2);
  }
  }
  }
fg284dd_gg_done:;
}
static __device__ __forceinline__ void fg284dd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, float p4, int64_t p5, int64_t p6, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  float t8;
  uint64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint64_t t14;
  float t15;
  float t16;
  uint64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  uint64_t t22;
  float t23;
  float t24;
  float t25;
  int64_t t26;
  int64_t t27;
  float t28;
  int64_t t29;
  int64_t t30;
  uint64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  uint64_t t36;
  float t37;
  uint64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  uint64_t t43;
  float t44;
  uint64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  uint64_t t50;
  float t51;
  float t52;
  float t53;
  float t54;
  float t55;
  float t56;
  float t57;
  int64_t t58;
  int64_t t59;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  int64_t v3;
  float v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  float v8;
  int64_t v9;
  float v10;
  float v11;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  t0 = ((int64_t)(v3));
  t1 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t2 = ((t0) > (t1));
  t3 = ((int64_t)(((int64_t)UINT64_C(20))));
  fg284dd_gg_function_2(t2, t3, base + 0);
  t4 = ((int64_t)(v6));
  t5 = ((int64_t)(v3));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  v8 = ((float)(((float)(fg284dd_gg_double_bits(UINT64_C(0))))));
  v9 = ((int64_t)(v5));
label1:;
  t6 = ((int64_t)(v9));
  t7 = ((int64_t)(((int64_t)(v3))));
  if (!(((t6) < (t7)))) goto label2;
  {
  t8 = ((float)(v8));
  t9 = ((uint64_t)(v1));
  t10 = ((int64_t)(v7));
  t11 = ((int64_t)(v9));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t11)))));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t14 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))))));
#ifndef __cplusplus
  extern float (fg284d_ld_f32)(uint64_t, uint64_t);
#endif
  t15 = fg284d_ld_f32(t9, t14);
  t16 = ((float)(t15));
  t17 = ((uint64_t)(v0));
  t18 = ((int64_t)(v7));
  t19 = ((int64_t)(v9));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t18) + (uint64_t)(t19)))));
  t21 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t22 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))))));
#ifndef __cplusplus
  extern float (fg284d_ld_f32)(uint64_t, uint64_t);
#endif
  t23 = fg284d_ld_f32(t17, t22);
  t24 = ((float)(t23));
  t25 = ((float)(((t16) * (t24))));
  v8 = ((float)(((t8) + (t25))));
  t26 = ((int64_t)(v9));
  t27 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t26) + (uint64_t)(t27)))));
  }
  goto label1;
label2:;
  t28 = ((float)(v8));
  v8 = fg284dd_gg_function_5(t28, base + 0);
  v9 = ((int64_t)(v5));
label4:;
  t29 = ((int64_t)(v9));
  t30 = ((int64_t)(((int64_t)(v3))));
  if (!(((t29) < (t30)))) goto label5;
  {
  t31 = ((uint64_t)(v0));
  t32 = ((int64_t)(v7));
  t33 = ((int64_t)(v9));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) + (uint64_t)(t33)))));
  t35 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t36 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t34) * (uint64_t)(t35)))))));
#ifndef __cplusplus
  extern float (fg284d_ld_f32)(uint64_t, uint64_t);
#endif
  t37 = fg284d_ld_f32(t31, t36);
  v10 = ((float)(t37));
  t38 = ((uint64_t)(v1));
  t39 = ((int64_t)(v7));
  t40 = ((int64_t)(v9));
  t41 = ((int64_t)(((int64_t)((uint64_t)(t39) + (uint64_t)(t40)))));
  t42 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t43 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t41) * (uint64_t)(t42)))))));
#ifndef __cplusplus
  extern float (fg284d_ld_f32)(uint64_t, uint64_t);
#endif
  t44 = fg284d_ld_f32(t38, t43);
  v11 = ((float)(t44));
  t45 = ((uint64_t)(v2));
  t46 = ((int64_t)(v7));
  t47 = ((int64_t)(v9));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t46) + (uint64_t)(t47)))));
  t49 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t50 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t48) * (uint64_t)(t49)))))));
  t51 = ((float)(v4));
  t52 = ((float)(v10));
  t53 = ((float)(v8));
  t54 = ((float)(((t52) - (t53))));
  t55 = ((float)(((t51) * (t54))));
  t56 = ((float)(v11));
  t57 = ((float)(((t55) * (t56))));
#ifndef __cplusplus
  extern void (fg284d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg284d_st_f32(t45, t50, t57);
  t58 = ((int64_t)(v9));
  t59 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t58) + (uint64_t)(t59)))));
  }
  goto label4;
label5:;
  }
fg284dd_gg_done:;
}
