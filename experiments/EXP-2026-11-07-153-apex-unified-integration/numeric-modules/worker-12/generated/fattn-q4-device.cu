
// Generated device prelude for fg012 Apex Q4_0 FlashAttention.
#pragma once
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cfloat>
#include <cuda_fp16.h>

typedef struct { void *data; int64_t len; } gg_slice;

static __host__ __device__ __forceinline__ int64_t gg_index(int64_t i, int64_t) { return i; }
static __host__ __device__ __forceinline__ gg_slice gg_view(void * p, int64_t n) { gg_slice s; s.data = p; s.len = n; return s; }
static __host__ __device__ __forceinline__ int64_t gg_div(int64_t a, int64_t b) { return a / b; }
static __host__ __device__ __forceinline__ int64_t gg_mod(int64_t a, int64_t b) { int64_t r = a % b; return r < 0 ? r + (b < 0 ? -b : b) : r; }
static __host__ __device__ __forceinline__ uint64_t gg_udiv(uint64_t a, uint64_t b) { return a / b; }
static __host__ __device__ __forceinline__ uint64_t gg_umod(uint64_t a, uint64_t b) { return a % b; }

static __host__ __device__ __forceinline__ double gg_double_bits(uint64_t b) { double d; memcpy(&d, &b, 8); return d; }
static __host__ __device__ __forceinline__ uint32_t dfa_ld_u8(int64_t p, int64_t o) {
    return *reinterpret_cast<const uint8_t *>(p + o);
}
static __host__ __device__ __forceinline__ uint32_t dfa_ld_u16(int64_t p, int64_t o) {
    uint16_t v; memcpy(&v, reinterpret_cast<const void *>(p + o), 2); return v;
}
static __host__ __device__ __forceinline__ uint32_t dfa_ld_u32(int64_t p, int64_t o) {
    uint16_t a, b;
    memcpy(&a, reinterpret_cast<const void *>(p + o), 2);
    memcpy(&b, reinterpret_cast<const void *>(p + o + 2), 2);
    return uint32_t(a) | (uint32_t(b) << 16);
}
static __host__ __device__ __forceinline__ float dfa_ld_f32(int64_t p, int64_t o) {
    float v; memcpy(&v, reinterpret_cast<const void *>(p + o), 4); return v;
}
static __host__ __device__ __forceinline__ void dfa_st_f32(int64_t p, int64_t o, float v) {
    memcpy(reinterpret_cast<void *>(p + o), &v, 4);
}
static __host__ __device__ __forceinline__ float dfa_half(uint32_t v) {
    return __half2float(__ushort_as_half(static_cast<unsigned short>(v)));
}
static __host__ __device__ __forceinline__ int32_t dfa_f2i(float v) { return static_cast<int32_t>(v); }
static __host__ __device__ __forceinline__ int32_t dfa_dp4a(int32_t a, int32_t b, int32_t c) {
#if defined(__CUDA_ARCH__) && __CUDA_ARCH__ >= 610
    return __dp4a(a, b, c);
#else
    const int8_t * pa = reinterpret_cast<const int8_t *>(&a);
    const int8_t * pb = reinterpret_cast<const int8_t *>(&b);
    return c + int32_t(pa[0]) * int32_t(pb[0]) + int32_t(pa[1]) * int32_t(pb[1])
             + int32_t(pa[2]) * int32_t(pb[2]) + int32_t(pa[3]) * int32_t(pb[3]);
#endif
}
static __host__ __device__ __forceinline__ float dfa_neg_half_flt_max() { return -FLT_MAX / 2.0f; }

static __device__ __forceinline__ int64_t dfa_tid(int64_t a) { return a == 0 ? threadIdx.x : (a == 1 ? threadIdx.y : threadIdx.z); }
static __device__ __forceinline__ int64_t dfa_bid(int64_t a) { return a == 0 ? blockIdx.x : (a == 1 ? blockIdx.y : blockIdx.z); }
static __device__ __forceinline__ int64_t dfa_gdim(int64_t a) { return a == 0 ? gridDim.x : (a == 1 ? gridDim.y : gridDim.z); }
static __device__ __forceinline__ float dfa_shfl_xor_f32(float v, int64_t m) { return __shfl_xor_sync(0xffffffffu, v, static_cast<int>(m), 32); }
static __device__ __forceinline__ void dfa_sync() { __syncthreads(); }
static __device__ __forceinline__ void dfa_syncwarp() { __syncwarp(); }

// Per-block shared scratch: max(ne_KQ, ne_combine) <= 1024 plus max/sum tables.
static __shared__ float dfa_shbuf[1280];
static __device__ __forceinline__ float dfa_sh_f32(int64_t i) { return dfa_shbuf[i]; }
static __device__ __forceinline__ void dfa_sh_set_f32(int64_t i, float v) { dfa_shbuf[i] = v; }
static __device__ __forceinline__ uint32_t dfa_sh_u32(int64_t i) {
    uint32_t v; memcpy(&v, &dfa_shbuf[i], 4); return v;
}
static __device__ __forceinline__ void dfa_sh_set_u32(int64_t i, uint32_t v) {
    memcpy(&dfa_shbuf[i], &v, 4);
}

typedef struct gg_type_8 gg_type_8;
#pragma pack(push, 1)
typedef struct { uint32_t data[2]; } gg_type_9;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_9 data[2]; } gg_type_10;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { float data[2]; } gg_type_11;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct { gg_type_11 data[4]; } gg_type_12;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_8 {
  gg_type_10 f0;
  gg_type_12 f1;
};
#pragma pack(pop)
typedef struct gg_type_13 gg_type_13;
#pragma pack(push, 1)
typedef struct { gg_type_12 data[2]; } gg_type_14;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_13 {
  gg_type_14 f2;
};
#pragma pack(pop)
typedef struct gg_type_15 gg_type_15;
#pragma pack(push, 1)
typedef struct { float data[4]; } gg_type_16;
#pragma pack(pop)
#pragma pack(push, 1)
struct gg_type_15 {
  gg_type_16 f3;
};
#pragma pack(pop)

static __device__ __forceinline__ float gg_function_33(float p0, int64_t p1, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  float t5;
  float t6;
  int64_t t7;
  float t8;
  float t9;
  int64_t t10;
  int64_t t11;
  float v0;
  int64_t v1;
  float v2;
  int64_t v3;
  v0 = p0;
  v1 = p1;
  {
  v2 = ((float)(v0));
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(2))));
  v3 = ((int64_t)(((int64_t)gg_div(t1, t2))));
label1:;
  t3 = ((int64_t)(v3));
  t4 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t3) > (t4)))) goto label2;
  {
  t5 = v2;
  t6 = ((float)(v2));
  t7 = ((int64_t)(v3));
  t8 = dfa_shfl_xor_f32(t6, t7);
  t9 = ((float)(t8));
  v2 = ((t5) + (t9));
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(((int64_t)UINT64_C(2))));
  v3 = ((int64_t)(((int64_t)gg_div(t10, t11))));
  }
  goto label1;
label2:;
  t0 = ((float)(v2));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ __forceinline__ float gg_function_40(float p0, float p1, int64_t base) {
  float t0;
  float t1;
  float t2;
  float t3;
  float t4;
  float v0;
  float v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((float)(v0));
  t2 = ((float)(v1));
  t3 = ((float)(((t1) - (t2))));
  t4 = expf(t3);
  t0 = ((float)(t4));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ __forceinline__ float gg_function_39(float p0, float p1, int64_t base) {
  float t0;
  float t1;
  float t2;
  float t3;
  float v0;
  float v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((float)(v0));
  t2 = ((float)(v1));
  t3 = fmaxf(t1, t2);
  t0 = ((float)(t3));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __device__ __forceinline__ float gg_function_48(float p0, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  float t3;
  float t4;
  int64_t t5;
  float t6;
  float t7;
  int64_t t8;
  int64_t t9;
  float v0;
  float v1;
  int64_t v2;
  v0 = p0;
  {
  v1 = ((float)(v0));
  v2 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(16))))));
label1:;
  t1 = ((int64_t)(v2));
  t2 = ((int64_t)(((int64_t)UINT64_C(0))));
  if (!(((t1) > (t2)))) goto label2;
  {
  t3 = ((float)(v1));
  t4 = ((float)(v1));
  t5 = ((int64_t)(v2));
  t6 = dfa_shfl_xor_f32(t4, t5);
  t7 = ((float)(t6));
  v1 = gg_function_39(t3, t7, base + 0);
  t8 = ((int64_t)(v2));
  t9 = ((int64_t)(((int64_t)UINT64_C(2))));
  v2 = ((int64_t)(((int64_t)gg_div(t8, t9))));
  }
  goto label1;
