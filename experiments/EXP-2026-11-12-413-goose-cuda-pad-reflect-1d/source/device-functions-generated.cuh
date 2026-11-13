#pragma once
static __device__ __forceinline__ void fg413dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg413dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg413dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg413dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg413dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg413dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ int64_t fg413dd_gg_function_4(int64_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg413dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ int64_t fg413dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, int64_t p19, int64_t p20, int64_t base);
static __device__ __forceinline__ void fg413dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg413dd_gg_div(int64_t a, int64_t b) { if (!b) fg413dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg413dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg413dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg413dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg413dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg413dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg413dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg413dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg413dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ int64_t fg413dd_gg_function_4(int64_t p0, int64_t p1, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) uint8_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) int64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) int64_t v0;
  _Alignas(8) int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  t4 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t5 = ((int64_t)(v0));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t4) - (uint64_t)(t5)))));
  goto fg413dd_gg_done;
  }
  }
  t6 = ((int64_t)(v0));
  t7 = ((int64_t)(v1));
  t8 = ((t6) < (t7));
  if (t8) {
  {
  t0 = ((int64_t)(v0));
  goto fg413dd_gg_done;
  }
  }
  t9 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t10 = ((int64_t)(v1));
  t11 = ((int64_t)(((int64_t)((uint64_t)(t9) * (uint64_t)(t10)))));
  t12 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) - (uint64_t)(t12)))));
  t14 = ((int64_t)(v0));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t13) - (uint64_t)(t14)))));
  goto fg413dd_gg_done;
  }
