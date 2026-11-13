#pragma once
static __device__ __forceinline__ void fg277dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg277dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg277dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg277dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg277dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg277dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ void fg277dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg277dd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t base);
static __device__ __forceinline__ void fg277dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg277dd_gg_div(int64_t a, int64_t b) { if (!b) fg277dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg277dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg277dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg277dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg277dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg277dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg277dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg277dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg277dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ void fg277dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg277d_fail)(int64_t);
#endif
  fg277d_fail(t2);
  }
  }
  }
fg277dd_gg_done:;
}
static __device__ __forceinline__ void fg277dd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t base) {
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
  uint8_t t10;
  int64_t t11;
  uint64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  uint64_t t19;
  int32_t t20;
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
  uint64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  uint64_t t46;
  uint64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  uint64_t t52;
  float t53;
  float t54;
  uint64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  uint64_t t60;
  float t61;
  float t62;
  float t63;
  int64_t t64;
  int64_t t65;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
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
  int64_t v15;
  int64_t v16;
  int64_t v17;
  int64_t v18;
  int64_t v19;
  int64_t v20;
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
  {
  t0 = ((int64_t)(v4));
  t1 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t2 = ((t0) > (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v5));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t4) > (t5));
  }
  t6 = t3;
  t7 = ((int64_t)(((int64_t)UINT64_C(20))));
  fg277dd_gg_function_2(t6, t7, base + 0);
  t8 = ((int64_t)(v12));
  t9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t10 = ((t8) > (t9));
  t11 = ((int64_t)(((int64_t)UINT64_C(21))));
  fg277dd_gg_function_2(t10, t11, base + 0);
  t12 = ((uint64_t)(v2));
  t13 = ((int64_t)(v10));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t13) * (uint64_t)(t14)))));
  t16 = ((int64_t)(v11));
  t17 = ((int64_t)(v9));
  t18 = ((int64_t)(((int64_t)((uint64_t)(t16) * (uint64_t)(t17)))));
  t19 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t15) + (uint64_t)(t18)))))));
#ifndef __cplusplus
  extern int32_t (fg277d_ld_i32)(uint64_t, uint64_t);
#endif
  t20 = fg277d_ld_i32(t12, t19);
  v14 = ((int64_t)(((int64_t)(t20))));
  t21 = ((int64_t)(v4));
  t22 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t21) * (uint64_t)(t22)))));
  t23 = ((int64_t)(v5));
  t24 = ((int64_t)(v15));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t23) * (uint64_t)(t24)))));
  t25 = ((int64_t)(v10));
  t26 = ((int64_t)(v15));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t25) * (uint64_t)(t26)))));
  t28 = ((int64_t)(v11));
  t29 = ((int64_t)(v16));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t27) + (uint64_t)(t30)))));
  t31 = ((int64_t)(v10));
  t32 = ((int64_t)(v6));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) * (uint64_t)(t32)))));
  t34 = ((int64_t)(v11));
  t35 = ((int64_t)(v7));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t34) * (uint64_t)(t35)))));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t33) + (uint64_t)(t36)))));
  t37 = ((int64_t)(v14));
  t38 = ((int64_t)(v8));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t37) * (uint64_t)(t38)))));
  v20 = ((int64_t)(v13));
label1:;
  t39 = ((int64_t)(v20));
  t40 = ((int64_t)(((int64_t)(v4))));
  if (!(((t39) < (t40)))) goto label2;
  {
  t41 = ((uint64_t)(v3));
  t42 = ((int64_t)(v17));
  t43 = ((int64_t)(v20));
  t44 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t45 = ((int64_t)(((int64_t)((uint64_t)(t43) * (uint64_t)(t44)))));
  t46 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t42) + (uint64_t)(t45)))))));
  t47 = ((uint64_t)(v0));
  t48 = ((int64_t)(v18));
  t49 = ((int64_t)(v20));
  t50 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t51 = ((int64_t)(((int64_t)((uint64_t)(t49) * (uint64_t)(t50)))));
  t52 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t48) + (uint64_t)(t51)))))));
#ifndef __cplusplus
  extern float (fg277d_ld_f32)(uint64_t, uint64_t);
#endif
  t53 = fg277d_ld_f32(t47, t52);
  t54 = ((float)(t53));
  t55 = ((uint64_t)(v1));
  t56 = ((int64_t)(v19));
  t57 = ((int64_t)(v20));
  t58 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t59 = ((int64_t)(((int64_t)((uint64_t)(t57) * (uint64_t)(t58)))));
  t60 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t56) + (uint64_t)(t59)))))));
#ifndef __cplusplus
  extern float (fg277d_ld_f32)(uint64_t, uint64_t);
#endif
  t61 = fg277d_ld_f32(t55, t60);
  t62 = ((float)(t61));
  t63 = ((float)(((t54) + (t62))));
#ifndef __cplusplus
  extern void (fg277d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg277d_st_f32(t41, t46, t63);
  t64 = ((int64_t)(v20));
  t65 = ((int64_t)(v12));
  v20 = ((int64_t)(((int64_t)((uint64_t)(t64) + (uint64_t)(t65)))));
  }
  goto label1;
label2:;
  }
fg277dd_gg_done:;
}
