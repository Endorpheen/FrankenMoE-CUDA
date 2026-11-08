#pragma once
static __device__ __forceinline__ void fg180qd_gg_fail(const char*);
static __device__ __forceinline__ int64_t fg180qd_gg_div(int64_t a, int64_t b);
static __device__ __forceinline__ int64_t fg180qd_gg_mod(int64_t a, int64_t b);
static __device__ __forceinline__ uint64_t fg180qd_gg_udiv(uint64_t a,uint64_t b);
static __device__ __forceinline__ uint64_t fg180qd_gg_umod(uint64_t a,uint64_t b);
static __device__ __forceinline__ double fg180qd_gg_double_bits(uint64_t bits);
static __device__ __forceinline__ void fg180qd_gg_function_3(uint8_t p0, int64_t p1, int64_t base);
static __device__ __forceinline__ int64_t fg180qd_gg_function_5(uint64_t p0, uint64_t p1, int64_t base);
static __device__ __forceinline__ float fg180qd_gg_function_15(float p0, int32_t p1, int64_t base);
static __device__ __forceinline__ float fg180qd_gg_function_12(float p0, float p1, int64_t base);
static __device__ __forceinline__ float fg180qd_gg_function_13(float p0, int32_t p1, int64_t base);
static __device__ __forceinline__ void fg180qd_gg_function_2(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int32_t p18, int32_t p19, int64_t base);
static __device__ __forceinline__ void fg180qd_gg_function_22(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base);
static __device__ __forceinline__ void fg180qd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base);
static __device__ __forceinline__ void fg180qd_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ int64_t fg180qd_gg_div(int64_t a, int64_t b) { if (!b) fg180qd_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg180qd_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg180qd_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg180qd_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg180qd_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg180qd_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg180qd_gg_umod(uint64_t a,uint64_t b) { if(!b)fg180qd_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ double fg180qd_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ void fg180qd_gg_function_3(uint8_t p0, int64_t p1, int64_t base) {
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
  fg180q_fail(t2);
  }
  }
  }
fg180qd_gg_done:;
}
static __device__ __forceinline__ int64_t fg180qd_gg_function_5(uint64_t p0, uint64_t p1, int64_t base) {
  int64_t t0;
  uint64_t t1;
  uint64_t t2;
  int64_t t3;
  uint64_t v0;
  uint64_t v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((uint64_t)(v0));
  t2 = ((uint64_t)(v1));
  t3 = fg180q_ld_i32(t1, t2);
  t0 = ((int64_t)(t3));
  goto fg180qd_gg_done;
  }
