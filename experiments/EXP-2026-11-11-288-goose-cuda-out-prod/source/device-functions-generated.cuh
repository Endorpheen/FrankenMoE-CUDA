#pragma once
static __device__ __forceinline__ void fg288dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg288dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg288dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg288dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg288dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg288dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ void fg288dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg288dd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, int64_t p19, int64_t p20, int64_t p21, int64_t base);
static __device__ __forceinline__ void fg288dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg288dd_gg_div(int64_t a, int64_t b) { if (!b) fg288dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg288dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg288dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg288dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg288dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg288dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg288dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg288dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg288dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ void fg288dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg288d_fail)(int64_t);
#endif
  fg288d_fail(t2);
  }
  }
  }
fg288dd_gg_done:;
}
static __device__ __forceinline__ void fg288dd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, int64_t p19, int64_t p20, int64_t p21, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  uint8_t t13;
  int64_t t14;
  int64_t t15;
  uint8_t t16;
  uint8_t t17;
  int64_t t18;
  int64_t t19;
  uint8_t t20;
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
  int64_t t47;
  int64_t t48;
  int64_t t49;
  uint64_t t50;
  uint64_t t51;
  uint64_t t52;
  int64_t t53;
  int64_t t54;
  uint64_t t55;
  uint64_t t56;
  uint64_t t57;
  uint64_t t58;
  uint64_t t59;
  int64_t t60;
  int64_t t61;
  uint64_t t62;
  uint64_t t63;
  uint64_t t64;
  uint64_t t65;
  uint64_t t66;
  int64_t t67;
  int64_t t68;
  uint64_t t69;
  uint64_t t70;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  uint64_t v4;
  uint64_t v5;
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
  int64_t v21;
  int64_t v22;
  int64_t v23;
  int64_t v24;
  int64_t v25;
  int64_t v26;
  int64_t v27;
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
  v21 = p21;
  {
  t0 = ((int64_t)(v16));
  t1 = ((int64_t)(v18));
  t2 = ((int64_t)(((int64_t)((uint64_t)(t0) * (uint64_t)(t1)))));
  t3 = ((int64_t)(v20));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t2) + (uint64_t)(t3)))));
  t4 = ((int64_t)(v17));
  t5 = ((int64_t)(v19));
  t6 = ((int64_t)(((int64_t)((uint64_t)(t4) * (uint64_t)(t5)))));
  t7 = ((int64_t)(v21));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t6) + (uint64_t)(t7)))));
  t8 = ((int64_t)(v22));
  t9 = ((int64_t)(v6));
  t10 = ((t8) >= (t9));
  if (t10) {
  {
  goto fg288dd_gg_done;
  }
  }
  t11 = ((int64_t)(v23));
  t12 = ((int64_t)(v7));
  t13 = ((t11) >= (t12));
  if (t13) {
  {
  goto fg288dd_gg_done;
  }
  }
  t14 = ((int64_t)(v8));
  t15 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t16 = ((t14) > (t15));
  t17 = t16;
  if (t17) {
  t18 = ((int64_t)(v9));
  t19 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t17 = ((t18) > (t19));
  }
  t20 = t17;
  t21 = ((int64_t)(((int64_t)UINT64_C(30))));
  fg288dd_gg_function_2(t20, t21, base + 0);
  t22 = ((int64_t)(v23));
  t23 = ((int64_t)(v6));
  t24 = ((int64_t)(((int64_t)((uint64_t)(t22) * (uint64_t)(t23)))));
  t25 = ((int64_t)(v22));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) + (uint64_t)(t25)))));
  t27 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  v24 = ((int64_t)(((int64_t)((uint64_t)(t26) * (uint64_t)(t27)))));
  t28 = ((int64_t)(v23));
  t29 = ((int64_t)(v9));
  t30 = ((int64_t)(((int64_t)fg288dd_gg_div(t28, t29))));
  t31 = ((int64_t)(v11));
  t32 = ((int64_t)(((int64_t)((uint64_t)(t30) * (uint64_t)(t31)))));
  t33 = ((int64_t)(v22));
  t34 = ((int64_t)(v8));
  t35 = ((int64_t)(((int64_t)fg288dd_gg_div(t33, t34))));
  t36 = ((int64_t)(v10));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) * (uint64_t)(t36)))));
  v25 = ((int64_t)(((int64_t)((uint64_t)(t32) + (uint64_t)(t37)))));
  t38 = ((int64_t)(v23));
  t39 = ((int64_t)(v13));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t38) * (uint64_t)(t39)))));
  t41 = ((int64_t)(v22));
  t42 = ((int64_t)(v12));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t41) * (uint64_t)(t42)))));
  v26 = ((int64_t)(((int64_t)((uint64_t)(t40) + (uint64_t)(t43)))));
  t44 = ((int64_t)(v23));
  t45 = ((int64_t)(v15));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) * (uint64_t)(t45)))));
  t47 = ((int64_t)(v22));
  t48 = ((int64_t)(v14));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t47) * (uint64_t)(t48)))));
  v27 = ((int64_t)(((int64_t)((uint64_t)(t46) + (uint64_t)(t49)))));
  t50 = ((uint64_t)(v3));
  t51 = ((uint64_t)(((uint64_t)(v24))));
  t52 = ((uint64_t)(v0));
  t53 = ((int64_t)(v25));
  t54 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t55 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t53) * (uint64_t)(t54)))))));
  t56 = ((uint64_t)(((uint64_t)((uint64_t)(t52) + (uint64_t)(t55)))));
#ifndef __cplusplus
  extern void (fg288d_st_u64)(uint64_t, uint64_t, uint64_t);
#endif
  fg288d_st_u64(t50, t51, t56);
  t57 = ((uint64_t)(v4));
  t58 = ((uint64_t)(((uint64_t)(v24))));
  t59 = ((uint64_t)(v1));
  t60 = ((int64_t)(v26));
  t61 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t62 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t60) * (uint64_t)(t61)))))));
  t63 = ((uint64_t)(((uint64_t)((uint64_t)(t59) + (uint64_t)(t62)))));
#ifndef __cplusplus
  extern void (fg288d_st_u64)(uint64_t, uint64_t, uint64_t);
#endif
  fg288d_st_u64(t57, t58, t63);
  t64 = ((uint64_t)(v5));
  t65 = ((uint64_t)(((uint64_t)(v24))));
  t66 = ((uint64_t)(v2));
  t67 = ((int64_t)(v27));
  t68 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t69 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t67) * (uint64_t)(t68)))))));
  t70 = ((uint64_t)(((uint64_t)((uint64_t)(t66) + (uint64_t)(t69)))));
#ifndef __cplusplus
  extern void (fg288d_st_u64)(uint64_t, uint64_t, uint64_t);
#endif
  fg288d_st_u64(t64, t65, t70);
  }
fg288dd_gg_done:;
}
