#pragma once
#include <stdint.h>
#include <string.h>


#pragma pack(push, 1)
typedef struct { float data[8]; } fg169r_gg_type_6;
#pragma pack(pop)
static __device__ __forceinline__ void fg169r_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ double fg169r_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ int64_t fg169r_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg169r_gg_fail("array index out of bounds"); return i; }
static __device__ __forceinline__ int64_t fg169r_gg_div(int64_t a, int64_t b) { if (!b) fg169r_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg169r_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg169r_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg169r_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg169r_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg169r_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg169r_gg_umod(uint64_t a,uint64_t b) { if(!b)fg169r_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ float fg169r_gg_function_8(float p0, int64_t base);
static __device__ __forceinline__ float fg169r_gg_function_7(float p0, uint64_t p1, int32_t p2, int32_t p3, int64_t base);
static __device__ __forceinline__ float fg169r_gg_function_5(uint64_t p0, int64_t p1, int32_t p2, int32_t p3, uint64_t p4, int64_t base);
static __device__ __forceinline__ uint64_t fg169r_gg_function_13(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base);
static __device__ __forceinline__ void fg169r_gg_function_14(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int32_t p9, int32_t p10, uint64_t p11, int64_t base);
static __device__ __forceinline__ void fg169r_gg_function_12(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int32_t p9, int32_t p10, uint64_t p11, int64_t base);
static __device__ __forceinline__ void fg169r_gg_function_11(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int32_t p4, int32_t p5, uint64_t p6, int64_t base);
static __device__ __forceinline__ void fg169r_gg_function_4(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int32_t p4, int32_t p5, uint64_t p6, int64_t base);
static __device__ __forceinline__ void fg169r_gg_function_1(uint64_t p0, int64_t p1, int32_t p2, int64_t base);
static __device__ __forceinline__ float fg169r_gg_function_8(float p0, int64_t base) {
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
  t6 = fg169r_shuffle(t4, t5);
  t7 = ((float)(t6));
  v1 = ((t3) + (t7));
  t8 = v2;
  t9 = ((int32_t)(((int64_t)UINT64_C(1))));
  v2 = ((t8) >> ((uint64_t)(t9) & 31));
  }
  goto label1;
label2:;
  t0 = ((float)(v1));
  goto fg169r_gg_done;
  }
fg169r_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg169r_gg_function_7(float p0, uint64_t p1, int32_t p2, int32_t p3, int64_t base) {
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
  uint64_t t10;
  int32_t t11;
  int32_t t12;
  uint64_t t13;
  float t14;
  int32_t t15;
  int32_t t16;
  int32_t t17;
  int32_t t18;
  uint8_t t19;
  uint64_t t20;
  uint64_t t21;
  float t22;
  float t23;
  float v0;
  uint64_t v1;
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
  v4 = fg169r_gg_function_8(t1, base + 0);
  t2 = ((int32_t)(v3));
  t3 = ((int32_t)(((int64_t)UINT64_C(32))));
  t4 = ((t2) > (t3));
  if (t4) {
  {
  t5 = ((int32_t)(v2));
  t6 = ((int32_t)(((int64_t)UINT64_C(32))));
  v5 = ((int32_t)(((int32_t)fg169r_gg_mod(t5, t6))));
  t7 = ((int32_t)(v5));
  t8 = ((int32_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t10 = ((uint64_t)(v1));
  t11 = ((int32_t)(v2));
  t12 = ((int32_t)(((int64_t)UINT64_C(32))));
  t13 = ((uint64_t)(((uint64_t)(((int32_t)fg169r_gg_div(t11, t12))))));
  t14 = ((float)(v4));
  fg169r_store(t10, t13, t14);
  }
  }
  fg169r_barrier();
  v4 = ((float)(fg169r_gg_double_bits(UINT64_C(0))));
  t15 = ((int32_t)(v5));
  t16 = ((int32_t)(v3));
  t17 = ((int32_t)(((int64_t)UINT64_C(32))));
  t18 = ((int32_t)(((int32_t)fg169r_gg_div(t16, t17))));
  t19 = ((t15) < (t18));
  if (t19) {
  {
  t20 = ((uint64_t)(v1));
  t21 = ((uint64_t)(((uint64_t)(v5))));
  t22 = fg169r_load(t20, t21);
  v4 = ((float)(t22));
  }
  }
  t23 = ((float)(v4));
  v4 = fg169r_gg_function_8(t23, base + 0);
  }
  }
  t0 = ((float)(v4));
  goto fg169r_gg_done;
  }
fg169r_gg_done:;
  return t0;
}
static __device__ __forceinline__ float fg169r_gg_function_5(uint64_t p0, int64_t p1, int32_t p2, int32_t p3, uint64_t p4, int64_t base) {
  float t0;
  float t1;
  float t2;
  int64_t t3;
  float t4;
  float t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int32_t t9;
  int32_t t10;
  int32_t t11;
  float * t12;
  int64_t t13;
  int64_t t14;
  uint8_t t15;
  uint64_t t16;
  uint64_t t17;
  float t18;
  int64_t t19;
  int64_t t20;
  int32_t t21;
  int32_t t22;
  int32_t t23;
  int32_t t24;
  int32_t t25;
  float * t26;
  int32_t t27;
  float * t28;
  float t29;
  int32_t t30;
  float * t31;
  float t32;
  int32_t t33;
  int32_t t34;
  int32_t t35;
  int32_t t36;
  float t37;
  int32_t t38;
  float * t39;
  float t40;
  int32_t t41;
  int32_t t42;
  float t43;
  uint64_t t44;
  int32_t t45;
  int32_t t46;
  uint64_t v0;
  int64_t v1;
  int32_t v2;
  int32_t v3;
  uint64_t v4;
  fg169r_gg_type_6 v5;
  fg169r_gg_type_6 v6;
  int64_t v7;
  int32_t v8;
  float v9;
  int32_t v10;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  fg169r_sync();
  memset(&t1, 0, sizeof(t1));
  t2 = ((float)(t1));
  for (t3=0; t3<8; t3++) (v5).data[t3] = t2;
  memset(&t4, 0, sizeof(t4));
  t5 = ((float)(t4));
  for (t6=0; t6<8; t6++) (v6).data[t6] = t5;
  v7 = ((int64_t)(((int64_t)(v2))));
label1:;
  t7 = ((int64_t)(v7));
  t8 = ((int64_t)(v1));
  if (!(((t7) < (t8)))) goto label2;
  {
  v8 = ((int32_t)(((int64_t)UINT64_C(0))));
label4:;
  t9 = ((int32_t)(v8));
  t10 = ((int32_t)(((int64_t)UINT64_C(8))));
  if (!(((t9) < (t10)))) goto label5;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t11 = v8;
  t12 = ((float *)(v5).data) + fg169r_gg_index(t11, ((int64_t)UINT64_C(8)));
  t13 = ((int64_t)(v7));
  t14 = ((int64_t)(v1));
  t15 = ((t13) < (t14));
  if (t15) {
  {
  t16 = ((uint64_t)(v0));
  t17 = ((uint64_t)(((uint64_t)(v7))));
  t18 = fg169r_load(t16, t17);
  (*t12) = ((float)(t18));
  }
  } else {
  {
  (*t12) = ((float)(fg169r_gg_double_bits(UINT64_C(0))));
  }
  }
  t19 = v7;
  t20 = ((int64_t)(((int64_t)(v3))));
  v7 = ((int64_t)((uint64_t)(t19) + (uint64_t)(t20)));
  t21 = v8;
  t22 = ((int32_t)(((int64_t)UINT64_C(1))));
  v8 = ((int32_t)((uint64_t)(t21) + (uint64_t)(t22)));
  }
  goto label4;
label5:;
  v8 = ((int32_t)(((int64_t)UINT64_C(0))));
label7:;
  t23 = ((int32_t)(v8));
  t24 = ((int32_t)(((int64_t)UINT64_C(8))));
  if (!(((t23) < (t24)))) goto label8;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t25 = v8;
  t26 = ((float *)(v6).data) + fg169r_gg_index(t25, ((int64_t)UINT64_C(8)));
  /* FG_DIRECT_SLICE_INDEX */
  t27 = v8;
  t28 = ((float *)(v6).data) + fg169r_gg_index(t27, ((int64_t)UINT64_C(8)));
  t29 = ((float)((*t28)));
  /* FG_DIRECT_SLICE_INDEX */
  t30 = v8;
  t31 = ((float *)(v5).data) + fg169r_gg_index(t30, ((int64_t)UINT64_C(8)));
  t32 = ((float)((*t31)));
  (*t26) = ((float)(((t29) + (t32))));
  t33 = v8;
  t34 = ((int32_t)(((int64_t)UINT64_C(1))));
  v8 = ((int32_t)((uint64_t)(t33) + (uint64_t)(t34)));
  }
  goto label7;
