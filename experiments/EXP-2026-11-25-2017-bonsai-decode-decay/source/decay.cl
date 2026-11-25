static inline void decay_result(uint token, uint head, int gate_row, float dot,
    __global const float *dt, __global const float *a, __global float *gate, __global float *mix) {
    if (gate_row) {
        const float p = dot + dt[head];
        gate[token*48+head] = a[head] * (p > 0.0f ?
            p + __nv_log1pf(__nv_expf(-p)) : __nv_log1pf(__nv_expf(p)));
    } else {
        mix[token*48+head] = 1.0f / (1.0f + __nv_expf(-dot));
    }
}

static inline __attribute__((always_inline)) void staged_decay(
    __global const float *x, __global const float *alpha, __global const float *beta,
    __global const float *dt, __global const float *a, __global float *gate, __global float *mix,
    __local float *products, uint threads, int split) {
    const uint t = local_id();
    const uint head = work_group() % 48;
    const uint token = work_group() / 48;
    const uint channel = split ? flash_grid_y() : 0;
    x += token * 5120;
    alpha += head * 5120;
    beta += head * 5120;
    for (uint i = t; i < 5120; i += threads) {
        const float value = x[i];
        if (split) {
            products[i] = value * (channel == 0 ? alpha[i] : beta[i]);
        } else {
            products[i] = value * alpha[i];
            products[5120+i] = value * beta[i];
        }
    }
    sync_threads();
    if (t >= (split ? 32 : 64)) return;
    const uint lane = t & 31;
    const uint offset = split ? 0 : (t/32) * 5120;
    float sum = 0.0f;
    #pragma unroll 8
    for (uint i = lane; i < 5120; i += 32) sum += products[offset+i];
    sum = warp_sum(sum);
    if (lane == 0) decay_result(token,head,(split ? channel : t/32)==0,sum,dt,a,gate,mix);
}

#define STAGED(T,S,N) \
__kernel void bonsai_decay_stage_##T##_##S(__global const float *x, __global const float *alpha, \
    __global const float *beta, __global const float *dt, __global const float *a, \
    __global float *gate, __global float *mix) { \
    __local float products[N]; \
    staged_decay(x,alpha,beta,dt,a,gate,mix,products,T,S); \
}
STAGED(128,0,10240)
STAGED(256,0,10240)
STAGED(128,1,5120)
STAGED(256,1,5120)
