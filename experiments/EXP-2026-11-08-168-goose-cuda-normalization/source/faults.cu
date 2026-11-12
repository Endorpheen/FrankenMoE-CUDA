// ---------------------------------------------------------------------------
// EXP168 subprocess-isolated malformed-input rejection harness.
//
// The trusted parent launcher runs each case in its own process:
//     ./faults <case-index>
// Exit codes:
//     0  - case 0 control: a valid native norm completed without abort
//     2  - usage error or CUDA API failure
//    81  - the input was REJECTED through ggml_abort (expected for 1..15);
//          ggml_set_abort_callback converts every rejection path (native
//          GGML_ASSERT and the Goose fail() bridge) into this exit code
//    85  - case 0 control aborted unexpectedly (defect)
//    86  - a malformed input was ACCEPTED (defect: rejection missing)
//
// Cases 1..15 invoke only the candidate implementation; the parent audits
// separately that every rejection happens before any stream or kernel
// effect. No timing. No files written.
// ---------------------------------------------------------------------------

#include "norm.cuh"
#include "fixture-api.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static int abort_exit_code = 81;

static void reject(const char *) {
    _Exit(abort_exit_code);
}

static void ck(cudaError_t e) {
    if (e != cudaSuccess) {
        fprintf(stderr, "CUDA error: %s\n", cudaGetErrorString(e));
        std::exit(2);
    }
}

static void set_f32(ggml_tensor & t, int64_t n0, int64_t n1, int64_t n2, int64_t n3, void * data) {
    std::memset(&t, 0, sizeof t);
    t.type = GGML_TYPE_F32;
    t.ne[0] = n0; t.ne[1] = n1; t.ne[2] = n2; t.ne[3] = n3;
    t.nb[0] = 4;
    t.nb[1] = (size_t)n0 * 4;
    t.nb[2] = t.nb[1] * (size_t)n1;
    t.nb[3] = t.nb[2] * (size_t)n2;
    t.data = data;
}

static void set_eps(ggml_tensor & t, float eps) {
    std::memcpy(&t.op_params[0], &eps, 4);
}

// group_norm keeps eps at op_params slot 1 (bytes 4..7)
static void set_eps1(ggml_tensor & t, float eps) {
    std::memcpy(&t.op_params[1], &eps, 4);
}

struct Mem {
    void * p = nullptr;
    explicit Mem(size_t bytes) { ck(cudaMalloc(&p, bytes)); ck(cudaMemset(p, 0, bytes)); }
    ~Mem() { ck(cudaFree(p)); }
    Mem(const Mem &) = delete;
    Mem & operator=(const Mem &) = delete;
};

