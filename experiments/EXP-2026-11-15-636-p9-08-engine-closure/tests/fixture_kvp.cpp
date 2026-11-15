// Differential gate for P9-08 S3: the request's KV state — clearing, prefix
// reuse, draft rewind and turn rollback.
//
// Oracle: a transcription of runtime/core/src/engine/session.cpp:1324-1337
// (clear_kv), :1434-1455 (prefix reuse and draft rewind), :1459-1478
// (rollback_turn), :1575-1576 and :1854 (what joins the token list). Port:
// source/kvp-kv-turn-state.goose.
//
// The KV, the decode and the chat template are inputs here: the gate compares
// the numbers the engine derives from them, over synthetic token lists.
#include <cstdio>
#include <cstdint>
#include <cstring>

#include "kvp.h"

static int g_cases = 0, g_checks = 0, g_fails = 0;

static void expect(const char * group, const char * what, int64_t got, int64_t want) {
    ++g_checks;
    if (got != want) {
        ++g_fails;
        if (g_fails <= 80) {
            std::printf("FAIL %s / %s: got %lld want %lld\n", group, what, (long long) got, (long long) want);
        }
    }
}

// ── oracle: transcribed from session.cpp ───────────────────────────────────

// session.cpp:1324-1337
static int64_t nat_clear(int64_t clear_kv, int64_t has_draft, int64_t has_sampler) {
    if (!clear_kv) return 0;
    int64_t m = 1 + 4 + 8; // target, history, tokens
    if (has_draft) m += 2;
    if (has_sampler) m += 16;
    return m;
}

// session.cpp:1436
static int64_t nat_max_common(int64_t n_tokens) { return n_tokens > 0 ? n_tokens - 1 : 0; }

// session.cpp:1437
static int64_t nat_common(const int64_t * prev, int64_t prev_len, const int64_t * cur, int64_t cur_len,
                          int64_t max_common) {
    int64_t n = 0;
    while (n < prev_len && n < max_common && prev[n] == cur[n]) ++n;
    return n;
}

// session.cpp:1438-1447
static void nat_rewind(int64_t prev_len, int64_t common, int64_t seq_rm_ok, int64_t * out) {
    int64_t action = 0, n = common;
    if (common < prev_len) {
        if (seq_rm_ok) {
            action = 1;
        } else {
            action = 2;
            n = 0;
        }
    }
    out[0] = action;
    out[1] = n;
}

// session.cpp:1448-1455
static int64_t nat_draft_rewind(int64_t has_draft, int64_t seq_rm_ok) {
    if (!has_draft) return 0;
    return seq_rm_ok ? 1 : 2;
}

// session.cpp:1459-1478
static void nat_rollback(int64_t chat_on, int64_t n_common, int64_t history_pushed, int64_t seq_rm_ok,
                         int64_t * out) {
    int64_t action = 0, tokens_len = 0, popped = 0, draft_keep = -1, keep = 0;
    if (chat_on) {
        keep = n_common;
        tokens_len = n_common;
        if (seq_rm_ok) {
            action = 1;
        } else {
            action = 2;
            keep = 0;
        }
        if (history_pushed) popped = 1;
        draft_keep = n_common;
    } else {
        action = 2;
        keep = 0;
        draft_keep = 0;
    }
    out[0] = action;
    out[1] = tokens_len;
    out[2] = popped;
    out[3] = draft_keep;
    out[4] = keep;
}