label2:;
  t0 = ((float)(v1));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ __forceinline__ float gg_function_43(float p0, float p1, float p2, int64_t base) {
  float t0;
  float t1;
  float t2;
  float t3;
  float t4;
  float v0;
  float v1;
  float v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((float)(v0));
  t2 = ((float)(v1));
  t3 = ((float)(((t1) * (t2))));
  t4 = ((float)(v2));
  t0 = ((float)(((t3) + (t4))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ __forceinline__ float gg_function_47(float p0, float p1, float p2, float p3, int64_t base) {
  float t0;
  float t1;
  float t2;
  float t3;
  float t4;
  float t5;
  float t6;
  float v0;
  float v1;
  float v2;
  float v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((float)(v0));
  t2 = ((float)(v1));
  t3 = ((float)(((t1) * (t2))));
  t4 = ((float)(v2));
  t5 = ((float)(v3));
  t6 = ((float)(((t4) * (t5))));
  t0 = ((float)(((t3) + (t6))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ __forceinline__ float gg_function_5(uint32_t p0, int64_t p1, int64_t p2, float p3, int64_t base) {
  float t0;
  uint32_t t1;
  int64_t t2;
  int64_t t3;
  uint32_t t4;
  uint32_t t5;
  uint32_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  uint32_t t10;
  uint32_t t11;
  uint32_t t12;
  uint32_t t13;
  float t14;
  int32_t t15;
  int32_t t16;
  float t17;
  uint32_t v0;
  int64_t v1;
  int64_t v2;
  float v3;
  uint32_t v4;
  uint32_t v5;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((uint32_t)(v0));
  t2 = ((int64_t)(((int64_t)UINT64_C(8))));
  t3 = ((int64_t)(v1));
  t4 = ((uint32_t)(((uint32_t)(((int64_t)((uint64_t)(t2) * (uint64_t)(t3)))))));
  t5 = ((uint32_t)(((t1) >> ((uint64_t)(t4) & 31))));
  t6 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(255))))));
  v4 = ((uint32_t)(((t5) & (t6))));
  t7 = ((int64_t)(v2));
  t8 = ((int64_t)(((int64_t)UINT64_C(0))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t10 = ((uint32_t)(v4));
  t11 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(15))))));
  v5 = ((uint32_t)(((t10) & (t11))));
  }
  } else {
  {
  t12 = ((uint32_t)(v4));
  t13 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(4))))));
  v5 = ((uint32_t)(((t12) >> ((uint64_t)(t13) & 31))));
  }
  }
  t14 = ((float)(v3));
  t15 = ((int32_t)(((int32_t)(v5))));
  t16 = ((int32_t)(((int64_t)UINT64_C(8))));
  t17 = ((float)(((float)(((int32_t)((uint64_t)(t15) - (uint64_t)(t16)))))));
  t0 = ((float)(((t14) * (t17))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ __forceinline__ gg_type_15 gg_function_46(uint32_t p0, int64_t p1, float p2, int64_t base) {
  gg_type_15 t0;
  float t1;
  float t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  float * t7;
  uint32_t t8;
  int64_t t9;
  int64_t t10;
  float t11;
  uint32_t v0;
  int64_t v1;
  float v2;
  gg_type_15 v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  memset(&(v3), 0, sizeof(v3));
  memset(&t1, 0, sizeof(t1));
  t2 = ((float)(t1));
  for (t3=0; t3<4; t3++) ((v3).f3).data[t3] = t2;
  t5 = ((int64_t)UINT64_C(4));
  t4 = 0;
label0:;
  if (t4 >= t5) goto label2;
  v4 = t4;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t6 = v4;
  t7 = ((float *)((v3).f3).data) + gg_index(t6, ((int64_t)UINT64_C(4)));
  t8 = ((uint32_t)(v0));
  t9 = ((int64_t)(v4));
  t10 = ((int64_t)(v1));
  t11 = ((float)(v2));
  (*t7) = gg_function_5(t8, t9, t10, t11, base + 0);
  }
label1:;
  t4++; goto label0;
label2:;
  t0 = v3;
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ __forceinline__ gg_type_15 gg_function_45(int64_t p0, int64_t p1, int64_t base) {
  gg_type_15 t0;
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
  uint32_t t15;
  uint32_t t16;
  float t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  uint32_t t22;
  uint32_t t23;
  int64_t t24;
  float t25;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  float v6;
  uint32_t v7;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(32))));
  v2 = ((int64_t)(((int64_t)gg_div(t1, t2))));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(((int64_t)UINT64_C(16))));
  v3 = ((int64_t)(((int64_t)gg_mod(t3, t4))));
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(32))));
  t7 = ((int64_t)(((int64_t)gg_mod(t5, t6))));
  t8 = ((int64_t)(((int64_t)UINT64_C(16))));
  v4 = ((int64_t)(((int64_t)gg_div(t7, t8))));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(v2));
  t11 = ((int64_t)(((int64_t)UINT64_C(18))));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  v5 = ((int64_t)(((int64_t)((uint64_t)(t9) + (uint64_t)(t12)))));
  t13 = ((int64_t)(v5));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = dfa_ld_u16(t13, t14);
  t16 = ((uint32_t)(t15));
  t17 = dfa_half(t16);
  v6 = ((float)(t17));
  t18 = ((int64_t)(v5));
  t19 = ((int64_t)(((int64_t)UINT64_C(2))));
  t20 = ((int64_t)(v3));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) + (uint64_t)(t20)))));
  t22 = dfa_ld_u32(t18, t21);
  v7 = ((uint32_t)(t22));
  t23 = ((uint32_t)(v7));
  t24 = ((int64_t)(v4));
  t25 = ((float)(v6));
  t0 = gg_function_46(t23, t24, t25, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ __forceinline__ float gg_function_38(int64_t base) {
  float t0;
  float t1;
  float t2;
  {
  t1 = ((float)(((float)(gg_double_bits(UINT64_C(4613937818241073152))))));
  t2 = ((float)(((float)(gg_double_bits(UINT64_C(4604418109348737398))))));
  t0 = ((float)(((t1) * (t2))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ __forceinline__ float gg_function_37(float p0, float p1, int64_t base) {
  float t0;
  float t1;
  float t2;
  float t3;
  float t4;
  float t5;
  float v0;
  float v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((float)(v0));
  t2 = ((float)(v1));
  t3 = gg_function_38(base + 0);
  t4 = ((float)(t3));
  t5 = ((float)(((t2) + (t4))));
  t0 = gg_function_39(t1, t5, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ __forceinline__ float gg_function_36(float p0, float p1, uint32_t p2, int64_t base) {
  float t0;
  float t1;
  float t2;
  uint32_t t3;
  float t4;
  float t5;
  float t6;
  float v0;
  float v1;
  uint32_t v2;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((float)(v0));
  t2 = ((float)(v1));
  t3 = ((uint32_t)(v2));
  t4 = dfa_half(t3);
  t5 = ((float)(t4));
  t6 = ((float)(((t2) * (t5))));
  t0 = ((float)(((t1) + (t6))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ __forceinline__ float gg_function_34(float p0, float p1, int64_t base) {
  float t0;
  float t1;
  float t2;
  uint8_t t3;
  float t4;
  float t5;
  float t6;
  float t7;
  float v0;
  float v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((float)(v1));
  t2 = ((float)(gg_double_bits(UINT64_C(0))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((float)(v0));
  goto gg_done;
  }
  }
  t4 = ((float)(v1));
  t5 = ((float)(v0));
  t6 = tanhf(t5);
  t7 = ((float)(t6));
  t0 = ((float)(((t4) * (t7))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ __forceinline__ float gg_function_31(int64_t p0, uint32_t p1, uint32_t p2, float p3, float p4, float p5, float p6, int64_t p7, int64_t p8, int64_t p9, int64_t base) {
  float t0;
  int64_t t1;
  int64_t t2;
  int64_t t3;
  int64_t t4;
  int64_t t5;
  int64_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
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
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  uint32_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  uint8_t t34;
  int64_t t35;
  int64_t t36;
  uint8_t t37;
  int64_t t38;
  int64_t t39;
  uint8_t t40;
  int64_t t41;
  int64_t t42;
  int64_t t43;
  int64_t t44;
  uint32_t t45;
  uint32_t t46;
  float t47;
  uint32_t t48;
  uint32_t t49;
  uint32_t t50;
  uint32_t t51;
  int32_t t52;
  int32_t t53;
  int32_t t54;
  int32_t t55;
  float t56;
  float t57;
  float t58;
  float t59;
  float t60;
  float t61;
  float t62;
  float t63;
  int64_t t64;
  int64_t t65;
  int64_t v0;
  uint32_t v1;
  uint32_t v2;
  float v3;
  float v4;
  float v5;
  float v6;
  int64_t v7;
  int64_t v8;
  int64_t v9;
  float v10;
  int64_t v11;
  int64_t v12;
  int64_t v13;
  int64_t v14;
  int64_t v15;
  uint32_t v16;
  int64_t v17;
  uint32_t v18;
  float v19;
  float v20;
  float v21;
  uint32_t v22;
  int32_t v23;
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
  v10 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  v11 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label1:;
  t1 = ((int64_t)(v11));
  t2 = ((int64_t)(v7));
  t3 = ((int64_t)(((int64_t)UINT64_C(4))));
  t4 = ((int64_t)(((int64_t)gg_div(t2, t3))));
  if (!(((t1) < (t4)))) goto label2;
  {
  t5 = ((int64_t)(v11));
  t7 = ((int64_t)(v8));
  t8 = ((int64_t)(((int64_t)UINT64_C(32))));
  t9 = ((t7) == (t8));
  if (t9) {
  {
  t6 = ((int64_t)(v9));
  }
  } else {
  {
  t10 = ((int64_t)(v9));
  t11 = ((int64_t)(v8));
  t6 = ((int64_t)(((int64_t)gg_mod(t10, t11))));
  }
  }
  t12 = ((int64_t)(t6));
  v12 = ((int64_t)(((int64_t)((uint64_t)(t5) + (uint64_t)(t12)))));
  t13 = ((int64_t)(v12));
  t14 = ((int64_t)(((int64_t)UINT64_C(8))));
  v13 = ((int64_t)(((int64_t)gg_div(t13, t14))));
  t15 = ((int64_t)(v12));
  t16 = ((int64_t)(((int64_t)UINT64_C(4))));
  v14 = ((int64_t)(((int64_t)gg_mod(t15, t16))));
  t17 = ((int64_t)(v12));
  t18 = ((int64_t)(((int64_t)UINT64_C(4))));
  v15 = ((int64_t)(((t17) & (t18))));
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(v13));
  t21 = ((int64_t)(((int64_t)UINT64_C(18))));
  t22 = ((int64_t)(((int64_t)((uint64_t)(t20) * (uint64_t)(t21)))));
  t23 = ((int64_t)(((int64_t)UINT64_C(2))));
  t24 = ((int64_t)(((int64_t)((uint64_t)(t22) + (uint64_t)(t23)))));
  t25 = ((int64_t)(v14));
  t26 = ((int64_t)(((int64_t)UINT64_C(4))));
  t27 = ((int64_t)(((int64_t)((uint64_t)(t25) * (uint64_t)(t26)))));
  t28 = ((int64_t)(((int64_t)((uint64_t)(t24) + (uint64_t)(t27)))));
  t29 = dfa_ld_u32(t19, t28);
  v16 = ((uint32_t)(t29));
  t30 = ((int64_t)(v11));
  t31 = ((int64_t)(v8));
  v17 = ((int64_t)(((int64_t)gg_div(t30, t31))));
  t32 = ((int64_t)(v17));
  t33 = ((int64_t)(((int64_t)UINT64_C(0))));
  t34 = ((t32) == (t33));
  if (t34) {
  {
  v18 = ((uint32_t)(v1));
  }
  } else {
  {
  v18 = ((uint32_t)(v2));
  }
  }
  t35 = ((int64_t)(v17));
  t36 = ((int64_t)(((int64_t)UINT64_C(0))));
  t37 = ((t35) == (t36));
  if (t37) {
  {
  v19 = ((float)(v3));
  }
  } else {
  {
  v19 = ((float)(v5));
  }
  }
  t38 = ((int64_t)(v17));
  t39 = ((int64_t)(((int64_t)UINT64_C(0))));
  t40 = ((t38) == (t39));
  if (t40) {
  {
  v20 = ((float)(v4));
  }
  } else {
  {
  v20 = ((float)(v6));
  }
  }
  t41 = ((int64_t)(v0));
  t42 = ((int64_t)(v13));
  t43 = ((int64_t)(((int64_t)UINT64_C(18))));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) * (uint64_t)(t43)))));
  t45 = dfa_ld_u16(t41, t44);
  t46 = ((uint32_t)(t45));
  t47 = dfa_half(t46);
  v21 = ((float)(t47));
  t48 = ((uint32_t)(v16));
  t49 = ((uint32_t)(((uint32_t)(v15))));
  t50 = ((uint32_t)(((t48) >> ((uint64_t)(t49) & 31))));
  t51 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(252645135))))));
  v22 = ((uint32_t)(((t50) & (t51))));
  t52 = ((int32_t)(((int32_t)(v22))));
  t53 = ((int32_t)(((int32_t)(v18))));
  t54 = ((int32_t)(((int64_t)UINT64_C(0))));
  t55 = dfa_dp4a(t52, t53, t54);
  v23 = ((int32_t)(t55));
  t56 = v10;
  t57 = ((float)(v21));
  t58 = ((float)(((float)(v23))));
  t59 = ((float)(v19));
  t60 = ((float)(((t58) * (t59))));
  t61 = ((float)(v20));
  t62 = ((float)(((t60) - (t61))));
  t63 = ((float)(((t57) * (t62))));
  v10 = ((t56) + (t63));
  t64 = v11;
  t65 = ((int64_t)(v8));
  v11 = ((int64_t)((uint64_t)(t64) + (uint64_t)(t65)));
  }
  goto label1;
label2:;
  t0 = ((float)(v10));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ __forceinline__ int32_t gg_function_25(float p0, float p1, int64_t base) {
  int32_t t0;
  float t1;
  float t2;
  uint8_t t3;
  float t4;
  float t5;
  float t6;
  float t7;
  float t8;
  int32_t t9;
  float v0;
  float v1;
  v0 = p0;
  v1 = p1;
  {
  t1 = ((float)(v1));
  t2 = ((float)(gg_double_bits(UINT64_C(0))));
  t3 = ((t1) == (t2));
  if (t3) {
  {
  t0 = ((int32_t)(((int64_t)UINT64_C(0))));
  goto gg_done;
  }
  }
  t4 = ((float)(v0));
  t5 = ((float)(v1));
  t6 = ((float)(((t4) / (t5))));
  t7 = roundf(t6);
  t8 = ((float)(t7));
  t9 = dfa_f2i(t8);
  t0 = ((int32_t)(t9));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ __forceinline__ uint32_t gg_function_24(float p0, float p1, float p2, float p3, float p4, int64_t base) {
  uint32_t t0;
  float t1;
  float t2;
  int32_t t3;
  uint32_t t4;
  uint32_t t5;
  float t6;
  float t7;
  int32_t t8;
  uint32_t t9;
  uint32_t t10;
  float t11;
  float t12;
  int32_t t13;
  uint32_t t14;
  uint32_t t15;
  float t16;
  float t17;
  int32_t t18;
  uint32_t t19;
  uint32_t t20;
  uint32_t t21;
  uint32_t t22;
  uint32_t t23;
  uint32_t t24;
  uint32_t t25;
  uint32_t t26;
  uint32_t t27;
  uint32_t t28;
  uint32_t t29;
  uint32_t t30;
  uint32_t t31;
  uint32_t t32;
  float v0;
  float v1;
  float v2;
  float v3;
  float v4;
  uint32_t v5;
  uint32_t v6;
  uint32_t v7;
  uint32_t v8;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((float)(v0));
  t2 = ((float)(v4));
  t3 = gg_function_25(t1, t2, base + 0);
  t4 = ((uint32_t)(((uint32_t)(t3))));
  t5 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(255))))));
  v5 = ((uint32_t)(((t4) & (t5))));
  t6 = ((float)(v1));
  t7 = ((float)(v4));
  t8 = gg_function_25(t6, t7, base + 0);
  t9 = ((uint32_t)(((uint32_t)(t8))));
  t10 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(255))))));
  v6 = ((uint32_t)(((t9) & (t10))));
  t11 = ((float)(v2));
  t12 = ((float)(v4));
  t13 = gg_function_25(t11, t12, base + 0);
  t14 = ((uint32_t)(((uint32_t)(t13))));
  t15 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(255))))));
  v7 = ((uint32_t)(((t14) & (t15))));
  t16 = ((float)(v3));
  t17 = ((float)(v4));
  t18 = gg_function_25(t16, t17, base + 0);
  t19 = ((uint32_t)(((uint32_t)(t18))));
  t20 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(255))))));
  v8 = ((uint32_t)(((t19) & (t20))));
  t21 = ((uint32_t)(v5));
  t22 = ((uint32_t)(v6));
  t23 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(8))))));
  t24 = ((uint32_t)(((uint32_t)((uint64_t)(t22) << ((uint64_t)(t23) & 31)))));
  t25 = ((uint32_t)(((t21) | (t24))));
  t26 = ((uint32_t)(v7));
  t27 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(16))))));
  t28 = ((uint32_t)(((uint32_t)((uint64_t)(t26) << ((uint64_t)(t27) & 31)))));
  t29 = ((uint32_t)(((t25) | (t28))));
  t30 = ((uint32_t)(v8));
  t31 = ((uint32_t)(((uint32_t)(((int64_t)UINT64_C(24))))));
  t32 = ((uint32_t)(((uint32_t)((uint64_t)(t30) << ((uint64_t)(t31) & 31)))));
  t0 = ((uint32_t)(((t29) | (t32))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ __forceinline__ float gg_function_22(float p0, float p1, float p2, float p3, int64_t base) {
  float t0;
  float t1;
  float t2;
  float t3;
  float t4;
  float t5;
  float t6;
  float v0;
  float v1;
  float v2;
  float v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((float)(v0));
  t2 = ((float)(v1));
  t3 = ((float)(((t1) + (t2))));
  t4 = ((float)(v2));
  t5 = ((float)(((t3) + (t4))));
  t6 = ((float)(v3));
  t0 = ((float)(((t5) + (t6))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ __forceinline__ float gg_function_19(float p0, float p1, float p2, float p3, int64_t base) {
  float t0;
  float t1;
  float t2;
  float t3;
  float t4;
  float t5;
  float t6;
  float t7;
  float t8;
  float t9;
  float t10;
  float t11;
  float t12;
  float t13;
  float t14;
  float t15;
  float t16;
  float t17;
  float v0;
  float v1;
  float v2;
  float v3;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  {
  t1 = ((float)(v0));
  t2 = fabsf(t1);
  t3 = ((float)(t2));
  t4 = ((float)(v1));
  t5 = fabsf(t4);
  t6 = ((float)(t5));
  t7 = fmaxf(t3, t6);
  t8 = ((float)(t7));
  t9 = ((float)(v2));
  t10 = fabsf(t9);
  t11 = ((float)(t10));
  t12 = ((float)(v3));
  t13 = fabsf(t12);
  t14 = ((float)(t13));
  t15 = fmaxf(t11, t14);
  t16 = ((float)(t15));
  t17 = fmaxf(t8, t16);
  t0 = ((float)(t17));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ __forceinline__ int64_t gg_function_16(int64_t p0, int64_t base) {
  int64_t t0;
  int64_t t1;
  int64_t t2;
  int64_t v0;
  v0 = p0;
  {
  t1 = ((int64_t)(((int64_t)UINT64_C(16))));
  t2 = ((int64_t)(v0));
  t0 = ((int64_t)(((int64_t)((uint64_t)(t1) * (uint64_t)(t2)))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __device__ __forceinline__ void gg_function_17(int64_t p0, float p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t base) {
  int64_t t0;
  int64_t t1;
  uint8_t t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  float t7;
  int64_t t8;
  int64_t t9;
  int64_t t10;
  float t11;
  float t12;
  float t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  int64_t t18;
  int64_t t19;
  float t20;
  float t21;
  float t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  float t29;
  float t30;
  float t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  float t38;
  float t39;
  float t40;
  float t41;
  float t42;
  float t43;
  float t44;
  float t45;
  float t46;
  float t47;
  float t48;
  float t49;
  int64_t t50;
  float t51;
  float t52;
  float t53;
  float t54;
  float t55;
  int64_t t56;
  float t57;
  float t58;
  float t59;
  float t60;
  int64_t t61;
  float t62;
  float t63;
  float t64;
  float t65;
  float t66;
  int64_t t67;
  float t68;
  float t69;
  float t70;
  float t71;
  int64_t t72;
  float t73;
  float t74;
  float t75;
  float t76;
  float t77;
  int64_t t78;
  float t79;
  float t80;
  float t81;
  float t82;
  float t83;
  float t84;
  float t85;
  float t86;
  float t87;
  int64_t t88;
  int64_t t89;
  int64_t t90;
  int64_t t91;
  int64_t t92;
  uint32_t t93;
  int64_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  uint8_t t98;
  uint8_t t99;
  uint8_t t100;
  int64_t t101;
  int64_t t102;
  int64_t t103;
  int64_t t104;
  int64_t t105;
  int64_t t106;
  int64_t t107;
  int64_t t108;
  int64_t t109;
  float t110;
  int64_t t111;
  int64_t t112;
  int64_t t113;
  int64_t t114;
  int64_t t115;
  int64_t t116;
  int64_t t117;
  float t118;
  int64_t v0;
  float v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  uint8_t v7;
  float v8;
  float v9;
  float v10;
  float v11;
  float v12;
  float v13;
  float v14;
  uint32_t v15;
  int64_t v16;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  v5 = p5;
  v6 = p6;
  {
  t0 = ((int64_t)(v2));
  t1 = ((int64_t)(((int64_t)UINT64_C(32))));
  t2 = ((t0) == (t1));
  t3 = t2;
  if (!t3) {
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(v2));
  t3 = ((t4) < (t5));
  }
  v7 = t3;
  v8 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  v9 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  v10 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  v11 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  t6 = v7;
  if (t6) {
  {
  t7 = ((float)(v1));
  t8 = ((int64_t)(v0));
  t9 = ((int64_t)(v3));
  t10 = gg_function_16(t9, base + 0);
  t11 = dfa_ld_f32(t8, t10);
  t12 = ((float)(t11));
  v8 = ((float)(((t7) * (t12))));
  t13 = ((float)(v1));
  t14 = ((int64_t)(v0));
  t15 = ((int64_t)(v3));
  t16 = gg_function_16(t15, base + 0);
  t17 = ((int64_t)(t16));
  t18 = ((int64_t)(((int64_t)UINT64_C(4))));
  t19 = ((int64_t)(((int64_t)((uint64_t)(t17) + (uint64_t)(t18)))));
  t20 = dfa_ld_f32(t14, t19);
  t21 = ((float)(t20));
  v9 = ((float)(((t13) * (t21))));
  t22 = ((float)(v1));
  t23 = ((int64_t)(v0));
  t24 = ((int64_t)(v3));
  t25 = gg_function_16(t24, base + 0);
  t26 = ((int64_t)(t25));
  t27 = ((int64_t)(((int64_t)UINT64_C(8))));
  t28 = ((int64_t)(((int64_t)((uint64_t)(t26) + (uint64_t)(t27)))));
  t29 = dfa_ld_f32(t23, t28);
  t30 = ((float)(t29));
  v10 = ((float)(((t22) * (t30))));
  t31 = ((float)(v1));
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(v3));
  t34 = gg_function_16(t33, base + 0);
  t35 = ((int64_t)(t34));
  t36 = ((int64_t)(((int64_t)UINT64_C(12))));
  t37 = ((int64_t)(((int64_t)((uint64_t)(t35) + (uint64_t)(t36)))));
  t38 = dfa_ld_f32(t32, t37);
  t39 = ((float)(t38));
  v11 = ((float)(((t31) * (t39))));
  }
  }
  t40 = ((float)(v8));
  t41 = ((float)(v9));
  t42 = ((float)(v10));
  t43 = ((float)(v11));
  v12 = gg_function_19(t40, t41, t42, t43, base + 0);
  t44 = ((float)(v8));
  t45 = ((float)(v9));
  t46 = ((float)(v10));
  t47 = ((float)(v11));
  v13 = gg_function_22(t44, t45, t46, t47, base + 0);
  t48 = ((float)(v12));
  t49 = ((float)(v12));
  t50 = ((int64_t)(((int64_t)UINT64_C(4))));
  t51 = dfa_shfl_xor_f32(t49, t50);
  t52 = ((float)(t51));
  t53 = fmaxf(t48, t52);
  v12 = ((float)(t53));
  t54 = v13;
  t55 = ((float)(v13));
  t56 = ((int64_t)(((int64_t)UINT64_C(4))));
  t57 = dfa_shfl_xor_f32(t55, t56);
  t58 = ((float)(t57));
  v13 = ((t54) + (t58));
  t59 = ((float)(v12));
  t60 = ((float)(v12));
  t61 = ((int64_t)(((int64_t)UINT64_C(2))));
  t62 = dfa_shfl_xor_f32(t60, t61);
  t63 = ((float)(t62));
  t64 = fmaxf(t59, t63);
  v12 = ((float)(t64));
  t65 = v13;
  t66 = ((float)(v13));
  t67 = ((int64_t)(((int64_t)UINT64_C(2))));
  t68 = dfa_shfl_xor_f32(t66, t67);
  t69 = ((float)(t68));
  v13 = ((t65) + (t69));
  t70 = ((float)(v12));
  t71 = ((float)(v12));
  t72 = ((int64_t)(((int64_t)UINT64_C(1))));
  t73 = dfa_shfl_xor_f32(t71, t72);
  t74 = ((float)(t73));
  t75 = fmaxf(t70, t74);
  v12 = ((float)(t75));
  t76 = v13;
  t77 = ((float)(v13));
  t78 = ((int64_t)(((int64_t)UINT64_C(1))));
  t79 = dfa_shfl_xor_f32(t77, t78);
  t80 = ((float)(t79));
  v13 = ((t76) + (t80));
  t81 = ((float)(v12));
  t82 = ((float)(((float)(gg_double_bits(UINT64_C(4638637247447433216))))));
  v14 = ((float)(((t81) / (t82))));
  t83 = ((float)(v8));
  t84 = ((float)(v9));
  t85 = ((float)(v10));
  t86 = ((float)(v11));
  t87 = ((float)(v14));
  v15 = gg_function_24(t83, t84, t85, t86, t87, base + 0);
  t88 = ((int64_t)(v4));
  t89 = ((int64_t)(v6));
  t90 = ((int64_t)(((int64_t)((uint64_t)(t88) + (uint64_t)(t89)))));
  t91 = ((int64_t)(v3));
  t92 = ((int64_t)(((int64_t)((uint64_t)(t90) + (uint64_t)(t91)))));
  t93 = ((uint32_t)(v15));
  dfa_sh_set_u32(t92, t93);
  t94 = ((int64_t)(v3));
  t95 = ((int64_t)(((int64_t)UINT64_C(8))));
  t96 = ((int64_t)(((int64_t)gg_mod(t94, t95))));
  t97 = ((int64_t)(((int64_t)UINT64_C(0))));
  t98 = ((t96) == (t97));
  t99 = t98;
  if (t99) {
  t99 = v7;
  }
  t100 = t99;
  if (t100) {
  {
  t101 = ((int64_t)(v6));
  t102 = ((int64_t)(v3));
  t103 = ((int64_t)(((int64_t)((uint64_t)(t101) + (uint64_t)(t102)))));
  t104 = ((int64_t)(((int64_t)UINT64_C(8))));
  v16 = ((int64_t)(((int64_t)gg_div(t103, t104))));
  t105 = ((int64_t)(v5));
  t106 = ((int64_t)(((int64_t)UINT64_C(2))));
  t107 = ((int64_t)(v16));
  t108 = ((int64_t)(((int64_t)((uint64_t)(t106) * (uint64_t)(t107)))));
  t109 = ((int64_t)(((int64_t)((uint64_t)(t105) + (uint64_t)(t108)))));
  t110 = ((float)(v14));
  dfa_sh_set_f32(t109, t110);
  t111 = ((int64_t)(v5));
  t112 = ((int64_t)(((int64_t)UINT64_C(2))));
  t113 = ((int64_t)(v16));
  t114 = ((int64_t)(((int64_t)((uint64_t)(t112) * (uint64_t)(t113)))));
  t115 = ((int64_t)(((int64_t)((uint64_t)(t111) + (uint64_t)(t114)))));
  t116 = ((int64_t)(((int64_t)UINT64_C(1))));
  t117 = ((int64_t)(((int64_t)((uint64_t)(t115) + (uint64_t)(t116)))));
  t118 = ((float)(v13));
  dfa_sh_set_f32(t117, t118);
  }
  }
  }
gg_done:;
}
static __device__ __forceinline__ void gg_function_13(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  int64_t t0;
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
  uint8_t t12;
  uint8_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  uint8_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  int64_t t22;
  int64_t t23;
  uint32_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  uint8_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
  int64_t t36;
  int64_t t37;
  int64_t t38;
  int64_t t39;
  int64_t t40;
  float t41;
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
  float t53;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  v3 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label1:;
  t0 = ((int64_t)(v3));
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(4))));
  t3 = ((int64_t)(((int64_t)gg_div(t1, t2))));
  if (!(((t0) < (t3)))) goto label2;
  {
  t4 = ((int64_t)(v3));
  t5 = ((int64_t)(v2));
  v4 = ((int64_t)(((int64_t)((uint64_t)(t4) + (uint64_t)(t5)))));
  t6 = ((int64_t)(v3));
  t7 = ((int64_t)(((int64_t)UINT64_C(32))));
  t8 = ((int64_t)(((int64_t)((uint64_t)(t6) + (uint64_t)(t7)))));
  t9 = ((int64_t)(v1));
  t10 = ((int64_t)(((int64_t)UINT64_C(4))));
  t11 = ((int64_t)(((int64_t)gg_div(t9, t10))));
  t12 = ((t8) <= (t11));
  t13 = t12;
  if (!t13) {
  t14 = ((int64_t)(v4));
  t15 = ((int64_t)(v1));
  t16 = ((int64_t)(((int64_t)UINT64_C(4))));
  t17 = ((int64_t)(((int64_t)gg_div(t15, t16))));
  t13 = ((t14) < (t17));
  }
  t18 = t13;
  if (t18) {
  {
  t19 = ((int64_t)(v0));
  t20 = ((int64_t)(v1));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  t22 = ((int64_t)(v4));
  t23 = ((int64_t)(((int64_t)((uint64_t)(t21) + (uint64_t)(t22)))));
  t24 = ((uint32_t)(((int64_t)UINT64_C(0))));
  dfa_sh_set_u32(t23, t24);
  }
  }
  t25 = v3;
  t26 = ((int64_t)(((int64_t)UINT64_C(32))));
  v3 = ((int64_t)((uint64_t)(t25) + (uint64_t)(t26)));
  }
  goto label1;
label2:;
  t27 = ((int64_t)(v2));
  t28 = ((int64_t)(v1));
  t29 = ((int64_t)(((int64_t)UINT64_C(32))));
  t30 = ((int64_t)(((int64_t)gg_div(t28, t29))));
  t31 = ((t27) < (t30));
  if (t31) {
  {
  t32 = ((int64_t)(v0));
  t33 = ((int64_t)(v1));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) * (uint64_t)(t33)))));
  t35 = ((int64_t)(v1));
  t36 = ((int64_t)(((int64_t)((uint64_t)(t34) + (uint64_t)(t35)))));
  t37 = ((int64_t)(((int64_t)UINT64_C(2))));
  t38 = ((int64_t)(v2));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t37) * (uint64_t)(t38)))));
  t40 = ((int64_t)(((int64_t)((uint64_t)(t36) + (uint64_t)(t39)))));
  t41 = ((float)(gg_double_bits(UINT64_C(0))));
  dfa_sh_set_f32(t40, t41);
  t42 = ((int64_t)(v0));
  t43 = ((int64_t)(v1));
  t44 = ((int64_t)(((int64_t)((uint64_t)(t42) * (uint64_t)(t43)))));
  t45 = ((int64_t)(v1));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) + (uint64_t)(t45)))));
  t47 = ((int64_t)(((int64_t)UINT64_C(2))));
  t48 = ((int64_t)(v2));
  t49 = ((int64_t)(((int64_t)((uint64_t)(t47) * (uint64_t)(t48)))));
  t50 = ((int64_t)(((int64_t)((uint64_t)(t46) + (uint64_t)(t49)))));
  t51 = ((int64_t)(((int64_t)UINT64_C(1))));
  t52 = ((int64_t)(((int64_t)((uint64_t)(t50) + (uint64_t)(t51)))));
  t53 = ((float)(gg_double_bits(UINT64_C(0))));
  dfa_sh_set_f32(t52, t53);
  }
  }
  }
