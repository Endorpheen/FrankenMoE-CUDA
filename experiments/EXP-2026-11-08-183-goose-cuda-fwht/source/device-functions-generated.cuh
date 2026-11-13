#pragma once
static __device__ __forceinline__ void fg183fd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg183fd_gg_index(int64_t i, int64_t len);
static __device__ __forceinline__ int64_t fg183fd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg183fd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg183fd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg183fd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg183fd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ void fg183fd_gg_function_2(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg183fd_gg_function_1(uint64_t p0, uint64_t p1, int32_t p2, int64_t p3, float p4, int32_t p5, int32_t p6, int32_t p7, int32_t p8, int32_t p9, int64_t base);
static __device__ __forceinline__ void fg183fd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg183fd_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg183fd_gg_fail("array index out of bounds"); return i; }
static __device__ __forceinline__ int64_t fg183fd_gg_div(int64_t a, int64_t b) { if (!b) fg183fd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg183fd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg183fd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg183fd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg183fd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg183fd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg183fd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg183fd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg183fd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ void fg183fd_gg_function_2(uint8_t p0, int64_t p1, int64_t base) {
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
  fg183f_fail(t2);
  }
  }
  }
fg183fd_gg_done:;
}
static __device__ __forceinline__ void fg183fd_gg_function_1(uint64_t p0, uint64_t p1, int32_t p2, int64_t p3, float p4, int32_t p5, int32_t p6, int32_t p7, int32_t p8, int32_t p9, int64_t base) {
  int32_t t0;
  int32_t t1;
  uint8_t t2;
  uint8_t t3;
  int32_t t4;
  int32_t t5;
  int32_t t6;
  int32_t t7;
  uint8_t t8;
  int64_t t9;
  int32_t t10;
  int32_t t11;
  uint8_t t12;
  int64_t t13;
  int32_t t14;
  int32_t t15;
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
  int64_t t33;
  int64_t t34;
  int32_t t35;
  int32_t t36;
  int64_t t37;
  int32_t t38;
  int32_t t39;
  int32_t t40;
  int32_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  int32_t t45;
  float * t46;
  uint64_t t47;
  uint64_t t48;
  float t49;
  float t50;
  float t51;
  int32_t t52;
  int32_t t53;
  int32_t t54;
  int32_t t55;
  int32_t t56;
  int32_t t57;
  int32_t t58;
  float * t59;
  float t60;
  uint32_t t61;
  int32_t t62;
  int32_t t63;
  float t64;
  int32_t t65;
  int32_t t66;
  int32_t t67;
  int32_t t68;
  uint8_t t69;
  int32_t t70;
  float * t71;
  float t72;
  float t73;
  int32_t t74;
  float * t75;
  float t76;
  float t77;
  int32_t t78;
  int32_t t79;
  int32_t t80;
  int32_t t81;
  int32_t t82;
  int32_t t83;
  int32_t t84;
  int32_t t85;
  int32_t t86;
  int32_t t87;
  int32_t t88;
  int32_t t89;
  fg183fd_gg_slice t90;
  int32_t t91;
  int32_t t92;
  int32_t t93;
  float * t94;
  fg183fd_gg_slice t95;
  int32_t t96;
  int32_t t97;
  int32_t t98;
  int32_t t99;
  int32_t t100;
  float * t101;
  fg183fd_gg_slice t102;
  int32_t t103;
  int32_t t104;
  int32_t t105;
  float * t106;
  float t107;
  float t108;
  fg183fd_gg_slice t109;
  int32_t t110;
  int32_t t111;
  int32_t t112;
  int32_t t113;
  int32_t t114;
  float * t115;
  float t116;
  float t117;
  int32_t t118;
  int32_t t119;
  int32_t t120;
  int32_t t121;
  int32_t t122;
  int32_t t123;
  int32_t t124;
  int32_t t125;
  int32_t t126;
  int32_t t127;
  int64_t t128;
  int32_t t129;
  int32_t t130;
  int32_t t131;
  int32_t t132;
  int64_t t133;
  int64_t t134;
  int64_t t135;
  uint64_t t136;
  uint64_t t137;
  int32_t t138;
  float * t139;
  float t140;
  int32_t t141;
  int32_t t142;
  uint64_t v0;
  uint64_t v1;
  int32_t v2;
  int64_t v3;
  float v4;
  int32_t v5;
  int32_t v6;
  int32_t v7;
  int32_t v8;
  int32_t v9;
  int32_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int32_t v14;
  fg183fd_gg_type_7 v15;
  int32_t v16;
  int64_t v17;
  int32_t v18;
  int32_t v19;
  float v20;
  float v21;
  int32_t v22;
  int32_t v23;
  int32_t v24;
  int32_t v25;
  float v26;
  float v27;
  int32_t v28;
  int64_t v29;
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
  t0 = ((int32_t)(v2));
  t1 = ((int32_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) > (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int32_t)(v2));
  t5 = ((int32_t)(v9));
  t6 = ((int32_t)(((int32_t)fg183fd_gg_mod(t4, t5))));
  t7 = ((int32_t)(((int64_t)UINT64_C(0))));
  t3 = ((t6) == (t7));
  }
  t8 = t3;
  t9 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg183fd_gg_function_2(t8, t9, base + 0);
  t10 = ((int32_t)(v9));
  t11 = ((int32_t)(((int64_t)UINT64_C(32))));
  t12 = ((t10) == (t11));
  t13 = ((int64_t)(((int64_t)UINT64_C(11))));
  fg183fd_gg_function_2(t12, t13, base + 0);
  t14 = ((int32_t)(v2));
  t15 = ((int32_t)(v9));
  v10 = ((int32_t)(((int32_t)fg183fd_gg_div(t14, t15))));
  t16 = ((int32_t)(v10));
  t17 = ((int32_t)(((int64_t)UINT64_C(1))));
  t18 = ((t16) >= (t17));
  t19 = t18;
  if (t19) {
  t20 = ((int32_t)(v10));
  t21 = ((int32_t)(((int64_t)UINT64_C(16))));
  t19 = ((t20) <= (t21));
  }
  t22 = t19;
  t23 = ((int64_t)(((int64_t)UINT64_C(12))));
  fg183fd_gg_function_2(t22, t23, base + 0);
  t24 = ((int64_t)(((int64_t)(v5))));
  t25 = ((int64_t)(((int64_t)(v6))));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t25)))));
  t27 = ((int64_t)(((int64_t)(v7))));
  v11 = ((int64_t)(((int64_t)((uint64_t)(t26) + (uint64_t)(t27)))));
  t28 = ((int64_t)(v11));
  t29 = ((int64_t)(v3));
  t30 = ((t28) < (t29));
  if (t30) {
  {
  t31 = ((int64_t)(v11));
  t32 = ((int64_t)(((int64_t)(v2))));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t31) * (uint64_t)(t32)))));
  t33 = ((int64_t)(v11));
  t34 = ((int64_t)(((int64_t)(v2))));
  v13 = ((int64_t)(((int64_t)((uint64_t)(t33) * (uint64_t)(t34)))));
  v14 = ((int32_t)(v8));
  (v15).data[0] = ((float)(fg183fd_gg_double_bits(UINT64_C(0))));
  (v15).data[1] = ((float)(fg183fd_gg_double_bits(UINT64_C(0))));
  (v15).data[2] = ((float)(fg183fd_gg_double_bits(UINT64_C(0))));
  (v15).data[3] = ((float)(fg183fd_gg_double_bits(UINT64_C(0))));
  (v15).data[4] = ((float)(fg183fd_gg_double_bits(UINT64_C(0))));
  (v15).data[5] = ((float)(fg183fd_gg_double_bits(UINT64_C(0))));
  (v15).data[6] = ((float)(fg183fd_gg_double_bits(UINT64_C(0))));
  (v15).data[7] = ((float)(fg183fd_gg_double_bits(UINT64_C(0))));
  (v15).data[8] = ((float)(fg183fd_gg_double_bits(UINT64_C(0))));
  (v15).data[9] = ((float)(fg183fd_gg_double_bits(UINT64_C(0))));
  (v15).data[10] = ((float)(fg183fd_gg_double_bits(UINT64_C(0))));
  (v15).data[11] = ((float)(fg183fd_gg_double_bits(UINT64_C(0))));
  (v15).data[12] = ((float)(fg183fd_gg_double_bits(UINT64_C(0))));
  (v15).data[13] = ((float)(fg183fd_gg_double_bits(UINT64_C(0))));
  (v15).data[14] = ((float)(fg183fd_gg_double_bits(UINT64_C(0))));
  (v15).data[15] = ((float)(fg183fd_gg_double_bits(UINT64_C(0))));
  v16 = ((int32_t)(((int64_t)UINT64_C(0))));
