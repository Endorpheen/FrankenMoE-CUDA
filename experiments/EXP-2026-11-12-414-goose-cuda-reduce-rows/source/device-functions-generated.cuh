#pragma once
static __device__ __forceinline__ void fg414dd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg414dd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg414dd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg414dd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg414dd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg414dd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ float fg414dd_gg_function_7(float p0, int64_t base);
static __device__ __forceinline__ float fg414dd_gg_function_6(float p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base);
static __device__ __forceinline__ void fg414dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ int64_t fg414dd_gg_function_4(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, uint64_t p7, int64_t base);
static __device__ __forceinline__ int64_t fg414dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, uint64_t p13, int64_t base);
static __device__ __forceinline__ void fg414dd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg414dd_gg_div(int64_t a, int64_t b) { if (!b) fg414dd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg414dd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg414dd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg414dd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg414dd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg414dd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg414dd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg414dd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg414dd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ float fg414dd_gg_function_7(float p0, int64_t base) {
  _Alignas(8) float t0;
  _Alignas(8) float t1;
  _Alignas(8) float t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) float t4;
  _Alignas(8) float t5;
  _Alignas(8) float t6;
  _Alignas(8) float t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) float t9;
  _Alignas(8) float t10;
  _Alignas(8) float t11;
  _Alignas(8) float t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) float t14;
  _Alignas(8) float t15;
  _Alignas(8) float t16;
  _Alignas(8) float t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) float t19;
  _Alignas(8) float t20;
  _Alignas(8) float t21;
  _Alignas(8) float t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) float t24;
  _Alignas(8) float t25;
  _Alignas(8) float v0;
  _Alignas(8) float v1;
  v0 = p0;
  {
  v1 = ((float)(v0));
  t1 = ((float)(v1));
  t2 = ((float)(v1));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
#ifndef __cplusplus
  extern float (fg414d_shfl_xor_f32)(float, int64_t);
#endif
  t4 = fg414d_shfl_xor_f32(t2, t3);
  t5 = ((float)(t4));
  v1 = ((float)(((t1) + (t5))));
  t6 = ((float)(v1));
  t7 = ((float)(v1));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
#ifndef __cplusplus
  extern float (fg414d_shfl_xor_f32)(float, int64_t);
#endif
  t9 = fg414d_shfl_xor_f32(t7, t8);
  t10 = ((float)(t9));
  v1 = ((float)(((t6) + (t10))));
  t11 = ((float)(v1));
  t12 = ((float)(v1));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
#ifndef __cplusplus
  extern float (fg414d_shfl_xor_f32)(float, int64_t);
#endif
  t14 = fg414d_shfl_xor_f32(t12, t13);
  t15 = ((float)(t14));
  v1 = ((float)(((t11) + (t15))));
  t16 = ((float)(v1));
  t17 = ((float)(v1));
  t18 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
#ifndef __cplusplus
  extern float (fg414d_shfl_xor_f32)(float, int64_t);
#endif
  t19 = fg414d_shfl_xor_f32(t17, t18);
  t20 = ((float)(t19));
  v1 = ((float)(((t16) + (t20))));
  t21 = ((float)(v1));
  t22 = ((float)(v1));
  t23 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
#ifndef __cplusplus
  extern float (fg414d_shfl_xor_f32)(float, int64_t);
#endif
  t24 = fg414d_shfl_xor_f32(t22, t23);
  t25 = ((float)(t24));
  v1 = ((float)(((t21) + (t25))));
  t0 = ((float)(v1));
  goto fg414dd_gg_done;
  }
