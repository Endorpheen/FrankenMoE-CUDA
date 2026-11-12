#pragma once
static __device__ __forceinline__ void fg259m_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg259m_gg_index(int64_t i, int64_t len);
static __device__ __forceinline__ int64_t fg259m_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg259m_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg259m_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg259m_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg259m_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ uint8_t fg259m_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base);
static __device__ __forceinline__ void fg259m_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg259m_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg259m_gg_fail("array index out of bounds"); return i; }
static __device__ __forceinline__ int64_t fg259m_gg_div(int64_t a, int64_t b) { if (!b) fg259m_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg259m_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg259m_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg259m_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg259m_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg259m_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg259m_gg_umod(uint64_t a,uint64_t b) { if(!b)fg259m_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg259m_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ uint8_t fg259m_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base) {
  _Alignas(8) uint8_t t0;
  _Alignas(8) uint64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) uint64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) uint64_t t11;
  _Alignas(8) uint64_t t12;
  _Alignas(8) uint64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) uint64_t t18;
  _Alignas(8) uint64_t t19;
  _Alignas(8) uint64_t t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) int64_t t24;
  _Alignas(8) uint64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) uint8_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) int64_t t32;
  _Alignas(8) int64_t t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) int64_t t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) int64_t t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) int64_t t41;
  _Alignas(8) int64_t t42;
  _Alignas(8) uint64_t t43;
  _Alignas(8) int64_t t44;
  _Alignas(8) int64_t t45;
  _Alignas(8) int64_t t46;
  _Alignas(8) int64_t t47;
  _Alignas(8) int64_t t48;
  _Alignas(8) int64_t t49;
  _Alignas(8) int64_t t50;
  _Alignas(8) int64_t t51;
  _Alignas(8) int64_t t52;
  _Alignas(8) int64_t t53;
  _Alignas(8) int64_t t54;
  _Alignas(8) int64_t t55;
  _Alignas(8) int64_t t56;
  _Alignas(8) int64_t t57;
  _Alignas(8) int64_t t58;
  _Alignas(8) int64_t t59;
  _Alignas(8) int64_t t60;
  _Alignas(8) int64_t t61;
  _Alignas(8) uint64_t v0;
  _Alignas(8) uint64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) uint64_t v7;
  _Alignas(8) int64_t v8;
  _Alignas(8) int64_t v9;
  _Alignas(8) int64_t v10;
  _Alignas(8) int64_t v11;
  _Alignas(8) int64_t v12;
  _Alignas(8) int64_t v13;
  _Alignas(8) int64_t v14;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((int64_t)(v2));
  t3 = ((int64_t)(v4));
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(((int64_t)((uint64_t)(t3) * (uint64_t)(t4)))));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t2) + (uint64_t)(t5)))));
  t7 = ((int64_t)(((int64_t)UINT64_C(176))));
  t8 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t6) * (uint64_t)(t7)))))));
  v7 = ((uint64_t)(((uint64_t)((uint64_t)(t1) + (uint64_t)(t8)))));
  t9 = ((int64_t)(v5));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  v8 = ((int64_t)(((int64_t)fg259m_gg_mod(t9, t10))));
  t11 = ((uint64_t)(v7));
  t12 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t13 = ((uint64_t)(((uint64_t)((uint64_t)(t11) + (uint64_t)(t12)))));
  t14 = ((int64_t)(v8));
  t15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) + (uint64_t)(t15)))));
#ifndef __cplusplus
  extern int64_t (fg259m_unpack_scales)(uint64_t, int64_t);
#endif
  t17 = fg259m_unpack_scales(t13, t16);
  v9 = ((int64_t)(t17));
  t18 = ((uint64_t)(v7));
  t19 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(4))))));
  t20 = ((uint64_t)(((uint64_t)((uint64_t)(t18) + (uint64_t)(t19)))));
  t21 = ((int64_t)(v8));
  t22 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) + (uint64_t)(t22)))));
#ifndef __cplusplus
  extern int64_t (fg259m_unpack_scales)(uint64_t, int64_t);
#endif
  t24 = fg259m_unpack_scales(t20, t23);
  v10 = ((int64_t)(t24));
  t25 = ((uint64_t)(v7));
  t26 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
#ifndef __cplusplus
  extern int64_t (fg259m_ld_i32)(uint64_t, int64_t);
#endif
  t27 = fg259m_ld_i32(t25, t26);
  v11 = ((int64_t)(t27));
  v12 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label1:;
  t28 = ((int64_t)(v12));
  t29 = ((int64_t)(((int64_t)UINT64_C(4))));
  t30 = ((t28) < (t29));
  if (!(t30)) goto label2;
  {
  t31 = ((int64_t)(v9));
  t32 = ((int64_t)(v12));
  t33 = ((int64_t)(((int64_t)UINT64_C(8))));
  t34 = ((int64_t)(((int64_t)(((int64_t)((uint64_t)(t32) * (uint64_t)(t33)))))));
  t35 = ((int64_t)(((t31) >> ((uint64_t)(t34) & 63))));
  t36 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(255))))));
  v13 = ((int64_t)(((t35) & (t36))));
  t37 = ((int64_t)(v10));
  t38 = ((int64_t)(v12));
  t39 = ((int64_t)(((int64_t)UINT64_C(8))));
  t40 = ((int64_t)(((int64_t)(((int64_t)((uint64_t)(t38) * (uint64_t)(t39)))))));
  t41 = ((int64_t)(((t37) >> ((uint64_t)(t40) & 63))));
  t42 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(255))))));
  v14 = ((int64_t)(((t41) & (t42))));
  t43 = ((uint64_t)(v1));
  t44 = ((int64_t)(v4));
  t45 = ((int64_t)(v6));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) * (uint64_t)(t45)))));
  t47 = ((int64_t)(((int64_t)UINT64_C(4))));
  t48 = ((int64_t)(((int64_t)(v8))));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t47) * (uint64_t)(t48)))));
  t50 = ((int64_t)(((int64_t)((uint64_t)(t46) + (uint64_t)(t49)))));
  t51 = ((int64_t)(v12));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t50) + (uint64_t)(t51)))));
  t53 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t52) * (uint64_t)(t53)))));
  t55 = ((int64_t)(v11));
  t56 = ((int64_t)(v13));
  t57 = ((int64_t)(v14));
#ifndef __cplusplus
  extern int64_t (fg259m_dm_scale)(int64_t, int64_t, int64_t);
#endif
  t58 = fg259m_dm_scale(t55, t56, t57);
  t59 = ((int64_t)(t58));
#ifndef __cplusplus
  extern void (fg259m_st_i32)(uint64_t, int64_t, int64_t);
#endif
  fg259m_st_i32(t43, t54, t59);
  t60 = ((int64_t)(v12));
  t61 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t60) + (uint64_t)(t61)))));
  }
  goto label1;
label2:;
  t0 = ((uint8_t)UINT64_C(1));
  }
  return t0;
}
static __device__ __forceinline__ uint8_t fg259m_run(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6){return fg259m_gg_function_1(p0, p1, p2, p3, p4, p5, p6, 0);}

