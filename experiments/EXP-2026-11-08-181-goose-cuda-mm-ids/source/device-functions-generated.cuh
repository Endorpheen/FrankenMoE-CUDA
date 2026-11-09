#pragma once
static __device__ __forceinline__ void fg181md_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg181md_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg181md_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg181md_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg181md_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg181md_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ int32_t fg181md_gg_function_10(int32_t p0, int64_t base);
static __device__ __forceinline__ int32_t fg181md_gg_function_5(int32_t p0, int64_t base);
static __device__ __forceinline__ void fg181md_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg181md_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int32_t p4, int32_t p5, int32_t p6, int32_t p7, int32_t p8, int32_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int64_t base);
static __device__ __forceinline__ void fg181md_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg181md_gg_div(int64_t a, int64_t b) { if (!b) fg181md_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg181md_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg181md_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg181md_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg181md_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg181md_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg181md_gg_umod(uint64_t a,uint64_t b) { if(!b)fg181md_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg181md_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ int32_t fg181md_gg_function_10(int32_t p0, int64_t base) {
  int32_t t0;
  int32_t t1;
  int32_t t2;
  int32_t t3;
  int32_t t4;
  uint32_t t5;
  int32_t t6;
  int32_t t7;
  int32_t t8;
  int32_t t9;
  int32_t t10;
  int32_t t11;
  int32_t v0;
  int32_t v1;
  int32_t v2;
  v0 = p0;
  {
  v1 = ((int32_t)(v0));
  v2 = ((int32_t)(((int64_t)UINT64_C(16))));
label1:;
  t1 = ((int32_t)(v2));
  t2 = ((int32_t)(((int64_t)UINT64_C(0))));
  if (!(((t1) > (t2)))) goto label2;
  {
  t3 = ((int32_t)(v1));
  t4 = ((int32_t)(v1));
  t5 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t6 = ((int32_t)(v2));
  t7 = ((int32_t)(((int64_t)UINT64_C(32))));
  t8 = fg181m_shfl_xor(t4, t5, t6, t7);
  t9 = ((int32_t)(t8));
  v1 = ((int32_t)(((int32_t)((uint64_t)(t3) + (uint64_t)(t9)))));
  t10 = ((int32_t)(v2));
  t11 = ((int32_t)(((int64_t)UINT64_C(2))));
  v2 = ((int32_t)(((int32_t)fg181md_gg_div(t10, t11))));
  }
  goto label1;
label2:;
  t0 = ((int32_t)(v1));
  goto fg181md_gg_done;
  }
fg181md_gg_done:;
  return t0;
}
static __device__ __forceinline__ int32_t fg181md_gg_function_5(int32_t p0, int64_t base) {
  int32_t t0;
  int32_t t1;
  uint32_t t2;
  int32_t t3;
  int32_t t4;
  int32_t t5;
  int32_t t6;
  int32_t t7;
  int32_t t8;
  uint32_t t9;
  int32_t t10;
  int32_t t11;
  int32_t t12;
  int32_t t13;
  int32_t t14;
  int32_t t15;
  uint32_t t16;
  int32_t t17;
  int32_t t18;
  int32_t t19;
  int32_t t20;
  int32_t t21;
  int32_t v0;
  int32_t v1;
  int32_t v2;
  v0 = p0;
  {
  v1 = ((int32_t)(v0));
  v2 = ((int32_t)(((int64_t)UINT64_C(0))));
  t1 = ((int32_t)(v1));
  t2 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t3 = ((int32_t)(((int64_t)UINT64_C(4))));
  t4 = ((int32_t)(((int64_t)UINT64_C(8))));
  t5 = fg181m_shfl_xor(t1, t2, t3, t4);
  v2 = ((int32_t)(t5));
  t6 = ((int32_t)(v1));
  t7 = ((int32_t)(v2));
  v1 = ((int32_t)(((t6) | (t7))));
  t8 = ((int32_t)(v1));
  t9 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t10 = ((int32_t)(((int64_t)UINT64_C(2))));
  t11 = ((int32_t)(((int64_t)UINT64_C(8))));
  t12 = fg181m_shfl_xor(t8, t9, t10, t11);
  v2 = ((int32_t)(t12));
  t13 = ((int32_t)(v1));
  t14 = ((int32_t)(v2));
  v1 = ((int32_t)(((t13) | (t14))));
  t15 = ((int32_t)(v1));
  t16 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t17 = ((int32_t)(((int64_t)UINT64_C(1))));
  t18 = ((int32_t)(((int64_t)UINT64_C(8))));
  t19 = fg181m_shfl_xor(t15, t16, t17, t18);
  v2 = ((int32_t)(t19));
  t20 = ((int32_t)(v1));
  t21 = ((int32_t)(v2));
  v1 = ((int32_t)(((t20) | (t21))));
  t0 = ((int32_t)(v1));
  goto fg181md_gg_done;
  }
fg181md_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg181md_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  fg181m_fail(t2);
  }
  }
  }
