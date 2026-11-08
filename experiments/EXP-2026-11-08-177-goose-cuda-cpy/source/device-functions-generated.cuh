#pragma once
static __device__ __forceinline__ void fg177cd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg177cd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg177cd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg177cd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg177cd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ void fg177cd_gg_function_3(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ void fg177cd_gg_function_9(uint64_t p0, uint64_t p1, int64_t p2, int32_t p3, int32_t p4, int32_t p5, int64_t base);
static __device__ __forceinline__ uint64_t fg177cd_gg_function_6(int64_t p0, int64_t base);
static __device__ __forceinline__ void fg177cd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int32_t p17, int32_t p18, int32_t p19, int64_t base);
static __device__ __forceinline__ void fg177cd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg177cd_gg_div(int64_t a, int64_t b) { if (!b) fg177cd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg177cd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg177cd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg177cd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg177cd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg177cd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg177cd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg177cd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ void fg177cd_gg_function_3(uint8_t p0, int64_t p1, int64_t base) {
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
  fg177c_fail(t2);
  }
  }
  }
fg177cd_gg_done:;
}
static __device__ __forceinline__ void fg177cd_gg_function_9(uint64_t p0, uint64_t p1, int64_t p2, int32_t p3, int32_t p4, int32_t p5, int64_t base) {
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
  uint64_t t21;
  uint64_t t22;
  uint64_t t23;
  uint64_t t24;
  float t25;
  float t26;
  uint64_t v0;
  uint64_t v1;
  int64_t v2;
  int32_t v3;
  int32_t v4;
  int32_t v5;
  int64_t v6;
  uint64_t v7;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t0 = ((int32_t)(v5));
  t1 = ((int32_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) > (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int32_t)(v3));
  t5 = ((int32_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) >= (t5));
  }
  t6 = t3;
  t7 = t6;
  if (t7) {
  t8 = ((int32_t)(v4));
  t9 = ((int32_t)(((int64_t)UINT64_C(0))));
  t7 = ((t8) >= (t9));
  }
  t10 = t7;
  t11 = ((int64_t)(((int64_t)UINT64_C(20))));
  fg177cd_gg_function_3(t10, t11, base + 0);
  t12 = ((int64_t)(((int64_t)(v5))));
  t13 = ((int64_t)(((int64_t)(v3))));
  t14 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t13)))));
  t15 = ((int64_t)(((int64_t)(v4))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t14) + (uint64_t)(t15)))));
  t16 = ((int64_t)(v6));
  t17 = ((int64_t)(v2));
  t18 = ((t16) < (t17));
  if (t18) {
  {
  fg177c_pdl_sync();
  t19 = ((int64_t)(v6));
  t20 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v7 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))))));
  t21 = ((uint64_t)(v1));
  t22 = ((uint64_t)(v7));
  t23 = ((uint64_t)(v0));
  t24 = ((uint64_t)(v7));
  t25 = fg177c_ld_f32(t23, t24);
  t26 = ((float)(t25));
  fg177c_st_f32(t21, t22, t26);
  }
  }
  }
fg177cd_gg_done:;
}
static __device__ __forceinline__ uint64_t fg177cd_gg_function_6(int64_t p0, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  t3 = ((t1) >= (t2));
  t4 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg177cd_gg_function_3(t3, t4, base + 0);
  t0 = ((uint64_t)(((uint64_t)(v0))));
  goto fg177cd_gg_done;
  }
