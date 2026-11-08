// worker-2/kv-q4-final/q4-services.cpp
// Typed projections + raw ggml primitives for the Q4_0 KV ownership/rotation
// path. Goose owns layer/buffer plan, n_rot decisions, hadamard lifetime and
// cleanup; this file only performs raw ggml/container operations.
#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "llama.h"
#include "llama-arch.h"
#include "ggml.h"
#include "ggml-backend.h"
#include "llama-batch.h"
#include "llama-graph.h"
#include "llama-memory.h"

#define private public
#include "llama-model.h"
#include "llama-kv-cache.h"
#undef private

#include "kv-native-adapter.h"

extern "C" {
int64_t fg153_q4_entry(int64_t mode, int64_t a, int64_t b, int64_t c);
void    fg153_q4_witness_impl(int64_t mode);
int64_t fg_goose_kv_lifecycle_entry(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
int64_t fg_kv_obj(int64_t);
void    fg_kv_vec_push(int64_t, int64_t);
void    fg_kv_destroy(int64_t);
void    fg_kv_put(int64_t, int64_t, int64_t, int64_t);
void    fg_kv_slot_push(int64_t, int64_t, int64_t);
}

namespace {
struct frame {
    const llama_model * model = nullptr;
    const llama_hparams * hp = nullptr;
    ggml_type tk = GGML_TYPE_F32;
    ggml_type tv = GGML_TYPE_F32;
    int offload = 0;
    uint32_t kv_size = 0;
    uint32_t n_stream = 1;
    const llama_kv_cache::layer_filter_cb * filter = nullptr;
    const char * tag = "";
    std::map<ggml_backend_buffer_type_t, ggml_context *> ctxs;
};
int64_t g_fail_after = -1;
bool take_ok() {
    if (g_fail_after < 0) return true;
    if (g_fail_after == 0) return false;
    g_fail_after -= 1;
    return true;
}
frame * as_frame(int64_t p) { return reinterpret_cast<frame *>(static_cast<intptr_t>(p)); }
llama_kv_cache * as_kv(int64_t p) { return reinterpret_cast<llama_kv_cache *>(static_cast<intptr_t>(p)); }
ggml_backend_buffer_type_t buft_of(frame & f, uint32_t il) {
    if (!f.offload) return ggml_backend_cpu_buffer_type();
    return ggml_backend_dev_buffer_type(f.model->dev_layer((int32_t) il));
}
} // namespace

extern "C" {
static int64_t g_witness = 0;
void fg153_q4_witness_impl(int64_t mode) { (void) mode; g_witness += 1; }
int64_t fg153_q4_witness(void) { return g_witness; }
void fg153_q4_force_oom(int64_t after) { g_fail_after = after; }
void fg153_q4_reset_oom(void) { g_fail_after = -1; }

int64_t q4_n_layer(int64_t fp) { return (int64_t) as_frame(fp)->hp->n_layer_all; }
int64_t q4_use_layer(int64_t fp, int64_t il) {
    frame & f = *as_frame(fp);
    if (!f.hp->has_kv((uint32_t) il)) return 0;
    if (f.filter && !(*f.filter)((int32_t) il)) return 0;
    return 1;
}
int64_t q4_n_embd_k(int64_t fp, int64_t il) { return (int64_t) as_frame(fp)->hp->n_embd_k_gqa((uint32_t) il); }
int64_t q4_n_embd_v(int64_t fp, int64_t il) { return (int64_t) as_frame(fp)->hp->n_embd_v_gqa((uint32_t) il); }
int64_t q4_type(int64_t fp, int64_t is_v) { return is_v ? (int64_t) as_frame(fp)->tv : (int64_t) as_frame(fp)->tk; }
int64_t q4_kv_size(int64_t fp) { return (int64_t) as_frame(fp)->kv_size; }
int64_t q4_nstream(int64_t fp) { return (int64_t) as_frame(fp)->n_stream; }
int64_t q4_tag(int64_t fp) { return (int64_t) (intptr_t) as_frame(fp)->tag; }
int64_t q4_other(int64_t fp) { (void) fp; return 0; }
int64_t q4_rot_disabled(int64_t fp) {
    frame & f = *as_frame(fp);
    const char * env = getenv("LLAMA_ATTN_ROT_DISABLE");
    const bool disabled = !f.model->attn_rot || (env && atoi(env) != 0);
    return disabled ? 1 : 0;
}
int64_t q4_quantized(int64_t fp, int64_t is_v) {
    return ggml_is_quantized(is_v ? as_frame(fp)->tv : as_frame(fp)->tk) ? 1 : 0;
}
int64_t q4_head_k(int64_t fp) { return (int64_t) as_frame(fp)->hp->n_embd_head_k(); }
int64_t q4_head_v(int64_t fp) { return (int64_t) as_frame(fp)->hp->n_embd_head_v(); }

int64_t q4_ctx(int64_t fp, int64_t il) {
    frame & f = *as_frame(fp);
    ggml_backend_buffer_type_t buft = buft_of(f, (uint32_t) il);
    auto it = f.ctxs.find(buft);
    if (it != f.ctxs.end()) return (int64_t) (intptr_t) it->second;
    ggml_init_params params = {
        size_t(2u * (1u + f.n_stream) * f.hp->n_layer_all * ggml_tensor_overhead()),
        nullptr,
        true,
    };
    ggml_context * ctx = ggml_init(params);
    if (!ctx) return 0;
    f.ctxs.emplace(buft, ctx);
    return (int64_t) (intptr_t) ctx;
}
int64_t q4_tensor3d(int64_t cp, int64_t kind, int64_t ne0, int64_t ne1, int64_t ne2,
                    int64_t tag, int64_t il, int64_t is_v) {
    if (!take_ok()) return 0;
    ggml_context * ctx = reinterpret_cast<ggml_context *>(static_cast<intptr_t>(cp));
    ggml_tensor * t = ggml_new_tensor_3d(ctx, (ggml_type) kind, ne0, ne1, ne2);
    if (!t) return 0;
    const char * name = reinterpret_cast<const char *>(static_cast<intptr_t>(tag));
    ggml_format_name(t, "cache_%s%s_l%d", name, is_v ? "v" : "k", (int) il);
    return (int64_t) (intptr_t) t;
}
int64_t q4_layer_add(int64_t kvp, int64_t il, int64_t kp, int64_t vp, int64_t cp,
                     int64_t nk, int64_t nv, int64_t n_stream) {
    llama_kv_cache * kv = as_kv(kvp);
    ggml_context * ctx = reinterpret_cast<ggml_context *>(static_cast<intptr_t>(cp));
    ggml_tensor * k = reinterpret_cast<ggml_tensor *>(static_cast<intptr_t>(kp));
    ggml_tensor * v = reinterpret_cast<ggml_tensor *>(static_cast<intptr_t>(vp));
    llama_kv_cache::kv_layer layer;
    layer.il = (uint32_t) il;
    layer.k = k;
    layer.v = v;
    for (int64_t s = 0; s < n_stream; ++s) {
        layer.k_stream.push_back(ggml_view_2d(ctx, k, nk, kv->v_cells[0].size(), k->nb[1], s * k->nb[2]));
        layer.v_stream.push_back(ggml_view_2d(ctx, v, nv, kv->v_cells[0].size(), v->nb[1], s * v->nb[2]));
    }
    kv->map_layer_ids[(int32_t) il] = (int32_t) kv->layers.size();
    kv->layers.push_back(std::move(layer));
    return 1;
}
void q4_set_head_all(int64_t kvp, int64_t k_all, int64_t v_all) {
    llama_kv_cache * kv = as_kv(kvp);
    kv->n_embd_head_k_all = (int32_t) k_all;
    kv->n_embd_head_v_all = (int32_t) v_all;
}
int64_t q4_finalize(int64_t kvp, int64_t fp) {
    llama_kv_cache * kv = as_kv(kvp);
    frame & f = *as_frame(fp);
    std::vector<std::pair<ggml_context *, ggml_backend_buffer_t>> made;
    for (auto & [buft, ctx] : f.ctxs) {
        if (!take_ok()) break;
        ggml_backend_buffer_t buf = nullptr;
        if (f.hp->no_alloc) {
            buf = ggml_backend_buft_alloc_buffer(buft, 0);
            if (buf) {
                for (ggml_tensor * t = ggml_get_first_tensor(ctx); t; t = ggml_get_next_tensor(ctx, t)) t->buffer = buf;
            }
        } else {
            buf = ggml_backend_alloc_ctx_tensors_from_buft(ctx, buft);
        }
        if (!buf) break;
        ggml_backend_buffer_clear(buf, 0);
        made.emplace_back(ctx, buf);
    }
    if (made.size() != f.ctxs.size()) {
        for (auto & [ctx, buf] : made) ggml_backend_buffer_free(buf);
        for (auto & [buft, ctx] : f.ctxs) ggml_free(ctx);
        f.ctxs.clear();
        return -1;
    }
    for (auto & [ctx, buf] : made) kv->ctxs_bufs.emplace_back(ggml_context_ptr(ctx), ggml_backend_buffer_ptr(buf));
    f.ctxs.clear();
    return 0;
}
void q4_copy_rot(int64_t kvp, int64_t fp) {
    llama_kv_cache * kv = as_kv(kvp);
    frame & f = *as_frame(fp);
    if (f.model == nullptr) return;
    // other is null for the Apex path; field copy kept for shared-cells rollback
    if (kv->other) {
        kv->n_embd_head_k_all = kv->other->n_embd_head_k_all;
        kv->n_embd_head_v_all = kv->other->n_embd_head_v_all;
        kv->n_rot_k = kv->other->n_rot_k;
        kv->n_rot_v = kv->other->n_rot_v;
        kv->attn_rot_hadamard = kv->other->attn_rot_hadamard;
    }
}
void q4_set_rot(int64_t kvp, int64_t nk, int64_t nv) {
    llama_kv_cache * kv = as_kv(kvp);
    kv->n_rot_k = (uint32_t) nk;
    kv->n_rot_v = (uint32_t) nv;
}
void q4_hadamard(int64_t kvp, int64_t n) {
    llama_kv_cache * kv = as_kv(kvp);
    if (n <= 0) return;
    std::vector<float> data((size_t) (n * n), 0.0f);
    data[0] = 1.0f / sqrtf((float) n);
    for (int64_t s = 1; s < n; s *= 2) {
        for (int64_t i = 0; i < s; ++i) {
            for (int64_t j = 0; j < s; ++j) {
                const float val = data[(size_t) (i * n + j)];
                data[(size_t) ((i + s) * n + j)] = val;
                data[(size_t) (i * n + (j + s))] = val;
                data[(size_t) ((i + s) * n + (j + s))] = -val;
            }
        }
    }
    kv->attn_rot_hadamard[n] = std::move(data);
}
void q4_clear_hadamard(int64_t kvp) { as_kv(kvp)->attn_rot_hadamard.clear(); }
void q4_release_impl(int64_t kvp) {
    llama_kv_cache * kv = as_kv(kvp);
    kv->ctxs_bufs.clear();
    kv->layers.clear();
    kv->map_layer_ids.clear();
}
void q4_witness(int64_t mode) { fg153_q4_witness_impl(mode); }

int fg153_q4_enabled(void) {
    const char * v = getenv("FG153_KV_Q4");
    return !v || strcmp(v, "0") != 0;
}
int fg153_q4_init(void * kvp, const llama_model * model, const llama_hparams * hp,
                  int tk, int tv, int offload, uint32_t kv_size, uint32_t n_stream,
                  const void * filter, const char * tag) {
    if (!fg153_q4_enabled()) return 0;
    frame f;
    f.model = model;
    f.hp = hp;
    f.tk = (ggml_type) tk;
    f.tv = (ggml_type) tv;
    f.offload = offload;
    f.kv_size = kv_size;
    f.n_stream = n_stream;
    f.filter = static_cast<const llama_kv_cache::layer_filter_cb *>(filter);
    f.tag = tag;
    const int64_t r = fg153_q4_entry(0, (int64_t) (intptr_t) kvp, (int64_t) (intptr_t) &f, 0);
    if (r == 0) {
        q4_release_impl((int64_t) (intptr_t) kvp);
        return -1;
    }
    llama_kv_cache * kv = as_kv((int64_t) (intptr_t) kvp);
    const int64_t h = fg_kv_attach_real(kv);
    if (h != 0) fg_kv_configure_real(h, kv);
    return 1;
}

} // extern "C"

llama_memory_context_ptr fg153_q4_context_batch(llama_kv_cache * kv,
        llama_kv_cache::slot_info_vec_t sinfos, std::vector<llama_ubatch> ubs) {
    const int64_t gkv = fg_kv_handle_for(kv);
    if (gkv == 0) return std::make_unique<llama_kv_cache_context>(kv, std::move(sinfos), std::move(ubs));
    const int64_t sv = fg_kv_obj(4);
    const int64_t uv = fg_kv_obj(4);
    for (const auto & si : sinfos) {
        const int64_t sh = fg_kv_obj(3);
        fg_kv_put(sh, 0, 0, si.s0);
        fg_kv_put(sh, 1, 0, si.s1);
        fg_kv_put(sh, 3, 0, (int64_t) si.n_stream());
        fg_kv_put(sh, 2, 0, si.empty() ? 0 : (int64_t) si.size());
        for (uint32_t x = 0; x < si.n_stream(); ++x) {
            fg_kv_put(sh, 10 + x, 0, si.strm[x]);
            for (size_t j = 0; j < si.idxs[x].size(); ++j) fg_kv_slot_push(sh, x, si.idxs[x][j]);
        }
        fg_kv_vec_push(sv, sh);
    }
    std::vector<llama_ubatch *> copies;
    for (const auto & u : ubs) {
        copies.push_back(new llama_ubatch(u));
        fg_kv_vec_push(uv, (int64_t) (intptr_t) copies.back());
    }
    const int64_t gctx = fg_goose_kv_lifecycle_entry(19, gkv, sv, uv, 0, 0, 0);
    fg_kv_destroy(sv);
    fg_kv_destroy(uv);
    if (gctx == 0) {
        for (auto * c : copies) delete c;
        return std::make_unique<llama_kv_cache_context>(LLAMA_MEMORY_STATUS_FAILED_PREPARE);
    }
    return fg_goose_context_batch(kv, gkv, gctx);
}

extern "C" {
void fg153_q4_release(void * kvp) {
    if (!fg153_q4_enabled()) return;
    fg153_q4_entry(1, (int64_t) (intptr_t) kvp, 0, 0);
}
}
