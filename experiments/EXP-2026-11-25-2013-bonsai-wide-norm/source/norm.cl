static inline __attribute__((always_inline)) void wide_norm(
    __global const float *x, __global const float *weight, __global float *out,
    float eps, __local float *cache, __local float *divisor, uint threads, uint stage) {
    const uint t = local_id();
    const size_t base = (size_t)work_group() * 5120;
    if (stage) {
        for (uint i=t; i<5120; i+=threads) cache[i]=x[base+i];
        sync_threads();
    }
    if (t<32) {
        float sum=0.0f;
        #pragma unroll 4
        for (uint i=t; i<5120; i+=32) {
            const float value = stage ? cache[i] : x[base+i];
            sum += value * value;
        }
        sum=warp_sum(sum);
        if (t==0) *divisor=__nv_sqrtf(sum / 5120.0f + eps);
    }
    sync_threads();
    const float d=*divisor;
    for (uint i=t; i<5120; i+=threads) {
        const float value=stage ? cache[i] : x[base+i];
        out[base+i]=(value / d) * weight[i];
    }
}

#define WIDE(T,S,C) \
__kernel void bonsai_norm_wide_##T##_##S(__global const float *x, __global const float *weight, \
    __global float *out, float eps) { \
    __local float cache[C], divisor; \
    wide_norm(x,weight,out,eps,cache,&divisor,T,S); \
}
WIDE(128,0,1)
WIDE(256,0,1)
WIDE(128,1,5120)
WIDE(256,1,5120)
WIDE(512,1,5120)
