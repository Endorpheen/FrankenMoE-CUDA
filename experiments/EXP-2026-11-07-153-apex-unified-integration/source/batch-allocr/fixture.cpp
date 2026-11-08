// fixture.cpp - differential native-vs-generated-Goose batch-allocr test.
// PROPOSAL ONLY. Not compiled or integrated here.
//
// Both engines run in the same process on the same installed batch state:
//   native : llama_batch_allocr::split_* / ubatch_reserve / split_reset
//   goose  : fg153_* hooks -> generated Goose entry -> fg153_goose_entry
// Every returned llama_ubatch is converted to the same JSONL row and compared
// field by field. There is no fallback: a missing generated adapter fails to
// link, and any divergence exits non-zero.
//
// Requires the class patch from integration/fg153-batch-hooks.h applied, and
// the generated adapter built with -DFG153_GOOSE_ENTRY_ID=<id>.

#define FG153_BATCH_ALLOCR_HOOKS 1
#define private public
#define protected public
#include "llama-batch.h"
#undef private
#undef protected

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct Case {
    std::string name;
    int mode = 0;                 // 0 simple, 1 equal, 2 seq
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
    std::string case_name;
    int ub = 0;
    uint32_t nt = 0, nst = 0, ns = 0;
    int eq = 0;
    std::vector<int32_t> bidx;
    std::vector<std::vector<llama_seq_id>> seqs;
    std::vector<llama_token> tok;
    std::vector<llama_pos> p;
    double embd_sum = 0.0;
};

void install(llama_batch_allocr & a, const Case & c) {
    std::vector<std::vector<llama_seq_id>> store = c.seq_ids;
    static std::vector<std::vector<llama_seq_id>> keep;   // fixture-lifetime storage
    keep = store;
    static std::vector<llama_seq_id *> ptrs;
    ptrs.resize(keep.size());
    for (size_t i = 0; i < keep.size(); ++i) ptrs[i] = keep[i].data();
    static std::vector<float> embd;
    embd.assign(c.embd_all ? c.token.size() * 8 : 0, 0.5f);

    a.clear();
    a.n_seq_max = c.n_seq_max;
    a.n_pos_per_embd = 1;
    a.n_embd = c.embd_all ? 8 : 0;
    a.n_embd_state = 0;

    a.batch = {};
    a.batch.n_tokens = (int32_t) c.token.size();
    a.batch.token = c.embd_all ? nullptr : const_cast<llama_token *>(c.token.data());
    a.batch.embd = c.embd_all ? embd.data() : nullptr;
    a.batch.pos = const_cast<llama_pos *>(c.pos.data());
    a.batch.n_seq_id = const_cast<int32_t *>(c.n_seq_id.data());
    a.batch.seq_id = ptrs.data();
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
        for (auto & u : a.used) u = true;
        a.n_used = (uint32_t) a.used.size();
    }
}

Row to_row(const Case & c, int ub, const llama_ubatch & u) {
    Row r;
    r.case_name = c.name; r.ub = ub;
    r.nt = u.n_tokens; r.nst = u.n_seq_tokens; r.ns = u.n_seqs; r.eq = u.equal_seqs() ? 1 : 0;
    if (u.data) r.bidx = u.data->batch_idxs;
    if (u.token) for (uint32_t i = 0; i < u.n_tokens; ++i) r.tok.push_back(u.token[i]);
    if (u.pos) for (uint32_t i = 0; i < u.n_tokens * u.n_pos; ++i) r.p.push_back(u.pos[i]);
    if (u.embd) for (uint32_t i = 0; i < u.n_tokens * 8; ++i) r.embd_sum += u.embd[i];
    for (uint32_t i = 0; i < u.n_tokens; ++i) {
        std::vector<llama_seq_id> s;
        for (int32_t k = 0; k < u.n_seq_id[i]; ++k) s.push_back(u.seq_id[i][k]);
        r.seqs.push_back(std::move(s));
    }
    return r;
}

