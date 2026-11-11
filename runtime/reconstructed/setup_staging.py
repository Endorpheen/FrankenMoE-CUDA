#!/usr/bin/env python3
"""Rebuild the staged tree from scratch: flint src + selected integration copies + our KV call sites."""
from pathlib import Path
import shutil, subprocess, sys
BASE = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
FL = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
SRC = BASE/'runtime/reconstructed/src'

if SRC.exists(): shutil.rmtree(SRC)
shutil.copytree(FL/'src', SRC)
subprocess.run(['python3', str(BASE/'runtime/reconstructed/overlay.py')], check=True)

p = SRC/'llama-kv-cache.cpp'; src = p.read_text()
a1='void llama_kv_cache::state_write(llama_io_write_i & io, llama_seq_id seq_id, llama_state_seq_flags flags) const {'
decl1='''// Goose state-write range selection (EXP345/EXP347). GGML_GOOSE_KV_STATE_WRITE=0 restores the native block.
extern "C" int fg345w_enabled(void);
extern "C" int64_t fg345w_select(int64_t other, int64_t partial, int64_t swa_none, int64_t seq_id,
                                 int64_t n_stream, int64_t n_swa, int64_t swa_type, int64_t size,
                                 const int32_t * pos, const uint64_t * seqmask, int32_t * out_counts,
                                 int32_t * out_ranges, int64_t out_ranges_cap);

'''
src = src.replace(a1, decl1 + a1, 1)
old1='''        const auto & cells = v_cells[s];

        // Count the number of cells with the specified seq_id
        // Find all the ranges of cells with this seq id (or all, when -1)
        uint32_t cell_range_begin = cells.size();'''
new1='''        const auto & cells = v_cells[s];

        if (fg345w_enabled() && n_seq_max <= 64) {
            const uint32_t n = cells.size();
            std::vector<int32_t> pos(n, -1);
            std::vector<uint64_t> mask(n, 0);
            for (uint32_t i = 0; i < n; ++i) {
                if (cells.is_empty(i)) continue;
                pos[i] = (int32_t) cells.pos_get(i);
                uint64_t m = 0;
                for (llama_seq_id q = 0; q < (llama_seq_id) n_seq_max; ++q) if (cells.seq_has(i, q)) m |= (1ULL << q);
                mask[i] = m;
            }
            int32_t count_out = 0;
            std::vector<int32_t> ranges(2 * (n + 1), -1);
            const int64_t n_ranges = fg345w_select(0, 0, 0, (int64_t) seq_id, 1,
                                                   (int64_t) n_swa, (int64_t) swa_type, (int64_t) n,
                                                   pos.data(), mask.data(), &count_out, ranges.data(),
                                                   (int64_t) ranges.size()/2);
            if (n_ranges < 0) return;
            cell_count = (uint32_t) count_out;
            for (int64_t r = 0; r < n_ranges; ++r) cr.data.emplace_back((uint32_t) ranges[r*2], (uint32_t) ranges[r*2+1]);
            uint32_t chk = 0;
            for (const auto & range : cr.data) chk += range.second - range.first;
            GGML_ASSERT(cell_count == chk);
            io.write(&cell_count, sizeof(cell_count));
            if (cell_count == 0) continue;
            state_write_meta(io, cr, seq_id);
            state_write_data(io, cr);
            continue;
        }

        // Count the number of cells with the specified seq_id
        // Find all the ranges of cells with this seq id (or all, when -1)
        uint32_t cell_range_begin = cells.size();'''
assert old1 in src
src = src.replace(old1, new1, 1)

a2='void llama_kv_cache::state_clear(llama_seq_id seq_id, uint32_t strm, const slot_info & sinfo) {'
decl2='''// Goose KV state clearing plan (EXP367). GGML_GOOSE_KV_STATE_CLEAR=0 restores the native loops.
extern "C" int fg367s_enabled(void);
extern "C" int64_t fg367s_clear(int64_t n_idx, const int32_t * idxs, int64_t head, int64_t cells_size,
                                int64_t n_layer, const int32_t * k_row, const int32_t * v_present,
                                const int32_t * v_row, int64_t v_trans, int64_t v_el, int64_t contig,
                                int32_t * out_kind, int32_t * out_il, int64_t * out_off, int64_t * out_size,
                                int64_t cap);

'''
src = src.replace(a2, decl2 + a2, 1)
old2='    const bool is_contiguous = sinfo.is_contiguous();\n'
new2='''    const bool is_contiguous = sinfo.is_contiguous();

    if (fg367s_enabled() && layers.size() <= 64) {
        const int64_t n_lay = (int64_t) layers.size();
        std::vector<int32_t> g_idx(cell_count, 0), g_krow((size_t) n_lay, 0), g_vp((size_t) n_lay, 0), g_vrow((size_t) n_lay, 0);
        int64_t g_vel = 1;
        if (v_trans && !layers.empty() && layers[0].v_stream[strm]) g_vel = (int64_t) ggml_type_size(layers[0].v_stream[strm]->type);
        for (uint32_t i = 0; i < cell_count; ++i) g_idx[i] = (int32_t) sinfo.idxs[0][i];
        for (int64_t il = 0; il < n_lay; ++il) {
            const auto & layer = layers[(size_t) il];
            const uint32_t n_embd_k_gqa = hparams.n_embd_k_gqa(layer.il);
            g_krow[(size_t) il] = (int32_t) ggml_row_size(layer.k_stream[strm]->type, n_embd_k_gqa);
            if (layer.v_stream[strm]) {
                const uint32_t n_embd_v_gqa = hparams.n_embd_v_gqa(layer.il);
                g_vp[(size_t) il] = 1;
                g_vrow[(size_t) il] = (int32_t) (!v_trans ? ggml_row_size(layer.v_stream[strm]->type, n_embd_v_gqa) : n_embd_v_gqa);
            }
        }
        const int64_t cap = (int64_t) cell_count * n_lay * 64 + 64;
        std::vector<int32_t> g_kind((size_t) cap, -1), g_il((size_t) cap, -1);
        std::vector<int64_t> g_off((size_t) cap, -1), g_size((size_t) cap, -1);
        const int64_t n_ops = fg367s_clear((int64_t) cell_count, g_idx.data(), (int64_t) sinfo.head(),
                                           (int64_t) cells.size(), n_lay, g_krow.data(), g_vp.data(),
                                           g_vrow.data(), v_trans ? 1 : 0, g_vel, is_contiguous ? 1 : 0,
                                           g_kind.data(), g_il.data(), g_off.data(), g_size.data(), cap);
        for (int64_t op = 0; op < n_ops; ++op) {
            const int64_t il = g_il[(size_t) op];
            if (il < 0 || il >= n_lay) continue;
            const auto & layer = layers[(size_t) il];
            ggml_tensor * t = g_kind[(size_t) op] == 0 ? layer.k_stream[strm] : layer.v_stream[strm];
            if (!t || g_size[(size_t) op] <= 0) continue;
            llama_clear_tensor_data(t, (size_t) g_off[(size_t) op], (size_t) g_size[(size_t) op]);
        }
        return;
    }
'''
assert old2 in src
src = src.replace(old2, new2, 1)

