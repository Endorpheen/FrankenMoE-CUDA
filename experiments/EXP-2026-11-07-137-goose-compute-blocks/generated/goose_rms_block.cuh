#pragma once
#include <stdint.h>
#include <limits.h>
#include <string.h>
typedef struct { void *data; int64_t len; } gg_slice;
static __device__ __forceinline__ void gg_fail(const char *) { asm volatile("trap;"); }
typedef struct gg_type_10 gg_type_10;
#pragma pack(push, 1)
struct gg_type_10 {
  float f0;
  float f1;
  float f2;
  float f3;
};
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_10 data[4]; } gg_type_11;
#pragma pack(pop)
static __device__ __forceinline__ int64_t gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) gg_fail("array index out of bounds"); return i; }
static __device__ __forceinline__ int64_t gg_div(int64_t a, int64_t b) { if (!b) gg_fail("division by zero"); if (a==INT64_MIN && b==-1) gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ double gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static_assert(sizeof(gg_type_10)==sizeof(float4), "F4 ABI");
static __device__ __forceinline__ gg_type_10 fg_rms_load4(gg_slice x, int32_t i) {
    const float4 v = ((const float4 *)x.data)[i];
    return {v.x,v.y,v.z,v.w};
}
static __device__ __forceinline__ void fg_rms_store4(gg_slice x, int32_t i, gg_type_10 v) {
    ((float4 *)x.data)[i] = make_float4(v.f0,v.f1,v.f2,v.f3);
}
static __device__ __forceinline__ float fg_rms_shuffle(float x, int32_t offset) { return __shfl_xor_sync(0xffffffff,x,offset,32); }
static __device__ __forceinline__ void fg_rms_barrier() { __syncthreads(); }
static __device__ __forceinline__ float fg_rms_rsqrt(float x) { return rsqrtf(x); }
static __device__ __forceinline__ float fg_rms_fma(float x,float y,float z) { return fmaf(x,y,z); }
static __device__ __forceinline__ float gg_function_4(float p0, int64_t base) {
  float t0;
  int32_t t1;
  int32_t t2;
  float t3;
  float t4;
  int32_t t5;
  float t6;
  float t7;
  int32_t t8;
  int32_t t9;
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
  t3 = v1;
  t4 = ((float)(v1));
  t5 = ((int32_t)(v2));
  t6 = fg_rms_shuffle(t4, t5);
  t7 = ((float)(t6));
  v1 = ((t3) + (t7));
  t8 = v2;
  t9 = ((int32_t)(((int64_t)UINT64_C(1))));
  v2 = ((t8) >> ((uint64_t)(t9) & 31));
  }
  goto label1;
label2:;
  t0 = ((float)(v1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __device__ __forceinline__ float gg_function_3(float p0, gg_slice p1, int32_t p2, int32_t p3, int64_t base) {
  float t0;
  float t1;
  int32_t t2;
  int32_t t3;
  uint8_t t4;
  int32_t t5;
  int32_t t6;
  int32_t t7;
  int32_t t8;
  uint8_t t9;
  gg_slice t10;
  int32_t t11;
  int32_t t12;
  int32_t t13;
  float * t14;
  int32_t t15;
  int32_t t16;
  int32_t t17;
  int32_t t18;
  uint8_t t19;
  int32_t t20;
  float * t21;
  float t22;
  float v0;
  gg_slice v1;
  int32_t v2;
  int32_t v3;
  float v4;
  int32_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((float)(v0));
  v4 = gg_function_4(t1, base + 0);
  t2 = ((int32_t)(v3));
  t3 = ((int32_t)(((int64_t)UINT64_C(32))));
  t4 = ((t2) > (t3));
  if (t4) {
  {
  t5 = ((int32_t)(v2));
  t6 = ((int32_t)(((int64_t)UINT64_C(32))));
  v5 = ((int32_t)(((int32_t)gg_mod(t5, t6))));
  t7 = ((int32_t)(v5));
  t8 = ((int32_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t10 = v1;
  t11 = ((int32_t)(v2));
  t12 = ((int32_t)(((int64_t)UINT64_C(32))));
  t13 = ((int32_t)gg_div(t11, t12));
  t14 = ((float *)t10.data) + gg_index(t13, t10.len);
  (*t14) = ((float)(v4));
  }
  }
  fg_rms_barrier();
  v4 = ((float)(gg_double_bits(UINT64_C(0))));
  t15 = ((int32_t)(v5));
  t16 = ((int32_t)(v3));
  t17 = ((int32_t)(((int64_t)UINT64_C(32))));
  t18 = ((int32_t)(((int32_t)gg_div(t16, t17))));
  t19 = ((t15) < (t18));
  if (t19) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t20 = v5;
  t21 = ((float *)(v1).data) + gg_index(t20, (v1).len);
  v4 = ((float)((*t21)));
  }
  }
  t22 = ((float)(v4));
  v4 = gg_function_4(t22, base + 0);
  }
  }
  t0 = ((float)(v4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __device__ __forceinline__ void gg_function_1(gg_slice p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, int32_t p5, int32_t p6, int32_t p7, int32_t p8, uint8_t p9, uint8_t p10, uint8_t p11, uint8_t p12, int32_t p13, int32_t p14, float p15, float p16, int64_t base) {
  uint8_t t0;
  gg_type_10 t1;
  float t2;
  float t3;
  float t4;
  float t5;
  gg_type_10 t6;
  int64_t t7;
  int32_t t8;
  int32_t t9;
  int32_t t10;
  int32_t t11;
  int32_t t12;
  int32_t t13;
  int32_t t14;
  int32_t t15;
  int32_t t16;
  int32_t t17;
  uint8_t t18;
  int32_t t19;
  gg_type_10 * t20;
  gg_slice t21;
  int32_t t22;
  gg_type_10 t23;
  int32_t t24;
  gg_type_10 * t25;
  float t26;
  float t27;
  float t28;
  float t29;
  float t30;
  float t31;
  float t32;
  float t33;
  float t34;
  float t35;
  float t36;
  float t37;
  float t38;
  float t39;
  float t40;
  float t41;
  int32_t t42;
  int32_t t43;
  float t44;
  gg_slice t45;
  int32_t t46;
  int32_t t47;
  float t48;
  float t49;
  float t50;
  float t51;
  float t52;
  float t53;
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
  uint8_t t64;
  int32_t t65;
  gg_type_10 * t66;
  float t67;
  float t68;
  float t69;
  float t70;
  float t71;
  float t72;
  float t73;
  float t74;
  uint8_t t75;
  gg_slice t76;
  int32_t t77;
  gg_type_10 t78;
  float t79;
  float t80;
  float t81;
  float t82;
  float t83;
  float t84;
  float t85;
  float t86;
  uint8_t t87;
  gg_slice t88;
  int32_t t89;
  gg_type_10 t90;
  float t91;
  float t92;
  float t93;
  float t94;
  float t95;
  float t96;
  float t97;
  float t98;
  uint8_t t99;
  float t100;
  float t101;
  float t102;
  float t103;
  float t104;
  float t105;
  float t106;
  float t107;
  gg_slice t108;
  int32_t t109;
  gg_type_10 t110;
  int32_t t111;
  int32_t t112;
  int32_t t113;
  int32_t t114;
  int32_t t115;
  float * t116;
  float t117;
  float t118;
  float t119;
  float t120;
  int32_t t121;
  int32_t t122;
  float t123;
  gg_slice t124;
  int32_t t125;
  int32_t t126;
  float t127;
  float t128;
  float t129;
  float t130;
  float t131;
  float t132;
  int32_t t133;
  int32_t t134;
  uint8_t t135;
  uint8_t t136;
  int32_t t137;
  float * t138;
  float t139;
  int32_t t140;
  float * t141;
  float t142;
  float t143;
  gg_slice t144;
  int32_t t145;
  int32_t t146;
  int32_t t147;
  float * t148;
  float t149;
  gg_slice t150;
  int32_t t151;
  int32_t t152;
  int32_t t153;
  float * t154;
  float t155;
  float t156;
  int32_t t157;
  float * t158;
  float t159;
  int32_t t160;
  float * t161;
  float t162;
  float t163;
  gg_slice t164;
  int32_t t165;
  int32_t t166;
  int32_t t167;
  float * t168;
  float t169;
  uint8_t t170;
  int32_t t171;
  float * t172;
  float t173;
  float t174;
  int32_t t175;
  float * t176;
  float t177;
  float t178;
  int32_t t179;
  float * t180;
  float t181;
  int32_t t182;
  float * t183;
  float t184;
  int32_t t185;
  int32_t t186;
  int32_t t187;
  int32_t t188;
  uint8_t t189;
  gg_slice v0;
  gg_slice v1;
  gg_slice v2;
  gg_slice v3;
  gg_slice v4;
  int32_t v5;
  int32_t v6;
  int32_t v7;
  int32_t v8;
  uint8_t v9;
  uint8_t v10;
  uint8_t v11;
  uint8_t v12;
  int32_t v13;
  int32_t v14;
  float v15;
  float v16;
  float v17;
  gg_type_11 v18;
  int32_t v19;
  int32_t v20;
  gg_type_10 v21;
  float v22;
  int32_t v23;
  gg_type_10 v24;
  gg_type_10 v25;
  gg_type_10 v26;
  gg_type_10 v27;
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
  {
  v17 = ((float)(gg_double_bits(UINT64_C(0))));
  t0 = v9;
  if (t0) {
  {
  memset(&(t1), 0, sizeof(t1));
  memset(&t2, 0, sizeof(t2));
  (t1).f0 = ((float)(t2));
  memset(&t3, 0, sizeof(t3));
  (t1).f1 = ((float)(t3));
  memset(&t4, 0, sizeof(t4));
  (t1).f2 = ((float)(t4));
  memset(&t5, 0, sizeof(t5));
  (t1).f3 = ((float)(t5));
  t6 = t1;
  for (t7=0; t7<4; t7++) (v18).data[t7] = t6;
  v19 = ((int32_t)(((int64_t)UINT64_C(0))));
label1:;
  t8 = ((int32_t)(v19));
  t9 = ((int32_t)(v8));
  if (!(((t8) < (t9)))) goto label2;
  {
  t10 = ((int32_t)(v6));
  t11 = ((int32_t)(v19));
  t12 = ((int32_t)(v7));
  t13 = ((int32_t)(((int32_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  v20 = ((int32_t)(((int32_t)((uint64_t)(t10) + (uint64_t)(t13)))));
  t14 = ((int32_t)(v20));
  t15 = ((int32_t)(v5));
  t16 = ((int32_t)(((int64_t)UINT64_C(4))));
  t17 = ((int32_t)(((int32_t)gg_div(t15, t16))));
  t18 = ((t14) < (t17));
  if (t18) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t19 = v19;
  t20 = ((gg_type_10 *)(v18).data) + gg_index(t19, ((int64_t)UINT64_C(4)));
  t21 = v0;
  t22 = ((int32_t)(v20));
  t23 = fg_rms_load4(t21, t22);
  (*t20) = t23;
  /* FG_DIRECT_SLICE_INDEX */
  t24 = v19;
  t25 = ((gg_type_10 *)(v18).data) + gg_index(t24, ((int64_t)UINT64_C(4)));
  v21 = (*t25);
  t26 = v17;
  t27 = ((float)((v21).f0));
  t28 = ((float)((v21).f0));
  t29 = ((float)(((t27) * (t28))));
  t30 = ((float)((v21).f1));
  t31 = ((float)((v21).f1));
  t32 = ((float)(((t30) * (t31))));
  t33 = ((float)(((t29) + (t32))));
  t34 = ((float)((v21).f2));
  t35 = ((float)((v21).f2));
  t36 = ((float)(((t34) * (t35))));
  t37 = ((float)(((t33) + (t36))));
  t38 = ((float)((v21).f3));
  t39 = ((float)((v21).f3));
  t40 = ((float)(((t38) * (t39))));
  t41 = ((float)(((t37) + (t40))));
  v17 = ((t26) + (t41));
  }
  }
  t42 = v19;
  t43 = ((int32_t)(((int64_t)UINT64_C(1))));
  v19 = ((int32_t)((uint64_t)(t42) + (uint64_t)(t43)));
  }
  goto label1;
label2:;
  t44 = ((float)(v17));
  t45 = v2;
  t46 = ((int32_t)(v6));
  t47 = ((int32_t)(v7));
  v17 = gg_function_3(t44, t45, t46, t47, base + 0);
  t48 = ((float)(v17));
  t49 = ((float)(((float)(v5))));
  t50 = ((float)(((t48) / (t49))));
  t51 = ((float)(v15));
  t52 = ((float)(((t50) + (t51))));
  t53 = fg_rms_rsqrt(t52);
  v22 = ((float)(t53));
  v19 = ((int32_t)(((int64_t)UINT64_C(0))));
label4:;
  t54 = ((int32_t)(v19));
  t55 = ((int32_t)(v8));
  if (!(((t54) < (t55)))) goto label5;
  {
  t56 = ((int32_t)(v6));
  t57 = ((int32_t)(v19));
  t58 = ((int32_t)(v7));
  t59 = ((int32_t)(((int32_t)((uint64_t)(t57) * (uint64_t)(t58)))));
  v23 = ((int32_t)(((int32_t)((uint64_t)(t56) + (uint64_t)(t59)))));
  t60 = ((int32_t)(v23));
  t61 = ((int32_t)(v5));
  t62 = ((int32_t)(((int64_t)UINT64_C(4))));
  t63 = ((int32_t)(((int32_t)gg_div(t61, t62))));
  t64 = ((t60) < (t63));
  if (t64) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t65 = v19;
  t66 = ((gg_type_10 *)(v18).data) + gg_index(t65, ((int64_t)UINT64_C(4)));
  v24 = (*t66);
  memset(&(v25), 0, sizeof(v25));
  t67 = ((float)(v22));
  t68 = ((float)((v24).f0));
  (v25).f0 = ((float)(((t67) * (t68))));
  t69 = ((float)(v22));
  t70 = ((float)((v24).f1));
  (v25).f1 = ((float)(((t69) * (t70))));
  t71 = ((float)(v22));
  t72 = ((float)((v24).f2));
  (v25).f2 = ((float)(((t71) * (t72))));
  t73 = ((float)(v22));
  t74 = ((float)((v24).f3));
  (v25).f3 = ((float)(((t73) * (t74))));
  t75 = v10;
  if (t75) {
  {
  t76 = v3;
  t77 = ((int32_t)(v23));
  t78 = fg_rms_load4(t76, t77);
  v26 = t78;
  t79 = (v25).f0;
  t80 = ((float)((v26).f0));
  (v25).f0 = ((t79) * (t80));
  t81 = (v25).f1;
  t82 = ((float)((v26).f1));
  (v25).f1 = ((t81) * (t82));
  t83 = (v25).f2;
  t84 = ((float)((v26).f2));
  (v25).f2 = ((t83) * (t84));
  t85 = (v25).f3;
  t86 = ((float)((v26).f3));
  (v25).f3 = ((t85) * (t86));
  }
  }
  t87 = v11;
  if (t87) {
  {
  t88 = v4;
  t89 = ((int32_t)(v23));
  t90 = fg_rms_load4(t88, t89);
  v27 = t90;
  t91 = (v25).f0;
  t92 = ((float)((v27).f0));
  (v25).f0 = ((t91) + (t92));
  t93 = (v25).f1;
  t94 = ((float)((v27).f1));
  (v25).f1 = ((t93) + (t94));
  t95 = (v25).f2;
  t96 = ((float)((v27).f2));
  (v25).f2 = ((t95) + (t96));
  t97 = (v25).f3;
  t98 = ((float)((v27).f3));
  (v25).f3 = ((t97) + (t98));
  }
  }
  t99 = v12;
  if (t99) {
  {
  t100 = (v25).f0;
  t101 = ((float)(v16));
  (v25).f0 = ((t100) * (t101));
  t102 = (v25).f1;
  t103 = ((float)(v16));
  (v25).f1 = ((t102) * (t103));
  t104 = (v25).f2;
  t105 = ((float)(v16));
  (v25).f2 = ((t104) * (t105));
  t106 = (v25).f3;
  t107 = ((float)(v16));
  (v25).f3 = ((t106) * (t107));
  }
  }
  t108 = v1;
  t109 = ((int32_t)(v23));
  t110 = v25;
  fg_rms_store4(t108, t109, t110);
  }
  }
  t111 = v19;
  t112 = ((int32_t)(((int64_t)UINT64_C(1))));
  v19 = ((int32_t)((uint64_t)(t111) + (uint64_t)(t112)));
  }
  goto label4;
label5:;
  }
  } else {
  {
  v28 = ((int32_t)(v6));
label7:;
  t113 = ((int32_t)(v28));
  t114 = ((int32_t)(v5));
  if (!(((t113) < (t114)))) goto label8;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t115 = v28;
  t116 = ((float *)(v0).data) + gg_index(t115, (v0).len);
  v29 = ((float)((*t116)));
  t117 = v17;
  t118 = ((float)(v29));
  t119 = ((float)(v29));
  t120 = ((float)(((t118) * (t119))));
  v17 = ((t117) + (t120));
  t121 = v28;
  t122 = ((int32_t)(v7));
  v28 = ((int32_t)((uint64_t)(t121) + (uint64_t)(t122)));
  }
  goto label7;
label8:;
  t123 = ((float)(v17));
  t124 = v2;
  t125 = ((int32_t)(v6));
  t126 = ((int32_t)(v7));
  v17 = gg_function_3(t123, t124, t125, t126, base + 0);
  t127 = ((float)(v17));
  t128 = ((float)(((float)(v5))));
  t129 = ((float)(((t127) / (t128))));
  t130 = ((float)(v15));
  t131 = ((float)(((t129) + (t130))));
  t132 = fg_rms_rsqrt(t131);
  v30 = ((float)(t132));
  v28 = ((int32_t)(v6));
label10:;
  t133 = ((int32_t)(v28));
  t134 = ((int32_t)(v5));
  if (!(((t133) < (t134)))) goto label11;
  {
  t135 = v10;
  if (t135) {
  {
  t136 = v11;
  if (t136) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t137 = v28;
  t138 = ((float *)(v1).data) + gg_index(t137, (v1).len);
  t139 = ((float)(v30));
  /* FG_DIRECT_SLICE_INDEX */
  t140 = v28;
  t141 = ((float *)(v0).data) + gg_index(t140, (v0).len);
  t142 = ((float)((*t141)));
  t143 = ((float)(((t139) * (t142))));
  t144 = v3;
  t145 = ((int32_t)(v28));
  t146 = ((int32_t)(v13));
  t147 = ((int32_t)gg_mod(t145, t146));
  t148 = ((float *)t144.data) + gg_index(t147, t144.len);
  t149 = ((float)((*t148)));
  t150 = v4;
  t151 = ((int32_t)(v28));
  t152 = ((int32_t)(v14));
  t153 = ((int32_t)gg_mod(t151, t152));
  t154 = ((float *)t150.data) + gg_index(t153, t150.len);
  t155 = ((float)((*t154)));
  t156 = fg_rms_fma(t143, t149, t155);
  (*t138) = ((float)(t156));
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t157 = v28;
  t158 = ((float *)(v1).data) + gg_index(t157, (v1).len);
  t159 = ((float)(v30));
  /* FG_DIRECT_SLICE_INDEX */
  t160 = v28;
  t161 = ((float *)(v0).data) + gg_index(t160, (v0).len);
  t162 = ((float)((*t161)));
  t163 = ((float)(((t159) * (t162))));
  t164 = v3;
  t165 = ((int32_t)(v28));
  t166 = ((int32_t)(v13));
  t167 = ((int32_t)gg_mod(t165, t166));
  t168 = ((float *)t164.data) + gg_index(t167, t164.len);
  t169 = ((float)((*t168)));
  (*t158) = ((float)(((t163) * (t169))));
  }
  }
  }
  } else {
  t170 = v12;
  if (t170) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t171 = v28;
  t172 = ((float *)(v1).data) + gg_index(t171, (v1).len);
  t173 = ((float)(v16));
  t174 = ((float)(v30));
  /* FG_DIRECT_SLICE_INDEX */
  t175 = v28;
  t176 = ((float *)(v0).data) + gg_index(t175, (v0).len);
  t177 = ((float)((*t176)));
  t178 = ((float)(((t174) * (t177))));
  (*t172) = ((float)(((t173) * (t178))));
  }
  } else {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t179 = v28;
  t180 = ((float *)(v1).data) + gg_index(t179, (v1).len);
  t181 = ((float)(v30));
  /* FG_DIRECT_SLICE_INDEX */
  t182 = v28;
  t183 = ((float *)(v0).data) + gg_index(t182, (v0).len);
  t184 = ((float)((*t183)));
  (*t180) = ((float)(((t181) * (t184))));
  }
  }
  }
  t185 = v28;
  t186 = ((int32_t)(v7));
  v28 = ((int32_t)((uint64_t)(t185) + (uint64_t)(t186)));
  }
  goto label10;
label11:;
  }
  }
  t187 = ((int32_t)(v7));
  t188 = ((int32_t)(((int64_t)UINT64_C(32))));
  t189 = ((t187) > (t188));
  if (t189) {
  {
  fg_rms_barrier();
  }
  }
  }
gg_done:;
}
