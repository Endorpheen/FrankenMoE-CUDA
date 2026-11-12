#pragma once
static __device__ __forceinline__ void fg286dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg286dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg286dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg286dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg286dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg286dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ void fg286dd_gg_function_13(uint64_t p0, uint64_t p1, float p2, float p3, int64_t p4, int64_t p5, int64_t base);
static __device__ __forceinline__ void fg286dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg286dd_gg_function_10(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base);
static __device__ __forceinline__ void fg286dd_gg_function_7(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base);
static __device__ __forceinline__ void fg286dd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base);
static __device__ __forceinline__ void fg286dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg286dd_gg_div(int64_t a, int64_t b) { if (!b) fg286dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg286dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg286dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg286dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg286dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg286dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg286dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg286dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg286dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ void fg286dd_gg_function_13(uint64_t p0, uint64_t p1, float p2, float p3, int64_t p4, int64_t p5, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint64_t t3;
  int64_t t4;
  int64_t t5;
  uint64_t t6;
  float t7;
  uint64_t t8;
  int64_t t9;
  int64_t t10;
  uint64_t t11;
  float t12;
  float t13;
  float t14;
  float t15;
  float t16;
  float t17;
  float t18;
  uint64_t v0;
  uint64_t v1;
  float v2;
  float v3;
  int64_t v4;
  int64_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t0 = ((int64_t)(v5));
  t1 = ((int64_t)(((int64_t)(v4))));
  t2 = ((t0) >= (t1));
  if (t2) {
  {
  goto fg286dd_gg_done;
  }
  }
  t3 = ((uint64_t)(v1));
  t4 = ((int64_t)(v5));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t6 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t5)))))));
  t7 = ((float)(v2));
  t8 = ((uint64_t)(v0));
  t9 = ((int64_t)(v5));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t11 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t9) * (uint64_t)(t10)))))));
#ifndef __cplusplus
  extern float (fg286d_ld_f32)(uint64_t, uint64_t);
#endif
  t12 = fg286d_ld_f32(t8, t11);
  t13 = ((float)(t12));
  t14 = ((float)(((t7) * (t13))));
#ifndef __cplusplus
  extern float (fg286d_tanhf)(float);
#endif
  t15 = fg286d_tanhf(t14);
  t16 = ((float)(t15));
  t17 = ((float)(v3));
  t18 = ((float)(((t16) * (t17))));
#ifndef __cplusplus
  extern void (fg286d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg286d_st_f32(t3, t6, t18);
  }
fg286dd_gg_done:;
}
static __device__ __forceinline__ void fg286dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg286d_fail)(int64_t);
#endif
  fg286d_fail(t2);
  }
  }
  }
fg286dd_gg_done:;
}
static __device__ __forceinline__ void fg286dd_gg_function_10(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint64_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t t12;
  float t13;
  uint64_t t14;
  int64_t t15;
  int64_t t16;
  uint64_t t17;
  float t18;
  float t19;
  float t20;
  float t21;
  float t22;
  uint64_t t23;
  int64_t t24;
  int64_t t25;
  uint64_t t26;
  float t27;
  float t28;
  float t29;
  float t30;
  uint64_t t31;
  int64_t t32;
  int64_t t33;
  uint64_t t34;
  float t35;
  float t36;
  float t37;
  float t38;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  float v8;
  float v9;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  t0 = ((int64_t)(v6));
  t1 = ((int64_t)(((int64_t)(v4))));
  t2 = ((t0) >= (t1));
  if (t2) {
  {
  goto fg286dd_gg_done;
  }
  }
  t3 = ((int64_t)(v5));
  t4 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t5 = ((t3) > (t4));
  t6 = ((int64_t)(((int64_t)UINT64_C(20))));
  fg286dd_gg_function_2(t5, t6, base + 0);
  t7 = ((int64_t)(v6));
  t8 = ((int64_t)(v5));
  v7 = ((int64_t)(((int64_t)fg286dd_gg_div(t7, t8))));
  t9 = ((uint64_t)(v0));
  t10 = ((int64_t)(v6));
  t11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t12 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))))));