# the lost bridge: the only preserved revision has a different signature
src = src.replace('        if (fg205g_enabled()) {',
                  '        // bridge revision lost (EXP205 signature mismatch); native walk used on this host\n        if (false && fg205g_enabled()) {', 1)
# read-orchestration guards (EXP373)
decl3='''// Goose KV read-orchestration guards (EXP373). GGML_GOOSE_KV_READ_SINFO=0 restores the native checks.
extern "C" int fg373s_enabled(void);
extern "C" int64_t fg373s_guard(int64_t op, int64_t a, int64_t b, int64_t c);

'''
anchor3='void llama_kv_cache::state_read_sinfo('
src = src.replace(anchor3, decl3 + anchor3, 1)

old_m='''    if (sinfos_in && sinfos_in->size() != n_stream) {
        throw std::runtime_error("failed to restore kv cache: mirrored slot layout has the wrong stream count");
    }'''
new_m='''    if (fg373s_enabled()) {
        if (fg373s_guard(0, sinfos_in ? 1 : 0, sinfos_in ? (int64_t) sinfos_in->size() : 0, (int64_t) n_stream) < 0) {
            throw std::runtime_error("failed to restore kv cache: mirrored slot layout has the wrong stream count");
        }
    } else if (sinfos_in && sinfos_in->size() != n_stream) {
        throw std::runtime_error("failed to restore kv cache: mirrored slot layout has the wrong stream count");
    }'''
assert old_m in src
src = src.replace(old_m, new_m, 1)

old_n='''    io.read(&n_stream_cur, sizeof(n_stream_cur));
    if (n_stream_cur != n_stream) {
        throw std::runtime_error("n_stream mismatch");
    }'''
new_n='''    io.read(&n_stream_cur, sizeof(n_stream_cur));
    if (fg373s_enabled()) {
        if (fg373s_guard(1, (int64_t) n_stream_cur, (int64_t) n_stream, 0) < 0) {
            throw std::runtime_error("n_stream mismatch");
        }
    } else if (n_stream_cur != n_stream) {
        throw std::runtime_error("n_stream mismatch");
    }'''
assert old_n in src
src = src.replace(old_n, new_n, 1)

old_z='''            if (sinfos_in && !(*sinfos_in)[s].empty()) {
                throw std::runtime_error("failed to restore kv cache: mirrored cache holds cells this one does not");
            }'''
new_z='''            if (fg373s_enabled()) {
                if (fg373s_guard(2, 0, sinfos_in ? ((*sinfos_in)[s].empty() ? 1 : 0) : 1, 0) < 0) {
                    throw std::runtime_error("failed to restore kv cache: mirrored cache holds cells this one does not");
                }
            } else if (sinfos_in && !(*sinfos_in)[s].empty()) {
                throw std::runtime_error("failed to restore kv cache: mirrored cache holds cells this one does not");
            }'''
assert old_z in src
src = src.replace(old_z, new_z, 1)

# hybrid memory decisions (EXP375)
h = SRC/'llama-memory-hybrid.cpp'; hsrc = h.read_text()
decl4='''// Goose hybrid memory decisions (EXP375). GGML_GOOSE_HYBRID_MEMORY=0 restores the native expressions.
extern "C" int fg375s_enabled(void);
extern "C" int64_t fg375s_decide(int64_t op, int64_t a, int64_t b);

'''
anchor4='llama_pos llama_memory_hybrid::seq_pos_min(llama_seq_id seq_id) const {'
hsrc = hsrc.replace(anchor4, decl4 + anchor4, 1)
hsrc = hsrc.replace('''    // the min of the total cache is the max of the two caches' min values
    return std::max(mem_attn->seq_pos_min(seq_id), mem_recr->seq_pos_min(seq_id));''','''    // the min of the total cache is the max of the two caches' min values
    const llama_pos a = mem_attn->seq_pos_min(seq_id);
    const llama_pos b = mem_recr->seq_pos_min(seq_id);
    if (fg375s_enabled()) return (llama_pos) fg375s_decide(0, a, b);
    return std::max(a, b);''',1)
hsrc = hsrc.replace('''    // the max of the total cache is the min of the two caches' max values
    return std::min(mem_attn->seq_pos_max(seq_id), mem_recr->seq_pos_max(seq_id));''','''    // the max of the total cache is the min of the two caches' max values
    const llama_pos a = mem_attn->seq_pos_max(seq_id);
    const llama_pos b = mem_recr->seq_pos_max(seq_id);
    if (fg375s_enabled()) return (llama_pos) fg375s_decide(1, a, b);
    return std::min(a, b);''',1)
old_catch='''    } catch (...) {
        // a failed restore leaves the sequence empty in both caches
        mem_attn->state_clear(seq_id);

        throw;
    }'''
new_catch='''    } catch (...) {
        // a failed restore leaves the sequence empty in both caches
        const bool attn_has = mem_attn->seq_pos_max(seq_id) >= 0;
        const bool go = fg375s_enabled() ? fg375s_decide(2, attn_has ? 1 : 0, 0) != 0 : attn_has;
        if (go) mem_attn->state_clear(seq_id);

        throw;
    }'''
assert old_catch in hsrc
hsrc = hsrc.replace(old_catch, new_catch, 1)
h.write_text(hsrc)

# hybrid context decisions (EXP376)
hsrc = h.read_text()
decl5='''// Goose hybrid context decisions (EXP376). GGML_GOOSE_HYBRID_CONTEXT=0 restores the native expressions.
extern "C" int fg376s_enabled(void);
extern "C" int64_t fg376s_decide(int64_t op, int64_t a, int64_t b);

'''
anchor5='bool llama_memory_hybrid_context::apply() {'
hsrc = hsrc.replace(anchor5, decl5 + anchor5, 1)
hsrc = hsrc.replace('''    bool res = true;

    res = res & ctx_attn->apply();
    res = res & ctx_recr->apply();

    return res;''','''    const bool attn = ctx_attn->apply();
    const bool recr = ctx_recr->apply();
    if (fg376s_enabled()) return fg376s_decide(0, attn ? 1 : 0, recr ? 1 : 0) != 0;
    return attn && recr;''',1)