fg414dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg414dd_gg_function_6(float p0, int64_t p1, int64_t p2, uint64_t p3, int64_t base) {
  _Alignas(8) float t0;
  _Alignas(8) float t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) int64_t t3;
  _Alignas(8) uint8_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) int64_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) uint8_t t11;
  _Alignas(8) uint64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) int64_t t14;
  _Alignas(8) uint64_t t15;
  _Alignas(8) float t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) int64_t t20;
  _Alignas(8) uint8_t t21;
  _Alignas(8) uint64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) int64_t t24;
  _Alignas(8) uint64_t t25;
  _Alignas(8) float t26;
  _Alignas(8) float t27;
  _Alignas(8) float v0;
  _Alignas(8) int64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) uint64_t v3;
  _Alignas(8) float v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) float v7;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((float)(v0));
  v4 = fg414dd_gg_function_7(t1, base + 0);
  t2 = ((int64_t)(v2));
  t3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t4 = ((t2) > (t3));
  if (t4) {
  {
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  v5 = ((int64_t)(((int64_t)fg414dd_gg_div(t5, t6))));
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  v6 = ((int64_t)(((int64_t)fg414dd_gg_mod(t7, t8))));
  t9 = ((int64_t)(v6));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t11 = ((t9) == (t10));
  if (t11) {
  {
  t12 = ((uint64_t)(v3));
  t13 = ((int64_t)(v5));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t15 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t13) * (uint64_t)(t14)))))));
  t16 = ((float)(v4));
#ifndef __cplusplus
  extern void (fg414d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg414d_st_f32(t12, t15, t16);
  }
  }
#ifndef __cplusplus
  extern void (fg414d_sync)(void);
#endif
  fg414d_sync();
  v7 = ((float)(fg414dd_gg_double_bits(UINT64_C(0))));
  t17 = ((int64_t)(v6));
  t18 = ((int64_t)(v2));
  t19 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t20 = ((int64_t)(((int64_t)fg414dd_gg_div(t18, t19))));
  t21 = ((t17) < (t20));
  if (t21) {
  {
  t22 = ((uint64_t)(v3));
  t23 = ((int64_t)(v6));
  t24 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t25 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t23) * (uint64_t)(t24)))))));
#ifndef __cplusplus
  extern float (fg414d_ld_f32)(uint64_t, uint64_t);
#endif
  t26 = fg414d_ld_f32(t22, t25);
  v7 = ((float)(t26));
  }
  }
  t27 = ((float)(v7));
  v4 = fg414dd_gg_function_7(t27, base + 0);
  }
  }
  t0 = ((float)(v4));
  goto fg414dd_gg_done;
  }