label1:;
  t35 = ((int32_t)(v16));
  t36 = ((int32_t)(v10));
  if (!(((t35) < (t36)))) goto label2;
  {
  t37 = ((int64_t)(v12));
  t38 = ((int32_t)(v16));
  t39 = ((int32_t)(v9));
  t40 = ((int32_t)(((int32_t)((uint64_t)(t38) * (uint64_t)(t39)))));
  t41 = ((int32_t)(v14));
  t42 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t40) + (uint64_t)(t41)))))));
  t43 = ((int64_t)(((int64_t)((uint64_t)(t37) + (uint64_t)(t42)))));
  t44 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v17 = ((int64_t)(((int64_t)((uint64_t)(t43) * (uint64_t)(t44)))));
  /* FG_DIRECT_SLICE_INDEX */
  t45 = v16;
  t46 = ((float *)(v15).data) + fg183fd_gg_index(t45, ((int64_t)UINT64_C(16)));
  t47 = ((uint64_t)(v0));
  t48 = ((uint64_t)(((uint64_t)(v17))));
  t49 = fg183f_ld_f32(t47, t48);
  t50 = ((float)(t49));
  t51 = ((float)(v4));
  (*t46) = ((float)(((t50) * (t51))));
  t52 = v16;
  t53 = ((int32_t)(((int64_t)UINT64_C(1))));
  v16 = ((int32_t)((uint64_t)(t52) + (uint64_t)(t53)));
  }
  goto label1;
