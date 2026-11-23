// Fused route prediction for expert prefetch: 256 blocks compute one logit each
// (W row . x, F32); the last block to finish ranks the 256 logits and writes the
// K largest indices (ties to the lower index) and then `seq` into host-mapped
// memory. The output only chooses which expert weights are copied early.
#define G __attribute__((global))
#define S __attribute__((shared))
extern "C" G void goose_pred_route(const float * __restrict__ w, const float * __restrict__ x,
        float * logits, unsigned * counter, volatile int * host, int width, int k, int seq) {
    S float part[8];
    S float v[256];
    S int last;
    const int row = __nvvm_read_ptx_sreg_ctaid_x(), t = __nvvm_read_ptx_sreg_tid_x();
    const float * r = w + (long) row * width;
    float s = 0.f;
    for (int i = t * 4; i < width; i += 256 * 4)
        s += r[i] * x[i] + r[i + 1] * x[i + 1] + r[i + 2] * x[i + 2] + r[i + 3] * x[i + 3];
    for (int o = 16; o; o >>= 1) s += __nvvm_shfl_sync_bfly_f32(0xffffffffu, s, o, 31);
    if ((t & 31) == 0) part[t >> 5] = s;
    __nvvm_bar_sync(0);
    if (t == 0) {
        float a = 0.f;
        for (int i = 0; i < 8; ++i) a += part[i];
        ((volatile float *) logits)[row] = a;
        __nvvm_membar_gl();
        last = __nvvm_atom_add_gen_i((int *) counter, 1) == 255;
    }
    __nvvm_bar_sync(0);
    if (!last) return;
    __nvvm_membar_gl();
    v[t] = ((volatile float *) logits)[t];
    __nvvm_bar_sync(0);
    const float me = v[t];
    int rank = 0;
    for (int j = 0; j < 256; ++j) rank += (v[j] > me) | ((v[j] == me) & (j < t));
    if (rank < k) host[1 + rank] = t;
    __nvvm_membar_sys();
    __nvvm_bar_sync(0);
    if (t == 0) {
        *counter = 0;
        host[0] = seq;
        __nvvm_membar_sys();
    }
}
