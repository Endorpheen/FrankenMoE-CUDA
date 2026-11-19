// Gate for the rule-reference branch of the grammar reject port.
//
// The native automaton is compiled into the fixture, so llama_grammar_reject_candidates is
// called directly and is the oracle. The port is the Goosed module in source/, reached through
// its generated header. Every case compares the reject verdict candidate by candidate.
//
// Shapes covered:
//   - a class followed by a reference to a one-alternate rule with a continuation;
//   - the EXP-1471 shape, space ::= " " | <empty>, where one alternate is empty;
//   - two non-empty alternates, where every row must be tried;
//   - a reference to a reference to an alternation (two descents);
//   - a negative class;
//   - an incomplete UTF-8 piece next to a complete one;
//   - nesting deeper than the per-row pool, which must decline rather than guess.
#include "../../../vendor/flint/src/llama-grammar.cpp"
#include "runtime-gen.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

// The oracle file mentions logging, abort and three vocabulary accessors that no reject path
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

struct Cand {
    std::vector<uint32_t> cps;
    uint32_t pval = 0;
    int n_remain = 0;
};

// window sizes as the port sees them
static const int64_t kMaxs = 8;
static const int64_t kMaxd = 16;
static const int64_t kMaxcp = 8;
static const int64_t kLevels = 4;

struct Port {
    int64_t run(const std::vector<int64_t> & et_v, const std::vector<int64_t> & ev_v,
                const std::vector<int64_t> & ro_v, const std::vector<int64_t> & rl_v,
                const std::vector<int64_t> & seed_v,
                const std::vector<Cand> & cps, std::vector<int64_t> & rej) {
        const int64_t ncand = (int64_t) cps.size();
        std::vector<int64_t> cp((size_t) (ncand * kMaxcp), 0), cpl((size_t) ncand, 0);
        std::vector<int64_t> pv((size_t) ncand, 0), pn((size_t) ncand, 0), cid((size_t) ncand, 0);
        for (int64_t i = 0; i < ncand; ++i) {
            cpl[(size_t) i] = (int64_t) cps[(size_t) i].cps.size();
            for (size_t j = 0; j < cps[(size_t) i].cps.size() && (int64_t) j < kMaxcp; ++j) {
                cp[(size_t) (i * kMaxcp + (int64_t) j)] = (int64_t) cps[(size_t) i].cps[j];
            }
            pv[(size_t) i] = (int64_t) cps[(size_t) i].pval;
            pn[(size_t) i] = (int64_t) cps[(size_t) i].n_remain;
            cid[(size_t) i] = i;
        }
        std::vector<int64_t> et(et_v), ev(ev_v), ro(ro_v), rl(rl_v);
        std::vector<int64_t> seed(seed_v), stk(seed_v);
        std::vector<int64_t> wp((size_t) (kMaxs * kMaxd), 0), wpd((size_t) kMaxs, 0);
        std::vector<int64_t> stdk((size_t) kMaxs, 0), scratch((size_t) kMaxd, 0);
        std::vector<int64_t> tmp_rej((size_t) ncand, 0), tmp_cur((size_t) ncand, 0);
        std::vector<int64_t> rowbuf((size_t) kMaxd, 0);
        std::vector<int64_t> pool((size_t) (kLevels * (3 * ncand + kMaxd)), 0);
        rej.assign((size_t) ncand, 0);
        stdk[0] = (int64_t) seed_v.size();

        sl_i64 s_et = sl(et), s_ev = sl(ev), s_ro = sl(ro), s_rl = sl(rl), s_seed = sl(seed);
        sl_i64 s_cp = sl(cp), s_cpl = sl(cpl), s_pv = sl(pv), s_pn = sl(pn), s_cid = sl(cid);
        sl_i64 s_wp = sl(wp), s_wpd = sl(wpd), s_stk = sl(stk), s_std = sl(stdk);
        sl_i64 s_scr = sl(scratch), s_tr = sl(tmp_rej), s_tc = sl(tmp_cur), s_row = sl(rowbuf);
        sl_i64 s_rej = sl(rej), s_pool = sl(pool);
        return fg_grammar_reject(s_et, s_ev, s_ro, s_rl, s_seed, (int64_t) seed_v.size(),
                                 s_cp, s_cpl, s_pv, s_pn, s_cid, ncand,
                                 s_wp, s_wpd, s_stk, s_std, s_scr, s_tr, s_tc, s_row, s_rej,
                                 kMaxs, kMaxd, kMaxcp, s_pool, kLevels, 180);
    }
};

