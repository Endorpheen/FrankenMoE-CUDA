/* CPU sampler for a userspace process when perf and ptrace are unavailable.
 *
 * LD_PRELOAD it and raise SIGUSR1 to dump. ITIMER_PROF fires on consumed CPU
 * time, so the histogram is a processor-time profile, not wall time. Addresses
 * are printed rebased against the first mapping of the main executable, so
 * addr2line on the binary resolves them directly.
 *
 *   GOOSE_SAMPLE_MS=1 LD_PRELOAD=./sampler.so ./frankenmoe-infer ...
 *   kill -USR1 <pid>
 */
#define _GNU_SOURCE
#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <ucontext.h>
#include <unistd.h>

enum { MAX_SAMPLES = 200000, MAX_FRAMES = 20, MAX_UNIQUE = 4096 };
static void *samples[MAX_SAMPLES][MAX_FRAMES];
static unsigned char depth[MAX_SAMPLES];
static long total;
static unsigned long base;

static void on_prof(int sig, siginfo_t *info, void *ctx) {
    (void) sig; (void) info;
    if (total >= MAX_SAMPLES) return;
    ucontext_t *uc = (ucontext_t *) ctx;
    unsigned long rip = 0;
#if defined(__x86_64__)
    rip = (unsigned long) uc->uc_mcontext.gregs[REG_RIP];
#endif
    if (!rip) return;
    samples[total][0] = (void *) rip;
    depth[total] = 1;
    ++total;
}

static void rebase(void) {
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f) return;
    char line[512];
    char self[512];
    ssize_t n = readlink("/proc/self/exe", self, sizeof self - 1);
    if (n <= 0) { fclose(f); return; }
    self[n] = 0;
    while (fgets(line, sizeof line, f)) {
        if (!strstr(line, self)) continue;
        unsigned long lo = strtoul(line, NULL, 16);
        base = lo; /* first mapping of the executable */
        break;
    }
    fclose(f);
}

static void dump(void) {
    if (!total) return;
    fprintf(stderr, "GOOSE-SAMPLE total=%ld base=%#lx\n", total, base);
    /* Print a coarse histogram of the innermost three frames. */
    for (long i = 0; i < total; ++i) {
        fprintf(stderr, "S");
        for (int k = 0; k < depth[i] && k < 4; ++k)
            fprintf(stderr, " %#lx", (unsigned long) samples[i][k]);
        fprintf(stderr, "\n");
    }
    total = 0;
}

static void on_usr1(int sig) { (void) sig; dump(); }

__attribute__((constructor)) static void start(void) {
    rebase();
    const char *ms = getenv("GOOSE_SAMPLE_MS");
    const long interval = ms ? atol(ms) : 1;
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_prof;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction(SIGPROF, &sa, NULL);
    struct sigaction su;
    memset(&su, 0, sizeof su);
    su.sa_handler = on_usr1;
    su.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &su, NULL);
    struct itimerval it;
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_usec = interval * 1000;
    it.it_value = it.it_interval;
    setitimer(ITIMER_PROF, &it, NULL);
    atexit(dump);
}