gg_done:;
}
static __host__ __device__ __forceinline__ float gg_function_10(float p0, int64_t p1, int64_t p2, float p3, float p4, int64_t base) {
  float t0;
  float t1;
  float t2;
  uint8_t t3;
  int64_t t4;
  int64_t t5;
  uint8_t t6;
  int64_t t7;
  int64_t t8;
  uint8_t t9;
  int64_t t10;
  int64_t t11;
  int64_t t12;
  int64_t t13;
  int64_t t14;
  int64_t t15;
  int64_t t16;
  int64_t t17;
  float t18;
  float t19;
  float t20;
  float v0;
  int64_t v1;
  int64_t v2;
  float v3;
  float v4;
  float v5;
  int64_t v6;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  v3 = p3;
  v4 = p4;
  {
  t1 = ((float)(v0));
  t2 = ((float)(gg_double_bits(UINT64_C(0))));
  t3 = ((t1) <= (t2));
  if (t3) {
  {
  t0 = ((float)(((float)(gg_double_bits(UINT64_C(4607182418800017408))))));
  goto gg_done;
  }
  }
  t4 = ((int64_t)(v1));
  t5 = ((int64_t)(v2));
  t6 = ((t4) < (t5));
  if (t6) {
  {
  v5 = ((float)(v3));
  }
  } else {
  {
  v5 = ((float)(v4));
  }
  }
  t7 = ((int64_t)(v1));
  t8 = ((int64_t)(v2));
  t9 = ((t7) < (t8));
  if (t9) {
  {
  t10 = ((int64_t)(v1));
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t10) + (uint64_t)(t11)))));
  }
  } else {
  {
  t12 = ((int64_t)(((int64_t)UINT64_C(2))));
  t13 = ((int64_t)(v1));
  t14 = ((int64_t)(v2));
  t15 = ((int64_t)(((int64_t)((uint64_t)(t13) - (uint64_t)(t14)))));
  t16 = ((int64_t)(((int64_t)((uint64_t)(t12) * (uint64_t)(t15)))));
  t17 = ((int64_t)(((int64_t)UINT64_C(1))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t16) + (uint64_t)(t17)))));
  }
  }
  t18 = ((float)(v5));
  t19 = ((float)(((float)(v6))));
  t20 = powf(t18, t19);
  t0 = ((float)(t20));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __device__ __forceinline__ float gg_function_6(int64_t p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t p6, int64_t p7, float p8, float p9, float p10, float p11, int64_t p12, float p13, int64_t p14, int64_t p15, int64_t p16, int64_t p17, int64_t p18, int64_t p19, int64_t p20, int64_t p21, int64_t p22, int64_t p23, int64_t p24, int64_t p25, int64_t p26, int64_t p27, int64_t p28, int64_t p29, int64_t p30, int64_t p31, int64_t p32, int64_t p33, int64_t p34, int64_t p35, int64_t p36, int64_t p37, int64_t p38, int64_t p39, int64_t p40, int64_t p41, int64_t base) {
  float t0;
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
  int64_t t21;
  int64_t t22;
  int64_t t23;
  int64_t t24;
  int64_t t25;
  int64_t t26;
  int64_t t27;
  int64_t t28;
  int64_t t29;
  int64_t t30;
  int64_t t31;
  int64_t t32;
  int64_t t33;
  int64_t t34;
  int64_t t35;
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
  uint8_t t61;
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
  float t72;
  int64_t t73;
  int64_t t74;
  float t75;
  float t76;
  int64_t t77;
  int64_t t78;
  int64_t t79;
  int64_t t80;
  uint8_t t81;
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
  uint8_t t94;
  int64_t t95;
  int64_t t96;
  int64_t t97;
  int64_t t98;
  int64_t t99;
  int64_t t100;
  gg_type_9 t101;
  uint32_t t102;
  uint32_t t103;
  int64_t t104;
  gg_type_9 t105;
  int64_t t106;
  gg_type_11 t107;
  float t108;
  float t109;
  int64_t t110;
  gg_type_11 t111;
  int64_t t112;
  gg_type_12 t113;
  gg_type_11 t114;
  float t115;
  float t116;
  int64_t t117;
  gg_type_11 t118;
  int64_t t119;
  gg_type_12 t120;
  int64_t t121;
  float t122;
  float t123;
  int64_t t124;
  float t125;
  float t126;
  int64_t t127;
  float t128;
  float t129;
  int64_t t130;
  float t131;
  float t132;
  int64_t t133;
  float t134;
  float t135;
  int64_t t136;
  int64_t t137;
  int64_t t138;
  int64_t t139;
  float * t140;
  float t141;
  int64_t t142;
  float * t143;
  int64_t t144;
  int64_t t145;
  int64_t t146;
  int64_t t147;
  uint8_t t148;
  int64_t t149;
  int64_t t150;
  int64_t t151;
  int64_t t152;
  int64_t t153;
  int64_t t154;
  int64_t t155;
  int64_t t156;
  int64_t t157;
  int64_t t158;
  uint8_t t159;
  uint8_t t160;
  int64_t t161;
  int64_t t162;
  uint8_t t163;
  uint8_t t164;
  int64_t t165;
  int64_t t166;
  uint8_t t167;
  uint8_t t168;
  int64_t t169;
  int64_t t170;
  int64_t t171;
  int64_t t172;
  uint8_t t173;
  uint8_t t174;
  uint8_t t175;
  int64_t t176;
  int64_t t177;
  int64_t t178;
  int64_t t179;
  int64_t t180;
  int64_t t181;
  int64_t t182;
  int64_t t183;
  int64_t t184;
  int64_t t185;
  int64_t t186;
  int64_t t187;
  int64_t t188;
  int64_t t189;
  int64_t t190;
  int64_t t191;
  float t192;
  int64_t t193;
  int64_t t194;
  int64_t t195;
  int64_t t196;
  int64_t t197;
  int64_t t198;
  int64_t t199;
  int64_t t200;
  int64_t t201;
  int64_t t202;
  int64_t t203;
  int64_t t204;
  int64_t t205;
  int64_t t206;
  int64_t t207;
  int64_t t208;
  int64_t t209;
  int64_t t210;
  int64_t t211;
  int64_t t212;
  int64_t t213;
  int64_t t214;
  int64_t t215;
  int64_t t216;
  int64_t t217;
  uint8_t t218;
  int64_t t219;
  int64_t t220;
  int64_t t221;
  int64_t t222;
  int64_t t223;
  int64_t t224;
  gg_type_9 * t225;
  gg_slice t226;
  int64_t t227;
  uint32_t * t228;
  int64_t t229;
  int64_t t230;
  int64_t t231;
  int64_t t232;
  int64_t t233;
  uint32_t t234;
  int64_t t235;
  int64_t t236;
  int64_t t237;
  gg_type_11 * t238;
  gg_slice t239;
  int64_t t240;
  int64_t t241;
  int64_t t242;
  float * t243;
  int64_t t244;
  int64_t t245;
  int64_t t246;
  int64_t t247;
  int64_t t248;
  int64_t t249;
  int64_t t250;
  int64_t t251;
  int64_t t252;
  float t253;
  int64_t t254;
  gg_type_11 * t255;
  gg_slice t256;
  int64_t t257;
  int64_t t258;
  int64_t t259;
  int64_t t260;
  int64_t t261;
  float * t262;
  int64_t t263;
  int64_t t264;
  int64_t t265;
  int64_t t266;
  int64_t t267;
  int64_t t268;
  int64_t t269;
  int64_t t270;
  int64_t t271;
  int64_t t272;
  int64_t t273;
  float t274;
  int64_t t275;
  int64_t t276;
  int64_t t277;
  int64_t t278;
  int64_t t279;
  int64_t t280;
  uint8_t t281;
  int64_t t282;
  int64_t t283;
  int64_t t284;
  int64_t t285;
  int64_t t286;
  int64_t t287;
  int64_t t288;
  int64_t t289;
  uint32_t t290;
  int64_t t291;
  int64_t t292;
  int64_t t293;
  int64_t t294;
  int64_t t295;
  int64_t t296;
  int64_t t297;
  int64_t t298;
  int64_t t299;
  int64_t t300;
  int64_t t301;
  int64_t t302;
  int64_t t303;
  int64_t t304;
  int64_t t305;
  int64_t t306;
  int64_t t307;
  int64_t t308;
  int64_t t309;
  int64_t t310;
  int64_t t311;
  int64_t t312;
  int64_t t313;
  int64_t t314;
  int64_t t315;
  float * t316;
  int64_t t317;
  float * t318;
  int64_t t319;
  int64_t t320;
  int64_t t321;
  int64_t t322;
  int64_t t323;
  int64_t t324;
  int64_t t325;
  int64_t t326;
  uint8_t t327;
  int64_t t328;
  int64_t t329;
  int64_t t330;
  int64_t t331;
  int64_t t332;
  int64_t t333;
  int64_t t334;
  int64_t t335;
  int64_t t336;
  int64_t t337;
  int64_t t338;
  uint8_t t339;
  uint8_t t340;
  int64_t t341;
  int64_t t342;
  int64_t t343;
  int64_t t344;
  uint8_t t345;
  int64_t t346;
  int64_t t347;
  int64_t t348;
  int64_t t349;
  int64_t t350;
  int64_t t351;
  gg_type_9 * t352;
  gg_slice t353;
  int64_t t354;
  uint32_t * t355;
  uint32_t t356;
  int64_t t357;
  gg_type_9 * t358;
  gg_slice t359;
  int64_t t360;
  uint32_t * t361;
  uint32_t t362;
  int64_t t363;
  gg_type_11 * t364;
  gg_slice t365;
  int64_t t366;
  float * t367;
  float t368;
  int64_t t369;
  gg_type_11 * t370;
  gg_slice t371;
  int64_t t372;
  float * t373;
  float t374;
  int64_t t375;
  gg_type_11 * t376;
  gg_slice t377;
  int64_t t378;
  float * t379;
  float t380;
  int64_t t381;
  gg_type_11 * t382;
  gg_slice t383;
  int64_t t384;
  float * t385;
  float t386;
  int64_t t387;
  int64_t t388;
  int64_t t389;
  float t390;
  int64_t t391;
  int64_t t392;
  int64_t t393;
  uint8_t t394;
  float t395;
  float t396;
  int64_t t397;
  int64_t t398;
  uint8_t t399;
  uint8_t t400;
  uint8_t t401;
  float t402;
  float t403;
  int64_t t404;
  int64_t t405;
  int64_t t406;
  int64_t t407;
  int64_t t408;
  int64_t t409;
  int64_t t410;
  int64_t t411;
  uint32_t t412;
  uint32_t t413;
  int64_t t414;
  float * t415;
  int64_t t416;
  float * t417;
  float t418;
  float t419;
  int64_t t420;
  int64_t t421;
  uint8_t t422;
  int64_t t423;
  int64_t t424;
  int64_t t425;
  int64_t t426;
  uint8_t t427;
  int64_t t428;
  float * t429;
  int64_t t430;
  int64_t t431;
  int64_t t432;
  int64_t t433;
  int64_t t434;
  int64_t t435;
  int64_t t436;
  int64_t t437;
  int64_t t438;
  float * t439;
  int64_t t440;
  float * t441;
  float t442;
  int64_t t443;
  float * t444;
  float t445;
  int64_t t446;
  float t447;
  float t448;
  int64_t t449;
  int64_t t450;
  int64_t t451;
  float * t452;
  float t453;
  int64_t t454;
  float * t455;
  float t456;
  int64_t t457;
  float * t458;
  int64_t t459;
  float * t460;
  int64_t t461;
  float * t462;
  int64_t t463;
  float * t464;
  float t465;
  int64_t t466;
  float * t467;
  float t468;
  int64_t t469;
  float * t470;
  int64_t t471;
  float * t472;
  float t473;
  float t474;
  int64_t t475;
  float * t476;
  float t477;
  int64_t t478;
  int64_t t479;
  int64_t t480;
  int64_t t481;
  int64_t t482;
  int64_t t483;
  float * t484;
  float t485;
  int64_t t486;
  int64_t t487;
  int64_t t488;
  gg_type_12 * t489;
  gg_slice t490;
  int64_t t491;
  gg_type_11 * t492;
  gg_slice t493;
  int64_t t494;
  float * t495;
  int64_t t496;
  gg_type_12 * t497;
  gg_slice t498;
  int64_t t499;
  gg_type_11 * t500;
  gg_slice t501;
  int64_t t502;
  float * t503;
  float t504;
  float t505;
  int64_t t506;
  gg_type_12 * t507;
  gg_slice t508;
  int64_t t509;
  gg_type_11 * t510;
  gg_slice t511;
  int64_t t512;
  float * t513;
  int64_t t514;
  gg_type_12 * t515;
  gg_slice t516;
  int64_t t517;
  gg_type_11 * t518;
  gg_slice t519;
  int64_t t520;
  float * t521;
  float t522;
  float t523;
  int64_t t524;
  int64_t t525;
  int64_t t526;
  int64_t t527;
  int64_t t528;
  int64_t t529;
  int64_t t530;
  int64_t t531;
  int64_t t532;
  int64_t t533;
  int64_t t534;
  int64_t t535;
  int64_t t536;
  int64_t t537;
  uint8_t t538;
  int64_t t539;
  int64_t t540;
  int64_t t541;
  int64_t t542;
  int64_t t543;
  int64_t t544;
  int64_t t545;
  uint8_t t546;
  int64_t t547;
  float * t548;
  int64_t t549;
  int64_t t550;
  int64_t t551;
  int64_t t552;
  int64_t t553;
  float t554;
  int64_t t555;
  int64_t t556;
  int64_t t557;
  int64_t t558;
  int64_t t559;
  int64_t t560;
  int64_t t561;
  int64_t t562;
  int64_t t563;
  int64_t t564;
  uint8_t t565;
  int64_t t566;
  int64_t t567;
  int64_t t568;
  int64_t t569;
  int64_t t570;
  int64_t t571;
  int64_t t572;
  int64_t t573;
  int64_t t574;
  int64_t t575;
  int64_t t576;
  int64_t t577;
  int64_t t578;
  int64_t t579;
  int64_t t580;
  int64_t t581;
  int64_t t582;
  int64_t t583;
  int64_t t584;
  int64_t t585;
  int64_t t586;
  int64_t t587;
  gg_type_12 * t588;
  gg_slice t589;
  int64_t t590;
  gg_type_11 * t591;
  gg_slice t592;
  int64_t t593;
  float * t594;
  int64_t t595;
  gg_type_12 * t596;
  gg_slice t597;
  int64_t t598;
  gg_type_11 * t599;
  gg_slice t600;
  int64_t t601;
  float * t602;
  float t603;
  float t604;
  gg_slice t605;
  int64_t t606;
  int64_t t607;
  int64_t t608;
  float * t609;
  float t610;
  int64_t t611;
  float * t612;
  float t613;
  int64_t t614;
  gg_type_12 * t615;
  gg_slice t616;
  int64_t t617;
  gg_type_11 * t618;
  gg_slice t619;
  int64_t t620;
  float * t621;
  int64_t t622;
  gg_type_12 * t623;
  gg_slice t624;
  int64_t t625;
  gg_type_11 * t626;
  gg_slice t627;
  int64_t t628;
  float * t629;
  float t630;
  float t631;
  gg_slice t632;
  int64_t t633;
  int64_t t634;
  int64_t t635;
  int64_t t636;
  int64_t t637;
  float * t638;
  float t639;
  int64_t t640;
  float * t641;
  float t642;
  int64_t t643;
  int64_t t644;
  int64_t t645;
  int64_t t646;
  int64_t t647;
  int64_t t648;
  int64_t t649;
  int64_t t650;
  int64_t t651;
  int64_t t652;
  int64_t t653;
  int64_t t654;
  int64_t t655;
  int64_t t656;
  int64_t t657;
  int64_t t658;
  int64_t t659;
  int64_t t660;
  int64_t t661;
  int64_t t662;
  int64_t t663;
  int64_t t664;
  int64_t t665;
  int64_t t666;
  int64_t t667;
  int64_t t668;
  int64_t t669;
  int64_t t670;
  int64_t t671;
  int64_t t672;
  int64_t t673;
  int64_t t674;
  int64_t t675;
  int64_t t676;
  int64_t t677;
  int64_t t678;
  uint8_t t679;
  uint8_t t680;
  int64_t t681;
  int64_t t682;
  uint8_t t683;
  int64_t t684;
  int64_t t685;
  int64_t t686;
  int64_t t687;
  float t688;
  int64_t t689;
  int64_t t690;
  int64_t t691;
  int64_t t692;
  int64_t t693;
  int64_t t694;
  int64_t t695;
  int64_t t696;
  uint8_t t697;
  uint8_t t698;
  int64_t t699;
  int64_t t700;
  uint8_t t701;
  uint8_t t702;
  float t703;
  int64_t t704;
  float * t705;
  float t706;
  int64_t t707;
  float * t708;
  float t709;
  float t710;
  int64_t t711;
  float * t712;
  int64_t t713;
  int64_t t714;
  uint8_t t715;
  float t716;
  int64_t t717;
  float * t718;
  float t719;
  int64_t t720;
  float * t721;
  int64_t t722;
  float * t723;
  float t724;
  float t725;
  float t726;
  int64_t t727;
  int64_t t728;
  int64_t t729;
  gg_type_12 * t730;
  gg_slice t731;
  int64_t t732;
  gg_type_11 * t733;
  gg_slice t734;
  int64_t t735;
  float * t736;
  int64_t t737;
  gg_type_12 * t738;
  gg_slice t739;
  int64_t t740;
  gg_type_11 * t741;
  gg_slice t742;
  int64_t t743;
  float * t744;
  float t745;
  float t746;
  int64_t t747;
  gg_type_12 * t748;
  gg_slice t749;
  int64_t t750;
  gg_type_11 * t751;
  gg_slice t752;
  int64_t t753;
  float * t754;
  int64_t t755;
  gg_type_12 * t756;
  gg_slice t757;
  int64_t t758;
  gg_type_11 * t759;
  gg_slice t760;
  int64_t t761;
  float * t762;
  float t763;
  float t764;
  int64_t t765;
  int64_t t766;
  int64_t t767;
  int64_t t768;
  int64_t t769;
  int64_t t770;
  int64_t t771;
  int64_t t772;
  uint8_t t773;
  uint8_t t774;
  int64_t t775;
  int64_t t776;
  uint8_t t777;
  int64_t t778;
  int64_t t779;
  int64_t t780;
  int64_t t781;
  int64_t t782;
  int64_t t783;
  int64_t t784;
  float t785;
  float t786;
  int64_t t787;
  int64_t t788;
  int64_t t789;
  int64_t t790;
  int64_t t791;
  int64_t t792;
  int64_t t793;
  float t794;
  int64_t t795;
  int64_t t796;
  int64_t t797;
  int64_t t798;
  uint8_t t799;
  uint8_t t800;
  int64_t t801;
  int64_t t802;
  uint8_t t803;
  int64_t t804;
  int64_t t805;
  int64_t t806;
  int64_t t807;
  int64_t t808;
  int64_t t809;
  int64_t t810;
  int64_t t811;
  float * t812;
  float t813;
  int64_t t814;
  int64_t t815;
  int64_t t816;
  int64_t t817;
  uint8_t t818;
  uint8_t t819;
  int64_t t820;
  int64_t t821;
  int64_t t822;
  int64_t t823;
  uint8_t t824;
  uint8_t t825;
  int64_t t826;
  int64_t t827;
  int64_t t828;
  int64_t t829;
  int64_t t830;
  int64_t t831;
  int64_t t832;
  float t833;
  float t834;
  int64_t t835;
  float * t836;
  float t837;
  float t838;
  int64_t t839;
  float * t840;
  int64_t t841;
  int64_t t842;
  int64_t t843;
  gg_type_12 * t844;
  gg_slice t845;
  int64_t t846;
  gg_type_11 * t847;
  gg_slice t848;
  int64_t t849;
  float * t850;
  int64_t t851;
  gg_type_12 * t852;
  gg_slice t853;
  int64_t t854;
  gg_type_11 * t855;
  gg_slice t856;
  int64_t t857;
  float * t858;
  float t859;
  float t860;
  int64_t t861;
  gg_type_12 * t862;
  gg_slice t863;
  int64_t t864;
  gg_type_11 * t865;
  gg_slice t866;
  int64_t t867;
  float * t868;
  int64_t t869;
  gg_type_12 * t870;
  gg_slice t871;
  int64_t t872;
  gg_type_11 * t873;
  gg_slice t874;
  int64_t t875;
  float * t876;
  float t877;
  float t878;
  int64_t t879;
  int64_t t880;
  int64_t t881;
  int64_t t882;
  int64_t t883;
  int64_t t884;
  int64_t t885;
  int64_t t886;
  int64_t t887;
  int64_t t888;
  int64_t t889;
  int64_t t890;
  int64_t t891;
  int64_t t892;
  int64_t t893;
  int64_t t894;
  uint8_t t895;
  int64_t t896;
  int64_t t897;
  int64_t t898;
  int64_t t899;
  int64_t t900;
  int64_t t901;
  int64_t t902;
  int64_t t903;
  int64_t t904;
  int64_t t905;
  int64_t t906;
  int64_t t907;
  int64_t t908;
  uint8_t t909;
  int64_t t910;
  int64_t t911;
  int64_t t912;
  int64_t t913;
  int64_t t914;
  int64_t t915;
  int64_t t916;
  int64_t t917;
  int64_t t918;
  int64_t t919;
  int64_t t920;
  gg_type_12 * t921;
  gg_slice t922;
  int64_t t923;
  int64_t t924;
  int64_t t925;
  gg_type_11 * t926;
  gg_slice t927;
  int64_t t928;
  float * t929;
  float t930;
  int64_t t931;
  int64_t t932;
  int64_t t933;
  int64_t t934;
  int64_t t935;
  int64_t t936;
  gg_type_12 * t937;
  gg_slice t938;
  int64_t t939;
  int64_t t940;
  int64_t t941;
  gg_type_11 * t942;
  gg_slice t943;
  int64_t t944;
  float * t945;
  float t946;
  int64_t t947;
  int64_t t948;
  int64_t t949;
  int64_t t950;
  int64_t t951;
  int64_t t952;
  gg_type_12 * t953;
  gg_slice t954;
  int64_t t955;
  int64_t t956;
  int64_t t957;
  int64_t t958;
  int64_t t959;
  gg_type_11 * t960;
  gg_slice t961;
  int64_t t962;
  float * t963;
  float t964;
  int64_t t965;
  int64_t t966;
  int64_t t967;
  int64_t t968;
  int64_t t969;
  int64_t t970;
  int64_t t971;
  int64_t t972;
  gg_type_12 * t973;
  gg_slice t974;
  int64_t t975;
  int64_t t976;
  int64_t t977;
  int64_t t978;
  int64_t t979;
  gg_type_11 * t980;
  gg_slice t981;
  int64_t t982;
  float * t983;
  float t984;
  int64_t t985;
  int64_t t986;
  int64_t t987;
  int64_t t988;
  int64_t t989;
  int64_t t990;
  int64_t t991;
  float * t992;
  int64_t t993;
  float * t994;
  float t995;
  float t996;
  int64_t t997;
  float * t998;
  int64_t t999;
  float * t1000;
  float t1001;
  int64_t t1002;
  int64_t t1003;
  int64_t t1004;
  uint8_t t1005;
  int64_t t1006;
  int64_t t1007;
  int64_t t1008;
  int64_t t1009;
  int64_t t1010;
  int64_t t1011;
  int64_t t1012;
  int64_t t1013;
  float * t1014;
  float t1015;
  int64_t t1016;
  int64_t t1017;
  uint8_t t1018;
  uint8_t t1019;
  int64_t t1020;
  int64_t t1021;
  uint8_t t1022;
  int64_t t1023;
  int64_t t1024;
  int64_t t1025;
  int64_t t1026;
  int64_t t1027;
  int64_t t1028;
  int64_t t1029;
  float t1030;
  float t1031;
  int64_t t1032;
  int64_t t1033;
  int64_t t1034;
  int64_t t1035;
  int64_t t1036;
  int64_t t1037;
  int64_t t1038;
  float t1039;
  int64_t t1040;
  int64_t t1041;
  int64_t t1042;
  int64_t t1043;
  int64_t t1044;
  int64_t t1045;
  int64_t t1046;
  int64_t t1047;
  int64_t t1048;
  int64_t t1049;
  int64_t t1050;
  int64_t t1051;
  int64_t t1052;
  float t1053;
  float t1054;
  int64_t t1055;
  int64_t t1056;
  int64_t t1057;
  int64_t t1058;
  int64_t t1059;
  int64_t t1060;
  uint8_t t1061;
  float t1062;
  float t1063;
  int64_t t1064;
  int64_t t1065;
  int64_t t1066;
  int64_t t1067;
  int64_t t1068;
  int64_t t1069;
  int64_t t1070;
  int64_t t1071;
  int64_t t1072;
  int64_t t1073;
  int64_t t1074;
  int64_t t1075;
  int64_t t1076;
  int64_t t1077;
  int64_t t1078;
  int64_t t1079;
  int64_t t1080;
  int64_t t1081;
  int64_t t1082;
  int64_t t1083;
  int64_t t1084;
  int64_t t1085;
  int64_t t1086;
  int64_t t1087;
  float t1088;
  int64_t t1089;
  int64_t t1090;
  int64_t t1091;
  int64_t t1092;
  int64_t t1093;
  int64_t t1094;
  uint8_t t1095;
  int64_t t1096;
  int64_t t1097;
  int64_t t1098;
  int64_t t1099;
  uint8_t t1100;
  uint8_t t1101;
  int64_t t1102;
  int64_t t1103;
  uint8_t t1104;
  uint8_t t1105;
  int64_t t1106;
  int64_t t1107;
  uint8_t t1108;
  uint8_t t1109;
  int64_t t1110;
  int64_t t1111;
  int64_t t1112;
  int64_t t1113;
  uint8_t t1114;
  int64_t t1115;
  int64_t t1116;
  int64_t t1117;
  int64_t t1118;
  int64_t t1119;
  int64_t t1120;
  int64_t t1121;
  int64_t t1122;
  int64_t t1123;
  int64_t t1124;
  int64_t t1125;
  int64_t t1126;
  int64_t t1127;
  int64_t t1128;
  int64_t t1129;
  int64_t t1130;
  int64_t t1131;
  int64_t t1132;
  int64_t t1133;
  float * t1134;
  float t1135;
  int64_t t1136;
  int64_t t1137;
  int64_t t1138;
  int64_t t1139;
  int64_t t1140;
  float * t1141;
  float t1142;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  int64_t v7;
  float v8;
  float v9;
  float v10;
  float v11;
  int64_t v12;
  float v13;
  int64_t v14;
  int64_t v15;
  int64_t v16;
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
  int64_t v39;
  int64_t v40;
  int64_t v41;
  int64_t v42;
  int64_t v43;
  int64_t v44;
  int64_t v45;
  int64_t v46;
  int64_t v47;
  int64_t v48;
  int64_t v49;
  int64_t v50;
  int64_t v51;
  int64_t v52;
  int64_t v53;
  int64_t v54;
  int64_t v55;
  int64_t v56;
  int64_t v57;
  int64_t v58;
  int64_t v59;
  float v60;
  int64_t v61;
  int64_t v62;
  int64_t v63;
  int64_t v64;
  int64_t v65;
  int64_t v66;
  int64_t v67;
  int64_t v68;
  int64_t v69;
  int64_t v70;
  int64_t v71;
  int64_t v72;
  gg_type_8 v73;
  gg_type_13 v74;
  gg_type_11 v75;
  gg_type_11 v76;
  gg_type_11 v77;
  gg_type_11 v78;
  gg_type_11 v79;
  int64_t v80;
  int64_t v81;
  int64_t v82;
  int64_t v83;
  uint8_t v84;
  int64_t v85;
  int64_t v86;
  int64_t v87;
  int64_t v88;
  int64_t v89;
  int64_t v90;
  int64_t v91;
  int64_t v92;
  int64_t v93;
  int64_t v94;
  int64_t v95;
  int64_t v96;
  uint8_t v97;
  float v98;
  float v99;
  int64_t v100;
  int64_t v101;
  int64_t v102;
  float v103;
  int64_t v104;
  int64_t v105;
  int64_t v106;
  int64_t v107;
  int64_t v108;
  int64_t v109;
  gg_type_15 v110;
  int64_t v111;
  int64_t v112;
  int64_t v113;
  float v114;
  int64_t v115;
  int64_t v116;
  float v117;
  float v118;
  float v119;
  int64_t v120;
  int64_t v121;
  int64_t v122;
  int64_t v123;
  uint8_t v124;
  float v125;
  float v126;
  int64_t v127;
  int64_t v128;
  int64_t v129;
  int64_t v130;
  float v131;
  int64_t v132;
  float v133;
  int64_t v134;
  int64_t v135;
  int64_t v136;
  int64_t v137;
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
  v23 = p23;
  v24 = p24;
  v25 = p25;
  v26 = p26;
  v27 = p27;
  v28 = p28;
  v29 = p29;
  v30 = p30;
  v31 = p31;
  v32 = p32;
  v33 = p33;
  v34 = p34;
  v35 = p35;
  v36 = p36;
  v37 = p37;
  v38 = p38;
  v39 = p39;
  v40 = p40;
  v41 = p41;
  {
  v42 = ((int64_t)(v14));
  t1 = ((int64_t)(((int64_t)UINT64_C(1))));
  t2 = dfa_tid(t1);
  v43 = ((int64_t)(t2));
  t3 = ((int64_t)(((int64_t)UINT64_C(0))));
  t4 = dfa_tid(t3);
  v44 = ((int64_t)(t4));
  t5 = ((int64_t)(v43));
  t6 = ((int64_t)(((int64_t)UINT64_C(32))));
  t7 = ((int64_t)(((int64_t)((uint64_t)(t5) * (uint64_t)(t6)))));
  t8 = ((int64_t)(v44));
  v45 = ((int64_t)(((int64_t)((uint64_t)(t7) + (uint64_t)(t8)))));
  t9 = ((int64_t)(((int64_t)UINT64_C(0))));
  t10 = dfa_bid(t9);
  v46 = ((int64_t)(t10));
  t11 = ((int64_t)(((int64_t)UINT64_C(1))));
  t12 = dfa_bid(t11);
  v47 = ((int64_t)(t12));
  t13 = ((int64_t)(((int64_t)UINT64_C(2))));
  t14 = dfa_bid(t13);
  v48 = ((int64_t)(t14));
  t15 = ((int64_t)(((int64_t)UINT64_C(1))));
  t16 = dfa_gdim(t15);
  v49 = ((int64_t)(t16));
  t17 = ((int64_t)(((int64_t)UINT64_C(0))));
  t18 = dfa_gdim(t17);
  v50 = ((int64_t)(t18));
  t19 = ((int64_t)(v46));
  t20 = ((int64_t)(v37));
  v51 = ((int64_t)(((int64_t)((uint64_t)(t19) * (uint64_t)(t20)))));
  t21 = ((int64_t)(v48));
  t22 = ((int64_t)(v16));
  v52 = ((int64_t)(((int64_t)gg_div(t21, t22))));
  t23 = ((int64_t)(v48));
  t24 = ((int64_t)(v52));
  t25 = ((int64_t)(v16));
  t26 = ((int64_t)(((int64_t)((uint64_t)(t24) * (uint64_t)(t25)))));
  v53 = ((int64_t)(((int64_t)((uint64_t)(t23) - (uint64_t)(t26)))));
  t27 = ((int64_t)(v16));
  t28 = ((int64_t)(v23));
  v54 = ((int64_t)(((int64_t)gg_div(t27, t28))));
  t29 = ((int64_t)(v53));
  t30 = ((int64_t)(v54));
  v55 = ((int64_t)(((int64_t)gg_div(t29, t30))));
  t31 = ((int64_t)(v0));
  t32 = ((int64_t)(v20));
  t33 = ((int64_t)(v52));
  t34 = ((int64_t)(((int64_t)((uint64_t)(t32) * (uint64_t)(t33)))));
  t35 = ((int64_t)(((int64_t)((uint64_t)(t31) + (uint64_t)(t34)))));
  t36 = ((int64_t)(v19));
  t37 = ((int64_t)(v53));
  t38 = ((int64_t)(((int64_t)((uint64_t)(t36) * (uint64_t)(t37)))));
  t39 = ((int64_t)(((int64_t)((uint64_t)(t35) + (uint64_t)(t38)))));
  t40 = ((int64_t)(v18));
  t41 = ((int64_t)(v51));
  t42 = ((int64_t)(((int64_t)((uint64_t)(t40) * (uint64_t)(t41)))));
  v56 = ((int64_t)(((int64_t)((uint64_t)(t39) + (uint64_t)(t42)))));
  t43 = ((int64_t)(v1));
  t44 = ((int64_t)(v27));
  t45 = ((int64_t)(v52));
  t46 = ((int64_t)(((int64_t)((uint64_t)(t44) * (uint64_t)(t45)))));
  t47 = ((int64_t)(((int64_t)((uint64_t)(t43) + (uint64_t)(t46)))));
  t48 = ((int64_t)(v26));
  t49 = ((int64_t)(v55));
  t50 = ((int64_t)(((int64_t)((uint64_t)(t48) * (uint64_t)(t49)))));
  v57 = ((int64_t)(((int64_t)((uint64_t)(t47) + (uint64_t)(t50)))));
  t51 = ((int64_t)(v2));
  t52 = ((int64_t)(v30));
  t53 = ((int64_t)(v52));
  t54 = ((int64_t)(((int64_t)((uint64_t)(t52) * (uint64_t)(t53)))));
  t55 = ((int64_t)(((int64_t)((uint64_t)(t51) + (uint64_t)(t54)))));
  t56 = ((int64_t)(v29));
  t57 = ((int64_t)(v55));
  t58 = ((int64_t)(((int64_t)((uint64_t)(t56) * (uint64_t)(t57)))));
  v58 = ((int64_t)(((int64_t)((uint64_t)(t55) + (uint64_t)(t58)))));
  v59 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  t59 = ((int64_t)(v38));
  t60 = ((int64_t)(((int64_t)UINT64_C(0))));
  t61 = ((t59) != (t60));
  if (t61) {
  {
  t62 = ((int64_t)(v3));
  t63 = ((int64_t)(v36));
  t64 = ((int64_t)(v52));
  t65 = ((int64_t)(v33));
  t66 = ((int64_t)(((int64_t)gg_mod(t64, t65))));
  t67 = ((int64_t)(((int64_t)((uint64_t)(t63) * (uint64_t)(t66)))));
  t68 = ((int64_t)(((int64_t)((uint64_t)(t62) + (uint64_t)(t67)))));
  t69 = ((int64_t)(v34));
  t70 = ((int64_t)(v51));
  t71 = ((int64_t)(((int64_t)((uint64_t)(t69) * (uint64_t)(t70)))));
  v59 = ((int64_t)(((int64_t)((uint64_t)(t68) + (uint64_t)(t71)))));
  }
  }
  t72 = ((float)(v9));
  t73 = ((int64_t)(v53));
  t74 = ((int64_t)(v12));
  t75 = ((float)(v10));
  t76 = ((float)(v11));
  v60 = gg_function_10(t72, t73, t74, t75, t76, base + 0);
  v61 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  v62 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(32))))));
  t77 = ((int64_t)(v42));
  t78 = ((int64_t)(((int64_t)UINT64_C(4))));
  t79 = ((int64_t)(((int64_t)gg_div(t77, t78))));
  t80 = ((int64_t)(((int64_t)UINT64_C(32))));
  t81 = ((t79) < (t80));
  if (t81) {
  {
  t82 = ((int64_t)(v42));
  t83 = ((int64_t)(((int64_t)UINT64_C(4))));
  v63 = ((int64_t)(((int64_t)gg_div(t82, t83))));
  }
  } else {
  {
  v63 = ((int64_t)(((int64_t)UINT64_C(32))));
  }
  }
  v64 = ((int64_t)(v63));
  v65 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(4))))));
  t84 = ((int64_t)(v62));
  t85 = ((int64_t)(v64));
  v66 = ((int64_t)(((int64_t)gg_div(t84, t85))));
  t86 = ((int64_t)(v37));
  t87 = ((int64_t)(v42));
  v67 = ((int64_t)(((int64_t)((uint64_t)(t86) * (uint64_t)(t87)))));
  t88 = ((int64_t)(v61));
  t89 = ((int64_t)(v66));
  t90 = ((int64_t)(((int64_t)((uint64_t)(t88) * (uint64_t)(t89)))));
  t91 = ((int64_t)(v42));
  v68 = ((int64_t)(((int64_t)((uint64_t)(t90) * (uint64_t)(t91)))));
  t92 = ((int64_t)(v67));
  t93 = ((int64_t)(v68));
  t94 = ((t92) > (t93));
  if (t94) {
  {
  v69 = ((int64_t)(v67));
  }
  } else {
  {
  v69 = ((int64_t)(v68));
  }
  }
  v70 = ((int64_t)(v69));
  t95 = ((int64_t)(v69));
  t96 = ((int64_t)(((int64_t)UINT64_C(64))));
  v71 = ((int64_t)(((int64_t)((uint64_t)(t95) + (uint64_t)(t96)))));
  t97 = ((int64_t)(v42));
  t98 = ((int64_t)(((int64_t)UINT64_C(2))));
  t99 = ((int64_t)(v64));
  t100 = ((int64_t)(((int64_t)((uint64_t)(t98) * (uint64_t)(t99)))));
  v72 = ((int64_t)(((int64_t)gg_div(t97, t100))));
  memset(&(v73), 0, sizeof(v73));
  memset(&t102, 0, sizeof(t102));
  t103 = ((uint32_t)(t102));
  for (t104=0; t104<2; t104++) (t101).data[t104] = t103;
  t105 = t101;
  for (t106=0; t106<2; t106++) ((v73).f0).data[t106] = t105;
  memset(&t108, 0, sizeof(t108));
  t109 = ((float)(t108));
  for (t110=0; t110<2; t110++) (t107).data[t110] = t109;
  t111 = t107;
  for (t112=0; t112<4; t112++) ((v73).f1).data[t112] = t111;
  memset(&(v74), 0, sizeof(v74));
  memset(&t115, 0, sizeof(t115));
  t116 = ((float)(t115));
  for (t117=0; t117<2; t117++) (t114).data[t117] = t116;
  t118 = t114;
  for (t119=0; t119<4; t119++) (t113).data[t119] = t118;
  t120 = t113;
  for (t121=0; t121<2; t121++) ((v74).f2).data[t121] = t120;
  memset(&t122, 0, sizeof(t122));
  t123 = ((float)(t122));
  for (t124=0; t124<2; t124++) (v75).data[t124] = t123;
  memset(&t125, 0, sizeof(t125));
  t126 = ((float)(t125));
  for (t127=0; t127<2; t127++) (v76).data[t127] = t126;
  memset(&t128, 0, sizeof(t128));
  t129 = ((float)(t128));
  for (t130=0; t130<2; t130++) (v77).data[t130] = t129;
  memset(&t131, 0, sizeof(t131));
  t132 = ((float)(t131));
  for (t133=0; t133<2; t133++) (v78).data[t133] = t132;
  memset(&t134, 0, sizeof(t134));
  t135 = ((float)(t134));
  for (t136=0; t136<2; t136++) (v79).data[t136] = t135;
  t138 = ((int64_t)UINT64_C(2));
  t137 = 0;
