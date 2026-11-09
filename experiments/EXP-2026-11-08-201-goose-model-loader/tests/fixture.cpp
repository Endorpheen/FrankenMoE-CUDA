// Differential gate for the Goose model-loader decision module.
//
// The reference is the two branches of check_tensor_dims written out the way
// llama-model-loader.cpp writes them, so the comparison is against the code being replaced.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <random>
#include <vector>

extern "C" {
int64_t fg201l_nelements(uint64_t, int64_t);
int64_t fg201l_dims_ok(uint64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
int64_t fg201l_shape_ok(int64_t, uint64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
int fg201l_shape_check(int, const int64_t *, int64_t, int64_t, const int64_t *);
}

static int checks, failures;
static void ck(bool ok, const char * what, long long a, long long b) {
    ++checks;
    if (!ok) {
        if (failures < 15) printf("FAIL %s: goose=%lld reference=%lld\n", what, a, b);
        ++failures;
    }
}

static void run_trap(const char * what, int expected, void (*fn)(void)) {
    int fd[2];
    if (pipe(fd) != 0) { ++failures; return; }
    pid_t pid = fork();
    if (pid == 0) { close(fd[0]); dup2(fd[1], 2); close(fd[1]); fn(); _exit(0); }
    close(fd[1]);
    char buf[256] = {0};
    ssize_t n = read(fd[0], buf, sizeof buf - 1);
    close(fd[0]);
    int st = 0;
    waitpid(pid, &st, 0);
    ++checks;
    bool died = WIFSIGNALED(st) && WTERMSIG(st) == SIGABRT;
    long long code = (n > 0 && strstr(buf, "validation code")) ? atoll(strstr(buf, "validation code") + 16) : -1;
    if (!died || code != expected) {
        if (failures < 15) printf("FAIL trap %s: died=%d code=%lld expected=%d (%s)\n", what, died, code, expected, buf);
        ++failures;
    }
}

static void trap_nreq_oor(void) { fg201l_nelements(0, 5); }

// Native reference, transcribed.
static bool ref_dims(const int64_t d[4], const std::vector<int64_t> & ne) {
    for (int i = 0; i < 4; ++i) {
        if ((i < (int)ne.size() && ne[i] != d[i]) || (i >= (int)ne.size() && d[i] != 1)) return false;
    }
    return true;
}
static int64_t ref_nelements(const int64_t d[4]) { return d[0]*d[1]*d[2]*d[3]; }

int main(void) {
    std::mt19937 rng(201001);

    // nelements over random dims and request lengths.
    for (int t = 0; t < 200; ++t) {
        int64_t d[4];
        for (int i = 0; i < 4; ++i) d[i] = 1 + (int64_t)(rng() % 8);
        ck(fg201l_nelements((uint64_t)(uintptr_t)d, 4) == ref_nelements(d), "nelements", 0, 0);
        const int64_t n = (int64_t)(rng() % 4);
        int64_t want = 1;
        for (int i = 0; i < n; ++i) want *= d[i];
        ck(fg201l_nelements((uint64_t)(uintptr_t)d, n) == want, "nelements n", 0, 0);
    }

    // dims_ok against the native expression, over every request length 0..4.
    for (int t = 0; t < 400; ++t) {
        int64_t d[4];
        for (int i = 0; i < 4; ++i) d[i] = (int64_t)(rng() % 4);   // includes 0 and 1
        std::vector<int64_t> ne;
        const int n = (int)(rng() % 5);
        for (int i = 0; i < n; ++i) ne.push_back((int64_t)(rng() % 4));
        const bool ref = ref_dims(d, ne);
        const int64_t g = fg201l_dims_ok((uint64_t)(uintptr_t)d, (int64_t)ne.size(),
                                          ne.size() > 0 ? ne[0] : 0, ne.size() > 1 ? ne[1] : 0,
                                          ne.size() > 2 ? ne[2] : 0, ne.size() > 3 ? ne[3] : 0);
        ck((g != 0) == ref, "dims_ok", g, ref);
    }

    // shape_ok: both branches.
    for (int t = 0; t < 400; ++t) {
        int64_t d[4];
        for (int i = 0; i < 4; ++i) d[i] = 1 + (int64_t)(rng() % 6);
        const bool allow = (rng() % 2) != 0;
        const int64_t total = ref_nelements(d);
        const int64_t total_alt = total + ((rng() % 3) - 1);   // sometimes off by one
        const int64_t use = allow ? total_alt : total;
        const bool ref = allow ? (ref_nelements(d) == use) : true; // dims match by construction
        const int64_t g = fg201l_shape_ok(allow ? 1 : 0, (uint64_t)(uintptr_t)d, 4, use,
                                          d[0], d[1], d[2], d[3]);
        ck((g != 0) == ref, "shape_ok", g, ref);
    }

    // The integration entry point mirrors the same decision.
    {
        int64_t d[4] = {4, 3, 2, 1};
        int64_t req[4] = {4, 3, 2, 1};
        ck(fg201l_shape_check(0, d, 4, 24, req) == 1, "entry match", 0, 1);
        int64_t req2[4] = {4, 3, 2, 2};
        ck(fg201l_shape_check(0, d, 4, 24, req2) == 0, "entry mismatch", 0, 0);
        ck(fg201l_shape_check(1, d, 4, 24, req) == 1, "entry reshape", 0, 1);
        ck(fg201l_shape_check(1, d, 4, 25, req) == 0, "entry reshape count", 0, 0);
    }

    run_trap("nelements n out of range", 10, trap_nreq_oor);

    printf("FG201L_FIXTURE checks=%d failures=%d\n", checks, failures);
    if (failures) { printf("EXP201_LOCAL_GATES_FAIL\n"); return 1; }
    printf("EXP201_LOCAL_GATES_PASS\n");
    return 0;
}
