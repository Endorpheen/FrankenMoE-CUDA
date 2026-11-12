#pragma once
static __device__ __forceinline__ void fg276dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg276dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg276dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg276dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg276dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg276dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ void fg276dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg276dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base);
static __device__ __forceinline__ void fg276dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg276dd_gg_div(int64_t a, int64_t b) { if (!b) fg276dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg276dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg276dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg276dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg276dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg276dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg276dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg276dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg276dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ void fg276dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg276d_fail)(int64_t);
#endif
  fg276d_fail(t2);
  }
  }
  }
fg276dd_gg_done:;
}
static __device__ __forceinline__ void fg276dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base) {
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
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  uint64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  uint64_t t27;
  float t28;
  int64_t t29;
  int64_t t30;
  uint8_t t31;
  uint64_t t32;
  int64_t t33;
  int64_t t34;
  uint64_t t35;
  float t36;
  float t37;
  float t38;
  float t39;
  float t40;
  float t41;
  float t42;
  float t43;
  float t44;
  float t45;
  float t46;
  float t47;
  float t48;
  uint64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  uint64_t t54;
  float t55;
  float t56;
  float t57;
  uint64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  uint64_t t65;
  float t66;
  float t67;
  float t68;
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
  float v11;
  float v12;
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
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t4) > (t5));
  }
  t6 = t3;
  t7 = ((int64_t)(((int64_t)UINT64_C(20))));
  fg276dd_gg_function_2(t6, t7, base + 0);
  t8 = ((int64_t)(v3));
  t9 = ((int64_t)(((int64_t)UINT64_C(2))));
  v7 = ((int64_t)(((int64_t)fg276dd_gg_div(t8, t9))));
  t10 = ((int64_t)(v5));
  t11 = ((int64_t)(v2));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  t12 = ((int64_t)(v3));
  t13 = ((int64_t)(((int64_t)UINT64_C(2))));
  t14 = ((int64_t)(((int64_t)fg276dd_gg_mod(t12, t13))));
  t15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t16 = ((t14) != (t15));
  if (t16) {
  {
  t17 = ((int64_t)(v6));
  t18 = ((int64_t)(v7));
  t19 = ((t17) == (t18));
  if (t19) {
  {
  t20 = ((uint64_t)(v0));
  t21 = ((int64_t)(v8));
  t22 = ((int64_t)(((int64_t)UINT64_C(2))));
  t23 = ((int64_t)(v7));
  t24 = ((int64_t)(((int64_t)((uint64_t)(t22) * (uint64_t)(t23)))));
  t25 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t25)))));
  t27 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t21) + (uint64_t)(t26)))))));
  t28 = ((float)(((float)(fg276dd_gg_double_bits(UINT64_C(0))))));
#ifndef __cplusplus
  extern void (fg276d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg276d_st_f32(t20, t27, t28);
  }
  }
  }
  }
  t29 = ((int64_t)(v6));
  t30 = ((int64_t)(((int64_t)(v7))));
  t31 = ((t29) >= (t30));
  if (t31) {
  {
  goto fg276dd_gg_done;
  }
  }
  t32 = ((uint64_t)(v1));
  t33 = ((int64_t)(v5));
  t34 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t35 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t33) * (uint64_t)(t34)))))));
#ifndef __cplusplus
  extern float (fg276d_ld_f32)(uint64_t, uint64_t);
#endif
  t36 = fg276d_ld_f32(t32, t35);
  v9 = ((float)(t36));
  t37 = ((float)(((float)(((int64_t)(v4))))));
#ifndef __cplusplus
  extern float (fg276d_logf)(float);
#endif
  t38 = fg276d_logf(t37);
  v10 = ((float)(t38));
  t39 = ((float)(((float)(fg276dd_gg_double_bits(UINT64_C(0))))));
  t40 = ((float)(v10));
  t41 = ((float)(((t39) - (t40))));
  t42 = ((float)(((float)(v6))));
  t43 = ((float)(((t41) * (t42))));
  t44 = ((float)(((float)(v7))));
  t45 = ((float)(((t43) / (t44))));
#ifndef __cplusplus
  extern float (fg276d_expf)(float);
#endif
  t46 = fg276d_expf(t45);
  v11 = ((float)(t46));
  t47 = ((float)(v9));
  t48 = ((float)(v11));
  v12 = ((float)(((t47) * (t48))));
  t49 = ((uint64_t)(v0));
  t50 = ((int64_t)(v8));
  t51 = ((int64_t)(v6));
  t52 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t53 = ((int64_t)(((int64_t)((uint64_t)(t51) * (uint64_t)(t52)))));
  t54 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t50) + (uint64_t)(t53)))))));
  t55 = ((float)(v12));
#ifndef __cplusplus
  extern float (fg276d_cosf)(float);
#endif
  t56 = fg276d_cosf(t55);
  t57 = ((float)(t56));
#ifndef __cplusplus
  extern void (fg276d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg276d_st_f32(t49, t54, t57);
  t58 = ((uint64_t)(v0));
  t59 = ((int64_t)(v8));
  t60 = ((int64_t)(v6));
  t61 = ((int64_t)(v7));
  t62 = ((int64_t)(((int64_t)((uint64_t)(t60) + (uint64_t)(t61)))));
  t63 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t64 = ((int64_t)(((int64_t)((uint64_t)(t62) * (uint64_t)(t63)))));
  t65 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t59) + (uint64_t)(t64)))))));
  t66 = ((float)(v12));
#ifndef __cplusplus
  extern float (fg276d_sinf)(float);
#endif
  t67 = fg276d_sinf(t66);
  t68 = ((float)(t67));
#ifndef __cplusplus
  extern void (fg276d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg276d_st_f32(t58, t65, t68);
  }
fg276dd_gg_done:;
}