fg180qd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg180qd_gg_function_15(float p0, int32_t p1, int64_t base) {
  float t0;
  int32_t t1;
  int32_t t2;
  uint8_t t3;
  float t4;
  float t5;
  uint32_t t6;
  int32_t t7;
  float t8;
  float t9;
  float t10;
  float t11;
  uint32_t t12;
  int32_t t13;
  float t14;
  float t15;
  float t16;
  float t17;
  uint32_t t18;
  int32_t t19;
  float t20;
  float t21;
  float t22;
  float t23;
  uint32_t t24;
  int32_t t25;
  float t26;
  float t27;
  float t28;
  float t29;
  uint32_t t30;
  int32_t t31;
  float t32;
  float t33;
  float t34;
  float t35;
  uint32_t t36;
  int32_t t37;
  float t38;
  float t39;
  float t40;
  float t41;
  uint32_t t42;
  int32_t t43;
  float t44;
  float t45;
  float v0;
  int32_t v1;
  float v2;
  v0 = p0;
  v1 = p1;
  {
  v2 = ((float)(v0));
  t1 = ((int32_t)(v1));
  t2 = ((int32_t)(((int64_t)UINT64_C(2))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = ((float)(v2));
  t5 = ((float)(v2));
  t6 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t7 = ((int32_t)(((int64_t)UINT64_C(8))));
  t8 = fg180q_shfl_xor(t5, t6, t7);
  t9 = ((float)(t8));
  v2 = ((float)(((t4) + (t9))));
  t10 = ((float)(v2));
  t11 = ((float)(v2));
  t12 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t13 = ((int32_t)(((int64_t)UINT64_C(4))));
  t14 = fg180q_shfl_xor(t11, t12, t13);
  t15 = ((float)(t14));
  v2 = ((float)(((t10) + (t15))));
  t16 = ((float)(v2));
  t17 = ((float)(v2));
  t18 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t19 = ((int32_t)(((int64_t)UINT64_C(2))));
  t20 = fg180q_shfl_xor(t17, t18, t19);
  t21 = ((float)(t20));
  v2 = ((float)(((t16) + (t21))));
  t22 = ((float)(v2));
  t23 = ((float)(v2));
  t24 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t25 = ((int32_t)(((int64_t)UINT64_C(1))));
  t26 = fg180q_shfl_xor(t23, t24, t25);
  t27 = ((float)(t26));
  v2 = ((float)(((t22) + (t27))));
  }
  } else {
  {
  t28 = ((float)(v2));
  t29 = ((float)(v2));
  t30 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t31 = ((int32_t)(((int64_t)UINT64_C(4))));
  t32 = fg180q_shfl_xor(t29, t30, t31);
  t33 = ((float)(t32));
  v2 = ((float)(((t28) + (t33))));
  t34 = ((float)(v2));
  t35 = ((float)(v2));
  t36 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t37 = ((int32_t)(((int64_t)UINT64_C(2))));
  t38 = fg180q_shfl_xor(t35, t36, t37);
  t39 = ((float)(t38));
  v2 = ((float)(((t34) + (t39))));
  t40 = ((float)(v2));
  t41 = ((float)(v2));
  t42 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t43 = ((int32_t)(((int64_t)UINT64_C(1))));
  t44 = fg180q_shfl_xor(t41, t42, t43);
  t45 = ((float)(t44));
  v2 = ((float)(((t40) + (t45))));
  }
  }
  t0 = ((float)(v2));
  goto fg180qd_gg_done;
  }
fg180qd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg180qd_gg_function_12(float p0, float p1, int64_t base) {
  float t0;
  float t1;
  float t2;
  uint8_t t3;
  float v0;
  float v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((float)(v0));
  t2 = ((float)(v1));
  t3 = ((t1) < (t2));
  if (t3) {
  {
  t0 = ((float)(v1));
  }
  } else {
  {
  t0 = ((float)(v0));
  }
  }
  goto fg180qd_gg_done;
  }
fg180qd_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg180qd_gg_function_13(float p0, int32_t p1, int64_t base) {
  float t0;
  int32_t t1;
  int32_t t2;
  uint8_t t3;
  float t4;
  float t5;
  uint32_t t6;
  int32_t t7;
  float t8;
  float t9;
  float t10;
  float t11;
  uint32_t t12;
  int32_t t13;
  float t14;
  float t15;
  float t16;
  float t17;
  uint32_t t18;
  int32_t t19;
  float t20;
  float t21;
  float t22;
  float t23;
  uint32_t t24;
  int32_t t25;
  float t26;
  float t27;
  float t28;
  float t29;
  uint32_t t30;
  int32_t t31;
  float t32;
  float t33;
  float t34;
  float t35;
  uint32_t t36;
  int32_t t37;
  float t38;
  float t39;
  float t40;
  float t41;
  uint32_t t42;
  int32_t t43;
  float t44;
  float t45;
  float v0;
  int32_t v1;
  float v2;
  v0 = p0;
  v1 = p1;
  {
  v2 = ((float)(v0));
  t1 = ((int32_t)(v1));
  t2 = ((int32_t)(((int64_t)UINT64_C(2))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t4 = ((float)(v2));
  t5 = ((float)(v2));
  t6 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t7 = ((int32_t)(((int64_t)UINT64_C(8))));
  t8 = fg180q_shfl_xor(t5, t6, t7);
  t9 = ((float)(t8));
  v2 = fg180qd_gg_function_12(t4, t9, base + 0);
  t10 = ((float)(v2));
  t11 = ((float)(v2));
  t12 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t13 = ((int32_t)(((int64_t)UINT64_C(4))));
  t14 = fg180q_shfl_xor(t11, t12, t13);
  t15 = ((float)(t14));
  v2 = fg180qd_gg_function_12(t10, t15, base + 0);
  t16 = ((float)(v2));
  t17 = ((float)(v2));
  t18 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t19 = ((int32_t)(((int64_t)UINT64_C(2))));
  t20 = fg180q_shfl_xor(t17, t18, t19);
  t21 = ((float)(t20));
  v2 = fg180qd_gg_function_12(t16, t21, base + 0);
  t22 = ((float)(v2));
  t23 = ((float)(v2));
  t24 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t25 = ((int32_t)(((int64_t)UINT64_C(1))));
  t26 = fg180q_shfl_xor(t23, t24, t25);
  t27 = ((float)(t26));
  v2 = fg180qd_gg_function_12(t22, t27, base + 0);
  }
  } else {
  {
  t28 = ((float)(v2));
  t29 = ((float)(v2));
  t30 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t31 = ((int32_t)(((int64_t)UINT64_C(4))));
  t32 = fg180q_shfl_xor(t29, t30, t31);
  t33 = ((float)(t32));
  v2 = fg180qd_gg_function_12(t28, t33, base + 0);
  t34 = ((float)(v2));
  t35 = ((float)(v2));
  t36 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t37 = ((int32_t)(((int64_t)UINT64_C(2))));
  t38 = fg180q_shfl_xor(t35, t36, t37);
  t39 = ((float)(t38));
  v2 = fg180qd_gg_function_12(t34, t39, base + 0);
  t40 = ((float)(v2));
  t41 = ((float)(v2));
  t42 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4294967295))))));
  t43 = ((int32_t)(((int64_t)UINT64_C(1))));
  t44 = fg180q_shfl_xor(t41, t42, t43);
  t45 = ((float)(t44));
  v2 = fg180qd_gg_function_12(t40, t45, base + 0);
  }
  }
  t0 = ((float)(v2));
  goto fg180qd_gg_done;
  }
