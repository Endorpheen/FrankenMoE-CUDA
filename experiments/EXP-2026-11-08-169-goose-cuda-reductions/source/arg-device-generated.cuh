#pragma once
#include <stdint.h>
#include <string.h>


typedef struct fg169a_gg_type_7 fg169a_gg_type_7;
#pragma pack(push, 1)
struct fg169a_gg_type_7 {
  float f0;
  int32_t f1;
};
#pragma pack(pop)
static __device__ __forceinline__ void fg169a_gg_fail(const char*){asm("trap;");}
static __device__ __forceinline__ double fg169a_gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ int64_t fg169a_gg_index(int64_t i, int64_t len) { if ((uint64_t)i >= (uint64_t)len) fg169a_gg_fail("array index out of bounds"); return i; }
static __device__ __forceinline__ int64_t fg169a_gg_div(int64_t a, int64_t b) { if (!b) fg169a_gg_fail("division by zero"); if (a==INT64_MIN && b==-1) fg169a_gg_fail("division overflow"); return a/b; }
static __device__ __forceinline__ int64_t fg169a_gg_mod(int64_t a, int64_t b) {
    int64_t r; if (!b) fg169a_gg_fail("division by zero"); if (b==-1) return 0;
    r=a%b; if(r<0) return (int64_t)((uint64_t)r + (b<0 ? (uint64_t)0-(uint64_t)b : (uint64_t)b)); return r;
}
static __device__ __forceinline__ uint64_t fg169a_gg_udiv(uint64_t a,uint64_t b) { if(!b)fg169a_gg_fail("division by zero");return a/b; }
static __device__ __forceinline__ uint64_t fg169a_gg_umod(uint64_t a,uint64_t b) { if(!b)fg169a_gg_fail("division by zero");return a%b; }
static __device__ __forceinline__ fg169a_gg_type_7 fg169a_gg_function_3(fg169a_gg_type_7 p0, int64_t base);
static __device__ __forceinline__ void fg169a_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int32_t p4, int32_t p5, int64_t base);
static __device__ __forceinline__ fg169a_gg_type_7 fg169a_gg_function_3(fg169a_gg_type_7 p0, int64_t base) {
  fg169a_gg_type_7 t0;
  int32_t t1;
  int32_t t2;
  float t3;
  int32_t t4;
  float t5;
  int32_t t6;
  int32_t t7;
  int32_t t8;
  float t9;
  float t10;
  uint8_t t11;
  int32_t t12;
  int32_t t13;
  fg169a_gg_type_7 v0;
  fg169a_gg_type_7 v1;
  int32_t v2;
  float v3;
  int32_t v4;
  v0 = p0;
  {
  v1 = v0;
  v2 = ((int32_t)(((int64_t)UINT64_C(16))));
label1:;
  t1 = ((int32_t)(v2));
  t2 = ((int32_t)(((int64_t)UINT64_C(0))));
  if (!(((t1) > (t2)))) goto label2;
  {
  t3 = ((float)((v1).f0));
  t4 = ((int32_t)(v2));
  t5 = fg169a_shuffle(t3, t4);
  v3 = ((float)(t5));
  t6 = ((int32_t)((v1).f1));
  t7 = ((int32_t)(v2));
  t8 = fg169a_ishuffle(t6, t7);
  v4 = ((int32_t)(t8));
  t9 = ((float)(v3));
  t10 = ((float)((v1).f0));
  t11 = ((t9) > (t10));
  if (t11) {
  {
  (v1).f0 = ((float)(v3));
  (v1).f1 = ((int32_t)(v4));
  }
  }
  t12 = v2;
  t13 = ((int32_t)(((int64_t)UINT64_C(1))));
  v2 = ((t12) >> ((uint64_t)(t13) & 31));
  }
  goto label1;
label2:;
  t0 = v1;
  goto fg169a_gg_done;
  }
fg169a_gg_done:;
  return t0;
}
static __device__ __forceinline__ void fg169a_gg_function_1(uint64_t p0, uint64_t p1, int64_t p2, int64_t p3, int32_t p4, int32_t p5, int64_t base) {
  double t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  uint64_t t6;
  int64_t t7;
  int64_t t8;
  uint64_t t9;
  float t10;
  float t11;
  float t12;
  uint8_t t13;
  int64_t t14;
  int64_t t15;
  fg169a_gg_type_7 t16;
  int32_t t17;
  int32_t t18;
  int32_t t19;
  int32_t t20;
  int32_t t21;
  int32_t t22;
  int32_t t23;
  int32_t t24;
  uint8_t t25;
  int32_t t26;
  int32_t t27;
  uint8_t t28;
  uint64_t t29;
  float t30;
  uint64_t t31;
  int32_t t32;
  int32_t t33;
  int32_t t34;
  uint8_t t35;
  int32_t t36;
  int32_t t37;
  uint8_t t38;
  fg169a_gg_type_7 t39;
  uint64_t t40;
  float t41;
  uint64_t t42;
  int32_t t43;
  fg169a_gg_type_7 t44;
  int32_t t45;
  int32_t t46;
  uint8_t t47;
  uint8_t t48;
  int32_t t49;
  int32_t t50;
  uint8_t t51;
  uint64_t t52;
  uint64_t t53;
  int32_t t54;
  uint64_t v0;
  uint64_t v1;
  int64_t v2;
  int64_t v3;
  int32_t v4;
  int32_t v5;
  float v6;
  int32_t v7;
  int64_t v8;
  int64_t v9;
  float v10;
  fg169a_gg_type_7 v11;
  int32_t v12;
  int32_t v13;
  int32_t v14;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  {
  t0 = fg169a_gg_double_bits(UINT64_C(5183643170566569984));
  v6 = ((float)((-(t0))));
  t1 = ((int64_t)UINT64_C(1));
  v7 = ((int32_t)(((int64_t)((uint64_t)0 - (uint64_t)(t1)))));
  t2 = ((int64_t)(v3));
  t3 = ((int64_t)(v2));
  v8 = ((int64_t)(((int64_t)((uint64_t)(t2) * (uint64_t)(t3)))));
  v9 = ((int64_t)(((int64_t)(v4))));
label1:;
  t4 = ((int64_t)(v9));
  t5 = ((int64_t)(v2));
  if (!(((t4) < (t5)))) goto label2;
  {
  t6 = ((uint64_t)(v0));
  t7 = ((int64_t)(v8));
  t8 = ((int64_t)(v9));
  t9 = ((uint64_t)(((uint64_t)(((int64_t)((uint64_t)(t7) + (uint64_t)(t8)))))));
  t10 = fg169a_load(t6, t9);
  v10 = ((float)(t10));
  t11 = ((float)(v10));
  t12 = ((float)(v6));
  t13 = ((t11) > (t12));
  if (t13) {
  {
  v6 = ((float)(v10));
  v7 = ((int32_t)(((int32_t)(v9))));
  }
  }
  t14 = v9;
  t15 = ((int64_t)(((int64_t)(v5))));
  v9 = ((int64_t)((uint64_t)(t14) + (uint64_t)(t15)));
  }
  goto label1;
label2:;
  memset(&(v11), 0, sizeof(v11));
  (v11).f0 = ((float)(v6));
  (v11).f1 = ((int32_t)(v7));
  t16 = v11;
  v11 = fg169a_gg_function_3(t16, base + 0);
  t17 = ((int32_t)(v4));
  t18 = ((int32_t)(((int64_t)UINT64_C(32))));
  v12 = ((int32_t)(((int32_t)fg169a_gg_mod(t17, t18))));
  t19 = ((int32_t)(v4));
  t20 = ((int32_t)(((int64_t)UINT64_C(32))));
  v13 = ((int32_t)(((int32_t)fg169a_gg_div(t19, t20))));
  t21 = ((int32_t)(v5));
  t22 = ((int32_t)(((int64_t)UINT64_C(32))));
  v14 = ((int32_t)(((int32_t)fg169a_gg_div(t21, t22))));
  t23 = ((int32_t)(v14));
  t24 = ((int32_t)(((int64_t)UINT64_C(1))));
  t25 = ((t23) > (t24));
  if (t25) {
  {
  t26 = ((int32_t)(v12));
  t27 = ((int32_t)(((int64_t)UINT64_C(0))));
  t28 = ((t26) == (t27));
  if (t28) {
  {
  t29 = ((uint64_t)(((uint64_t)(v13))));
  t30 = ((float)((v11).f0));
  fg169a_sstoref(t29, t30);
  t31 = ((uint64_t)(((uint64_t)(v13))));
  t32 = ((int32_t)((v11).f1));
  fg169a_sstorei(t31, t32);
  }
  }
  fg169a_barrier();
  t33 = ((int32_t)(v13));
  t34 = ((int32_t)(((int64_t)UINT64_C(0))));
  t35 = ((t33) == (t34));
  if (t35) {
  {
  t36 = ((int32_t)(v12));
  t37 = ((int32_t)(v14));
  t38 = ((t36) < (t37));
  if (t38) {
  {
  memset(&(t39), 0, sizeof(t39));
  t40 = ((uint64_t)(((uint64_t)(v12))));
  t41 = fg169a_sloadf(t40);
  (t39).f0 = ((float)(t41));
  t42 = ((uint64_t)(((uint64_t)(v12))));
  t43 = fg169a_sloadi(t42);
  (t39).f1 = ((int32_t)(t43));
  v11 = t39;
  }
  }
  t44 = v11;
  v11 = fg169a_gg_function_3(t44, base + 0);
  }
  }
  }
  }
  t45 = ((int32_t)(v13));
  t46 = ((int32_t)(((int64_t)UINT64_C(0))));
  t47 = ((t45) == (t46));
  t48 = t47;
  if (t48) {
  t49 = ((int32_t)(v12));
  t50 = ((int32_t)(((int64_t)UINT64_C(0))));
  t48 = ((t49) == (t50));
  }
  t51 = t48;
  if (t51) {
  {
  t52 = ((uint64_t)(v1));
  t53 = ((uint64_t)(((uint64_t)(v3))));
  t54 = ((int32_t)((v11).f1));
  fg169a_store(t52, t53, t54);
  }
  }
  }
fg169a_gg_done:;
}