label8:;
  }
  goto label1;
label2:;
  v9 = ((float)(fg169r_gg_double_bits(UINT64_C(0))));
  v10 = ((int32_t)(((int64_t)UINT64_C(0))));
label10:;
  t35 = ((int32_t)(v10));
  t36 = ((int32_t)(((int64_t)UINT64_C(8))));
  if (!(((t35) < (t36)))) goto label11;
  {
  t37 = v9;
  /* FG_DIRECT_SLICE_INDEX */
  t38 = v10;
  t39 = ((float *)(v6).data) + fg169r_gg_index(t38, ((int64_t)UINT64_C(8)));
  t40 = ((float)((*t39)));
  v9 = ((t37) + (t40));
  t41 = v10;
  t42 = ((int32_t)(((int64_t)UINT64_C(1))));
  v10 = ((int32_t)((uint64_t)(t41) + (uint64_t)(t42)));
  }
  goto label10;
label11:;
  t43 = ((float)(v9));
  t44 = ((uint64_t)(v4));
  t45 = ((int32_t)(v2));
  t46 = ((int32_t)(v3));
  v9 = fg169r_gg_function_7(t43, t44, t45, t46, base + 0);
  t0 = ((float)(v9));
  goto fg169r_gg_done;
  }
fg169r_gg_done:;
  return t0;
}
static __device__ __forceinline__ uint64_t fg169r_gg_function_13(uint64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  uint64_t t21;
  uint64_t t22;
  uint64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  uint64_t v10;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(v2));
  v7 = ((int64_t)(((int64_t)fg169r_gg_mod(t1, t2))));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(v2));
  t5 = ((int64_t)(((int64_t)fg169r_gg_div(t3, t4))));
  t6 = ((int64_t)(v3));
  v8 = ((int64_t)(((int64_t)fg169r_gg_mod(t5, t6))));
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(v3));
  t10 = ((int64_t)(((int64_t)((uint64_t)(t8) * (uint64_t)(t9)))));
  v9 = ((int64_t)(((int64_t)fg169r_gg_div(t7, t10))));
  t11 = ((int64_t)(v7));
  t12 = ((int64_t)(v4));
  t13 = ((int64_t)(((int64_t)((uint64_t)(t11) * (uint64_t)(t12)))));
  t14 = ((int64_t)(v8));
  t15 = ((int64_t)(v5));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t14) * (uint64_t)(t15)))));
  t17 = ((int64_t)(((int64_t)((uint64_t)(t13) + (uint64_t)(t16)))));
  t18 = ((int64_t)(v9));
  t19 = ((int64_t)(v6));
  t20 = ((int64_t)(((int64_t)((uint64_t)(t18) * (uint64_t)(t19)))));
  v10 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t20)))))));
  t21 = ((uint64_t)(v0));
  t22 = ((uint64_t)(v10));
  t0 = ((uint64_t)(((uint64_t)((uint64_t)(t21) + (uint64_t)(t22)))));
  goto fg169r_gg_done;
  }
