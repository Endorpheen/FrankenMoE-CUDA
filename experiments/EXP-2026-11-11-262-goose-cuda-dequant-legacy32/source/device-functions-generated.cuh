#pragma once
static __device__ __forceinline__ void fg262dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg262dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg262dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg262dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg262dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg262dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ void fg262dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg262dd_gg_function_13(uint64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base);
static __device__ __forceinline__ void fg262dd_gg_function_12(uint64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base);
static __device__ __forceinline__ void fg262dd_gg_function_10(uint64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base);
static __device__ __forceinline__ void fg262dd_gg_function_9(uint64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base);
static __device__ __forceinline__ void fg262dd_gg_function_8(uint64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base);
static __device__ __forceinline__ void fg262dd_gg_function_7(uint64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base);
static __device__ __forceinline__ void fg262dd_gg_function_1(uint64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base);
static __device__ __forceinline__ void fg262dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg262dd_gg_div(int64_t a, int64_t b) { if (!b) fg262dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg262dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg262dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg262dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg262dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg262dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg262dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg262dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg262dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ void fg262dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg262d_fail)(int64_t);
#endif
  fg262d_fail(t2);
  }
  }
  }
fg262dd_gg_done:;
}
static __device__ __forceinline__ void fg262dd_gg_function_13(uint64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint64_t t6;
  uint64_t t7;
  float t8;
  uint64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint64_t t14;
  int32_t t15;
  uint64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint64_t t21;
  int32_t t22;
  uint64_t t23;
  uint64_t t24;
  float t25;
  float t26;
  float t27;
  uint64_t t28;
  uint64_t t29;
  float t30;
  float t31;
  float t32;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  uint64_t v3;
  int64_t v4;
  float v5;
  int64_t v6;
  int64_t v7;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v2));
  t1 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t2 = ((t0) >= (t1));
  t3 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg262dd_gg_function_2(t2, t3, base + 0);
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(34))))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  t6 = ((uint64_t)(v0));
  t7 = ((uint64_t)(((uint64_t)(v4))));
#ifndef __cplusplus
  extern float (fg262d_ld_half)(uint64_t, uint64_t);
#endif
  t8 = fg262d_ld_half(t6, t7);
  v5 = ((float)(t8));
  t9 = ((uint64_t)(v0));
  t10 = ((int64_t)(v4));
  t11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t11)))));
  t13 = ((int64_t)(v2));
  t14 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t12) + (uint64_t)(t13)))))));
#ifndef __cplusplus
  extern int32_t (fg262d_ld_i8)(uint64_t, uint64_t);
#endif
  t15 = fg262d_ld_i8(t9, t14);
  v6 = ((int64_t)(((int64_t)(t15))));
  t16 = ((uint64_t)(v0));
  t17 = ((int64_t)(v4));
  t18 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(3))))));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t18)))));
  t20 = ((int64_t)(v2));
  t21 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t19) + (uint64_t)(t20)))))));
#ifndef __cplusplus
  extern int32_t (fg262d_ld_i8)(uint64_t, uint64_t);
#endif
  t22 = fg262d_ld_i8(t16, t21);
  v7 = ((int64_t)(((int64_t)(t22))));
  t23 = ((uint64_t)(v3));
  t24 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t25 = ((float)(((float)(v6))));
  t26 = ((float)(v5));
  t27 = ((float)(((t25) * (t26))));