fg180qd_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg180qd_gg_function_2(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int32_t p18, int32_t p19, int64_t base) {
  int32_t t0;
  int32_t t1;
  uint8_t t2;
  int32_t t3;
  int32_t t4;
  uint8_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  uint8_t t14;
  int64_t t15;
  int64_t t16;
  uint8_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  uint8_t t21;
  uint8_t t22;
  int64_t t23;
  int64_t t24;
  uint8_t t25;
  uint8_t t26;
  int64_t t27;
  int64_t t28;
  uint8_t t29;
  int64_t t30;
  int32_t t31;
  int32_t t32;
  uint8_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  uint64_t t40;
  uint64_t t41;
  uint8_t t42;
  uint64_t t43;
  int64_t t44;
  int64_t t45;
  uint64_t t46;
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
  uint8_t t65;
  int64_t t66;
  int64_t t67;
  int64_t t68;
  int64_t t69;
  uint64_t t70;
  uint64_t t71;
  float t72;
  uint64_t t73;
  uint64_t t74;
  float t75;
  uint64_t t76;
  uint64_t t77;
  float t78;
  uint64_t t79;
  uint64_t t80;
  float t81;
  float t82;
  float t83;
  float t84;
  float t85;
  float t86;
  float t87;
  float t88;
  float t89;
  float t90;
  float t91;
  float t92;
  float t93;
  float t94;
  float t95;
  float t96;
  int32_t t97;
  int32_t t98;
  int32_t t99;
  uint8_t t100;
  float t101;
  float t102;
  float t103;
  float t104;
  float t105;
  float t106;
  float t107;
  int32_t t108;
  float t109;
  float t110;
  float t111;
  float t112;
  float t113;
  float t114;
  float t115;
  int32_t t116;
  float t117;
  float t118;
  float t119;
  float t120;
  float t121;
  int32_t t122;
  float t123;
  float t124;
  float t125;
  float t126;
  float t127;
  int32_t t128;
  float t129;
  float t130;
  float t131;
  float t132;
  float t133;
  int32_t t134;
  int32_t t135;
  int32_t t136;
  int32_t t137;
  int32_t t138;
  int32_t t139;
  int32_t t140;
  int32_t t141;
  int32_t t142;
  int32_t t143;
  int32_t t144;
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
  float t155;
  float t156;
  int32_t t157;
  int32_t t158;
  uint8_t t159;
  int32_t t160;
  int32_t t161;
  int32_t t162;
  int32_t t163;
  uint8_t t164;
  int64_t t165;
  int64_t t166;
  int64_t t167;
  int64_t t168;
  uint64_t t169;
  int64_t t170;
  int64_t t171;
  uint64_t t172;
  int64_t t173;
  int64_t t174;
  int64_t t175;
  int64_t t176;
  int32_t t177;
  int32_t t178;
  int32_t t179;
  int32_t t180;
  int32_t t181;
  int32_t t182;
  int64_t t183;
  int64_t t184;
  int64_t t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  int64_t t191;
  int64_t t192;
  uint8_t t193;
  int64_t t194;
  int64_t t195;
  int64_t t196;
  uint64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  uint64_t t202;
  int32_t t203;
  int32_t t204;
  int32_t t205;
  uint8_t t206;
  int64_t t207;
  int64_t t208;
  int64_t t209;
  int64_t t210;
  uint8_t t211;
  uint8_t t212;
  int64_t t213;
  int64_t t214;
  uint8_t t215;
  uint64_t t216;
  int64_t t217;
  int64_t t218;
  int64_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  int64_t t223;
  int64_t t224;
  int64_t t225;
  int64_t t226;
  uint64_t t227;
  float t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  uint8_t t233;
  uint64_t t234;
  int64_t t235;
  int64_t t236;
  int64_t t237;
  int64_t t238;
  int64_t t239;
  int64_t t240;
  int64_t t241;
  int64_t t242;
  uint64_t t243;
  float t244;
  int64_t t245;
  int64_t t246;
  int64_t t247;
  int64_t t248;
  uint8_t t249;
  int32_t t250;
  int32_t t251;
  uint8_t t252;
  uint64_t t253;
  int64_t t254;
  int64_t t255;
  int64_t t256;
  int64_t t257;
  int64_t t258;
  int64_t t259;
  uint64_t t260;
  float t261;
  float t262;
  uint64_t t263;
  int64_t t264;
  int64_t t265;
  int64_t t266;
  int64_t t267;
  int64_t t268;
  int64_t t269;
  uint64_t t270;
  float t271;
  int32_t t272;
  int32_t t273;
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
  int64_t v22;
  int64_t v23;
  int64_t v24;
  int64_t v25;
  int64_t v26;
  int32_t v27;
  int64_t v28;
  int64_t v29;
  float v30;
  float v31;
  float v32;
  float v33;
  int64_t v34;
  uint64_t v35;
  float v36;
  float v37;
  float v38;
  int32_t v39;
  int32_t v40;
  int32_t v41;
  int32_t v42;
  int32_t v43;
  int32_t v44;
  float v45;
  int32_t v46;
  int32_t v47;
  int64_t v48;
  int64_t v49;
  int64_t v50;
  int32_t v51;
  int64_t v52;
  int64_t v53;
  int64_t v54;
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
  t0 = ((int32_t)(v11));
  t1 = ((int32_t)(((int64_t)UINT64_C(2))));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  v20 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(64))))));
  }
  } else {
  {
  v20 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
  }
  }
  t3 = ((int32_t)(v11));
  t4 = ((int32_t)(((int64_t)UINT64_C(2))));
  t5 = ((t3) == (t4));
  if (t5) {
  {
  v21 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(16))))));
  }
  } else {
  {
  v21 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(32))))));
  }
  }
  t6 = ((int64_t)(((int64_t)(v17))));
  t7 = ((int64_t)(((int64_t)(v14))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t6) * (uint64_t)(t7)))));
  t9 = ((int64_t)(((int64_t)(v16))));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t8) + (uint64_t)(t9)))));
  t11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v22 = ((int64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  t12 = ((int64_t)(v22));
  t13 = ((int64_t)(v7));
  t14 = ((t12) < (t13));
  if (t14) {
  {
  t15 = ((int64_t)(v7));
  t16 = ((int64_t)(((int64_t)UINT64_C(0))));
  t17 = ((t15) > (t16));
  t18 = t17;
  if (t18) {
  t19 = ((int64_t)(v8));
  t20 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = ((t19) > (t20));
  }
  t21 = t18;
  t22 = t21;
  if (t22) {
  t23 = ((int64_t)(v9));
  t24 = ((int64_t)(((int64_t)UINT64_C(0))));
  t22 = ((t23) > (t24));
  }
  t25 = t22;
  t26 = t25;
  if (t26) {
  t27 = ((int64_t)(v3));
  t28 = ((int64_t)(((int64_t)UINT64_C(0))));
  t26 = ((t27) > (t28));
  }
  t29 = t26;
  t30 = ((int64_t)(((int64_t)UINT64_C(10))));
  fg180qd_gg_function_3(t29, t30, base + 0);
  v23 = ((int64_t)(((int64_t)UINT64_C(0))));
  t31 = ((int32_t)(v12));
  t32 = ((int32_t)(((int64_t)UINT64_C(0))));
  t33 = ((t31) != (t32));
  if (t33) {
  {
  t34 = ((int64_t)(((int64_t)(v13))));
  t35 = ((int64_t)(v5));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t34) * (uint64_t)(t35)))));
  }
  } else {
  {
  t36 = ((int64_t)(((int64_t)(v15))));
  t37 = ((int64_t)(v9));
  v24 = ((int64_t)(((int64_t)fg180qd_gg_mod(t36, t37))));
  t38 = ((int64_t)(((int64_t)(v15))));
  t39 = ((int64_t)(v9));
  v25 = ((int64_t)(((int64_t)fg180qd_gg_div(t38, t39))));
  v26 = ((int64_t)(((int64_t)(v13))));
  t40 = ((uint64_t)(v1));
  t41 = ((uint64_t)(((int64_t)UINT64_C(0))));
  t42 = ((t40) != (t41));
  if (t42) {
  {
  t43 = ((uint64_t)(v1));
  t44 = ((int64_t)(((int64_t)(v13))));
  t45 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t46 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t44) * (uint64_t)(t45)))))));
  v26 = fg180qd_gg_function_5(t43, t46, base + 0);
  }
  }
  t47 = ((int64_t)(v25));
  t48 = ((int64_t)(v6));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t47) * (uint64_t)(t48)))));
  t50 = ((int64_t)(v24));
  t51 = ((int64_t)(v5));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t50) * (uint64_t)(t51)))));
  t53 = ((int64_t)(((int64_t)((uint64_t)(t49) + (uint64_t)(t52)))));
  t54 = ((int64_t)(v26));
  t55 = ((int64_t)(v4));
  t56 = ((int64_t)(((int64_t)((uint64_t)(t54) * (uint64_t)(t55)))));
  v23 = ((int64_t)(((int64_t)((uint64_t)(t53) + (uint64_t)(t56)))));
  }
  }
  t57 = ((int64_t)(v22));
  t58 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(128))))));
  v27 = ((int32_t)(((int32_t)(((int64_t)fg180qd_gg_div(t57, t58))))));
  v28 = ((int64_t)(((int64_t)(v27))));
  t59 = ((int64_t)(v22));
  t60 = ((int64_t)(v28));
  t61 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(128))))));
  t62 = ((int64_t)(((int64_t)((uint64_t)(t60) * (uint64_t)(t61)))));
  v29 = ((int64_t)(((int64_t)((uint64_t)(t59) - (uint64_t)(t62)))));
  v30 = ((float)(fg180qd_gg_double_bits(UINT64_C(0))));
  v31 = ((float)(fg180qd_gg_double_bits(UINT64_C(0))));
  v32 = ((float)(fg180qd_gg_double_bits(UINT64_C(0))));
  v33 = ((float)(fg180qd_gg_double_bits(UINT64_C(0))));
  t63 = ((int64_t)(v22));
  t64 = ((int64_t)(v3));
  t65 = ((t63) < (t64));
  if (t65) {
  {
  t66 = ((int64_t)(v23));
  t67 = ((int64_t)(v22));
  v34 = ((int64_t)(((int64_t)((uint64_t)(t66) + (uint64_t)(t67)))));
  t68 = ((int64_t)(v34));
  t69 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v35 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t68) * (uint64_t)(t69)))))));
  t70 = ((uint64_t)(v0));
  t71 = ((uint64_t)(v35));
  t72 = fg180q_ld_f4x(t70, t71);
  v30 = ((float)(t72));
  t73 = ((uint64_t)(v0));
  t74 = ((uint64_t)(v35));
  t75 = fg180q_ld_f4y(t73, t74);
  v31 = ((float)(t75));
  t76 = ((uint64_t)(v0));
  t77 = ((uint64_t)(v35));
  t78 = fg180q_ld_f4z(t76, t77);
  v32 = ((float)(t78));
  t79 = ((uint64_t)(v0));
  t80 = ((uint64_t)(v35));
  t81 = fg180q_ld_f4w(t79, t80);
  v33 = ((float)(t81));
  }
  }
  t82 = ((float)(v30));
  t83 = fg180q_fabs(t82);
  v36 = ((float)(t83));
  t84 = ((float)(v36));
  t85 = ((float)(v31));
  t86 = fg180q_fabs(t85);
  t87 = ((float)(t86));
  v36 = fg180qd_gg_function_12(t84, t87, base + 0);
  t88 = ((float)(v36));
  t89 = ((float)(v32));
  t90 = fg180q_fabs(t89);
  t91 = ((float)(t90));
  v36 = fg180qd_gg_function_12(t88, t91, base + 0);
  t92 = ((float)(v36));
  t93 = ((float)(v33));
  t94 = fg180q_fabs(t93);
  t95 = ((float)(t94));
  v36 = fg180qd_gg_function_12(t92, t95, base + 0);
  t96 = ((float)(v36));
  t97 = ((int32_t)(v11));
  v36 = fg180qd_gg_function_13(t96, t97, base + 0);
  v37 = ((float)(fg180qd_gg_double_bits(UINT64_C(0))));
  t98 = ((int32_t)(v11));
  t99 = ((int32_t)(((int64_t)UINT64_C(0))));
  t100 = ((t98) != (t99));
  if (t100) {
  {
  t101 = ((float)(v30));
  t102 = ((float)(v31));
  v37 = ((float)(((t101) + (t102))));
  t103 = ((float)(v37));
  t104 = ((float)(v32));
  v37 = ((float)(((t103) + (t104))));
  t105 = ((float)(v37));
  t106 = ((float)(v33));
  v37 = ((float)(((t105) + (t106))));
  t107 = ((float)(v37));
  t108 = ((int32_t)(v11));
  v37 = fg180qd_gg_function_15(t107, t108, base + 0);
  }
  }
  t109 = ((float)(fg180qd_gg_double_bits(UINT64_C(4638637247447433216))));
  t110 = ((float)(v36));
  v38 = ((float)(((t109) / (t110))));
  fg180q_pdl_sync();
  t111 = ((float)(v30));
  t112 = ((float)(v38));
  t113 = ((float)(((t111) * (t112))));
  t114 = fg180q_round(t113);
  t115 = ((float)(t114));
  t116 = fg180q_i32(t115);
  v39 = ((int32_t)(t116));
  t117 = ((float)(v31));
  t118 = ((float)(v38));
  t119 = ((float)(((t117) * (t118))));
  t120 = fg180q_round(t119);
  t121 = ((float)(t120));
  t122 = fg180q_i32(t121);
  v40 = ((int32_t)(t122));
  t123 = ((float)(v32));
  t124 = ((float)(v38));
  t125 = ((float)(((t123) * (t124))));
  t126 = fg180q_round(t125);
  t127 = ((float)(t126));
  t128 = fg180q_i32(t127);
  v41 = ((int32_t)(t128));
  t129 = ((float)(v33));
  t130 = ((float)(v38));
  t131 = ((float)(((t129) * (t130))));
  t132 = fg180q_round(t131);
  t133 = ((float)(t132));
  t134 = fg180q_i32(t133);
  v42 = ((int32_t)(t134));
  v43 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(255))))));
  t135 = ((int32_t)(v39));
  t136 = ((int32_t)(v43));
  t137 = ((int32_t)(((t135) & (t136))));
  t138 = ((int32_t)(v40));
  t139 = ((int32_t)(v43));
  t140 = ((int32_t)(((t138) & (t139))));
  t141 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(8))))));
  t142 = ((int32_t)(((int32_t)((uint64_t)(t140) << ((uint64_t)(t141) & 31)))));
  t143 = ((int32_t)(((t137) | (t142))));
  t144 = ((int32_t)(v41));
  t145 = ((int32_t)(v43));
  t146 = ((int32_t)(((t144) & (t145))));
  t147 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(16))))));
  t148 = ((int32_t)(((int32_t)((uint64_t)(t146) << ((uint64_t)(t147) & 31)))));
  t149 = ((int32_t)(((t143) | (t148))));
  t150 = ((int32_t)(v42));
  t151 = ((int32_t)(v43));
  t152 = ((int32_t)(((t150) & (t151))));
  t153 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(24))))));
  t154 = ((int32_t)(((int32_t)((uint64_t)(t152) << ((uint64_t)(t153) & 31)))));
  v44 = ((int32_t)(((t149) | (t154))));
  t155 = ((float)(fg180qd_gg_double_bits(UINT64_C(4607182418800017408))));
  t156 = ((float)(v38));
  v45 = ((float)(((t155) / (t156))));
  t157 = ((int32_t)(v12));
  t158 = ((int32_t)(((int64_t)UINT64_C(0))));
  t159 = ((t157) != (t158));
  if (t159) {
  {
  v46 = ((int32_t)(v10));
  }
  } else {
  {
  v46 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  }
  }
  v47 = ((int32_t)(((int64_t)UINT64_C(0))));
