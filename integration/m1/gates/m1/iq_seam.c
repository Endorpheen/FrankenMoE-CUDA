// M1 seam gate: the unit's IQ views come from the descriptor, not from ggml.
//
// What this gate proves, in one process:
//   1. Before any provider is bound, an IQ view aborts loudly instead of
//      reading a NULL table (checked in a forked child, signal SIGABRT).
//   2. fg_goose_host_init_with_tables() installs the CPU provider and runs
//      goose_init exactly once; a second, different descriptor is refused.
//   3. All 14 unit entry points return exactly what the native accessors in
//      ggml-quants.c return, over every grid entry and over the map/neighbour
//      slots the quantizer builds - so the seam moved the data path, not the
//      values.
//
// The unit calls qd_/qz_/qz2_ names; they are three spellings of the same six
// descriptor entries (grid/map/nb x iq2/iq3), and all three are compared.
//
// Build: cmake -B build-on -DFG_GOOSE_* ... (any flag ON); target fg-iq-seam.
// No model, no server, no GPU.

#include "goose-host.h"
#include "goose-iq-tables.h"

#include "ggml.h"

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

// The unit's entry points, exactly as goose-gen/main.h declares them.
extern int64_t qd_iq2_grid(int64_t group, int64_t index);
extern int64_t qd_iq3_grid(int64_t group, int64_t index);
extern int64_t qz_iq2_map(int64_t group, int64_t unit);
extern int64_t qz_iq2_nb(int64_t group, int64_t offset);
extern int64_t qz_iq2_qgrid(int64_t group, int64_t index);
extern int64_t qz_iq3_map(int64_t group, int64_t unit);
extern int64_t qz_iq3_nb(int64_t group, int64_t offset);
extern int64_t qz_iq3_qgrid(int64_t group, int64_t index);
extern int64_t qz2_iq2_map(int64_t group, int64_t unit);
extern int64_t qz2_iq2_nb(int64_t group, int64_t offset);
extern int64_t qz2_iq2_qgrid(int64_t group, int64_t index);
extern int64_t qz2_iq3_map(int64_t group, int64_t unit);
extern int64_t qz2_iq3_nb(int64_t group, int64_t offset);
extern int64_t qz2_iq3_qgrid(int64_t group, int64_t index);

// The native view, implemented in ggml/src/ggml-quants.c next to the tables.
extern int64_t fg_goose_iq2_grid(int64_t group, int64_t index);
extern int64_t fg_goose_iq3_grid(int64_t group, int64_t index);
extern int64_t fg_goose_iq2_map(int64_t group, int64_t unit);
extern int64_t fg_goose_iq2_nb(int64_t group, int64_t offset);
extern int64_t fg_goose_iq3_map(int64_t group, int64_t unit);
extern int64_t fg_goose_iq3_nb(int64_t group, int64_t offset);

static int64_t g_checks  = 0;
static int64_t g_failed  = 0;

static void expect(int64_t got, int64_t want, const char * what, int64_t group, int64_t at) {
    ++g_checks;
    if (got != want) {
        if (++g_failed <= 10) {
            printf("FG_IQ_SEAM mismatch %s group=%lld at=%lld got=%lld want=%lld\n",
                   what, (long long) group, (long long) at,
                   (long long) got, (long long) want);
        }
    }
}

// 1. The negative control. A child process inherits the unbound state; the
// first IQ view must abort, which is the difference between "tables are data
// installed at init" and "tables are a link-time definition".
static int unbound_view_aborts(void) {
    const pid_t pid = fork();
    if (pid < 0) {
        printf("FG_IQ_SEAM FAIL fork\n");
        return 0;
    }
    if (pid == 0) {
        (void) qd_iq2_grid(0, 0);
        _exit(0); // reached only if the view returned without a provider
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        printf("FG_IQ_SEAM FAIL waitpid\n");
        return 0;
    }
    return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
}