std::vector<Row> run_native(llama_batch_allocr & a, const Case & c) {
    std::vector<Row> rows;
    for (int guard = 0; guard < 10000; ++guard) {
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

std::vector<Row> run_goose(llama_batch_allocr & a, const Case & c) {
    std::vector<Row> rows;
    for (int guard = 0; guard < 10000; ++guard) {
        llama_ubatch u{};
        switch (c.mode) {
            case 0: u = a.fg153_split_simple(c.n_ubatch); break;
            case 1: u = a.fg153_split_equal(c.n_ubatch, c.sequential, c.n_keep_tail); break;
            case 2: u = a.fg153_split_seq(c.n_ubatch); break;
        }
        if (u.n_tokens == 0) break;
        rows.push_back(to_row(c, (int) rows.size(), u));
    }
    return rows;
}

bool equal_rows(const std::vector<Row> & a, const std::vector<Row> & b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].nt != b[i].nt || a[i].nst != b[i].nst || a[i].ns != b[i].ns || a[i].eq != b[i].eq) return false;
        if (a[i].bidx != b[i].bidx || a[i].seqs != b[i].seqs) return false;
        if (a[i].tok != b[i].tok || a[i].p != b[i].p) return false;
        if (a[i].embd_sum != b[i].embd_sum) return false;
    }
    return true;
}

void emit(const char * engine, const Row & r) {
    std::printf("{\"engine\":\"%s\",\"case\":\"%s\",\"ub\":%d,\"n_tokens\":%u,"
                "\"n_seq_tokens\":%u,\"n_seqs\":%u,\"equal\":%d,\"batch_idxs\":[",
                engine, r.case_name.c_str(), r.ub, r.nt, r.nst, r.ns, r.eq);
    for (size_t i = 0; i < r.bidx.size(); ++i) std::printf("%s%d", i ? "," : "", r.bidx[i]);
    std::printf("],\"seq_ids\":[");
    for (size_t i = 0; i < r.seqs.size(); ++i) {
        std::printf("%s[", i ? "," : "");
        for (size_t k = 0; k < r.seqs[i].size(); ++k) std::printf("%s%d", k ? "," : "", (int) r.seqs[i][k]);
        std::printf("]");
    }
    std::printf("],\"token\":[");
    for (size_t i = 0; i < r.tok.size(); ++i) std::printf("%s%d", i ? "," : "", (int) r.tok[i]);
    std::printf("],\"pos\":[");
    for (size_t i = 0; i < r.p.size(); ++i) std::printf("%s%d", i ? "," : "", (int) r.p[i]);
    std::printf("],\"embd_sum\":%.6f}\n", r.embd_sum);
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

int main(int argc, char ** argv) {
    bool show = false;
    std::string only;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--emit")) show = true;
        if (!std::strcmp(argv[i], "--case") && i + 1 < argc) only = argv[++i];
    }

    int failures = 0;
    for (const Case & c : cases()) {
        if (!only.empty() && only != c.name) continue;

        llama_batch_allocr native(1, false);
        install(native, c);
        const auto rn = run_native(native, c);

        llama_batch_allocr goose(1, false);
        install(goose, c);
        goose.fg153_import_native_state();
        const auto rg = run_goose(goose, c);

        const bool ok = equal_rows(rn, rg);
        if (!ok) {
            ++failures;
            std::fprintf(stderr, "MISMATCH case=%s native=%zu goose=%zu\n", c.name.c_str(), rn.size(), rg.size());
        }
        if (show) {
            for (const auto & r : rn) emit("native", r);
            for (const auto & r : rg) emit("goose", r);
        }
    }
    if (failures) {
        std::fprintf(stderr, "FAILED: %d case(s)\n", failures);
        return 1;
    }
    std::printf("OK: native and generated Goose agree on all cases\n");
    return 0;
}
