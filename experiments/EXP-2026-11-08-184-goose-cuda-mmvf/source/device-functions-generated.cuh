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
static __device__ __forceinline__ void fg184md_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, uint8_t p16, int64_t p17, int64_t p18, int64_t p19, int64_t p20, int64_t p21, int64_t p22, int64_t base);
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
static __device__ __forceinline__ void fg184md_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, uint8_t p16, int64_t p17, int64_t p18, int64_t p19, int64_t p20, int64_t p21, int64_t p22, int64_t base) {
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
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  uint64_t t33;
  uint64_t t34;
  int32_t t35;
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
  uint64_t t68;
  uint64_t t69;
  float t70;
  uint64_t t71;
  int64_t t72;
  int64_t t73;
  uint64_t t74;
  float t75;
  uint64_t t76;
  uint64_t t77;
  float t78;
  uint64_t t79;
  int64_t t80;
  int64_t t81;
  uint64_t t82;
  float t83;
  float t84;
  float t85;
  float t86;
  float t87;
  float t88;
  float t89;
  float t90;
  float t91;
  int64_t t92;
  int64_t t93;
  float t94;
  int64_t t95;
  int64_t t96;
  uint8_t t97;
  int64_t t98;
  int64_t t99;
  uint8_t t100;
  int32_t t101;
  float t102;
  int64_t t103;
  int64_t t104;
  int32_t t105;
  float t106;
  int64_t t107;
  int64_t t108;
  uint8_t t109;
  int32_t t110;
  float t111;
  float t112;
  int64_t t113;
  int64_t t114;
  uint8_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  int64_t t119;
  int64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  int64_t t125;
  uint64_t t126;
  uint64_t t127;
  float t128;
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
  uint8_t v16;
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
  int64_t v28;
  float v29;
  int64_t v30;
  int64_t v31;
  int64_t v32;
  float v33;
  float v34;
  float v35;
  float v36;
  int64_t v37;
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
  {
  t0 = ((int64_t)(v21));
  t1 = ((int64_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) > (t1));
  t3 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg184md_gg_function_2(t2, t3, base + 0);
  t4 = ((int64_t)(v22));
  t5 = ((int64_t)(((int64_t)UINT64_C(0))));
  t6 = ((t4) >= (t5));
  t7 = t6;
  if (t7) {
  t8 = ((int64_t)(v22));
  t9 = ((int64_t)(v21));
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
  v23 = ((int64_t)(v19));
  v24 = ((int64_t)(v19));
  v25 = ((int64_t)(v20));
  t16 = v16;
  t17 = (!(t16));
  if (t17) {
  {
  t18 = ((int64_t)(v15));
  t19 = ((int64_t)(((int64_t)UINT64_C(0))));
  t20 = ((t18) > (t19));
  t21 = ((int64_t)(((int64_t)UINT64_C(13))));
  fg184md_gg_function_2(t20, t21, base + 0);
  t22 = ((int64_t)(v19));
  t23 = ((int64_t)(v15));
  v23 = ((int64_t)(((int64_t)fg184md_gg_div(t22, t23))));
  t24 = ((int64_t)(v19));
  t25 = ((int64_t)(v14));
  v24 = ((int64_t)(((int64_t)fg184md_gg_mod(t24, t25))));
  }
  }
  t26 = v16;
  if (t26) {
  {
  t27 = ((int64_t)(v19));
  t28 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t29 = ((int64_t)(v17));
  t30 = ((int64_t)(((int64_t)((uint64_t)(t28) * (uint64_t)(t29)))));
  t31 = ((int64_t)(((int64_t)((uint64_t)(t27) + (uint64_t)(t30)))));
  t32 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v26 = ((int64_t)(((int64_t)((uint64_t)(t31) * (uint64_t)(t32)))));
  t33 = ((uint64_t)(v3));
  t34 = ((uint64_t)(((uint64_t)(v26))));
  t35 = fg184m_ld_i32(t33, t34);
  v23 = ((int64_t)(((int64_t)(t35))));
  t36 = ((int64_t)(v19));
  t37 = ((int64_t)(v14));
  v24 = ((int64_t)(((int64_t)fg184md_gg_mod(t36, t37))));
  v25 = ((int64_t)(((int64_t)UINT64_C(0))));
  }
  }
  t38 = ((int64_t)(v25));
  t39 = ((int64_t)(v11));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t38) * (uint64_t)(t39)))));
  t41 = ((int64_t)(v23));
  t42 = ((int64_t)(v8));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t41) * (uint64_t)(t42)))));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t40) + (uint64_t)(t43)))));
  t45 = ((int64_t)(v18));
  t46 = ((int64_t)(v5));
  t47 = ((int64_t)(((int64_t)((uint64_t)(t45) * (uint64_t)(t46)))));
  t48 = ((int64_t)(((int64_t)((uint64_t)(t44) + (uint64_t)(t47)))));
  t49 = ((int64_t)(((int64_t)UINT64_C(2))));
  v27 = ((int64_t)(((int64_t)fg184md_gg_div(t48, t49))));
  t50 = ((int64_t)(v20));
  t51 = ((int64_t)(v12));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t50) * (uint64_t)(t51)))));
  t53 = ((int64_t)(v24));
  t54 = ((int64_t)(v9));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t53) * (uint64_t)(t54)))));
  t56 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t55)))));
  t57 = ((int64_t)(((int64_t)UINT64_C(2))));
  v28 = ((int64_t)(((int64_t)fg184md_gg_div(t56, t57))));
  v29 = ((float)(fg184md_gg_double_bits(UINT64_C(0))));
  v30 = ((int64_t)(v22));