fg181md_gg_done:;
}
static __device__ __forceinline__ void fg181md_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int32_t p4, int32_t p5, int32_t p6, int32_t p7, int32_t p8, int32_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int64_t base) {
  int32_t t0;
  int32_t t1;
  uint8_t t2;
  uint8_t t3;
  int32_t t4;
  int32_t t5;
  uint8_t t6;
  int64_t t7;
  int32_t t8;
  int32_t t9;
  uint8_t t10;
  uint8_t t11;
  int32_t t12;
  int32_t t13;
  uint8_t t14;
  int64_t t15;
  int32_t t16;
  int32_t t17;
  uint8_t t18;
  int64_t t19;
  int32_t t20;
  int32_t t21;
  int32_t t22;
  int32_t t23;
  int32_t t24;
  int32_t t25;
  int32_t t26;
  int32_t t27;
  int32_t t28;
  int32_t t29;
  uint8_t t30;
  uint8_t t31;
  int32_t t32;
  int32_t t33;
  uint8_t t34;
  uint64_t t35;
  int32_t t36;
  int32_t t37;
  int32_t t38;
  int32_t t39;
  int64_t t40;
  int64_t t41;
  uint64_t t42;
  int64_t t43;
  int64_t t44;
  int32_t t45;
  int32_t t46;
  uint8_t t47;
  int32_t t48;
  int32_t t49;
  uint8_t t50;
  int32_t t51;
  int32_t t52;
  int32_t t53;
  int64_t t54;
  int32_t t55;
  uint8_t t56;
  int32_t t57;
  int32_t t58;
  int32_t t59;
  int32_t t60;
  uint32_t t61;
  int32_t t62;
  int32_t t63;
  int32_t t64;
  int32_t t65;
  int32_t t66;
  uint8_t t67;
  int32_t t68;
  int32_t t69;
  int32_t t70;
  int32_t t71;
  int32_t t72;
  int64_t t73;
  int32_t t74;
  uint8_t t75;
  int32_t t76;
  int32_t t77;
  int32_t t78;
  int32_t t79;
  int32_t t80;
  int32_t t81;
  int32_t t82;
  int32_t t83;
  int32_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int32_t t88;
  int32_t t89;
  int32_t t90;
  int32_t t91;
  uint32_t t92;
  int32_t t93;
  int32_t t94;
  int32_t t95;
  int32_t t96;
  int32_t t97;
  int32_t t98;
  int32_t t99;
  int32_t t100;
  int32_t t101;
  int32_t t102;
  int32_t t103;
  int32_t t104;
  int32_t t105;
  int32_t t106;
  int32_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int32_t t111;
  int32_t t112;
  int32_t t113;
  int32_t t114;
  int32_t t115;
  int32_t t116;
  int32_t t117;
  int32_t t118;
  int32_t t119;
  uint64_t t120;
  int64_t t121;
  int64_t t122;
  uint64_t t123;
  int32_t t124;
  int32_t t125;
  int32_t t126;
  int32_t t127;
  int32_t t128;
  int32_t t129;
  int32_t t130;
  uint8_t t131;
  uint64_t t132;
  int32_t t133;
  int32_t t134;
  int32_t t135;
  int32_t t136;
  int64_t t137;
  int64_t t138;
  uint64_t t139;
  int32_t t140;
  uint64_t t141;
  int64_t t142;
  int64_t t143;
  uint64_t t144;
  int32_t t145;
  int32_t t146;
  int32_t t147;
  int32_t t148;
  int32_t t149;
  int32_t t150;
  int32_t t151;
  int32_t t152;
  int32_t t153;
  int32_t t154;
  int32_t t155;
  uint8_t t156;
  uint64_t t157;
  int64_t t158;
  int64_t t159;
  uint64_t t160;
  int32_t t161;
  int32_t t162;
  int32_t t163;
  uint8_t t164;
  uint64_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  int64_t t169;
  uint64_t t170;
  int32_t t171;
  int32_t t172;
  int32_t t173;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  int32_t v4;
  int32_t v5;
  int32_t v6;
  int32_t v7;
  int32_t v8;
  int32_t v9;
  int32_t v10;
  int32_t v11;
  int32_t v12;
  int32_t v13;
  int32_t v14;
  int32_t v15;
  int32_t v16;
  int32_t v17;
  int32_t v18;
  int32_t v19;
  int32_t v20;
  int32_t v21;
  int32_t v22;
  int32_t v23;
  int32_t v24;
  int32_t v25;
  int32_t v26;
  int32_t v27;
  int32_t v28;
  int32_t v29;
  int32_t v30;
  int32_t v31;
  int32_t v32;
  int32_t v33;
  int32_t v34;
  int32_t v35;
  int32_t v36;
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
  t0 = ((int32_t)(v14));
  t1 = ((int32_t)(((int64_t)UINT64_C(8))));
  t2 = ((t0) >= (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int32_t)(v4));
  t5 = ((int32_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) > (t5));
  }
  t6 = t3;
  t7 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg181md_gg_function_2(t6, t7, base + 0);
  t8 = ((int32_t)(v5));
  t9 = ((int32_t)(((int64_t)UINT64_C(0))));
  t10 = ((t8) >= (t9));
  t11 = t10;
  if (t11) {
  t12 = ((int32_t)(v5));
  t13 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1024))))));
  t11 = ((t12) < (t13));
  }
  t14 = t11;
  t15 = ((int64_t)(((int64_t)UINT64_C(11))));
  fg181md_gg_function_2(t14, t15, base + 0);
  t16 = ((int32_t)(v4));
  t17 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(4194304))))));
  t18 = ((t16) < (t17));
  t19 = ((int64_t)(((int64_t)UINT64_C(12))));
  fg181md_gg_function_2(t18, t19, base + 0);
  v15 = ((int32_t)(v12));
  v16 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(8))))));
  v17 = ((int32_t)(((int64_t)UINT64_C(0))));
  v18 = ((int32_t)(((int64_t)UINT64_C(0))));
  v19 = ((int32_t)(((int64_t)UINT64_C(0))));
