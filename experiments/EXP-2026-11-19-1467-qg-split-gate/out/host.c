// The gate for the joint QG split: what the module says against what the vendored native line says.
//
// The module is compiled on its own with a capture host, so the four numbers it would hand to the graph builder are
// recorded instead of being turned into a tensor. The expectation is re-derived here from the native expression
// (vendor/flint/src/models/qwen35moe.cpp:293-300), transcribed rather than imported, and the test also checks the two
// properties the split depends on: the second view starts one head width in, and a head stride that is one head width
// instead of two would silently interleave garbage.
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

struct sl_i64 { int64_t * data; int64_t len; };
typedef int64_t (*qg_fn)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, struct sl_i64);

// the capture host: the module's two externs
struct view_args { int64_t c, a, n0, n1, n2, n3, nb1, nb2, nb3, offset; };
static struct view_args g_views[8];
static int g_n_views;
static int g_failed;
static int64_t g_fail_code;

int64_t fg158_view4(int64_t c, int64_t a, int64_t n0, int64_t n1, int64_t n2, int64_t n3,
                    int64_t nb1, int64_t nb2, int64_t nb3, int64_t offset) {
    if (g_n_views < 8) {
        g_views[g_n_views] = (struct view_args){ c, a, n0, n1, n2, n3, nb1, nb2, nb3, offset };
    }
    return 1000 + g_n_views++;
}
void fg158_fail(int64_t code) { g_failed = 1; g_fail_code = code; }

// Linked directly rather than dlopen'd: the module's own externs are provided by this file, so a shared object would need
// them resolved at load time and the capture would live in a different copy of them.
int64_t qg_split(int64_t c, int64_t qfull, int64_t head_dim, int64_t n_head, int64_t n_tokens, int64_t elem,
                 struct sl_i64 out);

void goose_init(int argc, char ** argv);

int main(int argc, char ** argv) {
    (void) argc; (void) argv;
    goose_init(0, NULL);   // the generated unit refuses every export until its heap is set up
    qg_fn split = (qg_fn) qg_split;

    // shapes an Apex-class model produces: head_dim 128, n_head 40, n_tokens 1..512, elem 2 and 4 bytes
    struct { int64_t head_dim, n_head, n_tokens, elem; } cases[] = {
        { 128, 40, 1, 2 }, { 128, 40, 128, 2 }, { 128, 40, 512, 2 }, { 128, 40, 4096, 2 },
        { 128, 40, 1, 4 }, { 256, 8, 64, 2 }, { 64, 4, 16, 2 },
    };
    int fails = 0;
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i) {
        g_n_views = 0; g_failed = 0;
        int64_t out[2] = { 0, 0 };
        const int64_t n = split(0, 0, cases[i].head_dim, cases[i].n_head, cases[i].n_tokens, cases[i].elem,
                                (struct sl_i64){ out, 2 });
        if (g_failed) { ++fails; printf("  REFUSED code=%lld for head_dim=%lld\n", (long long) g_fail_code,
                                        (long long) cases[i].head_dim); continue; }
        // the native expression, transcribed from vendor/flint/src/models/qwen35moe.cpp:293-300
        const int64_t nb1 = cases[i].elem * cases[i].head_dim * 2;
        const int64_t nb2 = nb1 * cases[i].n_head;
        const int64_t nb3 = nb2 * cases[i].n_tokens;
        if (n != 2 || g_n_views != 2) { ++fails; printf("  VIEWS n=%lld captured=%d\n", (long long) n, g_n_views); continue; }
        for (int v = 0; v < 2; ++v) {
            const struct view_args * w = &g_views[v];
            if (w->n0 != cases[i].head_dim || w->n1 != cases[i].n_head || w->n2 != cases[i].n_tokens) {
                ++fails; printf("  SHAPE view%d n=(%lld,%lld,%lld) want (%lld,%lld,%lld)\n", v,
                    (long long) w->n0, (long long) w->n1, (long long) w->n2,
                    (long long) cases[i].head_dim, (long long) cases[i].n_head, (long long) cases[i].n_tokens);
            }
            if (w->nb1 != nb1 || w->nb2 != nb2 || w->nb3 != nb3) {
                ++fails; printf("  STRIDES view%d nb=(%lld,%lld,%lld) want (%lld,%lld,%lld)\n", v,
                    (long long) w->nb1, (long long) w->nb2, (long long) w->nb3,
                    (long long) nb1, (long long) nb2, (long long) nb3);
            }
        }
        // the gate view starts exactly one head width inside each head, and nothing else moves
        if (g_views[1].offset != cases[i].elem * cases[i].head_dim) {
            ++fails; printf("  OFFSET %lld want %lld\n", (long long) g_views[1].offset,
                            (long long) (cases[i].elem * cases[i].head_dim));
        }
        if (g_views[0].offset != 0) { ++fails; printf("  QUERY OFFSET %lld want 0\n", (long long) g_views[0].offset); }
    }
    // refusals: a zero dimension must stop loudly rather than return a plausible handle
    struct { int64_t hd, nh, nt, el; int64_t code; } ref[] = {
        { 0, 40, 1, 2, 60 }, { 128, 0, 1, 2, 61 }, { 128, 40, 0, 2, 62 }, { 128, 40, 1, 0, 63 },
    };
    for (size_t i = 0; i < sizeof(ref)/sizeof(ref[0]); ++i) {
        g_n_views = 0; g_failed = 0;
        int64_t out[2] = { 0, 0 };
        split(0, 0, ref[i].hd, ref[i].nh, ref[i].nt, ref[i].el, (struct sl_i64){ out, 2 });
        if (!g_failed || g_fail_code != ref[i].code) { ++fails;
            printf("  REFUSAL hd=%lld nh=%lld nt=%lld el=%lld failed=%d code=%lld want=%lld\n",
                   (long long) ref[i].hd, (long long) ref[i].nh, (long long) ref[i].nt, (long long) ref[i].el,
                   g_failed, (long long) g_fail_code, (long long) ref[i].code); }
    }

    printf("qg split: shapes=%zu refusals=%zu failures=%d\n", sizeof(cases)/sizeof(cases[0]),
           sizeof(ref)/sizeof(ref[0]), fails);
    return fails == 0 ? 0 : 1;
}
