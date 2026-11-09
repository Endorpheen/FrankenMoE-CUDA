// Differential gate for the Goose KV cell-ownership module.
//
// The reference is llama_kv_cells from llama-kv-cells.h, transcribed with the same containers, so
// the comparison is against the class being replaced rather than a re-derivation.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <vector>
#include <bitset>
#include <set>
#include <random>
#include <utility>

static const int W = 8;
static const int MAXSEQ = 512;

extern "C" {
int fg199k_is_empty(int64_t, int64_t);
void fg199k_rm(uint64_t, int64_t, int64_t);
int64_t fg199k_seq_rm(uint64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
int64_t fg199k_seq_keep(uint64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
void fg199k_seq_add(uint64_t, int64_t, int64_t, int64_t, int64_t);
void fg199k_pos_set(uint64_t, int64_t, int64_t, int64_t);
int64_t fg199k_seq_count(uint64_t, int64_t, int64_t);
int64_t fg199k_used_max_p1(uint64_t, int64_t);
int64_t fg199k_place_cell(uint64_t, int64_t, int64_t, int64_t, int64_t, uint64_t);
int64_t fg199k_head_after_place(int64_t);
int64_t fg199k_head_start(int64_t, int64_t, int64_t);
int64_t fg199k_can_use(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
}

// ---- arena, laid out as the module documents it ----
struct Arena {
    std::vector<int64_t> v;
    int64_t n;
    explicit Arena(int64_t cells) : n(cells) {
        // pos[n] | seq[n*W] | used[n] | shift[n], matching the module's documented layout
        v.assign((size_t)(cells + cells*W + cells + cells), 0);
        for (int64_t i = 0; i < cells; ++i) pos()[i] = -1;   // empty cells start at -1
    }
    uint64_t base() { return (uint64_t)(uintptr_t)v.data(); }
    int64_t * pos()  { return v.data(); }
    int64_t * seq()  { return v.data() + n; }
    int64_t * used() { return v.data() + n + n*W; }
    int64_t * shift(){ return v.data() + n + n*W + n; }
};

// ---- native reference: llama_kv_cells ----
struct Ref {
    int64_t n;
    std::vector<int64_t> pos;
    std::vector<std::bitset<MAXSEQ>> seq;
    std::vector<int64_t> shift;
    std::set<uint32_t> used;

    explicit Ref(int64_t c) : n(c), pos(c, -1), seq(c), shift(c, 0) {}

    bool is_empty(uint32_t i) const { return pos[i] == -1; }

    void rm(uint32_t i) {
        seq[i].reset();
        pos[i] = -1;
        shift[i] = 0;
        used.erase(i);
    }

    bool seq_rm(uint32_t i, int seq_id) {
        seq[i].reset(seq_id);
        if (seq[i].none()) {
            pos[i] = -1;
            shift[i] = 0;
            used.erase(i);
            return true;
        }
        return false;
    }

    bool seq_keep(uint32_t i, int seq_id) {
        if (seq[i].test(seq_id)) {
            seq[i].reset();
            seq[i].set(seq_id);
            return false;
        }
        if (seq[i].any()) {
            seq[i].reset();
            pos[i] = -1;
            shift[i] = 0;
            used.erase(i);
            return true;
        }
        return false;
    }

    void seq_add(uint32_t i, int seq_id) { seq[i].set(seq_id); }

    void pos_set(uint32_t i, int64_t p) {
        pos[i] = p;
        if (p != -1) used.insert(i);
    }

    int seq_count(uint32_t i) const { return (int)seq[i].count(); }

    int64_t used_max_p1() const { return used.empty() ? 0 : (int64_t)*used.rbegin() + 1; }
};

static int checks, failures;
static void ck(bool ok, const char * what, long long a, long long b) {
    ++checks;
    if (!ok) {
        if (failures < 15) printf("FAIL %s: goose=%lld reference=%lld\n", what, a, b);
        ++failures;
    }
}

// Mirror one arena into the reference and compare every observable after each operation.
static void compare(Arena & A, Ref & R, const char * tag) {
    for (int64_t i = 0; i < A.n; ++i) {
        ck(A.pos()[i] == R.pos[i], "pos", A.pos()[i], R.pos[i]);
        ck(*(uint64_t*)(A.seq() + i*W + 0) == R.seq[i].to_ullong(), "seq word0",
           (long long)*(uint64_t*)(A.seq() + i*W), (long long)R.seq[i].to_ullong());
        ck((A.used()[i] != 0) == (R.used.count((uint32_t)i) != 0), "used",
           A.used()[i], (long long)R.used.count((uint32_t)i));
        ck(A.shift()[i] == R.shift[i], "shift", A.shift()[i], R.shift[i]);
    }
    ck(fg199k_used_max_p1(A.base(), A.n) == R.used_max_p1(), "used_max_p1",
       (long long)fg199k_used_max_p1(A.base(), A.n), (long long)R.used_max_p1());
    ck(fg199k_is_empty(A.n, A.pos()[0]) == R.is_empty(0), "is_empty0",
       fg199k_is_empty(A.n, A.pos()[0]), R.is_empty(0));
    (void)tag;
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

static void trap_rm_empty(void) { Arena A(4); fg199k_rm(A.base(), A.n, 0); }
static void trap_rm_oob(void)   { Arena A(4); A.pos()[0] = 5; fg199k_rm(A.base(), A.n, 9); }
static void trap_seq_rm_absent(void) {
    Arena A(4); A.pos()[0] = 3;
    fg199k_seq_rm(A.base(), A.n, 0, 1, 0, 1);
}

int main(void) {
    std::mt19937 rng(199001);
    // A deterministic operation sequence over a small cache, mirrored in both arms.
    for (int trial = 0; trial < 40; ++trial) {
        const int64_t n = 8;
        Arena A(n);
        Ref R(n);
        for (int step = 0; step < 60; ++step) {
            const uint32_t i = (uint32_t)(rng() % (unsigned)n);
            const int op = (int)(rng() % 5);
            const int seq_id = (int)(rng() % 4);
            const int widx = seq_id / 32;
            const int64_t bit = (int64_t)1 << (seq_id % 32);
            if (getenv("FG199K_TRACE")) {
                printf("trial=%d step=%d op=%d i=%u seq=%d pos=%lld bits=%llu\n",
                       trial, step, op, i, seq_id, (long long)R.pos[i],
                       (unsigned long long)R.seq[i].to_ullong());
                fflush(stdout);
            }

            if (op == 0) {
                if (!R.is_empty(i)) continue;
                if (!R.seq[i].none()) continue;   // a placed cell always carries a sequence
                // Native asserts the cell does not already hold this sequence.
                if (R.seq[i].test(seq_id)) continue;
                const int64_t p = (int64_t)(rng() % 100);
                // Place first, then add the sequence, and mirror the assert order.
                fg199k_pos_set(A.base(), n, i, p);
                R.pos_set(i, p);
                fg199k_seq_add(A.base(), n, i, widx, bit);
                R.seq_add(i, seq_id);
                (void)0;
            } else if (op == 1) {
                if (R.is_empty(i)) continue;
                if (R.pos[i] == -1) continue;
                fg199k_rm(A.base(), n, i);
                R.rm(i);
            } else if (op == 2) {
                if (R.is_empty(i)) continue;
                if (!R.seq[i].test(seq_id)) continue;
                int64_t g = fg199k_seq_rm(A.base(), n, i, seq_id, widx, bit);
                bool r = R.seq_rm(i, seq_id);
                ck((g != 0) == r, "seq_rm return", g, r);
            } else if (op == 3) {
                int64_t g = fg199k_seq_keep(A.base(), n, i, seq_id, widx, bit);
                bool r = R.seq_keep(i, seq_id);
                ck((g != 0) == r, "seq_keep return", g, r);
            } else {
                if (R.pos[i] == -1) continue;
                ck(fg199k_seq_count(A.base(), n, i) == (int64_t)R.seq_count(i), "seq_count",
                   (long long)fg199k_seq_count(A.base(), n, i), (long long)R.seq_count(i));
            }
            compare(A, R, "step");
        }
    }

    // place_cell against the native sequence: clear-if-occupied, pos_set, seq_add per sequence.
    for (int trial = 0; trial < 20; ++trial) {
        const int64_t n = 8;
        Arena A(n); Ref R(n);
        for (int step = 0; step < 40; ++step) {
            const uint32_t idx = (uint32_t)(rng() % (unsigned)n);
            const int64_t pos = (int64_t)(rng() % 50);
            const int nseq = (int)(rng() % 3);
            std::vector<int64_t> seqs;
            for (int s = 0; s < nseq; ++s) seqs.push_back((int64_t)(rng() % 4));
            int64_t g = fg199k_place_cell(A.base(), n, idx, pos, (int64_t)seqs.size(),
                                          (uint64_t)(uintptr_t)seqs.data());

            bool reused = false;
            if (!R.is_empty(idx)) { R.rm(idx); reused = true; }
            R.pos_set(idx, pos);
            std::set<int> uniq(seqs.begin(), seqs.end());
            for (int s : uniq) R.seq_add(idx, s);
            ck((g != 0) == reused, "place_cell reused", g, reused);
            compare(A, R, "place");
            ck(fg199k_head_after_place((int64_t)idx) == (int64_t)idx + 1, "head", 0, 0);
        }
    }

    // find_slot decisions against the native expressions.
    for (int i = 0; i < 400; ++i) {
        const int64_t head = rng() % 64, used = rng() % 64, nt = rng() % 8;
        const int64_t ref = (head > used + 2*nt) ? 0 : head;
        ck(fg199k_head_start(head, used, nt) == ref, "head_start", fg199k_head_start(head,used,nt), ref);

        const int64_t n_swa = (int64_t)(rng() % 32);
        const int64_t ty = rng() % 4;
        const int64_t p0 = rng() % 64, p1 = rng() % 64;
        bool m = false;
        if (ty == 1) m = (p1 - p0 >= n_swa);
        else if (ty == 2 && n_swa > 0) m = (p0 < (p1 / n_swa) * n_swa);
        else if (ty == 3) { const int64_t h = n_swa/2, d = p1-p0; m = (d < -h || d > h); }
        ck((fg199k_can_use(1, 1, p0, p1, n_swa, ty) != 0) == m, "can_use", 0, m);

        const int64_t occ = rng() % 2, sc = rng() % 3;
        ck(fg199k_can_use(occ, sc, p0, p1, 0, 0) == (occ == 0 ? 1 : 0), "can_use empty", 0, 0);
    }

    run_trap("rm on empty cell", 12, trap_rm_empty);
    run_trap("rm out of range", 11, trap_rm_oob);
    run_trap("seq_rm absent sequence", 23, trap_seq_rm_absent);

    printf("FG199K_FIXTURE checks=%d failures=%d\n", checks, failures);
    if (failures) { printf("EXP199_LOCAL_GATES_FAIL\n"); return 1; }
    printf("EXP199_LOCAL_GATES_PASS\n");
    return 0;
}
