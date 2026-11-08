// worker-2/fixture.cpp
//
// Differential reference fixture. It drives the REAL llama_kv_cells from
// inputs/llama-kv-cells.h through public methods only (resize/reset/
// pos_set/seq_add/seq_rm/seq_keep/pos_add/pos_div/cp/set/rm). It never
// reads private members and never fabricates C++ offsets.
//
// The printed lines are meant to be compared, step by step, with the same
// journal in worker-2/differential-journal.json executed by cells.goose
// through the storage bridge. UNVERIFIED_PROPOSAL: not built, not run.
//
// Requires the real llama headers (llama.h, llama-cparams.h) and defines
// from the parent source tree. See report.md for the compile command and
// the unresolved dependency list.

#include "llama-kv-cells.h"

#include <cstdio>

namespace {

void line(const char * tag, const char * key, long long value) {
    std::printf("%s %s=%lld\n", tag, key, value);
}

void dump(const char * tag, const llama_kv_cells & c, int seq) {
    line(tag, "size", static_cast<long long>(c.size()));
    line(tag, "used", static_cast<long long>(c.get_used()));
    line(tag, "used_min", static_cast<long long>(c.used_min()));
    line(tag, "used_max_p1", static_cast<long long>(c.used_max_p1()));
    line(tag, "has_shift", static_cast<long long>(c.get_has_shift()));
    line(tag, "seq_pos_min", static_cast<long long>(c.seq_pos_min(seq)));
    line(tag, "seq_pos_max", static_cast<long long>(c.seq_pos_max(seq)));
    const auto & sp = c.seq_pos_get(seq);
    line(tag, "seq_pos_count", static_cast<long long>(sp.size()));
}

} // namespace

int main() {
    // S1: lifecycle and empty indexes.
    {
        llama_kv_cells c;
        c.resize(3);
        dump("S1", c, 0);
        line("S1", "is_empty_0", c.is_empty(0) ? 1 : 0);
    }

    // S2: fill positions, ordered used min/max.
    {
        llama_kv_cells c;
        c.resize(3);
        c.pos_set(0, 10);
        c.pos_set(2, 0);
        dump("S2", c, 0);
    }

    // S3: overlapping sequences, position order, tok_le tie.
    {
        llama_kv_cells c;
        c.resize(4);
        c.pos_set(0, 5);
        c.pos_set(1, 7);
        c.pos_set(2, 5);
        c.seq_add(0, 1);
        c.seq_add(1, 1);
        c.seq_add(2, 1);
        c.seq_add(0, 2);

        llama_kv_cell_ext e0{};
        e0.tok = 1001;
        llama_kv_cell_ext e1{};
        e1.tok = 1002;
        llama_kv_cell_ext e2{};
        e2.tok = 1003;
        c.ext_set(0, e0);
        c.ext_set(1, e1);
        c.ext_set(2, e2);

        dump("S3", c, 1);
        line("S3", "seq_count_0", c.seq_count(0));
        line("S3", "seq_has_0_2", c.seq_has(0, 2) ? 1 : 0);
        line("S3", "tok_le_4", static_cast<long long>(c.seq_pos_tok_le(1, 4)));
        line("S3", "tok_le_5", static_cast<long long>(c.seq_pos_tok_le(1, 5)));
        line("S3", "tok_le_7", static_cast<long long>(c.seq_pos_tok_le(1, 7)));
    }

    // S4: shift accumulation and negative shift clearing.
    {
        llama_kv_cells c;
        c.resize(2);
        c.pos_set(0, 100);
        c.pos_set(1, 3);
        c.seq_add(0, 1);
        c.seq_add(1, 1);

        const bool e0 = c.pos_add(0, 10);
        line("S4", "add_pos0_ret", e0 ? 1 : 0);
        line("S4", "pos0", static_cast<long long>(c.pos_get(0)));
        line("S4", "shift0", static_cast<long long>(c.get_shift(0)));
        line("S4", "has_shift", c.get_has_shift() ? 1 : 0);
        line("S4", "seq_pos_min", static_cast<long long>(c.seq_pos_min(1)));
        line("S4", "seq_pos_max", static_cast<long long>(c.seq_pos_max(1)));

        c.pos_div(0, 2);
        line("S4", "pos0_div", static_cast<long long>(c.pos_get(0)));
        line("S4", "shift0_div", static_cast<long long>(c.get_shift(0)));

        const bool e1 = c.pos_add(1, -10);
        line("S4", "add_neg_ret", e1 ? 1 : 0);
        line("S4", "empty_1", c.is_empty(1) ? 1 : 0);
        line("S4", "used", static_cast<long long>(c.get_used()));
        line("S4", "seq_pos_count", static_cast<long long>(c.seq_pos_get(1).size()));

        c.reset_shift();
        line("S4", "has_shift_after_reset", c.get_has_shift() ? 1 : 0);
        line("S4", "shift0_after_reset", static_cast<long long>(c.get_shift(0)));
    }

    // S5: cp keeps indexes empty while copying cells.
    {
        llama_kv_cells c;
        c.resize(3);
        c.pos_set(0, 4);
        c.pos_set(1, 6);
        c.seq_add(0, 1);
        c.reset_shift();

        llama_kv_cells snap = c.cp(0, 2);
        dump("S5", snap, 1);

        llama_kv_cells q;
        q.resize(3);
        q.set(0, snap);
        dump("S5SET", q, 1);
        line("S5SET", "pos0", static_cast<long long>(q.pos_get(0)));
        line("S5SET", "pos1", static_cast<long long>(q.pos_get(1)));
    }

    // S6: seq_rm / seq_keep / rm transitions.
    {
        llama_kv_cells c;
        c.resize(3);
        c.pos_set(0, 5);
        c.seq_add(0, 1);
        c.seq_add(0, 2);
        line("S6", "seq_rm_ret", c.seq_rm(0, 1) ? 1 : 0);
        line("S6", "seq_count", c.seq_count(0));
        line("S6", "seq_single", c.seq_get(0));
        line("S6", "seq_keep_ret", c.seq_keep(0, 2) ? 1 : 0);
        line("S6", "seq_keep_other_ret", c.seq_keep(0, 3) ? 1 : 0);
        line("S6", "empty_0", c.is_empty(0) ? 1 : 0);
        line("S6", "used", static_cast<long long>(c.get_used()));
    }

    // S7: cp_list / set_list with an explicit index list.
    {
        llama_kv_cells c;
        c.resize(3);
        c.pos_set(0, 4);
        c.pos_set(2, 9);
        c.reset_shift();

        const std::vector<uint32_t> idxs = { 2, 0 };
        llama_kv_cells snap = c.cp(idxs);
        line("S7", "cp_size", static_cast<long long>(snap.size()));
        line("S7", "cp_pos0", static_cast<long long>(snap.pos_get(0)));
        line("S7", "cp_pos1", static_cast<long long>(snap.pos_get(1)));
        line("S7", "cp_used", static_cast<long long>(snap.get_used()));

        llama_kv_cells q;
        q.resize(3);
        q.set(idxs, snap);
        line("S7", "set_used", static_cast<long long>(q.get_used()));
        line("S7", "set_pos2", static_cast<long long>(q.pos_get(2)));
        line("S7", "set_pos0", static_cast<long long>(q.pos_get(0)));
    }

    return 0;
}