label1:;
  t20 = ((int32_t)(v19));
  t21 = ((int32_t)(v4));
  if (!(((t20) < (t21)))) goto label2;
  {
  t22 = ((int32_t)(v19));
  t23 = ((int32_t)(v13));
  t24 = ((int32_t)(v16));
  t25 = ((int32_t)(((int32_t)fg181md_gg_div(t23, t24))));
  v20 = ((int32_t)(((int32_t)((uint64_t)(t22) + (uint64_t)(t25)))));
  t26 = ((int32_t)(v13));
  t27 = ((int32_t)(v16));
  v21 = ((int32_t)(((int32_t)fg181md_gg_mod(t26, t27))));
  v22 = ((int32_t)(((int64_t)UINT64_C(2147483647))));
  t28 = ((int32_t)(v21));
  t29 = ((int32_t)(v10));
  t30 = ((t28) < (t29));
  t31 = t30;
  if (t31) {
  t32 = ((int32_t)(v20));
  t33 = ((int32_t)(v4));
  t31 = ((t32) < (t33));
  }
  t34 = t31;
  if (t34) {
  {
  t35 = ((uint64_t)(v0));
  t36 = ((int32_t)(v20));
  t37 = ((int32_t)(v7));
  t38 = ((int32_t)(((int32_t)((uint64_t)(t36) * (uint64_t)(t37)))));
  t39 = ((int32_t)(v21));
  t40 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t38) + (uint64_t)(t39)))))));
  t41 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t42 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t40) * (uint64_t)(t41)))))));
  t43 = fg181m_ld_i32(t35, t42);
  v22 = ((int32_t)(((int32_t)(t43))));
  }
  }
  t44 = ((int64_t)UINT64_C(1));
  v23 = ((int32_t)(((int64_t)((uint64_t)0 - (uint64_t)(t44)))));
  t45 = ((int32_t)(v22));
  t46 = ((int32_t)(v15));
  t47 = ((t45) == (t46));
  if (t47) {
  {
  v23 = ((int32_t)(v21));
  }
  }
  t48 = ((int32_t)(v22));
  t49 = ((int32_t)(v15));
  t50 = ((t48) < (t49));
  if (t50) {
  {
  t51 = ((int32_t)(v17));
  t52 = ((int32_t)(((int64_t)UINT64_C(1))));
  v17 = ((int32_t)(((int32_t)((uint64_t)(t51) + (uint64_t)(t52)))));
  }
  }
  v24 = ((int32_t)(((int64_t)UINT64_C(0))));
  t53 = ((int32_t)(v23));
  t54 = ((int64_t)UINT64_C(1));
  t55 = ((int32_t)(((int64_t)((uint64_t)0 - (uint64_t)(t54)))));
  t56 = ((t53) != (t55));
  if (t56) {
  {
  v24 = ((int32_t)(((int64_t)UINT64_C(1))));
  }
  }
  t57 = ((int32_t)(v24));
  v25 = fg181md_gg_function_5(t57, base + 0);
  v26 = ((int32_t)(((int64_t)UINT64_C(0))));
  v27 = ((int32_t)(v16));