old_next='''    if (++i_next >= ubatches.size()) {
        return false;
    }

    return true;'''
new_next='''    const uint32_t at_i = i_next + 1;
    if (fg376s_enabled()) {
        if (fg376s_decide(1, (int64_t) i_next, (int64_t) ubatches.size()) != 0) {
            i_next = at_i;
            return false;
        }
        i_next = at_i;
        return true;
    }

    if (at_i >= ubatches.size()) {
        i_next = at_i;
        return false;
    }

    i_next = at_i;
    return true;'''
assert old_next in hsrc
hsrc = hsrc.replace(old_next, new_next, 1)
h.write_text(hsrc)

# aligned-read plan (EXP377)
m = SRC/'llama-mmap.cpp'; msrc = m.read_text()
decl6='''// Goose aligned-read arithmetic (EXP377). GGML_GOOSE_FILE_ALIGN=0 restores the native expressions.
extern "C" int fg377s_enabled(void);
extern "C" int64_t fg377s_plan(int64_t offset, int64_t size, int64_t alignment, int64_t cap, int64_t * out);

'''
anchor6='    void read_aligned_chunk(void * dest, size_t size) {'
assert anchor6 in msrc
msrc = msrc.replace(anchor6, decl6 + anchor6, 1)
old_al='''        size_t offset = tell();
        off_t aligned_offset = offset & ~(alignment - 1);
        off_t offset_from_alignment = offset - aligned_offset;
        size_t bytes_to_read = (offset_from_alignment + size + alignment - 1) & ~(alignment - 1);

        // stage through a bounded buffer, so that a large tensor is not held in memory twice while it loads
        const size_t buffer_size = std::min<size_t>(bytes_to_read, LLAMA_DIRECT_IO_BUFFER_SIZE);'''
new_al='''        size_t offset = tell();
        size_t aligned_offset = 0, offset_from_alignment = 0, bytes_to_read = 0, buffer_size = 0;
        if (fg377s_enabled()) {
            int64_t plan[4] = {0, 0, 0, 0};
            fg377s_plan((int64_t) offset, (int64_t) size, (int64_t) alignment,
                        (int64_t) LLAMA_DIRECT_IO_BUFFER_SIZE, plan);
            aligned_offset        = (size_t) plan[0];
            bytes_to_read         = (size_t) plan[1];
            offset_from_alignment = (size_t) plan[2];
            buffer_size           = (size_t) plan[3];
        } else {
            aligned_offset = offset & ~(alignment - 1);
            offset_from_alignment = offset - aligned_offset;
            bytes_to_read = (offset_from_alignment + size + alignment - 1) & ~(alignment - 1);
            // stage through a bounded buffer, so that a large tensor is not held in memory twice while it loads
            buffer_size = std::min<size_t>(bytes_to_read, LLAMA_DIRECT_IO_BUFFER_SIZE);
        }'''
assert old_al in msrc
msrc = msrc.replace(old_al, new_al, 1)
m.write_text(msrc)

# penalty bookkeeping (EXP380)
smp = SRC/'llama-sampler.cpp'; smpsrc = smp.read_text()
decl7='''// Goose penalty bookkeeping (EXP380). GGML_GOOSE_PENALTY_ACCEPT=0 restores the native control flow.
extern "C" int fg380s_enabled(void);
extern "C" int64_t fg380s_decide(int64_t op, int64_t a, int64_t b);

'''
anchor7='static void llama_sampler_penalties_accept(struct llama_sampler * smpl, llama_token token) {'
assert anchor7 in smpsrc
smpsrc = smpsrc.replace(anchor7, decl7 + anchor7, 1)
old_skip='''    if (ctx->penalty_last_n == 0) {
        return;
    }

    ctx->token_count[token]++;

    // if the ring buffer is full, remove the oldest token
    if (ctx->prev.size() >= (size_t) ctx->penalty_last_n) {'''
new_skip='''    const bool g_on = fg380s_enabled();
    if (g_on ? fg380s_decide(0, 0, (int64_t) ctx->penalty_last_n) != 0
             : ctx->penalty_last_n == 0) {
        return;
    }

    ctx->token_count[token]++;

    // if the ring buffer is full, remove the oldest token
    if (g_on ? fg380s_decide(1, (int64_t) ctx->prev.size(), (int64_t) ctx->penalty_last_n) != 0
             : ctx->prev.size() >= (size_t) ctx->penalty_last_n) {'''
assert old_skip in smpsrc
smpsrc = smpsrc.replace(old_skip, new_skip, 1)
old_drop='''        ctx->token_count[old]--;
        if (ctx->token_count[old] == 0) {
            ctx->token_count.erase(old);
        }'''
new_drop='''        ctx->token_count[old]--;
        const bool drop = g_on ? fg380s_decide(2, (int64_t) ctx->token_count[old], 0) != 0
                               : ctx->token_count[old] == 0;
        if (drop) {
            ctx->token_count.erase(old);
        }'''
assert old_drop in smpsrc
smpsrc = smpsrc.replace(old_drop, new_drop, 1)
smp.write_text(smpsrc)

# KV whole-cache / meta-due / hybrid-drop decisions (EXP381)
decl8='''// Goose KV whole-cache and meta decisions (EXP381). GGML_GOOSE_KV_WHOLE_CLEAR=0 restores the native tests.
extern "C" int fg381s_enabled(void);
extern "C" int64_t fg381s_decide(int64_t op, int64_t a);

'''
src = src.replace('// Goose state-write range selection (EXP345/EXP347).', decl8 + '// Goose state-write range selection (EXP345/EXP347).', 1)
old_clear='''    if (seq_id == -1) {
        clear(true);
    }'''
new_clear='''    if (fg381s_enabled() ? fg381s_decide(0, (int64_t) seq_id) != 0 : seq_id == -1) {
        clear(true);
    }'''
assert old_clear in src
src = src.replace(old_clear, new_clear, 1)
old_meta='''            io.write(&cell_count, sizeof(cell_count));
            if (cell_count == 0) continue;
            state_write_meta(io, cr, seq_id);'''
