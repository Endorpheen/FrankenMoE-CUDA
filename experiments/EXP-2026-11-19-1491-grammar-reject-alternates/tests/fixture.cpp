// Gate for the rule-reference branch of the grammar reject port.
//
// The native automaton is compiled into the fixture, so llama_grammar_reject_candidates
// is called directly and is the oracle. The port is the Goosed module in source/,
// reached through its generated header.
//
// The shape under test is the one measured in EXP-1471: a character class is followed by
// a reference to a rule that has more than one alternate (`space ::= " "?`). The native
// advance_stack pushes the continuation and every alternate, then intersects their
// rejects; the port used to keep the first alternate only and reject a valid token.
#include "../../../vendor/flint/src/llama-grammar.cpp"
#include "runtime-gen.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

// The oracle file mentions logging, abort and two vocabulary accessors that no reject path
// reaches here. They are stubbed rather than linked from the shipped library, because linking
// it would put a second copy of the non-static grammar symbols into the same process and the
// fixture could silently call that copy instead of the one it compiled in.
extern "C" void ggml_abort(const char * file, int line, const char * fmt, ...) {
    (void) file; (void) line; (void) fmt;
    abort();
}
void llama_log_internal(ggml_log_level level, const char * fmt, ...) {
    (void) level; (void) fmt;
}
bool llama_vocab::is_eog(llama_token) const { return false; }
const std::string & llama_vocab::token_to_piece(llama_token) const {
    static const std::string empty;
    return empty;
}
int32_t llama_vocab::tokenize(const char *, int32_t, llama_token *, int32_t, bool, bool) const {
    return 0;
}

static int fails = 0, cases = 0;

static sl_i64 sl(std::vector<int64_t> & v) { return sl_i64{ v.data(), (int64_t) v.size() }; }

struct Port {
    // returns the port verdict per candidate: 1 rejected, 0 accepted, and the raw return
    int64_t run(const int64_t * et, const int64_t * ev, const int64_t * ro, const int64_t * rl,
                int64_t nelem, int64_t nrules,
                const std::vector<int64_t> & stack,
                const std::vector<std::vector<uint32_t>> & cps,
                std::vector<int64_t> & rej) {
        const int64_t maxcp = 4;
        const int64_t ncand = (int64_t) cps.size();
        std::vector<int64_t> cp(ncand * maxcp, 0), cpl(ncand, 0), pv(ncand, 0), pn(ncand, 0);
        std::vector<int64_t> cid(ncand, 0);
        for (int64_t i = 0; i < ncand; ++i) {
            cpl[i] = (int64_t) cps[i].size();
            for (size_t j = 0; j < cps[i].size() && (int64_t) j < maxcp; ++j) {
                cp[i * maxcp + j] = (int64_t) cps[i][j];
            }
            cid[i] = i;
        }
        std::vector<int64_t> et_v(et, et + nelem), ev_v(ev, ev + nelem);
        std::vector<int64_t> ro_v(ro, ro + nrules), rl_v(rl, rl + nrules);
        std::vector<int64_t> seed_v(stack), stk(stack);
        int64_t maxs = 8, maxd = 8;
        std::vector<int64_t> wp(maxs * maxd, 0), wpd(maxs, 0), stdk(maxs, 0);
        std::vector<int64_t> scratch(maxd, 0), tmp_rej(ncand, 0), tmp_cur(ncand, 0), rowbuf(maxd, 0);
        rej.assign(ncand, 0);
        stdk[0] = (int64_t) stack.size();

        sl_i64 s_et = sl(et_v), s_ev = sl(ev_v), s_ro = sl(ro_v), s_rl = sl(rl_v);
        sl_i64 s_seed = sl(seed_v);
        sl_i64 s_cp = sl(cp), s_cpl = sl(cpl), s_pv = sl(pv), s_pn = sl(pn), s_cid = sl(cid);
        sl_i64 s_wp = sl(wp), s_wpd = sl(wpd), s_stk = sl(stk), s_std = sl(stdk);
        sl_i64 s_scr = sl(scratch), s_tr = sl(tmp_rej), s_tc = sl(tmp_cur), s_row = sl(rowbuf);
        sl_i64 s_rej = sl(rej);
        return fg_grammar_reject(s_et, s_ev, s_ro, s_rl, s_seed, (int64_t) stack.size(),
                                 s_cp, s_cpl, s_pv, s_pn, s_cid, ncand,
                                 s_wp, s_wpd, s_stk, s_std, s_scr, s_tr, s_tc, s_row, s_rej,
                                 maxs, maxd, maxcp, 180);
    }
};

