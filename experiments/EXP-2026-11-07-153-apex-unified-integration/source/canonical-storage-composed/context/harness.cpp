// canonical-context harness: real builder/storage/view checks.
// Compares the canonical batch builder against the untouched llama_batch_ext
// oracle for the same script, and validates pointer-fixup/reserve/ownership.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "/inputs/native/src/llama-batch.h"
#include "/inputs/native/src/llama-vocab.h"

extern "C" {
#include "canonical-context.h"
#include "canonical-storage-imports.h"
}

static int failures = 0;
#define CHECK(c, msg) do { if (!(c)) { std::fprintf(stderr, "FAIL %s\n", msg); ++failures; } } while (0)

static int64_t rec_i(int64_t r, int64_t k) { return fgc_record_get(r, k); }
static int32_t vec_i32(int64_t v, int64_t i) { return (int32_t) fgc_raw_load_i32(fgc_raw_ptr_add(fgc_i64_vec_data(v), i * 4)); }
static int8_t vec_i8(int64_t v, int64_t i) { return (int8_t) fgc_raw_load_i8(fgc_raw_ptr_add(fgc_i64_vec_data(v), i)); }
static int64_t vlen(int64_t v) { return fgc_vec_len(v); }

struct Built { int64_t batch; int64_t ctx; };

// Canonical builder script used by both engines.
template <typename Add, typename AddSeq, typename SetId, typename SetPos, typename SetOut, typename SetDec>
static void script(Add add, AddSeq add_seq, SetId set_id, SetPos set_pos, SetOut set_out, SetDec set_dec) {
    int a = add(0);
    int b = add(0);
    add_seq(b, 1);
    int c = add(1);
    llama_pos p0 = 0, p1 = 1, p2 = 1;
    set_pos(a, p0);
    set_pos(b, p1);
    set_pos(c, p2);
    set_id(a, 10); set_id(b, 11); set_id(c, 12);
    set_out(c, true);
    set_dec(b, 7);
}

