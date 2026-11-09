// Differential gate for the Goose KV cell-copy module.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <random>
#include <vector>
#include <algorithm>

extern "C" {
int64_t fg204s_set_cells(uint64_t, int64_t, int64_t, uint64_t, int64_t, int64_t, int64_t);
}

static int checks, failures;
static void ck(bool ok, const char * what, long long a, long long b) {
    ++checks;
    if (!ok) { if (failures < 15) printf("FAIL %s: goose=%lld ref=%lld\n", what, a, b); ++failures; }
}

// Arena: pos[n] | seq[n*W] | used[n] | shift[n], W=8, int64 slots.
static const int W = 8;
struct Arena {
    std::vector<int64_t> v;
    int n;
    explicit Arena(int n_) : v((size_t)(n_ + n_*W + n_ + n_) * 8 / 8, 0), n(n_) {}
    int64_t  *pos()   { return v.data(); }
    int64_t  *seq()   { return v.data() + n; }
    int64_t  *used()  { return v.data() + n + (size_t)n*W; }
    int64_t  *shift() { return v.data() + n + (size_t)n*W + (size_t)n; }
};

// native transcription of llama-kv-cells.h:166-192, used-flag and pos/seq part only
static void ref_set(Arena & dst, int di, Arena & src, int sj, int m) {
    for (int j = 0; j < m; ++j) {
        const int didx = di + j, sidx = sj + j;
        const int64_t dpos = dst.pos()[didx], spos = src.pos()[sidx];
        if (dpos == -1 && spos != -1) dst.used()[didx] = 1;
        if (dpos != -1 && spos == -1) dst.used()[didx] = 0;
        dst.pos()[didx] = spos;
        for (int w = 0; w < W; ++w) dst.seq()[didx*W + w] = src.seq()[sidx*W + w];
    }
}

static void run_trap(const char * what, int expected, void (*fn)(void)) {
    int fd[2]; if (pipe(fd) != 0) { ++failures; return; }
    pid_t pid = fork();
    if (pid == 0) { close(fd[0]); dup2(fd[1], 2); close(fd[1]); fn(); _exit(0); }
    close(fd[1]);
    char buf[256] = {0}; ssize_t n = read(fd[0], buf, sizeof buf - 1); close(fd[0]);
    int st = 0; waitpid(pid, &st, 0); ++checks;
    bool died = WIFSIGNALED(st) && WTERMSIG(st) == SIGABRT;
    long long code = (n > 0 && strstr(buf, "validation code")) ? atoll(strstr(buf, "validation code") + 16) : -1;
    if (!died || code != expected) { if (failures < 15) printf("FAIL trap %s: died=%d code=%lld exp=%d\n", what, died, code, expected); ++failures; }
}
static void trap_shift(void) {
    Arena s(2); s.shift()[0] = 5;
    Arena d(2);
    fg204s_set_cells((uint64_t)(uintptr_t)d.pos(), 2, 0, (uint64_t)(uintptr_t)s.pos(), 2, 0, 1);
}
static void trap_overflow(void) {
    Arena s(2); Arena d(2);
    fg204s_set_cells((uint64_t)(uintptr_t)d.pos(), 2, 1, (uint64_t)(uintptr_t)s.pos(), 2, 0, 5);
}

int main(void) {
    std::mt19937 rng(204011);

    for (int t = 0; t < 300; ++t) {
        const int dn = 1 + (int)(rng() % 6), sn = 1 + (int)(rng() % 6);
        const int m = 1 + (int)(rng() % (unsigned)std::min(dn, sn));
        const int di = (int)(rng() % (unsigned)(dn - m + 1));
        const int sj = (int)(rng() % (unsigned)(sn - m + 1));

        Arena d(dn), s(sn), dr(dn), sr(sn);
        // fill both with random state; shift stays 0 because native asserts it
        for (int i = 0; i < dn; ++i) {
            const bool e = (rng() % 3) == 0;
            d.pos()[i] = e ? -1 : (int64_t)(rng() % 100);
            d.used()[i] = e ? 0 : 1;
            for (int w = 0; w < W; ++w) d.seq()[i*W+w] = (int64_t)(rng() % 256);
        }
        for (int i = 0; i < sn; ++i) {
            const bool e = (rng() % 3) == 0;
            s.pos()[i] = e ? -1 : (int64_t)(rng() % 100);
            s.used()[i] = e ? 0 : 1;
            for (int w = 0; w < W; ++w) s.seq()[i*W+w] = (int64_t)(rng() % 256);
        }
        dr.v = d.v; sr.v = s.v;

        const int64_t rc = fg204s_set_cells((uint64_t)(uintptr_t)d.pos(), dn, di,
                                            (uint64_t)(uintptr_t)s.pos(), sn, sj, m);
        ref_set(dr, di, sr, sj, m);

        ck(rc == m, "return m", rc, m);
        for (int i = 0; i < dn; ++i) {
            ck(d.pos()[i] == dr.pos()[i], "pos", d.pos()[i], dr.pos()[i]);
            ck(d.used()[i] == dr.used()[i], "used", d.used()[i], dr.used()[i]);
            for (int w = 0; w < W; ++w)
                ck(d.seq()[i*W+w] == dr.seq()[i*W+w], "seqword", d.seq()[i*W+w], dr.seq()[i*W+w]);
        }
    }

    run_trap("shifted source cell", 304, trap_shift);
    run_trap("range overflow", 302, trap_overflow);

    printf("FG204S_FIXTURE checks=%d failures=%d\n", checks, failures);
    if (failures) { printf("EXP204_LOCAL_GATES_FAIL\n"); return 1; }
    printf("EXP204_LOCAL_GATES_PASS\n");
    return 0;
}