label0:;
  if (t137 >= t138) goto label2;
  v80 = t137;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t139 = v80;
  t140 = ((float *)(v76).data) + gg_index(t139, ((int64_t)UINT64_C(2)));
  t141 = dfa_neg_half_flt_max();
  (*t140) = ((float)(t141));
  /* FG_DIRECT_SLICE_INDEX */
  t142 = v80;
  t143 = ((float *)(v77).data) + gg_index(t142, ((int64_t)UINT64_C(2)));
  (*t143) = ((float)(gg_double_bits(UINT64_C(0))));
  }
label1:;
  t137++; goto label0;
label2:;
  t144 = ((int64_t)(v42));
  t145 = ((int64_t)(((int64_t)UINT64_C(4))));
  t146 = ((int64_t)(((int64_t)gg_div(t144, t145))));
  t147 = ((int64_t)(((int64_t)UINT64_C(32))));
  t148 = ((t146) < (t147));
  if (t148) {
  {
  t149 = ((int64_t)(v42));
  t150 = ((int64_t)(((int64_t)UINT64_C(4))));
  v81 = ((int64_t)(((int64_t)gg_div(t149, t150))));
  }
  } else {
  {
  v81 = ((int64_t)(((int64_t)UINT64_C(32))));
  }
  }
  v82 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label4:;
  t151 = ((int64_t)(v82));
  t152 = ((int64_t)(v37));
  if (!(((t151) < (t152)))) goto label5;
  {
  t153 = ((int64_t)(v82));
  t154 = ((int64_t)(v43));
  v83 = ((int64_t)(((int64_t)((uint64_t)(t153) + (uint64_t)(t154)))));
  t155 = ((int64_t)(v82));
  t156 = ((int64_t)(v61));
  t157 = ((int64_t)(((int64_t)((uint64_t)(t155) + (uint64_t)(t156)))));
  t158 = ((int64_t)(v37));
  t159 = ((t157) > (t158));
  t160 = t159;
  if (t160) {
  t161 = ((int64_t)(v83));
  t162 = ((int64_t)(v37));
  t160 = ((t161) >= (t162));
  }
  t163 = t160;
  t164 = (!(t163));
  if (t164) {
  {
  t165 = ((int64_t)(v37));
  t166 = ((int64_t)(((int64_t)UINT64_C(1))));
  t167 = ((t165) > (t166));
  t168 = t167;
  if (t168) {
  t169 = ((int64_t)(v51));
  t170 = ((int64_t)(v83));
  t171 = ((int64_t)(((int64_t)((uint64_t)(t169) + (uint64_t)(t170)))));
  t172 = ((int64_t)(v15));
  t168 = ((t171) >= (t172));
  }
  t173 = t168;
  v84 = (!(t173));
  t174 = v84;
  t175 = (!(t174));
  if (t175) {
  {
  t176 = ((int64_t)(v83));
  t177 = ((int64_t)(v42));
  t178 = ((int64_t)(v44));
  gg_function_13(t176, t177, t178, base + 0);
  }
  } else {
  {
  v85 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label7:;
  t179 = ((int64_t)(v85));
  t180 = ((int64_t)(v42));
  t181 = ((int64_t)(((int64_t)UINT64_C(4))));
  t182 = ((int64_t)(((int64_t)gg_div(t180, t181))));
  if (!(((t179) < (t182)))) goto label8;
  {
  t183 = ((int64_t)(v56));
  t184 = ((int64_t)(v83));
  t185 = ((int64_t)(v18));
  t186 = ((int64_t)(((int64_t)((uint64_t)(t184) * (uint64_t)(t185)))));
  t187 = ((int64_t)(((int64_t)((uint64_t)(t183) + (uint64_t)(t186)))));
  t188 = ((int64_t)(v85));
  t189 = gg_function_16(t188, base + 0);
  t190 = ((int64_t)(t189));
  t191 = ((int64_t)(((int64_t)((uint64_t)(t187) + (uint64_t)(t190)))));
  t192 = ((float)(v8));
  t193 = ((int64_t)(v81));
  t194 = ((int64_t)(v44));
  t195 = ((int64_t)(v83));
  t196 = ((int64_t)(v42));
  t197 = ((int64_t)(((int64_t)((uint64_t)(t195) * (uint64_t)(t196)))));
  t198 = ((int64_t)(v83));
  t199 = ((int64_t)(v42));
  t200 = ((int64_t)(((int64_t)((uint64_t)(t198) * (uint64_t)(t199)))));
  t201 = ((int64_t)(v42));
  t202 = ((int64_t)(((int64_t)((uint64_t)(t200) + (uint64_t)(t201)))));
  t203 = ((int64_t)(v85));
  gg_function_17(t191, t192, t193, t194, t197, t202, t203, base + 0);
  t204 = v85;
  t205 = ((int64_t)(v81));
  v85 = ((int64_t)((uint64_t)(t204) + (uint64_t)(t205)));
  }
  goto label7;
label8:;
  }
  }
  }
  }
  t206 = v82;
  t207 = ((int64_t)(v61));
  v82 = ((int64_t)((uint64_t)(t206) + (uint64_t)(t207)));
  }
  goto label4;