// One case: compare native and port verdicts, or require an explicit decline.
static void one(const char * name, llama_grammar_rules & rules,
                const std::vector<const llama_grammar_element *> & stack,
                const std::vector<Cand> & cps,
                const std::vector<int64_t> & et, const std::vector<int64_t> & ev,
                const std::vector<int64_t> & ro, const std::vector<int64_t> & rl,
                const std::vector<int64_t> & flat_stack, bool expect_decline) {
    llama_grammar_stacks stacks{ stack };
    llama_grammar_candidates cands;
    std::vector<std::vector<uint32_t>> storage;
    storage.reserve(cps.size());
    for (size_t i = 0; i < cps.size(); ++i) {
        storage.push_back(cps[i].cps);
        storage.back().push_back(0);
        llama_grammar_candidate c;
        c.index = i;
        c.code_points = storage.back().data();
        c.partial_utf8.n_remain = cps[i].n_remain;
        c.partial_utf8.value = cps[i].pval;
        c.id = (llama_token) i;
        cands.push_back(c);
    }
    auto nrej = llama_grammar_reject_candidates(rules, stacks, cands);
    std::vector<int> native(cps.size(), 0);
    for (auto & r : nrej) native[(size_t) r.index] = 1;

    std::vector<int64_t> rej;
    Port p;
    const int64_t r = p.run(et, ev, ro, rl, flat_stack, cps, rej);

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

    // 1. a class, then a one-alternate rule reference with a continuation
    //    rule 0: '{' REF1 '"' END     globals 0..3
    //    rule 1: ' ' END              globals 4..5
    {
        llama_grammar_rules r(2);
        r[0] = { { LLAMA_GRETYPE_CHAR, '{' }, { LLAMA_GRETYPE_RULE_REF, 1 },
                 { LLAMA_GRETYPE_CHAR, '"' }, { LLAMA_GRETYPE_END, 0 } };
        r[1] = { { LLAMA_GRETYPE_CHAR, ' ' }, { LLAMA_GRETYPE_END, 0 } };
        one("single-alternate", r, { r[0].data() },
            { { { '{', '"' } }, { { '{', ' ' } }, { { '{', ' ', '"' } }, { { 'x' } },
              { { '{', '{' } } },
            { 3, 2, 3, 0, 3, 0, 0, 0 }, { '{', 1, '"', 0, ' ', 0, 0, 0 }, { 0, 4 }, { 4, 2 },
            { 0 }, false);
    }

    // 2. the EXP-1471 shape: space ::= " " | <empty>
    {
        llama_grammar_rules r(2);
        r[0] = { { LLAMA_GRETYPE_CHAR, '{' }, { LLAMA_GRETYPE_RULE_REF, 1 },
                 { LLAMA_GRETYPE_CHAR, '"' }, { LLAMA_GRETYPE_END, 0 } };
        r[1] = { { LLAMA_GRETYPE_CHAR, ' ' }, { LLAMA_GRETYPE_ALT, 0 },
                 { LLAMA_GRETYPE_END, 0 } };
        one("multi-alternate-empty", r, { r[0].data() },
            { { { '{', '"' } }, { { '{', ' ' } }, { { '{', ' ', '"' } }, { { 'x' } },
              { { '{', '{' } } },
            { 3, 2, 3, 0, 3, 1, 0, 0 }, { '{', 1, '"', 0, ' ', 0, 0, 0 }, { 0, 4 }, { 4, 3 },
            { 0 }, false);
    }

    // 3. two non-empty alternates: every row must be tried
    //    rule 0: '{' REF1 END        globals 0..2
    //    rule 1: 'a' | 'b' END       globals 3..6
    {
        llama_grammar_rules r(2);
        r[0] = { { LLAMA_GRETYPE_CHAR, '{' }, { LLAMA_GRETYPE_RULE_REF, 1 },
                 { LLAMA_GRETYPE_END, 0 } };
        r[1] = { { LLAMA_GRETYPE_CHAR, 'a' }, { LLAMA_GRETYPE_ALT, 0 },
                 { LLAMA_GRETYPE_CHAR, 'b' }, { LLAMA_GRETYPE_END, 0 } };
        one("two-alternates", r, { r[0].data() },
            { { { '{', 'a' } }, { { '{', 'b' } }, { { '{', 'c' } }, { { '{' } } },
            { 3, 2, 0, 3, 1, 3, 0, 0 }, { '{', 1, 0, 'a', 0, 'b', 0, 0 }, { 0, 3 }, { 3, 4 },
            { 0 }, false);
    }

    // 4. a reference to a reference to an alternation: two descents before the rows
    //    rule 0: '{' REF1 '"' END     globals 0..3
    //    rule 1: REF2 END             globals 4..5
    //    rule 2: ' ' | <empty>        globals 6..8
    {
        llama_grammar_rules r(3);
        r[0] = { { LLAMA_GRETYPE_CHAR, '{' }, { LLAMA_GRETYPE_RULE_REF, 1 },
                 { LLAMA_GRETYPE_CHAR, '"' }, { LLAMA_GRETYPE_END, 0 } };
        r[1] = { { LLAMA_GRETYPE_RULE_REF, 2 }, { LLAMA_GRETYPE_END, 0 } };
        r[2] = { { LLAMA_GRETYPE_CHAR, ' ' }, { LLAMA_GRETYPE_ALT, 0 },
                 { LLAMA_GRETYPE_END, 0 } };
        one("nested-reference", r, { r[0].data() },
            { { { '{', '"' } }, { { '{', ' ', '"' } }, { { '{', ' ', ' ' } }, { { 'x' } } },
            { 3, 2, 3, 0, 2, 0, 3, 1, 0 }, { '{', 1, '"', 0, 2, 0, ' ', 0, 0 },
            { 0, 4, 6 }, { 4, 2, 3 }, { 0 }, false);
    }

    // 5. negative class: everything but 'a' starts the sequence
    {
        llama_grammar_rules r(1);
        r[0] = { { LLAMA_GRETYPE_CHAR_NOT, 'a' }, { LLAMA_GRETYPE_END, 0 } };
        one("char-not", r, { r[0].data() },
            { { { 'b' } }, { { 'a' } }, { { 0xE9 } }, { { 'b', 'a' } } },
            { 4, 0, 0, 0, 0, 0, 0, 0 }, { 'a', 0, 0, 0, 0, 0, 0, 0 }, { 0 }, { 2 },
            { 0 }, false);
    }

    // 6. an incomplete UTF-8 piece next to a complete one
    //    rule 0: 'a' | [0x100, 0x2FFF] END
    {
        llama_grammar_rules r(2);
        r[0] = { { LLAMA_GRETYPE_CHAR, 'a' }, { LLAMA_GRETYPE_ALT, 0 },
                 { LLAMA_GRETYPE_CHAR, 0x100 }, { LLAMA_GRETYPE_CHAR_RNG_UPPER, 0x2FFF },
                 { LLAMA_GRETYPE_END, 0 } };
        r[1] = { { LLAMA_GRETYPE_CHAR, 'z' }, { LLAMA_GRETYPE_END, 0 } };
        Cand lead;
        lead.pval = 0xC3;
        lead.n_remain = 1;
        Cand ascii;
        ascii.cps = { 'a' };
        one("partial-lead", r, { r[0].data() }, { lead, ascii },
            { 3, 1, 3, 5, 0, 0, 0, 0 }, { 'a', 0, 0x100, 0x2FFF, 0, 0, 0, 0 }, { 0, 5 },
            { 5, 2 }, { 0 }, false);
    }

    // 7. nesting deeper than the per-row pool: the call must decline, never guess
    {
        llama_grammar_rules r(6);
        std::vector<int64_t> et, ev, ro, rl;
        auto emit = [&](const std::vector<llama_grammar_element> & v) {
            ro.push_back((int64_t) et.size());
            for (auto & e : v) { et.push_back((int64_t) e.type); ev.push_back((int64_t) e.value); }
            rl.push_back((int64_t) v.size());
        };
        r[0] = { { LLAMA_GRETYPE_CHAR, '{' }, { LLAMA_GRETYPE_RULE_REF, 1 },
                 { LLAMA_GRETYPE_END, 0 } };
        emit(r[0]);
        for (int k = 1; k <= 4; ++k) {
            r[(size_t) k] = { { LLAMA_GRETYPE_RULE_REF, (uint32_t) (k + 1) },
                              { LLAMA_GRETYPE_ALT, 0 }, { LLAMA_GRETYPE_CHAR, 'q' },
                              { LLAMA_GRETYPE_END, 0 } };
            emit(r[(size_t) k]);
        }
        r[5] = { { LLAMA_GRETYPE_CHAR, ' ' }, { LLAMA_GRETYPE_ALT, 0 },
                 { LLAMA_GRETYPE_END, 0 } };
        emit(r[5]);
        while (et.size() < 64) { et.push_back(0); ev.push_back(0); }
        one("deep-nesting", r, { r[0].data() }, { { { '{', ' ' } } }, et, ev, ro, rl,
            { 0 }, true);
    }

    printf("gate: cases=%d fails=%d\n", cases, fails);
    return fails ? 1 : 0;
}