fg414dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg414dd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  extern void (fg414d_fail)(int64_t);
#endif
  fg414d_fail(t2);
  }
  }
  }
}
static __device__ __forceinline__ int64_t fg414dd_gg_function_4(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, uint64_t p7, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) uint8_t t7;
  _Alignas(8) int64_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) uint8_t t10;
  _Alignas(8) uint64_t t11;
  _Alignas(8) int64_t t12;
  _Alignas(8) int64_t t13;
  _Alignas(8) uint64_t t14;
  _Alignas(8) float t15;
  _Alignas(8) int64_t t16;
  _Alignas(8) int64_t t17;
  _Alignas(8) int64_t t18;
  _Alignas(8) int64_t t19;
  _Alignas(8) uint8_t t20;
  _Alignas(8) uint64_t t21;
  _Alignas(8) int64_t t22;
  _Alignas(8) int64_t t23;
  _Alignas(8) uint64_t t24;
  _Alignas(8) float t25;
  _Alignas(8) int64_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) uint8_t t30;
  _Alignas(8) uint64_t t31;
  _Alignas(8) int64_t t32;
  _Alignas(8) int64_t t33;
  _Alignas(8) uint64_t t34;
  _Alignas(8) float t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) int64_t t37;
  _Alignas(8) int64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) uint8_t t40;
  _Alignas(8) uint64_t t41;
  _Alignas(8) int64_t t42;
  _Alignas(8) int64_t t43;
  _Alignas(8) uint64_t t44;
  _Alignas(8) float t45;
  _Alignas(8) int64_t t46;
  _Alignas(8) int64_t t47;
  _Alignas(8) int64_t t48;
  _Alignas(8) int64_t t49;
  _Alignas(8) uint8_t t50;
  _Alignas(8) uint64_t t51;
  _Alignas(8) int64_t t52;
  _Alignas(8) int64_t t53;
  _Alignas(8) uint64_t t54;
  _Alignas(8) float t55;
  _Alignas(8) int64_t t56;
  _Alignas(8) int64_t t57;
  _Alignas(8) int64_t t58;
  _Alignas(8) int64_t t59;
  _Alignas(8) uint8_t t60;
  _Alignas(8) uint64_t t61;
  _Alignas(8) int64_t t62;
  _Alignas(8) int64_t t63;
  _Alignas(8) uint64_t t64;
  _Alignas(8) float t65;
  _Alignas(8) int64_t t66;
  _Alignas(8) int64_t t67;
  _Alignas(8) int64_t t68;
  _Alignas(8) int64_t t69;
  _Alignas(8) uint8_t t70;
  _Alignas(8) uint64_t t71;
  _Alignas(8) int64_t t72;
  _Alignas(8) int64_t t73;
  _Alignas(8) uint64_t t74;
  _Alignas(8) float t75;
  _Alignas(8) int64_t t76;
  _Alignas(8) int64_t t77;
  _Alignas(8) int64_t t78;
  _Alignas(8) int64_t t79;
  _Alignas(8) uint8_t t80;
  _Alignas(8) uint64_t t81;
  _Alignas(8) int64_t t82;
  _Alignas(8) int64_t t83;
  _Alignas(8) uint64_t t84;
  _Alignas(8) float t85;
  _Alignas(8) int64_t t86;
  _Alignas(8) int64_t t87;
  _Alignas(8) float t88;
  _Alignas(8) float t89;
  _Alignas(8) float t90;
  _Alignas(8) float t91;
  _Alignas(8) float t92;
  _Alignas(8) float t93;
  _Alignas(8) float t94;
  _Alignas(8) float t95;
  _Alignas(8) float t96;
  _Alignas(8) float t97;
  _Alignas(8) float t98;
  _Alignas(8) float t99;
  _Alignas(8) float t100;
  _Alignas(8) float t101;
  _Alignas(8) float t102;
  _Alignas(8) float t103;
  _Alignas(8) float t104;
  _Alignas(8) float t105;
  _Alignas(8) float t106;
  _Alignas(8) float t107;
  _Alignas(8) float t108;
  _Alignas(8) float t109;
  _Alignas(8) float t110;
  _Alignas(8) float t111;
  _Alignas(8) float t112;
  _Alignas(8) float t113;
  _Alignas(8) float t114;
  _Alignas(8) float t115;
  _Alignas(8) float t116;
  _Alignas(8) float t117;
  _Alignas(8) float t118;
  _Alignas(8) int64_t t119;
  _Alignas(8) int64_t t120;
  _Alignas(8) uint64_t t121;
  _Alignas(8) int64_t t122;
  _Alignas(8) int64_t t123;
  _Alignas(8) uint8_t t124;
  _Alignas(8) int64_t t125;
  _Alignas(8) int64_t t126;
  _Alignas(8) uint8_t t127;
  _Alignas(8) uint64_t t128;
  _Alignas(8) int64_t t129;
  _Alignas(8) int64_t t130;
  _Alignas(8) uint64_t t131;
  _Alignas(8) float t132;
  _Alignas(8) float t133;
  _Alignas(8) float t134;
  _Alignas(8) uint64_t t135;
  _Alignas(8) int64_t t136;
  _Alignas(8) int64_t t137;
  _Alignas(8) uint64_t t138;
  _Alignas(8) float t139;
  _Alignas(8) uint64_t v0;
  _Alignas(8) uint64_t v1;
  _Alignas(8) int64_t v2;
  _Alignas(8) int64_t v3;
  _Alignas(8) int64_t v4;
  _Alignas(8) int64_t v5;
  _Alignas(8) int64_t v6;
  _Alignas(8) uint64_t v7;
  _Alignas(8) int64_t v8;
  _Alignas(8) float v9;
  _Alignas(8) float v10;
  _Alignas(8) float v11;
  _Alignas(8) float v12;
  _Alignas(8) float v13;
  _Alignas(8) float v14;
  _Alignas(8) float v15;
  _Alignas(8) float v16;
  _Alignas(8) float v17;
  _Alignas(8) float v18;
  _Alignas(8) float v19;
  _Alignas(8) float v20;
  _Alignas(8) float v21;
  _Alignas(8) float v22;
  _Alignas(8) float v23;
  _Alignas(8) float v24;
  _Alignas(8) float v25;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  v7 = p7;
  {
  t1 = ((int64_t)(v3));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) > (t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(20))));
  fg414dd_gg_function_2(t3, t4, base + 0);
  v8 = ((int64_t)(v5));
  v9 = ((float)(fg414dd_gg_double_bits(UINT64_C(0))));
  v10 = ((float)(fg414dd_gg_double_bits(UINT64_C(0))));
  v11 = ((float)(fg414dd_gg_double_bits(UINT64_C(0))));
  v12 = ((float)(fg414dd_gg_double_bits(UINT64_C(0))));
  v13 = ((float)(fg414dd_gg_double_bits(UINT64_C(0))));
  v14 = ((float)(fg414dd_gg_double_bits(UINT64_C(0))));
  v15 = ((float)(fg414dd_gg_double_bits(UINT64_C(0))));
  v16 = ((float)(fg414dd_gg_double_bits(UINT64_C(0))));