int main(int argc, char ** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: faults <case-index 0..15>\n");
        return 2;
    }
    const int idx = atoi(argv[1]);
    ggml_backend_cuda_context ctx(0);
    ck(cudaSetDevice(0));

    // case 0: control - valid input through the native oracle must not abort
    if (idx == 0) {
        abort_exit_code = 85;
        ggml_set_abort_callback(reject);
        Mem xd(256), od(256);
        ggml_tensor x, d;
        set_f32(x, 64, 1, 1, 1, xd.p);
        set_f32(d, 64, 1, 1, 1, od.p);
        d.src[0] = &x;
        set_eps(d, 1e-5f);
        fg168_native_ggml_cuda_op_norm(ctx, &d);
        ck(cudaStreamSynchronize(ctx.stream()));
        return 0;
    }

    abort_exit_code = 81;
    ggml_set_abort_callback(reject);

    Mem scratch(16384);
    Mem out(4096);
    char * base = (char *)scratch.p;
    void * p1 = base;
    void * p2 = base + 4096;
    void * p3 = base + 8192;
    void * p4 = base + 12288;

    ggml_tensor x{}, d{}, x2{}, x3{}, g{}, rms{}, sec{}, third{};

    switch (idx) {
        case 1:  // negative epsilon, mode 0
            set_f32(x, 64, 1, 1, 1, p1);
            set_f32(d, 64, 1, 1, 1, out.p);
            d.src[0] = &x;
            set_eps(d, -1.0f);
            ggml_cuda_op_norm(ctx, &d);
            break;
        case 2:  // source type is not F32, mode 0
            set_f32(x, 64, 1, 1, 1, p1);
            x.type = GGML_TYPE_F16;
            x.nb[0] = 2;
            x.nb[1] = 128;
            set_f32(d, 64, 1, 1, 1, out.p);
            d.src[0] = &x;
            set_eps(d, 1e-5f);
            ggml_cuda_op_norm(ctx, &d);
            break;
        case 3:  // nb0 != 4 with F32 type, mode 0
            set_f32(x, 64, 1, 1, 1, p1);
            x.nb[0] = 8;
            x.nb[1] = 512;
            set_f32(d, 64, 1, 1, 1, out.p);
            d.src[0] = &x;
            set_eps(d, 1e-5f);
            ggml_cuda_op_norm(ctx, &d);
            break;
        case 4:  // NULL source data, mode 2
            set_f32(x, 64, 1, 1, 1, nullptr);
            set_f32(d, 64, 1, 1, 1, out.p);
            d.src[0] = &x;
            set_eps(d, 1e-5f);
            ggml_cuda_op_rms_norm(ctx, &d);
            break;
        case 5:  // source/destination shape mismatch, mode 2
            set_f32(x, 32, 1, 1, 1, p2);
            set_f32(d, 64, 1, 1, 1, out.p);
            d.src[0] = &x;
            set_eps(d, 1e-5f);
            ggml_cuda_op_rms_norm(ctx, &d);
            break;
        case 6:  // groups == 0, mode 1
            set_f32(x, 8, 2, 2, 1, p1);
            set_f32(d, 8, 2, 2, 1, out.p);
            d.src[0] = &x;
            d.op_params[0] = 0;
            set_eps1(d, 1e-5f);
            ggml_cuda_op_group_norm(ctx, &d);
            break;
        case 7:  // non-contiguous group_norm source, mode 1
            set_f32(x, 8, 2, 2, 1, p1);
            x.nb[1] = (8 + 4) * 4;
            x.nb[2] = x.nb[1] * 2;
            x.nb[3] = x.nb[2] * 2;
            set_f32(d, 8, 2, 2, 1, out.p);
            d.src[0] = &x;
            d.op_params[0] = 2;
            set_eps1(d, 1e-5f);
            ggml_cuda_op_group_norm(ctx, &d);
            break;
        case 8:  // fused mul node does not reference the rms node, mode 3
            set_f32(x, 64, 1, 1, 1, p1);
            set_f32(x2, 64, 1, 1, 1, p2);
            set_f32(x3, 64, 1, 1, 1, p3);
            set_f32(rms, 64, 1, 1, 1, nullptr);
            rms.src[0] = &x;
            set_eps(rms, 1e-5f);
            set_f32(sec, 64, 1, 1, 1, out.p);
            sec.src[0] = &x2;
            sec.src[1] = &x3;
            ggml_cuda_op_rms_norm_fused(ctx, &rms, &sec);
            break;
        case 9:  // scale fusion with non-zero bias, mode 4
            set_f32(x, 64, 1, 1, 1, p1);
            set_f32(rms, 64, 1, 1, 1, nullptr);
            rms.src[0] = &x;
            set_eps(rms, 1e-5f);
            set_f32(sec, 64, 1, 1, 1, out.p);
            sec.src[0] = &rms;
            {
                const float s = 2.0f, bias = 0.5f;
                std::memcpy(&sec.op_params[0], &s, 4);
                std::memcpy(&sec.op_params[1], &bias, 4);
            }
            ggml_cuda_op_rms_norm_scale_fused(ctx, &rms, &sec);
            break;
        case 10:  // scale node src[0] is not the rms node, mode 4
            set_f32(x, 64, 1, 1, 1, p1);
            set_f32(x2, 64, 1, 1, 1, p2);
            set_f32(rms, 64, 1, 1, 1, nullptr);
            rms.src[0] = &x;
            set_eps(rms, 1e-5f);
            set_f32(sec, 64, 1, 1, 1, out.p);
            sec.src[0] = &x2;
            {
                const float s = 2.0f, zero = 0.0f;
                std::memcpy(&sec.op_params[0], &s, 4);
                std::memcpy(&sec.op_params[1], &zero, 4);
            }
            ggml_cuda_op_rms_norm_scale_fused(ctx, &rms, &sec);
            break;
        case 11:  // non-contiguous grad, mode 6
            set_f32(x, 64, 2, 1, 1, p1);
            set_f32(g, 64, 2, 1, 1, p2);
            g.nb[1] = (64 + 4) * 4;
            set_f32(d, 64, 2, 1, 1, out.p);
            d.src[0] = &g;
            d.src[1] = &x;
            set_eps(d, 1e-5f);
            ggml_cuda_op_rms_norm_back(ctx, &d);
            break;
        case 12:  // grad shape != forward-source shape, mode 6
            set_f32(x, 64, 2, 1, 1, p1);
            set_f32(g, 64, 1, 1, 1, p2);
            set_f32(d, 64, 2, 1, 1, out.p);
            d.src[0] = &g;
            d.src[1] = &x;
            set_eps(d, 1e-5f);
            ggml_cuda_op_rms_norm_back(ctx, &d);
            break;
        case 13:  // fused add node does not reference the mul node, mode 5
            set_f32(x, 64, 1, 1, 1, p1);
            set_f32(x2, 64, 1, 1, 1, p2);
            set_f32(x3, 64, 1, 1, 1, p3);
            set_f32(rms, 64, 1, 1, 1, nullptr);
            rms.src[0] = &x;
            set_eps(rms, 1e-5f);
            set_f32(sec, 64, 1, 1, 1, nullptr);
            sec.src[0] = &rms;
            sec.src[1] = &x2;
            set_f32(third, 64, 1, 1, 1, out.p);
            third.src[0] = &x3;
            third.src[1] = &x2;
            ggml_cuda_op_rms_norm_fused_add(ctx, &rms, &sec, &third);
            break;
        case 14:  // negative epsilon on the fused path, mode 3
            set_f32(x, 64, 1, 1, 1, p1);
            set_f32(x2, 64, 1, 1, 1, p2);
            set_f32(rms, 64, 1, 1, 1, nullptr);
            rms.src[0] = &x;
            set_eps(rms, -1.0f);
            set_f32(sec, 64, 1, 1, 1, out.p);
            sec.src[0] = &rms;
            sec.src[1] = &x2;
            ggml_cuda_op_rms_norm_fused(ctx, &rms, &sec);
            break;
        case 15:  // rms node without a source, mode 3
            set_f32(x2, 64, 1, 1, 1, p2);
            set_f32(rms, 64, 1, 1, 1, nullptr);
            rms.src[0] = nullptr;
            set_eps(rms, 1e-5f);
            set_f32(sec, 64, 1, 1, 1, out.p);
            sec.src[0] = &rms;
            sec.src[1] = &x2;
            ggml_cuda_op_rms_norm_fused(ctx, &rms, &sec);
            break;
        default:
            fprintf(stderr, "unknown case %d\n", idx);
            return 2;
    }

    // Reaching this point means the malformed input was accepted.
    _Exit(86);
}