label5:;
  dfa_sync();
  v86 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label10:;
  t208 = ((int64_t)(v86));
  t209 = ((int64_t)(v37));
  if (!(((t208) < (t209)))) goto label11;
  {
  v87 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label13:;
  t210 = ((int64_t)(v87));
  t211 = ((int64_t)(v42));
  t212 = ((int64_t)(((int64_t)UINT64_C(4))));
  t213 = ((int64_t)(((int64_t)gg_div(t211, t212))));
  if (!(((t210) < (t213)))) goto label14;
  {
  t214 = ((int64_t)(v87));
  t216 = ((int64_t)(v63));
  t217 = ((int64_t)(((int64_t)UINT64_C(32))));
  t218 = ((t216) == (t217));
  if (t218) {
  {
  t215 = ((int64_t)(v44));
  }
  } else {
  {
  t219 = ((int64_t)(v44));
  t220 = ((int64_t)(v63));
  t215 = ((int64_t)(((int64_t)gg_mod(t219, t220))));
  }
  }
  t221 = ((int64_t)(t215));
  v88 = ((int64_t)(((int64_t)((uint64_t)(t214) + (uint64_t)(t221)))));
  t222 = ((int64_t)(v87));
  t223 = ((int64_t)(v63));
  v89 = ((int64_t)(((int64_t)gg_div(t222, t223))));
  /* FG_DIRECT_SLICE_INDEX */
  t224 = v86;
  t225 = ((gg_type_9 *)((v73).f0).data) + gg_index(t224, ((int64_t)UINT64_C(2)));
  t226 = gg_view(((*t225)).data, 2);
  t227 = v89;
  t228 = ((uint32_t *)t226.data) + gg_index(t227, t226.len);
  t229 = ((int64_t)(v86));
  t230 = ((int64_t)(v42));
  t231 = ((int64_t)(((int64_t)((uint64_t)(t229) * (uint64_t)(t230)))));
  t232 = ((int64_t)(v88));
  t233 = ((int64_t)(((int64_t)((uint64_t)(t231) + (uint64_t)(t232)))));
  t234 = dfa_sh_u32(t233);
  (*t228) = ((uint32_t)(t234));
  t235 = ((int64_t)(v88));
  t236 = ((int64_t)(((int64_t)UINT64_C(8))));
  v90 = ((int64_t)(((int64_t)gg_div(t235, t236))));
  /* FG_DIRECT_SLICE_INDEX */
  t237 = v86;
  t238 = ((gg_type_11 *)((v73).f1).data) + gg_index(t237, ((int64_t)UINT64_C(4)));
  t239 = gg_view(((*t238)).data, 2);
  t240 = ((int64_t)(((int64_t)UINT64_C(2))));
  t241 = ((int64_t)(v89));
  t242 = ((int64_t)((uint64_t)(t240) * (uint64_t)(t241)));
  t243 = ((float *)t239.data) + gg_index(t242, t239.len);
  t244 = ((int64_t)(v86));
  t245 = ((int64_t)(v42));
  t246 = ((int64_t)(((int64_t)((uint64_t)(t244) * (uint64_t)(t245)))));
  t247 = ((int64_t)(v42));
  t248 = ((int64_t)(((int64_t)((uint64_t)(t246) + (uint64_t)(t247)))));
  t249 = ((int64_t)(((int64_t)UINT64_C(2))));
  t250 = ((int64_t)(v90));
  t251 = ((int64_t)(((int64_t)((uint64_t)(t249) * (uint64_t)(t250)))));
  t252 = ((int64_t)(((int64_t)((uint64_t)(t248) + (uint64_t)(t251)))));
  t253 = dfa_sh_f32(t252);
  (*t243) = ((float)(t253));
  /* FG_DIRECT_SLICE_INDEX */
  t254 = v86;
  t255 = ((gg_type_11 *)((v73).f1).data) + gg_index(t254, ((int64_t)UINT64_C(4)));
  t256 = gg_view(((*t255)).data, 2);
  t257 = ((int64_t)(((int64_t)UINT64_C(2))));
  t258 = ((int64_t)(v89));
  t259 = ((int64_t)(((int64_t)((uint64_t)(t257) * (uint64_t)(t258)))));
  t260 = ((int64_t)(((int64_t)UINT64_C(1))));
  t261 = ((int64_t)((uint64_t)(t259) + (uint64_t)(t260)));
  t262 = ((float *)t256.data) + gg_index(t261, t256.len);
  t263 = ((int64_t)(v86));
  t264 = ((int64_t)(v42));
  t265 = ((int64_t)(((int64_t)((uint64_t)(t263) * (uint64_t)(t264)))));
  t266 = ((int64_t)(v42));
  t267 = ((int64_t)(((int64_t)((uint64_t)(t265) + (uint64_t)(t266)))));
  t268 = ((int64_t)(((int64_t)UINT64_C(2))));
  t269 = ((int64_t)(v90));
  t270 = ((int64_t)(((int64_t)((uint64_t)(t268) * (uint64_t)(t269)))));
  t271 = ((int64_t)(((int64_t)((uint64_t)(t267) + (uint64_t)(t270)))));
  t272 = ((int64_t)(((int64_t)UINT64_C(1))));
  t273 = ((int64_t)(((int64_t)((uint64_t)(t271) + (uint64_t)(t272)))));
  t274 = dfa_sh_f32(t273);
  (*t262) = ((float)(t274));
  t275 = v87;
  t276 = ((int64_t)(v63));
  v87 = ((int64_t)((uint64_t)(t275) + (uint64_t)(t276)));
  }
  goto label13;
label14:;
  t277 = v86;
  t278 = ((int64_t)(((int64_t)UINT64_C(1))));
  v86 = ((int64_t)((uint64_t)(t277) + (uint64_t)(t278)));
  }
  goto label10;
label11:;
  dfa_sync();
  t279 = ((int64_t)(v40));
  t280 = ((int64_t)(((int64_t)UINT64_C(0))));
  t281 = ((t279) != (t280));
  if (t281) {
  {
  t282 = ((int64_t)(v5));
  t283 = ((int64_t)(((int64_t)UINT64_C(4))));
  t284 = ((int64_t)(v52));
  t285 = ((int64_t)(v50));
  t286 = ((int64_t)(((int64_t)((uint64_t)(t284) * (uint64_t)(t285)))));
  t287 = ((int64_t)(v46));
  t288 = ((int64_t)(((int64_t)((uint64_t)(t286) + (uint64_t)(t287)))));
  t289 = ((int64_t)(((int64_t)((uint64_t)(t283) * (uint64_t)(t288)))));
  t290 = dfa_ld_u32(t282, t289);
  v91 = ((int64_t)(((int64_t)(t290))));
  }
  } else {
  {
  v91 = ((int64_t)(v22));
  }
  }
  t291 = v57;
  t292 = ((int64_t)(v47));
  t293 = ((int64_t)(((int64_t)UINT64_C(128))));
  t294 = ((int64_t)(((int64_t)((uint64_t)(t292) * (uint64_t)(t293)))));
  t295 = ((int64_t)(v25));
  t296 = ((int64_t)(((int64_t)((uint64_t)(t294) * (uint64_t)(t295)))));
  v57 = ((int64_t)((uint64_t)(t291) + (uint64_t)(t296)));
  t297 = v58;
  t298 = ((int64_t)(v47));
  t299 = ((int64_t)(((int64_t)UINT64_C(128))));
  t300 = ((int64_t)(((int64_t)((uint64_t)(t298) * (uint64_t)(t299)))));
  t301 = ((int64_t)(v28));
  t302 = ((int64_t)(((int64_t)((uint64_t)(t300) * (uint64_t)(t301)))));
  v58 = ((int64_t)((uint64_t)(t297) + (uint64_t)(t302)));
  t303 = v59;
  t304 = ((int64_t)(((int64_t)UINT64_C(2))));
  t305 = ((int64_t)(v47));
  t306 = ((int64_t)(((int64_t)UINT64_C(128))));
  t307 = ((int64_t)(((int64_t)((uint64_t)(t305) * (uint64_t)(t306)))));
  t308 = ((int64_t)(((int64_t)((uint64_t)(t304) * (uint64_t)(t307)))));
  v59 = ((int64_t)((uint64_t)(t303) + (uint64_t)(t308)));
  t309 = ((int64_t)(v47));
  t310 = ((int64_t)(((int64_t)UINT64_C(128))));
  v92 = ((int64_t)(((int64_t)((uint64_t)(t309) * (uint64_t)(t310)))));