#ifndef __cplusplus
  extern void (fg262d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg262d_st_f32(t23, t24, t27);
  t28 = ((uint64_t)(v3));
  t29 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t30 = ((float)(((float)(v7))));
  t31 = ((float)(v5));
  t32 = ((float)(((t30) * (t31))));
#ifndef __cplusplus
  extern void (fg262d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg262d_st_f32(t28, t29, t32);
  }
fg262dd_gg_done:;
}
static __device__ __forceinline__ void fg262dd_gg_function_12(uint64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint64_t t6;
  uint64_t t7;
  float t8;
  uint64_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t t12;
  float t13;
  uint64_t t14;
  int64_t t15;
  int64_t t16;
  uint64_t t17;
  uint32_t t18;
  int64_t t19;
  int64_t t20;
  uint32_t t21;
  uint32_t t22;
  uint32_t t23;
  uint32_t t24;
  uint32_t t25;
  uint32_t t26;
  uint32_t t27;
  uint32_t t28;
  uint32_t t29;
  uint32_t t30;
  uint64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  uint64_t t36;
  uint32_t t37;
  uint32_t t38;
  uint32_t t39;
  uint32_t t40;
  uint32_t t41;
  uint32_t t42;
  uint32_t t43;
  uint32_t t44;
  uint32_t t45;
  uint64_t t46;
  uint64_t t47;
  float t48;
  float t49;
  float t50;
  float t51;
  float t52;
  uint64_t t53;
  uint64_t t54;
  float t55;
  float t56;
  float t57;
  float t58;
  float t59;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  uint64_t v3;
  int64_t v4;
  float v5;
  float v6;
  uint32_t v7;
  uint32_t v8;
  uint32_t v9;
  uint32_t v10;
  uint32_t v11;
  uint32_t v12;
  int64_t v13;
  int64_t v14;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v2));
  t1 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t2 = ((t0) >= (t1));
  t3 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg262dd_gg_function_2(t2, t3, base + 0);
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(24))))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  t6 = ((uint64_t)(v0));
  t7 = ((uint64_t)(((uint64_t)(v4))));
#ifndef __cplusplus
  extern float (fg262d_ld_half)(uint64_t, uint64_t);
#endif
  t8 = fg262d_ld_half(t6, t7);
  v5 = ((float)(t8));
  t9 = ((uint64_t)(v0));
  t10 = ((int64_t)(v4));
  t11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t12 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t11)))))));
#ifndef __cplusplus
  extern float (fg262d_ld_half)(uint64_t, uint64_t);
#endif
  t13 = fg262d_ld_half(t9, t12);
  v6 = ((float)(t13));
  t14 = ((uint64_t)(v0));
  t15 = ((int64_t)(v4));
  t16 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t17 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t15) + (uint64_t)(t16)))))));
#ifndef __cplusplus
  extern uint32_t (fg262d_ld_u32)(uint64_t, uint64_t);
#endif
  t18 = fg262d_ld_u32(t14, t17);
  v7 = ((uint32_t)(t18));
  v8 = ((uint32_t)(((uint32_t)(v2))));
  t19 = ((int64_t)(v2));
  t20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(12))))));
  v9 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t19) + (uint64_t)(t20)))))));
  t21 = ((uint32_t)(v7));
  t22 = ((uint32_t)(v8));
  t23 = ((uint32_t)(((t21) >> ((uint64_t)(t22) & 31))));
  t24 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4))))));
  t25 = ((uint32_t)(((uint32_t)((uint64_t)(t23) << ((uint64_t)(t24) & 31)))));
  t26 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(16))))));
  v10 = ((uint32_t)(((t25) & (t26))));
  t27 = ((uint32_t)(v7));
  t28 = ((uint32_t)(v9));
  t29 = ((uint32_t)(((t27) >> ((uint64_t)(t28) & 31))));
  t30 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(16))))));
  v11 = ((uint32_t)(((t29) & (t30))));
  t31 = ((uint64_t)(v0));
  t32 = ((int64_t)(v4));
  t33 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) + (uint64_t)(t33)))));
  t35 = ((int64_t)(v2));
  t36 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t34) + (uint64_t)(t35)))))));
#ifndef __cplusplus
  extern uint32_t (fg262d_ld_u8)(uint64_t, uint64_t);
#endif
  t37 = fg262d_ld_u8(t31, t36);
  v12 = ((uint32_t)(t37));
  t38 = ((uint32_t)(v12));
  t39 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(15))))));
  t40 = ((uint32_t)(((t38) & (t39))));
  t41 = ((uint32_t)(v10));
  v13 = ((int64_t)(((int64_t)(((t40) | (t41))))));
  t42 = ((uint32_t)(v12));
  t43 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4))))));
  t44 = ((uint32_t)(((t42) >> ((uint64_t)(t43) & 31))));
  t45 = ((uint32_t)(v11));
  v14 = ((int64_t)(((int64_t)(((t44) | (t45))))));
  t46 = ((uint64_t)(v3));
  t47 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t48 = ((float)(((float)(v13))));
  t49 = ((float)(v5));
  t50 = ((float)(((t48) * (t49))));
  t51 = ((float)(v6));
  t52 = ((float)(((t50) + (t51))));