int main() {
    // ---- builder parity vs untouched oracle ----
    {
        llama_batch_ext oracle(8, 8, 8, 4, nullptr, 100, 1);
        script(
            [&](int s) { return oracle.add_token((llama_seq_id) s); },
            [&](int i, int s) { oracle.add_seq(i, (llama_seq_id) s); },
            [&](int i, int id) { oracle.set_token_id(i, id); },
            [&](int i, llama_pos p) { oracle.set_token_pos(i, &p); },
            [&](int i, bool v) { oracle.set_output(i, v); },
            [&](int i, int o) { oracle.set_decision_order(i, o); });

        fgc_ctx_h ctx = fgc_ctx_new(8, 0, 1, 4, 100, 0);
        fgc_batch_h cb = fgc_batch_new(ctx);
        script(
            [&](int s) { return fgc_batch_add_token(cb, s); },
            [&](int i, int s) { fgc_batch_add_seq(cb, i, s); },
            [&](int i, int id) { fgc_batch_set_token_id(cb, i, id); },
            [&](int i, llama_pos p) { fgc_batch_set_token_pos(cb, i, (int64_t) (intptr_t) &p); },
            [&](int i, bool v) { fgc_batch_set_output(cb, i, v ? 1 : 0); },
            [&](int i, int o) { fgc_batch_set_decision(cb, i, o); });

        CHECK(rec_i(cb, FGC_B_N_TOKENS) == oracle.tokens.size(), "n_tokens");
        CHECK(rec_i(cb, FGC_B_N_EMBD) == oracle.n_embd, "n_embd");
        int64_t tokv = rec_i(cb, FGC_B_V_TOKEN);
        int64_t posv = rec_i(cb, FGC_B_V_POS);
        int64_t outv = rec_i(cb, FGC_B_V_OUTPUT);
        int64_t decv = rec_i(cb, FGC_B_V_DECISION);
        int64_t nsqv = rec_i(cb, FGC_B_V_NSQ);
        int64_t flat = rec_i(cb, FGC_B_V_SEQFLAT);
        int64_t offv = rec_i(cb, FGC_B_V_SEQOFF);
        for (int i = 0; i < 3; ++i) {
            CHECK(vec_i32(tokv, i) == oracle.tokens[i].id, "token id");
            CHECK(vec_i32(posv, i) == oracle.tokens[i].pos[0], "pos");
            if (vec_i32(posv, i) != oracle.tokens[i].pos[0])
                std::fprintf(stderr, "pos mismatch row=%d canon=%d oracle=%d\n", i, (int) vec_i32(posv, i), (int) oracle.tokens[i].pos[0]);
            CHECK(vec_i8(outv, i) == (oracle.tokens[i].output ? 1 : 0), "output");
            CHECK(vec_i32(decv, i) == oracle.tokens[i].decision_order, "decision");
            std::vector<int> a, b;
            for (int k = 0; k < vec_i32(nsqv, i); ++k) a.push_back(vec_i32(flat, vec_i32(offv, i) + k));
            for (auto s : oracle.tokens[i].seq_ids) b.push_back(s);
            std::sort(a.begin(), a.end()); std::sort(b.begin(), b.end());
            if (a != b) {
                std::fprintf(stderr, "seq mismatch row=%d canon=[", i);
                for (int x : a) std::fprintf(stderr, "%d,", x);
                std::fprintf(stderr, "] oracle=[");
                for (int x : b) std::fprintf(stderr, "%d,", x);
                std::fprintf(stderr, "]\n");
            }
            CHECK(a == b, "seq ids");
        }

        // pointer-fixup view
        fgc_ub_h ub = fgc_ubatch_from_batch(cb);
        fgc_ubatch_view view{};
        CHECK(fgc_ubatch_view_build(ub, &view) == 1, "view build");
        CHECK(view.n_tokens == 3 && view.n_seqs == 3 && view.n_seq_tokens == 1, "view counts");
        CHECK(view.token && view.pos && view.n_seq_id && view.seq_id && view.seq_id_unq && view.seq_idx, "view pointers");
        if (view.token && view.seq_id) {
        for (int i = 0; i < 3; ++i) {
                CHECK(view.token[i] == oracle.tokens[i].id, "view token");
                CHECK(view.seq_id[i] != nullptr, "view seq ptr");
                std::vector<int> a, b;
                for (int k = 0; k < view.n_seq_id[i]; ++k) a.push_back(view.seq_id[i][k]);
                for (auto s : oracle.tokens[i].seq_ids) b.push_back(s);
                std::sort(a.begin(), a.end()); std::sort(b.begin(), b.end());
                CHECK(a == b, "view seq ids");
            }
            CHECK(view.seq_idx[0] != -1 && view.seq_idx[1] != -1, "view seq idx");
        }
        CHECK(fgc_ubatch_free(ub) == 0, "ub free");

        // clear
        CHECK(fgc_batch_clear(cb) == 0, "clear");
        CHECK(rec_i(cb, FGC_B_N_TOKENS) == 0 && vlen(rec_i(cb, FGC_B_V_TOKEN)) == 0, "clear fields");
        CHECK(fgc_batch_free(cb) == 0 && fgc_ctx_free(ctx) == 0, "batch free");
    }

    // ---- embd + state ownership ----
    {
        fgc_ctx_h ctx = fgc_ctx_new(8, 4, 1, 4, 100, 0);
        fgc_batch_h b = fgc_batch_new(ctx);
        int i0 = fgc_batch_add_token(b, 0);
        float emb[8] = {0,1,2,3,4,5,6,7};
        float st[4] = {9,8,7,6};
        llama_pos p = 0;
        CHECK(fgc_batch_set_token_pos(b, i0, (int64_t) (intptr_t) &p) == 1, "embd pos");
        CHECK(fgc_batch_set_token_embd(b, i0, (int64_t) (intptr_t) emb, 1, 8) == 1, "set embd");
        CHECK(fgc_batch_set_token_state(b, i0, (int64_t) (intptr_t) st, 1, 4) == 1, "set state");
        CHECK(fgc_batch_set_token_embd(b, i0, (int64_t) (intptr_t) emb, 1, 8) == 0, "duplicate embd rejected");
        CHECK(rec_i(b, FGC_B_N_EMBD) == 8, "embd width");
        int64_t ev = rec_i(b, FGC_B_V_EMBD);
        int64_t sv = rec_i(b, FGC_B_V_STATE);
        CHECK(vlen(ev) == 8 && (float) *(float *) (intptr_t) fgc_i64_vec_data(ev) == 0.0f, "embd bytes");
        CHECK(vlen(sv) == 4, "state bytes");
        CHECK(fgc_batch_free(b) == 0 && fgc_ctx_free(ctx) == 0, "embd free");
    }

    // ---- reserve fix: clear first, only n_pos/n_seqs/token metadata ----
    {
        fgc_ctx_h ctx = fgc_ctx_new(8, 0, 1, 4, 100, 0);
        fgc_batch_h b = fgc_batch_new(ctx);
        // dirty the object first to prove reserve clears it
        fgc_batch_add_token(b, 3);
        fgc_batch_set_decision(b, 0, 42);
        fgc_ub_h ub = fgc_ubatch_reserve(b, 2, 3);
        CHECK(ub != 0, "reserve handle");
        CHECK(rec_i(b, FGC_B_N_TOKENS) == 6, "reserve n_tokens");
        CHECK(rec_i(b, FGC_B_N_USED) == 0 && vlen(rec_i(b, FGC_B_V_USED)) == 0, "reserve no used import");
        CHECK(rec_i(b, FGC_B_N_OUTPUTS) == 0, "reserve no outputs");
        int64_t nsq = rec_i(b, FGC_B_V_NSQ);
        int64_t flat = rec_i(b, FGC_B_V_SEQFLAT);
        int64_t tokv = rec_i(b, FGC_B_V_TOKEN);
        for (int i = 0; i < 6; ++i) {
            CHECK(vec_i32(nsq, i) == 1, "reserve nsq");
            CHECK(vec_i32(flat, i) == i % 3, "reserve seq pattern");
            CHECK(vec_i32(tokv, i) == 0, "reserve token metadata");
        }
        CHECK(rec_i(ub, FGC_U_N_TOKENS) == 6 && rec_i(ub, FGC_U_N_SEQ_TOKENS) == 2, "reserve ub counts");
        CHECK(rec_i(ub, FGC_U_N_SEQS) == 3 && rec_i(ub, FGC_U_EQUAL) == 1, "reserve ub equal");
        int64_t unq = rec_i(ub, FGC_U_V_SEQUNQ);
        CHECK(vlen(unq) == 3 && vec_i32(unq, 0) == 0 && vec_i32(unq, 2) == 2, "reserve seq unq");
        CHECK(fgc_ubatch_free(ub) == 0 && fgc_batch_free(b) == 0 && fgc_ctx_free(ctx) == 0, "reserve free");
    }

    // ---- invalid inputs ----
    {
        fgc_ctx_h ctx = fgc_ctx_new(8, 0, 1, 4, 100, 0);
        fgc_batch_h b = fgc_batch_new(ctx);
        CHECK(fgc_batch_add_token(b, -1) == -3, "invalid seq rejected");
        CHECK(fgc_batch_add_token(b, 4) == -3, "seq >= n_seq_max rejected");
        int i = fgc_batch_add_token(b, 0);
        CHECK(fgc_batch_set_token_id(b, i, 999) == 0, "invalid token rejected");
        float emb[8] = {0};
        CHECK(fgc_batch_set_token_embd(b, i, (int64_t) (intptr_t) emb, 1, 4) == 0, "width mismatch rejected");
        CHECK(fgc_batch_set_token_state(b, i, (int64_t) (intptr_t) emb, 1, 4) == 0, "state on zero-width ctx rejected");
        CHECK(fgc_batch_free(b) == 0 && fgc_ctx_free(ctx) == 0, "invalid free");
    }

    std::printf("canonical entries=%llu\n", (unsigned long long) fgc_ctx_entries());
    if (failures) { std::fprintf(stderr, "FAILED %d\n", failures); return 1; }
    std::printf("OK canonical context/batch/ubatch storage\n");
    return 0;
}