label16:;
  t311 = ((int64_t)(v92));
  t312 = ((int64_t)(v91));
  if (!(((t311) < (t312)))) goto label17;
  {
  t314 = ((int64_t)UINT64_C(2));
  t313 = 0;
label18:;
  if (t313 >= t314) goto label20;
  v93 = t313;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t315 = v93;
  t316 = ((float *)(v79).data) + gg_index(t315, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t317 = v93;
  t318 = ((float *)(v76).data) + gg_index(t317, ((int64_t)UINT64_C(2)));
  (*t316) = ((float)((*t318)));
  }
label19:;
  t313++; goto label18;
label20:;
  v94 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label22:;
  t319 = ((int64_t)(v94));
  t320 = ((int64_t)(v63));
  if (!(((t319) < (t320)))) goto label23;
  {
  t321 = ((int64_t)(v43));
  t322 = ((int64_t)(((int64_t)UINT64_C(32))));
  t323 = ((int64_t)(((int64_t)((uint64_t)(t321) * (uint64_t)(t322)))));
  t325 = ((int64_t)(v63));
  t326 = ((int64_t)(((int64_t)UINT64_C(32))));
  t327 = ((t325) == (t326));
  if (t327) {
  {
  t324 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  }
  } else {
  {
  t328 = ((int64_t)(v44));
  t329 = ((int64_t)(v44));
  t330 = ((int64_t)(v63));
  t331 = ((int64_t)(((int64_t)gg_mod(t329, t330))));
  t324 = ((int64_t)(((int64_t)((uint64_t)(t328) - (uint64_t)(t331)))));
  }
  }
  t332 = ((int64_t)(t324));
  t333 = ((int64_t)(((int64_t)((uint64_t)(t323) + (uint64_t)(t332)))));
  t334 = ((int64_t)(v94));
  v95 = ((int64_t)(((int64_t)((uint64_t)(t333) + (uint64_t)(t334)))));
  v96 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label25:;
  t335 = ((int64_t)(v96));
  t336 = ((int64_t)(v37));
  if (!(((t335) < (t336)))) goto label26;
  {
  t337 = ((int64_t)(v37));
  t338 = ((int64_t)(((int64_t)UINT64_C(1))));
  t339 = ((t337) > (t338));
  t340 = t339;
  if (t340) {
  t341 = ((int64_t)(v51));
  t342 = ((int64_t)(v96));
  t343 = ((int64_t)(((int64_t)((uint64_t)(t341) + (uint64_t)(t342)))));
  t344 = ((int64_t)(v15));
  t340 = ((t343) >= (t344));
  }
  t345 = t340;
  v97 = (!(t345));
  t346 = ((int64_t)(v57));
  t347 = ((int64_t)(v95));
  t348 = ((int64_t)(v25));
  t349 = ((int64_t)(((int64_t)((uint64_t)(t347) * (uint64_t)(t348)))));
  t350 = ((int64_t)(((int64_t)((uint64_t)(t346) + (uint64_t)(t349)))));
  /* FG_DIRECT_SLICE_INDEX */
  t351 = v96;
  t352 = ((gg_type_9 *)((v73).f0).data) + gg_index(t351, ((int64_t)UINT64_C(2)));
  t353 = gg_view(((*t352)).data, 2);
  t354 = ((int64_t)UINT64_C(0));
  t355 = ((uint32_t *)t353.data) + gg_index(t354, t353.len);
  t356 = ((uint32_t)((*t355)));
  /* FG_DIRECT_SLICE_INDEX */
  t357 = v96;
  t358 = ((gg_type_9 *)((v73).f0).data) + gg_index(t357, ((int64_t)UINT64_C(2)));
  t359 = gg_view(((*t358)).data, 2);
  t360 = ((int64_t)UINT64_C(1));
  t361 = ((uint32_t *)t359.data) + gg_index(t360, t359.len);
  t362 = ((uint32_t)((*t361)));
  /* FG_DIRECT_SLICE_INDEX */
  t363 = v96;
  t364 = ((gg_type_11 *)((v73).f1).data) + gg_index(t363, ((int64_t)UINT64_C(4)));
  t365 = gg_view(((*t364)).data, 2);
  t366 = ((int64_t)UINT64_C(0));
  t367 = ((float *)t365.data) + gg_index(t366, t365.len);
  t368 = ((float)((*t367)));
  /* FG_DIRECT_SLICE_INDEX */
  t369 = v96;
  t370 = ((gg_type_11 *)((v73).f1).data) + gg_index(t369, ((int64_t)UINT64_C(4)));
  t371 = gg_view(((*t370)).data, 2);
  t372 = ((int64_t)UINT64_C(1));
  t373 = ((float *)t371.data) + gg_index(t372, t371.len);
  t374 = ((float)((*t373)));
  /* FG_DIRECT_SLICE_INDEX */
  t375 = v96;
  t376 = ((gg_type_11 *)((v73).f1).data) + gg_index(t375, ((int64_t)UINT64_C(4)));
  t377 = gg_view(((*t376)).data, 2);
  t378 = ((int64_t)UINT64_C(2));
  t379 = ((float *)t377.data) + gg_index(t378, t377.len);
  t380 = ((float)((*t379)));
  /* FG_DIRECT_SLICE_INDEX */
  t381 = v96;
  t382 = ((gg_type_11 *)((v73).f1).data) + gg_index(t381, ((int64_t)UINT64_C(4)));
  t383 = gg_view(((*t382)).data, 2);
  t384 = ((int64_t)UINT64_C(3));
  t385 = ((float *)t383.data) + gg_index(t384, t383.len);
  t386 = ((float)((*t385)));
  t387 = ((int64_t)(v42));
  t388 = ((int64_t)(v63));
  t389 = ((int64_t)(v44));
  v98 = gg_function_31(t350, t356, t362, t368, t374, t380, t386, t387, t388, t389, base + 0);
  t390 = ((float)(v98));
  t391 = ((int64_t)(v63));
  v99 = gg_function_33(t390, t391, base + 0);
  t392 = ((int64_t)(v41));
  t393 = ((int64_t)(((int64_t)UINT64_C(0))));
  t394 = ((t392) != (t393));
  if (t394) {
  {
  t395 = ((float)(v99));
  t396 = ((float)(v13));
  v99 = gg_function_34(t395, t396, base + 0);
  }
  }
  t397 = ((int64_t)(v38));
  t398 = ((int64_t)(((int64_t)UINT64_C(0))));
  t399 = ((t397) != (t398));
  t400 = t399;
  if (t400) {
  t400 = v97;
  }
  t401 = t400;
  if (t401) {
  {
  t402 = ((float)(v99));
  t403 = ((float)(v60));
  t404 = ((int64_t)(v59));
  t405 = ((int64_t)(((int64_t)UINT64_C(2))));
  t406 = ((int64_t)(v96));
  t407 = ((int64_t)(v22));
  t408 = ((int64_t)(((int64_t)((uint64_t)(t406) * (uint64_t)(t407)))));
  t409 = ((int64_t)(v95));
  t410 = ((int64_t)(((int64_t)((uint64_t)(t408) + (uint64_t)(t409)))));
  t411 = ((int64_t)(((int64_t)((uint64_t)(t405) * (uint64_t)(t410)))));
  t412 = dfa_ld_u16(t404, t411);
  t413 = ((uint32_t)(t412));
  v99 = gg_function_36(t402, t403, t413, base + 0);
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t414 = v96;
  t415 = ((float *)(v79).data) + gg_index(t414, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t416 = v96;
  t417 = ((float *)(v79).data) + gg_index(t416, ((int64_t)UINT64_C(2)));
  t418 = ((float)((*t417)));
  t419 = ((float)(v99));
  (*t415) = gg_function_37(t418, t419, base + 0);
  t420 = ((int64_t)(v63));
  t421 = ((int64_t)(((int64_t)UINT64_C(32))));
  t422 = ((t420) == (t421));
  if (t422) {
  {
  v100 = ((int64_t)(v44));
  }
  } else {
  {
  t423 = ((int64_t)(v44));
  t424 = ((int64_t)(v63));
  v100 = ((int64_t)(((int64_t)gg_mod(t423, t424))));
  }
  }
  t425 = ((int64_t)(v100));
  t426 = ((int64_t)(v94));
  t427 = ((t425) == (t426));
  if (t427) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t428 = v96;
  t429 = ((float *)(v75).data) + gg_index(t428, ((int64_t)UINT64_C(2)));
  (*t429) = ((float)(v99));
  }
  }
  t430 = v96;
  t431 = ((int64_t)(((int64_t)UINT64_C(1))));
  v96 = ((int64_t)((uint64_t)(t430) + (uint64_t)(t431)));
  }
  goto label25;
label26:;
  t432 = v94;
  t433 = ((int64_t)(((int64_t)UINT64_C(1))));
  v94 = ((int64_t)((uint64_t)(t432) + (uint64_t)(t433)));
  }
  goto label22;
label23:;
  v101 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label28:;
  t434 = ((int64_t)(v101));
  t435 = ((int64_t)(v37));
  if (!(((t434) < (t435)))) goto label29;
  {
  v102 = ((int64_t)(v63));
label31:;
  t436 = ((int64_t)(v102));
  t437 = ((int64_t)(((int64_t)UINT64_C(32))));
  if (!(((t436) < (t437)))) goto label32;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t438 = v101;
  t439 = ((float *)(v79).data) + gg_index(t438, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t440 = v101;
  t441 = ((float *)(v79).data) + gg_index(t440, ((int64_t)UINT64_C(2)));
  t442 = ((float)((*t441)));
  /* FG_DIRECT_SLICE_INDEX */
  t443 = v101;
  t444 = ((float *)(v79).data) + gg_index(t443, ((int64_t)UINT64_C(2)));
  t445 = ((float)((*t444)));
  t446 = ((int64_t)(v102));
  t447 = dfa_shfl_xor_f32(t445, t446);
  t448 = ((float)(t447));
  (*t439) = gg_function_39(t442, t448, base + 0);
  t449 = ((int64_t)(v102));
  t450 = ((int64_t)(((int64_t)UINT64_C(2))));
  v102 = ((int64_t)(((int64_t)((uint64_t)(t449) * (uint64_t)(t450)))));
  }
  goto label31;
label32:;
  /* FG_DIRECT_SLICE_INDEX */
  t451 = v101;
  t452 = ((float *)(v76).data) + gg_index(t451, ((int64_t)UINT64_C(2)));
  t453 = ((float)((*t452)));
  /* FG_DIRECT_SLICE_INDEX */
  t454 = v101;
  t455 = ((float *)(v79).data) + gg_index(t454, ((int64_t)UINT64_C(2)));
  t456 = ((float)((*t455)));
  v103 = gg_function_40(t453, t456, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t457 = v101;
  t458 = ((float *)(v76).data) + gg_index(t457, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t459 = v101;
  t460 = ((float *)(v79).data) + gg_index(t459, ((int64_t)UINT64_C(2)));
  (*t458) = ((float)((*t460)));
  /* FG_DIRECT_SLICE_INDEX */
  t461 = v101;
  t462 = ((float *)(v75).data) + gg_index(t461, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t463 = v101;
  t464 = ((float *)(v75).data) + gg_index(t463, ((int64_t)UINT64_C(2)));
  t465 = ((float)((*t464)));
  /* FG_DIRECT_SLICE_INDEX */
  t466 = v101;
  t467 = ((float *)(v76).data) + gg_index(t466, ((int64_t)UINT64_C(2)));
  t468 = ((float)((*t467)));
  (*t462) = gg_function_40(t465, t468, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t469 = v101;
  t470 = ((float *)(v77).data) + gg_index(t469, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t471 = v101;
  t472 = ((float *)(v77).data) + gg_index(t471, ((int64_t)UINT64_C(2)));
  t473 = ((float)((*t472)));
  t474 = ((float)(v103));
  /* FG_DIRECT_SLICE_INDEX */
  t475 = v101;
  t476 = ((float *)(v75).data) + gg_index(t475, ((int64_t)UINT64_C(2)));
  t477 = ((float)((*t476)));
  (*t470) = gg_function_43(t473, t474, t477, base + 0);
  t478 = ((int64_t)(v101));
  t479 = ((int64_t)(((int64_t)UINT64_C(128))));
  t480 = ((int64_t)(((int64_t)((uint64_t)(t478) * (uint64_t)(t479)))));
  t481 = ((int64_t)(v45));
  t482 = ((int64_t)(((int64_t)((uint64_t)(t480) + (uint64_t)(t481)))));
  /* FG_DIRECT_SLICE_INDEX */
  t483 = v101;
  t484 = ((float *)(v75).data) + gg_index(t483, ((int64_t)UINT64_C(2)));
  t485 = ((float)((*t484)));
  dfa_sh_set_f32(t482, t485);
  v104 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label34:;
  t486 = ((int64_t)(v104));
  t487 = ((int64_t)(v72));
  if (!(((t486) < (t487)))) goto label35;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t488 = v101;
  t489 = ((gg_type_12 *)((v74).f2).data) + gg_index(t488, ((int64_t)UINT64_C(2)));
  t490 = gg_view(((*t489)).data, 4);
  t491 = v104;
  t492 = ((gg_type_11 *)t490.data) + gg_index(t491, t490.len);
  t493 = gg_view(((*t492)).data, 2);
  t494 = ((int64_t)UINT64_C(0));
  t495 = ((float *)t493.data) + gg_index(t494, t493.len);
  /* FG_DIRECT_SLICE_INDEX */
  t496 = v101;
  t497 = ((gg_type_12 *)((v74).f2).data) + gg_index(t496, ((int64_t)UINT64_C(2)));
  t498 = gg_view(((*t497)).data, 4);
  t499 = v104;
  t500 = ((gg_type_11 *)t498.data) + gg_index(t499, t498.len);
  t501 = gg_view(((*t500)).data, 2);
  t502 = ((int64_t)UINT64_C(0));
  t503 = ((float *)t501.data) + gg_index(t502, t501.len);
  t504 = ((float)((*t503)));
  t505 = ((float)(v103));
  (*t495) = ((float)(((t504) * (t505))));
  /* FG_DIRECT_SLICE_INDEX */
  t506 = v101;
  t507 = ((gg_type_12 *)((v74).f2).data) + gg_index(t506, ((int64_t)UINT64_C(2)));
  t508 = gg_view(((*t507)).data, 4);
  t509 = v104;
  t510 = ((gg_type_11 *)t508.data) + gg_index(t509, t508.len);
  t511 = gg_view(((*t510)).data, 2);
  t512 = ((int64_t)UINT64_C(1));
  t513 = ((float *)t511.data) + gg_index(t512, t511.len);
  /* FG_DIRECT_SLICE_INDEX */
  t514 = v101;
  t515 = ((gg_type_12 *)((v74).f2).data) + gg_index(t514, ((int64_t)UINT64_C(2)));
  t516 = gg_view(((*t515)).data, 4);
  t517 = v104;
  t518 = ((gg_type_11 *)t516.data) + gg_index(t517, t516.len);
  t519 = gg_view(((*t518)).data, 2);
  t520 = ((int64_t)UINT64_C(1));
  t521 = ((float *)t519.data) + gg_index(t520, t519.len);
  t522 = ((float)((*t521)));
  t523 = ((float)(v103));
  (*t513) = ((float)(((t522) * (t523))));
  t524 = v104;
  t525 = ((int64_t)(((int64_t)UINT64_C(1))));
  v104 = ((int64_t)((uint64_t)(t524) + (uint64_t)(t525)));
  }
  goto label34;
label35:;
  t526 = v101;
  t527 = ((int64_t)(((int64_t)UINT64_C(1))));
  v101 = ((int64_t)((uint64_t)(t526) + (uint64_t)(t527)));
  }
  goto label28;
label29:;
  dfa_syncwarp();
  v105 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label37:;
  t528 = ((int64_t)(v105));
  t529 = ((int64_t)(((int64_t)UINT64_C(32))));
  if (!(((t528) < (t529)))) goto label38;
  {
  t530 = ((int64_t)(v43));
  t531 = ((int64_t)(((int64_t)UINT64_C(32))));
  t532 = ((int64_t)(((int64_t)((uint64_t)(t530) * (uint64_t)(t531)))));
  t533 = ((int64_t)(v105));
  t534 = ((int64_t)(((int64_t)((uint64_t)(t532) + (uint64_t)(t533)))));
  t536 = ((int64_t)(v64));
  t537 = ((int64_t)(((int64_t)UINT64_C(32))));
  t538 = ((t536) == (t537));
  if (t538) {
  {
  t535 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  }
  } else {
  {
  t539 = ((int64_t)(v44));
  t540 = ((int64_t)(v64));
  t535 = ((int64_t)(((int64_t)gg_div(t539, t540))));
  }
  }
  t541 = ((int64_t)(t535));
  v106 = ((int64_t)(((int64_t)((uint64_t)(t534) + (uint64_t)(t541)))));
  t543 = ((int64_t)UINT64_C(2));
  t542 = 0;
label39:;
  if (t542 >= t543) goto label41;
  v107 = t542;
  {
  t544 = ((int64_t)(v107));
  t545 = ((int64_t)(v37));
  t546 = ((t544) < (t545));
  if (t546) {
  {
  /* FG_DIRECT_SLICE_INDEX */
  t547 = v107;
  t548 = ((float *)(v78).data) + gg_index(t547, ((int64_t)UINT64_C(2)));
  t549 = ((int64_t)(v107));
  t550 = ((int64_t)(((int64_t)UINT64_C(128))));
  t551 = ((int64_t)(((int64_t)((uint64_t)(t549) * (uint64_t)(t550)))));
  t552 = ((int64_t)(v106));
  t553 = ((int64_t)(((int64_t)((uint64_t)(t551) + (uint64_t)(t552)))));
  t554 = dfa_sh_f32(t553);
  (*t548) = ((float)(t554));
  }
  }
  }
label40:;
  t542++; goto label39;
label41:;
  v108 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label43:;
  t555 = ((int64_t)(v108));
  t556 = ((int64_t)(v42));
  t557 = ((int64_t)(((int64_t)UINT64_C(2))));
  t558 = ((int64_t)(((int64_t)gg_div(t556, t557))));
  if (!(((t555) < (t558)))) goto label44;
  {
  t559 = ((int64_t)(((int64_t)UINT64_C(2))));
  t560 = ((int64_t)(v108));
  t561 = ((int64_t)(((int64_t)((uint64_t)(t559) * (uint64_t)(t560)))));
  t563 = ((int64_t)(v64));
  t564 = ((int64_t)(((int64_t)UINT64_C(32))));
  t565 = ((t563) == (t564));
  if (t565) {
  {
  t562 = ((int64_t)(v44));
  }
  } else {
  {
  t566 = ((int64_t)(v44));
  t567 = ((int64_t)(v64));
  t562 = ((int64_t)(((int64_t)gg_mod(t566, t567))));
  }
  }
  t568 = ((int64_t)(t562));
  t569 = ((int64_t)(((int64_t)UINT64_C(4))));
  t570 = ((int64_t)(((int64_t)((uint64_t)(t568) * (uint64_t)(t569)))));
  v109 = ((int64_t)(((int64_t)((uint64_t)(t561) + (uint64_t)(t570)))));
  t571 = ((int64_t)(v58));
  t572 = ((int64_t)(v106));
  t573 = ((int64_t)(v28));
  t574 = ((int64_t)(((int64_t)((uint64_t)(t572) * (uint64_t)(t573)))));
  t575 = ((int64_t)(((int64_t)((uint64_t)(t571) + (uint64_t)(t574)))));
  t576 = ((int64_t)(v109));
  v110 = gg_function_45(t575, t576, base + 0);
  v111 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label46:;
  t577 = ((int64_t)(v111));
  t578 = ((int64_t)(v65));
  t579 = ((int64_t)(((int64_t)UINT64_C(2))));
  t580 = ((int64_t)(((int64_t)gg_div(t578, t579))));
  if (!(((t577) < (t580)))) goto label47;
  {
  v112 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label49:;
  t581 = ((int64_t)(v112));
  t582 = ((int64_t)(v37));
  if (!(((t581) < (t582)))) goto label50;
  {
  t583 = ((int64_t)(v108));
  t584 = ((int64_t)(v64));
  t585 = ((int64_t)(((int64_t)gg_div(t583, t584))));
  t586 = ((int64_t)(v111));
  v113 = ((int64_t)(((int64_t)((uint64_t)(t585) + (uint64_t)(t586)))));
  /* FG_DIRECT_SLICE_INDEX */
  t587 = v112;
  t588 = ((gg_type_12 *)((v74).f2).data) + gg_index(t587, ((int64_t)UINT64_C(2)));
  t589 = gg_view(((*t588)).data, 4);
  t590 = v113;
  t591 = ((gg_type_11 *)t589.data) + gg_index(t590, t589.len);
  t592 = gg_view(((*t591)).data, 2);
  t593 = ((int64_t)UINT64_C(0));
  t594 = ((float *)t592.data) + gg_index(t593, t592.len);
  /* FG_DIRECT_SLICE_INDEX */
  t595 = v112;
  t596 = ((gg_type_12 *)((v74).f2).data) + gg_index(t595, ((int64_t)UINT64_C(2)));
  t597 = gg_view(((*t596)).data, 4);
  t598 = v113;
  t599 = ((gg_type_11 *)t597.data) + gg_index(t598, t597.len);
  t600 = gg_view(((*t599)).data, 2);
  t601 = ((int64_t)UINT64_C(0));
  t602 = ((float *)t600.data) + gg_index(t601, t600.len);
  t603 = ((float)((*t602)));
  t604 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t605 = gg_view(((v110).f3).data, 4);
  t606 = ((int64_t)(((int64_t)UINT64_C(2))));
  t607 = ((int64_t)(v111));
  t608 = ((int64_t)((uint64_t)(t606) * (uint64_t)(t607)));
  t609 = ((float *)t605.data) + gg_index(t608, t605.len);
  t610 = ((float)((*t609)));
  /* FG_DIRECT_SLICE_INDEX */
  t611 = v112;
  t612 = ((float *)(v78).data) + gg_index(t611, ((int64_t)UINT64_C(2)));
  t613 = ((float)((*t612)));
  (*t594) = gg_function_47(t603, t604, t610, t613, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t614 = v112;
  t615 = ((gg_type_12 *)((v74).f2).data) + gg_index(t614, ((int64_t)UINT64_C(2)));
  t616 = gg_view(((*t615)).data, 4);
  t617 = v113;
  t618 = ((gg_type_11 *)t616.data) + gg_index(t617, t616.len);
  t619 = gg_view(((*t618)).data, 2);
  t620 = ((int64_t)UINT64_C(1));
  t621 = ((float *)t619.data) + gg_index(t620, t619.len);
  /* FG_DIRECT_SLICE_INDEX */
  t622 = v112;
  t623 = ((gg_type_12 *)((v74).f2).data) + gg_index(t622, ((int64_t)UINT64_C(2)));
  t624 = gg_view(((*t623)).data, 4);
  t625 = v113;
  t626 = ((gg_type_11 *)t624.data) + gg_index(t625, t624.len);
  t627 = gg_view(((*t626)).data, 2);
  t628 = ((int64_t)UINT64_C(1));
  t629 = ((float *)t627.data) + gg_index(t628, t627.len);
  t630 = ((float)((*t629)));
  t631 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t632 = gg_view(((v110).f3).data, 4);
  t633 = ((int64_t)(((int64_t)UINT64_C(2))));
  t634 = ((int64_t)(v111));
  t635 = ((int64_t)(((int64_t)((uint64_t)(t633) * (uint64_t)(t634)))));
  t636 = ((int64_t)(((int64_t)UINT64_C(1))));
  t637 = ((int64_t)((uint64_t)(t635) + (uint64_t)(t636)));
  t638 = ((float *)t632.data) + gg_index(t637, t632.len);
  t639 = ((float)((*t638)));
  /* FG_DIRECT_SLICE_INDEX */
  t640 = v112;
  t641 = ((float *)(v78).data) + gg_index(t640, ((int64_t)UINT64_C(2)));
  t642 = ((float)((*t641)));
  (*t621) = gg_function_47(t630, t631, t639, t642, base + 0);
  t643 = v112;
  t644 = ((int64_t)(((int64_t)UINT64_C(1))));
  v112 = ((int64_t)((uint64_t)(t643) + (uint64_t)(t644)));
  }
  goto label49;
label50:;
  t645 = v111;
  t646 = ((int64_t)(((int64_t)UINT64_C(1))));
  v111 = ((int64_t)((uint64_t)(t645) + (uint64_t)(t646)));
  }
  goto label46;
label47:;
  t647 = v108;
  t648 = ((int64_t)(v64));
  t649 = ((int64_t)(v65));
  t650 = ((int64_t)(((int64_t)((uint64_t)(t648) * (uint64_t)(t649)))));
  t651 = ((int64_t)(((int64_t)UINT64_C(2))));
  t652 = ((int64_t)(((int64_t)gg_div(t650, t651))));
  v108 = ((int64_t)((uint64_t)(t647) + (uint64_t)(t652)));
  }
  goto label43;
label44:;
  t653 = v105;
  t654 = ((int64_t)(v66));
  v105 = ((int64_t)((uint64_t)(t653) + (uint64_t)(t654)));
  }
  goto label37;
label38:;
  dfa_syncwarp();
  t655 = v92;
  t656 = ((int64_t)(v49));
  t657 = ((int64_t)(((int64_t)UINT64_C(128))));
  t658 = ((int64_t)(((int64_t)((uint64_t)(t656) * (uint64_t)(t657)))));
  v92 = ((int64_t)((uint64_t)(t655) + (uint64_t)(t658)));
  t659 = v57;
  t660 = ((int64_t)(v49));
  t661 = ((int64_t)(((int64_t)UINT64_C(128))));
  t662 = ((int64_t)(((int64_t)((uint64_t)(t660) * (uint64_t)(t661)))));
  t663 = ((int64_t)(v25));
  t664 = ((int64_t)(((int64_t)((uint64_t)(t662) * (uint64_t)(t663)))));
  v57 = ((int64_t)((uint64_t)(t659) + (uint64_t)(t664)));
  t665 = v58;
  t666 = ((int64_t)(v49));
  t667 = ((int64_t)(((int64_t)UINT64_C(128))));
  t668 = ((int64_t)(((int64_t)((uint64_t)(t666) * (uint64_t)(t667)))));
  t669 = ((int64_t)(v28));
  t670 = ((int64_t)(((int64_t)((uint64_t)(t668) * (uint64_t)(t669)))));
  v58 = ((int64_t)((uint64_t)(t665) + (uint64_t)(t670)));
  t671 = v59;
  t672 = ((int64_t)(((int64_t)UINT64_C(2))));
  t673 = ((int64_t)(v49));
  t674 = ((int64_t)(((int64_t)UINT64_C(128))));
  t675 = ((int64_t)(((int64_t)((uint64_t)(t673) * (uint64_t)(t674)))));
  t676 = ((int64_t)(((int64_t)((uint64_t)(t672) * (uint64_t)(t675)))));
  v59 = ((int64_t)((uint64_t)(t671) + (uint64_t)(t676)));
  }
  goto label16;
label17:;
  t677 = ((int64_t)(v39));
  t678 = ((int64_t)(((int64_t)UINT64_C(0))));
  t679 = ((t677) != (t678));
  t680 = t679;
  if (t680) {
  t681 = ((int64_t)(v47));
  t682 = ((int64_t)(((int64_t)UINT64_C(0))));
  t680 = ((t681) == (t682));
  }
  t683 = t680;
  if (t683) {
  {
  t684 = ((int64_t)(v4));
  t685 = ((int64_t)(((int64_t)UINT64_C(4))));
  t686 = ((int64_t)(v53));
  t687 = ((int64_t)(((int64_t)((uint64_t)(t685) * (uint64_t)(t686)))));
  t688 = dfa_ld_f32(t684, t687);
  v114 = ((float)(t688));
  v115 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label52:;
  t689 = ((int64_t)(v115));
  t690 = ((int64_t)(v37));
  if (!(((t689) < (t690)))) goto label53;
  {
  t691 = ((int64_t)(v115));
  t692 = ((int64_t)(v43));
  v116 = ((int64_t)(((int64_t)((uint64_t)(t691) + (uint64_t)(t692)))));
  t693 = ((int64_t)(v115));
  t694 = ((int64_t)(v61));
  t695 = ((int64_t)(((int64_t)((uint64_t)(t693) + (uint64_t)(t694)))));
  t696 = ((int64_t)(v37));
  t697 = ((t695) > (t696));
  t698 = t697;
  if (t698) {
  t699 = ((int64_t)(v116));
  t700 = ((int64_t)(v37));
  t698 = ((t699) >= (t700));
  }
  t701 = t698;
  t702 = (!(t701));
  if (t702) {
  {
  t703 = ((float)(v114));
  /* FG_DIRECT_SLICE_INDEX */
  t704 = v116;
  t705 = ((float *)(v76).data) + gg_index(t704, ((int64_t)UINT64_C(2)));
  t706 = ((float)((*t705)));
  v117 = gg_function_39(t703, t706, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t707 = v116;
  t708 = ((float *)(v76).data) + gg_index(t707, ((int64_t)UINT64_C(2)));
  t709 = ((float)((*t708)));
  t710 = ((float)(v117));
  v118 = gg_function_40(t709, t710, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t711 = v116;
  t712 = ((float *)(v76).data) + gg_index(t711, ((int64_t)UINT64_C(2)));
  (*t712) = ((float)(v117));
  t713 = ((int64_t)(v44));
  t714 = ((int64_t)(((int64_t)UINT64_C(0))));
  t715 = ((t713) == (t714));
  if (t715) {
  {
  t716 = ((float)(v114));
  /* FG_DIRECT_SLICE_INDEX */
  t717 = v116;
  t718 = ((float *)(v76).data) + gg_index(t717, ((int64_t)UINT64_C(2)));
  t719 = ((float)((*t718)));
  v119 = gg_function_40(t716, t719, base + 0);
  }
  } else {
  {
  v119 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  }
  }
  /* FG_DIRECT_SLICE_INDEX */
  t720 = v116;
  t721 = ((float *)(v77).data) + gg_index(t720, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t722 = v116;
  t723 = ((float *)(v77).data) + gg_index(t722, ((int64_t)UINT64_C(2)));
  t724 = ((float)((*t723)));
  t725 = ((float)(v118));
  t726 = ((float)(v119));
  (*t721) = gg_function_43(t724, t725, t726, base + 0);
  v120 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label55:;
  t727 = ((int64_t)(v120));
  t728 = ((int64_t)(v72));
  if (!(((t727) < (t728)))) goto label56;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t729 = v116;
  t730 = ((gg_type_12 *)((v74).f2).data) + gg_index(t729, ((int64_t)UINT64_C(2)));
  t731 = gg_view(((*t730)).data, 4);
  t732 = v120;
  t733 = ((gg_type_11 *)t731.data) + gg_index(t732, t731.len);
  t734 = gg_view(((*t733)).data, 2);
  t735 = ((int64_t)UINT64_C(0));
  t736 = ((float *)t734.data) + gg_index(t735, t734.len);
  /* FG_DIRECT_SLICE_INDEX */
  t737 = v116;
  t738 = ((gg_type_12 *)((v74).f2).data) + gg_index(t737, ((int64_t)UINT64_C(2)));
  t739 = gg_view(((*t738)).data, 4);
  t740 = v120;
  t741 = ((gg_type_11 *)t739.data) + gg_index(t740, t739.len);
  t742 = gg_view(((*t741)).data, 2);
  t743 = ((int64_t)UINT64_C(0));
  t744 = ((float *)t742.data) + gg_index(t743, t742.len);
  t745 = ((float)((*t744)));
  t746 = ((float)(v118));
  (*t736) = ((float)(((t745) * (t746))));
  /* FG_DIRECT_SLICE_INDEX */
  t747 = v116;
  t748 = ((gg_type_12 *)((v74).f2).data) + gg_index(t747, ((int64_t)UINT64_C(2)));
  t749 = gg_view(((*t748)).data, 4);
  t750 = v120;
  t751 = ((gg_type_11 *)t749.data) + gg_index(t750, t749.len);
  t752 = gg_view(((*t751)).data, 2);
  t753 = ((int64_t)UINT64_C(1));
  t754 = ((float *)t752.data) + gg_index(t753, t752.len);
  /* FG_DIRECT_SLICE_INDEX */
  t755 = v116;
  t756 = ((gg_type_12 *)((v74).f2).data) + gg_index(t755, ((int64_t)UINT64_C(2)));
  t757 = gg_view(((*t756)).data, 4);
  t758 = v120;
  t759 = ((gg_type_11 *)t757.data) + gg_index(t758, t757.len);
  t760 = gg_view(((*t759)).data, 2);
  t761 = ((int64_t)UINT64_C(1));
  t762 = ((float *)t760.data) + gg_index(t761, t760.len);
  t763 = ((float)((*t762)));
  t764 = ((float)(v118));
  (*t754) = ((float)(((t763) * (t764))));
  t765 = v120;
  t766 = ((int64_t)(((int64_t)UINT64_C(1))));
  v120 = ((int64_t)((uint64_t)(t765) + (uint64_t)(t766)));
  }
  goto label55;
label56:;
  }
  }
  t767 = v115;
  t768 = ((int64_t)(v61));
  v115 = ((int64_t)((uint64_t)(t767) + (uint64_t)(t768)));
  }
  goto label52;
label53:;
  }
  }
  t770 = ((int64_t)UINT64_C(2));
  t769 = 0;
label57:;
  if (t769 >= t770) goto label59;
  v121 = t769;
  {
  t771 = ((int64_t)(v121));
  t772 = ((int64_t)(v37));
  t773 = ((t771) < (t772));
  t774 = t773;
  if (t774) {
  t775 = ((int64_t)(v43));
  t776 = ((int64_t)(((int64_t)UINT64_C(0))));
  t774 = ((t775) == (t776));
  }
  t777 = t774;
  if (t777) {
  {
  t778 = ((int64_t)(v70));
  t779 = ((int64_t)(v121));
  t780 = ((int64_t)(((int64_t)UINT64_C(32))));
  t781 = ((int64_t)(((int64_t)((uint64_t)(t779) * (uint64_t)(t780)))));
  t782 = ((int64_t)(((int64_t)((uint64_t)(t778) + (uint64_t)(t781)))));
  t783 = ((int64_t)(v44));
  t784 = ((int64_t)(((int64_t)((uint64_t)(t782) + (uint64_t)(t783)))));
  t785 = dfa_neg_half_flt_max();
  t786 = ((float)(t785));
  dfa_sh_set_f32(t784, t786);
  t787 = ((int64_t)(v71));
  t788 = ((int64_t)(v121));
  t789 = ((int64_t)(((int64_t)UINT64_C(32))));
  t790 = ((int64_t)(((int64_t)((uint64_t)(t788) * (uint64_t)(t789)))));
  t791 = ((int64_t)(((int64_t)((uint64_t)(t787) + (uint64_t)(t790)))));
  t792 = ((int64_t)(v44));
  t793 = ((int64_t)(((int64_t)((uint64_t)(t791) + (uint64_t)(t792)))));
  t794 = ((float)(gg_double_bits(UINT64_C(0))));
  dfa_sh_set_f32(t793, t794);
  }
  }
  }
label58:;
  t769++; goto label57;
label59:;
  dfa_sync();
  t796 = ((int64_t)UINT64_C(2));
  t795 = 0;
label60:;
  if (t795 >= t796) goto label62;
  v122 = t795;
  {
  t797 = ((int64_t)(v122));
  t798 = ((int64_t)(v37));
  t799 = ((t797) < (t798));
  t800 = t799;
  if (t800) {
  t801 = ((int64_t)(v44));
  t802 = ((int64_t)(((int64_t)UINT64_C(0))));
  t800 = ((t801) == (t802));
  }
  t803 = t800;
  if (t803) {
  {
  t804 = ((int64_t)(v70));
  t805 = ((int64_t)(v122));
  t806 = ((int64_t)(((int64_t)UINT64_C(32))));
  t807 = ((int64_t)(((int64_t)((uint64_t)(t805) * (uint64_t)(t806)))));
  t808 = ((int64_t)(((int64_t)((uint64_t)(t804) + (uint64_t)(t807)))));
  t809 = ((int64_t)(v43));
  t810 = ((int64_t)(((int64_t)((uint64_t)(t808) + (uint64_t)(t809)))));
  /* FG_DIRECT_SLICE_INDEX */
  t811 = v122;
  t812 = ((float *)(v76).data) + gg_index(t811, ((int64_t)UINT64_C(2)));
  t813 = ((float)((*t812)));
  dfa_sh_set_f32(t810, t813);
  }
  }
  }
label61:;
  t795++; goto label60;
label62:;
  dfa_sync();
  v123 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label64:;
  t814 = ((int64_t)(v123));
  t815 = ((int64_t)(v37));
  if (!(((t814) < (t815)))) goto label65;
  {
  t816 = ((int64_t)(v37));
  t817 = ((int64_t)(((int64_t)UINT64_C(1))));
  t818 = ((t816) > (t817));
  t819 = t818;
  if (t819) {
  t820 = ((int64_t)(v51));
  t821 = ((int64_t)(v123));
  t822 = ((int64_t)(((int64_t)((uint64_t)(t820) + (uint64_t)(t821)))));
  t823 = ((int64_t)(v15));
  t819 = ((t822) >= (t823));
  }
  t824 = t819;
  v124 = (!(t824));
  t825 = v124;
  if (t825) {
  {
  t826 = ((int64_t)(v70));
  t827 = ((int64_t)(v123));
  t828 = ((int64_t)(((int64_t)UINT64_C(32))));
  t829 = ((int64_t)(((int64_t)((uint64_t)(t827) * (uint64_t)(t828)))));
  t830 = ((int64_t)(((int64_t)((uint64_t)(t826) + (uint64_t)(t829)))));
  t831 = ((int64_t)(v44));
  t832 = ((int64_t)(((int64_t)((uint64_t)(t830) + (uint64_t)(t831)))));
  t833 = dfa_sh_f32(t832);
  v125 = ((float)(t833));
  t834 = ((float)(v125));
  v125 = gg_function_48(t834, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t835 = v123;
  t836 = ((float *)(v76).data) + gg_index(t835, ((int64_t)UINT64_C(2)));
  t837 = ((float)((*t836)));
  t838 = ((float)(v125));
  v126 = gg_function_40(t837, t838, base + 0);
  /* FG_DIRECT_SLICE_INDEX */
  t839 = v123;
  t840 = ((float *)(v76).data) + gg_index(t839, ((int64_t)UINT64_C(2)));
  (*t840) = ((float)(v125));
  v127 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label67:;
  t841 = ((int64_t)(v127));
  t842 = ((int64_t)(v72));
  if (!(((t841) < (t842)))) goto label68;
  {
  /* FG_DIRECT_SLICE_INDEX */
  t843 = v123;
  t844 = ((gg_type_12 *)((v74).f2).data) + gg_index(t843, ((int64_t)UINT64_C(2)));
  t845 = gg_view(((*t844)).data, 4);
  t846 = v127;
  t847 = ((gg_type_11 *)t845.data) + gg_index(t846, t845.len);
  t848 = gg_view(((*t847)).data, 2);
  t849 = ((int64_t)UINT64_C(0));
  t850 = ((float *)t848.data) + gg_index(t849, t848.len);
  /* FG_DIRECT_SLICE_INDEX */
  t851 = v123;
  t852 = ((gg_type_12 *)((v74).f2).data) + gg_index(t851, ((int64_t)UINT64_C(2)));
  t853 = gg_view(((*t852)).data, 4);
  t854 = v127;
  t855 = ((gg_type_11 *)t853.data) + gg_index(t854, t853.len);
  t856 = gg_view(((*t855)).data, 2);
  t857 = ((int64_t)UINT64_C(0));
  t858 = ((float *)t856.data) + gg_index(t857, t856.len);
  t859 = ((float)((*t858)));
  t860 = ((float)(v126));
  (*t850) = ((float)(((t859) * (t860))));
  /* FG_DIRECT_SLICE_INDEX */
  t861 = v123;
  t862 = ((gg_type_12 *)((v74).f2).data) + gg_index(t861, ((int64_t)UINT64_C(2)));
  t863 = gg_view(((*t862)).data, 4);
  t864 = v127;
  t865 = ((gg_type_11 *)t863.data) + gg_index(t864, t863.len);
  t866 = gg_view(((*t865)).data, 2);
  t867 = ((int64_t)UINT64_C(1));
  t868 = ((float *)t866.data) + gg_index(t867, t866.len);
  /* FG_DIRECT_SLICE_INDEX */
  t869 = v123;
  t870 = ((gg_type_12 *)((v74).f2).data) + gg_index(t869, ((int64_t)UINT64_C(2)));
  t871 = gg_view(((*t870)).data, 4);
  t872 = v127;
  t873 = ((gg_type_11 *)t871.data) + gg_index(t872, t871.len);
  t874 = gg_view(((*t873)).data, 2);
  t875 = ((int64_t)UINT64_C(1));
  t876 = ((float *)t874.data) + gg_index(t875, t874.len);
  t877 = ((float)((*t876)));
  t878 = ((float)(v126));
  (*t868) = ((float)(((t877) * (t878))));
  t879 = v127;
  t880 = ((int64_t)(((int64_t)UINT64_C(1))));
  v127 = ((int64_t)((uint64_t)(t879) + (uint64_t)(t880)));
  }
  goto label67;
label68:;
  v128 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label70:;
  t881 = ((int64_t)(v128));
  t882 = ((int64_t)(v42));
  t883 = ((int64_t)(((int64_t)UINT64_C(2))));
  t884 = ((int64_t)(((int64_t)gg_div(t882, t883))));
  if (!(((t881) < (t884)))) goto label71;
  {
  t885 = ((int64_t)(v43));
  t886 = ((int64_t)(v66));
  t887 = ((int64_t)(v42));
  t888 = ((int64_t)(((int64_t)((uint64_t)(t886) * (uint64_t)(t887)))));
  t889 = ((int64_t)(((int64_t)UINT64_C(2))));
  t890 = ((int64_t)(((int64_t)gg_div(t888, t889))));
  t891 = ((int64_t)(((int64_t)((uint64_t)(t885) * (uint64_t)(t890)))));
  t893 = ((int64_t)(v64));
  t894 = ((int64_t)(((int64_t)UINT64_C(32))));
  t895 = ((t893) == (t894));
  if (t895) {
  {
  t892 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
  }
  } else {
  {
  t896 = ((int64_t)(v44));
  t897 = ((int64_t)(v64));
  t898 = ((int64_t)(((int64_t)gg_div(t896, t897))));
  t899 = ((int64_t)(v42));
  t900 = ((int64_t)(((int64_t)UINT64_C(2))));
  t901 = ((int64_t)(((int64_t)gg_div(t899, t900))));
  t892 = ((int64_t)(((int64_t)((uint64_t)(t898) * (uint64_t)(t901)))));
  }
  }
  t902 = ((int64_t)(t892));
  v129 = ((int64_t)(((int64_t)((uint64_t)(t891) + (uint64_t)(t902)))));
  t903 = ((int64_t)(v129));
  t904 = ((int64_t)(v128));
  t905 = ((int64_t)(((int64_t)((uint64_t)(t903) + (uint64_t)(t904)))));
  t907 = ((int64_t)(v64));
  t908 = ((int64_t)(((int64_t)UINT64_C(32))));
  t909 = ((t907) == (t908));
  if (t909) {
  {
  t906 = ((int64_t)(v44));
  }
  } else {
  {
  t910 = ((int64_t)(v44));
  t911 = ((int64_t)(v64));
  t906 = ((int64_t)(((int64_t)gg_mod(t910, t911))));
  }
  }
  t912 = ((int64_t)(t906));
  t913 = ((int64_t)(v65));
  t914 = ((int64_t)(((int64_t)UINT64_C(2))));
  t915 = ((int64_t)(((int64_t)gg_div(t913, t914))));
  t916 = ((int64_t)(((int64_t)((uint64_t)(t912) * (uint64_t)(t915)))));
  v130 = ((int64_t)(((int64_t)((uint64_t)(t905) + (uint64_t)(t916)))));
  t917 = ((int64_t)(((int64_t)UINT64_C(2))));
  t918 = ((int64_t)(v130));
  t919 = ((int64_t)(((int64_t)((uint64_t)(t917) * (uint64_t)(t918)))));
  /* FG_DIRECT_SLICE_INDEX */
  t920 = v123;
  t921 = ((gg_type_12 *)((v74).f2).data) + gg_index(t920, ((int64_t)UINT64_C(2)));
  t922 = gg_view(((*t921)).data, 4);
  t923 = ((int64_t)(v128));
  t924 = ((int64_t)(v64));
  t925 = ((int64_t)gg_div(t923, t924));
  t926 = ((gg_type_11 *)t922.data) + gg_index(t925, t922.len);
  t927 = gg_view(((*t926)).data, 2);
  t928 = ((int64_t)UINT64_C(0));
  t929 = ((float *)t927.data) + gg_index(t928, t927.len);
  t930 = ((float)((*t929)));
  dfa_sh_set_f32(t919, t930);
  t931 = ((int64_t)(((int64_t)UINT64_C(2))));
  t932 = ((int64_t)(v130));
  t933 = ((int64_t)(((int64_t)((uint64_t)(t931) * (uint64_t)(t932)))));
  t934 = ((int64_t)(((int64_t)UINT64_C(1))));
  t935 = ((int64_t)(((int64_t)((uint64_t)(t933) + (uint64_t)(t934)))));
  /* FG_DIRECT_SLICE_INDEX */
  t936 = v123;
  t937 = ((gg_type_12 *)((v74).f2).data) + gg_index(t936, ((int64_t)UINT64_C(2)));
  t938 = gg_view(((*t937)).data, 4);
  t939 = ((int64_t)(v128));
  t940 = ((int64_t)(v64));
  t941 = ((int64_t)gg_div(t939, t940));
  t942 = ((gg_type_11 *)t938.data) + gg_index(t941, t938.len);
  t943 = gg_view(((*t942)).data, 2);
  t944 = ((int64_t)UINT64_C(1));
  t945 = ((float *)t943.data) + gg_index(t944, t943.len);
  t946 = ((float)((*t945)));
  dfa_sh_set_f32(t935, t946);
  t947 = ((int64_t)(((int64_t)UINT64_C(2))));
  t948 = ((int64_t)(v130));
  t949 = ((int64_t)(((int64_t)UINT64_C(1))));
  t950 = ((int64_t)(((int64_t)((uint64_t)(t948) + (uint64_t)(t949)))));
  t951 = ((int64_t)(((int64_t)((uint64_t)(t947) * (uint64_t)(t950)))));
  /* FG_DIRECT_SLICE_INDEX */
  t952 = v123;
  t953 = ((gg_type_12 *)((v74).f2).data) + gg_index(t952, ((int64_t)UINT64_C(2)));
  t954 = gg_view(((*t953)).data, 4);
  t955 = ((int64_t)(v128));
  t956 = ((int64_t)(v64));
  t957 = ((int64_t)(((int64_t)gg_div(t955, t956))));
  t958 = ((int64_t)(((int64_t)UINT64_C(1))));
  t959 = ((int64_t)((uint64_t)(t957) + (uint64_t)(t958)));
  t960 = ((gg_type_11 *)t954.data) + gg_index(t959, t954.len);
  t961 = gg_view(((*t960)).data, 2);
  t962 = ((int64_t)UINT64_C(0));
  t963 = ((float *)t961.data) + gg_index(t962, t961.len);
  t964 = ((float)((*t963)));
  dfa_sh_set_f32(t951, t964);
  t965 = ((int64_t)(((int64_t)UINT64_C(2))));
  t966 = ((int64_t)(v130));
  t967 = ((int64_t)(((int64_t)UINT64_C(1))));
  t968 = ((int64_t)(((int64_t)((uint64_t)(t966) + (uint64_t)(t967)))));
  t969 = ((int64_t)(((int64_t)((uint64_t)(t965) * (uint64_t)(t968)))));
  t970 = ((int64_t)(((int64_t)UINT64_C(1))));
  t971 = ((int64_t)(((int64_t)((uint64_t)(t969) + (uint64_t)(t970)))));
  /* FG_DIRECT_SLICE_INDEX */
  t972 = v123;
  t973 = ((gg_type_12 *)((v74).f2).data) + gg_index(t972, ((int64_t)UINT64_C(2)));
  t974 = gg_view(((*t973)).data, 4);
  t975 = ((int64_t)(v128));
  t976 = ((int64_t)(v64));
  t977 = ((int64_t)(((int64_t)gg_div(t975, t976))));
  t978 = ((int64_t)(((int64_t)UINT64_C(1))));
  t979 = ((int64_t)((uint64_t)(t977) + (uint64_t)(t978)));
  t980 = ((gg_type_11 *)t974.data) + gg_index(t979, t974.len);
  t981 = gg_view(((*t980)).data, 2);
  t982 = ((int64_t)UINT64_C(1));
  t983 = ((float *)t981.data) + gg_index(t982, t981.len);
  t984 = ((float)((*t983)));
  dfa_sh_set_f32(t971, t984);
  t985 = v128;
  t986 = ((int64_t)(v64));
  t987 = ((int64_t)(v65));
  t988 = ((int64_t)(((int64_t)((uint64_t)(t986) * (uint64_t)(t987)))));
  t989 = ((int64_t)(((int64_t)UINT64_C(2))));
  t990 = ((int64_t)(((int64_t)gg_div(t988, t989))));
  v128 = ((int64_t)((uint64_t)(t985) + (uint64_t)(t990)));
  }
  goto label70;
label71:;
  /* FG_DIRECT_SLICE_INDEX */
  t991 = v123;
  t992 = ((float *)(v77).data) + gg_index(t991, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t993 = v123;
  t994 = ((float *)(v77).data) + gg_index(t993, ((int64_t)UINT64_C(2)));
  t995 = ((float)((*t994)));
  t996 = ((float)(v126));
  (*t992) = ((float)(((t995) * (t996))));
  /* FG_DIRECT_SLICE_INDEX */
  t997 = v123;
  t998 = ((float *)(v77).data) + gg_index(t997, ((int64_t)UINT64_C(2)));
  /* FG_DIRECT_SLICE_INDEX */
  t999 = v123;
  t1000 = ((float *)(v77).data) + gg_index(t999, ((int64_t)UINT64_C(2)));
  t1001 = ((float)((*t1000)));
  t1002 = ((int64_t)(((int64_t)UINT64_C(32))));
  (*t998) = gg_function_33(t1001, t1002, base + 0);
  t1003 = ((int64_t)(v44));
  t1004 = ((int64_t)(((int64_t)UINT64_C(0))));
  t1005 = ((t1003) == (t1004));
  if (t1005) {
  {
  t1006 = ((int64_t)(v71));
  t1007 = ((int64_t)(v123));
  t1008 = ((int64_t)(((int64_t)UINT64_C(32))));
  t1009 = ((int64_t)(((int64_t)((uint64_t)(t1007) * (uint64_t)(t1008)))));
  t1010 = ((int64_t)(((int64_t)((uint64_t)(t1006) + (uint64_t)(t1009)))));
  t1011 = ((int64_t)(v43));
  t1012 = ((int64_t)(((int64_t)((uint64_t)(t1010) + (uint64_t)(t1011)))));
  /* FG_DIRECT_SLICE_INDEX */
  t1013 = v123;
  t1014 = ((float *)(v77).data) + gg_index(t1013, ((int64_t)UINT64_C(2)));
  t1015 = ((float)((*t1014)));
  dfa_sh_set_f32(t1012, t1015);
  }
  }
  dfa_sync();
  t1016 = ((int64_t)(((int64_t)UINT64_C(128))));
  t1017 = ((int64_t)(v42));
  t1018 = ((t1016) <= (t1017));
  t1019 = t1018;
  if (!t1019) {
  t1020 = ((int64_t)(v45));
  t1021 = ((int64_t)(v42));
  t1019 = ((t1020) < (t1021));
  }
  t1022 = t1019;
  if (t1022) {
  {
  t1023 = ((int64_t)(v71));
  t1024 = ((int64_t)(v123));
  t1025 = ((int64_t)(((int64_t)UINT64_C(32))));
  t1026 = ((int64_t)(((int64_t)((uint64_t)(t1024) * (uint64_t)(t1025)))));
  t1027 = ((int64_t)(((int64_t)((uint64_t)(t1023) + (uint64_t)(t1026)))));
  t1028 = ((int64_t)(v44));
  t1029 = ((int64_t)(((int64_t)((uint64_t)(t1027) + (uint64_t)(t1028)))));
  t1030 = dfa_sh_f32(t1029);
  v131 = ((float)(t1030));
  t1031 = ((float)(v131));
  t1032 = ((int64_t)(((int64_t)UINT64_C(32))));
  v131 = gg_function_33(t1031, t1032, base + 0);
  v132 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label73:;
  t1033 = ((int64_t)(v132));
  t1034 = ((int64_t)(v42));
  if (!(((t1033) < (t1034)))) goto label74;
  {
  v133 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  v134 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label76:;
  t1035 = ((int64_t)(v134));
  t1036 = ((int64_t)(v61));
  if (!(((t1035) < (t1036)))) goto label77;
  {
  v135 = ((int64_t)(((int64_t)(((int64_t)UINT64_C(0))))));
label79:;
  t1037 = ((int64_t)(v135));
  t1038 = ((int64_t)(v66));
  if (!(((t1037) < (t1038)))) goto label80;
  {
  t1039 = v133;
  t1040 = ((int64_t)(v134));
  t1041 = ((int64_t)(v66));
  t1042 = ((int64_t)(v42));
  t1043 = ((int64_t)(((int64_t)((uint64_t)(t1041) * (uint64_t)(t1042)))));
  t1044 = ((int64_t)(((int64_t)((uint64_t)(t1040) * (uint64_t)(t1043)))));
  t1045 = ((int64_t)(v135));
  t1046 = ((int64_t)(v42));
  t1047 = ((int64_t)(((int64_t)((uint64_t)(t1045) * (uint64_t)(t1046)))));
  t1048 = ((int64_t)(((int64_t)((uint64_t)(t1044) + (uint64_t)(t1047)))));
  t1049 = ((int64_t)(v132));
  t1050 = ((int64_t)(((int64_t)((uint64_t)(t1048) + (uint64_t)(t1049)))));
  t1051 = ((int64_t)(v45));
  t1052 = ((int64_t)(((int64_t)((uint64_t)(t1050) + (uint64_t)(t1051)))));
  t1053 = dfa_sh_f32(t1052);
  t1054 = ((float)(t1053));
  v133 = ((t1039) + (t1054));
  t1055 = v135;
  t1056 = ((int64_t)(((int64_t)UINT64_C(1))));
  v135 = ((int64_t)((uint64_t)(t1055) + (uint64_t)(t1056)));
  }
  goto label79;
label80:;
  t1057 = v134;
  t1058 = ((int64_t)(((int64_t)UINT64_C(1))));
  v134 = ((int64_t)((uint64_t)(t1057) + (uint64_t)(t1058)));
  }
  goto label76;
label77:;
  t1059 = ((int64_t)(v49));
  t1060 = ((int64_t)(((int64_t)UINT64_C(1))));
  t1061 = ((t1059) == (t1060));
  if (t1061) {
  {
  t1062 = ((float)(v133));
  t1063 = ((float)(v131));
  v133 = ((float)(((t1062) / (t1063))));
  }
  }
  t1064 = ((int64_t)(((int64_t)UINT64_C(4))));
  t1065 = ((int64_t)(v52));
  t1066 = ((int64_t)(v15));
  t1067 = ((int64_t)(((int64_t)((uint64_t)(t1065) * (uint64_t)(t1066)))));
  t1068 = ((int64_t)(v51));
  t1069 = ((int64_t)(((int64_t)((uint64_t)(t1067) + (uint64_t)(t1068)))));
  t1070 = ((int64_t)(v123));
  t1071 = ((int64_t)(((int64_t)((uint64_t)(t1069) + (uint64_t)(t1070)))));
  t1072 = ((int64_t)(v16));
  t1073 = ((int64_t)(((int64_t)((uint64_t)(t1071) * (uint64_t)(t1072)))));
  t1074 = ((int64_t)(v53));
  t1075 = ((int64_t)(((int64_t)((uint64_t)(t1073) + (uint64_t)(t1074)))));
  t1076 = ((int64_t)(v49));
  t1077 = ((int64_t)(((int64_t)((uint64_t)(t1075) * (uint64_t)(t1076)))));
  t1078 = ((int64_t)(v47));
  t1079 = ((int64_t)(((int64_t)((uint64_t)(t1077) + (uint64_t)(t1078)))));
  t1080 = ((int64_t)(v42));
  t1081 = ((int64_t)(((int64_t)((uint64_t)(t1079) * (uint64_t)(t1080)))));
  t1082 = ((int64_t)(v132));
  t1083 = ((int64_t)(((int64_t)((uint64_t)(t1081) + (uint64_t)(t1082)))));
  t1084 = ((int64_t)(v45));
  t1085 = ((int64_t)(((int64_t)((uint64_t)(t1083) + (uint64_t)(t1084)))));
  v136 = ((int64_t)(((int64_t)((uint64_t)(t1064) * (uint64_t)(t1085)))));
  t1086 = ((int64_t)(v6));
  t1087 = ((int64_t)(v136));
  t1088 = ((float)(v133));
  dfa_st_f32(t1086, t1087, t1088);
  t1089 = v132;
  t1090 = ((int64_t)(((int64_t)UINT64_C(128))));
  v132 = ((int64_t)((uint64_t)(t1089) + (uint64_t)(t1090)));
  }
  goto label73;
label74:;
  }
  }
  t1091 = ((int64_t)(v123));
  t1092 = ((int64_t)(((int64_t)UINT64_C(1))));
  t1093 = ((int64_t)(((int64_t)((uint64_t)(t1091) + (uint64_t)(t1092)))));
  t1094 = ((int64_t)(v37));
  t1095 = ((t1093) < (t1094));
  if (t1095) {
  {
  dfa_sync();
  }
  }
  }
  }
  t1096 = v123;
  t1097 = ((int64_t)(((int64_t)UINT64_C(1))));
  v123 = ((int64_t)((uint64_t)(t1096) + (uint64_t)(t1097)));
  }
  goto label64;
label65:;
  t1098 = ((int64_t)(v49));
  t1099 = ((int64_t)(((int64_t)UINT64_C(1))));
  t1100 = ((t1098) != (t1099));
  t1101 = t1100;
  if (t1101) {
  t1102 = ((int64_t)(v45));
  t1103 = ((int64_t)(v37));
  t1101 = ((t1102) < (t1103));
  }
  t1104 = t1101;
  t1105 = t1104;
  if (t1105) {
  t1106 = ((int64_t)(v37));
  t1107 = ((int64_t)(((int64_t)UINT64_C(1))));
  t1108 = ((t1106) == (t1107));
  t1109 = t1108;
  if (!t1109) {
  t1110 = ((int64_t)(v51));
  t1111 = ((int64_t)(v45));
  t1112 = ((int64_t)(((int64_t)((uint64_t)(t1110) + (uint64_t)(t1111)))));
  t1113 = ((int64_t)(v15));
  t1109 = ((t1112) < (t1113));
  }
  t1105 = t1109;
  }
  t1114 = t1105;
  if (t1114) {
  {
  t1115 = ((int64_t)(((int64_t)UINT64_C(8))));
  t1116 = ((int64_t)(v52));
  t1117 = ((int64_t)(v15));
  t1118 = ((int64_t)(((int64_t)((uint64_t)(t1116) * (uint64_t)(t1117)))));
  t1119 = ((int64_t)(v51));
  t1120 = ((int64_t)(((int64_t)((uint64_t)(t1118) + (uint64_t)(t1119)))));
  t1121 = ((int64_t)(v45));
  t1122 = ((int64_t)(((int64_t)((uint64_t)(t1120) + (uint64_t)(t1121)))));
  t1123 = ((int64_t)(v16));
  t1124 = ((int64_t)(((int64_t)((uint64_t)(t1122) * (uint64_t)(t1123)))));
  t1125 = ((int64_t)(v53));
  t1126 = ((int64_t)(((int64_t)((uint64_t)(t1124) + (uint64_t)(t1125)))));
  t1127 = ((int64_t)(v49));
  t1128 = ((int64_t)(((int64_t)((uint64_t)(t1126) * (uint64_t)(t1127)))));
  t1129 = ((int64_t)(v47));
  t1130 = ((int64_t)(((int64_t)((uint64_t)(t1128) + (uint64_t)(t1129)))));
  v137 = ((int64_t)(((int64_t)((uint64_t)(t1115) * (uint64_t)(t1130)))));
  t1131 = ((int64_t)(v7));
  t1132 = ((int64_t)(v137));
  /* FG_DIRECT_SLICE_INDEX */
  t1133 = v45;
  t1134 = ((float *)(v76).data) + gg_index(t1133, ((int64_t)UINT64_C(2)));
  t1135 = ((float)((*t1134)));
  dfa_st_f32(t1131, t1132, t1135);
  t1136 = ((int64_t)(v7));
  t1137 = ((int64_t)(v137));
  t1138 = ((int64_t)(((int64_t)UINT64_C(4))));
  t1139 = ((int64_t)(((int64_t)((uint64_t)(t1137) + (uint64_t)(t1138)))));
  /* FG_DIRECT_SLICE_INDEX */
  t1140 = v45;
  t1141 = ((float *)(v77).data) + gg_index(t1140, ((int64_t)UINT64_C(2)));
  t1142 = ((float)((*t1141)));
  dfa_st_f32(t1136, t1139, t1142);
  }
  }
  t0 = ((float)(((float)(gg_double_bits(UINT64_C(0))))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __host__ __device__ __forceinline__ float gg_function_1(int64_t p0, int64_t p1, int64_t p2, int64_t base) {
  float t0;
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
  uint32_t t15;
  uint32_t t16;
  float t17;
  int64_t t18;
  int64_t t19;
  int64_t t20;
  int64_t t21;
  uint32_t t22;
  uint32_t t23;
  int64_t t24;
  int64_t t25;
  float t26;
  int64_t v0;
  int64_t v1;
  int64_t v2;
  int64_t v3;
  int64_t v4;
  int64_t v5;
  int64_t v6;
  float v7;
  uint32_t v8;
  v0 = p0;
  v1 = p1;
  v2 = p2;
  {
  t1 = ((int64_t)(v1));
  t2 = ((int64_t)(((int64_t)UINT64_C(32))));
  v3 = ((int64_t)(((int64_t)gg_div(t1, t2))));
  t3 = ((int64_t)(v1));
  t4 = ((int64_t)(((int64_t)UINT64_C(16))));
  v4 = ((int64_t)(((int64_t)gg_mod(t3, t4))));
  t5 = ((int64_t)(v1));
  t6 = ((int64_t)(((int64_t)UINT64_C(32))));
  t7 = ((int64_t)(((int64_t)gg_mod(t5, t6))));
  t8 = ((int64_t)(((int64_t)UINT64_C(16))));
  v5 = ((int64_t)(((int64_t)gg_div(t7, t8))));
  t9 = ((int64_t)(v0));
  t10 = ((int64_t)(v3));
  t11 = ((int64_t)(((int64_t)UINT64_C(18))));
  t12 = ((int64_t)(((int64_t)((uint64_t)(t10) * (uint64_t)(t11)))));
  v6 = ((int64_t)(((int64_t)((uint64_t)(t9) + (uint64_t)(t12)))));
  t13 = ((int64_t)(v6));
  t14 = ((int64_t)(((int64_t)UINT64_C(0))));
  t15 = dfa_ld_u16(t13, t14);
  t16 = ((uint32_t)(t15));
  t17 = dfa_half(t16);
  v7 = ((float)(t17));
  t18 = ((int64_t)(v6));
  t19 = ((int64_t)(((int64_t)UINT64_C(2))));
  t20 = ((int64_t)(v4));
  t21 = ((int64_t)(((int64_t)((uint64_t)(t19) + (uint64_t)(t20)))));
  t22 = dfa_ld_u32(t18, t21);
  v8 = ((uint32_t)(t22));
  t23 = ((uint32_t)(v8));
  t24 = ((int64_t)(v2));
  t25 = ((int64_t)(v5));
  t26 = ((float)(v7));
  t0 = gg_function_5(t23, t24, t25, t26, base + 0);
  goto gg_done;
  }
gg_done:;
  return t0;
}
