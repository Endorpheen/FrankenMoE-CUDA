#pragma once
static __device__ __forceinline__ void fg184md_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg184md_gg_index(int64_t i, int64_t len);
static __device__ __forceinline__ int64_t fg184md_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg184md_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg184md_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg184md_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg184md_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ float fg184md_gg_function_7(float p0, int64_t base);
static __device__ __forceinline__ void fg184md_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg184md_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, uint8_t p17, int64_t p18, int64_t p19, int64_t p20, int64_t p21, int64_t p22, int64_t p23, int64_t base);
static __device__ __forceinline__ void fg184md_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg184md_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg184md_gg_fail("array index out of bounds"); return i; }
static __device__ __forceinline__ int64_t fg184md_gg_div(int64_t a, int64_t b) { if (!b) fg184md_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg184md_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg184md_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg184md_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg184md_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg184md_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg184md_gg_umod(uint64_t a,uint64_t b) { if(!b)fg184md_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg184md_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ float fg184md_gg_function_7(float p0, int64_t base) {
  float t0;
  float t1;
  float t2;
  int32_t t3;
  int32_t t4;
  float t5;
  float t6;
  float t7;
  float t8;
  int32_t t9;
  int32_t t10;
  float t11;
  float t12;
  float t13;
  float t14;
  int32_t t15;
  int32_t t16;
  float t17;
  float t18;
  float t19;
  float t20;
  int32_t t21;
  int32_t t22;
  float t23;
  float t24;
  float t25;
  float t26;
  int32_t t27;
  int32_t t28;
  float t29;
  float t30;
  float v0;
  float v1;
  v0 = p0;
  {
  v1 = ((float)(v0));
  t1 = ((float)(v1));
  t2 = ((float)(v1));
  t3 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(16))))));
  t4 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
  t5 = fg184m_shfl_xor(t2, t3, t4);
  t6 = ((float)(t5));
  v1 = ((float)(((t1) + (t6))));
  t7 = ((float)(v1));
  t8 = ((float)(v1));
  t9 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(8))))));
  t10 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
  t11 = fg184m_shfl_xor(t8, t9, t10);
  t12 = ((float)(t11));
  v1 = ((float)(((t7) + (t12))));
  t13 = ((float)(v1));
  t14 = ((float)(v1));
  t15 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(4))))));
  t16 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
  t17 = fg184m_shfl_xor(t14, t15, t16);
  t18 = ((float)(t17));
  v1 = ((float)(((t13) + (t18))));
  t19 = ((float)(v1));
  t20 = ((float)(v1));
  t21 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(2))))));
  t22 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
  t23 = fg184m_shfl_xor(t20, t21, t22);
  t24 = ((float)(t23));
  v1 = ((float)(((t19) + (t24))));
  t25 = ((float)(v1));
  t26 = ((float)(v1));
  t27 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  t28 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
  t29 = fg184m_shfl_xor(t26, t27, t28);
  t30 = ((float)(t29));
  v1 = ((float)(((t25) + (t30))));
  t0 = ((float)(v1));
  goto fg184md_gg_done;
  }
fg184md_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg184md_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  fg184m_fail(t2);
  }
  }
  }
