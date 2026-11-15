// The recurrent layer's full sequence, against ggml's constructors.
//
// Rewritten after EXP-695. Every node goes through the same `call()` helper the
// working attention gate uses, with named parameters - no hand-assembled argument
// arrays. That was the failure in the previous attempt: the packed layout is
// [mode, ctx, a, b, d, op, inplace, a7..a14], and mode 11 needs GGML_OP_MUL_MAT in the
// op slot while mode 4 needs 0 in the inplace slot before eps.
//
// The sequence, restated from qwen35moe.cpp:379-505 before anything is built:
//
//   beta  = sigmoid(reshape(w_beta * cur, [1, n_v, T, B]))
//   alpha = reshape(w_alpha * cur, [n_v, T, B])
//   gate  = reshape(softplus(alpha + ssm_dt) * ssm_a, [1, n_v, T, B])
//   conv  = silu(ssm_conv(conv_input, kernel))
//   q/k/v = views over conv at the qkv stride; l2_norm(q), l2_norm(k); repeat to n_v
//   core  = gated_delta_net(q, k, v, gate, beta, state, K)  -> [S_v*H, T + K*S_v]
//   out   = view_4d(core, head_v, n_v, T, B)                <- output rows only
//   norm  = rms_norm(out) * silu(reshape(z, [head_v, n_v, T, B]))
//   cur'  = ssm_out * reshape(norm, [head_v*n_v, T, B])
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gguf.h"
#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
#include "main.h"

extern void fg_goose_host_ensure_init(void);
extern int64_t gdn_qkv_view(int64_t c, int64_t mixed, int64_t which, int64_t head_k,
                            int64_t n_k, int64_t head_h, int64_t n_v, int64_t n_tokens,
                            int64_t n_seqs, int64_t elem);
extern int64_t gdn_l2norm(int64_t c, int64_t a, int64_t eps_bits);
extern int64_t gdn_repeat_to_v(int64_t c, int64_t a, int64_t head_dim, int64_t n_k_heads,
                               int64_t n_v_heads, int64_t n_tokens, int64_t n_seqs);

static int32_t fb_of(float f) { int32_t b; memcpy(&b, &f, sizeof b); return b; }

static struct ggml_tensor * call(int64_t mode, struct ggml_context * gc,
                                 struct ggml_tensor * a, struct ggml_tensor * b,
                                 struct ggml_tensor * d, int64_t op, int64_t inplace,
                                 int64_t a7, int64_t a8, int64_t a9, int64_t a10,
                                 int64_t a11, int64_t a12, int64_t a13, int64_t a14) {
    uint64_t args[16] = {
        (uint64_t) mode, (uint64_t) (uintptr_t) gc,
        (uint64_t) (uintptr_t) a, (uint64_t) (uintptr_t) b, (uint64_t) (uintptr_t) d,
        (uint64_t) op, (uint64_t) inplace,
        (uint64_t) a7, (uint64_t) a8, (uint64_t) a9, (uint64_t) a10, (uint64_t) a11,
        (uint64_t) a12, (uint64_t) a13, (uint64_t) a14, 0
    };
    return (struct ggml_tensor *) (uintptr_t) gb_common((int64_t) (uintptr_t) args);
}

