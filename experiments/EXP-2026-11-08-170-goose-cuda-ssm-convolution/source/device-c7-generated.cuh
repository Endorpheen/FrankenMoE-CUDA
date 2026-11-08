#pragma once
#include <stdint.h>
#include <string.h>
typedef struct { void *data; int64_t len; } fg170s7_gg_slice;


#pragma pack(push, 1)
typedef struct { float data[7]; } fg170s7_gg_type_6;
#pragma pack(pop)
static __device__ __forceinline__ void fg170s7_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ fg170s7_gg_slice fg170s7_gg_view(void *p, int64_t n) { fg170s7_gg_slice v; v.data=p; v.len=n; return v; }
static __device__ __forceinline__ double fg170s7_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ int64_t fg170s7_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg170s7_gg_fail("array index out of bounds"); return i; }
static __device__ __forceinline__ int64_t fg170s7_gg_div(int64_t a, int64_t b) { if (!b) fg170s7_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg170s7_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg170s7_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg170s7_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg170s7_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg170s7_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg170s7_gg_umod(uint64_t a,uint64_t b) { if(!b)fg170s7_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ float fg170s7_gg_function_5(float p0, int64_t base);
static __device__ __forceinline__ void fg170s7_gg_function_8(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, int32_t p5, int32_t p6, int32_t p7, int32_t p8, int32_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, uint8_t p14, uint8_t p15, uint8_t p16, int32_t p17, int32_t p18, int32_t p19, int32_t p20, int64_t base);
static __device__ __forceinline__ void fg170s7_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int32_t p4, int32_t p5, int32_t p6, int32_t p7, int32_t p8, int32_t p9, int32_t p10, int32_t p11, int32_t p12, uint8_t p13, uint8_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base);
static __device__ __forceinline__ float fg170s7_gg_function_5(float p0, int64_t base) {
  float t0;
  float t1;
  float t2;
  float t3;
  float t4;
  float t5;
  float t6;
  float t7;
  float v0;
  v0 = p0;
  {
  t1 = ((float)(v0));
  t2 = ((float)(fg170s7_gg_double_bits(UINT64_C(4607182418800017408))));
  t3 = v0;
  t4 = ((float)((-(t3))));
  t5 = fg170s_expf(t4);
  t6 = ((float)(t5));
  t7 = ((float)(((t2) + (t6))));
  t0 = ((float)(((t1) / (t7))));
  goto fg170s7_gg_done;
  }
fg170s7_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg170s7_gg_function_8(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, int32_t p5, int32_t p6, int32_t p7, int32_t p8, int32_t p9, int32_t p10, int32_t p11, int32_t p12, int32_t p13, uint8_t p14, uint8_t p15, uint8_t p16, int32_t p17, int32_t p18, int32_t p19, int32_t p20, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  uint64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint64_t t6;
  uint64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint64_t t10;
  uint64_t t11;
  uint64_t t12;
  uint64_t t13;
  uint64_t t14;
  uint64_t t15;
  uint64_t t16;
  uint64_t t17;
  uint64_t t18;
  uint64_t t19;
  uint64_t t20;
  uint64_t t21;
  uint64_t t22;
  uint64_t t23;
  uint64_t t24;
  uint64_t t25;
  uint64_t t26;
  uint64_t t27;
  uint64_t t28;
  uint64_t t29;
  uint64_t t30;
  uint64_t t31;
  uint64_t t32;
  uint64_t t33;
  uint64_t t34;
  uint64_t t35;
  uint64_t t36;
  uint64_t t37;
  uint64_t t38;
  uint64_t t39;
  uint64_t t40;
  uint64_t t41;
  uint64_t t42;
  uint64_t t43;
  int32_t t44;
  int32_t t45;
  int32_t t46;
  int32_t t47;
  int32_t t48;
  int32_t t49;
  uint8_t t50;
  int32_t t51;
  int32_t t52;
  int32_t t53;
  int32_t t54;
  int32_t t55;
  int32_t t56;
  int32_t t57;
  int32_t t58;
  int32_t t59;
  int32_t t60;
  int32_t t61;
  int32_t t62;
  int32_t t63;
  int32_t t64;
  int32_t t65;
  int32_t t66;
  int32_t t67;
  int32_t t68;
  int32_t t69;
  int32_t t70;
  int32_t t71;
  int32_t t72;
  uint8_t t73;
  uint8_t t74;
  int32_t t75;
  int32_t t76;
  uint8_t t77;
  uint64_t t78;
  uint64_t t79;
  uint64_t t80;
  uint64_t t81;
  uint64_t t82;
  uint64_t t83;
  uint64_t t84;
  uint64_t t85;
  uint64_t t86;
  uint64_t t87;
  uint64_t t88;
  uint64_t t89;
  float t90;
  float t91;
  uint64_t t92;
  uint64_t t93;
  uint64_t t94;
  uint64_t t95;
  uint64_t t96;
  uint64_t t97;
  uint64_t t98;
  uint64_t t99;
  uint64_t t100;
  uint64_t t101;
  uint64_t t102;
  uint64_t t103;
  float t104;
  float t105;
  int32_t t106;
  int32_t t107;
  int32_t t108;
  int32_t t109;
  int32_t t110;
  int32_t t111;
  int32_t t112;
  int32_t t113;
  int32_t t114;
  int32_t t115;
  float t116;
  float t117;
  int64_t t118;
  int32_t t119;
  int32_t t120;
  int32_t t121;
  float * t122;
  uint64_t t123;
  uint64_t t124;
  uint64_t t125;
  uint64_t t126;
  uint64_t t127;
  uint64_t t128;
  float t129;
  int32_t t130;
  int32_t t131;
  uint8_t t132;
  uint64_t t133;
  uint64_t t134;
  uint64_t t135;
  uint64_t t136;
  uint64_t t137;
  uint64_t t138;
  float t139;
  int32_t t140;
  int32_t t141;
  int32_t t142;
  int32_t t143;
  float t144;
  uint64_t t145;
  uint64_t t146;
  uint64_t t147;
  uint64_t t148;
  int32_t t149;
  int32_t t150;
  uint64_t t151;
  uint64_t t152;
  float t153;
  float t154;
  int32_t t155;
  float * t156;
  float t157;
  float t158;
  int32_t t159;
  int32_t t160;
  float t161;
  float t162;
  uint8_t t163;
  float t164;
  uint64_t t165;
  uint64_t t166;
  uint64_t t167;
  uint64_t t168;
  uint64_t t169;
  uint64_t t170;
  float t171;
  int32_t t172;
  int32_t t173;
  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  uint64_t v4;
  int32_t v5;
  int32_t v6;
  int32_t v7;
  int32_t v8;
  int32_t v9;
  int32_t v10;
  int32_t v11;
  int32_t v12;
  int32_t v13;
  uint8_t v14;
  uint8_t v15;
  uint8_t v16;
  int32_t v17;
  int32_t v18;
  int32_t v19;
  int32_t v20;
  uint64_t v21;
  uint64_t v22;
  uint64_t v23;
  uint64_t v24;
  uint64_t v25;
  uint64_t v26;
  int32_t v27;
  int32_t v28;
  int32_t v29;
  int32_t v30;
  int32_t v31;
  int32_t v32;
  int32_t v33;
  fg170s7_gg_type_6 v34;
  int32_t v35;
  float v36;
  int32_t v37;
  float v38;
  float v39;
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
  {
  t0 = ((uint64_t)(v0));
  t1 = ((uint64_t)(((uint64_t)(v18))));
  t2 = ((uint64_t)(((uint64_t)(v7))));
  t3 = ((uint64_t)(((uint64_t)((uint64_t)(t1) * (uint64_t)(t2)))));
  t4 = ((uint64_t)(((uint64_t)((uint64_t)(t0) + (uint64_t)(t3)))));
  t5 = ((uint64_t)(((uint64_t)(v19))));
  t6 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(128))))));
  t7 = ((uint64_t)(((uint64_t)((uint64_t)(t5) * (uint64_t)(t6)))));
  t8 = ((uint64_t)(((uint64_t)(v6))));
  t9 = ((uint64_t)(((uint64_t)((uint64_t)(t7) * (uint64_t)(t8)))));
  t10 = ((uint64_t)(((uint64_t)((uint64_t)(t4) + (uint64_t)(t9)))));
  t11 = ((uint64_t)(((uint64_t)(v20))));
  t12 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(32))))));
  t13 = ((uint64_t)(((uint64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  t14 = ((uint64_t)(((uint64_t)(v5))));
  t15 = ((uint64_t)(((uint64_t)((uint64_t)(t13) * (uint64_t)(t14)))));
  v21 = ((uint64_t)(((uint64_t)((uint64_t)(t10) + (uint64_t)(t15)))));
  t16 = ((uint64_t)(v1));
  t17 = ((uint64_t)(((uint64_t)(v19))));
  t18 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(128))))));
  t19 = ((uint64_t)(((uint64_t)((uint64_t)(t17) * (uint64_t)(t18)))));
  t20 = ((uint64_t)(((uint64_t)(v8))));
  t21 = ((uint64_t)(((uint64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  v22 = ((uint64_t)(((uint64_t)((uint64_t)(t16) + (uint64_t)(t21)))));
  t22 = ((uint64_t)(v3));
  t23 = ((uint64_t)(((uint64_t)(v18))));
  t24 = ((uint64_t)(((uint64_t)(v11))));
  t25 = ((uint64_t)(((uint64_t)((uint64_t)(t23) * (uint64_t)(t24)))));
  t26 = ((uint64_t)(((uint64_t)((uint64_t)(t22) + (uint64_t)(t25)))));
  t27 = ((uint64_t)(((uint64_t)(v20))));
  t28 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(32))))));
  t29 = ((uint64_t)(((uint64_t)((uint64_t)(t27) * (uint64_t)(t28)))));
  t30 = ((uint64_t)(((uint64_t)(v10))));
  t31 = ((uint64_t)(((uint64_t)((uint64_t)(t29) * (uint64_t)(t30)))));
  t32 = ((uint64_t)(((uint64_t)((uint64_t)(t26) + (uint64_t)(t31)))));
  t33 = ((uint64_t)(((uint64_t)(v19))));
  t34 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(128))))));
  t35 = ((uint64_t)(((uint64_t)((uint64_t)(t33) * (uint64_t)(t34)))));
  t36 = ((uint64_t)(((uint64_t)(v9))));
  t37 = ((uint64_t)(((uint64_t)((uint64_t)(t35) * (uint64_t)(t36)))));
  v23 = ((uint64_t)(((uint64_t)((uint64_t)(t32) + (uint64_t)(t37)))));
  t38 = ((uint64_t)(((uint64_t)(v6))));
  t39 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v24 = ((uint64_t)(((uint64_t)fg170s7_gg_udiv(t38, t39))));
  t40 = ((uint64_t)(((uint64_t)(v8))));
  t41 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v25 = ((uint64_t)(((uint64_t)fg170s7_gg_udiv(t40, t41))));
  t42 = ((uint64_t)(((uint64_t)(v10))));
  t43 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v26 = ((uint64_t)(((uint64_t)fg170s7_gg_udiv(t42, t43))));
  t44 = ((int32_t)(v12));
  t45 = ((int32_t)(v20));
  t46 = ((int32_t)(((int64_t)UINT64_C(32))));
  t47 = ((int32_t)(((int32_t)((uint64_t)(t45) * (uint64_t)(t46)))));
  t48 = ((int32_t)(((int32_t)((uint64_t)(t44) - (uint64_t)(t47)))));
  t49 = ((int32_t)(((int64_t)UINT64_C(32))));
  t50 = ((t48) < (t49));
  if (t50) {
  {
  t51 = ((int32_t)(v12));
  t52 = ((int32_t)(v20));
  t53 = ((int32_t)(((int64_t)UINT64_C(32))));
  t54 = ((int32_t)(((int32_t)((uint64_t)(t52) * (uint64_t)(t53)))));
  v27 = ((int32_t)(((int32_t)((uint64_t)(t51) - (uint64_t)(t54)))));
  }
  } else {
  {
  v27 = ((int32_t)(((int64_t)UINT64_C(32))));
  }
  }
  t55 = ((int32_t)(v13));
  t56 = ((int32_t)(((int64_t)UINT64_C(1))));
  t57 = ((int32_t)(((int32_t)((uint64_t)(t55) - (uint64_t)(t56)))));
  t58 = ((int32_t)(((int64_t)UINT64_C(32))));
  v28 = ((int32_t)(((int32_t)((uint64_t)(t57) + (uint64_t)(t58)))));
  t59 = ((int32_t)(((int64_t)UINT64_C(128))));
  t60 = ((int32_t)(v28));
  v29 = ((int32_t)(((int32_t)((uint64_t)(t59) * (uint64_t)(t60)))));
  t61 = ((int32_t)(v27));
  t62 = ((int32_t)(v13));
  t63 = ((int32_t)(((int32_t)((uint64_t)(t61) + (uint64_t)(t62)))));
  t64 = ((int32_t)(((int64_t)UINT64_C(1))));
  v30 = ((int32_t)(((int32_t)((uint64_t)(t63) - (uint64_t)(t64)))));
  t65 = ((int32_t)(v17));
  t66 = ((int32_t)(v28));
  v31 = ((int32_t)(((int32_t)fg170s7_gg_div(t65, t66))));
  t67 = ((int32_t)(v17));
  t68 = ((int32_t)(v28));
  v32 = ((int32_t)(((int32_t)fg170s7_gg_mod(t67, t68))));
  v33 = ((int32_t)(((int64_t)UINT64_C(0))));
