#pragma once
static __device__ __forceinline__ void fg179wd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg179wd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg179wd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg179wd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg179wd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg179wd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ void fg179wd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg179wd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t p4, int64_t p5, int32_t p6, int32_t p7, int32_t p8, int32_t p9, int64_t base);
static __device__ __forceinline__ void fg179wd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg179wd_gg_div(int64_t a, int64_t b) { if (!b) fg179wd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg179wd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg179wd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg179wd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg179wd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg179wd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg179wd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg179wd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg179wd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ void fg179wd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  fg179w_fail(t2);
  }
  }
  }
fg179wd_gg_done:;
}
static __device__ __forceinline__ void fg179wd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t p4, int64_t p5, int32_t p6, int32_t p7, int32_t p8, int32_t p9, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  uint8_t t7;
  int32_t t8;
  int32_t t9;
  uint8_t t10;
  uint8_t t11;
  int32_t t12;
  int32_t t13;
  uint8_t t14;
  uint8_t t15;
  int32_t t16;
  int32_t t17;
  uint8_t t18;
  uint8_t t19;
  int32_t t20;
  int32_t t21;
  uint8_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  uint8_t t30;
  int64_t t31;
  int64_t t32;
  uint64_t t33;
  uint64_t t34;
  uint8_t t35;
  uint64_t t36;
  int64_t t37;
  int64_t t38;
  uint64_t t39;
  float t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  uint64_t t45;
  int64_t t46;
  int64_t t47;
  uint64_t t48;
  float t49;
  float t50;
  float t51;
  float t52;
  uint64_t t53;
  int64_t t54;
  int64_t t55;
  uint64_t t56;
  float t57;
  float t58;
  int64_t t59;
  int64_t t60;
  int64_t t61;
  int64_t t62;
  uint64_t t63;
  uint64_t t64;
  uint8_t t65;
  uint64_t t66;
  int64_t t67;
  int64_t t68;
  uint64_t t69;
  float t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  float t75;
  uint64_t t76;
  int64_t t77;
  int64_t t78;
  uint64_t t79;
  float t80;
  float t81;
  float t82;
  float t83;
  uint64_t t84;
  int64_t t85;
  int64_t t86;
  uint64_t t87;
  float t88;
  float t89;
  float t90;
  int64_t t91;
  int64_t t92;
  uint64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  uint64_t t100;
  float t101;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  int64_t v4;
  int64_t v5;
  int32_t v6;
  int32_t v7;
  int32_t v8;
  int32_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  float v13;
  int64_t v14;
  float v15;
  int64_t v16;
  int64_t v17;
  float v18;
  int64_t v19;
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
  {
  t0 = ((int64_t)(v4));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) > (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int64_t)(v5));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) > (t5));
  }
  t6 = t3;
  t7 = t6;
  if (t7) {
  t8 = ((int32_t)(v9));
  t9 = ((int32_t)(((int64_t)UINT64_C(0))));
  t7 = ((t8) > (t9));
  }
  t10 = t7;
  t11 = t10;
  if (t11) {
  t12 = ((int32_t)(v6));
  t13 = ((int32_t)(((int64_t)UINT64_C(0))));
  t11 = ((t12) >= (t13));
  }
  t14 = t11;
  t15 = t14;
  if (t15) {
  t16 = ((int32_t)(v7));
  t17 = ((int32_t)(((int64_t)UINT64_C(0))));
  t15 = ((t16) >= (t17));
  }
  t18 = t15;
  t19 = t18;
  if (t19) {
  t20 = ((int32_t)(v8));
  t21 = ((int32_t)(((int64_t)UINT64_C(0))));
  t19 = ((t20) >= (t21));
  }
  t22 = t19;
  t23 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg179wd_gg_function_2(t22, t23, base + 0);
  t24 = ((int64_t)(((int64_t)(v7))));
  t25 = ((int64_t)(((int64_t)(v9))));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t25)))));
  t27 = ((int64_t)(((int64_t)(v8))));
  v10 = ((int64_t)(((int64_t)((uint64_t)(t26) + (uint64_t)(t27)))));
  t28 = ((int64_t)(v10));
  t29 = ((int64_t)(v4));
  t30 = ((t28) < (t29));
  if (t30) {
  {
  v11 = ((int64_t)(((int64_t)(v6))));
  t31 = ((int64_t)(v11));
  t32 = ((int64_t)(v5));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t31) * (uint64_t)(t32)))));
  v13 = ((float)(fg179wd_gg_double_bits(UINT64_C(4607182418800017408))));
  t33 = ((uint64_t)(v1));
  t34 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t35 = ((t33) != (t34));
  if (t35) {
  {
  t36 = ((uint64_t)(v1));
  t37 = ((int64_t)(v12));
  t38 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t39 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t37) * (uint64_t)(t38)))))));
  t40 = fg179w_ld_f32(t36, t39);
  v13 = ((float)(t40));
  }
  }
  t41 = ((int64_t)(v12));
  t42 = ((int64_t)(v4));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t41) * (uint64_t)(t42)))));
  t44 = ((int64_t)(v10));
  v14 = ((int64_t)(((int64_t)((uint64_t)(t43) + (uint64_t)(t44)))));
  t45 = ((uint64_t)(v0));
  t46 = ((int64_t)(v14));
  t47 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t48 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t46) * (uint64_t)(t47)))))));
  t49 = fg179w_ld_f32(t45, t48);
  t50 = ((float)(t49));
  t51 = ((float)(v13));
  t52 = ((float)(((t50) * (t51))));
  t53 = ((uint64_t)(v2));
  t54 = ((int64_t)(v12));
  t55 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t56 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t54) * (uint64_t)(t55)))))));
  t57 = fg179w_ld_f32(t53, t56);
  t58 = ((float)(t57));
  v15 = ((float)(((t52) * (t58))));
  v16 = ((int64_t)(((int64_t)UINT64_C(1))));