fg169r_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg169r_gg_function_14(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int32_t p9, int32_t p10, uint64_t p11, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t t7;
  int64_t t8;
  int32_t t9;
  int32_t t10;
  uint64_t t11;
  int32_t t12;
  int32_t t13;
  uint8_t t14;
  uint64_t t15;
  uint64_t t16;
  float t17;
  float t18;
  float t19;
  uint64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int32_t v9;
  int32_t v10;
  uint64_t v11;
  uint64_t v12;
  float v13;
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
  {
  t0 = ((uint64_t)(v0));
  t1 = ((int64_t)(v8));
  t2 = ((int64_t)(v3));
  t3 = ((int64_t)(v4));
  t4 = ((int64_t)(v5));
  t5 = ((int64_t)(v6));
  t6 = ((int64_t)(v7));
  v12 = fg169r_gg_function_13(t0, t1, t2, t3, t4, t5, t6, base + 0);
  t7 = ((uint64_t)(v12));
  t8 = ((int64_t)(v2));
  t9 = ((int32_t)(v9));
  t10 = ((int32_t)(v10));
  t11 = ((uint64_t)(v11));
  v13 = fg169r_gg_function_5(t7, t8, t9, t10, t11, base + 0);
  t12 = ((int32_t)(v9));
  t13 = ((int32_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) != (t13));
  if (t14) {
  {
  goto fg169r_gg_done;
  }
  }
  t15 = ((uint64_t)(v1));
  t16 = ((uint64_t)(((uint64_t)(v8))));
  t17 = ((float)(v13));
  t18 = ((float)(((float)(v2))));
  t19 = ((float)(((t17) / (t18))));
  fg169r_store(t15, t16, t19);
  }
fg169r_gg_done:;
}
static __device__ __forceinline__ void fg169r_gg_function_12(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, int64_t p8, int32_t p9, int32_t p10, uint64_t p11, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  uint64_t t7;
  int64_t t8;
  int32_t t9;
  int32_t t10;
  uint64_t t11;
  int32_t t12;
  int32_t t13;
  uint8_t t14;
  uint64_t t15;
  uint64_t t16;
  float t17;
  uint64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  int64_t v8;
  int32_t v9;
  int32_t v10;
  uint64_t v11;
  uint64_t v12;
  float v13;
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
  {
  t0 = ((uint64_t)(v0));
  t1 = ((int64_t)(v8));
  t2 = ((int64_t)(v3));
  t3 = ((int64_t)(v4));
  t4 = ((int64_t)(v5));
  t5 = ((int64_t)(v6));
  t6 = ((int64_t)(v7));
  v12 = fg169r_gg_function_13(t0, t1, t2, t3, t4, t5, t6, base + 0);
  t7 = ((uint64_t)(v12));
  t8 = ((int64_t)(v2));
  t9 = ((int32_t)(v9));
  t10 = ((int32_t)(v10));
  t11 = ((uint64_t)(v11));
  v13 = fg169r_gg_function_5(t7, t8, t9, t10, t11, base + 0);
  t12 = ((int32_t)(v9));
  t13 = ((int32_t)(((int64_t)UINT64_C(0))));
  t14 = ((t12) != (t13));
  if (t14) {
  {
  goto fg169r_gg_done;
  }
  }
  t15 = ((uint64_t)(v1));
  t16 = ((uint64_t)(((uint64_t)(v8))));
  t17 = ((float)(v13));
  fg169r_store(t15, t16, t17);
  }
fg169r_gg_done:;
}
static __device__ __forceinline__ void fg169r_gg_function_11(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int32_t p4, int32_t p5, uint64_t p6, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  uint64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint64_t t6;
  int64_t t7;
  int32_t t8;
  int32_t t9;
  uint64_t t10;
  int32_t t11;
  int32_t t12;
  uint8_t t13;
  uint64_t t14;
  uint64_t t15;
  float t16;
  float t17;
  float t18;
  uint64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  int32_t v4;
  int32_t v5;
  uint64_t v6;
  uint64_t v7;
  float v8;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  t0 = ((uint64_t)(v0));
  t1 = ((int64_t)(v3));
  t2 = ((int64_t)(v2));
  t3 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t1) * (uint64_t)(t2)))))));
  t4 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t5 = ((uint64_t)(((uint64_t)((uint64_t)(t3) * (uint64_t)(t4)))));
  v7 = ((uint64_t)(((uint64_t)((uint64_t)(t0) + (uint64_t)(t5)))));
  t6 = ((uint64_t)(v7));
  t7 = ((int64_t)(v2));
  t8 = ((int32_t)(v4));
  t9 = ((int32_t)(v5));
  t10 = ((uint64_t)(v6));
  v8 = fg169r_gg_function_5(t6, t7, t8, t9, t10, base + 0);
  t11 = ((int32_t)(v4));
  t12 = ((int32_t)(((int64_t)UINT64_C(0))));
  t13 = ((t11) != (t12));
  if (t13) {
  {
  goto fg169r_gg_done;
  }
  }
  t14 = ((uint64_t)(v1));
  t15 = ((uint64_t)(((uint64_t)(v3))));
  t16 = ((float)(v8));
  t17 = ((float)(((float)(v2))));
  t18 = ((float)(((t16) / (t17))));
  fg169r_store(t14, t15, t18);
  }