label1:;
  t69 = ((int32_t)(v33));
  t70 = ((int32_t)(v29));
  if (!(((t69) < (t70)))) goto label2;
  {
  t71 = ((int32_t)(v31));
  t72 = ((int32_t)(((int64_t)UINT64_C(128))));
  t73 = ((t71) < (t72));
  if (t73) {
  {
  t74 = v16;
  if (t74) {
  {
  t75 = ((int32_t)(v32));
  t76 = ((int32_t)(v30));
  t77 = ((t75) < (t76));
  if (t77) {
  {
  t78 = ((uint64_t)(v4));
  t79 = ((uint64_t)(((uint64_t)(v31))));
  t80 = ((uint64_t)(((uint64_t)(v28))));
  t81 = ((uint64_t)(((uint64_t)((uint64_t)(t79) * (uint64_t)(t80)))));
  t82 = ((uint64_t)(((uint64_t)(v32))));
  t83 = ((uint64_t)(((uint64_t)((uint64_t)(t81) + (uint64_t)(t82)))));
  t84 = ((uint64_t)(v21));
  t85 = ((uint64_t)(((uint64_t)(v31))));
  t86 = ((uint64_t)(v24));
  t87 = ((uint64_t)(((uint64_t)((uint64_t)(t85) * (uint64_t)(t86)))));
  t88 = ((uint64_t)(((uint64_t)(v32))));
  t89 = ((uint64_t)(((uint64_t)((uint64_t)(t87) + (uint64_t)(t88)))));
  t90 = fg170s_load(t84, t89);
  t91 = ((float)(t90));
  fg170s_store(t78, t83, t91);
  }
  }
  }
  } else {
  {
  t92 = ((uint64_t)(v4));
  t93 = ((uint64_t)(((uint64_t)(v31))));
  t94 = ((uint64_t)(((uint64_t)(v28))));
  t95 = ((uint64_t)(((uint64_t)((uint64_t)(t93) * (uint64_t)(t94)))));
  t96 = ((uint64_t)(((uint64_t)(v32))));
  t97 = ((uint64_t)(((uint64_t)((uint64_t)(t95) + (uint64_t)(t96)))));
  t98 = ((uint64_t)(v21));
  t99 = ((uint64_t)(((uint64_t)(v31))));
  t100 = ((uint64_t)(v24));
  t101 = ((uint64_t)(((uint64_t)((uint64_t)(t99) * (uint64_t)(t100)))));
  t102 = ((uint64_t)(((uint64_t)(v32))));
  t103 = ((uint64_t)(((uint64_t)((uint64_t)(t101) + (uint64_t)(t102)))));
  t104 = fg170s_load(t98, t103);
  t105 = ((float)(t104));
  fg170s_store(t92, t97, t105);
  }
  }
  }
  }
  t106 = v32;
  t107 = ((int32_t)(((int64_t)UINT64_C(128))));
  v32 = ((int32_t)((uint64_t)(t106) + (uint64_t)(t107)));
  t108 = v31;
  t109 = ((int32_t)(v32));
  t110 = ((int32_t)(v28));
  t111 = ((int32_t)(((int32_t)fg170s7_gg_div(t109, t110))));
  v31 = ((int32_t)((uint64_t)(t108) + (uint64_t)(t111)));
  t112 = ((int32_t)(v32));
  t113 = ((int32_t)(v28));
  v32 = ((int32_t)(((int32_t)fg170s7_gg_mod(t112, t113))));
  t114 = v33;
  t115 = ((int32_t)(((int64_t)UINT64_C(128))));
  v33 = ((int32_t)((uint64_t)(t114) + (uint64_t)(t115)));
  }
  goto label1;
