// Differential gate for the Goose tokenizer module.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <random>
#include <algorithm>

extern "C" {
int64_t fg202t_utf8_len(int64_t);
int64_t fg202t_bigram_valid(int64_t, int64_t, int64_t);
int64_t fg202t_try_add_allowed(int64_t, int64_t, int64_t, int64_t, int64_t);
int64_t fg202t_resegment_kind(int64_t, int64_t);
int64_t fg202t_clamp_symbol_len(int64_t, int64_t);
}

static int checks, failures;
static void ck(bool ok, const char * what, long long a, long long b) {
    ++checks;
    if (!ok) { if (failures < 15) printf("FAIL %s: goose=%lld reference=%lld\n", what, a, b); ++failures; }
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
    if (!died || code != expected) { if (failures < 15) printf("FAIL trap %s: died=%d code=%lld expected=%d\n", what, died, code, expected); ++failures; }
}
static void trap_utf8_range(void) { fg202t_utf8_len(256); }

// Native reference for utf8_len: the table from unicode.cpp.
static size_t ref_utf8(char src) {
    static const size_t lookup[] = {1,1,1,1,1,1,1,1,1,1,1,1,2,2,3,4};
    return lookup[(uint8_t)src >> 4];
}

int main(void) {
    std::mt19937 rng(202001);

    // utf8_len over all 256 byte values.
    for (int b = 0; b < 256; ++b) {
        ck(fg202t_utf8_len(b) == (int64_t)ref_utf8((char)b), "utf8_len", fg202t_utf8_len(b), ref_utf8((char)b));
    }

    // bigram_valid against the native skip condition.
    for (int i = 0; i < 300; ++i) {
        const int64_t ln = (int64_t)(rng() % 6), rn = (int64_t)(rng() % 6), sz = (int64_t)(rng() % 12);
        const bool ref = !(ln == 0 || rn == 0 || ln + rn != sz);
        ck((fg202t_bigram_valid(ln, rn, sz) != 0) == ref, "bigram_valid", 0, ref);
    }

    // try_add_allowed.
    for (int i = 0; i < 300; ++i) {
        const int64_t l = (int64_t)(rng() % 5) - 1, r = (int64_t)(rng() % 5) - 1;
        const int64_t tok = (int64_t)(rng() % 10) - 1, nt = (int64_t)(rng() % 10);
        const int64_t null_tok = -1;
        const bool ref = !(l == -1 || r == -1 || tok == null_tok || tok >= nt);
        ck((fg202t_try_add_allowed(l, r, tok, null_tok, nt) != 0) == ref, "try_add", 0, ref);
    }

    // resegment_kind over all four combinations.
    for (int a = 0; a < 2; ++a) for (int b = 0; b < 2; ++b) {
        const int64_t ref = a ? 0 : (b ? 1 : 2);
        ck(fg202t_resegment_kind(a, b) == ref, "resegment", fg202t_resegment_kind(a, b), ref);
    }

    // clamp_symbol_len.
    for (int i = 0; i < 300; ++i) {
        const int64_t len = 1 + (int64_t)(rng() % 4), rem = 1 + (int64_t)(rng() % 8);
        ck(fg202t_clamp_symbol_len(len, rem) == std::min(len, rem), "clamp", 0, 0);
    }

    run_trap("utf8_len out of range", 10, trap_utf8_range);

    printf("FG202T_FIXTURE checks=%d failures=%d\n", checks, failures);
    if (failures) { printf("EXP202_LOCAL_GATES_FAIL\n"); return 1; }
    printf("EXP202_LOCAL_GATES_PASS\n");
    return 0;
}
