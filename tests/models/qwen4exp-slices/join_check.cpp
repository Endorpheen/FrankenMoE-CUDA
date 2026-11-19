// C1-02 join gate: the canon/qwen4exp.goose id space and the q4t_ tensor map
// must describe the same tensors.
//
// Both modules are linked into one binary. For every layer kind and every
// tensor id the shape slice answers "owned" and the map slice answers
// "declared"; the two answers have to agree, otherwise an id in one module
// names a different tensor than the same id in the other and the integration
// step is a rename, not a join.
//
// Tensor id 5 (per_layer_token_embd) is excluded: it is global and gated by the
// PLE metadata, so neither a layer kind nor the map alone decides it. That is
// covered separately by q4t_present_ple_table and q4m_ple_group_present.

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include "gen.h"

static int g_cases = 0, g_values = 0, g_fails = 0;

static void bad(const char * what, const std::string & detail) {
    ++g_fails;
    if (g_fails <= 100) printf("FAIL %s: %s\n", what, detail.c_str());
}

extern "C" void fg200g_fail(int64_t code) {
    ++g_fails;
    printf("FAIL shim: canon abort %lld\n", (long long) code);
}
extern "C" void fg200g_st_i64(uint64_t base, int64_t byte, int64_t v) {
    (void) base; (void) byte; (void) v;
}
extern "C" int64_t fg200g_ld_u32(uint64_t base, int64_t index) {
    (void) base; (void) index;
    return 0;
}
extern "C" void q4t_fail(int64_t code) {
    ++g_fails;
    printf("FAIL shim: map abort %lld\n", (long long) code);
}

// The canon parameter vector, same layout as tests/fixture.cpp of EXP580.
static std::vector<int64_t> params() {
    std::vector<int64_t> p(27, 0);
    p[0] = 2048; p[1] = 151936; p[2] = 48; p[3] = 49;
    p[4] = 16; p[5] = 4; p[6] = 8192; p[7] = 256; p[8] = 8;
    p[9] = 0; p[10] = 0; p[11] = 4; p[12] = 4096; p[13] = 128;
    p[14] = 32; p[15] = 4; p[16] = 128; p[17] = 4; p[18] = 4;
    p[19] = 512; p[20] = 4; p[21] = 128; p[22] = 64; p[23] = 65536;
    p[24] = 4; p[25] = 48; p[26] = 1;
    return p;
}

int main() {
    goose_init(0, nullptr);

    std::vector<int64_t> p = params();
    const int64_t nt = q4t_count();
    const int64_t ncanon = 52;

    ++g_cases;
    g_values += 1;
    if (nt != ncanon) {
        bad("id-space", "map count " + std::to_string((long long) nt) +
                        " canon count " + std::to_string((long long) ncanon));
    }

    ++g_cases;
    for (int64_t kind = 0; kind < 3; ++kind) {
        for (int64_t tid = 0; tid < ncanon; ++tid) {
            if (tid == 5) continue;
            std::vector<int64_t> out(3, -1);
            const int64_t owned = q4_shape({p.data(), (int64_t) p.size()}, kind, tid,
                                           {out.data(), 3});
            const int64_t declared = q4t_present(kind, tid);
            ++g_values;
            if (owned != declared) {
                bad("join", "kind " + std::to_string((long long) kind) + " tid " +
                            std::to_string((long long) tid) + ": shape owns " +
                            std::to_string((long long) owned) + ", map declares " +
                            std::to_string((long long) declared));
            }
        }
    }

    // The PLE table: canon gates it on the metadata triple, the map on the head
    // count. With a PLE layer both say yes; with none both say no.
    ++g_cases;
    {
        std::vector<int64_t> out(3, -1);
        const int64_t with_ple = q4_shape({p.data(), (int64_t) p.size()}, 1, 5, {out.data(), 3});
        ++g_values;
        if (with_ple != 1 || q4t_present_ple_table(16) != 1) {
            bad("ple-table-on", "canon " + std::to_string((long long) with_ple));
        }
        std::vector<int64_t> p0 = p;
        p0[25] = -1;
        const int64_t no_ple = q4_shape({p0.data(), (int64_t) p0.size()}, 1, 5, {out.data(), 3});
        ++g_values;
        if (no_ple != 0 || q4t_present_ple_table(0) != 0) {
            bad("ple-table-off", "canon " + std::to_string((long long) no_ple));
        }
    }

    printf("C1_02_JOIN cases=%d values=%d failures=%d\n", g_cases, g_values, g_fails);
    return g_fails == 0 ? 0 : 1;
}