label1:;
  t160 = ((int32_t)(v47));
  t161 = ((int32_t)(v46));
  if (!(((t160) < (t161)))) goto label2;
  {
  v48 = ((int64_t)(((int64_t)UINT64_C(0))));
  t162 = ((int32_t)(v12));
  t163 = ((int32_t)(((int64_t)UINT64_C(0))));
  t164 = ((t162) != (t163));
  if (t164) {
  {
  t165 = ((int64_t)(((int64_t)(v13))));
  t166 = ((int64_t)(((int64_t)(v10))));
  t167 = ((int64_t)(((int64_t)((uint64_t)(t165) * (uint64_t)(t166)))));
  t168 = ((int64_t)(((int64_t)(v47))));
  v49 = ((int64_t)(((int64_t)((uint64_t)(t167) + (uint64_t)(t168)))));
  t169 = ((uint64_t)(v1));
  t170 = ((int64_t)(v49));
  t171 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t172 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t170) * (uint64_t)(t171)))))));
  v50 = fg180qd_gg_function_5(t169, t172, base + 0);
  t173 = ((int64_t)(v28));
  t174 = ((int64_t)(v8));
  t175 = ((int64_t)(((int64_t)((uint64_t)(t173) * (uint64_t)(t174)))));
  t176 = ((int64_t)(v50));
  v48 = ((int64_t)(((int64_t)((uint64_t)(t175) + (uint64_t)(t176)))));
  }
  } else {
  {
  t177 = ((int32_t)(v18));
  t178 = ((int32_t)(v19));
  t179 = ((int32_t)(((int32_t)((uint64_t)(t177) * (uint64_t)(t178)))));
  t180 = ((int32_t)(v17));
  t181 = ((int32_t)(((int32_t)((uint64_t)(t179) * (uint64_t)(t180)))));
  t182 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(128))))));
  v51 = ((int32_t)(((int32_t)fg180qd_gg_div(t181, t182))));
  v52 = ((int64_t)(((int64_t)(v51))));
  t183 = ((int64_t)(((int64_t)(v15))));
  t184 = ((int64_t)(v52));
  v53 = ((int64_t)(((int64_t)((uint64_t)(t183) * (uint64_t)(t184)))));
  t185 = ((int64_t)(v53));
  t186 = ((int64_t)(v28));
  t187 = ((int64_t)(v8));
  t188 = ((int64_t)(((int64_t)((uint64_t)(t186) * (uint64_t)(t187)))));
  t189 = ((int64_t)(((int64_t)((uint64_t)(t185) + (uint64_t)(t188)))));
  t190 = ((int64_t)(((int64_t)(v13))));
  v48 = ((int64_t)(((int64_t)((uint64_t)(t189) + (uint64_t)(t190)))));
  }
  }
  t191 = ((int64_t)(v48));
  t192 = ((int64_t)(((int64_t)UINT64_C(0))));
  t193 = ((t191) >= (t192));
  t194 = ((int64_t)(((int64_t)UINT64_C(11))));
  fg180qd_gg_function_3(t193, t194, base + 0);
  t195 = ((int64_t)(v48));
  t196 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(144))))));
  v54 = ((int64_t)(((int64_t)((uint64_t)(t195) * (uint64_t)(t196)))));
  t197 = ((uint64_t)(v2));
  t198 = ((int64_t)(v54));
  t199 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
  t200 = ((int64_t)(((int64_t)((uint64_t)(t198) + (uint64_t)(t199)))));
  t201 = ((int64_t)(v29));
  t202 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t200) + (uint64_t)(t201)))))));
  t203 = ((int32_t)(v44));
  fg180q_st_i32(t197, t202, t203);
  t204 = ((int32_t)(v11));
  t205 = ((int32_t)(((int64_t)UINT64_C(2))));
  t206 = ((t204) == (t205));
  if (t206) {
  {
  t207 = ((int64_t)(v29));
  t208 = ((int64_t)(((int64_t)UINT64_C(16))));
  t209 = ((int64_t)(((int64_t)fg180qd_gg_mod(t207, t208))));
  t210 = ((int64_t)(((int64_t)UINT64_C(0))));
  t211 = ((t209) == (t210));
  t212 = t211;
  if (t212) {
  t213 = ((int64_t)(v29));
  t214 = ((int64_t)(((int64_t)UINT64_C(96))));
  t212 = ((t213) < (t214));
  }
  t215 = t212;
  if (t215) {
  {
  t216 = ((uint64_t)(v2));
  t217 = ((int64_t)(v54));
  t218 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t219 = ((int64_t)(((int64_t)((uint64_t)(t217) + (uint64_t)(t218)))));
  t220 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(2))))));
  t221 = ((int64_t)(v29));
  t222 = ((int64_t)(((int64_t)UINT64_C(16))));
  t223 = ((int64_t)(((int64_t)fg180qd_gg_div(t221, t222))));
  t224 = ((int64_t)(((int64_t)((uint64_t)(t220) + (uint64_t)(t223)))));
  t225 = ((int64_t)(((int64_t)UINT64_C(2))));
  t226 = ((int64_t)(((int64_t)((uint64_t)(t224) * (uint64_t)(t225)))));
  t227 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t219) + (uint64_t)(t226)))))));
  t228 = ((float)(v37));
  fg180q_st_f32(t216, t227, t228);
  t229 = ((int64_t)(v29));
  t230 = ((int64_t)(((int64_t)UINT64_C(64))));
  t231 = ((int64_t)(((int64_t)fg180qd_gg_mod(t229, t230))));
  t232 = ((int64_t)(((int64_t)UINT64_C(0))));
  t233 = ((t231) == (t232));
  if (t233) {
  {
  t234 = ((uint64_t)(v2));
  t235 = ((int64_t)(v54));
  t236 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t237 = ((int64_t)(((int64_t)((uint64_t)(t235) + (uint64_t)(t236)))));
  t238 = ((int64_t)(v29));
  t239 = ((int64_t)(((int64_t)UINT64_C(64))));
  t240 = ((int64_t)(((int64_t)fg180qd_gg_div(t238, t239))));
  t241 = ((int64_t)(((int64_t)UINT64_C(2))));
  t242 = ((int64_t)(((int64_t)((uint64_t)(t240) * (uint64_t)(t241)))));
  t243 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t237) + (uint64_t)(t242)))))));
  t244 = ((float)(v45));
  fg180q_st_f32(t234, t243, t244);
  }
  }
  }
  }
  }
  } else {
  {
  t245 = ((int64_t)(v29));
  t246 = ((int64_t)(((int64_t)UINT64_C(32))));
  t247 = ((int64_t)(((int64_t)fg180qd_gg_mod(t245, t246))));
  t248 = ((int64_t)(((int64_t)UINT64_C(0))));
  t249 = ((t247) == (t248));
  if (t249) {
  {
  t250 = ((int32_t)(v11));
  t251 = ((int32_t)(((int64_t)UINT64_C(1))));
  t252 = ((t250) == (t251));
  if (t252) {
  {
  t253 = ((uint64_t)(v2));
  t254 = ((int64_t)(v54));
  t255 = ((int64_t)(v29));
  t256 = ((int64_t)(((int64_t)UINT64_C(32))));
  t257 = ((int64_t)(((int64_t)fg180qd_gg_div(t255, t256))));
  t258 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t259 = ((int64_t)(((int64_t)((uint64_t)(t257) * (uint64_t)(t258)))));
  t260 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t254) + (uint64_t)(t259)))))));
  t261 = ((float)(v45));
  t262 = ((float)(v37));
  fg180q_st_half2(t253, t260, t261, t262);
  }
  } else {
  {
  t263 = ((uint64_t)(v2));
  t264 = ((int64_t)(v54));
  t265 = ((int64_t)(v29));
  t266 = ((int64_t)(((int64_t)UINT64_C(32))));
  t267 = ((int64_t)(((int64_t)fg180qd_gg_div(t265, t266))));
  t268 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t269 = ((int64_t)(((int64_t)((uint64_t)(t267) * (uint64_t)(t268)))));
  t270 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t264) + (uint64_t)(t269)))))));
  t271 = ((float)(v45));
  fg180q_st_f32(t263, t270, t271);
  }
  }
  }
  }
  }
  }
  t272 = v47;
  t273 = ((int32_t)(((int64_t)UINT64_C(1))));
  v47 = ((int32_t)((uint64_t)(t272) + (uint64_t)(t273)));
  }
  goto label1;
