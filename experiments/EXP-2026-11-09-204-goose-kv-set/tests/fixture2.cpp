// Differential gate for the Goose KV cell-copy module, real container shape.
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
int64_t fg204w_set_indexed(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, int64_t, int64_t);
int64_t fg204w_set_contig(uint64_t, uint64_t, uint64_t, int64_t, int64_t, uint64_t, uint64_t, int64_t, int64_t);
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

static int64_t dpos[16], dseq[16*4], dused[16];
static int64_t spos[16], sseq[16*4];
static int64_t idxs[16];

static void trap_neg_index(void) {
    idxs[0] = -1;
    fg204w_set_indexed((uint64_t)(uintptr_t)dpos,(uint64_t)(uintptr_t)dseq,(uint64_t)(uintptr_t)dused,
                       (uint64_t)(uintptr_t)spos,(uint64_t)(uintptr_t)sseq,(uint64_t)(uintptr_t)idxs,1,4);
}
static void trap_contig_overflow(void) {
    fg204w_set_contig((uint64_t)(uintptr_t)dpos,(uint64_t)(uintptr_t)dseq,(uint64_t)(uintptr_t)dused,
                      2,1,(uint64_t)(uintptr_t)spos,(uint64_t)(uintptr_t)sseq,5,4);
}

int main(void) {
    std::mt19937 rng(204022);
    const int W = 4;   // LLAMA_MAX_SEQ=256 => four 64-bit words

    // indexed form
    for (int t = 0; t < 300; ++t) {
        const int dn = 1 + (int)(rng() % 8), sn = 1 + (int)(rng() % 8);
        const int m = 1 + (int)(rng() % (unsigned)std::min(dn, sn));
        std::vector<int64_t> D(dn), DS((size_t)dn*W), DU(dn);
        std::vector<int64_t> S(sn), SS((size_t)sn*W);
        for (int i = 0; i < dn; ++i) { D[i] = (rng()%3)==0 ? -1 : (int64_t)(rng()%100); DU[i] = D[i]==-1?0:1; for(int k=0;k<W;k++) DS[i*W+k]=(int64_t)(rng()%256); }
        for (int i = 0; i < sn; ++i) { S[i] = (rng()%3)==0 ? -1 : (int64_t)(rng()%100); for(int k=0;k<W;k++) SS[i*W+k]=(int64_t)(rng()%256); }
        // arbitrary distinct destination indices
        std::vector<int64_t> I(dn); for (int i=0;i<dn;i++) I[i]=i;
        std::shuffle(I.begin(), I.end(), rng);
        std::vector<int64_t> ID = D, IDS = DS, IDU = DU;

        // reference
        for (int j = 0; j < m; ++j) {
            const int didx = (int)I[j], sidx = j;
            const int64_t dp = ID[didx], sp = S[sidx];
            if (dp == -1 && sp != -1) IDU[didx] = 1;
            if (dp != -1 && sp == -1) IDU[didx] = 0;
            ID[didx] = sp;
            for (int k = 0; k < W; ++k) IDS[didx*W+k] = SS[sidx*W+k];
        }

        const int64_t rc = fg204w_set_indexed((uint64_t)(uintptr_t)D.data(),(uint64_t)(uintptr_t)DS.data(),
                                              (uint64_t)(uintptr_t)DU.data(),(uint64_t)(uintptr_t)S.data(),
                                              (uint64_t)(uintptr_t)SS.data(),(uint64_t)(uintptr_t)I.data(),m,W);
        ck(rc == m, "indexed rc", rc, m);
        for (int i = 0; i < dn; ++i) {
            ck(D[i] == ID[i], "idx pos", D[i], ID[i]);
            ck(DU[i] == IDU[i], "idx used", DU[i], IDU[i]);
            for (int k = 0; k < W; ++k) ck(DS[i*W+k] == IDS[i*W+k], "idx seq", DS[i*W+k], IDS[i*W+k]);
        }
    }

    // contiguous form
    for (int t = 0; t < 200; ++t) {
        const int dn = 1 + (int)(rng() % 8), sn = 1 + (int)(rng() % 8);
        const int m = 1 + (int)(rng() % (unsigned)std::min(dn, sn));
        const int di = (int)(rng() % (unsigned)(dn - m + 1));
        std::vector<int64_t> D(dn), DS((size_t)dn*W), DU(dn);
        std::vector<int64_t> S(sn), SS((size_t)sn*W);
        for (int i = 0; i < dn; ++i) { D[i] = (rng()%3)==0 ? -1 : (int64_t)(rng()%100); DU[i] = D[i]==-1?0:1; for(int k=0;k<W;k++) DS[i*W+k]=(int64_t)(rng()%256); }
        for (int i = 0; i < sn; ++i) { S[i] = (rng()%3)==0 ? -1 : (int64_t)(rng()%100); for(int k=0;k<W;k++) SS[i*W+k]=(int64_t)(rng()%256); }
        std::vector<int64_t> CD = D, CDS = DS, CDU = DU;
        for (int j = 0; j < m; ++j) {
            const int didx = di + j, sidx = j;
            const int64_t dp = CD[didx], sp = S[sidx];
            if (dp == -1 && sp != -1) CDU[didx] = 1;
            if (dp != -1 && sp == -1) CDU[didx] = 0;
            CD[didx] = sp;
            for (int k = 0; k < W; ++k) CDS[didx*W+k] = SS[sidx*W+k];
        }
        const int64_t rc = fg204w_set_contig((uint64_t)(uintptr_t)D.data(),(uint64_t)(uintptr_t)DS.data(),
                                             (uint64_t)(uintptr_t)DU.data(),dn,di,
                                             (uint64_t)(uintptr_t)S.data(),(uint64_t)(uintptr_t)SS.data(),m,W);
        ck(rc == m, "contig rc", rc, m);
        for (int i = 0; i < dn; ++i) {
            ck(D[i] == CD[i], "contig pos", D[i], CD[i]);
            ck(DU[i] == CDU[i], "contig used", DU[i], CDU[i]);
            for (int k = 0; k < W; ++k) ck(DS[i*W+k] == CDS[i*W+k], "contig seq", DS[i*W+k], CDS[i*W+k]);
        }
    }

    run_trap("negative index", 401, trap_neg_index);
    run_trap("contiguous overflow", 411, trap_contig_overflow);

    printf("FG204W_FIXTURE checks=%d failures=%d\n", checks, failures);
    if (failures) { printf("EXP204_LOCAL_GATES_FAIL\n"); return 1; }
    printf("EXP204_LOCAL_GATES_PASS\n");
    return 0;
}