#ifndef __cplusplus
  extern void (fg262d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg262d_st_f32(t46, t47, t52);
  t53 = ((uint64_t)(v3));
  t54 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t55 = ((float)(((float)(v14))));
  t56 = ((float)(v5));
  t57 = ((float)(((t55) * (t56))));
  t58 = ((float)(v6));
  t59 = ((float)(((t57) + (t58))));
#ifndef __cplusplus
  extern void (fg262d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg262d_st_f32(t53, t54, t59);
  }
fg262dd_gg_done:;
}
static __device__ __forceinline__ void fg262dd_gg_function_10(uint64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint64_t t6;
  uint64_t t7;
  float t8;
  uint64_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t t12;
  uint32_t t13;
  int64_t t14;
  int64_t t15;
  uint32_t t16;
  uint32_t t17;
  uint32_t t18;
  uint32_t t19;
  uint32_t t20;
  uint32_t t21;
  uint32_t t22;
  uint32_t t23;
  uint32_t t24;
  uint32_t t25;
  uint64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  uint64_t t31;
  uint32_t t32;
  uint32_t t33;
  uint32_t t34;
  uint32_t t35;
  uint32_t t36;
  uint32_t t37;
  uint32_t t38;
  uint32_t t39;
  uint32_t t40;
  uint64_t t41;
  uint64_t t42;
  float t43;
  float t44;
  float t45;
  float t46;
  float t47;
  uint64_t t48;
  uint64_t t49;
  float t50;
  float t51;
  float t52;
  float t53;
  float t54;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  uint64_t v3;
  int64_t v4;
  float v5;
  uint32_t v6;
  uint32_t v7;
  uint32_t v8;
  uint32_t v9;
  uint32_t v10;
  uint32_t v11;
  int64_t v12;
  int64_t v13;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v2));
  t1 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t2 = ((t0) >= (t1));
  t3 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg262dd_gg_function_2(t2, t3, base + 0);
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(22))))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  t6 = ((uint64_t)(v0));
  t7 = ((uint64_t)(((uint64_t)(v4))));
#ifndef __cplusplus
  extern float (fg262d_ld_half)(uint64_t, uint64_t);
#endif
  t8 = fg262d_ld_half(t6, t7);
  v5 = ((float)(t8));
  t9 = ((uint64_t)(v0));
  t10 = ((int64_t)(v4));
  t11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t12 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t11)))))));
#ifndef __cplusplus
  extern uint32_t (fg262d_ld_u32)(uint64_t, uint64_t);
#endif
  t13 = fg262d_ld_u32(t9, t12);
  v6 = ((uint32_t)(t13));
  v7 = ((uint32_t)(((uint32_t)(v2))));
  t14 = ((int64_t)(v2));
  t15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(12))))));
  v8 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t14) + (uint64_t)(t15)))))));
  t16 = ((uint32_t)(v6));
  t17 = ((uint32_t)(v7));
  t18 = ((uint32_t)(((t16) >> ((uint64_t)(t17) & 31))));
  t19 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4))))));
  t20 = ((uint32_t)(((uint32_t)((uint64_t)(t18) << ((uint64_t)(t19) & 31)))));
  t21 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(16))))));
  v9 = ((uint32_t)(((t20) & (t21))));
  t22 = ((uint32_t)(v6));
  t23 = ((uint32_t)(v8));
  t24 = ((uint32_t)(((t22) >> ((uint64_t)(t23) & 31))));
  t25 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(16))))));
  v10 = ((uint32_t)(((t24) & (t25))));
  t26 = ((uint64_t)(v0));
  t27 = ((int64_t)(v4));
  t28 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(6))))));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) + (uint64_t)(t28)))));
  t30 = ((int64_t)(v2));
  t31 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t30)))))));
#ifndef __cplusplus
  extern uint32_t (fg262d_ld_u8)(uint64_t, uint64_t);
