float __nv_roundf(float);

static inline float mvq_half(ushort bits) {
    float result;
    __asm__("cvt.f32.f16 %0, %1;" : "=f"(result) : "h"(bits));
    return result;
}

__kernel void bonsai_mv_pack_q8(__global const float *x, __global uchar *out, uint groups) {
    const uint thread = local_id(), lane = thread & 31;
    const uint group = work_group()*4 + thread/32;
    if (group >= groups) return;
    const float value = x[group*32 + lane];
    float peak = as_float(as_uint(value) & 0x7fffffff);
    for (uint bit = 16; bit; bit >>= 1) peak = flash_max(peak,flash_shuffle(peak,bit));
    float scale, normalized = 0.0f;
    __asm__("div.rn.f32 %0, %1, %2;" : "=f"(scale) : "f"(peak), "f"(127.0f));
    if (peak != 0.0f) {
        __asm__("div.rn.f32 %0, %1, %2;" : "=f"(normalized) : "f"(value), "f"(scale));
    }
    ushort encoded;
    __asm__("cvt.rn.f16.f32 %0, %1;" : "=h"(encoded) : "f"(scale));
    __global uchar *row = out + group*36;
    if (lane == 0) *(__global uint *)row = encoded;
    row[4+lane] = (uchar)(int)__nv_roundf(normalized);
}

static inline uint mvq_permute(uint a, uint b, uint order) {
    uint out;
    __asm__("prmt.b32 %0, %1, %2, %3;" : "=r"(out) : "r"(a), "r"(b), "r"(order));
    return out;
}

static inline int mvq_dot(__global const ushort *pair, __global const uint *a) {
    const uint map = 0x020100ff;
    int sum = 0;
    #pragma unroll
    for (uint i = 0; i < 4; ++i) {
        const uint code = pair[i];
        const uint even = mvq_permute(map,map,code & 0x3333);
        const uint odd = mvq_permute(map,map,(code>>2) & 0x3333);
        const uint w0 = mvq_permute(even,odd,0x5140);
        const uint w1 = mvq_permute(even,odd,0x7362);
        __asm__("dp4a.s32.s32 %0, %1, %2, %0;" : "+r"(sum) : "r"(w0), "r"(a[2*i]));
        __asm__("dp4a.s32.s32 %0, %1, %2, %0;" : "+r"(sum) : "r"(w1), "r"(a[2*i+1]));
    }
    return sum;
}

static inline __attribute__((always_inline)) void mvq_rows(
    __global const uchar *weights, __global const uchar *input,
    __global float *output, uint width, uint rows, uint grouping) {
    const uint thread=local_id(), lane=thread&31;
    const uint start=(work_group()*4+thread/32)*grouping;
    if (start>=rows) return;
    const uint blocks=width/128;
    const uint groups=width/32;
    float value[2] = {0.0f,0.0f};
    for (uint i=lane; i<groups; i+=32) {
        const __global uchar *a=input+i*36;
        const float dx=mvq_half(*(__global const ushort *)a);
        #pragma unroll
        for (uint r=0; r<grouping; ++r) {
            const uint row=start+r < rows ? start+r : rows-1;
            const __global uchar *w=weights+((size_t)row*blocks+i/4)*34;
            const int dot=mvq_dot((__global const ushort *)(w+2+(i%4)*8),(__global const uint *)(a+4));
            const float dw=mvq_half(*(__global const ushort *)w);
            value[r]+=(dw*dx)*(float)dot;
        }
    }
    #pragma unroll
    for (uint r=0; r<grouping; ++r) {
        for (uint bit=16; bit; bit>>=1) value[r]+=flash_shuffle(value[r],bit);
        if (lane==0 && start+r<rows) output[start+r]=value[r];
    }
}
#define MVQ(N) \
__kernel void bonsai_pq2_mvq_q8_##N(__global const uchar *weights, __global const uchar *input, \
    __global float *output, uint width, uint rows) { \
    mvq_rows(weights,input,output,width,rows,N); \
}
MVQ(1)
MVQ(2)