label2:;
  fg170s_barrier();
  memset(&t116, 0, sizeof(t116));
  t117 = ((float)(t116));
  for (t118=0; t118<7; t118++) (v34).data[t118] = t117;
  v35 = ((int32_t)(((int64_t)UINT64_C(0))));
label4:;
  t119 = ((int32_t)(v35));
  t120 = ((int32_t)(v13));
  if (!(((t119) < (t120)))) goto label5;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t121 = v35;
  t122 = ((float *)(v34).data) + fg170s7_gg_index(t121, ((int64_t)UINT64_C(7)));
  t123 = ((uint64_t)(v22));
  t124 = ((uint64_t)(((uint64_t)(v17))));
  t125 = ((uint64_t)(v25));
  t126 = ((uint64_t)(((uint64_t)((uint64_t)(t124) * (uint64_t)(t125)))));
  t127 = ((uint64_t)(((uint64_t)(v35))));
  t128 = ((uint64_t)(((uint64_t)((uint64_t)(t126) + (uint64_t)(t127)))));
  t129 = fg170s_load(t123, t128);
  (*t122) = ((float)(t129));
  t130 = v35;
  t131 = ((int32_t)(((int64_t)UINT64_C(1))));
  v35 = ((int32_t)((uint64_t)(t130) + (uint64_t)(t131)));
  }
  goto label4;
