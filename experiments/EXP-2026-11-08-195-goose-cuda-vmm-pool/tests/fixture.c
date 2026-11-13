// Differential gate for the Goose VMM pool arithmetic.
//
// The reference is a line-by-line copy of ggml_cuda_pool_vmm::alloc/free from ggml-cuda.cu,
// with the driver calls removed, so the comparison is against the native algorithm and not
// against a re-derivation of it.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_SIZE (1ull << 35)

// ---- native reference, transcribed from ggml-cuda.cu -------------------------------
struct nat { unsigned long long pool_addr_off, pool_used, pool_size; };

static void nat_alloc(struct nat *n, size_t size, size_t granularity,
                      size_t *actual, unsigned long long *reserve_out, int *grew) {
    const size_t alignment = 128;
    size = alignment * ((size + alignment - 1) / alignment);
    size_t avail = n->pool_size - n->pool_used;
    *reserve_out = 0; *grew = 0;
    if (size > avail) {
        size_t reserve_size = size - avail;
        reserve_size = granularity * ((reserve_size + granularity - 1) / granularity);
        if (n->pool_size + reserve_size > MAX_SIZE) { fprintf(stderr, "native overflow\n"); exit(3); }
        n->pool_size += reserve_size;
        *reserve_out = reserve_size; *grew = 1;
    }
    *actual = size;
    n->pool_addr_off = n->pool_used;
    n->pool_used += size;
}

static void nat_free(struct nat *n, unsigned long long ptr_off, size_t size) {
    n->pool_used -= size;
    if (ptr_off != n->pool_used) { fprintf(stderr, "native free order violated\n"); exit(4); }
}

// ---- Goose entry points ------------------------------------------------------------
void fg195v_plan_alloc(int64_t size, int64_t pool_used, int64_t pool_size,
                       int64_t granularity, uint64_t out);
int64_t fg195v_plan_free(int64_t ptr_off, int64_t pool_used, int64_t size);

static int checks, failures;
static void ck(int ok, const char *what) { ++checks; if (!ok) { ++failures; fprintf(stderr, "FAIL %s\n", what); } }

static unsigned rs = 195001;
static unsigned nx(void) { rs = rs * 1664525u + 1013904223u; return rs; }


// ---- trap coverage ------------------------------------------------------------------
// The pool must refuse to continue on a violated invariant, exactly as the native
// GGML_ASSERT does. Each case runs in a child so the abort is observed rather than
// survived; the child writes the trap code to a pipe and the parent checks both the
// signal and the code.
static void run_trap(const char * what, int expected, void (*fn)(void)) {
    int fd[2];
    if (pipe(fd) != 0) { printf("FAIL pipe %s\n", what); failures++; return; }
    pid_t pid = fork();
    if (pid == 0) {
        close(fd[0]);
        dup2(fd[1], 2);
        close(fd[1]);
        fn();
        // Reaching here means the trap did not fire.
        _exit(0);
    }
    close(fd[1]);
    char buf[256] = {0};
    ssize_t n = read(fd[0], buf, sizeof(buf) - 1);
    close(fd[0]);
    int status = 0;
    waitpid(pid, &status, 0);

    ++checks;
    int died = WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
    int code_ok = n > 0 && strstr(buf, "validation code") != NULL &&
                  atoi(strstr(buf, "validation code") + 16) == expected;
    if (!died || !code_ok) {
        printf("FAIL trap %s: died=%d code=%d expected=%d (%s)\n", what, died,
               n > 0 ? atoi(strstr(buf, "validation code") + 16) : -1, expected, buf);
        ++failures;
    }
}

static void trap_zero_size(void)  { int64_t r[8] = {0}; fg195v_plan_alloc(0, 0, 0, 2097152, (uint64_t)(uintptr_t)r); }
static void trap_used_gt_size(void) { int64_t r[8] = {0}; fg195v_plan_alloc(128, 4096, 1024, 2097152, (uint64_t)(uintptr_t)r); }
static void trap_zero_gran(void)  { int64_t r[8] = {0}; fg195v_plan_alloc(128, 0, 0, 0, (uint64_t)(uintptr_t)r); }
// One byte short of the 32 GB ceiling: the growth cannot be legal.
// The pool is full to within 100 bytes of the 32 GB ceiling: the 2 MB growth needed for
// this request cannot be legal.
static void trap_ceiling(void) { int64_t r[8] = {0}; fg195v_plan_alloc(2097152, (int64_t)(MAX_SIZE - 100), (int64_t)MAX_SIZE, 2097152, (uint64_t)(uintptr_t)r); }
static void trap_free_too_big(void) { fg195v_plan_free(0, 128, 256); }
static void trap_free_order(void)   { fg195v_plan_free(0, 256, 128); }

static void traps(void) {
    run_trap("alloc size 0",             10, trap_zero_size);
    run_trap("alloc pool_used > size",   10, trap_used_gt_size);
    run_trap("alloc granularity 0",      10, trap_zero_gran);
    run_trap("alloc over ceiling",       12, trap_ceiling);
    run_trap("free size > pool_used",    20, trap_free_too_big);
    run_trap("free out of order",        21, trap_free_order);
}

int main(void) {
    const size_t grans[] = { 2u << 20, 16u << 20 };
    for (unsigned g = 0; g < sizeof(grans)/sizeof(grans[0]); ++g) {
        const size_t gran = grans[g];
        struct nat n = {0, 0, 0};
        for (int iter = 0; iter < 400; ++iter) {
            // mixture of small, large and exactly-borderline requests
            size_t size;
            switch (nx() % 5) {
                case 0: size = 1; break;
                case 1: size = 128; break;
                case 2: size = 1 + nx() % 4096; break;
                case 3: size = 1 + nx() % (2u << 20); break;
                default: size = 4096 + nx() % (1u << 20); break;
            }
            size_t actual_n = 0; unsigned long long reserve_n = 0; int grew_n = 0;
            nat_alloc(&n, size, gran, &actual_n, &reserve_n, &grew_n);

            uint64_t out[8] = {0};
            fg195v_plan_alloc((int64_t)size, (int64_t)0, (int64_t)0, (int64_t)gran, (uint64_t)(uintptr_t)out);
            // rerun with the real pool state, since the first call above only checked the guards
            struct nat snapshot = n;
            snapshot.pool_used = n.pool_used - actual_n;   // state before this alloc
            snapshot.pool_size = n.pool_size - reserve_n;
            uint64_t o2[8] = {0};
            fg195v_plan_alloc((int64_t)size, (int64_t)snapshot.pool_used, (int64_t)snapshot.pool_size,
                              (int64_t)gran, (uint64_t)(uintptr_t)o2);
            ck((unsigned long long)o2[0] == reserve_n, "reserve matches native");
            ck(o2[1] == snapshot.pool_used, "pointer offset matches native");
            ck((unsigned long long)o2[2] == n.pool_size, "pool_size matches native");
            ck((unsigned long long)o2[3] == n.pool_used, "pool_used matches native");
            ck((unsigned long long)o2[4] == actual_n, "actual_size matches native");
            ck((unsigned long long)o2[5] == snapshot.pool_size, "map offset is the pre-growth pool_size");

            // free immediately, exercising the reverse-order check
            int64_t got = fg195v_plan_free((int64_t)snapshot.pool_used, (int64_t)n.pool_used, (int64_t)actual_n);
            nat_free(&n, snapshot.pool_used, actual_n);
            ck((unsigned long long)got == n.pool_used, "free rewinds like native");
        }
    }
    traps();
    printf("FG195V_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