label1:;
  t5 = ((int64_t)(v8));
  t6 = ((int64_t)(v3));
  t7 = ((t5) < (t6));
  if (!(t7)) goto label2;
  {
  v17 = ((float)(fg414dd_gg_double_bits(UINT64_C(0))));
  v18 = ((float)(fg414dd_gg_double_bits(UINT64_C(0))));
  v19 = ((float)(fg414dd_gg_double_bits(UINT64_C(0))));
  v20 = ((float)(fg414dd_gg_double_bits(UINT64_C(0))));
  v21 = ((float)(fg414dd_gg_double_bits(UINT64_C(0))));
  v22 = ((float)(fg414dd_gg_double_bits(UINT64_C(0))));
  v23 = ((float)(fg414dd_gg_double_bits(UINT64_C(0))));
  v24 = ((float)(fg414dd_gg_double_bits(UINT64_C(0))));
  t8 = ((int64_t)(v8));
  t9 = ((int64_t)(v3));
  t10 = ((t8) < (t9));
  if (t10) {
  {
  t11 = ((uint64_t)(v0));
  t12 = ((int64_t)(v8));
  t13 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t14 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))))));
#ifndef __cplusplus
  extern float (fg414d_ld_f32)(uint64_t, uint64_t);
#endif
  t15 = fg414d_ld_f32(t11, t14);
  v17 = ((float)(t15));
  }
  }
  t16 = ((int64_t)(v8));
  t17 = ((int64_t)(v6));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t16) + (uint64_t)(t17)))));
  t18 = ((int64_t)(v8));
  t19 = ((int64_t)(v3));
  t20 = ((t18) < (t19));
  if (t20) {
  {
  t21 = ((uint64_t)(v0));
  t22 = ((int64_t)(v8));
  t23 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t24 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t22) * (uint64_t)(t23)))))));
#ifndef __cplusplus
  extern float (fg414d_ld_f32)(uint64_t, uint64_t);
#endif
  t25 = fg414d_ld_f32(t21, t24);
  v18 = ((float)(t25));
  }
  }
  t26 = ((int64_t)(v8));
  t27 = ((int64_t)(v6));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t26) + (uint64_t)(t27)))));
  t28 = ((int64_t)(v8));
  t29 = ((int64_t)(v3));
  t30 = ((t28) < (t29));
  if (t30) {
  {
  t31 = ((uint64_t)(v0));
  t32 = ((int64_t)(v8));
  t33 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t34 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t32) * (uint64_t)(t33)))))));
#ifndef __cplusplus
  extern float (fg414d_ld_f32)(uint64_t, uint64_t);