label5:;
  v36 = ((float)(fg170s7_gg_double_bits(UINT64_C(0))));
  t132 = v15;
  if (t132) {
  {
  t133 = ((uint64_t)(v2));
  t134 = ((uint64_t)(((uint64_t)(v19))));
  t135 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(128))))));
  t136 = ((uint64_t)(((uint64_t)((uint64_t)(t134) * (uint64_t)(t135)))));
  t137 = ((uint64_t)(((uint64_t)(v17))));
  t138 = ((uint64_t)(((uint64_t)((uint64_t)(t136) + (uint64_t)(t137)))));
  t139 = fg170s_load(t133, t138);
  v36 = ((float)(t139));
  }
  }
  v37 = ((int32_t)(((int64_t)UINT64_C(0))));
label7:;
  t140 = ((int32_t)(v37));
  t141 = ((int32_t)(v27));
  if (!(((t140) < (t141)))) goto label8;
  {
  v38 = ((float)(fg170s7_gg_double_bits(UINT64_C(0))));
  v35 = ((int32_t)(((int64_t)UINT64_C(0))));
label10:;
  t142 = ((int32_t)(v35));
  t143 = ((int32_t)(v13));
  if (!(((t142) < (t143)))) goto label11;
  {
  t144 = v38;
  t145 = ((uint64_t)(v4));
  t146 = ((uint64_t)(((uint64_t)(v17))));
  t147 = ((uint64_t)(((uint64_t)(v28))));
  t148 = ((uint64_t)(((uint64_t)((uint64_t)(t146) * (uint64_t)(t147)))));
  t149 = ((int32_t)(v37));
  t150 = ((int32_t)(v35));
  t151 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t149) + (uint64_t)(t150)))))));
  t152 = ((uint64_t)(((uint64_t)((uint64_t)(t148) + (uint64_t)(t151)))));
  t153 = fg170s_load(t145, t152);
  t154 = ((float)(t153));
  /* FG_DIRECT_SLICE_INDEX */
  t155 = v35;
  t156 = ((float *)(v34).data) + fg170s7_gg_index(t155, ((int64_t)UINT64_C(7)));
  t157 = ((float)((*t156)));
  t158 = ((float)(((t154) * (t157))));
  v38 = ((t144) + (t158));
  t159 = v35;
  t160 = ((int32_t)(((int64_t)UINT64_C(1))));
  v35 = ((int32_t)((uint64_t)(t159) + (uint64_t)(t160)));
  }
  goto label10;