label2:;
  }
  }
  }
fg180qd_gg_done:;
}
static __device__ __forceinline__ void fg180qd_gg_function_22(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int32_t t10;
  int32_t t11;
  int32_t t12;
  int32_t t13;
  int32_t t14;
  int32_t t15;
  int32_t t16;
  int32_t t17;
  int32_t t18;
  int32_t t19;
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
  int32_t v10;
  int32_t v11;
  int32_t v12;
  int32_t v13;
  int32_t v14;
  int32_t v15;
  int32_t v16;
  int32_t v17;
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
  {
  t0 = ((uint64_t)(v0));
  t1 = ((uint64_t)(v1));
  t2 = ((uint64_t)(v2));
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(v5));
  t6 = ((int64_t)(v6));
  t7 = ((int64_t)(v7));
  t8 = ((int64_t)(v8));
  t9 = ((int64_t)(v9));
  t10 = ((int32_t)(v10));
  t11 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  t12 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  t13 = ((int32_t)(v11));
  t14 = ((int32_t)(v12));
  t15 = ((int32_t)(v13));
  t16 = ((int32_t)(v14));
  t17 = ((int32_t)(v15));
  t18 = ((int32_t)(v16));
  t19 = ((int32_t)(v17));
  fg180qd_gg_function_2(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, t19, base + 0);
  }