fg184md_gg_done:;
}
static __device__ __forceinline__ void fg184md_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, uint8_t p17, int64_t p18, int64_t p19, int64_t p20, int64_t p21, int64_t p22, int64_t p23, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  uint8_t t7;
  int64_t t8;
  int64_t t9;
  uint8_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
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
  uint8_t t26;
  uint8_t t27;
  int64_t t28;
  int64_t t29;
  uint8_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  uint8_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  uint64_t t41;
  uint64_t t42;
  int32_t t43;
  int64_t t44;
  int64_t t45;
  int64_t t46;
  int64_t t47;
  int64_t t48;
  int64_t t49;
  int64_t t50;
  int64_t t51;
  int64_t t52;
  int64_t t53;
  int64_t t54;
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
  int64_t t67;
  int64_t t68;
  int64_t t69;
  int64_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  uint64_t t76;
  uint64_t t77;
  float t78;
  uint64_t t79;
  int64_t t80;
  int64_t t81;
  uint64_t t82;
  float t83;
  uint64_t t84;
  uint64_t t85;
  float t86;
  uint64_t t87;
  int64_t t88;
  int64_t t89;
  uint64_t t90;
  float t91;
  float t92;
  float t93;
  float t94;
  float t95;
  float t96;
  float t97;
  float t98;
  float t99;
  int64_t t100;
  int64_t t101;
  float t102;
  int64_t t103;
  int64_t t104;
  uint8_t t105;
  int64_t t106;
  int64_t t107;
  uint8_t t108;
  int32_t t109;
  float t110;
  int64_t t111;
  int64_t t112;
  int32_t t113;
  float t114;
  int64_t t115;
  int64_t t116;
  uint8_t t117;
  int32_t t118;
  float t119;
  float t120;
  int64_t t121;
  int64_t t122;
  uint8_t t123;
  int64_t t124;
  int64_t t125;
  int64_t t126;
  int64_t t127;
  int64_t t128;
  int64_t t129;
  int64_t t130;
  int64_t t131;
  int64_t t132;
  int64_t t133;
  uint64_t t134;
  uint64_t t135;
  float t136;
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
  uint8_t v17;
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
  int64_t v28;
  int64_t v29;
  float v30;
  int64_t v31;
  int64_t v32;
  int64_t v33;
  float v34;
  float v35;
  float v36;
  float v37;
  int64_t v38;
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
  v22 = p22;
  v23 = p23;
  {
  t0 = ((int64_t)(v22));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) > (t1));
  t3 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg184md_gg_function_2(t2, t3, base + 0);
  t4 = ((int64_t)(v23));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) >= (t5));
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)(v23));
  t9 = ((int64_t)(v22));
  t7 = ((t8) < (t9));
  }
  t10 = t7;
  t11 = ((int64_t)(((int64_t)UINT64_C(11))));
  fg184md_gg_function_2(t10, t11, base + 0);
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) >= (t13));
  t15 = ((int64_t)(((int64_t)UINT64_C(12))));
  fg184md_gg_function_2(t14, t15, base + 0);
  v24 = ((int64_t)(v20));
  v25 = ((int64_t)(v20));
  v26 = ((int64_t)(v21));
  t16 = v17;
  t17 = (!(t16));
  if (t17) {
  {
  t18 = ((int64_t)(v15));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((t18) > (t19));
  t21 = ((int64_t)(((int64_t)UINT64_C(13))));
  fg184md_gg_function_2(t20, t21, base + 0);
  t22 = ((int64_t)(v20));
  t23 = ((int64_t)(v15));
  v24 = ((int64_t)(((int64_t)fg184md_gg_div(t22, t23))));
  t24 = ((int64_t)(v20));
  t25 = ((int64_t)(v14));
  v25 = ((int64_t)(((int64_t)fg184md_gg_mod(t24, t25))));
  }
  }
  t26 = v17;
  t27 = (!(t26));
  if (t27) {
  {
  t28 = ((int64_t)(v16));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t30 = ((t28) > (t29));
  t31 = ((int64_t)(((int64_t)UINT64_C(14))));
  fg184md_gg_function_2(t30, t31, base + 0);
  t32 = ((int64_t)(v21));
  t33 = ((int64_t)(v16));
  v26 = ((int64_t)(((int64_t)fg184md_gg_div(t32, t33))));
  }
  }
  t34 = v17;
  if (t34) {
  {
  t35 = ((int64_t)(v20));
  t36 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t37 = ((int64_t)(v18));
  t38 = ((int64_t)(((int64_t)((uint64_t)(t36) * (uint64_t)(t37)))));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t35) + (uint64_t)(t38)))));
  t40 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v27 = ((int64_t)(((int64_t)((uint64_t)(t39) * (uint64_t)(t40)))));
  t41 = ((uint64_t)(v3));
  t42 = ((uint64_t)(((uint64_t)(v27))));
  t43 = fg184m_ld_i32(t41, t42);
  v24 = ((int64_t)(((int64_t)(t43))));
  t44 = ((int64_t)(v20));
  t45 = ((int64_t)(v14));
  v25 = ((int64_t)(((int64_t)fg184md_gg_mod(t44, t45))));
  v26 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t46 = ((int64_t)(v26));
  t47 = ((int64_t)(v11));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t46) * (uint64_t)(t47)))));
  t49 = ((int64_t)(v24));
  t50 = ((int64_t)(v8));
  t51 = ((int64_t)(((int64_t)((uint64_t)(t49) * (uint64_t)(t50)))));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t48) + (uint64_t)(t51)))));
  t53 = ((int64_t)(v19));
  t54 = ((int64_t)(v5));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t53) * (uint64_t)(t54)))));
  t56 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t55)))));
  t57 = ((int64_t)(((int64_t)UINT64_C(2))));
  v28 = ((int64_t)(((int64_t)fg184md_gg_div(t56, t57))));
  t58 = ((int64_t)(v21));
  t59 = ((int64_t)(v12));
  t60 = ((int64_t)(((int64_t)((uint64_t)(t58) * (uint64_t)(t59)))));
  t61 = ((int64_t)(v25));
  t62 = ((int64_t)(v9));
  t63 = ((int64_t)(((int64_t)((uint64_t)(t61) * (uint64_t)(t62)))));
  t64 = ((int64_t)(((int64_t)((uint64_t)(t60) + (uint64_t)(t63)))));
  t65 = ((int64_t)(((int64_t)UINT64_C(2))));
  v29 = ((int64_t)(((int64_t)fg184md_gg_div(t64, t65))));
  v30 = ((float)(fg184md_gg_double_bits(UINT64_C(0))));
  v31 = ((int64_t)(v23));