label11:;
  t161 = v38;
  t162 = ((float)(v36));
  v38 = ((t161) + (t162));
  t163 = v14;
  if (t163) {
  {
  t164 = ((float)(v38));
  v39 = fg170s7_gg_function_5(t164, base + 0);
  }
  } else {
  {
  v39 = ((float)(v38));
  }
  }
  t165 = ((uint64_t)(v23));
  t166 = ((uint64_t)(((uint64_t)(v37))));
  t167 = ((uint64_t)(v26));
  t168 = ((uint64_t)(((uint64_t)((uint64_t)(t166) * (uint64_t)(t167)))));
  t169 = ((uint64_t)(((uint64_t)(v17))));
  t170 = ((uint64_t)(((uint64_t)((uint64_t)(t168) + (uint64_t)(t169)))));
  t171 = ((float)(v39));
  fg170s_store(t165, t170, t171);
  t172 = v37;
  t173 = ((int32_t)(((int64_t)UINT64_C(1))));
  v37 = ((int32_t)((uint64_t)(t172) + (uint64_t)(t173)));
  }
  goto label7;
label8:;
  }
fg170s7_gg_done:;
}
static __device__ __forceinline__ void fg170s7_gg_function_1(uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, int32_t p4, int32_t p5, int32_t p6, int32_t p7, int32_t p8, int32_t p9, int32_t p10, int32_t p11, int32_t p12, uint8_t p13, uint8_t p14, int32_t p15, int32_t p16, int32_t p17, int64_t base) {
  uint64_t t0;
  uint64_t t1;
  uint64_t t2;
  uint64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint64_t t6;
  uint64_t t7;
  uint64_t t8;
  uint64_t t9;
  uint64_t t10;
  uint64_t t11;
  uint64_t t12;
  uint64_t t13;
  uint64_t t14;
  uint64_t t15;
  uint64_t t16;
  uint64_t t17;
  uint64_t t18;
  uint64_t t19;
  uint64_t t20;
  uint64_t t21;
  uint64_t t22;
  uint64_t t23;
  uint64_t t24;
  uint64_t t25;
  uint64_t t26;
  uint64_t t27;
  uint64_t t28;
  uint64_t t29;
  uint64_t t30;
  uint64_t t31;
  float t32;
  float t33;
  int64_t t34;
  float t35;
  float t36;
  int64_t t37;
  int32_t t38;
  int32_t t39;
  int32_t t40;
  float * t41;
  uint64_t t42;
  uint64_t t43;
  uint64_t t44;
  uint64_t t45;
  uint64_t t46;
  uint64_t t47;
  float t48;
  int32_t t49;
  int32_t t50;
  uint8_t t51;
  uint64_t t52;
  uint64_t t53;
  uint64_t t54;
  uint64_t t55;
  uint64_t t56;
  uint64_t t57;
  float t58;
  int32_t t59;
  int32_t t60;
  int32_t t61;
  int32_t t62;
  uint8_t t63;
  int32_t t64;
  int32_t t65;
  int32_t t66;
  float * t67;
  uint64_t t68;
  uint64_t t69;
  uint64_t t70;
  uint64_t t71;
  uint64_t t72;
  uint64_t t73;
  float t74;
  int32_t t75;
  int32_t t76;
  fg170s7_gg_slice t77;
  int32_t t78;
  int32_t t79;
  int32_t t80;
  int32_t t81;
  int32_t t82;
  float * t83;
  uint64_t t84;
  uint64_t t85;
  uint64_t t86;
  uint64_t t87;
  int32_t t88;
  int32_t t89;
  int32_t t90;
  int32_t t91;
  uint64_t t92;
  uint64_t t93;
  float t94;
  int32_t t95;
  int32_t t96;
  float t97;
  fg170s7_gg_slice t98;
  int32_t t99;
  int32_t t100;
  int32_t t101;
  int32_t t102;
  int32_t t103;
  float * t104;
  float t105;
  int32_t t106;
  float * t107;
  float t108;
  float t109;
  int32_t t110;
  int32_t t111;
  float t112;
  float t113;
  uint8_t t114;
  float t115;
  uint64_t t116;
  uint64_t t117;
  uint64_t t118;
  uint64_t t119;
  uint64_t t120;
  uint64_t t121;
  float t122;
  int32_t t123;
  int32_t t124;
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
  uint8_t v13;
  uint8_t v14;
  int32_t v15;
  int32_t v16;
  int32_t v17;
  uint64_t v18;
  uint64_t v19;
  uint64_t v20;
  uint64_t v21;
  uint64_t v22;
  uint64_t v23;
  fg170s7_gg_type_6 v24;
  fg170s7_gg_type_6 v25;
  int32_t v26;
  float v27;
  int32_t v28;
  float v29;
  float v30;
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
  fg170s_lc();
  t0 = ((uint64_t)(v0));
  t1 = ((uint64_t)(((uint64_t)(v16))));
  t2 = ((uint64_t)(((uint64_t)(v6))));
  t3 = ((uint64_t)(((uint64_t)((uint64_t)(t1) * (uint64_t)(t2)))));
  t4 = ((uint64_t)(((uint64_t)((uint64_t)(t0) + (uint64_t)(t3)))));
  t5 = ((uint64_t)(((uint64_t)(v17))));
  t6 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(128))))));
  t7 = ((uint64_t)(((uint64_t)((uint64_t)(t5) * (uint64_t)(t6)))));
  t8 = ((uint64_t)(((uint64_t)(v5))));
  t9 = ((uint64_t)(((uint64_t)((uint64_t)(t7) * (uint64_t)(t8)))));
  v18 = ((uint64_t)(((uint64_t)((uint64_t)(t4) + (uint64_t)(t9)))));
  t10 = ((uint64_t)(v1));
  t11 = ((uint64_t)(((uint64_t)(v17))));
  t12 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(128))))));
  t13 = ((uint64_t)(((uint64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  t14 = ((uint64_t)(((uint64_t)(v7))));
  t15 = ((uint64_t)(((uint64_t)((uint64_t)(t13) * (uint64_t)(t14)))));
  v19 = ((uint64_t)(((uint64_t)((uint64_t)(t10) + (uint64_t)(t15)))));
  t16 = ((uint64_t)(v3));
  t17 = ((uint64_t)(((uint64_t)(v16))));
  t18 = ((uint64_t)(((uint64_t)(v10))));
  t19 = ((uint64_t)(((uint64_t)((uint64_t)(t17) * (uint64_t)(t18)))));
  t20 = ((uint64_t)(((uint64_t)((uint64_t)(t16) + (uint64_t)(t19)))));
  t21 = ((uint64_t)(((uint64_t)(v17))));
  t22 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(128))))));
  t23 = ((uint64_t)(((uint64_t)((uint64_t)(t21) * (uint64_t)(t22)))));
  t24 = ((uint64_t)(((uint64_t)(v8))));
  t25 = ((uint64_t)(((uint64_t)((uint64_t)(t23) * (uint64_t)(t24)))));
  v20 = ((uint64_t)(((uint64_t)((uint64_t)(t20) + (uint64_t)(t25)))));
  t26 = ((uint64_t)(((uint64_t)(v5))));
  t27 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v21 = ((uint64_t)(((uint64_t)fg170s7_gg_udiv(t26, t27))));
  t28 = ((uint64_t)(((uint64_t)(v7))));
  t29 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v22 = ((uint64_t)(((uint64_t)fg170s7_gg_udiv(t28, t29))));
  t30 = ((uint64_t)(((uint64_t)(v9))));
  t31 = ((uint64_t)(((int64_t)UINT64_C(4))));
  v23 = ((uint64_t)(((uint64_t)fg170s7_gg_udiv(t30, t31))));
  memset(&t32, 0, sizeof(t32));
  t33 = ((float)(t32));
  for (t34=0; t34<7; t34++) (v24).data[t34] = t33;
  memset(&t35, 0, sizeof(t35));
  t36 = ((float)(t35));
  for (t37=0; t37<7; t37++) (v25).data[t37] = t36;
  fg170s_sync();
  v26 = ((int32_t)(((int64_t)UINT64_C(0))));
