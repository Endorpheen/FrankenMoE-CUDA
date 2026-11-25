__kernel void bonsai_split_q_gate(__global const float *x, __global float *q,
                                  __global float *gate) {
    const uint col = work_group() * 256 + local_id();
    const uint base = work_group() * 512 + local_id();
    q[col] = x[base];
    gate[col] = x[base + 256];
}

__kernel void bonsai_attention_gate(__global const float *x, __global const float *gate,
                                   __global float *out) {
    const uint col = work_group() * 256 + local_id();
    out[col] = x[col] / (1.0f + __nv_expf(-gate[col]));
}