#endif
  t32 = fg262d_ld_u8(t26, t31);
  v11 = ((uint32_t)(t32));
  t33 = ((uint32_t)(v11));
  t34 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(15))))));
  t35 = ((uint32_t)(((t33) & (t34))));
  t36 = ((uint32_t)(v9));
  v12 = ((int64_t)(((int64_t)(((t35) | (t36))))));
  t37 = ((uint32_t)(v11));
  t38 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4))))));
  t39 = ((uint32_t)(((t37) >> ((uint64_t)(t38) & 31))));
  t40 = ((uint32_t)(v10));
  v13 = ((int64_t)(((int64_t)(((t39) | (t40))))));
  t41 = ((uint64_t)(v3));
  t42 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t43 = ((float)(((float)(v12))));
  t44 = ((float)(((float)(fg262dd_gg_double_bits(UINT64_C(4625196817309499392))))));
  t45 = ((float)(((t43) - (t44))));
  t46 = ((float)(v5));
  t47 = ((float)(((t45) * (t46))));
#ifndef __cplusplus
  extern void (fg262d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg262d_st_f32(t41, t42, t47);
  t48 = ((uint64_t)(v3));
  t49 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t50 = ((float)(((float)(v13))));
  t51 = ((float)(((float)(fg262dd_gg_double_bits(UINT64_C(4625196817309499392))))));
  t52 = ((float)(((t50) - (t51))));
  t53 = ((float)(v5));
  t54 = ((float)(((t52) * (t53))));
#ifndef __cplusplus
  extern void (fg262d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg262d_st_f32(t48, t49, t54);
  }
fg262dd_gg_done:;
}
static __device__ __forceinline__ void fg262dd_gg_function_9(uint64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint64_t t6;
  uint64_t t7;
  float t8;
  uint64_t t9;
  int64_t t10;
  int64_t t11;
  uint64_t t12;
  float t13;
  uint64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  uint64_t t19;
  uint32_t t20;
  uint32_t t21;
  uint32_t t22;
  uint32_t t23;
  uint32_t t24;
  uint64_t t25;
  uint64_t t26;
  float t27;
  float t28;
  float t29;
  float t30;
  float t31;
  uint64_t t32;
  uint64_t t33;
  float t34;
  float t35;
  float t36;
  float t37;
  float t38;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  uint64_t v3;
  int64_t v4;
  float v5;
  float v6;
  uint32_t v7;
  int64_t v8;
  int64_t v9;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v2));
  t1 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t2 = ((t0) >= (t1));
  t3 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg262dd_gg_function_2(t2, t3, base + 0);
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(20))))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  t6 = ((uint64_t)(v0));
  t7 = ((uint64_t)(((uint64_t)(v4))));
#ifndef __cplusplus
  extern float (fg262d_ld_half)(uint64_t, uint64_t);
#endif
  t8 = fg262d_ld_half(t6, t7);
  v5 = ((float)(t8));
  t9 = ((uint64_t)(v0));
  t10 = ((int64_t)(v4));
  t11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t12 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t11)))))));
#ifndef __cplusplus
  extern float (fg262d_ld_half)(uint64_t, uint64_t);
#endif
  t13 = fg262d_ld_half(t9, t12);
  v6 = ((float)(t13));
  t14 = ((uint64_t)(v0));
  t15 = ((int64_t)(v4));
  t16 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t15) + (uint64_t)(t16)))));
  t18 = ((int64_t)(v2));
  t19 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t18)))))));
#ifndef __cplusplus
  extern uint32_t (fg262d_ld_u8)(uint64_t, uint64_t);
#endif
  t20 = fg262d_ld_u8(t14, t19);
  v7 = ((uint32_t)(t20));
  t21 = ((uint32_t)(v7));
  t22 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(15))))));
  v8 = ((int64_t)(((int64_t)(((t21) & (t22))))));
  t23 = ((uint32_t)(v7));
  t24 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4))))));
  v9 = ((int64_t)(((int64_t)(((t23) >> ((uint64_t)(t24) & 31))))));
  t25 = ((uint64_t)(v3));
  t26 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t27 = ((float)(((float)(v8))));
  t28 = ((float)(v5));
  t29 = ((float)(((t27) * (t28))));
  t30 = ((float)(v6));
  t31 = ((float)(((t29) + (t30))));