label1:;
  t38 = ((int32_t)(v26));
  t39 = ((int32_t)(v12));
  if (!(((t38) < (t39)))) goto label2;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t40 = v26;
  t41 = ((float *)(v25).data) + fg170s7_gg_index(t40, ((int64_t)UINT64_C(7)));
  t42 = ((uint64_t)(v19));
  t43 = ((uint64_t)(((uint64_t)(v15))));
  t44 = ((uint64_t)(v22));
  t45 = ((uint64_t)(((uint64_t)((uint64_t)(t43) * (uint64_t)(t44)))));
  t46 = ((uint64_t)(((uint64_t)(v26))));
  t47 = ((uint64_t)(((uint64_t)((uint64_t)(t45) + (uint64_t)(t46)))));
  t48 = fg170s_load(t42, t47);
  (*t41) = ((float)(t48));
  t49 = v26;
  t50 = ((int32_t)(((int64_t)UINT64_C(1))));
  v26 = ((int32_t)((uint64_t)(t49) + (uint64_t)(t50)));
  }
  goto label1;
label2:;
  v27 = ((float)(fg170s7_gg_double_bits(UINT64_C(0))));
  t51 = v14;
  if (t51) {
  {
  t52 = ((uint64_t)(v2));
  t53 = ((uint64_t)(((uint64_t)(v17))));
  t54 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(128))))));
  t55 = ((uint64_t)(((uint64_t)((uint64_t)(t53) * (uint64_t)(t54)))));
  t56 = ((uint64_t)(((uint64_t)(v15))));
  t57 = ((uint64_t)(((uint64_t)((uint64_t)(t55) + (uint64_t)(t56)))));
  t58 = fg170s_load(t52, t57);
  v27 = ((float)(t58));
  }
  }
  v28 = ((int32_t)(((int64_t)UINT64_C(0))));