new_meta='''            io.write(&cell_count, sizeof(cell_count));
            const bool meta_due = fg381s_enabled() ? fg381s_decide(1, (int64_t) cell_count) != 0
                                                   : cell_count != 0;
            if (!meta_due) continue;
            state_write_meta(io, cr, seq_id);'''
assert old_meta in src
src = src.replace(old_meta, new_meta, 1)

hidx = SRC/'llama-memory-hybrid-idx.cpp'; hx = hidx.read_text()
hx = hx.replace('void llama_memory_hybrid_idx::state_drop(llama_seq_id seq_id) {', decl8 + 'void llama_memory_hybrid_idx::state_drop(llama_seq_id seq_id) {', 1)
old_drop='''    if (seq_id < 0) {
        clear(true);

        return;
    }'''
new_drop='''    if (fg381s_enabled() ? fg381s_decide(2, (int64_t) seq_id) != 0 : seq_id < 0) {
        clear(true);

        return;
    }'''
assert old_drop in hx
hx = hx.replace(old_drop, new_drop, 1)
hidx.write_text(hx)

# recurrent I/O plan (EXP382)
rec = SRC/'llama-memory-recurrent.cpp'; rsrc = rec.read_text()
decl9='''// Goose recurrent I/O plan (EXP382). GGML_GOOSE_RECURRENT_IO_PLAN=0 restores the native null checks.
extern "C" int fg382s_enabled(void);
extern "C" int64_t fg382s_decide(int64_t op, int64_t a);

'''
anchor9='void llama_memory_recurrent::state_write_data('
assert anchor9 in rsrc
rsrc = rsrc.replace(anchor9, decl9 + anchor9, 1)
old_skip='''        if (r_l[il] == nullptr) continue;'''
new_skip='''        if (fg382s_enabled() ? fg382s_decide(0, r_l[il] ? 1 : 0) != 0 : r_l[il] == nullptr) continue;'''
assert old_skip in rsrc
rsrc = rsrc.replace(old_skip, new_skip, 1)
old_s='''        if (s_l[il] == nullptr) continue;'''
new_s='''        if (fg382s_enabled() ? fg382s_decide(1, s_l[il] ? 1 : 0) == 0 : s_l[il] == nullptr) continue;'''
if old_s in rsrc:
    rsrc = rsrc.replace(old_s, new_s, 1)
rec.write_text(rsrc)

# ubatch container plan (EXP383)
bch = SRC/'llama-batch.cpp'; bsrc = bch.read_text()
decl10='''// Goose ubatch container plan (EXP383). GGML_GOOSE_BATCH_CONTAINER_PLAN=0 restores the native sizing.
extern "C" int fg383s_enabled(void);
extern "C" int64_t fg383s_decide(int64_t op, int64_t a, int64_t b);

'''
anchor10='llama_ubatch llama_batch_allocr::ubatch_add('
assert anchor10 in bsrc
bsrc = bsrc.replace(anchor10, decl10 + anchor10, 1)
old_cont='''    udata->type      .resize(mixed ? n_tokens : 0);
    udata->decision_order.resize(decision_order.empty() ? 0 : n_tokens);'''
new_cont='''    const bool g_cont = fg383s_enabled();
    udata->type      .resize(g_cont ? (size_t) fg383s_decide(0, mixed ? 1 : 0, (int64_t) n_tokens)
                                    : (mixed ? n_tokens : 0));
    udata->decision_order.resize(g_cont ? (size_t) fg383s_decide(1, decision_order.empty() ? 1 : 0, (int64_t) n_tokens)
                                        : (decision_order.empty() ? 0 : n_tokens));'''
assert old_cont in bsrc
bsrc = bsrc.replace(old_cont, new_cont, 1)
old_res='''    udata->seq_id_data.reserve(n_tokens);'''
new_res='''    udata->seq_id_data.reserve(g_cont ? (size_t) fg383s_decide(2, 0, (int64_t) n_tokens) : (size_t) n_tokens);'''
assert old_res in bsrc
bsrc = bsrc.replace(old_res, new_res, 1)
bch.write_text(bsrc)

# sampler softmax (EXP384)
smp2 = SRC/'llama-sampler.cpp'; ssrc = smp2.read_text()
decl11='''// Goose sampler softmax (EXP384). GGML_GOOSE_SAMPLER_SOFTMAX=0 restores the native normaliser.
extern "C" int fg384s_enabled(void);
extern "C" int64_t fg384s_softmax(const float * logits, int64_t n, int32_t sorted, float * out);

'''
anchor11='static void llama_sampler_softmax_impl(llama_token_data_array * cur_p, bool do_sort) {'
assert anchor11 in ssrc
ssrc = ssrc.replace(anchor11, decl11 + anchor11, 1)
old_soft='''    float max_l = cur_p->data[0].logit;
    if (!cur_p->sorted) {
        for (size_t i = 1; i < cur_p->size; ++i) {
            max_l = std::max(max_l, cur_p->data[i].logit);
        }
    }

    float cum_sum = 0.0f;

    for (size_t i = 0; i < cur_p->size; ++i) {
        float p = expf(cur_p->data[i].logit - max_l);
        cur_p->data[i].p = p;
        cum_sum += p;
    }

    for (size_t i = 0; i < cur_p->size; ++i) {
        cur_p->data[i].p /= cum_sum;
    }'''
new_soft='''    if (fg384s_enabled()) {
        // the entries are strided, so the bridge gets flat copies; float copies are exact
        std::vector<float> l(cur_p->size), o(cur_p->size);
        for (size_t i = 0; i < cur_p->size; ++i) l[i] = cur_p->data[i].logit;
        fg384s_softmax(l.data(), (int64_t) cur_p->size, cur_p->sorted ? 1 : 0, o.data());
        for (size_t i = 0; i < cur_p->size; ++i) cur_p->data[i].p = o[i];
        return;
    }

    float max_l = cur_p->data[0].logit;
    if (!cur_p->sorted) {
        for (size_t i = 1; i < cur_p->size; ++i) {
            max_l = std::max(max_l, cur_p->data[i].logit);
        }
    }

    float cum_sum = 0.0f;

    for (size_t i = 0; i < cur_p->size; ++i) {
        float p = expf(cur_p->data[i].logit - max_l);
        cur_p->data[i].p = p;
        cum_sum += p;
    }

    for (size_t i = 0; i < cur_p->size; ++i) {
        cur_p->data[i].p /= cum_sum;
    }'''
