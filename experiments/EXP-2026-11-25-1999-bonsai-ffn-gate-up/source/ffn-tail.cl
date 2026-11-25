// Exact SwiGLU, signed FWHT and FP16 pack. No global intermediate
// between these three operations. The following MMQ reads the packed output.
__kernel void bonsai_ffn_tail(__global const float *gate,__global const float *up,
                              __global const int *signs,__global uint *packed,uint width) {
    __local float values[1024];
    const ulong token=(ulong)flash_grid_y()*width;
    const uint base=work_group()*1024,lane=local_id();
    for (uint i=lane;i<1024;i+=256) {
        const float result=bonsai_ffn_swiglu(gate[token+base+i],up[token+base+i]);
        values[i]=result*(float)signs[base+i];
    }
    sync_threads();
    for (uint span=1;span<1024;span*=2) {
        for (uint i=lane;i<512;i+=256) {
            const uint a=(i/span)*(2*span)+i%span;
            const float x=values[a],y=values[a+span];
            values[a]=x+y;
            values[a+span]=x-y;
        }
        sync_threads();
    }
    for (uint i=lane;i<512;i+=256) {
        packed[(token+base)/2+i]=bonsai_half_pair(values[2*i]*0.03125f,values[2*i+1]*0.03125f);
    }
}
