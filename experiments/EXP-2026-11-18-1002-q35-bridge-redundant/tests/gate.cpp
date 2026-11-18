// Two objects in the shipped build provide the same Apex plan decision:
//   fg-q35.o        generated from goose/model/qwen35*.goose (the canonical unit)
//   fg-q35-bridge.o frozen from experiments/, still linked, called by nothing
//
// EXP1001 showed the second is uncalled. Uncalled is not the same as redundant: it
// could be the only correct one. This calls both with the same inputs and compares
// their answers, which is what a removal decision needs (rule 15 forbids removing
// until the replacement is proven, not until it is merely present).
#include <cstdio>
#include <cstdint>
#include <vector>

extern "C" int      fg_q35s_enabled(void);
extern "C" int64_t  fg_q35s_plan(const int64_t * p, int64_t np, int64_t flags_base, int64_t * kinds, int64_t nk);
extern "C" int64_t  fg_q35s_kind(const int64_t * p, int64_t np, int64_t il, int64_t flags_base);
extern "C" int64_t  fg_q35s_shape(const int64_t * p, int64_t np, int64_t kind, int64_t tid, int64_t * out, int64_t nout);

// Slices are passed BY VALUE as {data,len}, which is the ABI this project settled on in
// EXP905: a raw-pointer declaration links and then corrupts. The first version of this
// gate declared them the C way and hung, because the callee read a struct as a pointer.
struct sl_i64 { int64_t * data; int64_t len; };

extern "C" {
extern int64_t q35_plan(sl_i64 params, int64_t flags_base, sl_i64 kinds);
extern int64_t q35_kind(sl_i64 params, int64_t il, int64_t flags_base);
extern int64_t q35_shape(sl_i64 params, int64_t kind, int64_t tid, sl_i64 out);
}

static int checks = 0, fails = 0;

static void check(int cond, const char *what) {
    checks++;
    if (!cond) { fails++; std::printf("FAIL %s\n", what); }
}

// The unit is compiled with -Dgoose_init=fg950_goose_init (build.py), so its exports
// wait on THAT initialiser. Calling the library's plain goose_init leaves them refusing
// with "goose_init must be called before exported functions" - a message that names the
// wrong symbol, which is why the first two attempts looked like a hang.
extern "C" void goose_init(int argc, char **argv);
// fg-q35.o and fg-qg.o each define their own goose_init under a renamed symbol, and the
// bridge calls both on first use (integration/m1/common/fg-apex-plan-bridge.c). Calling
// only goose_init leaves the unit's exports refusing; calling fg950_goose_init addresses
// a different object entirely - the tokenizer unit - which is why three attempts failed
// in three different ways before the bridge source was read.
extern "C" void fg_q35_goose_init(int argc, char **argv);
extern "C" void fg_qg_goose_init(int argc, char **argv);