label1:;
  t58 = ((int64_t)(v30));
  t59 = ((int64_t)(v4));
  if (!(((t58) < (t59)))) goto label2;
  {
  t60 = ((int64_t)(v27));
  t61 = ((int64_t)(v30));
  t62 = ((int64_t)(((int64_t)((uint64_t)(t60) + (uint64_t)(t61)))));
  t63 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  v31 = ((int64_t)(((int64_t)((uint64_t)(t62) * (uint64_t)(t63)))));
  t64 = ((int64_t)(v28));
  t65 = ((int64_t)(v30));
  t66 = ((int64_t)(((int64_t)((uint64_t)(t64) + (uint64_t)(t65)))));
  t67 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(8))))));
  v32 = ((int64_t)(((int64_t)((uint64_t)(t66) * (uint64_t)(t67)))));
  t68 = ((uint64_t)(v0));
  t69 = ((uint64_t)(((uint64_t)(v31))));
  t70 = fg184m_ld_f32(t68, t69);
  v33 = ((float)(t70));
  t71 = ((uint64_t)(v0));
  t72 = ((int64_t)(v31));
  t73 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t74 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t72) + (uint64_t)(t73)))))));
  t75 = fg184m_ld_f32(t71, t74);
  v34 = ((float)(t75));
  t76 = ((uint64_t)(v1));
  t77 = ((uint64_t)(((uint64_t)(v32))));
  t78 = fg184m_ld_f32(t76, t77);
  v35 = ((float)(t78));
  t79 = ((uint64_t)(v1));
  t80 = ((int64_t)(v32));
  t81 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t82 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t80) + (uint64_t)(t81)))))));
  t83 = fg184m_ld_f32(t79, t82);
  v36 = ((float)(t83));
  t84 = ((float)(v29));
  t85 = ((float)(v33));
  t86 = ((float)(v35));
  t87 = fg184m_mad(t84, t85, t86);
  v29 = ((float)(t87));
  t88 = ((float)(v29));
  t89 = ((float)(v34));
  t90 = ((float)(v36));
  t91 = fg184m_mad(t88, t89, t90);
  v29 = ((float)(t91));
  t92 = ((int64_t)(v30));
  t93 = ((int64_t)(v21));
  v30 = ((int64_t)(((int64_t)((uint64_t)(t92) + (uint64_t)(t93)))));
  }
  goto label1;
label2:;
  t94 = ((float)(v29));
  v29 = fg184md_gg_function_7(t94, base + 0);
  t95 = ((int64_t)(v21));
  t96 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t97 = ((t95) > (t96));
  if (t97) {
  {
  t98 = ((int64_t)(v22));
  t99 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t100 = ((t98) < (t99));
  if (t100) {
  {
  t101 = ((int32_t)(((int32_t)(v22))));
  t102 = ((float)(fg184md_gg_double_bits(UINT64_C(0))));
  fg184m_shm_put(t101, t102);
  }
  }
  fg184m_barrier();
  t103 = ((int64_t)(v22));
  t104 = ((int64_t)(((int64_t)UINT64_C(32))));
  t105 = ((int32_t)(((int32_t)(((int64_t)fg184md_gg_div(t103, t104))))));
  t106 = ((float)(v29));
  fg184m_shm_put(t105, t106);
  fg184m_barrier();
  t107 = ((int64_t)(v22));
  t108 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t109 = ((t107) < (t108));
  if (t109) {
  {
  t110 = ((int32_t)(((int32_t)(v22))));
  t111 = fg184m_shm_get(t110);
  t112 = ((float)(t111));
  v29 = fg184md_gg_function_7(t112, base + 0);
  }
  }
  }
  }
  t113 = ((int64_t)(v22));
  t114 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(1))))));
  t115 = ((t113) < (t114));
  if (t115) {
  {
  t116 = ((int64_t)(v20));
  t117 = ((int64_t)(v13));
  t118 = ((int64_t)(((int64_t)((uint64_t)(t116) * (uint64_t)(t117)))));
  t119 = ((int64_t)(v19));
  t120 = ((int64_t)(v10));
  t121 = ((int64_t)(((int64_t)((uint64_t)(t119) * (uint64_t)(t120)))));
  t122 = ((int64_t)(((int64_t)((uint64_t)(t118) + (uint64_t)(t121)))));
  t123 = ((int64_t)(v18));
  t124 = ((int64_t)(((int64_t)((uint64_t)(t122) + (uint64_t)(t123)))));
  t125 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v37 = ((int64_t)(((int64_t)((uint64_t)(t124) * (uint64_t)(t125)))));
  t126 = ((uint64_t)(v2));
  t127 = ((uint64_t)(((uint64_t)(v37))));
  t128 = ((float)(v29));
  fg184m_st_f32(t126, t127, t128);
  }
  }
  }
fg184md_gg_done:;
}