label4:;
  t58 = ((int32_t)(v27));
  t59 = ((int32_t)(v14));
  if (!(((t58) < (t59)))) goto label5;
  {
  t60 = ((int32_t)(v25));
  t61 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t62 = ((int32_t)(v27));
  t63 = ((int32_t)(v14));
  t64 = fg181m_shfl_up(t60, t61, t62, t63);
  v28 = ((int32_t)(t64));
  t65 = ((int32_t)(v13));
  t66 = ((int32_t)(v27));
  t67 = ((t65) >= (t66));
  if (t67) {
  {
  t68 = ((int32_t)(v26));
  t69 = ((int32_t)(v28));
  v26 = ((int32_t)(((int32_t)((uint64_t)(t68) + (uint64_t)(t69)))));
  }
  }
  t70 = ((int32_t)(v27));
  t71 = ((int32_t)(v16));
  v27 = ((int32_t)(((int32_t)((uint64_t)(t70) + (uint64_t)(t71)))));
  }
  goto label4;
label5:;
  t72 = ((int32_t)(v23));
  t73 = ((int64_t)UINT64_C(1));
  t74 = ((int32_t)(((int64_t)((uint64_t)0 - (uint64_t)(t73)))));
  t75 = ((t72) != (t74));
  if (t75) {
  {
  t76 = ((int32_t)(v20));
  t77 = ((int32_t)(((int64_t)UINT64_C(4194303))));
  t78 = ((int32_t)(((t76) & (t77))));
  t79 = ((int32_t)(v23));
  t80 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(22))))));
  t81 = ((int32_t)(((int32_t)((uint64_t)(t79) << ((uint64_t)(t80) & 31)))));
  v29 = ((int32_t)(((t78) | (t81))));
  t82 = ((int32_t)(((int64_t)UINT64_C(0))));
  t83 = ((int32_t)(v18));
  t84 = ((int32_t)(v26));
  t85 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t83) + (uint64_t)(t84)))))));
  t86 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t87 = ((int64_t)(((int64_t)((uint64_t)(t85) * (uint64_t)(t86)))));
  t88 = ((int32_t)(v29));
  fg181m_shm_st(t82, t87, t88);
  }
  }
  t89 = ((int32_t)(v26));
  t90 = ((int32_t)(v25));
  t91 = ((int32_t)(((int32_t)((uint64_t)(t89) + (uint64_t)(t90)))));
  t92 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t93 = ((int32_t)(v14));
  t94 = ((int32_t)(((int64_t)UINT64_C(1))));
  t95 = ((int32_t)(((int32_t)((uint64_t)(t93) - (uint64_t)(t94)))));
  t96 = ((int32_t)(v14));
  t97 = fg181m_shfl(t91, t92, t95, t96);
  v30 = ((int32_t)(t97));
  t98 = ((int32_t)(v18));
  t99 = ((int32_t)(v30));
  v18 = ((int32_t)(((int32_t)((uint64_t)(t98) + (uint64_t)(t99)))));
  t100 = ((int32_t)(v19));
  t101 = ((int32_t)(v14));
  t102 = ((int32_t)(v16));
  t103 = ((int32_t)(((int32_t)fg181md_gg_div(t101, t102))));
  v19 = ((int32_t)(((int32_t)((uint64_t)(t100) + (uint64_t)(t103)))));
  }
  goto label1;
label2:;
  t104 = ((int32_t)(v17));
  v31 = fg181md_gg_function_10(t104, base + 0);
  fg181m_syncwarp();
  v32 = ((int32_t)(v13));
