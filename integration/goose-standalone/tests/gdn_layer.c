// GDN layer wiring gate: the recurrent path's chain, against ggml's constructors.
//
//   conv_silu -> view_4d q/k/v (qkv stride) -> l2_norm(q), l2_norm(k)
//             -> repeat q/k to the value head count -> gated_delta_net
//
// This is the recurrent-specific half of the layer. The norm/projection/residual
// half is the same as the attention layer, already proven bit-identical in EXP-688b.
//
// Same oracle discipline: build the chain twice, once through the Goose modules and
// once with ggml's own constructors, and compare every intermediate.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "gguf.h"
#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
#include "main.h"

extern int64_t gb_common(int64_t p);
// The view primitive directly: the module's `view4` name is the Goose side. The
// stride decision currently sits in this gate and is the next thing to move into a
// Goose export, exactly as the QG split did in EXP-687c.
extern int64_t fg158_view4(int64_t c, int64_t a, int64_t n0, int64_t n1, int64_t n2,
                           int64_t n3, int64_t nb1, int64_t nb2, int64_t nb3, int64_t offset);
extern void fg_goose_host_ensure_init(void);

static int32_t float_bits_of(float f) { int32_t b; memcpy(&b, &f, sizeof b); return b; }

static int64_t kv_u32(struct gguf_context * c, const char * key, int64_t dflt) {
    const int64_t id = gguf_find_key(c, key);
    if (id < 0) return dflt;
    switch (gguf_get_kv_type(c, (int) id)) {
        case GGUF_TYPE_UINT32: return (int64_t) gguf_get_val_u32(c, (int) id);
        case GGUF_TYPE_INT32:  return (int64_t) gguf_get_val_i32(c, (int) id);
        default: return dflt;
    }
}