assert old_soft in ssrc
ssrc = ssrc.replace(old_soft, new_soft, 1)
smp2.write_text(ssrc)

# sampler guards (EXP385)
smp3 = SRC/'llama-sampler.cpp'; gsrc = smp3.read_text()
decl12='''// Goose sampler early-exit guards (EXP385). GGML_GOOSE_SAMPLER_GUARDS=0 restores the native tests.
extern "C" int fg385s_enabled(void);
extern "C" int64_t fg385s_decide(int64_t op, int64_t a, int64_t b);

'''
anchor12='// Goose sampler softmax (EXP384).'
assert anchor12 in gsrc
gsrc = gsrc.replace(anchor12, decl12 + anchor12, 1)

old_minp='''    if (ctx->p <= 0.0f || !cur_p->size) {
        return;
    }'''
new_minp='''    if (fg385s_enabled() ? fg385s_decide(0, (int64_t) (ctx->p * 1000000.0f), (int64_t) cur_p->size) != 0
                         : (ctx->p <= 0.0f || !cur_p->size)) {
        return;
    }'''
assert old_minp in gsrc
gsrc = gsrc.replace(old_minp, new_minp, 1)

old_topk='''    if (k <= 0) {
        return;
    }'''
new_topk='''    if (fg385s_enabled() ? fg385s_decide(3, (int64_t) k, 0) != 0 : k <= 0) {
        return;
    }'''
assert old_topk in gsrc
gsrc = gsrc.replace(old_topk, new_topk, 1)
smp3.write_text(gsrc)

# sampler selection (EXP386): the single-pass algorithm this line actually uses
decl13='''// Goose sampler selection (EXP386). GGML_GOOSE_SAMPLER_DIST=0 restores the native single pass.
extern "C" int fg386s_enabled(void);
extern "C" int64_t fg386s_pick(const float * logits, int64_t n, int32_t sorted, float * out, double rnd);

'''
anchor13='static void llama_sampler_dist_apply(struct llama_sampler * smpl, llama_token_data_array * cur_p) {'
assert anchor13 in gsrc
gsrc = gsrc.replace(anchor13, decl13 + anchor13, 1)
old_dist='''    // max logit for numerical stability
    float max_l = cur_p->data[0].logit;'''
new_dist='''    if (fg386s_enabled()) {
        // one draw, exactly as the native path takes it; the decision and both passes move to Goose
        const double rnd = dist(ctx->rng);
        std::vector<float> lo(cur_p->size), po(cur_p->size);
        for (size_t i = 0; i < cur_p->size; ++i) lo[i] = cur_p->data[i].logit;
        const int sel = (int) fg386s_pick(lo.data(), (int64_t) cur_p->size, cur_p->sorted ? 1 : 0,
                                          po.data(), rnd);
        for (size_t i = 0; i < cur_p->size; ++i) cur_p->data[i].p = po[i];
        cur_p->selected = sel;
        return;
    }

    // max logit for numerical stability
    float max_l = cur_p->data[0].logit;'''
assert old_dist in gsrc
gsrc = gsrc.replace(old_dist, new_dist, 1)
smp3.write_text(gsrc)

# sampler chain (EXP387)
decl14='''// Goose sampler chain orchestration (EXP387). GGML_GOOSE_SAMPLER_CHAIN=0 restores the native walk.
extern "C" int fg387s_enabled(void);
extern "C" int64_t fg387s_decide(int64_t op, int64_t a, int64_t b);

'''
anchor14='static void llama_sampler_chain_accept(struct llama_sampler * smpl, llama_token token) {'
assert anchor14 in gsrc
gsrc = gsrc.replace(anchor14, decl14 + anchor14, 1)
old_bk='''        if (is_backend && smpl.is_backend) {
            continue;
        }

        is_backend = false;

        if (smpl.ptr->iface->apply == nullptr) {
            continue;
        }'''
new_bk='''        if (fg387s_enabled() ? fg387s_decide(0, is_backend ? 1 : 0, smpl.is_backend ? 1 : 0) != 0
                             : (is_backend && smpl.is_backend)) {
            continue;
        }

        is_backend = false;

        if (fg387s_enabled() ? fg387s_decide(1, smpl.ptr->iface->apply ? 1 : 0, 0) != 0
                             : smpl.ptr->iface->apply == nullptr) {
            continue;
        }'''
assert old_bk in gsrc
gsrc = gsrc.replace(old_bk, new_bk, 1)
old_cnt='''    chain->n_sample++;'''
new_cnt='''    chain->n_sample = fg387s_enabled() ? (uint32_t) fg387s_decide(3, (int64_t) chain->n_sample, 0)
                                       : chain->n_sample + 1;'''
assert old_cnt in gsrc
gsrc = gsrc.replace(old_cnt, new_cnt, 1)
smp3.write_text(gsrc)

# sampler filter guards (EXP388)
decl15='''// Goose sampler filter guards (EXP388). GGML_GOOSE_SAMPLER_FILTERS=0 restores the native tests.
extern "C" int fg388s_enabled(void);
extern "C" int64_t fg388s_decide(int64_t op, int64_t a, int64_t b);

'''
anchor15='// Goose sampler chain orchestration (EXP387).'
assert anchor15 in gsrc
gsrc = gsrc.replace(anchor15, decl15 + anchor15, 1)
old_dry='''    if (ctx->dry_multiplier == 0.0f || ctx->dry_base < 1.0f || ctx->dry_penalty_last_n == 0) {'''
new_dry='''    if (fg388s_enabled() ? (fg388s_decide(0, (int64_t) (ctx->dry_multiplier * 1000000.0f), (int64_t) ctx->dry_penalty_last_n) != 0
                            || fg388s_decide(1, (int64_t) (ctx->dry_base * 1000000.0f), 0) != 0)
                         : (ctx->dry_multiplier == 0.0f || ctx->dry_base < 1.0f || ctx->dry_penalty_last_n == 0)) {'''
assert old_dry in gsrc
gsrc = gsrc.replace(old_dry, new_dry, 1)
old_sigma='''    if (ctx->n <= 0.0f || cur_p->size <= 1) {'''
new_sigma='''    if (fg388s_enabled() ? fg388s_decide(2, (int64_t) (ctx->n * 1000000.0f), (int64_t) cur_p->size) != 0
                         : (ctx->n <= 0.0f || cur_p->size <= 1)) {'''
