// Changed-boundary fixture for CPU_REPACK control policy and scheduling.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int64_t fg006_rc_entry(int64_t, int64_t, int64_t, int64_t, int64_t,
                              int64_t, int64_t, int64_t, int64_t);
extern int fg006_rc_enabled(void);
extern void fg006_rc_report(void);

static int failures;
#define CHECK(cond, name) do { if (!(cond)) { fprintf(stderr, "FAIL %s\n", name); ++failures; } } while (0)

struct chunk_rec { int64_t s0s, s0e, s1s, s1e; };
static struct chunk_rec chunks[64];
static int nchunks;
static int64_t spy_next;
static int quant4_calls, quant1_calls, barriers, mmid_calls;
static int64_t row_count[2], row_i1v[2][8], row_i2v[2][8];
static int64_t mmid_log[2][8][6];
static int mmid_log_n[2];
static const int32_t ids[2][2] = {{0, 1}, {1, 0}};

int64_t fg006_rc_chunk(int64_t a, int64_t b, int64_t c, int64_t d) {
    if (nchunks < 64) chunks[nchunks++] = (struct chunk_rec){a, b, c, d};
    return 0;
}
void fg006_rc_chunk_set(int64_t v) { spy_next = v; }
int64_t fg006_rc_chunk_add(int64_t d) { spy_next += d; return spy_next; }
void fg006_rc_barrier(void) { ++barriers; }
void fg006_rc_quant4(int64_t i12, int64_t i11, int64_t ne10) {
    (void) i12; (void) i11; (void) ne10; ++quant4_calls;
}
void fg006_rc_quant1(int64_t i12, int64_t i11, int64_t ne10) {
    (void) i12; (void) i11; (void) ne10; ++quant1_calls;
}
void fg006_rc_rows_clear(int64_t n_as) {
    (void) n_as; memset(row_count, 0, sizeof(row_count));
    memset(row_i1v, 0, sizeof(row_i1v)); memset(row_i2v, 0, sizeof(row_i2v));
    memset(mmid_log_n, 0, sizeof(mmid_log_n));
}
int32_t fg006_rc_ids_get(int64_t iid1, int64_t id) {
    if (iid1 < 0 || iid1 >= 2 || id < 0 || id >= 2) return -1;
    return ids[iid1][id];
}
int64_t fg006_rc_row_count(int64_t expert) { return row_count[expert]; }
void fg006_rc_row_set(int64_t expert, int64_t index, int64_t i1, int64_t i2) {
    if (index >= 0 && index < 8) { row_i1v[expert][index] = i1; row_i2v[expert][index] = i2; }
}
void fg006_rc_row_inc(int64_t expert) { ++row_count[expert]; }
int64_t fg006_rc_row_i1(int64_t expert, int64_t index) { return row_i1v[expert][index]; }
int64_t fg006_rc_row_i2(int64_t expert, int64_t index) { return row_i2v[expert][index]; }
void fg006_rc_mmid_gemv(int64_t expert, int64_t i1, int64_t i2, int64_t i11,
                        int64_t s0s, int64_t s0e) {
    if (mmid_log_n[expert] < 8) {
        int64_t * p = mmid_log[expert][mmid_log_n[expert]++];
        p[0] = expert; p[1] = i1; p[2] = i2; p[3] = i11; p[4] = s0s; p[5] = s0e;
    }
    ++mmid_calls;
}

static void ref_plan(int64_t nr0, int64_t nth, int64_t ne12, int64_t numa,
                     int64_t * nchunk0, int64_t * nchunk1, int64_t * dr0) {
    int64_t nth_scaled = nth * 4;
    int64_t chunk_size0 = (nr0 + nth_scaled - 1) / nth_scaled;
    int64_t nc = (nr0 + chunk_size0 - 1) / chunk_size0;
    const int64_t min_chunk = 8;
    if (nc > 0 && (nr0 / nc) < min_chunk && nr0 >= min_chunk) nc = (nr0 + min_chunk - 1) / min_chunk;
    int64_t dr = (nr0 + nc - 1) / nc;
    if (nth == 1 || ((nc < nth || numa) && (nr0 + nth - 1) / nth >= min_chunk)) {
        nc = nth; dr = (nr0 + nc - 1) / nc;
    }
    const int64_t max_nchunk = (nr0 + min_chunk - 1) / min_chunk;
    if (nc > max_nchunk) nc = max_nchunk;
    *nchunk0 = nc; *nchunk1 = ne12; *dr0 = dr;
}

static void test_plan(int64_t nr0, int64_t nth, int64_t ne12, int64_t numa) {
    int64_t a = fg006_rc_entry(7, nr0, nth, ne12, numa, 0, 0, 0, 0);
    int64_t b = fg006_rc_entry(7, nr0, nth, ne12, numa, 1, 0, 0, 0);
    int64_t c = fg006_rc_entry(7, nr0, nth, ne12, numa, 2, 0, 0, 0);
    int64_t ra, rb, rc;
    ref_plan(nr0, nth, ne12, numa, &ra, &rb, &rc);
    CHECK(a == ra && b == rb && c == rc, "plan");
}