int main(void) {
    // 1. unbound abort
    const int aborts = unbound_view_aborts();

    // 2. bind + one init, and a second provider refused
    const fg_goose_iq_tables * native = fg_goose_iq_tables_native();
    const int bind_rc  = fg_goose_host_init_with_tables(native);
    const int bound    = fg_goose_host_iq_tables_bound();
    const int64_t runs = fg_goose_host_init_runs();
    const int rebind_rc = fg_goose_host_bind_iq_tables(native); // same pointer: 0

    // A second, different descriptor is a second copy of the tables: refused.
    fg_goose_iq_tables other = *native;
    const int replace_rc = fg_goose_host_bind_iq_tables(&other);

    // Build the run-time map/neighbour tables exactly as the engine does.
    static const enum ggml_type types[] = {
        GGML_TYPE_IQ2_XXS, GGML_TYPE_IQ2_XS, GGML_TYPE_IQ1_S,
        GGML_TYPE_IQ1_M,   GGML_TYPE_IQ2_S,  GGML_TYPE_IQ3_XXS,
        GGML_TYPE_IQ3_S,
    };
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i) {
        ggml_quantize_init(types[i]);
    }

    // 3a. every grid entry of both families, through all three spellings
    static const int64_t iq2_grid_len[4] = { 256, 512, 2048, 1024 };
    static const int64_t iq3_grid_len[2] = { 256, 512 };

    for (int64_t g = 0; g < 4; ++g) {
        for (int64_t i = 0; i < iq2_grid_len[g]; ++i) {
            const int64_t want = fg_goose_iq2_grid(g, i);
            expect(qd_iq2_grid(g, i),   want, "qd_iq2_grid",   g, i);
            expect(qz_iq2_qgrid(g, i),  want, "qz_iq2_qgrid",  g, i);
            expect(qz2_iq2_qgrid(g, i), want, "qz2_iq2_qgrid", g, i);
        }
    }
    for (int64_t g = 0; g < 2; ++g) {
        for (int64_t i = 0; i < iq3_grid_len[g]; ++i) {
            const int64_t want = fg_goose_iq3_grid(g, i);
            expect(qd_iq3_grid(g, i),   want, "qd_iq3_grid",   g, i);
            expect(qz_iq3_qgrid(g, i),  want, "qz_iq3_qgrid",  g, i);
            expect(qz2_iq3_qgrid(g, i), want, "qz2_iq3_qgrid", g, i);
        }
    }

    // 3b. the first SLOTS entries of every map and neighbour table. The native
    // tables are kmap_size 43692 (iq2) / 4096 (iq3) and grid_size*2+ neighbours,
    // so 64 is inside every one of them.
    const int64_t SLOTS = 64;
    for (int64_t g = 0; g < 4; ++g) {
        for (int64_t u = 0; u < SLOTS; ++u) {
            const int64_t want_map = fg_goose_iq2_map(g, u);
            expect(qz_iq2_map(g, u),  want_map, "qz_iq2_map",  g, u);
            expect(qz2_iq2_map(g, u), want_map, "qz2_iq2_map", g, u);
            const int64_t want_nb = fg_goose_iq2_nb(g, u);
            expect(qz_iq2_nb(g, u),  want_nb, "qz_iq2_nb",  g, u);
            expect(qz2_iq2_nb(g, u), want_nb, "qz2_iq2_nb", g, u);
        }
    }
    for (int64_t g = 0; g < 2; ++g) {
        for (int64_t u = 0; u < SLOTS; ++u) {
            const int64_t want_map = fg_goose_iq3_map(g, u);
            expect(qz_iq3_map(g, u),  want_map, "qz_iq3_map",  g, u);
            expect(qz2_iq3_map(g, u), want_map, "qz2_iq3_map", g, u);
            const int64_t want_nb = fg_goose_iq3_nb(g, u);
            expect(qz_iq3_nb(g, u),  want_nb, "qz_iq3_nb",  g, u);
            expect(qz2_iq3_nb(g, u), want_nb, "qz2_iq3_nb", g, u);
        }
    }

    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i) {
        ggml_quantize_free();
    }

    const int ok = aborts && bind_rc == 0 && bound == 1 && runs == 1 &&
                   rebind_rc == 0 && replace_rc == -4 && g_failed == 0;

    printf("FG_IQ_SEAM unbound_aborts=%d bind_rc=%d iq_bound=%d goose_init_runs=%lld "
           "rebind_rc=%d replace_rc=%d checks=%lld failures=%lld %s\n",
           aborts, bind_rc, bound, (long long) runs, rebind_rc, replace_rc,
           (long long) g_checks, (long long) g_failed, ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
