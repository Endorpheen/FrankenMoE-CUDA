// harness.cpp - real differential harness for llama_batch_allocr.
// Links the genuine inputs/llama-batch.cpp implementation and runs native
// split_* against generated Goose on identical installed batch state.
// Diagnostics only; no model, no benchmark.

#include <algorithm>
#include <array>
#include <bitset>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define private public
#define protected public
#include "/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/src/llama-batch.h"
#include "/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/src/llama-vocab.h"
#undef private
#undef protected

#include "bridge.h"
#include "patched_api.h"

extern "C" int64_t fg153_goose_entry(int64_t mode, int64_t a, int64_t b, int64_t c,
                                     int64_t d, int64_t e, int64_t f);

namespace {

struct Case {
    std::string name;
    int mode = 0;
    uint32_t n_ubatch = 4;
    bool sequential = false;
    uint32_t n_keep_tail = 0;
    std::vector<llama_token> token;
    std::vector<llama_pos> pos;
    std::vector<int32_t> n_seq_id;
    std::vector<std::vector<llama_seq_id>> seq_ids;
    std::vector<int8_t> logits;
    bool embd_all = false;
    bool mark_all_used = false;
    bool has_cpl = false;
    uint32_t n_seq_max = 4;
};

struct Row {
    std::string name;
    int ub = 0;
    uint32_t nt = 0, nst = 0, ns = 0;
    int eq = 0;
    std::vector<int32_t> bidx;
    std::vector<std::vector<llama_seq_id>> seqs;
    std::vector<llama_token> tok;
    std::vector<llama_pos> p;
    double embd_sum = 0.0;
};

struct Store {
    std::vector<std::vector<llama_seq_id>> seq;
    std::vector<llama_seq_id *> ptr;
    std::vector<float> embd;
};

void sync_to(llama_batch_allocr & a, int64_t s);

void install(llama_batch_allocr & a, const Case & c, Store & st) {
    st.seq = c.seq_ids;
    st.ptr.resize(st.seq.size());
    for (size_t i = 0; i < st.seq.size(); ++i) st.ptr[i] = st.seq[i].data();
    st.embd.assign(c.embd_all ? c.token.size() * 8 : 0, 0.5f);

    a.clear();
    a.n_seq_max = c.n_seq_max;
    a.n_embd = c.embd_all ? 8 : 0;
    a.n_embd_state = 0;
    a.batch = {};
    a.batch.n_tokens = (int32_t) c.token.size();
    a.batch.token = c.embd_all ? nullptr : const_cast<llama_token *>(c.token.data());
    a.batch.embd = c.embd_all ? st.embd.data() : nullptr;
    a.batch.pos = const_cast<llama_pos *>(c.pos.data());
    a.batch.n_seq_id = const_cast<int32_t *>(c.n_seq_id.data());
    a.batch.seq_id = st.ptr.data();
    a.batch.logits = const_cast<int8_t *>(c.logits.data());
    if (c.embd_all) a.is_embd_vec.assign(c.token.size(), 1);

    a.seq_set.assign(c.seq_ids.size(), llama_batch_allocr::seq_set_t());
    a.seq_set_map.clear();
    a.has_cpl = c.has_cpl;
    for (size_t i = 0; i < c.seq_ids.size(); ++i) {
        for (auto s : c.seq_ids[i]) a.seq_set[i].set((size_t) s);
        a.seq_set_map[a.seq_set[i]].push_back((int32_t) i);
    }
    a.split_reset();
    if (c.mark_all_used) {
        a.used.assign(a.used.size(), true);
        a.n_used = (uint32_t) a.used.size();
    }
}

void import(llama_batch_allocr & a, int64_t s) {
    const int64_t nt = a.batch.n_tokens;
    fg153_ba_put(s, 0, 0, nt);
    fg153_ba_put(s, 1, 0, (int64_t) a.n_embd);
    fg153_ba_put(s, 2, 0, (int64_t) a.n_embd_state);
    fg153_ba_put(s, 3, 0, (int64_t) a.n_seq_max);
    fg153_ba_put(s, 4, 0, (int64_t) a.n_pos_per_embd);
    fg153_ba_put(s, 5, 0, (int64_t) a.n_outputs);
    fg153_ba_put(s, 7, 0, a.has_cpl ? 1 : 0);
    fg153_ba_put(s, 8, 0, a.is_embd_vec.empty() ? 0 : 1);
    fg153_ba_put(s, 9, 0, a.batch.token ? 1 : 0);
    fg153_ba_put(s, 10, 0, a.batch.embd ? 1 : 0);
    fg153_ba_put(s, 11, 0, a.state_vec.empty() ? 0 : 1);

    auto pv = [&](int64_t key, const auto & src) {
        fg153_ba_call(74, s, key, (int64_t) src.size(), 0);
        for (size_t i = 0; i < src.size(); ++i)
            fg153_ba_call(72, s, key, (int64_t) i, (int64_t) src[i]);
    };
    std::vector<int64_t> tok((size_t) nt, 0), posv, nsqv((size_t) nt, 0), seqflat, outv((size_t) nt, 0);
    std::vector<int64_t> offv((size_t) nt + 1, 0);
    for (int64_t i = 0; i < nt; ++i) {
        if (a.batch.token) tok[(size_t) i] = (int64_t) a.batch.token[i];
        if (a.batch.pos) posv.push_back((int64_t) a.batch.pos[i]);
        if (a.batch.n_seq_id) nsqv[(size_t) i] = (int64_t) a.batch.n_seq_id[i];
        if (a.batch.logits) outv[(size_t) i] = (int64_t) a.batch.logits[i];
        if (a.batch.seq_id)
            for (int32_t k = 0; k < nsqv[(size_t) i]; ++k) seqflat.push_back((int64_t) a.batch.seq_id[i][k]);
        offv[(size_t) i + 1] = (int64_t) seqflat.size();
    }
    pv(20, tok);
    pv(24, posv);
    pv(25, nsqv);
    pv(26, seqflat);
    pv(27, offv);
    pv(28, outv);
    pv(23, a.is_embd_vec);
    pv(29, a.decision_order);
    pv(30, a.seq_id_unq);
    {
        std::vector<int64_t> sx((size_t) LLAMA_MAX_SEQ, -1);
        for (int i = 0; i < LLAMA_MAX_SEQ && i < (int) a.seq_idx.size(); ++i) sx[(size_t) i] = a.seq_idx[(size_t) i];
        pv(31, sx);
    }
    if (a.batch.embd && a.n_embd > 0) {
        std::vector<float> copy(a.batch.embd, a.batch.embd + (size_t) nt * a.n_embd);
        fg153_ba_import_embd(s, (int64_t) (uintptr_t) copy.data(), (int64_t) copy.size());
    }
    if (!a.state_vec.empty())
        fg153_ba_import_state(s, (int64_t) (uintptr_t) a.state_vec.data(), (int64_t) a.state_vec.size());

    fg153_ba_call(28, s, 0, 0, 0);
    for (int64_t i = 0; i < nt; ++i) {
        fg153_ba_call(23, s, i, 0, 0);
        for (int32_t k = 0; k < a.batch.n_seq_id[i]; ++k)
            fg153_ba_call(24, s, i, (int64_t) a.batch.seq_id[i][k], 0);
        const int64_t slot = fg153_ba_call(29, s, i, 0, 0);
        fg153_ba_call(33, s, slot, i, 0);
    }
    fg153_ba_call(41, s, 0, 0, 0);
    if (a.batch.pos && a.batch.seq_id && a.batch.n_seq_id) {
        for (int64_t i = 0; i < nt; ++i)
            for (int32_t k = 0; k < a.batch.n_seq_id[i]; ++k)
                fg153_ba_call(36, s, (int64_t) a.batch.seq_id[i][k], (int64_t) a.batch.pos[i], 0);
    }
    fg153_ba_call(42, s, 0, 0, 0);
    for (size_t a0 = 0; a0 < a.seq_cpl.size(); ++a0)
        for (size_t a1 = 0; a1 < a.seq_cpl[a0].size(); ++a1)
            if (a.seq_cpl[a0][a1]) fg153_ba_call(43, s, (int64_t) a0, (int64_t) a1, 0);

    sync_to(a, s);
}

void sync_to(llama_batch_allocr & a, int64_t s) {
    const int64_t n = a.batch.n_tokens;
    fg153_ba_call(74, s, 32, n, 0);
    for (int64_t i = 0; i < n; ++i)
        fg153_ba_call(72, s, 32, i, (i < (int64_t) a.used.size() && a.used[(size_t) i]) ? 1 : 0);
    fg153_ba_put(s, 6, 0, (int64_t) a.n_used);
    fg153_ba_call(75, s, 33, 0, 0);
    for (int32_t id : a.out_ids) fg153_ba_call(73, s, 33, id, 0);
}

void sync_from(llama_batch_allocr & a, int64_t s) {
    const int64_t n = a.batch.n_tokens;
    a.used.assign((size_t) n, false);
    for (int64_t i = 0; i < n; ++i) a.used[(size_t) i] = fg153_ba_call(71, s, 32, i, 0) != 0;
    a.n_used = (uint32_t) fg153_ba_get(s, 6, 0);
    a.out_ids.clear();
    const int64_t no = fg153_ba_call(70, s, 33, 0, 0);
    for (int64_t i = 0; i < no; ++i) a.out_ids.push_back((int32_t) fg153_ba_call(71, s, 33, i, 0));
}

llama_ubatch to_native(int64_t s, int64_t uh) {
    const int64_t nt = fg153_ba_ub_n(uh, 0);
    const int64_t nseq = fg153_ba_ub_n(uh, 1);
    const int64_t npos = fg153_ba_get(s, 4, 0);

    auto data = std::make_shared<llama_ubatch::data_t>();
    data->token.resize((size_t) nt);
    for (int64_t i = 0; i < nt; ++i) data->token[(size_t) i] = (llama_token) fg153_ba_ub_i(uh, 0, i);

    int64_t nf = 0;
    if (const float * e = fg153_ba_ub_embd(uh, &nf); e && nf > 0) data->embd.assign(e, e + nf);
    if (const float * st = fg153_ba_ub_state(uh, &nf); st && nf > 0) data->embd_state.assign(st, st + nf);

    data->pos.resize((size_t) (nt * npos));
    for (int64_t i = 0; i < nt * npos; ++i) data->pos[(size_t) i] = (llama_pos) fg153_ba_ub_i(uh, 1, i);

    data->n_seq_id.resize((size_t) nt);
    int64_t total = 0;
    for (int64_t i = 0; i < nt; ++i) {
        data->n_seq_id[(size_t) i] = (int32_t) fg153_ba_ub_i(uh, 2, i);
        total += data->n_seq_id[(size_t) i];
    }
    data->seq_id_data.reserve((size_t) total);
    for (int64_t i = 0; i < total; ++i) data->seq_id_data.push_back((llama_seq_id) fg153_ba_ub_i(uh, 9, i));
    data->seq_id.resize((size_t) nt);
    size_t off = 0;
    for (int64_t i = 0; i < nt; ++i) {
        data->seq_id[(size_t) i] = data->seq_id_data.data() + off;
        off += data->n_seq_id[(size_t) i];
    }

    const int64_t nuq = fg153_ba_ub_n(uh, 7);
    data->seq_id_unq.resize((size_t) nuq);
    for (int64_t i = 0; i < nuq; ++i) data->seq_id_unq[(size_t) i] = (llama_seq_id) fg153_ba_ub_i(uh, 7, i);

    const int64_t nsqidx = fg153_ba_ub_n(uh, 6);
    data->seq_idx.assign(LLAMA_MAX_SEQ, -1);
    for (int64_t i = 0; i < nsqidx && i < (int64_t) data->seq_idx.size(); ++i)
        data->seq_idx[(size_t) i] = (int32_t) fg153_ba_ub_i(uh, 8, i);

    data->output.resize((size_t) nt);
    for (int64_t i = 0; i < nt; ++i) data->output[(size_t) i] = (int8_t) fg153_ba_ub_i(uh, 3, i);

    for (int64_t i = 0; i < nt; ++i) {
        const int64_t v = fg153_ba_ub_i(uh, 4, i);
        if (v < 0) break;
        data->type.push_back((int8_t) v);
    }
    for (int64_t i = 0; i < nt; ++i) {
        const int64_t v = fg153_ba_ub_i(uh, 5, i);
        if (v < 0) break;
        data->decision_order.push_back((int32_t) v);
    }
    data->batch_idxs.resize((size_t) nt);
    for (int64_t i = 0; i < nt; ++i) data->batch_idxs[(size_t) i] = (int32_t) fg153_ba_ub_i(uh, 6, i);

    llama_ubatch res{};
    res.b_equal_seqs = (uint32_t) fg153_ba_ub_n(uh, 3);
    res.n_tokens = (uint32_t) nt;
    res.n_seq_tokens = (uint32_t) (nseq > 0 ? nt / nseq : 0);
    res.n_seqs = (uint32_t) nseq;
    res.n_seqs_unq = (uint32_t) nuq;
    res.n_pos = (uint32_t) npos;

    bool use_token = fg153_ba_get(s, 9, 0) > 0;
    if (!data->type.empty()) {
        bool all_embd = true;
        for (int8_t t : data->type) if (!t) { all_embd = false; break; }
        if (all_embd) use_token = false;
    }
    res.token = use_token ? data->token.data() : nullptr;
    res.embd = data->embd.empty() ? nullptr : data->embd.data();
    res.embd_state = data->embd_state.empty() ? nullptr : data->embd_state.data();
    res.pos = data->pos.data();
    res.n_seq_id = data->n_seq_id.data();
    res.seq_id = data->seq_id.data();
    res.seq_id_unq = data->seq_id_unq.data();
    res.seq_idx = data->seq_idx.data();
    res.output = data->output.data();
    res.type = data->type.empty() ? nullptr : data->type.data();
    res.decision_order = data->decision_order.empty() ? nullptr : data->decision_order.data();
    res.data = std::move(data);
    return res;
}

Row to_row(const Case & c, int ub, const llama_ubatch & u) {
    Row r;
    r.name = c.name; r.ub = ub;
    r.nt = u.n_tokens; r.nst = u.n_seq_tokens; r.ns = u.n_seqs; r.eq = u.equal_seqs() ? 1 : 0;
    if (u.data) r.bidx = u.data->batch_idxs;
    for (uint32_t i = 0; i < u.n_tokens; ++i) {
        std::vector<llama_seq_id> s;
        for (int32_t k = 0; k < u.n_seq_id[i]; ++k) s.push_back(u.seq_id[i][k]);
        r.seqs.push_back(std::move(s));
    }
    if (u.token) for (uint32_t i = 0; i < u.n_tokens; ++i) r.tok.push_back(u.token[i]);
    if (u.pos) for (uint32_t i = 0; i < u.n_tokens * u.n_pos; ++i) r.p.push_back(u.pos[i]);
    if (u.embd) for (uint32_t i = 0; i < u.n_tokens * 8; ++i) r.embd_sum += u.embd[i];
    return r;
}

std::vector<Row> run_native(llama_batch_allocr & a, const Case & c) {
    std::vector<Row> rows;
    for (int g = 0; g < 10000; ++g) {
        llama_ubatch u{};
        switch (c.mode) {
            case 0: u = a.split_simple(c.n_ubatch); break;
            case 1: u = a.split_equal(c.n_ubatch, c.sequential, c.n_keep_tail); break;
            case 2: u = a.split_seq(c.n_ubatch); break;
        }
        if (u.n_tokens == 0) break;
        rows.push_back(to_row(c, (int) rows.size(), u));
    }
    return rows;
}

std::vector<Row> run_goose(llama_batch_allocr & a, const Case & c, int64_t s) {
    std::vector<Row> rows;
    for (int g = 0; g < 10000; ++g) {
        sync_to(a, s);
        int64_t uh = 0;
        switch (c.mode) {
            case 0: uh = fg153_goose_entry(1, s, c.n_ubatch, 0, 0, 0, 0); break;
            case 1: uh = fg153_goose_entry(2, s, c.n_ubatch, c.sequential ? 1 : 0, c.n_keep_tail, 0, 0); break;
            case 2: uh = fg153_goose_entry(3, s, c.n_ubatch, 0, 0, 0, 0); break;
        }
        llama_ubatch u{};
        if (uh) { u = to_native(s, uh); fg153_ba_ub_release(uh); }
        sync_from(a, s);
        if (u.n_tokens == 0) break;
        rows.push_back(to_row(c, (int) rows.size(), u));
    }
    return rows;
}

bool same(const std::vector<Row> & x, const std::vector<Row> & y) {
    if (x.size() != y.size()) return false;
    for (size_t i = 0; i < x.size(); ++i) {
        if (x[i].nt != y[i].nt || x[i].nst != y[i].nst || x[i].ns != y[i].ns || x[i].eq != y[i].eq) return false;
        if (x[i].bidx != y[i].bidx || x[i].seqs != y[i].seqs) return false;
        if (x[i].tok != y[i].tok || x[i].p != y[i].p) return false;
        if (x[i].embd_sum != y[i].embd_sum) return false;
    }
    return true;
}

std::vector<Case> cases() {
    std::vector<Case> v;
    { Case c; c.name="seq_overlap"; c.mode=0; c.n_ubatch=4;
      c.token={10,11,12,13}; c.pos={0,1,2,0}; c.n_seq_id={1,2,1,1};
      c.seq_ids={{0},{0,1},{1},{2}}; c.logits={0,0,0,0}; c.has_cpl=true; v.push_back(c); }
    { Case c; c.name="equal_tail"; c.mode=1; c.n_ubatch=6; c.n_keep_tail=3;
      for (int s=0;s<2;++s) for (int p=0;p<5;++p){c.token.push_back(100+s*10+p);c.pos.push_back(p);
        c.n_seq_id.push_back(1);c.seq_ids.push_back({(llama_seq_id)s});c.logits.push_back(0);} v.push_back(c); }
    { Case c; c.name="short_final"; c.mode=1; c.n_ubatch=4;
      for (int p=0;p<4;++p){c.token.push_back(200+p);c.pos.push_back(p);c.n_seq_id.push_back(1);
        c.seq_ids.push_back({0});c.logits.push_back(1);}
      for (int p=0;p<2;++p){c.token.push_back(300+p);c.pos.push_back(p);c.n_seq_id.push_back(1);
        c.seq_ids.push_back({1});c.logits.push_back(1);} v.push_back(c); }
    { Case c; c.name="embd_all"; c.mode=0; c.n_ubatch=2;
      c.token={0,0,0}; c.pos={0,1,2}; c.n_seq_id={1,1,1}; c.seq_ids={{0},{0},{0}};
      c.logits={1,0,1}; c.embd_all=true; v.push_back(c); }
    { Case c; c.name="empty"; c.mode=0; c.n_ubatch=2;
      c.token={400,401}; c.pos={0,1}; c.n_seq_id={1,1}; c.seq_ids={{0},{0}};
      c.logits={0,0}; c.mark_all_used=true; v.push_back(c); }
    { Case c; c.name="err_sequential_coupled"; c.mode=1; c.n_ubatch=4; c.sequential=true; c.has_cpl=true;
      c.token={500,501}; c.pos={0,0}; c.n_seq_id={2,1}; c.seq_ids={{0,1},{0}}; c.logits={0,0}; v.push_back(c); }
    { Case c; c.name="seq_split_overlap"; c.mode=2; c.n_ubatch=3;
      c.token={600,601,602}; c.pos={0,1,0}; c.n_seq_id={2,1,1};
      c.seq_ids={{0,1},{0},{2}}; c.logits={0,0,0}; c.has_cpl=true; v.push_back(c); }
    return v;
}

}  // namespace

