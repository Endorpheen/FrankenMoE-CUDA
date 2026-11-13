#pragma once
static __device__ __forceinline__ void fg188dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg188dd_gg_index(int64_t i, int64_t len);
static __device__ __forceinline__ int64_t fg188dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg188dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg188dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg188dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg188dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ void fg188dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg188dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base);
static __device__ __forceinline__ void fg188dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg188dd_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg188dd_gg_fail("array index out of bounds"); return i; }
static __device__ __forceinline__ int64_t fg188dd_gg_div(int64_t a, int64_t b) { if (!b) fg188dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg188dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg188dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg188dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg188dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg188dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg188dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg188dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg188dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ void fg188dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  fg188d_fail(t2);
  }
  }
  }
fg188dd_gg_done:;
}
static __device__ __forceinline__ void fg188dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base) {
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
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
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
  uint64_t t31;
  uint64_t t32;
  uint64_t t33;
  float t34;
  float t35;
  float t36;
  float t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int64_t t45;
  uint64_t t46;
  int64_t t47;
  int64_t t48;
  uint64_t t49;
  uint64_t t50;
  uint32_t t51;
  uint32_t t52;
  uint32_t t53;
  uint32_t t54;
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
  uint64_t t66;
  uint64_t t67;
  uint64_t t68;
  float t69;
  float t70;
  float t71;
  float t72;
  float t73;
  uint64_t t74;
  uint64_t t75;
  uint64_t t76;
  float t77;
  float t78;
  float t79;
  float t80;
  float t81;
  int64_t t82;
  int64_t t83;
  uint64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  float v10;
  float v11;
  int64_t v12;
  int64_t v13;
  uint32_t v14;
  uint32_t v15;
  uint32_t v16;
  int64_t v17;
  int64_t v18;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t0 = ((int64_t)(v4));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t3 = ((t4) < (t5));
  }
  t6 = t3;
  t7 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg188dd_gg_function_2(t6, t7, base + 0);
  t8 = ((int64_t)(v4));
  t9 = ((int64_t)(((int64_t)UINT64_C(8))));
  v5 = ((int64_t)(((int64_t)fg188dd_gg_div(t8, t9))));
  t10 = ((int64_t)(v4));
  t11 = ((int64_t)(((int64_t)UINT64_C(8))));
  v6 = ((int64_t)(((int64_t)fg188dd_gg_mod(t10, t11))));
  t12 = ((int64_t)(((int64_t)UINT64_C(8))));
  t13 = ((int64_t)(v3));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  t15 = ((int64_t)(v6));
  v7 = ((int64_t)(((int64_t)((uint64_t)(t14) + (uint64_t)(t15)))));
  t16 = ((int64_t)(v7));
  t17 = ((int64_t)(v2));
  t18 = ((t16) < (t17));
  if (t18) {
  {
  t19 = ((int64_t)(v7));
  t20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(18))))));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  t21 = ((int64_t)(((int64_t)UINT64_C(256))));
  t22 = ((int64_t)(v3));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) * (uint64_t)(t22)))));
  t24 = ((int64_t)(((int64_t)UINT64_C(32))));
  t25 = ((int64_t)(v6));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t25)))));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t23) + (uint64_t)(t26)))));
  t28 = ((int64_t)(((int64_t)UINT64_C(4))));
  t29 = ((int64_t)(v5));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  v9 = ((int64_t)(((int64_t)((uint64_t)(t27) + (uint64_t)(t30)))));
  t31 = ((uint64_t)(v0));
  t32 = ((uint64_t)(((uint64_t)(v8))));
  t33 = ((uint64_t)(((uint64_t)((uint64_t)(t31) + (uint64_t)(t32)))));
  t34 = fg188d_ld_half(t33);
  v10 = ((float)(t34));
  t35 = ((float)(v10));
  t36 = ((float)(fg188dd_gg_double_bits(UINT64_C(4620693217682128896))));
  t37 = ((float)((-(t36))));
  v11 = ((float)(((t35) * (t37))));
  t38 = ((int64_t)(v8));
  t39 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t38) + (uint64_t)(t39)))));
  t41 = ((int64_t)(((int64_t)UINT64_C(4))));
  t42 = ((int64_t)(v5));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t41) * (uint64_t)(t42)))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t40) + (uint64_t)(t43)))));
  v13 = ((int64_t)(((int64_t)UINT64_C(0))));
label1:;
  t44 = ((int64_t)(v13));
  t45 = ((int64_t)(((int64_t)UINT64_C(4))));
  if (!(((t44) < (t45)))) goto label2;
  {
  t46 = ((uint64_t)(v0));
  t47 = ((int64_t)(v12));
  t48 = ((int64_t)(v13));
  t49 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t47) + (uint64_t)(t48)))))));
  t50 = ((uint64_t)(((uint64_t)((uint64_t)(t46) + (uint64_t)(t49)))));
  t51 = fg188d_ld_u8(t50);
  v14 = ((uint32_t)(t51));
  t52 = ((uint32_t)(v14));
  t53 = ((uint32_t)(((int64_t)UINT64_C(15))));
  v15 = ((uint32_t)(((t52) & (t53))));
  t54 = ((uint32_t)(v14));
  t55 = ((int64_t)(((int64_t)UINT64_C(4))));
  v16 = ((uint32_t)(((t54) >> ((uint64_t)(t55) & 31))));
  t56 = ((int64_t)(v9));
  t57 = ((int64_t)(v13));
  t58 = ((int64_t)(((int64_t)((uint64_t)(t56) + (uint64_t)(t57)))));
  t59 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t58) * (uint64_t)(t59)))));
  t60 = ((int64_t)(v9));
  t61 = ((int64_t)(v13));
  t62 = ((int64_t)(((int64_t)((uint64_t)(t60) + (uint64_t)(t61)))));
  t63 = ((int64_t)(((int64_t)UINT64_C(16))));
  t64 = ((int64_t)(((int64_t)((uint64_t)(t62) + (uint64_t)(t63)))));
  t65 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  v18 = ((int64_t)(((int64_t)((uint64_t)(t64) * (uint64_t)(t65)))));
  t66 = ((uint64_t)(v1));
  t67 = ((uint64_t)(((uint64_t)(v17))));
  t68 = ((uint64_t)(((uint64_t)((uint64_t)(t66) + (uint64_t)(t67)))));
  t69 = ((float)(v10));
  t70 = ((float)(((float)(v15))));
  t71 = ((float)(((t69) * (t70))));
  t72 = ((float)(v11));
  t73 = ((float)(((t71) + (t72))));
  fg188d_st_half(t68, t73);
  t74 = ((uint64_t)(v1));
  t75 = ((uint64_t)(((uint64_t)(v18))));
  t76 = ((uint64_t)(((uint64_t)((uint64_t)(t74) + (uint64_t)(t75)))));
  t77 = ((float)(v10));
  t78 = ((float)(((float)(v16))));
  t79 = ((float)(((t77) * (t78))));
  t80 = ((float)(v11));
  t81 = ((float)(((t79) + (t80))));
  fg188d_st_half(t76, t81);
  t82 = ((int64_t)(v13));
  t83 = ((int64_t)(((int64_t)UINT64_C(1))));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t82) + (uint64_t)(t83)))));
  }
  goto label1;
label2:;
  }
  }
  }
fg188dd_gg_done:;
}