label4:;
  t59 = ((int32_t)(v28));
  t60 = ((int32_t)(v11));
  if (!(((t59) < (t60)))) goto label5;
  {
  v29 = ((float)(fg170s7_gg_double_bits(UINT64_C(0))));
  t61 = ((int32_t)(v28));
  t62 = ((int32_t)(((int64_t)UINT64_C(0))));
  t63 = ((t61) == (t62));
  if (t63) {
  {
  v26 = ((int32_t)(((int64_t)UINT64_C(0))));
label7:;
  t64 = ((int32_t)(v26));
  t65 = ((int32_t)(v12));
  if (!(((t64) < (t65)))) goto label8;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t66 = v26;
  t67 = ((float *)(v24).data) + fg170s7_gg_index(t66, ((int64_t)UINT64_C(7)));
  t68 = ((uint64_t)(v18));
  t69 = ((uint64_t)(((uint64_t)(v15))));
  t70 = ((uint64_t)(v21));
  t71 = ((uint64_t)(((uint64_t)((uint64_t)(t69) * (uint64_t)(t70)))));
  t72 = ((uint64_t)(((uint64_t)(v26))));
  t73 = ((uint64_t)(((uint64_t)((uint64_t)(t71) + (uint64_t)(t72)))));
  t74 = fg170s_load(t68, t73);
  (*t67) = ((float)(t74));
  t75 = v26;
  t76 = ((int32_t)(((int64_t)UINT64_C(1))));
  v26 = ((int32_t)((uint64_t)(t75) + (uint64_t)(t76)));
  }
  goto label7;
label8:;
  }
  } else {
  {
  t77 = fg170s7_gg_view((v24).data, 7);
  t78 = ((int32_t)(v28));
  t79 = ((int32_t)(((int64_t)UINT64_C(1))));
  t80 = ((int32_t)(((int32_t)((uint64_t)(t78) - (uint64_t)(t79)))));
  t81 = ((int32_t)(v12));
  t82 = ((int32_t)fg170s7_gg_mod(t80, t81));
  t83 = ((float *)t77.data) + fg170s7_gg_index(t82, t77.len);
  t84 = ((uint64_t)(v18));
  t85 = ((uint64_t)(((uint64_t)(v15))));
  t86 = ((uint64_t)(v21));
  t87 = ((uint64_t)(((uint64_t)((uint64_t)(t85) * (uint64_t)(t86)))));
  t88 = ((int32_t)(v28));
  t89 = ((int32_t)(v12));
  t90 = ((int32_t)(((int32_t)((uint64_t)(t88) + (uint64_t)(t89)))));
  t91 = ((int32_t)(((int64_t)UINT64_C(1))));
  t92 = ((uint64_t)(((uint64_t)(((int32_t)((uint64_t)(t90) - (uint64_t)(t91)))))));
  t93 = ((uint64_t)(((uint64_t)((uint64_t)(t87) + (uint64_t)(t92)))));
  t94 = fg170s_load(t84, t93);
  (*t83) = ((float)(t94));
  }
  }
  v26 = ((int32_t)(((int64_t)UINT64_C(0))));