struct InitCase {
    std::string name;
    std::vector<llama_token> tok;
    std::vector<std::vector<llama_seq_id>> seqs;
    std::vector<llama_pos> pos;
    std::vector<int8_t> out;
    bool output_all = false;
    uint32_t n_seq_max = 4;
    llama_token n_vocab = 100;
};

int64_t st_len(int64_t s, int64_t key) { return fg153_ba_call(70, s, key, 0, 0); }
int64_t st_at(int64_t s, int64_t key, int64_t i) { return fg153_ba_call(71, s, key, i, 0); }

bool run_init_case(const InitCase & c, llama_vocab & vocab, std::string & why) {
    llama_batch_ext ext(c.tok.size(), 8, 8, (llama_seq_id) c.n_seq_max, nullptr, c.n_vocab, 1);
    for (size_t i = 0; i < c.tok.size(); ++i) {
        const int32_t idx = ext.add_token(c.seqs[i].empty() ? 0 : c.seqs[i][0]);
        if (idx < 0) { why = "add_token failed"; return false; }
        for (size_t k = 1; k < c.seqs[i].size(); ++k) ext.add_seq(idx, c.seqs[i][k]);
        ext.set_token_id(idx, c.tok[i]);
        const llama_pos p = c.pos[i];
        ext.set_token_pos(idx, &p);
        ext.set_output(idx, c.out[i] != 0);
    }

    llama_batch_allocr native(1, false);
    setenv("GGML_GOOSE_BATCH_ALLOCR","0",1);
    const bool nok = native.init(ext, vocab, c.output_all);
    unsetenv("GGML_GOOSE_BATCH_ALLOCR");
    llama_batch_allocr actual(1,false);
    const bool actual_ok=actual.init(ext,vocab,c.output_all);
    if(actual_ok!=nok || (nok && (actual.n_seq_max!=native.n_seq_max || actual.n_embd!=native.n_embd || actual.n_embd_state!=native.n_embd_state || actual.has_cpl!=native.has_cpl))){why="normal init metadata";return false;}

    // Patched class (Goose-routed normal init) through the C ABI.
    std::vector<int64_t> id, out, posv, sf, so(1, 0), nsq;
    for (size_t i = 0; i < c.tok.size(); ++i) {
        id.push_back((int64_t) ext.tokens[i].id);
        out.push_back(ext.tokens[i].output ? 1 : 0);
        posv.push_back((int64_t) ext.tokens[i].pos[0]);
        nsq.push_back((int64_t) ext.tokens[i].seq_ids.size());
        for (auto s : ext.tokens[i].seq_ids) sf.push_back((int64_t) s);
        so.push_back((int64_t) sf.size());
    }
    fg153p_init_out po{};
    const int rc = fg153p_init_case(id.data(), posv.data(), nsq.data(), sf.data(), so.data(), out.data(),
                                    (int64_t) c.tok.size(), c.n_seq_max, c.n_vocab, c.output_all ? 1 : 0, &po);
    if (rc != 0) { why = "patched init abi failed"; return false; }

    bool ok = (nok == (po.ok != 0));
    if (!ok) why = "init accept/reject differs";
    if (ok && nok) {
        const auto & nb = native.get_batch();
        if (po.n_tokens != nb.n_tokens) { ok = false; why = "n_tokens"; }
        if (ok && po.n_outputs != (int32_t) native.get_n_outputs()) { ok = false; why = "n_outputs"; }
        for (int32_t i = 0; i < nb.n_tokens && ok; ++i) {
            if (po.token[i] != nb.token[i]) { ok = false; why = "token"; }
            if (po.pos[i] != nb.pos[i]) { ok = false; why = "pos"; }
            if (po.n_seq_id[i] != nb.n_seq_id[i]) { ok = false; why = "n_seq_id"; }
            if (po.logits[i] != nb.logits[i]) { ok = false; why = "logits"; }
            // seq_id order comes from an unordered_set upstream; compare as sets.
            std::vector<llama_seq_id> ns(nb.seq_id[i], nb.seq_id[i] + nb.n_seq_id[i]);
            std::vector<llama_seq_id> ps;
            for (int32_t k = 0; k < po.n_seq_id[i]; ++k) ps.push_back(po.seq_flat[po.seq_off[i] + k]);
            std::sort(ns.begin(), ns.end());
            std::sort(ps.begin(), ps.end());
            if (ns != ps) { ok = false; why = "seq_id"; }
        }
    }
    return ok;
}