label7:;
  t105 = ((int32_t)(v32));
  t106 = ((int32_t)(v18));
  if (!(((t105) < (t106)))) goto label8;
  {
  t107 = ((int32_t)(((int64_t)UINT64_C(0))));
  t108 = ((int64_t)(((int64_t)(v32))));
  t109 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t110 = ((int64_t)(((int64_t)((uint64_t)(t108) * (uint64_t)(t109)))));
  t111 = fg181m_shm_ld(t107, t110);
  v33 = ((int32_t)(t111));
  t112 = ((int32_t)(v33));
  t113 = ((int32_t)(((int64_t)UINT64_C(4194303))));
  v34 = ((int32_t)(((t112) & (t113))));
  t114 = ((int32_t)(v33));
  t115 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(22))))));
  t116 = ((int32_t)(((t114) >> ((uint64_t)(t115) & 31))));
  t117 = ((int32_t)(((int64_t)UINT64_C(1023))));
  v35 = ((int32_t)(((t116) & (t117))));
  t118 = ((int32_t)(v31));
  t119 = ((int32_t)(v32));
  v36 = ((int32_t)(((int32_t)((uint64_t)(t118) + (uint64_t)(t119)))));
  t120 = ((uint64_t)(v2));
  t121 = ((int64_t)(((int64_t)(v36))));
  t122 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t123 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t121) * (uint64_t)(t122)))))));
  t124 = ((int32_t)(v34));
  t125 = ((int32_t)(v10));
  t126 = ((int32_t)(((int32_t)((uint64_t)(t124) * (uint64_t)(t125)))));
  t127 = ((int32_t)(v35));
  t128 = ((int32_t)(((int32_t)((uint64_t)(t126) + (uint64_t)(t127)))));
  fg181m_st_i32(t120, t123, t128);
  t129 = ((int32_t)(v9));
  t130 = ((int32_t)(((int64_t)UINT64_C(0))));
  t131 = ((t129) != (t130));
  if (t131) {
  {
  t132 = ((uint64_t)(v1));
  t133 = ((int32_t)(v34));
  t134 = ((int32_t)(v10));
  t135 = ((int32_t)(((int32_t)((uint64_t)(t133) * (uint64_t)(t134)))));
  t136 = ((int32_t)(v35));
  t137 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t135) + (uint64_t)(t136)))))));
  t138 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t139 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t137) * (uint64_t)(t138)))))));
  t140 = ((int32_t)(v36));
  fg181m_st_i32(t132, t139, t140);
  }
  } else {
  {
  t141 = ((uint64_t)(v1));
  t142 = ((int64_t)(((int64_t)(v36))));
  t143 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t144 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t142) * (uint64_t)(t143)))))));
  t145 = ((int32_t)(v34));
  t146 = ((int32_t)(v8));
  t147 = ((int32_t)(((int32_t)((uint64_t)(t145) * (uint64_t)(t146)))));
  t148 = ((int32_t)(v35));
  t149 = ((int32_t)(v6));
  t150 = ((int32_t)(((int32_t)fg181md_gg_mod(t148, t149))));
  t151 = ((int32_t)(((int32_t)((uint64_t)(t147) + (uint64_t)(t150)))));
  fg181m_st_i32(t141, t144, t151);
  }
  }
  t152 = ((int32_t)(v32));
  t153 = ((int32_t)(v14));
  v32 = ((int32_t)(((int32_t)((uint64_t)(t152) + (uint64_t)(t153)))));
  }
  goto label7;
label8:;
  t154 = ((int32_t)(v13));
  t155 = ((int32_t)(((int64_t)UINT64_C(0))));
  t156 = ((t154) == (t155));
  if (t156) {
  {
  t157 = ((uint64_t)(v3));
  t158 = ((int64_t)(((int64_t)(v15))));
  t159 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t160 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t158) * (uint64_t)(t159)))))));
  t161 = ((int32_t)(v31));
  fg181m_st_i32(t157, t160, t161);
  t162 = ((int32_t)(v15));
  t163 = ((int32_t)(v11));
  t164 = ((t162) == (t163));
  if (t164) {
  {
  t165 = ((uint64_t)(v3));
  t166 = ((int64_t)(((int64_t)(v15))));
  t167 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t168 = ((int64_t)(((int64_t)((uint64_t)(t166) * (uint64_t)(t167)))));
  t169 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t170 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t168) + (uint64_t)(t169)))))));
  t171 = ((int32_t)(v31));
  t172 = ((int32_t)(v18));
  t173 = ((int32_t)(((int32_t)((uint64_t)(t171) + (uint64_t)(t172)))));
  fg181m_st_i32(t165, t170, t173);
  }
  }
  }
  }
  }
fg181md_gg_done:;
}