label1:;
  t59 = ((int64_t)(v16));
  t60 = ((int64_t)(v5));
  if (!(((t59) < (t60)))) goto label2;
  {
  t61 = ((int64_t)(v12));
  t62 = ((int64_t)(v16));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t61) + (uint64_t)(t62)))));
  v18 = ((float)(fg179wd_gg_double_bits(UINT64_C(4607182418800017408))));
  t63 = ((uint64_t)(v1));
  t64 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t65 = ((t63) != (t64));
  if (t65) {
  {
  t66 = ((uint64_t)(v1));
  t67 = ((int64_t)(v17));
  t68 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t69 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t67) * (uint64_t)(t68)))))));
  t70 = fg179w_ld_f32(t66, t69);
  v18 = ((float)(t70));
  }
  }
  t71 = ((int64_t)(v17));
  t72 = ((int64_t)(v4));
  t73 = ((int64_t)(((int64_t)((uint64_t)(t71) * (uint64_t)(t72)))));
  t74 = ((int64_t)(v10));
  v19 = ((int64_t)(((int64_t)((uint64_t)(t73) + (uint64_t)(t74)))));
  t75 = ((float)(v15));
  t76 = ((uint64_t)(v0));
  t77 = ((int64_t)(v19));
  t78 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t79 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t77) * (uint64_t)(t78)))))));
  t80 = fg179w_ld_f32(t76, t79);
  t81 = ((float)(t80));
  t82 = ((float)(v18));
  t83 = ((float)(((t81) * (t82))));
  t84 = ((uint64_t)(v2));
  t85 = ((int64_t)(v17));
  t86 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t87 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t85) * (uint64_t)(t86)))))));
  t88 = fg179w_ld_f32(t84, t87);
  t89 = ((float)(t88));
  t90 = ((float)(((t83) * (t89))));
  v15 = ((float)(((t75) + (t90))));
  t91 = v16;
  t92 = ((int64_t)(((int64_t)UINT64_C(1))));
  v16 = ((int64_t)((uint64_t)(t91) + (uint64_t)(t92)));
  }
  goto label1;
label2:;
  t93 = ((uint64_t)(v3));
  t94 = ((int64_t)(v11));
  t95 = ((int64_t)(v4));
  t96 = ((int64_t)(((int64_t)((uint64_t)(t94) * (uint64_t)(t95)))));
  t97 = ((int64_t)(v10));
  t98 = ((int64_t)(((int64_t)((uint64_t)(t96) + (uint64_t)(t97)))));
  t99 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t100 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t98) * (uint64_t)(t99)))))));
  t101 = ((float)(v15));
  fg179w_st_f32(t93, t100, t101);
  }
  }
  }
fg179wd_gg_done:;
}