#endif
  t35 = fg414d_ld_f32(t31, t34);
  v19 = ((float)(t35));
  }
  }
  t36 = ((int64_t)(v8));
  t37 = ((int64_t)(v6));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t36) + (uint64_t)(t37)))));
  t38 = ((int64_t)(v8));
  t39 = ((int64_t)(v3));
  t40 = ((t38) < (t39));
  if (t40) {
  {
  t41 = ((uint64_t)(v0));
  t42 = ((int64_t)(v8));
  t43 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t44 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t42) * (uint64_t)(t43)))))));
#ifndef __cplusplus
  extern float (fg414d_ld_f32)(uint64_t, uint64_t);
#endif
  t45 = fg414d_ld_f32(t41, t44);
  v20 = ((float)(t45));
  }
  }
  t46 = ((int64_t)(v8));
  t47 = ((int64_t)(v6));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t46) + (uint64_t)(t47)))));
  t48 = ((int64_t)(v8));
  t49 = ((int64_t)(v3));
  t50 = ((t48) < (t49));
  if (t50) {
  {
  t51 = ((uint64_t)(v0));
  t52 = ((int64_t)(v8));
  t53 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t54 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t52) * (uint64_t)(t53)))))));
#ifndef __cplusplus
  extern float (fg414d_ld_f32)(uint64_t, uint64_t);
#endif
  t55 = fg414d_ld_f32(t51, t54);
  v21 = ((float)(t55));
  }
  }
  t56 = ((int64_t)(v8));
  t57 = ((int64_t)(v6));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t56) + (uint64_t)(t57)))));
  t58 = ((int64_t)(v8));
  t59 = ((int64_t)(v3));
  t60 = ((t58) < (t59));
  if (t60) {
  {
  t61 = ((uint64_t)(v0));
  t62 = ((int64_t)(v8));
  t63 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t64 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t62) * (uint64_t)(t63)))))));
#ifndef __cplusplus
  extern float (fg414d_ld_f32)(uint64_t, uint64_t);
#endif
  t65 = fg414d_ld_f32(t61, t64);
  v22 = ((float)(t65));
  }
  }
  t66 = ((int64_t)(v8));
  t67 = ((int64_t)(v6));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t66) + (uint64_t)(t67)))));
  t68 = ((int64_t)(v8));
  t69 = ((int64_t)(v3));
  t70 = ((t68) < (t69));
  if (t70) {
  {
  t71 = ((uint64_t)(v0));
  t72 = ((int64_t)(v8));
  t73 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t74 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t72) * (uint64_t)(t73)))))));
#ifndef __cplusplus
  extern float (fg414d_ld_f32)(uint64_t, uint64_t);
#endif
  t75 = fg414d_ld_f32(t71, t74);
  v23 = ((float)(t75));
  }
  }
  t76 = ((int64_t)(v8));
  t77 = ((int64_t)(v6));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t76) + (uint64_t)(t77)))));
  t78 = ((int64_t)(v8));
  t79 = ((int64_t)(v3));
  t80 = ((t78) < (t79));
  if (t80) {
  {
  t81 = ((uint64_t)(v0));
  t82 = ((int64_t)(v8));
  t83 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t84 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t82) * (uint64_t)(t83)))))));
#ifndef __cplusplus
  extern float (fg414d_ld_f32)(uint64_t, uint64_t);
#endif
  t85 = fg414d_ld_f32(t81, t84);
  v24 = ((float)(t85));
  }
  }
  t86 = ((int64_t)(v8));
  t87 = ((int64_t)(v6));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t86) + (uint64_t)(t87)))));
  t88 = ((float)(v9));
  t89 = ((float)(v17));
  v9 = ((float)(((t88) + (t89))));
  t90 = ((float)(v10));
  t91 = ((float)(v18));
  v10 = ((float)(((t90) + (t91))));
  t92 = ((float)(v11));
  t93 = ((float)(v19));
  v11 = ((float)(((t92) + (t93))));
  t94 = ((float)(v12));
  t95 = ((float)(v20));
  v12 = ((float)(((t94) + (t95))));
  t96 = ((float)(v13));
  t97 = ((float)(v21));
  v13 = ((float)(((t96) + (t97))));
  t98 = ((float)(v14));
  t99 = ((float)(v22));
  v14 = ((float)(((t98) + (t99))));
  t100 = ((float)(v15));
  t101 = ((float)(v23));
  v15 = ((float)(((t100) + (t101))));
  t102 = ((float)(v16));
  t103 = ((float)(v24));
  v16 = ((float)(((t102) + (t103))));
  }
  goto label1;