int main(int argc, char ** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <apex.gguf>\n", argv[0]);
        return 2;
    }
    fg_goose_host_ensure_init();

    struct ggml_context * meta = NULL;
    struct gguf_init_params mp = { /*.no_alloc =*/ true, /*.ctx =*/ &meta };
    struct gguf_context * gguf = gguf_init_from_file(argv[1], mp);
    if (gguf == NULL) {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        return 1;
    }

    const int64_t tconv = gguf_find_tensor(gguf, "blk.0.ssm_conv1d.weight");
    const int64_t tnorm = gguf_find_tensor(gguf, "blk.0.ssm_norm.weight");
    const int64_t tqkv  = gguf_find_tensor(gguf, "blk.0.attn_qkv.weight");
    const int64_t tout  = gguf_find_tensor(gguf, "blk.0.ssm_out.weight");
    if (tconv < 0 || tnorm < 0 || tqkv < 0 || tout < 0) {
        fprintf(stderr, "blk.0 ssm tensors not found\n");
        return 1;
    }
    const int64_t kernel_size   = gguf_get_tensor_ne(gguf, (int) tconv)[0];   // 4
    const int64_t conv_channels = gguf_get_tensor_ne(gguf, (int) tconv)[1];   // 8192
    const int64_t head_v        = gguf_get_tensor_ne(gguf, (int) tnorm)[0];   // 128
    const int64_t qkv_width     = gguf_get_tensor_ne(gguf, (int) tqkv)[1];    // 8192
    const int64_t value_dim     = gguf_get_tensor_ne(gguf, (int) tout)[0];    // 4096
    const int64_t n_v_heads     = value_dim / head_v;                         // 32
    // qkv = 2*head_k*n_k + head_v*n_v  ->  head_k*n_k = (qkv - value_dim)/2
    const int64_t head_k_total  = (qkv_width - value_dim) / 2;                // 2048
    const int64_t n_k_heads     = n_v_heads / 2;                              // 16
    const int64_t head_k        = head_k_total / n_k_heads;                   // 128

    struct ggml_init_params gp = { 512 * 1024 * 1024, NULL, /*.no_alloc =*/ false };
    struct ggml_context * gc = ggml_init(gp);
    if (gc == NULL) {
        printf("FG_GDN_LAYER init failed\n");
        return 2;
    }

    const size_t elem = sizeof(float);
    const int64_t tokens = 1, seqs = 1;

    // conv_input [kernel_size, conv_channels] and the kernel.
    struct ggml_tensor * cin = ggml_new_tensor_2d(gc, GGML_TYPE_F32, kernel_size, conv_channels);
    struct ggml_tensor * ck  = ggml_new_tensor_2d(gc, GGML_TYPE_F32, kernel_size, conv_channels);
    {
        float * d1 = (float *) cin->data;
        for (int64_t i = 0; i < ggml_nelements(cin); ++i) d1[i] = (float) ((i % 11) - 5) / 32.0f;
        float * d2 = (float *) ck->data;
        for (int64_t i = 0; i < ggml_nelements(ck); ++i) d2[i] = (float) ((i % 7) - 3) / 16.0f;
    }

    const float eps = 1e-6f;

    // ---- arm A: through the Goose modules ----
    // conv (mode 27), then silu (mode 3), then the three stride views (primitive),
    // then l2_norm and repeat (gdn-ops), then gated_delta_net (mode 28).
    struct ggml_tensor * convA = (struct ggml_tensor *) (uintptr_t) gb_common(
        (int64_t) (uintptr_t) (uint64_t[16]){ 27, (uint64_t) (uintptr_t) gc,
            (uint64_t) (uintptr_t) cin, (uint64_t) (uintptr_t) ck, 0, 0, 0,
            0,0,0,0,0,0,0,0,0 });
    if (!convA) { printf("FG_GDN_LAYER conv failed\n"); return 1; }
    struct ggml_tensor * siluA = (struct ggml_tensor *) (uintptr_t) gb_common(
        (int64_t) (uintptr_t) (uint64_t[16]){ 3, (uint64_t) (uintptr_t) gc,
            (uint64_t) (uintptr_t) convA, 0, 0, (uint64_t) GGML_UNARY_OP_SILU, 0,
            0,0,0,0,0,0,0,0,0 });
    if (!siluA) { printf("FG_GDN_LAYER silu failed\n"); return 1; }

    const int64_t nb1_qkv = conv_channels * (int64_t) elem;   // row stride of the mixed output
    struct ggml_tensor * qvA = (struct ggml_tensor *) (uintptr_t) fg158_view4(
        (int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) siluA,
        head_k, n_k_heads, tokens, seqs,
        elem * head_k, nb1_qkv, nb1_qkv * tokens, 0);
    struct ggml_tensor * kvA = (struct ggml_tensor *) (uintptr_t) fg158_view4(
        (int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) siluA,
        head_k, n_k_heads, tokens, seqs,
        elem * head_k, nb1_qkv, nb1_qkv * tokens,
        elem * head_k * n_k_heads);
    struct ggml_tensor * vvA = (struct ggml_tensor *) (uintptr_t) fg158_view4(
        (int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) siluA,
        head_v, n_v_heads, tokens, seqs,
        elem * head_v, nb1_qkv, nb1_qkv * tokens,
        elem * 2 * head_k * n_k_heads);
    if (!qvA || !kvA || !vvA) { printf("FG_GDN_LAYER views failed\n"); return 1; }

    const int64_t qnA = gdn_l2norm((int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) qvA,
                                   (int64_t) float_bits_of(eps));
    const int64_t knA = gdn_l2norm((int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) kvA,
                                   (int64_t) float_bits_of(eps));
    const int64_t qrA = gdn_repeat_to_v((int64_t) (uintptr_t) gc, qnA, head_k,
                                        n_k_heads, n_v_heads, tokens, seqs);
    const int64_t krA = gdn_repeat_to_v((int64_t) (uintptr_t) gc, knA, head_k,
                                        n_k_heads, n_v_heads, tokens, seqs);
    if (!qnA || !knA || !qrA || !krA) { printf("FG_GDN_LAYER norm/repeat failed\n"); return 1; }

    // gate [1, H, T, B], beta [1, H, T, B], state [head_v, head_v, H, B]
    struct ggml_tensor * gt = ggml_new_tensor_4d(gc, GGML_TYPE_F32, 1, n_v_heads, tokens, seqs);
    struct ggml_tensor * bt = ggml_new_tensor_4d(gc, GGML_TYPE_F32, 1, n_v_heads, tokens, seqs);
    struct ggml_tensor * st = ggml_new_tensor_4d(gc, GGML_TYPE_F32, head_v, head_v, n_v_heads, seqs);
    {
        float * dg = (float *) gt->data; for (int64_t i = 0; i < ggml_nelements(gt); ++i) dg[i] = -0.5f;
        float * db = (float *) bt->data; for (int64_t i = 0; i < ggml_nelements(bt); ++i) db[i] = 0.5f;
        float * ds = (float *) st->data; for (int64_t i = 0; i < ggml_nelements(st); ++i) ds[i] = (float) ((i % 9) - 4) / 64.0f;
    }

    struct ggml_tensor * gdnA = (struct ggml_tensor *) (uintptr_t) gb_common(
        (int64_t) (uintptr_t) (uint64_t[16]){ 28, (uint64_t) (uintptr_t) gc,
            (uint64_t) qrA, (uint64_t) krA, (uint64_t) (uintptr_t) vvA, 0, 0,
            (uint64_t) (uintptr_t) gt, (uint64_t) (uintptr_t) bt,
            (uint64_t) (uintptr_t) st, 1, 0,0,0,0,0 });
    if (!gdnA) { printf("FG_GDN_LAYER gated_delta_net failed\n"); return 1; }

    // ---- arm B: ggml constructors ----
    struct ggml_tensor * convB = ggml_ssm_conv(gc, cin, ck);
    struct ggml_tensor * siluB = ggml_silu(gc, convB);
    struct ggml_tensor * qvB = ggml_view_4d(gc, siluB, head_k, n_k_heads, tokens, seqs,
        elem * head_k, nb1_qkv, nb1_qkv * tokens, 0);
    struct ggml_tensor * kvB = ggml_view_4d(gc, siluB, head_k, n_k_heads, tokens, seqs,
        elem * head_k, nb1_qkv, nb1_qkv * tokens, elem * head_k * n_k_heads);
    struct ggml_tensor * vvB = ggml_view_4d(gc, siluB, head_v, n_v_heads, tokens, seqs,
        elem * head_v, nb1_qkv, nb1_qkv * tokens, elem * 2 * head_k * n_k_heads);
    struct ggml_tensor * qnB = ggml_l2_norm(gc, qvB, eps);
    struct ggml_tensor * knB = ggml_l2_norm(gc, kvB, eps);
    struct ggml_tensor * qrB = ggml_repeat_4d(gc, qnB, head_k, n_v_heads, tokens, seqs);
    struct ggml_tensor * krB = ggml_repeat_4d(gc, knB, head_k, n_v_heads, tokens, seqs);
    struct ggml_tensor * gdnB = ggml_gated_delta_net(gc, qrB, krB, vvB, gt, bt, st, 1);

    // The views share the left-hand tensor's data, so comparing them compares the
    // stride arithmetic; the results are computed by the backend.
    ggml_backend_t be = ggml_backend_cpu_init();
    struct { const char * n; struct ggml_tensor * a; struct ggml_tensor * b; } cmp[] = {
        { "conv", convA, convB }, { "silu", siluA, siluB },
        { "q_view", qvA, qvB }, { "k_view", kvA, kvB }, { "v_view", vvA, vvB },
        { "q_norm", (struct ggml_tensor *) (uintptr_t) qnA, qnB },
        { "k_norm", (struct ggml_tensor *) (uintptr_t) knA, knB },
        { "q_rep", (struct ggml_tensor *) (uintptr_t) qrA, qrB },
        { "k_rep", (struct ggml_tensor *) (uintptr_t) krA, krB },
        { "gdn", gdnA, gdnB },
    };
    int bad = 0;
    for (size_t i = 0; i < sizeof cmp / sizeof cmp[0]; ++i) {
        struct ggml_cgraph * fa = ggml_new_graph(gc);
        ggml_build_forward_expand(fa, cmp[i].a);
        struct ggml_cgraph * fb = ggml_new_graph(gc);
        ggml_build_forward_expand(fb, cmp[i].b);
        ggml_backend_graph_compute(be, fa);
        ggml_backend_graph_compute(be, fb);
        const int64_t n = ggml_nelements(cmp[i].a);
        if (n != ggml_nelements(cmp[i].b)) {
            printf("  cmp %-7s ELEMENTS %lld vs %lld\n", cmp[i].n,
                   (long long) n, (long long) ggml_nelements(cmp[i].b));
            ++bad; continue;
        }
        float * pa = (float *) malloc((size_t) n * sizeof(float));
        float * pb = (float *) malloc((size_t) n * sizeof(float));
        memcpy(pa, cmp[i].a->data, (size_t) n * sizeof(float));
        memcpy(pb, cmp[i].b->data, (size_t) n * sizeof(float));
        int d = 0;
        for (int64_t j = 0; j < n; ++j) if (memcmp(&pa[j], &pb[j], sizeof(float)) != 0) ++d;
        printf("  cmp %-7s n=%lld differs=%d\n", cmp[i].n, (long long) n, d);
        if (d != 0) ++bad;
        free(pa); free(pb);
    }

    printf("FG_GDN_LAYER kernel=%lld chan=%lld head_k=%lld n_k=%lld head_v=%lld n_v=%lld "
           "bad_nodes=%d %s\n",
           (long long) kernel_size, (long long) conv_channels, (long long) head_k,
           (long long) n_k_heads, (long long) head_v, (long long) n_v_heads, bad,
           bad == 0 ? "PASS" : "FAIL");

    ggml_backend_free(be);
    ggml_free(gc);
    gguf_free(gguf);
    if (meta) ggml_free(meta);
    return 0;
}