int main(int argc, char ** argv) {
    int rollback = argc > 1 && strcmp(argv[1], "rollback") == 0;
    CHECK(fg006_rc_enabled() == (rollback ? 0 : 1), "gate");

    CHECK(fg006_rc_entry(0, 8, 1, 0, 0, 0, 0, 0, 0) == 1, "select avx2");
    CHECK(fg006_rc_entry(0, 4, 1, 0, 0, 0, 0, 0, 0) == 0, "select ne1 guard");
    CHECK(fg006_rc_entry(0, 8, 0, 1, 1, 0, 0, 0, 0) == 1, "select neon mm8");
    CHECK(fg006_rc_entry(0, 8, 0, 1, 0, 1, 0, 0, 0) == 2, "select neon dotprod");
    CHECK(fg006_rc_entry(0, 8, 0, 0, 0, 0, 0, 0, 0) == 0, "select none");

    CHECK(fg006_rc_entry(1, 0, 512, 4, 1, 1, 0, 0, 0) == 2336, "work mat");
    CHECK(fg006_rc_entry(1, 1, 512, 4, 1, 2, 0, 0, 0) == 2368, "work id");

    CHECK(fg006_rc_entry(2, 3, 0, 0, 0, 1, 0, 0, 0) == 4, "provider repack");
    CHECK(fg006_rc_entry(2, 0, 1, 0, 0, 0, 0, 0, 0) == 1, "provider amx");
    CHECK(fg006_rc_entry(2, 1, 0, 0, 0, 0, 0, 0, 0) == 0, "provider off");

    CHECK(fg006_rc_entry(3, 1, 0, 2, 1, 1, 0, 1, 0) == 1, "supports mat");
    CHECK(fg006_rc_entry(3, 0, 1, 3, 1, 1, 0, 1, 0) == 1, "supports id");
    CHECK(fg006_rc_entry(3, 1, 0, 3, 1, 1, 0, 1, 0) == 0, "supports mat dims");
    CHECK(fg006_rc_entry(3, 1, 0, 2, 1, 1, 1, 1, 0) == 0, "supports nonhost");
    CHECK(fg006_rc_entry(4, 1, 0, 1, 1, 0, 0, 0, 0) == 1, "get traits");
    CHECK(fg006_rc_entry(4, 0, 0, 1, 1, 0, 0, 0, 0) == 0, "get traits op");

    test_plan(64, 4, 2, 0);
    test_plan(1, 1, 1, 0);
    test_plan(257, 8, 3, 1);

    for (int64_t ch = 0; ch < 16; ++ch) {
        int64_t ok = fg006_rc_entry(8, 8, 2, 8, ch, 64, 4, 0, 0);
        CHECK(ok == 1, "bounds ok");
        if (ok) {
            int64_t s0 = fg006_rc_entry(8, 8, 2, 8, ch, 64, 4, 1, 0);
            int64_t e0 = fg006_rc_entry(8, 8, 2, 8, ch, 64, 4, 2, 0);
            int64_t s1 = fg006_rc_entry(8, 8, 2, 8, ch, 64, 4, 3, 0);
            int64_t e1 = fg006_rc_entry(8, 8, 2, 8, ch, 64, 4, 4, 0);
            CHECK(s0 % 8 == 0 && e0 % 8 == 0 && s0 < e0 && e0 <= 64, "bounds align");
            CHECK(s1 % 4 == 0 && e1 == s1 + 4, "bounds plane");
        }
    }

    memset(chunks, 0, sizeof(chunks)); nchunks = 0; spy_next = 0; barriers = 0;
    int64_t fwd = fg006_rc_entry(5, 0, 4, 64, 64, 4, 2, 0, 0);
    CHECK(fwd > 0 && fwd <= 16, "forward chunks");
    CHECK(barriers == 1, "forward barrier");
    for (int i = 0; i < nchunks; ++i) {
        CHECK(chunks[i].s0s % 8 == 0 && chunks[i].s0e % 8 == 0 && chunks[i].s0s < chunks[i].s0e, "forward align");
        CHECK(chunks[i].s1e == chunks[i].s1s + 4, "forward plane");
    }

    CHECK(fg006_rc_entry(9, 0, 1, 7, 2, 512, 0, 0, 0) == 0, "quantize");
    CHECK(quant4_calls == 2 * 1 && quant1_calls == 2 * 3, "quantize split");

    memset(row_count, 0, sizeof(row_count)); memset(mmid_log_n, 0, sizeof(mmid_log_n));
    mmid_calls = 0; barriers = 0;
    int64_t rows = fg006_rc_entry(6, 0, 1, 16, 4, 2, 2, 2, 2);
    CHECK(rows == 4 && mmid_calls == 4, "mmid calls");
    CHECK(row_count[0] == 2 && row_count[1] == 2, "mmid groups");
    CHECK(mmid_log[0][0][1] == 0 && mmid_log[0][0][2] == 0 && mmid_log[0][0][3] == 0, "mmid e0r0");
    CHECK(mmid_log[0][1][1] == 1 && mmid_log[0][1][2] == 1 && mmid_log[0][1][3] == 1, "mmid e0r1");
    CHECK(mmid_log[1][0][1] == 1 && mmid_log[1][0][2] == 0 && mmid_log[1][0][3] == 1, "mmid e1r0");
    CHECK(mmid_log[1][1][1] == 0 && mmid_log[1][1][2] == 1 && mmid_log[1][1][3] == 0, "mmid e1r1");
    CHECK(barriers == 1, "mmid barrier");

    if (failures) { fprintf(stderr, "FG006_RC FAIL failures=%d\n", failures); return 1; }
    fprintf(stderr, "FG006_RC PASS select=1 work=2336/2368 chunks=%d mmid=4 gate=%d\n",
            nchunks, fg006_rc_enabled());
    return 0;
}