label2:;
  v18 = ((int32_t)(((int64_t)UINT64_C(1))));
label4:;
  t54 = ((int32_t)(v18));
  t55 = ((int32_t)(v9));
  if (!(((t54) < (t55)))) goto label5;
  {
  v19 = ((int32_t)(((int64_t)UINT64_C(0))));
label7:;
  t56 = ((int32_t)(v19));
  t57 = ((int32_t)(v10));
  if (!(((t56) < (t57)))) goto label8;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t58 = v19;
  t59 = ((float *)(v15).data) + fg183fd_gg_index(t58, ((int64_t)UINT64_C(16)));
  v20 = ((float)((*t59)));
  t60 = ((float)(v20));
  t61 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t62 = ((int32_t)(v18));
  t63 = ((int32_t)(v9));
  t64 = fg183f_shfl_xor(t60, t61, t62, t63);
  v21 = ((float)(t64));
  t65 = ((int32_t)(v14));
  t66 = ((int32_t)(v18));
  t67 = ((int32_t)(((t65) & (t66))));
  t68 = ((int32_t)(((int64_t)UINT64_C(0))));
  t69 = ((t67) == (t68));
  if (t69) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t70 = v19;
  t71 = ((float *)(v15).data) + fg183fd_gg_index(t70, ((int64_t)UINT64_C(16)));
  t72 = ((float)(v20));
  t73 = ((float)(v21));
  (*t71) = ((float)(((t72) + (t73))));
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t74 = v19;
  t75 = ((float *)(v15).data) + fg183fd_gg_index(t74, ((int64_t)UINT64_C(16)));
  t76 = ((float)(v21));
  t77 = ((float)(v20));
  (*t75) = ((float)(((t76) - (t77))));
  }
  }
  t78 = v19;
  t79 = ((int32_t)(((int64_t)UINT64_C(1))));
  v19 = ((int32_t)((uint64_t)(t78) + (uint64_t)(t79)));
  }
  goto label7;
label8:;
  t80 = ((int32_t)(v18));
  t81 = ((int32_t)(((int64_t)UINT64_C(2))));
  v18 = ((int32_t)(((int32_t)((uint64_t)(t80) * (uint64_t)(t81)))));
  }
  goto label4;
label5:;
  v22 = ((int32_t)(v9));
