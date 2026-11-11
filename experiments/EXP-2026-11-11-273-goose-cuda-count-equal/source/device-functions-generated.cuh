#pragma once
static __device__ __forceinline__ void fg273dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg273dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg273dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg273dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg273dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg273dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ void fg273dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg273dd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base);
static __device__ __forceinline__ void fg273dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg273dd_gg_div(int64_t a, int64_t b) { if (!b) fg273dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg273dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg273dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg273dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg273dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg273dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg273dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg273dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg273dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ void fg273dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg273d_fail)(int64_t);
#endif
  fg273d_fail(t2);
  }
  }
  }
fg273dd_gg_done:;
}
static __device__ __forceinline__ void fg273dd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base) {
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
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  uint64_t t19;
  int64_t t20;
  int64_t t21;
  uint64_t t22;
  int32_t t23;
  uint64_t t24;
  int64_t t25;
  int64_t t26;
  uint64_t t27;
  int32_t t28;
  int32_t t29;
  int32_t t30;
  uint8_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int32_t t38;
  int32_t t39;
  int32_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  uint8_t t47;
  uint64_t t48;
  int32_t t49;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int32_t v11;
  int32_t v12;
  int64_t v13;
  int32_t v14;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  t0 = ((int64_t)(v6));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v6));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t3 = ((t4) < (t5));
  }
  t6 = t3;
  t7 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg273dd_gg_function_2(t6, t7, base + 0);
  t8 = ((int64_t)(v5));
  t9 = ((int64_t)(v3));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t8) * (uint64_t)(t9)))));
  t10 = ((int64_t)(v7));
  t11 = ((int64_t)(v3));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t11)))));
  t12 = ((int64_t)(v8));
  t13 = ((int64_t)(((int64_t)(v4))));
  t14 = ((t12) > (t13));
  if (t14) {
  {
  v8 = ((int64_t)(v4));
  }
  }
  v9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((int64_t)(v7));
  t16 = ((int64_t)(v6));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t15) + (uint64_t)(t16)))));
label1:;
  t17 = ((int64_t)(v10));
  t18 = ((int64_t)(v8));
  if (!(((t17) < (t18)))) goto label2;
  {
  t19 = ((uint64_t)(v0));
  t20 = ((int64_t)(v10));
  t21 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t22 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))))));
#ifndef __cplusplus
  extern int32_t (fg273d_ld_i32)(uint64_t, uint64_t);
#endif
  t23 = fg273d_ld_i32(t19, t22);
  v11 = ((int32_t)(t23));
  t24 = ((uint64_t)(v1));
  t25 = ((int64_t)(v10));
  t26 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t27 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t25) * (uint64_t)(t26)))))));
#ifndef __cplusplus
  extern int32_t (fg273d_ld_i32)(uint64_t, uint64_t);
#endif
  t28 = fg273d_ld_i32(t24, t27);
  v12 = ((int32_t)(t28));
  t29 = ((int32_t)(v11));
  t30 = ((int32_t)(v12));
  t31 = ((t29) == (t30));
  if (t31) {
  {
  t32 = ((int64_t)(v9));
  t33 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t32) + (uint64_t)(t33)))));
  }
  }
  t34 = ((int64_t)(v10));
  t35 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t34) + (uint64_t)(t35)))));
  }
  goto label1;
label2:;
  v13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
label4:;
  t36 = ((int64_t)(v13));
  t37 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  if (!(((t36) >= (t37)))) goto label5;
  {
  t38 = ((int32_t)(((int32_t)(v9))));
  t39 = ((int32_t)(((int32_t)(v13))));
#ifndef __cplusplus
  extern int32_t (fg273d_shfl_xor)(int32_t, int32_t);
#endif
  t40 = fg273d_shfl_xor(t38, t39);
  v14 = ((int32_t)(t40));
  t41 = ((int64_t)(v9));
  t42 = ((int64_t)(((int64_t)(v14))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t41) + (uint64_t)(t42)))));
  t43 = ((int64_t)(v13));
  t44 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  v13 = ((int64_t)(((int64_t)fg273dd_gg_div(t43, t44))));
  }
  goto label4;
label5:;
  t45 = ((int64_t)(v6));
  t46 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t47 = ((t45) == (t46));
  if (t47) {
  {
  t48 = ((uint64_t)(v2));
  t49 = ((int32_t)(((int32_t)(v9))));
#ifndef __cplusplus
  extern void (fg273d_atomic_add)(uint64_t, int32_t);
#endif
  fg273d_atomic_add(t48, t49);
  }
  }
  }
fg273dd_gg_done:;
}