#ifndef __cplusplus
  extern void (fg262d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg262d_st_f32(t25, t26, t31);
  t32 = ((uint64_t)(v3));
  t33 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t34 = ((float)(((float)(v9))));
  t35 = ((float)(v5));
  t36 = ((float)(((t34) * (t35))));
  t37 = ((float)(v6));
  t38 = ((float)(((t36) + (t37))));
#ifndef __cplusplus
  extern void (fg262d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg262d_st_f32(t32, t33, t38);
  }
fg262dd_gg_done:;
}
static __device__ __forceinline__ void fg262dd_gg_function_8(uint64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint64_t t6;
  uint64_t t7;
  float t8;
  uint64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint64_t t14;
  uint32_t t15;
  uint32_t t16;
  uint32_t t17;
  uint32_t t18;
  uint32_t t19;
  uint64_t t20;
  uint64_t t21;
  float t22;
  float t23;
  float t24;
  float t25;
  float t26;
  uint64_t t27;
  uint64_t t28;
  float t29;
  float t30;
  float t31;
  float t32;
  float t33;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  uint64_t v3;
  int64_t v4;
  float v5;
  uint32_t v6;
  int64_t v7;
  int64_t v8;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v2));
  t1 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t2 = ((t0) >= (t1));
  t3 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg262dd_gg_function_2(t2, t3, base + 0);
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(18))))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  t6 = ((uint64_t)(v0));
  t7 = ((uint64_t)(((uint64_t)(v4))));
#ifndef __cplusplus
  extern float (fg262d_ld_half)(uint64_t, uint64_t);
#endif
  t8 = fg262d_ld_half(t6, t7);
  v5 = ((float)(t8));
  t9 = ((uint64_t)(v0));
  t10 = ((int64_t)(v4));
  t11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t11)))));
  t13 = ((int64_t)(v2));
  t14 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t12) + (uint64_t)(t13)))))));
#ifndef __cplusplus
  extern uint32_t (fg262d_ld_u8)(uint64_t, uint64_t);
#endif
  t15 = fg262d_ld_u8(t9, t14);
  v6 = ((uint32_t)(t15));
  t16 = ((uint32_t)(v6));
  t17 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(15))))));
  v7 = ((int64_t)(((int64_t)(((t16) & (t17))))));
  t18 = ((uint32_t)(v6));
  t19 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4))))));
  v8 = ((int64_t)(((int64_t)(((t18) >> ((uint64_t)(t19) & 31))))));
  t20 = ((uint64_t)(v3));
  t21 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t22 = ((float)(((float)(v7))));
  t23 = ((float)(((float)(fg262dd_gg_double_bits(UINT64_C(4620693217682128896))))));
  t24 = ((float)(((t22) - (t23))));
  t25 = ((float)(v5));
  t26 = ((float)(((t24) * (t25))));
#ifndef __cplusplus
  extern void (fg262d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg262d_st_f32(t20, t21, t26);
  t27 = ((uint64_t)(v3));
  t28 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t29 = ((float)(((float)(v8))));
  t30 = ((float)(((float)(fg262dd_gg_double_bits(UINT64_C(4620693217682128896))))));
  t31 = ((float)(((t29) - (t30))));
  t32 = ((float)(v5));
  t33 = ((float)(((t31) * (t32))));
#ifndef __cplusplus
  extern void (fg262d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg262d_st_f32(t27, t28, t33);
  }
fg262dd_gg_done:;
}
static __device__ __forceinline__ void fg262dd_gg_function_7(uint64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint64_t t6;
  uint64_t t7;
  float t8;
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
  uint64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  uint64_t t28;
  uint32_t t29;
  uint32_t t30;
  uint32_t t31;
  uint32_t t32;
  uint32_t t33;
  uint64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  uint64_t t39;
  uint32_t t40;
  uint32_t t41;
  uint32_t t42;
  uint32_t t43;
  uint32_t t44;
  uint64_t t45;
  uint64_t t46;
  int64_t t47;
  int64_t t48;
  float t49;
  float t50;
  float t51;
  uint64_t t52;
  uint64_t t53;
  int64_t t54;
  int64_t t55;
  float t56;
  float t57;
  float t58;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  uint64_t v3;
  int64_t v4;
  float v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  uint32_t v12;
  uint32_t v13;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v2));
  t1 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t2 = ((t0) >= (t1));
  t3 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg262dd_gg_function_2(t2, t3, base + 0);
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(18))))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  t6 = ((uint64_t)(v0));
  t7 = ((uint64_t)(((uint64_t)(v4))));
#ifndef __cplusplus
  extern float (fg262d_ld_half)(uint64_t, uint64_t);
