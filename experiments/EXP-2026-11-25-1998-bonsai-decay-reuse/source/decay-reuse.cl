// Each warp preserves its lane-local accumulation and reduction order.
// The same load of alpha/beta weights feeds several independent tokens.
#define BONSAI_DECAY_REUSE(NAME, TILE) \
__kernel void NAME(__global const float *x,__global const float *alpha, \
                   __global const float *beta,__global const float *dt, \
                   __global const float *a,__global float *gate, \
                   __global float *mix,uint tokens) { \
    const uint thread=local_id(), lane=thread%32; \
    const uint work=work_group()*4+thread/32, head=work%48; \
    const uint first=(work/48)*(TILE); \
    if (first>=tokens) return; \
    const uint active=flash_min(tokens-first,(TILE)); \
    float al[TILE],be[TILE]; \
    for (uint t=0;t<(TILE);++t) {al[t]=0.0f;be[t]=0.0f;} \
    alpha+=head*5120; beta+=head*5120; \
    x+=(ulong)first*5120; \
    for (uint i=lane;i<5120;i+=32) { \
        const float wa=alpha[i], wb=beta[i]; \
        _Pragma("unroll") \
        for (uint t=0;t<(TILE);++t) { \
            if (t<active) { \
                const float value=x[t*5120+i]; \
                al[t]+=value*wa; be[t]+=value*wb; \
            } \
        } \
    } \
    _Pragma("unroll") \
    for (uint t=0;t<(TILE);++t) { \
        if (t<active) { \
            const float left=warp_sum(al[t]), right=warp_sum(be[t]); \
            if (lane==0) { \
                const float p=left+dt[head]; \
                const uint at=(first+t)*48+head; \
                gate[at]=a[head]*(p>0.0f ? p+__nv_log1pf(__nv_expf(-p)) : __nv_log1pf(__nv_expf(p))); \
                mix[at]=1.0f/(1.0f+__nv_expf(-right)); \
            } \
        } \
    } \
}
BONSAI_DECAY_REUSE(bonsai_decay_warp_pack,1)
BONSAI_DECAY_REUSE(bonsai_decay_reuse2,2)
BONSAI_DECAY_REUSE(bonsai_decay_reuse4,4)
BONSAI_DECAY_REUSE(bonsai_decay_reuse8,8)
