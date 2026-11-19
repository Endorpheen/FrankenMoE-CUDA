// Gate for the grammar first-set port: the allowed first codepoints at the current stacks.
//
// Oracle: llama_grammar_match_char from the native file compiled into the fixture is asked
// about the same probe codepoints the port's ranges are tested with. A codepoint is allowed
// when any stack accepts it, which is the same union the reject intersection implies.
//
// The port is the exported fg_grammar_first_set of the live module. It reports ranges for
// positive classes and a negative code when the decision cannot be expressed as ranges; those
// cases must be reported as a decline, never answered with a wrong range.
#include "../../../vendor/flint/src/llama-grammar.cpp"
#include "runtime-gen.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

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

// The module indexes rows with a stride of 8 (g_first_set), so rows are padded to 8.
static const int64_t kStride = 8;
static const int64_t kMaxRanges = 64;

static const std::vector<uint32_t> kProbes = {
    0x00, 0x09, 0x0A, 0x1F, 0x20, 0x30, 0x39, 0x3A, 0x40, 0x41, 0x5B, 0x5C, 0x5D, 0x60,
    0x61, 0x62, 0x63, 0x64, 0x7A, 0x7B, 0x7D, 0x7F,
    0x80, 0xC3, 0xE9, 0x100, 0x2FFF, 0x3000, 0x4E00, 0xFFFD, 0x10FFFF,
};

struct Port {
    // returns the raw return code; ranges land in lo/hi
    int64_t run(const std::vector<int64_t> & et_v, const std::vector<int64_t> & ev_v,
                const std::vector<int64_t> & stacks_v, const std::vector<int64_t> & stackd_v,
                int64_t nstacks, std::vector<int64_t> & lo, std::vector<int64_t> & hi) {
        std::vector<int64_t> et(et_v), ev(ev_v), stk(stacks_v), stdk(stackd_v);
        lo.assign((size_t) kMaxRanges, 0);
        hi.assign((size_t) kMaxRanges, 0);
        sl_i64 s_et = sl(et), s_ev = sl(ev), s_stk = sl(stk), s_std = sl(stdk);
        sl_i64 s_lo = sl(lo), s_hi = sl(hi);
        return fg_grammar_first_set(s_et, s_ev, s_stk, s_std, nstacks, s_lo, s_hi, kMaxRanges, 181);
    }
};

// Native verdict for one codepoint: does any stack accept it as a first codepoint?
static bool native_allows(const std::vector<const llama_grammar_element *> & tops, uint32_t cp) {
    for (const auto * top : tops) {
        if (llama_grammar_match_char(top, cp).first) return true;
    }
    return false;
}

static void one_ranges(const char * name, const std::vector<const llama_grammar_element *> & tops,
                       const std::vector<int64_t> & et, const std::vector<int64_t> & ev,
                       const std::vector<int64_t> & stacks, const std::vector<int64_t> & stackd,
                       int64_t nstacks) {
    std::vector<int64_t> lo, hi;
    Port p;
    const int64_t r = p.run(et, ev, stacks, stackd, nstacks, lo, hi);
    cases++;
    if (r < 0) {
        fails++;
        printf("FAIL %s port declined (%lld) but the tops are positive classes\n", name,
               (long long) r);
        return;
    }
    for (uint32_t cp : kProbes) {
        bool in_port = false;
        for (int64_t i = 0; i < r; ++i) {
            if (lo[(size_t) i] <= (int64_t) cp && (int64_t) cp <= hi[(size_t) i]) in_port = true;
        }
        const bool in_native = native_allows(tops, cp);
        if (in_port != in_native) {
            fails++;
            printf("MISMATCH %s cp=0x%X native=%d port=%d ranges=%lld\n", name, cp,
                   in_native ? 1 : 0, in_port ? 1 : 0, (long long) r);
            return;
        }
    }
}

static void one_declines(const char * name, const std::vector<int64_t> & et,
                         const std::vector<int64_t> & ev, const std::vector<int64_t> & stacks,
                         const std::vector<int64_t> & stackd, int64_t nstacks) {
    std::vector<int64_t> lo, hi;
    Port p;
    const int64_t r = p.run(et, ev, stacks, stackd, nstacks, lo, hi);
    cases++;
    if (r >= 0) {
        fails++;
        printf("FAIL %s expected a decline, port returned %lld ranges\n", name, (long long) r);
    }
}

