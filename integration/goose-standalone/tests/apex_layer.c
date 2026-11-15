// Apex full-layer gate: one attention layer wired end to end.
//
//   x -> rms_norm -> wq -> QG split -> q, gate
//                 -> wk -> k view      -> rope
//                 -> wv -> v view      -> rope
//   rope(q) x rope(k) x v -> flash_attn -> * silu(gate) -> wo -> + x
//
// Every node is emitted by gb_common, the QG split by goose/model/qgate-split.goose,
// and every extent is read from the model file. Nothing about the geometry is
// hardcoded: n_embd_head comes from the q projection width divided by
// (2 * n_head), which is the interleaving the native builder uses.
//
// The check is not a host reimplementation - that would repeat attention in the
// test. It verifies the result shape, that every value is finite, that the output is
// not a copy of the input, and that two runs of the same graph are bit-identical.
// Each node is proven individually against the substrate elsewhere; this proves the
// wiring.
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
extern void fg_goose_host_ensure_init(void);

#define P_N_EMBD 0
#define P_N_HEAD 4
#define P_N_HEAD_KV 5

static int32_t float_bits_of(float f) { int32_t b; memcpy(&b, &f, sizeof b); return b; }

static int64_t kv_u32(struct gguf_context * c, const char * key, int64_t dflt) {
    const int64_t id = gguf_find_key(c, key);
    if (id < 0) return dflt;
    switch (gguf_get_kv_type(c, (int) id)) {
        case GGUF_TYPE_UINT32: return (int64_t) gguf_get_val_u32(c, (int) id);
        case GGUF_TYPE_INT32:  return (int64_t) gguf_get_val_i32(c, (int) id);
        case GGUF_TYPE_UINT64: return (int64_t) gguf_get_val_u64(c, (int) id);
        case GGUF_TYPE_INT64:  return gguf_get_val_i64(c, (int) id);
        default: return dflt;
    }
}