#ifndef __cplusplus
  extern float (fg286d_ld_bf16)(uint64_t, uint64_t);
#endif
  t13 = fg286d_ld_bf16(t9, t12);
  v8 = ((float)(t13));
  t14 = ((uint64_t)(v1));
  t15 = ((int64_t)(v7));
  t16 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t17 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t15) * (uint64_t)(t16)))))));
#ifndef __cplusplus
  extern float (fg286d_ld_f32)(uint64_t, uint64_t);
#endif
  t18 = fg286d_ld_f32(t14, t17);
  t19 = ((float)(t18));
  t20 = ((float)(v8));
  t21 = ((float)(((t19) * (t20))));
#ifndef __cplusplus
  extern float (fg286d_sinf)(float);
#endif
  t22 = fg286d_sinf(t21);
  v9 = ((float)(t22));
  t23 = ((uint64_t)(v3));
  t24 = ((int64_t)(v6));
  t25 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t26 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t25)))))));
  t27 = ((float)(v8));
  t28 = ((float)(v9));
  t29 = ((float)(v9));
  t30 = ((float)(((t28) * (t29))));
  t31 = ((uint64_t)(v2));
  t32 = ((int64_t)(v7));
  t33 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t34 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t32) * (uint64_t)(t33)))))));
#ifndef __cplusplus
  extern float (fg286d_ld_f32)(uint64_t, uint64_t);
#endif
  t35 = fg286d_ld_f32(t31, t34);
  t36 = ((float)(t35));
  t37 = ((float)(((t30) * (t36))));
  t38 = ((float)(((t27) + (t37))));
#ifndef __cplusplus
  extern void (fg286d_st_bf16)(uint64_t, uint64_t, float);
#endif
  fg286d_st_bf16(t23, t26, t38);
  }
fg286dd_gg_done:;
}
static __device__ __forceinline__ void fg286dd_gg_function_7(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint64_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t t12;
  float t13;
  uint64_t t14;
  int64_t t15;
  int64_t t16;
  uint64_t t17;
  float t18;
  float t19;
  float t20;
  float t21;
  float t22;
  uint64_t t23;
  int64_t t24;
  int64_t t25;
  uint64_t t26;
  float t27;
  float t28;
  float t29;
  float t30;
  uint64_t t31;
  int64_t t32;
  int64_t t33;
  uint64_t t34;
  float t35;
  float t36;
  float t37;
  float t38;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  float v8;
  float v9;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  t0 = ((int64_t)(v6));
  t1 = ((int64_t)(((int64_t)(v4))));
  t2 = ((t0) >= (t1));
  if (t2) {
  {
  goto fg286dd_gg_done;
  }
  }
  t3 = ((int64_t)(v5));
  t4 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t5 = ((t3) > (t4));
  t6 = ((int64_t)(((int64_t)UINT64_C(20))));
  fg286dd_gg_function_2(t5, t6, base + 0);
  t7 = ((int64_t)(v6));
  t8 = ((int64_t)(v5));
  v7 = ((int64_t)(((int64_t)fg286dd_gg_div(t7, t8))));
  t9 = ((uint64_t)(v0));
  t10 = ((int64_t)(v6));
  t11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t12 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))))));
#ifndef __cplusplus
  extern float (fg286d_ld_half)(uint64_t, uint64_t);
#endif
  t13 = fg286d_ld_half(t9, t12);
  v8 = ((float)(t13));
  t14 = ((uint64_t)(v1));
  t15 = ((int64_t)(v7));
  t16 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t17 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t15) * (uint64_t)(t16)))))));
#ifndef __cplusplus
  extern float (fg286d_ld_f32)(uint64_t, uint64_t);
#endif
  t18 = fg286d_ld_f32(t14, t17);
  t19 = ((float)(t18));
  t20 = ((float)(v8));
  t21 = ((float)(((t19) * (t20))));
#ifndef __cplusplus
  extern float (fg286d_sinf)(float);