int main() {
    goose_init(0, nullptr);

    // ── clear_kv: which stores a new chat drops ─────────────────────────────
    {
        struct c { const char * name; int64_t clear, draft, sampler, want; } cases[] = {
            {"continued turn",      0, 1, 1,  0},
            {"continued, bare",     0, 0, 0,  0},
            {"bare session",        1, 0, 0, 13},
            {"with draft",          1, 1, 0, 15},
            {"with sampler",        1, 0, 1, 29},
            {"with both",           1, 1, 1, 31},
        };
        for (const auto & c : cases) {
            ++g_cases;
            expect(c.name, "mask", kvp_clear_actions(c.clear, c.draft, c.sampler), c.want);
            expect(c.name, "mask vs oracle", kvp_clear_actions(c.clear, c.draft, c.sampler),
                   nat_clear(c.clear, c.draft, c.sampler));
        }
    }

    // ── the reuse ceiling ───────────────────────────────────────────────────
    {
        struct c { const char * name; int64_t n, want; } cases[] = {
            {"empty prompt",   0, 0},
            {"one token",      1, 0},
            {"two tokens",     2, 1},
            {"typical",      100, 99},
            {"long",       32768, 32767},
        };
        for (const auto & c : cases) {
            ++g_cases;
            expect(c.name, "ceiling", kvp_max_common(c.n), c.want);
            expect(c.name, "ceiling vs oracle", kvp_max_common(c.n), nat_max_common(c.n));
        }
    }

    // ── the matching prefix ─────────────────────────────────────────────────
    {
        const int64_t base[] = {1, 2, 3, 4, 5};
        const int64_t same[] = {1, 2, 3, 4, 5};
        const int64_t shorter[] = {1, 2, 3};
        const int64_t diverge[] = {1, 2, 9, 4, 5};
        const int64_t first_diff[] = {9, 2, 3, 4, 5};
        const int64_t appended[] = {1, 2, 3, 4, 5, 6, 7};
        const int64_t empty[] = {0};

        struct c {
            const char * name; const int64_t * prev; int64_t plen;
            const int64_t * cur; int64_t clen; int64_t max_common; int64_t want;
        } cases[] = {
            {"identical",        base, 5, same, 5, 10, 5},
            {"current shorter",  base, 5, shorter, 3, 10, 3},
            {"diverges at 2",    base, 5, diverge, 5, 10, 2},
            {"first token diff", base, 5, first_diff, 5, 10, 0},
            {"previous shorter", shorter, 3, base, 5, 10, 3},
            {"appended current", base, 5, appended, 7, 10, 5},
            {"ceiling 0",        base, 5, same, 5, 0, 0},
            {"ceiling 3",        base, 5, same, 5, 3, 3},
            {"ceiling above both", base, 5, same, 5, 99, 5},
            {"empty previous",   empty, 0, base, 5, 10, 0},
            {"empty current",    base, 5, empty, 0, 10, 0},
        };
        for (const auto & c : cases) {
            ++g_cases;
            const int64_t got = kvp_common_len(sl_i64{(int64_t *) c.prev, c.plen},
                                               sl_i64{(int64_t *) c.cur, c.clen}, c.max_common);
            expect(c.name, "common", got, c.want);
            expect(c.name, "common vs oracle", got,
                   nat_common(c.prev, c.plen, c.cur, c.clen, c.max_common));
        }

        // The ceiling really is applied to a real prompt: 5 identical tokens
        // reuse 4, never 5, so the turn always decodes something.
        ++g_cases;
        int64_t out[2] = {-1, -1};
        const int64_t maxc = kvp_max_common(5);
        expect("never empty", "ceiling on a full match",
               kvp_common_len(sl_i64{(int64_t *) base, 5}, sl_i64{(int64_t *) same, 5}, maxc), 4);
        expect("never empty", "one token left", 5 - 4, 1);
        expect("never empty", "guard", kvp_never_empty(5), 1);
    }

    // ── the rewind decision ─────────────────────────────────────────────────
    {
        const int64_t prevs[] = {0, 5};
        const int64_t commons[] = {0, 3, 5};
        for (int64_t plen : prevs) {
            for (int64_t common : commons) {
                for (int64_t ok = 0; ok <= 1; ++ok) {
                    ++g_cases;
                    char name[96];
                    std::snprintf(name, sizeof(name), "rewind prev=%lld common=%lld ok=%lld",
                                  (long long) plen, (long long) common, (long long) ok);
                    int64_t got[2] = {-1, -1};
                    int64_t want[2] = {-1, -1};
                    const int64_t action = kvp_rewind(plen, common, ok, sl_i64{got, 2});
                    nat_rewind(plen, common, ok, want);
                    expect(name, "action", action, want[0]);
                    expect(name, "common", got[0], want[0]);
                    expect(name, "new common", got[1], want[1]);
                    // A cache that is already a prefix is not touched at all.
                    if (common == plen) {
                        expect(name, "keep", got[0], 0);
                    }
                }
            }
        }
        ++g_cases;
        int64_t small[1] = {-1};
        expect("rewind", "short slice refused", kvp_rewind(5, 3, 1, sl_i64{small, 1}), -1);
        expect("rewind", "slot untouched", small[0], -1);
    }

    // ── the draft rewind ────────────────────────────────────────────────────
    {
        ++g_cases;
        expect("draft", "no draft context", kvp_draft_rewind(0, 1), 0);
        expect("draft", "no draft, refused", kvp_draft_rewind(0, 0), 0);
        expect("draft", "removal accepted", kvp_draft_rewind(1, 1), 1);
        expect("draft", "removal refused", kvp_draft_rewind(1, 0), 2);
        for (int64_t d = 0; d <= 1; ++d) {
            for (int64_t ok = 0; ok <= 1; ++ok) {
                expect("draft", "vs oracle", kvp_draft_rewind(d, ok), nat_draft_rewind(d, ok));
            }
        }
    }

    // ── rollback_turn over its whole grid ───────────────────────────────────
    {
        const int64_t commons[] = {0, 7};
        for (int64_t chat = 0; chat <= 1; ++chat) {
            for (int64_t pushed = 0; pushed <= 1; ++pushed) {
                for (int64_t ok = 0; ok <= 1; ++ok) {
                    for (int64_t common : commons) {
                        ++g_cases;
                        char name[112];
                        std::snprintf(name, sizeof(name),
                                      "rollback chat=%lld pushed=%lld ok=%lld common=%lld",
                                      (long long) chat, (long long) pushed, (long long) ok,
                                      (long long) common);
                        int64_t got[5] = {-9, -9, -9, -9, -9};
                        int64_t want[5] = {-9, -9, -9, -9, -9};
                        const int64_t action =
                            kvp_rollback_keep(chat, common, pushed, ok, sl_i64{got, 5});
                        nat_rollback(chat, common, pushed, ok, want);
                        expect(name, "action", action, want[0]);
                        for (int i = 0; i < 5; ++i) {
                            expect(name, "field", got[i], want[i]);
                        }
                        // The invariant a caller depends on: a raw-prompt turn
                        // never keeps a prefix.
                        if (!chat) {
                            expect(name, "raw turn clears", got[4], 0);
                        }
                    }
                }
            }
        }
        ++g_cases;
        int64_t small[4] = {-1, -1, -1, -1};
        expect("rollback", "short slice refused",
               kvp_rollback_keep(1, 7, 1, 1, sl_i64{small, 4}), -1);
        expect("rollback", "slot untouched", small[4 - 1], -1);
    }

    // ── what a prefill appends ──────────────────────────────────────────────
    {
        struct c { const char * name; int64_t chat, common, prompt, want; } cases[] = {
            {"raw prompt appends nothing", 0,  0, 40,  0},
            {"full prefill",               1,  0, 40, 40},
            {"suffix only",                1, 30, 40, 10},
            {"nothing new",                1, 40, 40,  0},
            {"prefix past the prompt",     1, 50, 40,  0},
            {"empty prompt",               1,  0,  0,  0},
        };
        for (const auto & c : cases) {
            ++g_cases;
            expect(c.name, "span", kvp_prefill_span(c.chat, c.common, c.prompt), c.want);
        }

        ++g_cases;
        expect("append", "chat token joins", kvp_append_token(1, 0), 1);
        expect("append", "raw token does not", kvp_append_token(0, 0), 0);
        expect("append", "speculative token does not", kvp_append_token(1, 1), 0);
        expect("append", "raw speculative does not", kvp_append_token(0, 1), 0);
    }

    // ── the never-empty invariant ───────────────────────────────────────────
    {
        ++g_cases;
        expect("invariant", "empty prompt", kvp_never_empty(0), 0);
        expect("invariant", "one token", kvp_never_empty(1), 1);
        expect("invariant", "two tokens", kvp_never_empty(2), 1);
        expect("invariant", "long prompt", kvp_never_empty(4096), 1);
        for (int64_t n = 1; n <= 64; ++n) {
            expect("invariant", "reuse is strictly shorter", kvp_max_common(n) < n ? 1 : 0, 1);
        }
    }

    std::printf("FG634_KVP cases=%d checks=%d failures=%d\n", g_cases, g_checks, g_fails);
    return g_fails == 0 ? 0 : 1;
}