#endif
  t8 = fg262d_ld_half(t6, t7);
  v5 = ((float)(t8));
  v6 = ((int64_t)(v2));
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t9) + (uint64_t)(t10)))));
  t11 = ((int64_t)(v6));
  t12 = ((int64_t)(((int64_t)UINT64_C(4))));
  v8 = ((int64_t)(((int64_t)fg262dd_gg_div(t11, t12))));
  t13 = ((int64_t)(v6));
  t14 = ((int64_t)(((int64_t)UINT64_C(4))));
  t15 = ((int64_t)(((int64_t)fg262dd_gg_mod(t13, t14))));
  t16 = ((int64_t)(((int64_t)UINT64_C(2))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t15) * (uint64_t)(t16)))));
  t17 = ((int64_t)(v7));
  t18 = ((int64_t)(((int64_t)UINT64_C(4))));
  v10 = ((int64_t)(((int64_t)fg262dd_gg_div(t17, t18))));
  t19 = ((int64_t)(v7));
  t20 = ((int64_t)(((int64_t)UINT64_C(4))));
  t21 = ((int64_t)(((int64_t)fg262dd_gg_mod(t19, t20))));
  t22 = ((int64_t)(((int64_t)UINT64_C(2))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t21) * (uint64_t)(t22)))));
  t23 = ((uint64_t)(v0));
  t24 = ((int64_t)(v4));
  t25 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) + (uint64_t)(t25)))));
  t27 = ((int64_t)(v8));
  t28 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t26) + (uint64_t)(t27)))))));
#ifndef __cplusplus
  extern uint32_t (fg262d_ld_u8)(uint64_t, uint64_t);
#endif
  t29 = fg262d_ld_u8(t23, t28);
  t30 = ((uint32_t)(t29));
  t31 = ((uint32_t)(((uint32_t)(v9))));
  t32 = ((uint32_t)(((t30) >> ((uint64_t)(t31) & 31))));
  t33 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(3))))));
  v12 = ((uint32_t)(((t32) & (t33))));
  t34 = ((uint64_t)(v0));
  t35 = ((int64_t)(v4));
  t36 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) + (uint64_t)(t36)))));
  t38 = ((int64_t)(v10));
  t39 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t37) + (uint64_t)(t38)))))));
#ifndef __cplusplus
  extern uint32_t (fg262d_ld_u8)(uint64_t, uint64_t);
#endif
  t40 = fg262d_ld_u8(t34, t39);
  t41 = ((uint32_t)(t40));
  t42 = ((uint32_t)(((uint32_t)(v11))));
  t43 = ((uint32_t)(((t41) >> ((uint64_t)(t42) & 31))));
  t44 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(3))))));
  v13 = ((uint32_t)(((t43) & (t44))));
  t45 = ((uint64_t)(v3));
  t46 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t47 = ((int64_t)(((int64_t)(v12))));
  t48 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t49 = ((float)(((float)(((int64_t)((uint64_t)(t47) - (uint64_t)(t48)))))));
  t50 = ((float)(v5));
  t51 = ((float)(((t49) * (t50))));
#ifndef __cplusplus
  extern void (fg262d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg262d_st_f32(t45, t46, t51);
  t52 = ((uint64_t)(v3));
  t53 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t54 = ((int64_t)(((int64_t)(v13))));
  t55 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t56 = ((float)(((float)(((int64_t)((uint64_t)(t54) - (uint64_t)(t55)))))));
  t57 = ((float)(v5));
  t58 = ((float)(((t56) * (t57))));
#ifndef __cplusplus
  extern void (fg262d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg262d_st_f32(t52, t53, t58);
  }
fg262dd_gg_done:;
}
static __device__ __forceinline__ void fg262dd_gg_function_1(uint64_t p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint64_t t6;
  uint64_t t7;
  float t8;
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
  uint64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  uint64_t t24;
  uint32_t t25;
  uint32_t t26;
  uint32_t t27;
  uint32_t t28;
  uint32_t t29;
  uint64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  uint64_t t35;
  uint32_t t36;
  uint32_t t37;
  uint32_t t38;
  uint32_t t39;
  uint32_t t40;
  uint64_t t41;
  uint64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  float t47;
  float t48;
  float t49;
  uint64_t t50;
  uint64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
  int64_t t55;
  float t56;
  float t57;
  float t58;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  uint64_t v3;
  int64_t v4;
  float v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  int64_t v10;
  int64_t v11;
  uint32_t v12;
  uint32_t v13;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t0 = ((int64_t)(v2));
  t1 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t2 = ((t0) >= (t1));
  t3 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg262dd_gg_function_2(t2, t3, base + 0);
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(18))))));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  t6 = ((uint64_t)(v0));
  t7 = ((uint64_t)(((uint64_t)(v4))));