label10:;
  t95 = ((int32_t)(v26));
  t96 = ((int32_t)(v12));
  if (!(((t95) < (t96)))) goto label11;
  {
  t97 = v29;
  t98 = fg170s7_gg_view((v24).data, 7);
  t99 = ((int32_t)(v28));
  t100 = ((int32_t)(v26));
  t101 = ((int32_t)(((int32_t)((uint64_t)(t99) + (uint64_t)(t100)))));
  t102 = ((int32_t)(v12));
  t103 = ((int32_t)fg170s7_gg_mod(t101, t102));
  t104 = ((float *)t98.data) + fg170s7_gg_index(t103, t98.len);
  t105 = ((float)((*t104)));
  /* FG_DIRECT_SLICE_INDEX */
  t106 = v26;
  t107 = ((float *)(v25).data) + fg170s7_gg_index(t106, ((int64_t)UINT64_C(7)));
  t108 = ((float)((*t107)));
  t109 = ((float)(((t105) * (t108))));
  v29 = ((t97) + (t109));
  t110 = v26;
  t111 = ((int32_t)(((int64_t)UINT64_C(1))));
  v26 = ((int32_t)((uint64_t)(t110) + (uint64_t)(t111)));
  }
  goto label10;
label11:;
  t112 = v29;
  t113 = ((float)(v27));
  v29 = ((t112) + (t113));
  t114 = v13;
  if (t114) {
  {
  t115 = ((float)(v29));
  v30 = fg170s7_gg_function_5(t115, base + 0);
  }
  } else {
  {
  v30 = ((float)(v29));
  }
  }
  t116 = ((uint64_t)(v20));
  t117 = ((uint64_t)(((uint64_t)(v28))));
  t118 = ((uint64_t)(v23));
  t119 = ((uint64_t)(((uint64_t)((uint64_t)(t117) * (uint64_t)(t118)))));
  t120 = ((uint64_t)(((uint64_t)(v15))));
  t121 = ((uint64_t)(((uint64_t)((uint64_t)(t119) + (uint64_t)(t120)))));
  t122 = ((float)(v30));
  fg170s_store(t116, t121, t122);
  t123 = v28;
  t124 = ((int32_t)(((int64_t)UINT64_C(1))));
  v28 = ((int32_t)((uint64_t)(t123) + (uint64_t)(t124)));
  }
  goto label4;
label5:;
  }
fg170s7_gg_done:;
}