label2:;
  t104 = ((float)(v9));
  t105 = ((float)(v10));
  v25 = ((float)(((t104) + (t105))));
  t106 = ((float)(v25));
  t107 = ((float)(v11));
  v25 = ((float)(((t106) + (t107))));
  t108 = ((float)(v25));
  t109 = ((float)(v12));
  v25 = ((float)(((t108) + (t109))));
  t110 = ((float)(v25));
  t111 = ((float)(v13));
  v25 = ((float)(((t110) + (t111))));
  t112 = ((float)(v25));
  t113 = ((float)(v14));
  v25 = ((float)(((t112) + (t113))));
  t114 = ((float)(v25));
  t115 = ((float)(v15));
  v25 = ((float)(((t114) + (t115))));
  t116 = ((float)(v25));
  t117 = ((float)(v16));
  v25 = ((float)(((t116) + (t117))));
  t118 = ((float)(v25));
  t119 = ((int64_t)(v5));
  t120 = ((int64_t)(v6));
  t121 = ((uint64_t)(v7));
  v25 = fg414dd_gg_function_6(t118, t119, t120, t121, base + 0);
  t122 = ((int64_t)(v5));
  t123 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t124 = ((t122) == (t123));
  if (t124) {
  {
  t125 = ((int64_t)(v4));
  t126 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t127 = ((t125) != (t126));
  if (t127) {
  {
  t128 = ((uint64_t)(v1));
  t129 = ((int64_t)(v2));
  t130 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t131 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t129) * (uint64_t)(t130)))))));
  t132 = ((float)(v25));
  t133 = ((float)(((float)(v3))));
  t134 = ((float)(((t132) / (t133))));
#ifndef __cplusplus
  extern void (fg414d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg414d_st_f32(t128, t131, t134);
  }
  } else {
  {
  t135 = ((uint64_t)(v1));
  t136 = ((int64_t)(v2));
  t137 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t138 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t136) * (uint64_t)(t137)))))));
  t139 = ((float)(v25));
#ifndef __cplusplus
  extern void (fg414d_st_f32)(uint64_t, uint64_t, float);
#endif
  fg414d_st_f32(t135, t138, t139);
  }
  }
  }
  }
  t0 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  goto fg414dd_gg_done;
  }