#ifndef __cplusplus
  extern float (fg262d_ld_half)(uint64_t, uint64_t);
#endif
  t8 = fg262d_ld_half(t6, t7);
  v5 = ((float)(t8));
  v6 = ((int64_t)(v2));
  t9 = ((int64_t)(v2));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t9) + (uint64_t)(t10)))));
  t11 = ((int64_t)(v6));
  t12 = ((int64_t)(((int64_t)UINT64_C(8))));
  v8 = ((int64_t)(((int64_t)fg262dd_gg_div(t11, t12))));
  t13 = ((int64_t)(v6));
  t14 = ((int64_t)(((int64_t)UINT64_C(8))));
  v9 = ((int64_t)(((int64_t)fg262dd_gg_mod(t13, t14))));
  t15 = ((int64_t)(v7));
  t16 = ((int64_t)(((int64_t)UINT64_C(8))));
  v10 = ((int64_t)(((int64_t)fg262dd_gg_div(t15, t16))));
  t17 = ((int64_t)(v7));
  t18 = ((int64_t)(((int64_t)UINT64_C(8))));
  v11 = ((int64_t)(((int64_t)fg262dd_gg_mod(t17, t18))));
  t19 = ((uint64_t)(v0));
  t20 = ((int64_t)(v4));
  t21 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) + (uint64_t)(t21)))));
  t23 = ((int64_t)(v8));
  t24 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t22) + (uint64_t)(t23)))))));
#ifndef __cplusplus
  extern uint32_t (fg262d_ld_u8)(uint64_t, uint64_t);
#endif
  t25 = fg262d_ld_u8(t19, t24);
  t26 = ((uint32_t)(t25));
  t27 = ((uint32_t)(((uint32_t)(v9))));
  t28 = ((uint32_t)(((t26) >> ((uint64_t)(t27) & 31))));
  t29 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(1))))));
  v12 = ((uint32_t)(((t28) & (t29))));
  t30 = ((uint64_t)(v0));
  t31 = ((int64_t)(v4));
  t32 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t32)))));
  t34 = ((int64_t)(v10));
  t35 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t33) + (uint64_t)(t34)))))));
#ifndef __cplusplus
  extern uint32_t (fg262d_ld_u8)(uint64_t, uint64_t);
#endif
  t36 = fg262d_ld_u8(t30, t35);
  t37 = ((uint32_t)(t36));
  t38 = ((uint32_t)(((uint32_t)(v11))));
  t39 = ((uint32_t)(((t37) >> ((uint64_t)(t38) & 31))));
  t40 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(1))))));
  v13 = ((uint32_t)(((t39) & (t40))));
  t41 = ((uint64_t)(v3));
  t42 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t43 = ((int64_t)(((int64_t)UINT64_C(2))));
  t44 = ((int64_t)(((int64_t)(v12))));
  t45 = ((int64_t)(((int64_t)((uint64_t)(t43) * (uint64_t)(t44)))));
  t46 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t47 = ((float)(((float)(((int64_t)((uint64_t)(t45) - (uint64_t)(t46)))))));
  t48 = ((float)(v5));
  t49 = ((float)(((t47) * (t48))));
#ifndef __cplusplus
  extern void (fg262d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg262d_st_f32(t41, t42, t49);
  t50 = ((uint64_t)(v3));
  t51 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t52 = ((int64_t)(((int64_t)UINT64_C(2))));
  t53 = ((int64_t)(((int64_t)(v13))));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t52) * (uint64_t)(t53)))));
  t55 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t56 = ((float)(((float)(((int64_t)((uint64_t)(t54) - (uint64_t)(t55)))))));
  t57 = ((float)(v5));
  t58 = ((float)(((t56) * (t57))));
#ifndef __cplusplus
  extern void (fg262d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg262d_st_f32(t50, t51, t58);
  }
fg262dd_gg_done:;
}
