#pragma once
static __device__ __forceinline__ void fg278dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg278dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg278dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg278dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg278dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg278dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ void fg278dd_gg_function_3(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg278dd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t base);
static __device__ __forceinline__ void fg278dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg278dd_gg_div(int64_t a, int64_t b) { if (!b) fg278dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg278dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg278dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg278dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg278dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg278dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg278dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg278dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg278dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ void fg278dd_gg_function_3(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg278d_fail)(int64_t);
#endif
  fg278d_fail(t2);
  }
  }
  }
fg278dd_gg_done:;
}
static __device__ __forceinline__ void fg278dd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  uint64_t t5;
  int64_t t6;
  int64_t t7;
  uint64_t t8;
  float t9;
  int64_t t10;
  int64_t t11;
  uint8_t t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  uint8_t t16;
  int64_t t17;
  int64_t t18;
  uint8_t t19;
  uint8_t t20;
  int64_t t21;
  int64_t t22;
  uint8_t t23;
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
  uint8_t t45;
  int64_t t46;
  int64_t t47;
  uint8_t t48;
  int64_t t49;
  int64_t t50;
  uint8_t t51;
  int64_t t52;
  int64_t t53;
  uint8_t t54;
  int64_t t55;
  int64_t t56;
  int64_t t57;
  int64_t t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  int64_t t63;
  int64_t t64;
  int64_t t65;
  int64_t t66;
  float t67;
  uint64_t t68;
  int64_t t69;
  int64_t t70;
  uint64_t t71;
  float t72;
  float t73;
  uint64_t t74;
  int64_t t75;
  int64_t t76;
  uint64_t t77;
  float t78;
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
  int64_t v11;
  int64_t v12;
  int64_t v13;
  float v14;
  int64_t v15;
  int64_t v16;
  int64_t v17;
  int64_t v18;
  int64_t v19;
  int64_t v20;
  int64_t v21;
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
  {
  t0 = ((int64_t)(v12));
  t1 = ((int64_t)(((int64_t)(v3))));
  t2 = ((t0) >= (t1));
  if (t2) {
  {
  goto fg278dd_gg_done;
  }
  }
  t3 = ((int64_t)(v12));
  t4 = ((int64_t)(v11));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t3) - (uint64_t)(t4)))));
  t5 = ((uint64_t)(v0));
  t6 = ((int64_t)(v12));
  t7 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t8 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t6) * (uint64_t)(t7)))))));
#ifndef __cplusplus
  extern float (fg278d_ld_f32)(uint64_t, uint64_t);
#endif
  t9 = fg278d_ld_f32(t5, t8);
  v14 = ((float)(t9));
  t10 = ((int64_t)(v13));
  t11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t12 = ((t10) >= (t11));
  if (t12) {
  {
  t13 = ((int64_t)(v8));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t15 = ((t13) > (t14));
  t16 = t15;
  if (t16) {
  t17 = ((int64_t)(v9));
  t18 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t16 = ((t17) > (t18));
  }
  t19 = t16;
  t20 = t19;
  if (t20) {
  t21 = ((int64_t)(v10));
  t22 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t20 = ((t21) > (t22));
  }
  t23 = t20;
  t24 = ((int64_t)(((int64_t)UINT64_C(20))));
  fg278dd_gg_function_3(t23, t24, base + 0);
  t25 = ((int64_t)(v13));
  t26 = ((int64_t)(v10));
  v15 = ((int64_t)(((int64_t)fg278dd_gg_div(t25, t26))));
  t27 = ((int64_t)(v13));
  t28 = ((int64_t)(v15));
  t29 = ((int64_t)(v10));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  v16 = ((int64_t)(((int64_t)((uint64_t)(t27) - (uint64_t)(t30)))));
  t31 = ((int64_t)(v16));
  t32 = ((int64_t)(v9));
  v17 = ((int64_t)(((int64_t)fg278dd_gg_div(t31, t32))));
  t33 = ((int64_t)(v16));
  t34 = ((int64_t)(v17));
  t35 = ((int64_t)(v9));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t34) * (uint64_t)(t35)))));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t33) - (uint64_t)(t36)))));
  t37 = ((int64_t)(v18));
  t38 = ((int64_t)(v8));
  v19 = ((int64_t)(((int64_t)fg278dd_gg_div(t37, t38))));
  t39 = ((int64_t)(v18));
  t40 = ((int64_t)(v19));
  t41 = ((int64_t)(v8));
  t42 = ((int64_t)(((int64_t)((uint64_t)(t40) * (uint64_t)(t41)))));
  v20 = ((int64_t)(((int64_t)((uint64_t)(t39) - (uint64_t)(t42)))));
  t43 = ((int64_t)(v20));
  t44 = ((int64_t)(((int64_t)(v4))));
  t45 = ((t43) < (t44));
  if (t45) {
  {
  t46 = ((int64_t)(v19));
  t47 = ((int64_t)(((int64_t)(v5))));
  t48 = ((t46) < (t47));
  if (t48) {
  {
  t49 = ((int64_t)(v17));
  t50 = ((int64_t)(((int64_t)(v6))));
  t51 = ((t49) < (t50));
  if (t51) {
  {
  t52 = ((int64_t)(v15));
  t53 = ((int64_t)(((int64_t)(v7))));
  t54 = ((t52) < (t53));
  if (t54) {
  {
  t55 = ((int64_t)(v15));
  t56 = ((int64_t)(v6));
  t57 = ((int64_t)(((int64_t)((uint64_t)(t55) * (uint64_t)(t56)))));
  t58 = ((int64_t)(v17));
  t59 = ((int64_t)(((int64_t)((uint64_t)(t57) + (uint64_t)(t58)))));
  t60 = ((int64_t)(v5));
  t61 = ((int64_t)(((int64_t)((uint64_t)(t59) * (uint64_t)(t60)))));
  t62 = ((int64_t)(v19));
  t63 = ((int64_t)(((int64_t)((uint64_t)(t61) + (uint64_t)(t62)))));
  t64 = ((int64_t)(v4));
  t65 = ((int64_t)(((int64_t)((uint64_t)(t63) * (uint64_t)(t64)))));
  t66 = ((int64_t)(v20));
  v21 = ((int64_t)(((int64_t)((uint64_t)(t65) + (uint64_t)(t66)))));
  t67 = ((float)(v14));
  t68 = ((uint64_t)(v1));
  t69 = ((int64_t)(v21));
  t70 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t71 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t69) * (uint64_t)(t70)))))));
#ifndef __cplusplus
  extern float (fg278d_ld_f32)(uint64_t, uint64_t);
#endif
  t72 = fg278d_ld_f32(t68, t71);
  t73 = ((float)(t72));
  v14 = ((float)(((t67) + (t73))));
  }
  }
  }
  }
  }
  }
  }
  }
  }
  }
  t74 = ((uint64_t)(v2));
  t75 = ((int64_t)(v12));
  t76 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t77 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t75) * (uint64_t)(t76)))))));
  t78 = ((float)(v14));
#ifndef __cplusplus
  extern void (fg278d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg278d_st_f32(t74, t77, t78);
  }
fg278dd_gg_done:;
}