// One case: compare the native rejects with the port, or require a decline.
static void one(const char * name, llama_grammar_rules & rules,
                const std::vector<const llama_grammar_element *> & stack,
                const std::vector<std::vector<uint32_t>> & cps,
                const int64_t * et, const int64_t * ev, const int64_t * ro, const int64_t * rl,
                int64_t nelem, int64_t nrules,
                const std::vector<int64_t> & flat_stack, bool expect_decline) {
    llama_grammar_stacks stacks{ stack };
    llama_grammar_candidates cands;
    std::vector<std::vector<uint32_t>> storage;
    storage.reserve(cps.size());
    for (size_t i = 0; i < cps.size(); ++i) {
        storage.push_back(cps[i]);
        storage.back().push_back(0);
        cands.push_back({ (int) i, storage.back().data(), {}, (llama_token) i });
    }
    auto nrej = llama_grammar_reject_candidates(rules, stacks, cands);
    std::vector<int> native(cps.size(), 0);
    for (auto & r : nrej) native[(size_t) r.index] = 1;

    std::vector<int64_t> rej;
    Port p;
    const int64_t r = p.run(et, ev, ro, rl, nelem, nrules, flat_stack, cps, rej);

    cases++;
    if (expect_decline) {
        if (r != -2) {
            fails++;
            printf("FAIL %s expected a decline (-2), port returned %lld\n", name, (long long) r);
        }
        return;
    }
    if (r < 0) {
        fails++;
        printf("FAIL %s port declined (%lld) but must decide\n", name, (long long) r);
        return;
    }
    for (size_t i = 0; i < cps.size(); ++i) {
        if (native[i] != rej[i]) {
            fails++;
            printf("MISMATCH %s cand=%zu native=%d goose=%lld\n", name, i, native[i],
                   (long long) rej[i]);
            return;
        }
    }
}

int main() {
    goose_init(0, nullptr);
    // ---- case 1: a class, then a rule reference with ONE alternate and a continuation
    //  rule 0: '{' RULE_REF 1 '"'  END      globals 0..3
    //  rule 1: ' ' END                     globals 4..5
    {
        llama_grammar_rules r(2);
        r[0].push_back({ LLAMA_GRETYPE_CHAR, '{' });
        r[0].push_back({ LLAMA_GRETYPE_RULE_REF, 1 });
        r[0].push_back({ LLAMA_GRETYPE_CHAR, '"' });
        r[0].push_back({ LLAMA_GRETYPE_END, 0 });
        r[1].push_back({ LLAMA_GRETYPE_CHAR, ' ' });
        r[1].push_back({ LLAMA_GRETYPE_END, 0 });
        int64_t et[8] = { 3, 2, 3, 0, 3, 0, 0, 0 };
        int64_t ev[8] = { '{', 1, '"', 0, ' ', 0, 0, 0 };
        int64_t ro[2] = { 0, 4 };
        int64_t rl[2] = { 4, 2 };
        std::vector<const llama_grammar_element *> stack{ r[0].data() };
        std::vector<int64_t> flat{ 0 };
        one("single-alternate", r, stack,
            { { '{', '"' }, { '{', ' ' }, { '{', ' ', '"' }, { 'x' }, { '{', '{' } },
            et, ev, ro, rl, 8, 2, flat, false);
    }

    // ---- case 2: the EXP-1471 shape - the rule has an empty alternate
    //  rule 0: '{' RULE_REF 1 '"' END      globals 0..3
    //  rule 1: ' ' | <empty>               globals 4..6  (' ' ALT END)
    {
        llama_grammar_rules r(2);
        r[0].push_back({ LLAMA_GRETYPE_CHAR, '{' });
        r[0].push_back({ LLAMA_GRETYPE_RULE_REF, 1 });
        r[0].push_back({ LLAMA_GRETYPE_CHAR, '"' });
        r[0].push_back({ LLAMA_GRETYPE_END, 0 });
        r[1].push_back({ LLAMA_GRETYPE_CHAR, ' ' });
        r[1].push_back({ LLAMA_GRETYPE_ALT, 0 });
        r[1].push_back({ LLAMA_GRETYPE_END, 0 });
        int64_t et[8] = { 3, 2, 3, 0, 3, 1, 0, 0 };
        int64_t ev[8] = { '{', 1, '"', 0, ' ', 0, 0, 0 };
        int64_t ro[2] = { 0, 4 };
        int64_t rl[2] = { 4, 3 };
        std::vector<const llama_grammar_element *> stack{ r[0].data() };
        std::vector<int64_t> flat{ 0 };
        one("multi-alternate", r, stack,
            { { '{', '"' }, { '{', ' ' }, { 'x' } },
            et, ev, ro, rl, 8, 2, flat, true);
    }

    printf("gate: cases=%d fails=%d\n", cases, fails);
    return fails ? 1 : 0;
}