fg169r_gg_done:;
}
static __device__ __forceinline__ void fg169r_gg_function_4(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int32_t p4, int32_t p5, uint64_t p6, int64_t base) {
  uint64_t t0;
  int64_t t1;
  int64_t t2;
  uint64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint64_t t6;
  int64_t t7;
  int32_t t8;
  int32_t t9;
  uint64_t t10;
  int32_t t11;
  int32_t t12;
  uint8_t t13;
  uint64_t t14;
  uint64_t t15;
  float t16;
  uint64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  int32_t v4;
  int32_t v5;
  uint64_t v6;
  uint64_t v7;
  float v8;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  t0 = ((uint64_t)(v0));
  t1 = ((int64_t)(v3));
  t2 = ((int64_t)(v2));
  t3 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t1) * (uint64_t)(t2)))))));
  t4 = ((uint64_t)(((int64_t)UINT64_C(4))));
  t5 = ((uint64_t)(((uint64_t)((uint64_t)(t3) * (uint64_t)(t4)))));
  v7 = ((uint64_t)(((uint64_t)((uint64_t)(t0) + (uint64_t)(t5)))));
  t6 = ((uint64_t)(v7));
  t7 = ((int64_t)(v2));
  t8 = ((int32_t)(v4));
  t9 = ((int32_t)(v5));
  t10 = ((uint64_t)(v6));
  v8 = fg169r_gg_function_5(t6, t7, t8, t9, t10, base + 0);
  t11 = ((int32_t)(v4));
  t12 = ((int32_t)(((int64_t)UINT64_C(0))));
  t13 = ((t11) != (t12));
  if (t13) {
  {
  goto fg169r_gg_done;
  }
  }
  t14 = ((uint64_t)(v1));
  t15 = ((uint64_t)(((uint64_t)(v3))));
  t16 = ((float)(v8));
  fg169r_store(t14, t15, t16);
  }
fg169r_gg_done:;
}
static __device__ __forceinline__ void fg169r_gg_function_1(uint64_t p0, int64_t p1, int32_t p2, int64_t base) {
  int32_t t0;
  int32_t t1;
  uint8_t t2;
  uint64_t t3;
  uint64_t t4;
  uint64_t t5;
  uint64_t t6;
  float t7;
  float t8;
  float t9;
  float t10;
  uint64_t v0;
  int64_t v1;
  int32_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t0 = ((int32_t)(v2));
  t1 = ((int32_t)(((int64_t)UINT64_C(0))));
  t2 = ((t0) == (t1));
  if (t2) {
  {
  t3 = ((uint64_t)(v0));
  t4 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t5 = ((uint64_t)(v0));
  t6 = ((uint64_t)(((uint64_t)(((int64_t)UINT64_C(0))))));
  t7 = fg169r_load(t5, t6);
  t8 = ((float)(t7));
  t9 = ((float)(((float)(v1))));
  t10 = ((float)(((t8) / (t9))));
  fg169r_store(t3, t4, t10);
  }
  }
  }
fg169r_gg_done:;
}