std::vector<InitCase> init_cases() {
    std::vector<InitCase> v;
    { InitCase c; c.name = "init_valid";
      c.tok={10,11,12,13}; c.seqs={{0},{0},{1},{1}}; c.pos={0,1,0,1};
      c.out={0,1,0,0}; c.output_all=false; v.push_back(c); }
    { InitCase c; c.name = "init_output_all";
      c.tok={20,21}; c.seqs={{0},{0}}; c.pos={0,1}; c.out={0,0}; c.output_all=true; v.push_back(c); }
    { InitCase c; c.name = "init_decreasing_pos";
      c.tok={30,31,32}; c.seqs={{0},{0},{0}}; c.pos={4,5,0}; c.out={0,0,0}; v.push_back(c); }
    { InitCase c; c.name = "init_coupled";
      c.tok={40,41,42}; c.seqs={{0},{0,1},{1}}; c.pos={0,1,1}; c.out={0,0,0}; v.push_back(c); }
    return v;
}


std::vector<Row> run_patched(const Case & c, bool rollback, int & rc_out) {
    std::vector<int64_t> tok, posv, nsq, seqflat, seqoff(1, 0), logits;
    for (size_t i = 0; i < c.token.size(); ++i) {
        tok.push_back((int64_t) c.token[i]);
        posv.push_back((int64_t) c.pos[i]);
        nsq.push_back((int64_t) c.n_seq_id[i]);
        logits.push_back((int64_t) c.logits[i]);
        for (auto s : c.seq_ids[i]) seqflat.push_back((int64_t) s);
        seqoff.push_back((int64_t) seqflat.size());
    }
    if (rollback) setenv("GGML_GOOSE_BATCH_ALLOCR", "0", 1);
    else unsetenv("GGML_GOOSE_BATCH_ALLOCR");
    fg153p_row rows[64]{};
    const int n = fg153p_run_case(c.mode, c.n_ubatch, c.sequential ? 1 : 0, c.n_keep_tail,
                                  tok.data(), posv.data(), nsq.data(), seqflat.data(), seqoff.data(),
                                  logits.data(), (int64_t) c.token.size(), c.n_seq_max,
                                  c.embd_all ? 1 : 0, c.mark_all_used ? 1 : 0, rows, 64);
    unsetenv("GGML_GOOSE_BATCH_ALLOCR");
    rc_out = n;
    std::vector<Row> out;
    for (int i = 0; i < n; ++i) {
        Row r;
        r.name = c.name; r.ub = i;
        r.nt = rows[i].n_tokens; r.nst = rows[i].n_seq_tokens; r.ns = rows[i].n_seqs; r.eq = rows[i].equal;
        int32_t nf = 0;
        for (uint32_t t = 0; t < r.nt; ++t) {
            std::vector<llama_seq_id> s;
            for (int32_t k = 0; k < rows[i].n_seq_id[t]; ++k) s.push_back(rows[i].seq_flat[nf++]);
            r.seqs.push_back(std::move(s));
            if (rows[i].has_token) r.tok.push_back(rows[i].token[t]);
            r.p.push_back(rows[i].pos[t]);
            r.bidx.push_back(rows[i].batch_idxs[t]);
        }
        r.embd_sum = rows[i].embd_sum;
        out.push_back(std::move(r));
    }
    return out;
}