fg413dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg413dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
  _Alignas(8) uint8_t t0;
  _Alignas(8) uint8_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t v0;
  _Alignas(8) int64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t0 = v0;
  t1 = (!(t0));
  if (t1) {
  {
  t2 = ((int64_t)(v1));
#ifndef __cplusplus
  extern void (fg413d_fail)(int64_t);
#endif
  fg413d_fail(t2);
  }
  }
  }
}
static __device__ __forceinline__ int64_t fg413dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, int64_t p19, int64_t p20, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) uint8_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) uint8_t t7;
  _Alignas(8) uint8_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) uint8_t t11;
  _Alignas(8) uint8_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) uint8_t t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) int64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) int64_t t24;
  _Alignas(8) int64_t t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) uint8_t t27;
  _Alignas(8) uint8_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) uint8_t t31;
  _Alignas(8) uint8_t t32;
  _Alignas(8) int64_t t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) uint8_t t35;
  _Alignas(8) uint8_t t36;
  _Alignas(8) int64_t t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) uint8_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) int64_t t41;
  _Alignas(8) int64_t t42;
  _Alignas(8) int64_t t43;
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
  _Alignas(8) int64_t t62;
  _Alignas(8) int64_t t63;
  _Alignas(8) int64_t t64;
  _Alignas(8) int64_t t65;
  _Alignas(8) int64_t t66;
  _Alignas(8) int64_t t67;
  _Alignas(8) int64_t t68;
  _Alignas(8) int64_t t69;
  _Alignas(8) int64_t t70;
  _Alignas(8) int64_t t71;
  _Alignas(8) uint64_t t72;
  _Alignas(8) uint64_t t73;
  _Alignas(8) float t74;
  _Alignas(8) uint64_t t75;
  _Alignas(8) uint64_t t76;
  _Alignas(8) float t77;
  _Alignas(8) uint64_t v0;
  _Alignas(8) uint64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) int64_t v7;
  _Alignas(8) int64_t v8;
  _Alignas(8) int64_t v9;
  _Alignas(8) int64_t v10;
  _Alignas(8) int64_t v11;
  _Alignas(8) int64_t v12;
  _Alignas(8) int64_t v13;
  _Alignas(8) int64_t v14;
  _Alignas(8) int64_t v15;
  _Alignas(8) int64_t v16;
  _Alignas(8) int64_t v17;
  _Alignas(8) int64_t v18;
  _Alignas(8) int64_t v19;
  _Alignas(8) int64_t v20;
  _Alignas(8) int64_t v21;
  _Alignas(8) int64_t v22;
  _Alignas(8) int64_t v23;
  _Alignas(8) int64_t v24;
  _Alignas(8) int64_t v25;
  _Alignas(8) int64_t v26;
  _Alignas(8) int64_t v27;
  _Alignas(8) uint64_t v28;
  _Alignas(8) uint64_t v29;
  _Alignas(8) float v30;
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
  v16 = p16;
  v17 = p17;
  v18 = p18;
  v19 = p19;
  v20 = p20;
  {
  t1 = ((int64_t)(v3));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) > (t2));
  t4 = t3;
  if (t4) {
  t5 = ((int64_t)(v4));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t4 = ((t5) > (t6));
  }
  t7 = t4;
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v5));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t8 = ((t9) > (t10));
  }
  t11 = t8;
  t12 = t11;
  if (t12) {
  t13 = ((int64_t)(v6));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t12 = ((t13) > (t14));
  }
  t15 = t12;
  t16 = ((int64_t)(((int64_t)UINT64_C(20))));
  fg413dd_gg_function_2(t15, t16, base + 0);
  v21 = ((int64_t)(v18));
  v22 = ((int64_t)(v17));
  t17 = ((int64_t)(v16));
  t18 = ((int64_t)(v4));
  v23 = ((int64_t)(((int64_t)fg413dd_gg_mod(t17, t18))));
  t19 = ((int64_t)(v16));
  t20 = ((int64_t)(v4));
  v24 = ((int64_t)(((int64_t)fg413dd_gg_div(t19, t20))));
  t21 = ((int64_t)(v19));
  t22 = ((int64_t)(v24));
  t23 = ((int64_t)(v20));
  t24 = ((int64_t)(((int64_t)((uint64_t)(t22) * (uint64_t)(t23)))));
  v25 = ((int64_t)(((int64_t)((uint64_t)(t21) + (uint64_t)(t24)))));
  t25 = ((int64_t)(v25));
  t26 = ((int64_t)(v2));
  t27 = ((t25) >= (t26));
  t28 = t27;
  if (!t28) {
  t29 = ((int64_t)(v23));
  t30 = ((int64_t)(v4));
  t28 = ((t29) >= (t30));
  }
  t31 = t28;
  t32 = t31;
  if (!t32) {
  t33 = ((int64_t)(v22));
  t34 = ((int64_t)(v5));
  t32 = ((t33) >= (t34));
  }
  t35 = t32;
  t36 = t35;
  if (!t36) {
  t37 = ((int64_t)(v21));
  t38 = ((int64_t)(v6));
  t36 = ((t37) >= (t38));
  }
  t39 = t36;
  if (t39) {
  {
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  goto fg413dd_gg_done;
  }
  }
  t40 = ((int64_t)(v25));
  t41 = ((int64_t)(v15));
  v26 = ((int64_t)(((int64_t)((uint64_t)(t40) - (uint64_t)(t41)))));
  t42 = ((int64_t)(v26));
  t43 = ((int64_t)(v3));
  v27 = fg413dd_gg_function_4(t42, t43, base + 0);
  t44 = ((int64_t)(v21));
  t45 = ((int64_t)(v10));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) * (uint64_t)(t45)))));
  t47 = ((int64_t)(v22));
  t48 = ((int64_t)(v9));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t47) * (uint64_t)(t48)))));
  t50 = ((int64_t)(((int64_t)((uint64_t)(t46) + (uint64_t)(t49)))));
  t51 = ((int64_t)(v23));
  t52 = ((int64_t)(v8));
  t53 = ((int64_t)(((int64_t)((uint64_t)(t51) * (uint64_t)(t52)))));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t50) + (uint64_t)(t53)))));
  t55 = ((int64_t)(v27));
  t56 = ((int64_t)(v7));
  t57 = ((int64_t)(((int64_t)((uint64_t)(t55) * (uint64_t)(t56)))));
  v28 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t54) + (uint64_t)(t57)))))));
  t58 = ((int64_t)(v21));
  t59 = ((int64_t)(v14));
  t60 = ((int64_t)(((int64_t)((uint64_t)(t58) * (uint64_t)(t59)))));
  t61 = ((int64_t)(v22));
  t62 = ((int64_t)(v13));
  t63 = ((int64_t)(((int64_t)((uint64_t)(t61) * (uint64_t)(t62)))));
  t64 = ((int64_t)(((int64_t)((uint64_t)(t60) + (uint64_t)(t63)))));
  t65 = ((int64_t)(v23));
  t66 = ((int64_t)(v12));
  t67 = ((int64_t)(((int64_t)((uint64_t)(t65) * (uint64_t)(t66)))));
  t68 = ((int64_t)(((int64_t)((uint64_t)(t64) + (uint64_t)(t67)))));
  t69 = ((int64_t)(v25));
  t70 = ((int64_t)(v11));
  t71 = ((int64_t)(((int64_t)((uint64_t)(t69) * (uint64_t)(t70)))));
  v29 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t68) + (uint64_t)(t71)))))));
  t72 = ((uint64_t)(v0));
  t73 = ((uint64_t)(v28));
#ifndef __cplusplus
  extern float (fg413d_ld_f32)(uint64_t, uint64_t);
#endif
  t74 = fg413d_ld_f32(t72, t73);
  v30 = ((float)(t74));
  t75 = ((uint64_t)(v1));
  t76 = ((uint64_t)(v29));
  t77 = ((float)(v30));
#ifndef __cplusplus
  extern void (fg413d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg413d_st_f32(t75, t76, t77);
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto fg413dd_gg_done;
  }
fg413dd_gg_done:;
  return t0;
}