assert old_sigma in gsrc
gsrc = gsrc.replace(old_sigma, new_sigma, 1)
old_xtc='''    if (ctx->probability <= 0.0f
        || ctx->threshold > 0.5f
        || cur_p->size < 2) {'''
new_xtc='''    if (fg388s_enabled() ? (fg388s_decide(3, (int64_t) (ctx->probability * 1000000.0f), (int64_t) cur_p->size) != 0
                            || fg388s_decide(4, (int64_t) (ctx->threshold * 1000000.0f), 0) != 0)
                         : (ctx->probability <= 0.0f || ctx->threshold > 0.5f || cur_p->size < 2)) {'''
assert old_xtc in gsrc
gsrc = gsrc.replace(old_xtc, new_xtc, 1)
smp3.write_text(gsrc)

# GGUF inventory (EXP389)
ldr = SRC/'llama-model-loader.cpp'; lsrc = ldr.read_text()
decl16='''// Goose GGUF inventory decisions (EXP389). GGML_GOOSE_GGUF_INVENTORY=0 restores the native walk.
extern "C" int fg389s_enabled(void);
extern "C" int64_t fg389s_decide(int64_t op, int64_t a, int64_t b, int64_t c, int64_t * out2);

'''
anchor16='extern "C" int fg201l_shape_check('
assert anchor16 in lsrc
lsrc = lsrc.replace(anchor16, decl16 + anchor16, 1)
old_dup='''            if (weights_map.find(tensor_name) != weights_map.end()) {
                throw std::runtime_error(format("invalid model: tensor '%s' is duplicated", ggml_get_name(cur)));
            }'''
new_dup='''            if (fg389s_enabled() ? fg389s_decide(0, weights_map.find(tensor_name) != weights_map.end() ? 1 : 0, 0, 0, nullptr) != 0
                                 : weights_map.find(tensor_name) != weights_map.end()) {
                throw std::runtime_error(format("invalid model: tensor '%s' is duplicated", ggml_get_name(cur)));
            }'''
assert old_dup in lsrc
lsrc = lsrc.replace(old_dup, new_dup, 1)
old_dom='''            if (n_type_max < n_type[type]) {
                n_type_max = n_type[type];
                type_max   = type;
            }'''
new_dom='''            if (fg389s_enabled()) {
                int64_t g2 = 0;
                const int64_t g_type = fg389s_decide(1, (int64_t) type, (int64_t) n_type[type], (int64_t) n_type_max, &g2);
                if (g_type >= 0) { type_max = (enum ggml_type) g_type; n_type_max = (uint32_t) g2; }
            } else if (n_type_max < n_type[type]) {
                n_type_max = n_type[type];
                type_max   = type;
            }'''
assert old_dom in lsrc
lsrc = lsrc.replace(old_dom, new_dom, 1)
ldr.write_text(lsrc)

# vocab partition (EXP390)
voc = SRC/'llama-vocab.cpp'; vsrc = voc.read_text()
decl17='''// Goose special-token partition decisions (EXP390). GGML_GOOSE_VOCAB_PARTITION=0 restores the native tests.
extern "C" int fg390s_enabled(void);
extern "C" int64_t fg390s_decide(int64_t op, int64_t a, int64_t b);

'''
anchor17='extern "C" int fg202t_enabled(void);'
assert anchor17 in vsrc
vsrc = vsrc.replace(anchor17, decl17 + anchor17, 1)
old_pt='''        if (!parse_special && (data.attr & (LLAMA_TOKEN_ATTR_CONTROL | LLAMA_TOKEN_ATTR_UNKNOWN))) {'''
new_pt='''        const bool g_vocab = fg390s_enabled();
        if (g_vocab ? fg390s_decide(0, parse_special ? 1 : 0,
                                    (data.attr & (LLAMA_TOKEN_ATTR_CONTROL | LLAMA_TOKEN_ATTR_UNKNOWN)) ? 1 : 0) != 0
                    : (!parse_special && (data.attr & (LLAMA_TOKEN_ATTR_CONTROL | LLAMA_TOKEN_ATTR_UNKNOWN)))) {'''
assert old_pt in vsrc
vsrc = vsrc.replace(old_pt, new_pt, 1)
old_l='''                        if (data.attr & LLAMA_TOKEN_ATTR_LSTRIP) {'''
new_l='''                        const bool g_lstrip = g_vocab ? fg390s_decide(1, (data.attr & LLAMA_TOKEN_ATTR_LSTRIP) ? 1 : 0,
                                                                     left_reminder_length) != 0
                                                     : (data.attr & LLAMA_TOKEN_ATTR_LSTRIP);
                        if (g_lstrip) {'''
assert old_l in vsrc
vsrc = vsrc.replace(old_l, new_l, 1)
old_r='''                        if (data.attr & LLAMA_TOKEN_ATTR_RSTRIP) {'''
new_r='''                        const bool g_rstrip = g_vocab ? fg390s_decide(2, (data.attr & LLAMA_TOKEN_ATTR_RSTRIP) ? 1 : 0,
                                                                     right_reminder_length) != 0
                                                     : (data.attr & LLAMA_TOKEN_ATTR_RSTRIP);
                        if (g_rstrip) {'''
assert old_r in vsrc
vsrc = vsrc.replace(old_r, new_r, 1)
voc.write_text(vsrc)

# fused-op resolution (EXP391)
ctx = SRC/'llama-context.cpp'; csrc = ctx.read_text()
decl18='''// Goose fused-op resolution (EXP391). GGML_GOOSE_FUSED_OPS_RESOLVE=0 restores the native decisions.
extern "C" int fg391s_enabled(void);
extern "C" int64_t fg391s_decide(int64_t op, int64_t a, int64_t b);

'''
anchor18='void llama_context::resolve_fused_ops('
assert anchor18 in csrc
csrc = csrc.replace(anchor18, decl18 + anchor18, 1)
old_probe='''    auto resolve = [&](const llm_fused_op_probe & probe, bool & enabled) {
        if (!enabled) {
            return;
        }

        const uint32_t n_tokens_probe = probe.n_tokens_per_seq*n_seqs;'''
new_probe='''    auto resolve = [&](const llm_fused_op_probe & probe, bool & enabled) {
        if (fg391s_enabled() ? fg391s_decide(0, enabled ? 1 : 0, 1) == 0 : !enabled) {
            return;
        }

        const uint32_t n_tokens_probe = fg391s_enabled()
            ? (uint32_t) fg391s_decide(1, (int64_t) probe.n_tokens_per_seq, (int64_t) n_seqs)
            : probe.n_tokens_per_seq*n_seqs;'''