int main() {
    goose_init(0, nullptr);

    // 1. one stack, one range: [a-c]
    //    rule 0: 'a'-'c' END   elements 0..2
    {
        llama_grammar_rules r(1);
        r[0] = { { LLAMA_GRETYPE_CHAR, 'a' }, { LLAMA_GRETYPE_CHAR_RNG_UPPER, 'c' },
                 { LLAMA_GRETYPE_END, 0 } };
        std::vector<int64_t> et{ 3, 5, 0, 0, 0, 0, 0, 0 };
        std::vector<int64_t> ev{ 'a', 'c', 0, 0, 0, 0, 0, 0 };
        std::vector<int64_t> stacks{ 0, 0, 0, 0, 0, 0, 0, 0 };
        std::vector<int64_t> stackd{ 1 };
        one_ranges("one-range", { r[0].data() }, et, ev, stacks, stackd, 1);
    }

    // 2. two stacks: [a-c] on one, [0-9] on the other - the union must be reported
    //    rule 0: 'a'-'c' END at 0..2 ; rule 1: '0'-'9' END at 3..5
    {
        llama_grammar_rules r(2);
        r[0] = { { LLAMA_GRETYPE_CHAR, 'a' }, { LLAMA_GRETYPE_CHAR_RNG_UPPER, 'c' },
                 { LLAMA_GRETYPE_END, 0 } };
        r[1] = { { LLAMA_GRETYPE_CHAR, '0' }, { LLAMA_GRETYPE_CHAR_RNG_UPPER, '9' },
                 { LLAMA_GRETYPE_END, 0 } };
        std::vector<int64_t> et{ 3, 5, 0, 3, 5, 0, 0, 0 };
        std::vector<int64_t> ev{ 'a', 'c', 0, '0', '9', 0, 0, 0 };
        std::vector<int64_t> stacks{ 0, 0, 0, 0, 0, 0, 0, 0,
                                     3, 0, 0, 0, 0, 0, 0, 0 };
        std::vector<int64_t> stackd{ 1, 1 };
        one_ranges("two-stacks", { r[0].data(), r[1].data() }, et, ev, stacks, stackd, 2);
    }

    // 3. CHAR_ANY at the top: every codepoint is allowed
    {
        llama_grammar_rules r(1);
        r[0] = { { LLAMA_GRETYPE_CHAR_ANY, 0 }, { LLAMA_GRETYPE_END, 0 } };
        std::vector<int64_t> et{ 7, 0, 0, 0, 0, 0, 0, 0 };
        std::vector<int64_t> ev{ 0, 0, 0, 0, 0, 0, 0, 0 };
        std::vector<int64_t> stacks{ 0, 0, 0, 0, 0, 0, 0, 0 };
        std::vector<int64_t> stackd{ 1 };
        one_ranges("char-any", { r[0].data() }, et, ev, stacks, stackd, 1);
    }

    // 4. several alternates inside one class element chain: 'a' | 'x' written as a class
    //    with two CHAR_ALT parts, which the module walks as one range list
    {
        llama_grammar_rules r(1);
        r[0] = { { LLAMA_GRETYPE_CHAR, 'a' }, { LLAMA_GRETYPE_CHAR_ALT, 'x' },
                 { LLAMA_GRETYPE_END, 0 } };
        std::vector<int64_t> et{ 3, 6, 0, 0, 0, 0, 0, 0 };
        std::vector<int64_t> ev{ 'a', 'x', 0, 0, 0, 0, 0, 0 };
        std::vector<int64_t> stacks{ 0, 0, 0, 0, 0, 0, 0, 0 };
        std::vector<int64_t> stackd{ 1 };
        one_ranges("class-alt", { r[0].data() }, et, ev, stacks, stackd, 1);
    }

    // 5. an inverse class cannot be expressed as positive ranges: the port must decline
    {
        llama_grammar_rules r(1);
        r[0] = { { LLAMA_GRETYPE_CHAR_NOT, 'a' }, { LLAMA_GRETYPE_END, 0 } };
        std::vector<int64_t> et{ 4, 0, 0, 0, 0, 0, 0, 0 };
        std::vector<int64_t> ev{ 'a', 0, 0, 0, 0, 0, 0, 0 };
        std::vector<int64_t> stacks{ 0, 0, 0, 0, 0, 0, 0, 0 };
        std::vector<int64_t> stackd{ 1 };
        one_declines("char-not", et, ev, stacks, stackd, 1);
    }

    // 6. a rule reference at the top is not a first set either: it must decline
    {
        std::vector<int64_t> et{ 2, 0, 0, 0, 0, 0, 0, 0 };
        std::vector<int64_t> ev{ 1, 0, 0, 0, 0, 0, 0, 0 };
        std::vector<int64_t> stacks{ 0, 0, 0, 0, 0, 0, 0, 0 };
        std::vector<int64_t> stackd{ 1 };
        one_declines("rule-ref-top", et, ev, stacks, stackd, 1);
    }

    printf("gate: cases=%d fails=%d\n", cases, fails);
    return fails ? 1 : 0;
}