fg177cd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg177cd_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, int64_t p12, int64_t p13, int64_t p14, int64_t p15, int64_t p16, int32_t p17, int32_t p18, int32_t p19, int64_t base) {
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
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  uint8_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  uint8_t t19;
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
  uint8_t t31;
  int64_t t32;
  int64_t t33;
  uint8_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  int64_t t41;
  uint8_t t42;
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
  int64_t t68;
  int64_t t69;
  int64_t t70;
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
  int64_t t83;
  int64_t t84;
  int64_t t85;
  int64_t t86;
  int64_t t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  int64_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  int64_t t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  int64_t t118;
  uint64_t t119;
  int64_t t120;
  uint64_t t121;
  uint64_t t122;
  int64_t t123;
  uint64_t t124;
  float t125;
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
  int64_t v9;
  int64_t v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
  int32_t v17;
  int32_t v18;
  int32_t v19;
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
  int64_t v30;
  int64_t v31;
  int64_t v32;
  int64_t v33;
  int64_t v34;
  int64_t v35;
  int64_t v36;
  int64_t v37;
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
  {
  fg177c_pdl_lc();
  t0 = ((int32_t)(v19));
  t1 = ((int32_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) > (t1));
  t3 = t2;
  if (t3) {
  t4 = ((int32_t)(v17));
  t5 = ((int32_t)(((int64_t)UINT64_C(0))));
  t3 = ((t4) >= (t5));
  }
  t6 = t3;
  t7 = t6;
  if (t7) {
  t8 = ((int32_t)(v18));
  t9 = ((int32_t)(((int64_t)UINT64_C(0))));
  t7 = ((t8) >= (t9));
  }
  t10 = t7;
  t11 = ((int64_t)(((int64_t)UINT64_C(11))));
  fg177cd_gg_function_3(t10, t11, base + 0);
  t12 = ((int64_t)(v3));
  t13 = ((int64_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) > (t13));
  t15 = t14;
  if (t15) {
  t16 = ((int64_t)(v4));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = ((t16) > (t17));
  }
  t18 = t15;
  t19 = t18;
  if (t19) {
  t20 = ((int64_t)(v5));
  t21 = ((int64_t)(((int64_t)UINT64_C(0))));
  t19 = ((t20) > (t21));
  }
  t22 = t19;
  t23 = t22;
  if (t23) {
  t24 = ((int64_t)(v10));
  t25 = ((int64_t)(((int64_t)UINT64_C(0))));
  t23 = ((t24) > (t25));
  }
  t26 = t23;
  t27 = t26;
  if (t27) {
  t28 = ((int64_t)(v11));
  t29 = ((int64_t)(((int64_t)UINT64_C(0))));
  t27 = ((t28) > (t29));
  }
  t30 = t27;
  t31 = t30;
  if (t31) {
  t32 = ((int64_t)(v12));
  t33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((t32) > (t33));
  }
  t34 = t31;
  t35 = ((int64_t)(((int64_t)UINT64_C(12))));
  fg177cd_gg_function_3(t34, t35, base + 0);
  t36 = ((int64_t)(((int64_t)(v19))));
  t37 = ((int64_t)(((int64_t)(v17))));
  t38 = ((int64_t)(((int64_t)((uint64_t)(t36) * (uint64_t)(t37)))));
  t39 = ((int64_t)(((int64_t)(v18))));
  v20 = ((int64_t)(((int64_t)((uint64_t)(t38) + (uint64_t)(t39)))));
  t40 = ((int64_t)(v20));
  t41 = ((int64_t)(v2));
  t42 = ((t40) < (t41));
  if (t42) {
  {
  t43 = ((int64_t)(v3));
  t44 = ((int64_t)(v4));
  t45 = ((int64_t)(((int64_t)((uint64_t)(t43) * (uint64_t)(t44)))));
  t46 = ((int64_t)(v5));
  v21 = ((int64_t)(((int64_t)((uint64_t)(t45) * (uint64_t)(t46)))));
  t47 = ((int64_t)(v20));
  t48 = ((int64_t)(v21));
  v22 = ((int64_t)(((int64_t)fg177cd_gg_div(t47, t48))));
  t49 = ((int64_t)(v20));
  t50 = ((int64_t)(v22));
  t51 = ((int64_t)(v21));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t50) * (uint64_t)(t51)))));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t49) - (uint64_t)(t52)))));
  t53 = ((int64_t)(v3));
  t54 = ((int64_t)(v4));
  v24 = ((int64_t)(((int64_t)((uint64_t)(t53) * (uint64_t)(t54)))));
  t55 = ((int64_t)(v23));
  t56 = ((int64_t)(v24));
  v25 = ((int64_t)(((int64_t)fg177cd_gg_div(t55, t56))));
  t57 = ((int64_t)(v23));
  t58 = ((int64_t)(v25));
  t59 = ((int64_t)(v24));
  t60 = ((int64_t)(((int64_t)((uint64_t)(t58) * (uint64_t)(t59)))));
  v26 = ((int64_t)(((int64_t)((uint64_t)(t57) - (uint64_t)(t60)))));
  t61 = ((int64_t)(v26));
  t62 = ((int64_t)(v3));
  v27 = ((int64_t)(((int64_t)fg177cd_gg_div(t61, t62))));
  t63 = ((int64_t)(v26));
  t64 = ((int64_t)(v27));
  t65 = ((int64_t)(v3));
  t66 = ((int64_t)(((int64_t)((uint64_t)(t64) * (uint64_t)(t65)))));
  v28 = ((int64_t)(((int64_t)((uint64_t)(t63) - (uint64_t)(t66)))));
  t67 = ((int64_t)(v28));
  t68 = ((int64_t)(v6));
  t69 = ((int64_t)(((int64_t)((uint64_t)(t67) * (uint64_t)(t68)))));
  t70 = ((int64_t)(v27));
  t71 = ((int64_t)(v7));
  t72 = ((int64_t)(((int64_t)((uint64_t)(t70) * (uint64_t)(t71)))));
  t73 = ((int64_t)(((int64_t)((uint64_t)(t69) + (uint64_t)(t72)))));
  t74 = ((int64_t)(v25));
  t75 = ((int64_t)(v8));
  t76 = ((int64_t)(((int64_t)((uint64_t)(t74) * (uint64_t)(t75)))));
  t77 = ((int64_t)(((int64_t)((uint64_t)(t73) + (uint64_t)(t76)))));
  t78 = ((int64_t)(v22));
  t79 = ((int64_t)(v9));
  t80 = ((int64_t)(((int64_t)((uint64_t)(t78) * (uint64_t)(t79)))));
  v29 = ((int64_t)(((int64_t)((uint64_t)(t77) + (uint64_t)(t80)))));
  t81 = ((int64_t)(v10));
  t82 = ((int64_t)(v11));
  t83 = ((int64_t)(((int64_t)((uint64_t)(t81) * (uint64_t)(t82)))));
  t84 = ((int64_t)(v12));
  v30 = ((int64_t)(((int64_t)((uint64_t)(t83) * (uint64_t)(t84)))));
  t85 = ((int64_t)(v20));
  t86 = ((int64_t)(v30));
  v31 = ((int64_t)(((int64_t)fg177cd_gg_div(t85, t86))));
  t87 = ((int64_t)(v20));
  t88 = ((int64_t)(v31));
  t89 = ((int64_t)(v30));
  t90 = ((int64_t)(((int64_t)((uint64_t)(t88) * (uint64_t)(t89)))));
  v32 = ((int64_t)(((int64_t)((uint64_t)(t87) - (uint64_t)(t90)))));
  t91 = ((int64_t)(v10));
  t92 = ((int64_t)(v11));
  v33 = ((int64_t)(((int64_t)((uint64_t)(t91) * (uint64_t)(t92)))));
  t93 = ((int64_t)(v32));
  t94 = ((int64_t)(v33));
  v34 = ((int64_t)(((int64_t)fg177cd_gg_div(t93, t94))));
  t95 = ((int64_t)(v32));
  t96 = ((int64_t)(v34));
  t97 = ((int64_t)(v33));
  t98 = ((int64_t)(((int64_t)((uint64_t)(t96) * (uint64_t)(t97)))));
  v35 = ((int64_t)(((int64_t)((uint64_t)(t95) - (uint64_t)(t98)))));
  t99 = ((int64_t)(v35));
  t100 = ((int64_t)(v10));
  v36 = ((int64_t)(((int64_t)fg177cd_gg_div(t99, t100))));
  t101 = ((int64_t)(v35));
  t102 = ((int64_t)(v36));
  t103 = ((int64_t)(v10));
  t104 = ((int64_t)(((int64_t)((uint64_t)(t102) * (uint64_t)(t103)))));
  v37 = ((int64_t)(((int64_t)((uint64_t)(t101) - (uint64_t)(t104)))));
  t105 = ((int64_t)(v37));
  t106 = ((int64_t)(v13));
  t107 = ((int64_t)(((int64_t)((uint64_t)(t105) * (uint64_t)(t106)))));
  t108 = ((int64_t)(v36));
  t109 = ((int64_t)(v14));
  t110 = ((int64_t)(((int64_t)((uint64_t)(t108) * (uint64_t)(t109)))));
  t111 = ((int64_t)(((int64_t)((uint64_t)(t107) + (uint64_t)(t110)))));
  t112 = ((int64_t)(v34));
  t113 = ((int64_t)(v15));
  t114 = ((int64_t)(((int64_t)((uint64_t)(t112) * (uint64_t)(t113)))));
  t115 = ((int64_t)(((int64_t)((uint64_t)(t111) + (uint64_t)(t114)))));
  t116 = ((int64_t)(v31));
  t117 = ((int64_t)(v16));
  t118 = ((int64_t)(((int64_t)((uint64_t)(t116) * (uint64_t)(t117)))));
  v38 = ((int64_t)(((int64_t)((uint64_t)(t115) + (uint64_t)(t118)))));
  fg177c_pdl_sync();
  t119 = ((uint64_t)(v1));
  t120 = ((int64_t)(v38));
  t121 = fg177cd_gg_function_6(t120, base + 0);
  t122 = ((uint64_t)(v0));
  t123 = ((int64_t)(v29));
  t124 = fg177cd_gg_function_6(t123, base + 0);
  t125 = fg177c_ld_f32(t122, t124);
  t126 = ((float)(t125));
  fg177c_st_f32(t119, t121, t126);
  }
  }
  }
fg177cd_gg_done:;
}