assert old_probe in csrc
csrc = csrc.replace(old_probe, new_probe, 1)
old_v='        if (device_mismatch) {'
assert old_v in csrc
csrc = csrc.replace(old_v, '        if (fg391s_enabled() ? fg391s_decide(2, device_mismatch ? 1 : 0, 0) == 0 : device_mismatch) {', 1)
ctx.write_text(csrc)

# KV slot walk (EXP392)
decl19='''// Goose KV slot walk (EXP392). GGML_GOOSE_KV_FIND_SLOT=0 restores the native walk.
extern "C" int fg392s_enabled(void);
extern "C" int64_t fg392s_find_slots(int64_t size, int64_t head, int64_t n_tokens, int64_t cont,
                                     int64_t n_swa, int64_t swa_type,
                                     const int64_t * pos, const int64_t * seqcnt, const int64_t * maxp1,
                                     int64_t * out, int64_t cap);

'''
anchor19='extern "C" int fg205g_enabled(void);'
assert anchor19 in src
src = src.replace(anchor19, decl19 + anchor19, 1)
old_walk='''        // bridge revision lost (EXP205 signature mismatch); native walk used on this host
        if (false && fg205g_enabled()) {
            // The per-cell predicate is a callback into the real llama_kv_cells, so Goose tests
            // exactly the cells native would and nothing is precomputed over the whole cache.
            std::vector<int64_t> out(n_tokens);
            const int64_t got = fg205g_find_slots(&cells, (int) n_swa, (int) swa_type,
                                                  (int64_t) head_cur,
                                                  (int64_t) n_tokens, cont ? 1 : 0, out.data());
            if (got < 0) { return { }; }
            res.idxs[s].clear();
            for (int64_t i = 0; i < got; ++i) res.idxs[s].push_back((uint32_t) out[i]);
            if (res.idxs[s].size() < n_tokens) { return { }; }
            continue;
        }'''
new_walk='''        if (fg392s_enabled()) {
            // the cells cross as flat arrays; the predicate is fg199k_can_use, exactly the one native uses
            const uint32_t n_cells = cells.size();
            std::vector<int64_t> pos_(n_cells, 0), cnt_(n_cells, 0), maxp1_(n_cells, 0), out_(n_tokens, -1);
            for (uint32_t i = 0; i < n_cells; ++i) {
                const bool occ = !cells.is_empty(i);
                cnt_[(size_t) i] = occ ? cells.seq_count(i) : 0;
                pos_[(size_t) i] = occ ? cells.pos_get(i) : 0;
                maxp1_[(size_t) i] = (occ && cnt_[(size_t) i] == 1) ? cells.seq_pos_max(cells.seq_get(i)) + 1 : 0;
            }
            const int64_t got = fg392s_find_slots((int64_t) n_cells, (int64_t) head_cur, (int64_t) n_tokens,
                                                  cont ? 1 : 0, (int64_t) n_swa, (int64_t) swa_type,
                                                  pos_.data(), cnt_.data(), maxp1_.data(),
                                                  out_.data(), (int64_t) out_.size());
            if (got < 0) { return { }; }
            res.idxs[s].clear();
            for (int64_t i = 0; i < got; ++i) res.idxs[s].push_back((uint32_t) out_[(size_t) i]);
            if (res.idxs[s].size() < n_tokens) { return { }; }
            continue;
        }'''
assert old_walk in src
src = src.replace(old_walk, new_walk, 1)
# KV metadata plan (EXP393)
decl20='''// Goose KV metadata plan (EXP393). GGML_GOOSE_KV_META_PLAN=0 restores the native field selection.
extern "C" int fg393s_enabled(void);
extern "C" int64_t fg393s_decide(int64_t op, int64_t a, int64_t b, int64_t c);

'''
anchor20='void llama_kv_cache::state_write_meta('
assert anchor20 in src
src = src.replace(anchor20, decl20 + anchor20, 1)
old_meta2='''            for (llama_seq_id cur = 0; cur < (int) n_seq_max; ++cur) {
                if (cur == seq_id || seq_id == -1) {
                    if (cells.seq_has(i, cur)) {
                        seq_ids.push_back(cur);
                    }
                }
            }'''
new_meta2='''            for (llama_seq_id cur = 0; cur < (int) n_seq_max; ++cur) {
                const bool holds = cells.seq_has(i, cur);
                const bool take = fg393s_enabled()
                    ? fg393s_decide(0, (int64_t) cur, (int64_t) seq_id, holds ? 1 : 0) != 0
                    : ((cur == seq_id || seq_id == -1) && holds);
                if (take) {
                    seq_ids.push_back(cur);
                }
            }'''
assert old_meta2 in src
src = src.replace(old_meta2, new_meta2, 1)
old_ext='''            if (has_cell_ext()) {
                const llama_kv_cell_ext ext = cells.ext_get(i);
                io.write(&ext, sizeof(ext));
            }'''
new_ext='''            if (fg393s_enabled() ? fg393s_decide(1, has_cell_ext() ? 1 : 0, 0, 0) != 0 : has_cell_ext()) {
                const llama_kv_cell_ext ext = cells.ext_get(i);
                io.write(&ext, sizeof(ext));
            }'''
assert old_ext in src
src = src.replace(old_ext, new_ext, 1)
# KV layout arithmetic (EXP394)
decl21='''// Goose KV layout arithmetic (EXP394). GGML_GOOSE_KV_LAYOUT_SPAN=0 restores the native expressions.
extern "C" int fg394s_enabled(void);
extern "C" int64_t fg394s_decide(int64_t op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t * out2);

'''
anchor21='void llama_kv_cache::state_write_data('
assert anchor21 in src
src = src.replace(anchor21, decl21 + anchor21, 1)

def span_row(first, count, row):
    return f'''            size_t g_off = (size_t) ({first}) * ({row}), g_size = (size_t) ({count}) * ({row});
            if (fg394s_enabled()) {{ g_off = (size_t) fg394s_decide(0, (int64_t) ({first}), (int64_t) ({count}), (int64_t) ({row}), 0, 0, nullptr);
                                    g_size = (size_t) fg394s_decide_out; }}'''

# write K
old_wk='''            const size_t range_size = range.second - range.first;
            const size_t buf_size = range_size * k_size_row;
            io.write_tensor(k, range.first * k_size_row, buf_size);'''
