#pragma once
#include <stdint.h>
#include <string.h>
static __device__ __forceinline__ double gg_double_bits(uint64_t bits) { double d; memcpy(&d, &bits, sizeof d); return d; }
static __device__ __forceinline__ float gg_function_1(float p0, int64_t base) {
  float t0;
  float t1;
  float t2;
  float t3;
  float t4;
  float t5;
  float t6;
  float t7;
  float v0;
  float v1;
  v0 = p0;
  {
  v1 = ((float)(gg_double_bits(UINT64_C(4607182418800017408))));
  t1 = ((float)(v0));
  t2 = ((float)(v1));
  t3 = v0;
  t4 = ((float)((-(t3))));
  t5 = expf(t4);
  t6 = ((float)(t5));
  t7 = ((float)(((t2) + (t6))));
  t0 = ((float)(((t1) / (t7))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __device__ __forceinline__ float gg_function_3(float p0, float p1, float p2, int64_t base) {
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
  t1 = ((float)(v1));
  t2 = ((float)(v0));
  t3 = ((float)(((t1) * (t2))));
  t4 = ((float)(v2));
  t0 = ((float)(((t3) + (t4))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
static __device__ __forceinline__ float gg_function_4(float p0, int64_t base) {
  float t0;
  float t1;
  float t2;
  float v0;
  v0 = p0;
  {
  t1 = ((float)(v0));
  t2 = ((float)(v0));
  t0 = ((float)(((t1) * (t2))));
  goto gg_done;
  }
gg_done:;
  return t0;
}