label1:;
  t66 = ((int64_t)(v31));
  t67 = ((int64_t)(v4));
  if (!(((t66) < (t67)))) goto label2;
  {
  t68 = ((int64_t)(v28));
  t69 = ((int64_t)(v31));
  t70 = ((int64_t)(((int64_t)((uint64_t)(t68) + (uint64_t)(t69)))));
  t71 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  v32 = ((int64_t)(((int64_t)((uint64_t)(t70) * (uint64_t)(t71)))));
  t72 = ((int64_t)(v29));
  t73 = ((int64_t)(v31));
  t74 = ((int64_t)(((int64_t)((uint64_t)(t72) + (uint64_t)(t73)))));
  t75 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  v33 = ((int64_t)(((int64_t)((uint64_t)(t74) * (uint64_t)(t75)))));
  t76 = ((uint64_t)(v0));
  t77 = ((uint64_t)(((uint64_t)(v32))));
  t78 = fg184m_ld_f32(t76, t77);
  v34 = ((float)(t78));
  t79 = ((uint64_t)(v0));
  t80 = ((int64_t)(v32));
  t81 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t82 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t80) + (uint64_t)(t81)))))));
  t83 = fg184m_ld_f32(t79, t82);
  v35 = ((float)(t83));
  t84 = ((uint64_t)(v1));
  t85 = ((uint64_t)(((uint64_t)(v33))));
  t86 = fg184m_ld_f32(t84, t85);
  v36 = ((float)(t86));
  t87 = ((uint64_t)(v1));
  t88 = ((int64_t)(v33));
  t89 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t90 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t88) + (uint64_t)(t89)))))));
  t91 = fg184m_ld_f32(t87, t90);
  v37 = ((float)(t91));
  t92 = ((float)(v30));
  t93 = ((float)(v34));
  t94 = ((float)(v36));
  t95 = fg184m_mad(t92, t93, t94);
  v30 = ((float)(t95));
  t96 = ((float)(v30));
  t97 = ((float)(v35));
  t98 = ((float)(v37));
  t99 = fg184m_mad(t96, t97, t98);
  v30 = ((float)(t99));
  t100 = ((int64_t)(v31));
  t101 = ((int64_t)(v22));
  v31 = ((int64_t)(((int64_t)((uint64_t)(t100) + (uint64_t)(t101)))));
  }
  goto label1;
label2:;
  t102 = ((float)(v30));
  v30 = fg184md_gg_function_7(t102, base + 0);
  t103 = ((int64_t)(v22));
  t104 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t105 = ((t103) > (t104));
  if (t105) {
  {
  t106 = ((int64_t)(v23));
  t107 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t108 = ((t106) < (t107));
  if (t108) {
  {
  t109 = ((int32_t)(((int32_t)(v23))));
  t110 = ((float)(fg184md_gg_double_bits(UINT64_C(0))));
  fg184m_shm_put(t109, t110);
  }
  }
  fg184m_barrier();
  t111 = ((int64_t)(v23));
  t112 = ((int64_t)(((int64_t)UINT64_C(32))));
  t113 = ((int32_t)(((int32_t)(((int64_t)fg184md_gg_div(t111, t112))))));
  t114 = ((float)(v30));
  fg184m_shm_put(t113, t114);
  fg184m_barrier();
  t115 = ((int64_t)(v23));
  t116 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t117 = ((t115) < (t116));
  if (t117) {
  {
  t118 = ((int32_t)(((int32_t)(v23))));
  t119 = fg184m_shm_get(t118);
  t120 = ((float)(t119));
  v30 = fg184md_gg_function_7(t120, base + 0);
  }
  }
  }
  }
  t121 = ((int64_t)(v23));
  t122 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t123 = ((t121) < (t122));
  if (t123) {
  {
  t124 = ((int64_t)(v21));
  t125 = ((int64_t)(v13));
  t126 = ((int64_t)(((int64_t)((uint64_t)(t124) * (uint64_t)(t125)))));
  t127 = ((int64_t)(v20));
  t128 = ((int64_t)(v10));
  t129 = ((int64_t)(((int64_t)((uint64_t)(t127) * (uint64_t)(t128)))));
  t130 = ((int64_t)(((int64_t)((uint64_t)(t126) + (uint64_t)(t129)))));
  t131 = ((int64_t)(v19));
  t132 = ((int64_t)(((int64_t)((uint64_t)(t130) + (uint64_t)(t131)))));
  t133 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v38 = ((int64_t)(((int64_t)((uint64_t)(t132) * (uint64_t)(t133)))));
  t134 = ((uint64_t)(v2));
  t135 = ((uint64_t)(((uint64_t)(v38))));
  t136 = ((float)(v30));
  fg184m_st_f32(t134, t135, t136);
  }
  }
  }
fg184md_gg_done:;
}