label10:;
  t82 = ((int32_t)(v22));
  t83 = ((int32_t)(v2));
  if (!(((t82) < (t83)))) goto label11;
  {
  t84 = ((int32_t)(v22));
  t85 = ((int32_t)(v9));
  v23 = ((int32_t)(((int32_t)fg183fd_gg_div(t84, t85))));
  v24 = ((int32_t)(((int64_t)UINT64_C(0))));
label13:;
  t86 = ((int32_t)(v24));
  t87 = ((int32_t)(v10));
  if (!(((t86) < (t87)))) goto label14;
  {
  v25 = ((int32_t)(((int64_t)UINT64_C(0))));
label16:;
  t88 = ((int32_t)(v25));
  t89 = ((int32_t)(v23));
  if (!(((t88) < (t89)))) goto label17;
  {
  t90 = fg183fd_gg_view((v15).data, 16);
  t91 = ((int32_t)(v24));
  t92 = ((int32_t)(v25));
  t93 = ((int32_t)((uint64_t)(t91) + (uint64_t)(t92)));
  t94 = ((float *)t90.data) + fg183fd_gg_index(t93, t90.len);
  v26 = ((float)((*t94)));
  t95 = fg183fd_gg_view((v15).data, 16);
  t96 = ((int32_t)(v24));
  t97 = ((int32_t)(v25));
  t98 = ((int32_t)(((int32_t)((uint64_t)(t96) + (uint64_t)(t97)))));
  t99 = ((int32_t)(v23));
  t100 = ((int32_t)((uint64_t)(t98) + (uint64_t)(t99)));
  t101 = ((float *)t95.data) + fg183fd_gg_index(t100, t95.len);
  v27 = ((float)((*t101)));
  t102 = fg183fd_gg_view((v15).data, 16);
  t103 = ((int32_t)(v24));
  t104 = ((int32_t)(v25));
  t105 = ((int32_t)((uint64_t)(t103) + (uint64_t)(t104)));
  t106 = ((float *)t102.data) + fg183fd_gg_index(t105, t102.len);
  t107 = ((float)(v26));
  t108 = ((float)(v27));
  (*t106) = ((float)(((t107) + (t108))));
  t109 = fg183fd_gg_view((v15).data, 16);
  t110 = ((int32_t)(v24));
  t111 = ((int32_t)(v25));
  t112 = ((int32_t)(((int32_t)((uint64_t)(t110) + (uint64_t)(t111)))));
  t113 = ((int32_t)(v23));
  t114 = ((int32_t)((uint64_t)(t112) + (uint64_t)(t113)));
  t115 = ((float *)t109.data) + fg183fd_gg_index(t114, t109.len);
  t116 = ((float)(v26));
  t117 = ((float)(v27));
  (*t115) = ((float)(((t116) - (t117))));
  t118 = v25;
  t119 = ((int32_t)(((int64_t)UINT64_C(1))));
  v25 = ((int32_t)((uint64_t)(t118) + (uint64_t)(t119)));
  }
  goto label16;
label17:;
  t120 = ((int32_t)(v24));
  t121 = ((int32_t)(((int64_t)UINT64_C(2))));
  t122 = ((int32_t)(v23));
  t123 = ((int32_t)(((int32_t)((uint64_t)(t121) * (uint64_t)(t122)))));
  v24 = ((int32_t)(((int32_t)((uint64_t)(t120) + (uint64_t)(t123)))));
  }
  goto label13;
label14:;
  t124 = ((int32_t)(v22));
  t125 = ((int32_t)(((int64_t)UINT64_C(2))));
  v22 = ((int32_t)(((int32_t)((uint64_t)(t124) * (uint64_t)(t125)))));
  }
  goto label10;
label11:;
  v28 = ((int32_t)(((int64_t)UINT64_C(0))));
label19:;
  t126 = ((int32_t)(v28));
  t127 = ((int32_t)(v10));
  if (!(((t126) < (t127)))) goto label20;
  {
  t128 = ((int64_t)(v13));
  t129 = ((int32_t)(v28));
  t130 = ((int32_t)(v9));
  t131 = ((int32_t)(((int32_t)((uint64_t)(t129) * (uint64_t)(t130)))));
  t132 = ((int32_t)(v14));
  t133 = ((int64_t)(((int64_t)(((int32_t)((uint64_t)(t131) + (uint64_t)(t132)))))));
  t134 = ((int64_t)(((int64_t)((uint64_t)(t128) + (uint64_t)(t133)))));
  t135 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v29 = ((int64_t)(((int64_t)((uint64_t)(t134) * (uint64_t)(t135)))));
  t136 = ((uint64_t)(v1));
  t137 = ((uint64_t)(((uint64_t)(v29))));
  /* FG_DIRECT_SLICE_INDEX */
  t138 = v28;
  t139 = ((float *)(v15).data) + fg183fd_gg_index(t138, ((int64_t)UINT64_C(16)));
  t140 = ((float)((*t139)));
  fg183f_st_f32(t136, t137, t140);
  t141 = v28;
  t142 = ((int32_t)(((int64_t)UINT64_C(1))));
  v28 = ((int32_t)((uint64_t)(t141) + (uint64_t)(t142)));
  }
  goto label19;
label20:;
  }
  }
  }
fg183fd_gg_done:;
}