#endif
  t22 = fg286d_sinf(t21);
  v9 = ((float)(t22));
  t23 = ((uint64_t)(v3));
  t24 = ((int64_t)(v6));
  t25 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t26 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t25)))))));
  t27 = ((float)(v8));
  t28 = ((float)(v9));
  t29 = ((float)(v9));
  t30 = ((float)(((t28) * (t29))));
  t31 = ((uint64_t)(v2));
  t32 = ((int64_t)(v7));
  t33 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t34 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t32) * (uint64_t)(t33)))))));
#ifndef __cplusplus
  extern float (fg286d_ld_f32)(uint64_t, uint64_t);
#endif
  t35 = fg286d_ld_f32(t31, t34);
  t36 = ((float)(t35));
  t37 = ((float)(((t30) * (t36))));
  t38 = ((float)(((t27) + (t37))));
#ifndef __cplusplus
  extern void (fg286d_st_half)(uint64_t, uint64_t, float);
#endif
  fg286d_st_half(t23, t26, t38);
  }
fg286dd_gg_done:;
}
static __device__ __forceinline__ void fg286dd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  uint8_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint64_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t t12;
  float t13;
  uint64_t t14;
  int64_t t15;
  int64_t t16;
  uint64_t t17;
  float t18;
  float t19;
  float t20;
  float t21;
  float t22;
  uint64_t t23;
  int64_t t24;
  int64_t t25;
  uint64_t t26;
  float t27;
  float t28;
  float t29;
  float t30;
  uint64_t t31;
  int64_t t32;
  int64_t t33;
  uint64_t t34;
  float t35;
  float t36;
  float t37;
  float t38;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  float v8;
  float v9;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  t0 = ((int64_t)(v6));
  t1 = ((int64_t)(((int64_t)(v4))));
  t2 = ((t0) >= (t1));
  if (t2) {
  {
  goto fg286dd_gg_done;
  }
  }
  t3 = ((int64_t)(v5));
  t4 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t5 = ((t3) > (t4));
  t6 = ((int64_t)(((int64_t)UINT64_C(20))));
  fg286dd_gg_function_2(t5, t6, base + 0);
  t7 = ((int64_t)(v6));
  t8 = ((int64_t)(v5));
  v7 = ((int64_t)(((int64_t)fg286dd_gg_div(t7, t8))));
  t9 = ((uint64_t)(v0));
  t10 = ((int64_t)(v6));
  t11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t12 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))))));
#ifndef __cplusplus
  extern float (fg286d_ld_f32)(uint64_t, uint64_t);
#endif
  t13 = fg286d_ld_f32(t9, t12);
  v8 = ((float)(t13));
  t14 = ((uint64_t)(v1));
  t15 = ((int64_t)(v7));
  t16 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t17 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t15) * (uint64_t)(t16)))))));
#ifndef __cplusplus
  extern float (fg286d_ld_f32)(uint64_t, uint64_t);
#endif
  t18 = fg286d_ld_f32(t14, t17);
  t19 = ((float)(t18));
  t20 = ((float)(v8));
  t21 = ((float)(((t19) * (t20))));
#ifndef __cplusplus
  extern float (fg286d_sinf)(float);
#endif
  t22 = fg286d_sinf(t21);
  v9 = ((float)(t22));
  t23 = ((uint64_t)(v3));
  t24 = ((int64_t)(v6));
  t25 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t26 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t25)))))));
  t27 = ((float)(v8));
  t28 = ((float)(v9));
  t29 = ((float)(v9));
  t30 = ((float)(((t28) * (t29))));
  t31 = ((uint64_t)(v2));
  t32 = ((int64_t)(v7));
  t33 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t34 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t32) * (uint64_t)(t33)))))));
#ifndef __cplusplus
  extern float (fg286d_ld_f32)(uint64_t, uint64_t);
#endif
  t35 = fg286d_ld_f32(t31, t34);
  t36 = ((float)(t35));
  t37 = ((float)(((t30) * (t36))));
  t38 = ((float)(((t27) + (t37))));
#ifndef __cplusplus
  extern void (fg286d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg286d_st_f32(t23, t26, t38);
  }
fg286dd_gg_done:;
}