fg414dd_gg_done:;
  return t0;
}
static __device__ __forceinline__ int64_t fg414dd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, uint64_t p13, int64_t base) {
  _Alignas(8) int64_t t0;
  _Alignas(8) int64_t t1;
  _Alignas(8) int64_t t2;
  _Alignas(8) uint8_t t3;
  _Alignas(8) int64_t t4;
  _Alignas(8) int64_t t5;
  _Alignas(8) int64_t t6;
  _Alignas(8) uint8_t t7;
  _Alignas(8) uint8_t t8;
  _Alignas(8) int64_t t9;
  _Alignas(8) int64_t t10;
  _Alignas(8) uint8_t t11;
  _Alignas(8) int64_t t12;
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
  _Alignas(8) uint64_t t26;
  _Alignas(8) int64_t t27;
  _Alignas(8) int64_t t28;
  _Alignas(8) int64_t t29;
  _Alignas(8) int64_t t30;
  _Alignas(8) int64_t t31;
  _Alignas(8) int64_t t32;
  _Alignas(8) int64_t t33;
  _Alignas(8) int64_t t34;
  _Alignas(8) int64_t t35;
  _Alignas(8) int64_t t36;
  _Alignas(8) uint64_t t37;
  _Alignas(8) uint64_t t38;
  _Alignas(8) int64_t t39;
  _Alignas(8) int64_t t40;
  _Alignas(8) int64_t t41;
  _Alignas(8) int64_t t42;
  _Alignas(8) uint64_t t43;
  _Alignas(8) uint64_t t44;
  _Alignas(8) uint64_t t45;
  _Alignas(8) int64_t t46;
  _Alignas(8) int64_t t47;
  _Alignas(8) int64_t t48;
  _Alignas(8) int64_t t49;
  _Alignas(8) int64_t t50;
  _Alignas(8) uint64_t t51;
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
  _Alignas(8) uint64_t v13;
  _Alignas(8) uint64_t v14;
  _Alignas(8) int64_t v15;
  _Alignas(8) int64_t v16;
  _Alignas(8) int64_t v17;
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
  t1 = ((int64_t)(v3));
  t2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t3 = ((t1) > (t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(21))));
  fg414dd_gg_function_2(t3, t4, base + 0);
  t5 = ((int64_t)(v6));
  t6 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t7 = ((t5) > (t6));
  t8 = t7;
  if (t8) {
  t9 = ((int64_t)(v7));
  t10 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t8 = ((t9) > (t10));
  }
  t11 = t8;
  t12 = ((int64_t)(((int64_t)UINT64_C(22))));
  fg414dd_gg_function_2(t11, t12, base + 0);
  v14 = ((uint64_t)(v0));
  t13 = ((int64_t)(v5));
  t14 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t15 = ((t13) != (t14));
  if (t15) {
  {
  t16 = ((int64_t)(v2));
  t17 = ((int64_t)(v6));
  v15 = ((int64_t)(((int64_t)fg414dd_gg_mod(t16, t17))));
  t18 = ((int64_t)(v2));
  t19 = ((int64_t)(v6));
  t20 = ((int64_t)(((int64_t)fg414dd_gg_div(t18, t19))));
  t21 = ((int64_t)(v7));
  v16 = ((int64_t)(((int64_t)fg414dd_gg_mod(t20, t21))));
  t22 = ((int64_t)(v2));
  t23 = ((int64_t)(v6));
  t24 = ((int64_t)(v7));
  t25 = ((int64_t)(((int64_t)((uint64_t)(t23) * (uint64_t)(t24)))));
  v17 = ((int64_t)(((int64_t)fg414dd_gg_div(t22, t25))));
  t26 = ((uint64_t)(v0));
  t27 = ((int64_t)(v17));
  t28 = ((int64_t)(v10));
  t29 = ((int64_t)(((int64_t)((uint64_t)(t27) * (uint64_t)(t28)))));
  t30 = ((int64_t)(v16));
  t31 = ((int64_t)(v9));
  t32 = ((int64_t)(((int64_t)((uint64_t)(t30) * (uint64_t)(t31)))));
  t33 = ((int64_t)(((int64_t)((uint64_t)(t29) + (uint64_t)(t32)))));
  t34 = ((int64_t)(v15));
  t35 = ((int64_t)(v8));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t34) * (uint64_t)(t35)))));
  t37 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t33) + (uint64_t)(t36)))))));
  v14 = ((uint64_t)(((uint64_t)((uint64_t)(t26) + (uint64_t)(t37)))));
  }
  } else {
  {
  t38 = ((uint64_t)(v0));
  t39 = ((int64_t)(v2));
  t40 = ((int64_t)(v3));
  t41 = ((int64_t)(((int64_t)((uint64_t)(t39) * (uint64_t)(t40)))));
  t42 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t43 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t41) * (uint64_t)(t42)))))));
  v14 = ((uint64_t)(((uint64_t)((uint64_t)(t38) + (uint64_t)(t43)))));
  }
  }
  t44 = ((uint64_t)(v14));
  t45 = ((uint64_t)(v1));
  t46 = ((int64_t)(v2));
  t47 = ((int64_t)(v3));
  t48 = ((int64_t)(v4));
  t49 = ((int64_t)(v11));
  t50 = ((int64_t)(v12));
  t51 = ((uint64_t)(v13));
  t0 = fg414dd_gg_function_4(t44, t45, t46, t47, t48, t49, t50, t51, base + 0);
  goto fg414dd_gg_done;
  }
fg414dd_gg_done:;
  return t0;
}