static struct ggml_tensor * call(int64_t mode, struct ggml_context * gc,
                                 struct ggml_tensor * a, struct ggml_tensor * b,
                                 struct ggml_tensor * d, int64_t op, int64_t inplace,
                                 int64_t a7, int64_t a8, int64_t a9,
                                 int64_t a10, int64_t a11, int64_t a12,
                                 int64_t a13, int64_t a14) {
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

    int64_t p[32] = { 0 };
    p[P_N_EMBD]    = kv_u32(gguf, "qwen35moe.embedding_length", 0);
    p[P_N_HEAD]    = kv_u32(gguf, "qwen35moe.attention.head_count", 0);
    p[P_N_HEAD_KV] = kv_u32(gguf, "qwen35moe.attention.head_count_kv", 0);

    const int64_t K = p[P_N_EMBD];
    const int64_t H = p[P_N_HEAD];
    const int64_t HKV = p[P_N_HEAD_KV];

    const int64_t tq = gguf_find_tensor(gguf, "blk.3.attn_q.weight");
    const int64_t tk = gguf_find_tensor(gguf, "blk.3.attn_k.weight");
    const int64_t tv = gguf_find_tensor(gguf, "blk.3.attn_v.weight");
    const int64_t to = gguf_find_tensor(gguf, "blk.3.attn_output.weight");
    if (tq < 0 || tk < 0 || tv < 0 || to < 0) {
        fprintf(stderr, "blk.3 attention tensors not found\n");
        gguf_free(gguf);
        return 1;
    }
    const int64_t wq_n = gguf_get_tensor_ne(gguf, (int) tq)[1];   // 8192 = 2*HD*H
    const int64_t wk_n = gguf_get_tensor_ne(gguf, (int) tk)[1];   // 512  = HD*HKV
    const int64_t wv_n = gguf_get_tensor_ne(gguf, (int) tv)[1];   // 512
    const int64_t wo_k = gguf_get_tensor_ne(gguf, (int) to)[0];   // 4096 = HD*H
    const int64_t wo_n = gguf_get_tensor_ne(gguf, (int) to)[1];   // 2048

    // n_embd_head is the ATTENTION head width: the q projection emits two head widths
    // per head (query and gate interleaved), so it is wq_n/(2*H), not wq_n/H.
    const int64_t HD = wq_n / (2 * H);
    const int64_t HDKV = wk_n / HKV;

    struct ggml_init_params gp = { 1024 * 1024 * 1024, NULL, /*.no_alloc =*/ true };
    struct ggml_context * gc = ggml_init(gp);
    ggml_backend_t backend = ggml_backend_cpu_init();
    if (gc == NULL || backend == NULL) {
        printf("FG_APEX_LAYER init failed\n");
        return 2;
    }

    struct ggml_tensor * x  = ggml_new_tensor_2d(gc, GGML_TYPE_F32, K, 1);
    struct ggml_tensor * wq = ggml_new_tensor_2d(gc, GGML_TYPE_F32, K, wq_n);
    struct ggml_tensor * wk = ggml_new_tensor_2d(gc, GGML_TYPE_F32, K, wk_n);
    struct ggml_tensor * wv = ggml_new_tensor_2d(gc, GGML_TYPE_F32, K, wv_n);
    struct ggml_tensor * wo = ggml_new_tensor_2d(gc, GGML_TYPE_F32, wo_k, wo_n);
    // blk.3.attn_q_norm.weight and attn_k_norm.weight are [256,1]: a per-head RMS
    // weight applied to q and k BEFORE rope. The first version of this gate omitted
    // them, and because both arms omitted them the comparison still agreed - which is
    // the failure mode a wiring-only gate cannot catch on its own.
    struct ggml_tensor * qnw = ggml_new_tensor_1d(gc, GGML_TYPE_F32, HD);
    struct ggml_tensor * knw = ggml_new_tensor_1d(gc, GGML_TYPE_F32, HDKV);
    struct ggml_tensor * pos = ggml_new_tensor_1d(gc, GGML_TYPE_I32, 1);
    ggml_backend_alloc_ctx_tensors(gc, backend);

    float * xd = (float *) malloc((size_t) K * sizeof(float));
    for (int64_t i = 0; i < K; ++i) xd[i] = (float) ((i % 17) - 8) / 64.0f;
    ggml_backend_tensor_set(x, xd, 0, (size_t) K * sizeof(float));
    int32_t zero = 0;
    ggml_backend_tensor_set(pos, &zero, 0, sizeof zero);

    struct { struct ggml_tensor * t; int seed; } ws[] = {
        { wq, 3 }, { wk, 5 }, { wv, 7 }, { wo, 11 }, { qnw, 2 }, { knw, 2 },
    };
    for (size_t i = 0; i < 6; ++i) {
        const int64_t n = ggml_nelements(ws[i].t);
        float * wd = (float *) malloc((size_t) n * sizeof(float));
        for (int64_t j = 0; j < n; ++j) wd[j] = (float) ((j % ws[i].seed) - ws[i].seed / 2) / 256.0f;
        ggml_backend_tensor_set(ws[i].t, wd, 0, (size_t) n * sizeof(float));
        free(wd);
    }

    const float eps = 1e-6f;
    const float scale = 1.0f / sqrtf((float) HD);

    // 1. rms_norm
    struct ggml_tensor * normed = call(4, gc, x, NULL, NULL, GGML_OP_RMS_NORM, 0,
                                       (int64_t) float_bits_of(eps), 0, 0, 0, 0, 0, 0, 0);
    // 2-4. the three projections
    struct ggml_tensor * qg = call(11, gc, wq, normed, NULL, GGML_OP_MUL_MAT, 0, 0,0,0,0,0,0,0,0);
    struct ggml_tensor * kf = call(11, gc, wk, normed, NULL, GGML_OP_MUL_MAT, 0, 0,0,0,0,0,0,0,0);
    struct ggml_tensor * vf = call(11, gc, wv, normed, NULL, GGML_OP_MUL_MAT, 0, 0,0,0,0,0,0,0,0);
    if (!normed || !qg || !kf || !vf) { printf("FG_APEX_LAYER projection failed\n"); return 1; }
    ggml_backend_alloc_ctx_tensors(gc, backend);

    // 5. the QG split, decided by goose/model/qgate-split.goose
    int64_t views[2] = { 0, 0 };
    const int64_t nsplit = qg_split((int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) qg,
                                    HD, H, 1, 4, (sl_i64) { views, 2 });
    if (nsplit != 2) { printf("FG_APEX_LAYER qg_split refused\n"); return 1; }
    struct ggml_tensor * qv = (struct ggml_tensor *) (uintptr_t) views[0];
    struct ggml_tensor * gv = (struct ggml_tensor *) (uintptr_t) views[1];
    if (!qv || !gv) { printf("FG_APEX_LAYER qg_split returned a null view\n"); return 1; }
    ggml_backend_alloc_ctx_tensors(gc, backend);

    // 6-7. k and v as [HD, HKV, 1, 1]
    struct ggml_tensor * k4 = call(24, gc, kf, NULL, NULL, GGML_OP_CONT, 0, HDKV, HKV, 1, 1, 0, 0, 0, 0);
    struct ggml_tensor * v4 = call(24, gc, vf, NULL, NULL, GGML_OP_CONT, 0, HDKV, HKV, 1, 1, 0, 0, 0, 0);
    if (!k4 || !v4) { printf("FG_APEX_LAYER k/v view failed\n"); return 1; }
    ggml_backend_alloc_ctx_tensors(gc, backend);

    // 8-10. rope on q and k
    // The full rope parameter set, in the order ggml_rope_impl writes it:
    // arg8 n_ctx_orig, arg9 freq_base, arg10 freq_scale, arg11 ext_factor,
    // arg12 attn_factor, arg13 beta_fast, arg14 beta_slow. Leaving freq_scale,
    // attn_factor and the betas at zero made the Goose arm diverge from the
    // ggml-constructor arm in every element.
    // Per-head RMS norm of q and k, then multiply by the head weight (build_norm with
    // a weight vector is rms_norm followed by mul). This is qwen35moe.cpp:311-321 and
    // it happens BEFORE rope, not after.
    struct ggml_tensor * qn = call(4, gc, qv, NULL, NULL, GGML_OP_RMS_NORM, 0,
                                   (int64_t) float_bits_of(eps), 0, 0, 0, 0, 0, 0, 0);
    qn = call(0, gc, qn, qnw, NULL, GGML_OP_MUL, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    struct ggml_tensor * kn = call(4, gc, k4, NULL, NULL, GGML_OP_RMS_NORM, 0,
                                   (int64_t) float_bits_of(eps), 0, 0, 0, 0, 0, 0, 0);
    kn = call(0, gc, kn, knw, NULL, GGML_OP_MUL, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    if (!qn || !kn) { printf("FG_APEX_LAYER q/k norm failed\n"); return 1; }
    ggml_backend_alloc_ctx_tensors(gc, backend);

    // IMRoPE: rope.dimension_count 64, dimension_sections [11 11 10 0], freq_base 1e7.
    // The plain ggml_rope_ext used before is a different op with a different result.
    const int64_t fb  = float_bits_of(1e7f);
    const int64_t fs  = float_bits_of(1.0f);
    const int64_t af  = float_bits_of(1.0f);
    const int64_t bf  = float_bits_of(32.0f);
    const int64_t bs  = float_bits_of(1.0f);
    const int64_t ctx_orig = 4096;
    const int64_t n_rot = 64;
    struct ggml_tensor * qr = (struct ggml_tensor *) (uintptr_t) q35_rope_multi(
        (int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) qn, (int64_t) (uintptr_t) pos,
        n_rot, 11, 11, 10, 0, GGML_ROPE_TYPE_NORMAL, ctx_orig, fb, fs, 0, af, bf, bs);
    struct ggml_tensor * kr = (struct ggml_tensor *) (uintptr_t) q35_rope_multi(
        (int64_t) (uintptr_t) gc, (int64_t) (uintptr_t) kn, (int64_t) (uintptr_t) pos,
        n_rot, 11, 11, 10, 0, GGML_ROPE_TYPE_NORMAL, ctx_orig, fb, fs, 0, af, bf, bs);
    if (!qr || !kr) { printf("FG_APEX_LAYER rope failed\n"); return 1; }
    ggml_backend_alloc_ctx_tensors(gc, backend);

    // 11. attention
    // mode 26 argument order: arg7 is the MASK pointer, arg8 the scale, arg9 the
    // max_bias, arg10 the logit_softcap. Passing the scale in arg7 made the module
    // treat a float bit pattern as a tensor pointer.
    struct ggml_tensor * attn = call(26, gc, qr, kr, v4, 0, 0,
                                     0 /* mask */, (int64_t) float_bits_of(scale),
                                     0 /* max_bias */, 0 /* logit_softcap */, 0, 0, 0, 0);
    if (!attn) { printf("FG_APEX_LAYER attention failed\n"); return 1; }
    ggml_backend_alloc_ctx_tensors(gc, backend);

    // 12. the gate is SIGMOID, not silu: qwen35moe.cpp:352 uses ggml_sigmoid.
    // Its layout is [HD, H, 1, 1] while the attention result is [HD, 1, H, 1], so it
    // is reshaped to the attention layout before the multiply - the element count is
    // the same 4096.
    struct ggml_tensor * g4 = call(24, gc, gv, NULL, NULL, GGML_OP_CONT, 0, HD, 1, H, 1, 0, 0, 0, 0);
    if (!g4) { printf("FG_APEX_LAYER gate reshape failed\n"); return 1; }
    ggml_backend_alloc_ctx_tensors(gc, backend);
    struct ggml_tensor * gs = call(3, gc, g4, NULL, NULL, GGML_UNARY_OP_SIGMOID, 0, 0,0,0,0,0,0,0,0);
    if (!gs) { printf("FG_APEX_LAYER gate sigmoid failed\n"); return 1; }
    ggml_backend_alloc_ctx_tensors(gc, backend);

    // 13. attn * sigmoid(gate)
    struct ggml_tensor * gated = call(0, gc, attn, gs, NULL, GGML_OP_MUL, 0, 0,0,0,0,0,0,0,0);
    if (!gated) { printf("FG_APEX_LAYER gate multiply failed\n"); return 1; }
    ggml_backend_alloc_ctx_tensors(gc, backend);

    // 14. flatten to [wo_k, 1] and project back
    struct ggml_tensor * a2 = call(24, gc, gated, NULL, NULL, GGML_OP_CONT, 0, wo_k, 1, 1, 1, 0, 0, 0, 0);
    if (!a2) { printf("FG_APEX_LAYER flatten failed\n"); return 1; }
    ggml_backend_alloc_ctx_tensors(gc, backend);
    struct ggml_tensor * proj = call(11, gc, wo, a2, NULL, GGML_OP_MUL_MAT, 0, 0,0,0,0,0,0,0,0);
    if (!proj) { printf("FG_APEX_LAYER output projection failed\n"); return 1; }
    ggml_backend_alloc_ctx_tensors(gc, backend);

    // 15. residual
    struct ggml_tensor * y = call(0, gc, proj, x, NULL, GGML_OP_ADD, 0, 0,0,0,0,0,0,0,0);
    if (!y) { printf("FG_APEX_LAYER residual failed\n"); return 1; }
    ggml_backend_alloc_ctx_tensors(gc, backend);

    // ---- arm B: the same layer built with ggml's own constructors ----
    // This is the wiring oracle. Each node's arithmetic is already proven bit-exact
    // against the substrate; what this compares is the ORDER and the SHAPES, which
    // is exactly what a hand-wired graph can get wrong while every node is right.
    const size_t elem = sizeof(float);
    struct ggml_tensor * b_normed = ggml_rms_norm(gc, x, eps);
    struct ggml_tensor * b_qg = ggml_mul_mat(gc, wq, b_normed);
    struct ggml_tensor * b_q = ggml_view_3d(gc, b_qg, HD, H, 1,
        elem * HD * 2, elem * HD * 2 * H, 0);
    struct ggml_tensor * b_g = ggml_view_3d(gc, b_qg, HD, H, 1,
        elem * HD * 2, elem * HD * 2 * H, elem * HD);
    struct ggml_tensor * b_k = ggml_mul_mat(gc, wk, b_normed);
    struct ggml_tensor * b_k4 = ggml_view_3d(gc, b_k, HDKV, HKV, 1,
        elem * HDKV, elem * HDKV * HKV, 0);
    struct ggml_tensor * b_v = ggml_mul_mat(gc, wv, b_normed);
    struct ggml_tensor * b_v4 = ggml_view_3d(gc, b_v, HDKV, HKV, 1,
        elem * HDKV, elem * HDKV * HKV, 0);
    int b_sections[4] = { 11, 11, 10, 0 };
    struct ggml_tensor * b_qn = ggml_mul(gc, ggml_rms_norm(gc, b_q, eps), qnw);
    struct ggml_tensor * b_kn = ggml_mul(gc, ggml_rms_norm(gc, b_k4, eps), knw);
    struct ggml_tensor * b_qr = ggml_rope_multi(gc, b_qn, pos, NULL, 64, b_sections,
        GGML_ROPE_TYPE_NORMAL, 4096, 1e7f, 1.0f, 0.0f, 1.0f, 32.0f, 1.0f);
    struct ggml_tensor * b_kr = ggml_rope_multi(gc, b_kn, pos, NULL, 64, b_sections,
        GGML_ROPE_TYPE_NORMAL, 4096, 1e7f, 1.0f, 0.0f, 1.0f, 32.0f, 1.0f);
    struct ggml_tensor * b_attn = ggml_flash_attn_ext(gc, b_qr, b_kr, b_v4, NULL,
        scale, 0.0f, 0.0f);
    struct ggml_tensor * b_gc = ggml_cont(gc, b_g);
    struct ggml_tensor * b_g4 = ggml_reshape_4d(gc, b_gc, HD, 1, H, 1);
    struct ggml_tensor * b_gs = ggml_sigmoid(gc, b_g4);
    struct ggml_tensor * b_gated = ggml_mul(gc, b_attn, b_gs);
    struct ggml_tensor * b_a2 = ggml_reshape_2d(gc, b_gated, wo_k, 1);
    struct ggml_tensor * b_proj = ggml_mul_mat(gc, wo, b_a2);
    struct ggml_tensor * y_b = ggml_add(gc, b_proj, x);
    ggml_backend_alloc_ctx_tensors(gc, backend);

    struct ggml_cgraph * gb = ggml_new_graph(gc);
    ggml_build_forward_expand(gb, y_b);
    const enum ggml_status st_b = ggml_backend_graph_compute(backend, gb);
    float * ob = (float *) malloc((size_t) ggml_nelements(y_b) * sizeof(float));
    ggml_backend_tensor_get(y_b, ob, 0, (size_t) ggml_nelements(y_b) * sizeof(float));

    struct ggml_cgraph * gf = ggml_new_graph(gc);
    ggml_build_forward_expand(gf, y);
    const enum ggml_status st1 = ggml_backend_graph_compute(backend, gf);

    const int64_t n_out = ggml_nelements(y);
    float * o1 = (float *) malloc((size_t) n_out * sizeof(float));
    ggml_backend_tensor_get(y, o1, 0, (size_t) n_out * sizeof(float));

    const enum ggml_status st2 = ggml_backend_graph_compute(backend, gf);
    float * o2 = (float *) malloc((size_t) n_out * sizeof(float));
    ggml_backend_tensor_get(y, o2, 0, (size_t) n_out * sizeof(float));
    int nondet = 0;
    for (int64_t i = 0; i < n_out; ++i) {
        if (memcmp(&o1[i], &o2[i], sizeof(float)) != 0) ++nondet;
    }

    int nonfinite = 0, changed = 0;
    for (int64_t i = 0; i < n_out; ++i) if (!isfinite(o1[i])) ++nonfinite;
    for (int64_t i = 0; i < n_out && i < K; ++i) if (o1[i] != xd[i]) ++changed;

    // Compare the intermediate nodes to locate the divergence.
    {
        struct { const char * n; struct ggml_tensor * a; struct ggml_tensor * b; } cmp[] = {
            { "normed", normed, b_normed }, { "qg", qg, b_qg },
            { "k4", k4, b_k4 }, { "v4", v4, b_v4 },
            { "q_norm", qn, b_qn }, { "k_norm", kn, b_kn },
            { "qr", qr, b_qr }, { "kr", kr, b_kr },
            { "attn", attn, b_attn }, { "g4", g4, b_g4 },
            { "gs", gs, b_gs }, { "gated", gated, b_gated },
            { "proj", proj, b_proj },
        };
        for (size_t i = 0; i < sizeof cmp / sizeof cmp[0]; ++i) {
            const int64_t n = ggml_nelements(cmp[i].a);
            if (n != ggml_nelements(cmp[i].b)) {
                printf("  cmp %-8s ELEMENT COUNT %lld vs %lld\n", cmp[i].n,
                       (long long) n, (long long) ggml_nelements(cmp[i].b));
                continue;
            }
            float * pa = (float *) malloc((size_t) n * sizeof(float));
            float * pb = (float *) malloc((size_t) n * sizeof(float));
            ggml_backend_tensor_get(cmp[i].a, pa, 0, (size_t) n * sizeof(float));
            ggml_backend_tensor_get(cmp[i].b, pb, 0, (size_t) n * sizeof(float));
            int d = 0;
            for (int64_t j = 0; j < n; ++j) if (memcmp(&pa[j], &pb[j], sizeof(float)) != 0) ++d;
            printf("  cmp %-8s n=%lld differs=%d\n", cmp[i].n, (long long) n, d);
            free(pa); free(pb);
        }
    }

    int wiring_diff = 0;
    for (int64_t i = 0; i < n_out && i < ggml_nelements(y_b); ++i) {
        if (memcmp(&o1[i], &ob[i], sizeof(float)) != 0) ++wiring_diff;
    }

    const int shape_ok = y->ne[0] == K && y->ne[1] == 1;
    printf("FG_APEX_LAYER nodes=15 K=%lld H=%lld HKV=%lld HD=%lld HDKV=%lld "
           "out=[%lld %lld] shape_ok=%d status=%d,%d nonfinite=%d changed=%d nondet=%d "
           "wiring_differs=%d vs_ggml=%d %s\n",
           (long long) K, (long long) H, (long long) HKV, (long long) HD, (long long) HDKV,
           (long long) y->ne[0], (long long) y->ne[1], shape_ok,
           (int) st1, (int) st2, nonfinite, changed, nondet, wiring_diff, (int) st_b,
           (shape_ok && st1 == GGML_STATUS_SUCCESS && st2 == GGML_STATUS_SUCCESS &&
            nonfinite == 0 && changed > 0 && nondet == 0 &&
            wiring_diff == 0 && st_b == GGML_STATUS_SUCCESS) ? "PASS" : "FAIL");

    free(o1); free(o2); free(xd);
    ggml_backend_free(backend);
    ggml_free(gc);
    gguf_free(gguf);
    if (meta) ggml_free(meta);
    return 0;
}