fg180qd_gg_done:;
}
static __device__ __forceinline__ void fg180qd_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int64_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, int32_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int32_t t10;
  int32_t t11;
  int32_t t12;
  int32_t t13;
  int32_t t14;
  int32_t t15;
  int32_t t16;
  int32_t t17;
  int32_t t18;
  int32_t t19;
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
  int32_t v10;
  int32_t v11;
  int32_t v12;
  int32_t v13;
  int32_t v14;
  int32_t v15;
  int32_t v16;
  int32_t v17;
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
  {
  t0 = ((uint64_t)(v0));
  t1 = ((uint64_t)(v1));
  t2 = ((uint64_t)(v2));
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(v4));
  t5 = ((int64_t)(v5));
  t6 = ((int64_t)(v6));
  t7 = ((int64_t)(v7));
  t8 = ((int64_t)(v8));
  t9 = ((int64_t)(v9));
  t10 = ((int32_t)(v10));
  t11 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(1))))));
  t12 = ((int32_t)(((int32_t)(((int64_t)UINT64_C(0))))));
  t13 = ((int32_t)(v11));
  t14 = ((int32_t)(v12));
  t15 = ((int32_t)(v13));
  t16 = ((int32_t)(v14));
  t17 = ((int32_t)(v15));
  t18 = ((int32_t)(v16));
  t19 = ((int32_t)(v17));
  fg180qd_gg_function_2(t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, t19, base + 0);
  }
fg180qd_gg_done:;
}
