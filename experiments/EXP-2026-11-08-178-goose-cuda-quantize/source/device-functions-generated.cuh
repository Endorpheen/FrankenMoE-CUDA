#pragma once
static __device__ __forceinline__ void fg178qd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg178qd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg178qd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg178qd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg178qd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg178qd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ float fg178qd_gg_function_10(float p0, int64_t base);
static __device__ __forceinline__ float fg178qd_gg_function_8(float p0, int64_t base);
static __device__ __forceinline__ void fg178qd_gg_function_3(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg178qd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, fg178qd_gg_type_3 p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int64_t base);
static __device__ __forceinline__ void fg178qd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg178qd_gg_div(int64_t a, int64_t b) { if (!b) fg178qd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg178qd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg178qd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg178qd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg178qd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg178qd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg178qd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg178qd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg178qd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ float fg178qd_gg_function_10(float p0, int64_t base) {
  float t0;
  int32_t t1;
  int32_t t2;
  float t3;
  float t4;
  uint32_t t5;
  int32_t t6;
  float t7;
  float t8;
  int32_t t9;
  int32_t t10;
  float v0;
  float v1;
  int32_t v2;
  v0 = p0;
  {
  v1 = ((float)(v0));
  v2 = ((int32_t)(((int64_t)UINT64_C(16))));
label1:;
  t1 = ((int32_t)(v2));
  t2 = ((int32_t)(((int64_t)UINT64_C(0))));
  if (!(((t1) > (t2)))) goto label2;
  {
  t3 = ((float)(v1));
  t4 = ((float)(v1));
  t5 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t6 = ((int32_t)(v2));
  t7 = fg178q_shfl_xor(t4, t5, t6);
  t8 = ((float)(t7));
  v1 = ((float)(((t3) + (t8))));
  t9 = ((int32_t)(v2));
  t10 = ((int32_t)(((int64_t)UINT64_C(2))));
  v2 = ((int32_t)(((int32_t)fg178qd_gg_div(t9, t10))));
  }
  goto label1;
label2:;
  t0 = ((float)(v1));
  goto fg178qd_gg_done;
  }
fg178qd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg178qd_gg_function_8(float p0, int64_t base) {
  float t0;
  int32_t t1;
  int32_t t2;
  float t3;
  uint32_t t4;
  int32_t t5;
  float t6;
  float t7;
  float t8;
  uint8_t t9;
  int32_t t10;
  int32_t t11;
  float v0;
  float v1;
  int32_t v2;
  float v3;
  v0 = p0;
  {
  v1 = ((float)(v0));
  v2 = ((int32_t)(((int64_t)UINT64_C(16))));
label1:;
  t1 = ((int32_t)(v2));
  t2 = ((int32_t)(((int64_t)UINT64_C(0))));
  if (!(((t1) > (t2)))) goto label2;
  {
  t3 = ((float)(v1));
  t4 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t5 = ((int32_t)(v2));
  t6 = fg178q_shfl_xor(t3, t4, t5);
  v3 = ((float)(t6));
  t7 = ((float)(v1));
  t8 = ((float)(v3));
  t9 = ((t7) < (t8));
  if (t9) {
  {
  v1 = ((float)(v3));
  }
  }
  t10 = ((int32_t)(v2));
  t11 = ((int32_t)(((int64_t)UINT64_C(2))));
  v2 = ((int32_t)(((int32_t)fg178qd_gg_div(t10, t11))));
  }
  goto label1;
label2:;
  t0 = ((float)(v1));
  goto fg178qd_gg_done;
  }
fg178qd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg178qd_gg_function_3(uint8_t p0, int64_t p1, int64_t base) {
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
  fg178q_fail(t2);
  }
  }
  }
fg178qd_gg_done:;
}
static __device__ __forceinline__ void fg178qd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, fg178qd_gg_type_3 p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int64_t base) {
  int32_t t0;
  int32_t t1;
  uint8_t t2;
  uint8_t t3;
  int32_t t4;
  int32_t t5;
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
  int64_t t19;
  int64_t t20;
  int64_t t21;
  uint8_t t22;
  uint8_t t23;
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
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  uint8_t t38;
  uint32_t t39;
  uint32_t t40;
  uint32_t t41;
  uint32_t t42;
  uint32_t t43;
  uint32_t t44;
  uint32_t t45;
  uint32_t t46;
  uint32_t t47;
  uint32_t t48;
  uint32_t t49;
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
  uint8_t t70;
  int64_t t71;
  int64_t t72;
  int64_t t73;
  int64_t t74;
  int64_t t75;
  int64_t t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  int64_t t81;
  int64_t t82;
  uint64_t t83;
  int64_t t84;
  int64_t t85;
  uint64_t t86;
  float t87;
  float t88;
  float t89;
  float t90;
  float t91;
  float t92;
  float t93;
  float t94;
  float t95;
  uint8_t t96;
  float t97;
  float t98;
  float t99;
  float t100;
  float t101;
  int32_t t102;
  uint64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  uint64_t t110;
  int32_t t111;
  int64_t t112;
  int64_t t113;
  uint8_t t114;
  uint64_t t115;
  int64_t t116;
  int64_t t117;
  uint64_t t118;
  float t119;
  uint64_t t120;
  int64_t t121;
  int64_t t122;
  int64_t t123;
  int64_t t124;
  uint64_t t125;
  float t126;
  uint64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  fg178qd_gg_type_3 v9;
  int32_t v10;
  int32_t v11;
  int32_t v12;
  int32_t v13;
  int32_t v14;
  int64_t v15;
  uint32_t v16;
  uint32_t v17;
  uint32_t v18;
  int64_t v19;
  int64_t v20;
  int64_t v21;
  int64_t v22;
  int64_t v23;
  int64_t v24;
  float v25;
  int64_t v26;
  float v27;
  float v28;
  float v29;
  int32_t v30;
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
  {
  fg178q_pdl_lc();
  t0 = ((int32_t)(v14));
  t1 = ((int32_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) > (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int32_t)(v10));
  t5 = ((int32_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) >= (t5));
  }
  t6 = t3;
  t7 = t6;
  if (t7) {
  t8 = ((int32_t)(v11));
  t9 = ((int32_t)(((int64_t)UINT64_C(0))));
  t7 = ((t8) >= (t9));
  }
  t10 = t7;
  t11 = t10;
  if (t11) {
  t12 = ((int32_t)(v12));
  t13 = ((int32_t)(((int64_t)UINT64_C(0))));
  t11 = ((t12) >= (t13));
  }
  t14 = t11;
  t15 = t14;
  if (t15) {
  t16 = ((int32_t)(v13));
  t17 = ((int32_t)(((int64_t)UINT64_C(0))));
  t15 = ((t16) >= (t17));
  }
  t18 = t15;
  t19 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg178qd_gg_function_3(t18, t19, base + 0);
  t20 = ((int64_t)(v6));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((t20) > (t21));
  t23 = t22;
  if (t23) {
  t24 = ((int64_t)(v7));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((t24) > (t25));
  }
  t26 = t23;
  t27 = t26;
  if (t27) {
  t28 = ((int64_t)(v8));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((t28) > (t29));
  }
  t30 = t27;
  t31 = ((int64_t)(((int64_t)UINT64_C(11))));
  fg178qd_gg_function_3(t30, t31, base + 0);
  t32 = ((int64_t)(((int64_t)(v14))));
  t33 = ((int64_t)(((int64_t)(v10))));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) * (uint64_t)(t33)))));
  t35 = ((int64_t)(((int64_t)(v13))));
  v15 = ((int64_t)(((int64_t)((uint64_t)(t34) + (uint64_t)(t35)))));
  t36 = ((int64_t)(v15));
  t37 = ((int64_t)(v6));
  t38 = ((t36) >= (t37));
  if (t38) {
  {
  goto fg178qd_gg_done;
  }
  }
  v16 = ((uint32_t)(((uint32_t)(v12))));
  t39 = ((uint32_t)(v16));
  t40 = ((uint32_t)((v9).f0));
  t41 = fg178q_mulhi(t39, t40);
  v17 = ((uint32_t)(t41));
  t42 = ((uint32_t)(v17));
  t43 = ((uint32_t)(v16));
  t44 = ((uint32_t)(((uint32_t)((uint64_t)(t42) + (uint64_t)(t43)))));
  t45 = ((uint32_t)(((uint32_t)((v9).f1))));
  v18 = ((uint32_t)(((t44) >> ((uint64_t)(t45) & 31))));
  v19 = ((int64_t)(((int64_t)(v18))));
  t46 = ((uint32_t)(((uint32_t)(v12))));
  t47 = ((uint32_t)(v18));
  t48 = ((uint32_t)((v9).f2));
  t49 = ((uint32_t)(((uint32_t)((uint64_t)(t47) * (uint64_t)(t48)))));
  v20 = ((int64_t)(((int64_t)(((uint32_t)((uint64_t)(t46) - (uint64_t)(t49)))))));
  v21 = ((int64_t)(((int64_t)(v11))));
  t50 = ((int64_t)(v19));
  t51 = ((int64_t)(v8));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t50) * (uint64_t)(t51)))));
  t53 = ((int64_t)(v20));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t52) + (uint64_t)(t53)))));
  t55 = ((int64_t)(v7));
  t56 = ((int64_t)(((int64_t)((uint64_t)(t54) * (uint64_t)(t55)))));
  t57 = ((int64_t)(v21));
  t58 = ((int64_t)(((int64_t)((uint64_t)(t56) + (uint64_t)(t57)))));
  t59 = ((int64_t)(v6));
  t60 = ((int64_t)(((int64_t)((uint64_t)(t58) * (uint64_t)(t59)))));
  t61 = ((int64_t)(v15));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t60) + (uint64_t)(t61)))));
  t62 = ((int64_t)(v22));
  t63 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  v23 = ((int64_t)(((int64_t)fg178qd_gg_div(t62, t63))));
  t64 = ((int64_t)(v22));
  t65 = ((int64_t)(v23));
  t66 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t67 = ((int64_t)(((int64_t)((uint64_t)(t65) * (uint64_t)(t66)))));
  v24 = ((int64_t)(((int64_t)((uint64_t)(t64) - (uint64_t)(t67)))));
  v25 = ((float)(fg178qd_gg_double_bits(UINT64_C(0))));
  t68 = ((int64_t)(v15));
  t69 = ((int64_t)(v2));
  t70 = ((t68) < (t69));
  if (t70) {
  {
  t71 = ((int64_t)(v19));
  t72 = ((int64_t)(v5));
  t73 = ((int64_t)(((int64_t)((uint64_t)(t71) * (uint64_t)(t72)))));
  t74 = ((int64_t)(v20));
  t75 = ((int64_t)(v4));
  t76 = ((int64_t)(((int64_t)((uint64_t)(t74) * (uint64_t)(t75)))));
  t77 = ((int64_t)(((int64_t)((uint64_t)(t73) + (uint64_t)(t76)))));
  t78 = ((int64_t)(v21));
  t79 = ((int64_t)(v3));
  t80 = ((int64_t)(((int64_t)((uint64_t)(t78) * (uint64_t)(t79)))));
  t81 = ((int64_t)(((int64_t)((uint64_t)(t77) + (uint64_t)(t80)))));
  t82 = ((int64_t)(v15));
  v26 = ((int64_t)(((int64_t)((uint64_t)(t81) + (uint64_t)(t82)))));
  t83 = ((uint64_t)(v0));
  t84 = ((int64_t)(v26));
  t85 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t86 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t84) * (uint64_t)(t85)))))));
  t87 = fg178q_ld_f32(t83, t86);
  v25 = ((float)(t87));
  }
  }
  t88 = ((float)(v25));
  t89 = fg178q_fabs(t88);
  t90 = ((float)(t89));
  v27 = fg178qd_gg_function_8(t90, base + 0);
  t91 = ((float)(v25));
  v28 = fg178qd_gg_function_10(t91, base + 0);
  fg178q_pdl_sync();
  t92 = ((float)(v27));
  t93 = ((float)(fg178qd_gg_double_bits(UINT64_C(4638637247447433216))));
  v29 = ((float)(((t92) / (t93))));
  v30 = ((int32_t)(((int64_t)UINT64_C(0))));
  t94 = ((float)(v27));
  t95 = ((float)(fg178qd_gg_double_bits(UINT64_C(0))));
  t96 = ((t94) != (t95));
  if (t96) {
  {
  t97 = ((float)(v25));
  t98 = ((float)(v29));
  t99 = ((float)(((t97) / (t98))));
  t100 = fg178q_round(t99);
  t101 = ((float)(t100));
  t102 = fg178q_trunc_i32(t101);
  v30 = ((int32_t)(t102));
  }
  }
  t103 = ((uint64_t)(v1));
  t104 = ((int64_t)(v23));
  t105 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(36))))));
  t106 = ((int64_t)(((int64_t)((uint64_t)(t104) * (uint64_t)(t105)))));
  t107 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t108 = ((int64_t)(((int64_t)((uint64_t)(t106) + (uint64_t)(t107)))));
  t109 = ((int64_t)(v24));
  t110 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t108) + (uint64_t)(t109)))))));
  t111 = ((int32_t)(((int32_t)(v30))));
  fg178q_st_i8(t103, t110, t111);
  t112 = ((int64_t)(v24));
  t113 = ((int64_t)(((int64_t)UINT64_C(0))));
  t114 = ((t112) == (t113));
  if (t114) {
  {
  t115 = ((uint64_t)(v1));
  t116 = ((int64_t)(v23));
  t117 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(36))))));
  t118 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t116) * (uint64_t)(t117)))))));
  t119 = ((float)(v29));
  fg178q_st_f16(t115, t118, t119);
  t120 = ((uint64_t)(v1));
  t121 = ((int64_t)(v23));
  t122 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(36))))));
  t123 = ((int64_t)(((int64_t)((uint64_t)(t121) * (uint64_t)(t122)))));
  t124 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t125 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t123) + (uint64_t)(t124)))))));
  t126 = ((float)(v28));
  fg178q_st_f16(t120, t125, t126);
  }
  }
  }
fg178qd_gg_done:;
}