new_wk='''            const size_t range_size = range.second - range.first;
            size_t k_off_w = range.first * k_size_row, k_buf_w = range_size * k_size_row;
            if (fg394s_enabled()) {
                int64_t g2 = 0;
                k_off_w = (size_t) fg394s_decide(0, (int64_t) range.first, (int64_t) range_size, (int64_t) k_size_row, 0, 0, &g2);
                k_buf_w = (size_t) g2;
            }
            io.write_tensor(k, k_off_w, k_buf_w);'''
assert old_wk in src
src = src.replace(old_wk, new_wk, 1)

# write V (non-transposed)
old_wv='''                const size_t buf_size = range_size * v_size_row;
                io.write_tensor(v, range.first * v_size_row, buf_size);'''
new_wv='''                size_t v_off_w = range.first * v_size_row, v_buf_w = range_size * v_size_row;
                if (fg394s_enabled()) {
                    int64_t g2 = 0;
                    v_off_w = (size_t) fg394s_decide(0, (int64_t) range.first, (int64_t) range_size, (int64_t) v_size_row, 0, 0, &g2);
                    v_buf_w = (size_t) g2;
                }
                io.write_tensor(v, v_off_w, v_buf_w);'''
assert old_wv in src
src = src.replace(old_wv, new_wv, 1)

# write V transposed
old_vt='''                    const size_t src_offset = (range.first + j * kv_size) * v_size_el;
                    const size_t buf_size = range_size * v_size_el;
                    io.write_tensor(v, src_offset, buf_size);'''
new_vt='''                    size_t t_off = (size_t) ((size_t) range.first + j * kv_size) * v_size_el;
                    size_t t_buf = range_size * v_size_el;
                    if (fg394s_enabled()) {
                        int64_t g2 = 0;
                        t_off = (size_t) fg394s_decide(1, (int64_t) range.first, (int64_t) range_size, (int64_t) v_size_el, (int64_t) j, (int64_t) kv_size, &g2);
                        t_buf = (size_t) g2;
                    }
                    io.write_tensor(v, t_off, t_buf);'''
assert old_vt in src
src = src.replace(old_vt, new_vt, 1)

# read K
old_rk='''            io.read_tensor(k, (size_t) r.from * k_size_row, (size_t) (r.to - r.from) * k_size_row);'''
new_rk='''            size_t rk_off = (size_t) r.from * k_size_row, rk_buf = (size_t) (r.to - r.from) * k_size_row;
            if (fg394s_enabled()) {
                int64_t g2 = 0;
                rk_off = (size_t) fg394s_decide(0, (int64_t) r.from, (int64_t) (r.to - r.from), (int64_t) k_size_row, 0, 0, &g2);
                rk_buf = (size_t) g2;
            }
            io.read_tensor(k, rk_off, rk_buf);'''
assert old_rk in src
src = src.replace(old_rk, new_rk, 1)

# read V non-transposed
old_rv='''                io.read_tensor(v, (size_t) r.from * v_size_row, (size_t) (r.to - r.from) * v_size_row);'''
new_rv='''                size_t rv_off = (size_t) r.from * v_size_row, rv_buf = (size_t) (r.to - r.from) * v_size_row;
                if (fg394s_enabled()) {
                    int64_t g2 = 0;
                    rv_off = (size_t) fg394s_decide(0, (int64_t) r.from, (int64_t) (r.to - r.from), (int64_t) v_size_row, 0, 0, &g2);
                    rv_buf = (size_t) g2;
                }
                io.read_tensor(v, rv_off, rv_buf);'''
assert old_rv in src
src = src.replace(old_rv, new_rv, 1)

# read V transposed
old_rt='''                    const size_t dst_offset = ((size_t) r.from + j * cells.size()) * v_size_el;
                    io.read_tensor(v, dst_offset, (size_t) (r.to - r.from) * v_size_el);'''
new_rt='''                    size_t rt_off = ((size_t) r.from + j * cells.size()) * v_size_el;
                    size_t rt_buf = (size_t) (r.to - r.from) * v_size_el;
                    if (fg394s_enabled()) {
                        int64_t g2 = 0;
                        rt_off = (size_t) fg394s_decide(1, (int64_t) r.from, (int64_t) (r.to - r.from), (int64_t) v_size_el, (int64_t) j, (int64_t) cells.size(), &g2);
                        rt_buf = (size_t) g2;
                    }
                    io.read_tensor(v, rt_off, rt_buf);'''
assert old_rt in src
src = src.replace(old_rt, new_rt, 1)

# KV metadata read (EXP395)
decl22='''// Goose KV metadata read (EXP395). GGML_GOOSE_KV_READ_META=0 restores the native checks.
extern "C" int fg395s_enabled(void);
extern "C" int64_t fg395s_decide(int64_t op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t * out2);

'''
anchor22='bool llama_kv_cache::state_read_meta('
assert anchor22 in src
src = src.replace(anchor22, decl22 + anchor22, 1)
old_rj='            if (n_seq_id != 1) {'
new_rj='            if (fg395s_enabled() ? fg395s_decide(0, (int64_t) n_seq_id, 0, 0, 0, 0, nullptr) != 0 : n_seq_id != 1) {'
assert old_rj in src
src = src.replace(old_rj, new_rj, 1)
old_re='''            if (has_cell_ext()) {
                llama_kv_cell_ext ext;'''
new_re='''            if (fg395s_enabled() ? fg395s_decide(1, has_cell_ext() ? 1 : 0, 0, 0, 0, 0, nullptr) != 0 : has_cell_ext()) {
                llama_kv_cell_ext ext;'''
assert old_re in src
src = src.replace(old_re, new_re, 1)
old_rm='''            if (sinfo_in->empty() || sinfo_in->n_stream() != 1 || sinfo_in->idxs[0].size() != cell_count) {'''
new_rm='''            const bool g_mirror_bad = fg395s_enabled()
                ? fg395s_decide(2, sinfo_in->empty() ? 1 : 0, (int64_t) sinfo_in->n_stream(),
                                sinfo_in->empty() ? 0 : (int64_t) sinfo_in->idxs[0].size(),
                                (int64_t) cell_count, 0, nullptr) == 0
                : (sinfo_in->empty() || sinfo_in->n_stream() != 1 || sinfo_in->idxs[0].size() != cell_count);
            if (g_mirror_bad) {'''
assert old_rm in src
src = src.replace(old_rm, new_rm, 1)
p.write_text(src)
print('staging rebuilt ... + KV metadata read')
