static __device__ __forceinline__ float goose_silu_gate_g(float gate_g, float up_g) {
    float t0;
    float t1 = gate_g;
    float t2 = 1.0f;
    float t3 = (-gate_g);
    float t4 = expf(t3);
    float t5 = ((float)(t1) / (float)(((float)(t2) + (float)(t4))));
    t0 = ((float)(t5) * (float)(up_g));
    return t0;
}