int main(int argc, char ** argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc < 2) {
        fprintf(stderr, "usage: %s <apex.gguf>\n", argv[0]);
        return 2;
    }
    fg_goose_host_ensure_init();

    struct ggml_context * meta = NULL;
    struct gguf_init_params mp = { /*.no_alloc =*/ true, /*.ctx =*/ &meta };
    struct gguf_context * gg = gguf_init_from_file(argv[1], mp);
    if (gg == NULL) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }

    const int64_t tconv = gguf_find_tensor(gg, "blk.0.ssm_conv1d.weight");
    const int64_t tnorm = gguf_find_tensor(gg, "blk.0.ssm_norm.weight");
    const int64_t tqkv  = gguf_find_tensor(gg, "blk.0.attn_qkv.weight");
    const int64_t tout  = gguf_find_tensor(gg, "blk.0.ssm_out.weight");
    const int64_t ta    = gguf_find_tensor(gg, "blk.0.ssm_a");
    if (tconv < 0 || tnorm < 0 || tqkv < 0 || tout < 0 || ta < 0) {
        fprintf(stderr, "blk.0 ssm tensors not found\n");
        return 1;
    }
    const int64_t kernel   = gguf_get_tensor_ne(gg, (int) tconv)[0];   // 4
    const int64_t channels = gguf_get_tensor_ne(gg, (int) tconv)[1];   // 8192
    const int64_t head_v   = gguf_get_tensor_ne(gg, (int) tnorm)[0];   // 128
    const int64_t qkv_w    = gguf_get_tensor_ne(gg, (int) tqkv)[1];    // 8192
    const int64_t vdim     = gguf_get_tensor_ne(gg, (int) tout)[0];    // 4096
    const int64_t n_v      = vdim / head_v;                            // 32
    const int64_t dt_rank  = gguf_get_tensor_ne(gg, (int) ta)[0];      // 32
    const int64_t n_k      = n_v / 2;                                  // 16
    const int64_t head_k   = (qkv_w - vdim) / 2 / n_k;                 // 128
    const int64_t n_embd   = 2048;
    const int64_t T = 1, B = 1;
    const size_t elem = sizeof(float);
    const float eps = 1e-6f;

    struct ggml_init_params gp = { 2048u * 1024u * 1024u, NULL, /*.no_alloc =*/ false };
    struct ggml_context * gc = ggml_init(gp);
    if (gc == NULL) { printf("FG_GDN_LAYER init failed\n"); return 2; }

    struct ggml_tensor * cur    = ggml_new_tensor_2d(gc, GGML_TYPE_F32, n_embd, 1);
    struct ggml_tensor * wz     = ggml_new_tensor_2d(gc, GGML_TYPE_F32, n_embd, vdim);
    struct ggml_tensor * wout   = ggml_new_tensor_2d(gc, GGML_TYPE_F32, vdim, n_embd);
    struct ggml_tensor * walpha = ggml_new_tensor_2d(gc, GGML_TYPE_F32, n_embd, dt_rank);
    struct ggml_tensor * wbeta  = ggml_new_tensor_2d(gc, GGML_TYPE_F32, n_embd, dt_rank);
    struct ggml_tensor * sdt    = ggml_new_tensor_1d(gc, GGML_TYPE_F32, n_v);
    struct ggml_tensor * sa     = ggml_new_tensor_1d(gc, GGML_TYPE_F32, n_v);
    struct ggml_tensor * cin    = ggml_new_tensor_2d(gc, GGML_TYPE_F32, kernel, channels);
    struct ggml_tensor * ck     = ggml_new_tensor_2d(gc, GGML_TYPE_F32, kernel, channels);
    struct ggml_tensor * st     = ggml_new_tensor_4d(gc, GGML_TYPE_F32, head_v, head_v, n_v, 1);
    {
        struct { struct ggml_tensor * t; int seed; } ws[] = {
            { cur, 13 }, { wz, 7 }, { wout, 11 }, { walpha, 3 }, { wbeta, 5 },
            { cin, 11 }, { ck, 7 }, { st, 9 },
        };
        for (size_t i = 0; i < sizeof ws / sizeof ws[0]; ++i) {
            float * d = (float *) ws[i].t->data;
            const int64_t n = ggml_nelements(ws[i].t);
            for (int64_t j = 0; j < n; ++j)
                d[j] = (float) ((j % ws[i].seed) - ws[i].seed / 2) / 64.0f;
        }
        float * dd = (float *) sdt->data;
        for (int64_t i = 0; i < n_v; ++i) dd[i] = (float) ((i % 5) - 2) / 32.0f;
        float * da = (float *) sa->data;
        for (int64_t i = 0; i < n_v; ++i) da[i] = -0.5f - (float) (i % 7) / 16.0f;
    }

    // ---- beta and gate, Goose arm ----
    struct ggml_tensor * arawA = call(11, gc, walpha, cur, NULL, GGML_OP_MUL_MAT, 0,
                                      0,0,0,0,0,0,0,0);
    struct ggml_tensor * brawA = call(11, gc, wbeta, cur, NULL, GGML_OP_MUL_MAT, 0,
                                      0,0,0,0,0,0,0,0);
    struct ggml_tensor * a3A = ggml_reshape_3d(gc, arawA, n_v, T, B);
    struct ggml_tensor * biasedA = call(0, gc, a3A, sdt, NULL, GGML_OP_ADD, 0, 0,0,0,0,0,0,0,0);
    struct ggml_tensor * spA = call(3, gc, biasedA, NULL, NULL, GGML_UNARY_OP_SOFTPLUS, 0,
                                    0,0,0,0,0,0,0,0);
    struct ggml_tensor * gm = call(0, gc, spA, sa, NULL, GGML_OP_MUL, 0, 0,0,0,0,0,0,0,0);
    struct ggml_tensor * gateA = ggml_reshape_4d(gc, gm, 1, n_v, T, B);
    struct ggml_tensor * betaA = call(3, gc,
        ggml_reshape_4d(gc, brawA, 1, n_v, T, B), NULL, NULL, GGML_UNARY_OP_SIGMOID, 0,
        0,0,0,0,0,0,0,0);

    // ---- beta and gate, native arm ----
    struct ggml_tensor * arawB = ggml_mul_mat(gc, walpha, cur);
    struct ggml_tensor * brawB = ggml_mul_mat(gc, wbeta, cur);
    struct ggml_tensor * a3B = ggml_reshape_3d(gc, arawB, n_v, T, B);
    struct ggml_tensor * biasedB = ggml_add(gc, a3B, sdt);
    struct ggml_tensor * spB = ggml_softplus(gc, biasedB);
    struct ggml_tensor * gmB = ggml_mul(gc, spB, sa);
    struct ggml_tensor * gateB = ggml_reshape_4d(gc, gmB, 1, n_v, T, B);
    struct ggml_tensor * betaB = ggml_sigmoid(gc, ggml_reshape_4d(gc, brawB, 1, n_v, T, B));

    // ---- conv chain ----
    struct ggml_tensor * convA = call(27, gc, cin, ck, NULL, 0, 0, 0,0,0,0,0,0,0,0);
    struct ggml_tensor * siluA = call(3, gc, convA, NULL, NULL, GGML_UNARY_OP_SILU, 0,
                                      0,0,0,0,0,0,0,0);
    struct ggml_tensor * convB = ggml_ssm_conv(gc, cin, ck);
    struct ggml_tensor * siluB = ggml_silu(gc, convB);

    struct ggml_tensor * qvA = (struct ggml_tensor *) (uintptr_t) gdn_qkv_view(
        (int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) siluA, 0, head_k, n_k,
        head_v, n_v, T, B, (int64_t) elem);
    struct ggml_tensor * kvA = (struct ggml_tensor *) (uintptr_t) gdn_qkv_view(
        (int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) siluA, 1, head_k, n_k,
        head_v, n_v, T, B, (int64_t) elem);
    struct ggml_tensor * vvA = (struct ggml_tensor *) (uintptr_t) gdn_qkv_view(
        (int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) siluA, 2, head_k, n_k,
        head_v, n_v, T, B, (int64_t) elem);

    const int64_t qnA = gdn_l2norm((int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) qvA,
                                   (int64_t) fb_of(eps));
    const int64_t knA = gdn_l2norm((int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) kvA,
                                   (int64_t) fb_of(eps));
    const int64_t qrA = gdn_repeat_to_v((int64_t) (uintptr_t) gc, qnA, head_k, n_k, n_v, T, B);
    const int64_t krA = gdn_repeat_to_v((int64_t) (uintptr_t) gc, knA, head_k, n_k, n_v, T, B);
    if (!convA || !siluA || !qvA || !kvA || !vvA || !qnA || !knA || !qrA || !krA) {
        printf("FG_GDN_LAYER a Goose node returned null\n");
        return 1;
    }

    // ---- gated_delta_net ----
    struct ggml_tensor * gdnA = call(28, gc, (struct ggml_tensor *) (uintptr_t) qrA,
                                     (struct ggml_tensor *) (uintptr_t) krA, vvA, 0, 0,
                                     (int64_t) (uintptr_t) gateA, (int64_t) (uintptr_t) betaA,
                                     (int64_t) (uintptr_t) st, 1, 0,0,0,0);
    if (!gdnA) { printf("FG_GDN_LAYER gated_delta_net failed\n"); return 1; }

    const int64_t nb1_qkv = elem * (2 * head_k * n_k + head_v * n_v);
    struct ggml_tensor * qvB = ggml_view_4d(gc, siluB, head_k, n_k, T, B,
        elem * head_k, nb1_qkv, nb1_qkv * T, 0);
    struct ggml_tensor * kvB = ggml_view_4d(gc, siluB, head_k, n_k, T, B,
        elem * head_k, nb1_qkv, nb1_qkv * T, elem * head_k * n_k);
    struct ggml_tensor * vvB = ggml_view_4d(gc, siluB, head_v, n_v, T, B,
        elem * head_v, nb1_qkv, nb1_qkv * T, elem * 2 * head_k * n_k);
    struct ggml_tensor * qnB = ggml_l2_norm(gc, qvB, eps);
    struct ggml_tensor * knB = ggml_l2_norm(gc, kvB, eps);
    struct ggml_tensor * qrB = ggml_repeat_4d(gc, qnB, head_k, n_v, T, B);
    struct ggml_tensor * krB = ggml_repeat_4d(gc, knB, head_k, n_v, T, B);
    struct ggml_tensor * gdnB = ggml_gated_delta_net(gc, qrB, krB, vvB, gateB, betaB, st, 1);

    // ---- the node returns output rows then the next state ----
    const int64_t rows = T * B;
    struct ggml_tensor * coreA = ggml_view_4d(gc, gdnA, head_v, n_v, T, B,
        elem * head_v, elem * head_v * n_v, elem * head_v * n_v * rows, 0);
    struct ggml_tensor * coreB = ggml_view_4d(gc, gdnB, head_v, n_v, T, B,
        elem * head_v, elem * head_v * n_v, elem * head_v * n_v * rows, 0);

    // ---- gated norm and output projection ----
    struct ggml_tensor * zA = call(11, gc, wz, cur, NULL, GGML_OP_MUL_MAT, 0, 0,0,0,0,0,0,0,0);
    struct ggml_tensor * zA4 = ggml_reshape_4d(gc, zA, head_v, n_v, T, B);
    struct ggml_tensor * rmsA = call(4, gc, coreA, NULL, NULL, GGML_OP_RMS_NORM, 0,
                                     (int64_t) fb_of(eps), 0,0,0,0,0,0,0);
    struct ggml_tensor * silA = call(3, gc, zA4, NULL, NULL, GGML_UNARY_OP_SILU, 0,
                                     0,0,0,0,0,0,0,0);
    struct ggml_tensor * nrmA = call(0, gc, rmsA, silA, NULL, GGML_OP_MUL, 0, 0,0,0,0,0,0,0,0);
    // The projection needs the flattened [head_v*n_v, T, B] form, which the native arm
    // does with reshape_3d. Without it wout->ne[0] (4096) does not match nrmA->ne[0]
    // (128) and can_mul_mat refuses.
    struct ggml_tensor * nrmA3 = ggml_reshape_3d(gc, nrmA, head_v * n_v, T, B);
    struct ggml_tensor * outA = call(11, gc, wout, nrmA3, NULL, GGML_OP_MUL_MAT, 0,
                                     0,0,0,0,0,0,0,0);
    if (!rmsA || !silA || !nrmA || !outA) {
        printf("FG_GDN_LAYER gated norm returned null\n");
        return 1;
    }

    struct ggml_tensor * zB = ggml_mul_mat(gc, wz, cur);
    struct ggml_tensor * zB4 = ggml_reshape_4d(gc, zB, head_v, n_v, T, B);
    struct ggml_tensor * nrmB = ggml_mul(gc, ggml_rms_norm(gc, coreB, eps), ggml_silu(gc, zB4));
    struct ggml_tensor * outB = ggml_mul_mat(gc, wout,
        ggml_reshape_3d(gc, nrmB, head_v * n_v, T, B));

    struct { const char * n; struct ggml_tensor * a; struct ggml_tensor * b; } cmp[] = {
        { "alpha",    arawA, arawB }, { "beta_sig", betaA, betaB },
        { "gate",     gateA, gateB }, { "conv",     convA, convB },
        { "silu",     siluA, siluB }, { "q_view",   qvA,   qvB },
        { "k_view",   kvA,   kvB },   { "v_view",   vvA,   vvB },
        { "gdn",      gdnA,  gdnB },  { "core",     coreA, coreB },
        { "z",        zA4,   zB4 },   { "norm",     nrmA,  nrmB },
        { "out",      outA,  outB },
    };
    struct ggml_backend * be = ggml_backend_cpu_init();
    int bad = 0;
    for (size_t i = 0; i < sizeof cmp / sizeof cmp[0]; ++i) {
        if (!cmp[i].a || !cmp[i].b) { printf("  cmp %-9s NULL\n", cmp[i].n); ++bad; continue; }
        struct ggml_cgraph * fa = ggml_new_graph(gc);
        ggml_build_forward_expand(fa, cmp[i].a);
        struct ggml_cgraph * f2 = ggml_new_graph(gc);
        ggml_build_forward_expand(f2, cmp[i].b);
        ggml_backend_graph_compute(be, fa);
        ggml_backend_graph_compute(be, f2);
        const int64_t n = ggml_nelements(cmp[i].a);
        if (n != ggml_nelements(cmp[i].b)) {
            printf("  cmp %-9s ELEMENTS %lld vs %lld\n", cmp[i].n, (long long) n,
                   (long long) ggml_nelements(cmp[i].b));
            ++bad; continue;
        }
        float * pa = (float *) malloc((size_t) n * sizeof(float));
        float * pb = (float *) malloc((size_t) n * sizeof(float));
        memcpy(pa, cmp[i].a->data, (size_t) n * sizeof(float));
        memcpy(pb, cmp[i].b->data, (size_t) n * sizeof(float));
        int d = 0;
        for (int64_t j = 0; j < n; ++j) if (memcmp(&pa[j], &pb[j], sizeof(float)) != 0) ++d;
        printf("  cmp %-9s n=%lld differs=%d\n", cmp[i].n, (long long) n, d);
        if (d) ++bad;
        free(pa); free(pb);
    }

    printf("FG_GDN_LAYER_FULL k=%lld chan=%lld head_k=%lld n_k=%lld head_v=%lld n_v=%lld "
           "bad=%d %s\n", (long long) kernel, (long long) channels, (long long) head_k,
           (long long) n_k, (long long) head_v, (long long) n_v, bad,
           bad == 0 ? "PASS" : "FAIL");

    ggml_backend_free(be);
    ggml_free(gc);
    gguf_free(gg);
    if (meta) ggml_free(meta);
    return 0;
}