int main() {
    // The generated runtime refuses exported calls until it is initialised; without this
    // the first goose call aborted on stderr while stdout showed nothing, which read as
    // a hang rather than as a refusal.
    goose_init(0, (char **) 0);
    fg_q35_goose_init(0, (char **) 0);
    fg_qg_goose_init(0, (char **) 0);

    // Apex-35B shape as the working gate uses it (integration/goose-standalone/tests/
    // gdn_plan.c): 19 parameters, p[3] = all layers, p[17] = attention interval.
    // The first version of this gate filled only p[0] and both sides returned 0, which
    // looked like agreement and was two refusals.
    int64_t p[19];
    for (int i = 0; i < 19; i++) p[i] = 0;
    const int64_t n_layer = 40, n_nextn = 1;
    p[2]  = n_layer;
    p[3]  = n_layer + n_nextn;      // 41
    p[4]  = 40;                     // head_count
    p[5]  = 8;                      // head_count_kv
    p[6]  = 8192;                   // feed_forward_length
    p[7]  = 256;                    // expert_count
    p[8]  = 8;                      // expert_used_count
    p[9]  = 512;                    // expert_feed_forward_length
    p[10] = 1024;                   // expert_shared_feed_forward_length
    p[11] = 4;                      // ssm.conv_kernel
    p[12] = 4096;                   // ssm.inner_size
    p[13] = 128;                    // ssm.state_size
    p[14] = 32;                     // ssm.time_step_rank
    p[15] = 4;                      // ssm.group_count
    p[16] = 128;                    // attention.key_length
    p[17] = 4;                      // full_attention_interval
    p[18] = 1;                      // has_output

    std::printf("bridge enabled: %d\n", fg_q35s_enabled());

    std::vector<int64_t> old_kinds((size_t) p[3], -99), new_kinds((size_t) p[3], -99);
    const int64_t n_old = fg_q35s_plan(p, 19, 0, old_kinds.data(), p[3]);
    const int64_t n_new = q35_plan(sl_i64{ p, 19 }, 0,
                                   sl_i64{ new_kinds.data(), p[3] });
    std::printf("plan length: bridge %lld, unit %lld\n", (long long) n_old, (long long) n_new);
    check(n_old == n_new && n_old == 41, "both plans are 41 layers");

    if (n_old == n_new && n_old > 0) {
        int same = 1;
        for (int64_t i = 0; i < n_old; i++) {
            if (old_kinds[(size_t) i] != new_kinds[(size_t) i]) {
                same = 0;
                std::printf("  layer %lld: bridge %lld, unit %lld\n", (long long) i,
                            (long long) old_kinds[(size_t) i], (long long) new_kinds[(size_t) i]);
            }
        }
        check(same, "every layer kind agrees");
        int a = 0, r = 0, m = 0;
        for (int64_t i = 0; i < n_new; i++) {
            const int64_t k = new_kinds[(size_t) i];
            if (k == 0) a++; else if (k == 1) r++; else if (k == 2) m++;
        }
        std::printf("unit: attn %d reqr %d mtp %d\n", a, r, m);
        check(a > 0 && r > 0 && m > 0, "the plan contains all three kinds");
    }

    int kind_same = 1;
    for (int64_t il = -1; il <= p[3]; il++) {
        const int64_t x = fg_q35s_kind(p, 19, il, 0);
        const int64_t y = q35_kind(sl_i64{ p, 19 }, il, 0);
        if (x != y) {
            kind_same = 0;
            std::printf("  kind layer %lld: bridge %lld, unit %lld\n", (long long) il,
                        (long long) x, (long long) y);
        }
    }
    check(kind_same, "per-layer kind agrees, including out-of-range layers");

    int shape_same = 1, shape_checked = 0;
    for (int64_t kind = 0; kind <= 2; kind++) {
        for (int64_t tid = 0; tid < 4; tid++) {
            std::vector<int64_t> x(8, -77), y(8, -77);
            const int64_t nx = fg_q35s_shape(p, 19, kind, tid, x.data(), 8);
            const int64_t ny = q35_shape(sl_i64{ p, 19 }, kind, tid, sl_i64{ y.data(), 8 });
            shape_checked++;
            if (nx != ny) {
                shape_same = 0;
                std::printf("  shape kind %lld tid %lld: lengths %lld vs %lld\n",
                            (long long) kind, (long long) tid, (long long) nx, (long long) ny);
                continue;
            }
            for (int64_t k = 0; k < nx && k < 8; k++) {
                if (x[(size_t) k] != y[(size_t) k]) {
                    shape_same = 0;
                    std::printf("  shape kind %lld tid %lld field %lld: %lld vs %lld\n",
                                (long long) kind, (long long) tid, (long long) k,
                                (long long) x[(size_t) k], (long long) y[(size_t) k]);
                }
            }
        }
    }
    std::printf("shape comparisons: %d\n", shape_checked);
    check(shape_same, "shapes agree for every kind and tensor id");

    std::printf("%s checks=%d failures=%d\n", fails == 0 ? "PASS" : "BAD", checks, fails);
    return fails ? 1 : 0;
}
