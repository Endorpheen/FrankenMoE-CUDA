// Differential gate for the Goose KV cell gather module.
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
int64_t fg206c_gather_cells(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, int64_t, int64_t);
int64_t fg206c_gather_contig(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, int64_t, int64_t, int64_t);
}

static int checks, failures;
static void ck(bool ok, const char * what, long long a, long long b) {
    ++checks;
    if (!ok) { if (failures < 15) printf("FAIL %s: goose=%lld ref=%lld\n", what, a, b); ++failures; }
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
static int64_t sp_[16], ss_[16*4], sh_[16], idx_[16], dp_[16], ds_[16*4];
static void trap_shift(void) { sh_[idx_[0]] = 2;
    fg206c_gather_cells((uint64_t)(uintptr_t)dp_,(uint64_t)(uintptr_t)ds_,(uint64_t)(uintptr_t)sp_,
                        (uint64_t)(uintptr_t)ss_,(uint64_t)(uintptr_t)sh_,(uint64_t)(uintptr_t)idx_,1,4); }
static void trap_contig_shift(void) { sh_[1] = 2;
    fg206c_gather_contig((uint64_t)(uintptr_t)dp_,(uint64_t)(uintptr_t)ds_,(uint64_t)(uintptr_t)sp_,
                         (uint64_t)(uintptr_t)ss_,(uint64_t)(uintptr_t)sh_,0,3,4); }

int main(void) {
    std::mt19937 rng(206011);
    const int W = 4;

    for (int t = 0; t < 400; ++t) {
        const int sn = 1 + (int)(rng() % 12);
        const int m  = 1 + (int)(rng() % (unsigned)sn);
        std::vector<int64_t> SP(sn), SS((size_t)sn*W), SH(sn, 0), ID(sn);
        for (int i = 0; i < sn; ++i) {
            SP[i] = (rng()%3)==0 ? -1 : (int64_t)(rng()%100);
            for (int k = 0; k < W; ++k) SS[i*W+k] = (int64_t)(rng()%1000);
            ID[i] = i;
        }
        std::shuffle(ID.begin(), ID.end(), rng);
        std::vector<int64_t> DP(m,-1), DS((size_t)m*W,-1);
        // reference gather
        std::vector<int64_t> RP(m), RS((size_t)m*W);
        for (int j = 0; j < m; ++j) {
            RP[j] = SP[ID[j]];
            for (int k = 0; k < W; ++k) RS[j*W+k] = SS[(size_t)ID[j]*W+k];
        }
        const int64_t rc = fg206c_gather_cells((uint64_t)(uintptr_t)DP.data(),(uint64_t)(uintptr_t)DS.data(),
                                               (uint64_t)(uintptr_t)SP.data(),(uint64_t)(uintptr_t)SS.data(),
                                               (uint64_t)(uintptr_t)SH.data(),(uint64_t)(uintptr_t)ID.data(),m,W);
        ck(rc == m, "indexed rc", rc, m);
        for (int j = 0; j < m; ++j) {
            ck(DP[j] == RP[j], "idx pos", DP[j], RP[j]);
            for (int k = 0; k < W; ++k) ck(DS[j*W+k] == RS[j*W+k], "idx seq", DS[j*W+k], RS[j*W+k]);
        }
        // contiguous from a random start
        if (sn >= m) {
            const int i0 = (int)(rng() % (unsigned)(sn - m + 1));
            std::vector<int64_t> DP2(m,-1), DS2((size_t)m*W,-1);
            const int64_t rc2 = fg206c_gather_contig((uint64_t)(uintptr_t)DP2.data(),(uint64_t)(uintptr_t)DS2.data(),
                                                     (uint64_t)(uintptr_t)SP.data(),(uint64_t)(uintptr_t)SS.data(),
                                                     (uint64_t)(uintptr_t)SH.data(),i0,m,W);
            ck(rc2 == m, "contig rc", rc2, m);
            for (int j = 0; j < m; ++j) {
                ck(DP2[j] == SP[i0+j], "contig pos", DP2[j], SP[i0+j]);
                for (int k = 0; k < W; ++k) ck(DS2[j*W+k] == SS[(size_t)(i0+j)*W+k], "contig seq", 0, 0);
            }
        }
    }
    idx_[0] = 0; run_trap("shifted source (indexed)", 602, trap_shift);
    run_trap("shifted source (contiguous)", 611, trap_contig_shift);

    printf("FG206C_FIXTURE checks=%d failures=%d\n", checks, failures);
    if (failures) { printf("EXP206_LOCAL_GATES_FAIL\n"); return 1; }
    printf("EXP206_LOCAL_GATES_PASS\n");
    return 0;
}