int main() {
    int failures = 0;
    llama_vocab vocab;
    for (const InitCase & ic : init_cases()) {
        std::string why;
        if (!run_init_case(ic, vocab, why)) {
            ++failures;
            std::fprintf(stderr, "MISMATCH init case=%s (%s)\n", ic.name.c_str(), why.c_str());
        } else {
            std::printf("OK init case=%s\n", ic.name.c_str());
        }
    }
    std::printf("sizeof oracle=%zu patched=%lld\n", sizeof(llama_batch_allocr), (long long) fg153p_sizeof());
    if ((int64_t) sizeof(llama_batch_allocr) != fg153p_sizeof()) {
        std::fprintf(stderr, "ABI MISMATCH: sizeof changed\n");
        ++failures;
    }

    uint64_t c0 = 0, f0 = 0, u0 = 0;
    fg153p_counters(&c0, &f0, &u0);

    for (const Case & c : cases()) {
        Store sn, sg;

        llama_batch_allocr an(1, false);
        install(an, c, sn);
        setenv("GGML_GOOSE_BATCH_ALLOCR","0",1);
        const auto rn = run_native(an, c);
        unsetenv("GGML_GOOSE_BATCH_ALLOCR");

        int rc = 0;
        const auto rg = run_patched(c, false, rc);
        if (rc < 0) { ++failures; std::fprintf(stderr, "patched rc=%d case=%s\n", rc, c.name.c_str()); continue; }

        if (!same(rn, rg)) {
            ++failures;
            std::fprintf(stderr, "MISMATCH case=%s native=%zu patched=%zu\n", c.name.c_str(), rn.size(), rg.size());
            for (size_t i = 0; i < std::min(rn.size(), rg.size()); ++i) {
                if (rn[i].nt != rg[i].nt || rn[i].seqs != rg[i].seqs || rn[i].bidx != rg[i].bidx)
                    std::fprintf(stderr, "  ub%zu native(nt=%u seqs=%zu bidx=%zu) patched(nt=%u seqs=%zu bidx=%zu)\n",
                                 i, rn[i].nt, rn[i].seqs.size(), rn[i].bidx.size(),
                                 rg[i].nt, rg[i].seqs.size(), rg[i].bidx.size());
            }
        } else {
            std::printf("OK case=%s ubatches=%zu\n", c.name.c_str(), rn.size());
        }

        const auto rr = run_patched(c, true, rc);
        if (rc < 0 || !same(rn, rr)) {
            ++failures;
            std::fprintf(stderr, "ROLLBACK MISMATCH case=%s rc=%d native=%zu rollback=%zu\n",
                         c.name.c_str(), rc, rn.size(), rr.size());
        }
    }

    uint64_t c1 = 0, f1 = 0, u1 = 0;
    fg153p_counters(&c1, &f1, &u1);
    std::printf("goose calls=%llu fallbacks=%llu ubatches=%llu\n",
                (unsigned long long) (c1 - c0), (unsigned long long) (f1 - f0), (unsigned long long) (u1 - u0));
    if (c1 <= c0 || u1 <= u0) { std::fprintf(stderr, "Goose counters did not advance\n"); ++failures; }
    if (f1 <= f0) { std::fprintf(stderr, "rollback flag never took the native path\n"); ++failures; }

    if (failures) { std::fprintf(stderr, "FAILED %d case(s)\n", failures); return 1; }
    std::printf("OK all differential cases (patched class vs native oracle, rollback verified)\n");
    return 0;
}
