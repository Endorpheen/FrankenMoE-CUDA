// Gate for the eight tokenizer decisions the composed Goose unit exports (P11-01 / EXP-683).
//
// migration-status used to call the tokenizer engine "linked, not yet called". These functions are pure
// integer decisions with no I/O, so the cheapest way to promote that claim from "linked" to "answers" is
// to call every one of them with known inputs and compare. Expected values are taken from the native
// semantics the module transcribes, not from running the module and recording what it said.
//
// Not covered here: the load-and-tokenise path around the decisions (reading a vocab, driving the merge
// loop, producing ids). That is the engine, and it is still missing.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "main.h"

// The unit's validation hook. Provided here so the gate links with the unit and the host primitives only,
// without pulling graph-bindings.o and with it ggml: this gate tests decisions, not kernels.
void fg202t_fail(int64_t c);
void fg202t_fail(int64_t c) {
    fprintf(stderr, "FG202T validation code %lld\n", (long long) c);
    abort();
}

static int failures = 0;
static int checks   = 0;

static void expect(const char * name, int64_t got, int64_t want) {
    ++checks;
    if (got != want) {
        ++failures;
        fprintf(stderr, "FAIL %s: got %lld want %lld\n", name, (long long) got, (long long) want);
    }
}

void goose_init(int argc, char **argv);

int main(void) {
    goose_init(0, (char **) 0);
    // utf8 sequence length from the first byte: hi nibble <= 11 -> 1, <= 13 -> 2, == 14 -> 3, else 4
    expect("utf8_len 0x00", tok_utf8_len(0x00), 1);
    expect("utf8_len 0x7f", tok_utf8_len(0x7f), 1);
    expect("utf8_len 0xaf", tok_utf8_len(0xaf), 1);
    expect("utf8_len 0xb0", tok_utf8_len(0xb0), 1);
    expect("utf8_len 0xc0", tok_utf8_len(0xc0), 2);
    expect("utf8_len 0xdf", tok_utf8_len(0xdf), 2);
    expect("utf8_len 0xe0", tok_utf8_len(0xe0), 3);
    expect("utf8_len 0xef", tok_utf8_len(0xef), 3);
    expect("utf8_len 0xf0", tok_utf8_len(0xf0), 4);
    expect("utf8_len 0xff", tok_utf8_len(0xff), 4);

    // bigram valid: both sides non-empty and the sizes must add up
    expect("bigram 0,0,0",  tok_bigram_valid(0, 0, 0), 0);
    expect("bigram 1,0,1",  tok_bigram_valid(1, 0, 1), 0);
    expect("bigram 0,1,1",  tok_bigram_valid(0, 1, 1), 0);
    expect("bigram 1,1,3",  tok_bigram_valid(1, 1, 3), 0);
    expect("bigram 1,1,2",  tok_bigram_valid(1, 1, 2), 1);

    // try add: neither neighbour may be -1, the token must not be the null token, and it must be in range
    expect("add -1 left",   tok_try_add_allowed(-1, 0, 0, 0, 10), 0);
    expect("add -1 right",  tok_try_add_allowed(0, -1, 0, 0, 10), 0);
    expect("add null tok",  tok_try_add_allowed(0, 0, 5, 5, 10), 0);
    expect("add out range", tok_try_add_allowed(0, 0, 10, 5, 10), 0);
    expect("add ok",        tok_try_add_allowed(0, 0, 3, 5, 10), 1);

    // resegment kind: 0 already a token, 1 has a merge, 2 neither
    expect("reseg token",   tok_resegment_kind(1, 0), 0);
    expect("reseg token+m", tok_resegment_kind(1, 1), 0);
    expect("reseg merge",   tok_resegment_kind(0, 1), 1);
    expect("reseg none",    tok_resegment_kind(0, 0), 2);

    // clamp symbol length: min(len, remaining), both positive
    expect("clamp 3/5",     tok_clamp_symbol_len(3, 5), 3);
    expect("clamp 5/3",     tok_clamp_symbol_len(5, 3), 3);
    expect("clamp 4/4",     tok_clamp_symbol_len(4, 4), 4);

    // bpe add: neighbours not -1 and a non-negative rank
    expect("bpe add -1 l",  tok_bpe_add_allowed(-1, 0, 0), 0);
    expect("bpe add -1 r",  tok_bpe_add_allowed(0, -1, 0), 0);
    expect("bpe add rank<0",tok_bpe_add_allowed(0, 0, -1), 0);
    expect("bpe add ok",    tok_bpe_add_allowed(0, 0, 0), 1);

    // bpe merge valid: both sides non-empty and the concat must match
    expect("bpe merge 0,1", tok_bpe_merge_valid(0, 1, 1), 0);
    expect("bpe merge 1,0", tok_bpe_merge_valid(1, 0, 1), 0);
    expect("bpe merge nom", tok_bpe_merge_valid(1, 1, 0), 0);
    expect("bpe merge ok",  tok_bpe_merge_valid(1, 1, 1), 1);

    // ignore merges: both flags must be set
    expect("ignore 1,1",    tok_bpe_ignore_merges(1, 1), 1);
    expect("ignore 1,0",    tok_bpe_ignore_merges(1, 0), 0);
    expect("ignore 0,1",    tok_bpe_ignore_merges(0, 1), 0);
    expect("ignore 0,0",    tok_bpe_ignore_merges(0, 0), 0);

    printf("FG_TOKENIZER_DECISIONS checks=%d failures=%d %s\n",
           checks, failures, failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
