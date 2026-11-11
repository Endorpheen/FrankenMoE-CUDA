// Differential gate for the Goose Hadamard matrix and rotation sizes.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <cmath>
#include <random>

extern "C" uint8_t fg320dh_run(int64_t packet);

static int64_t g_packet[4][16];

extern "C" int64_t fg320k_get(int64_t p, int64_t k, int64_t i) { return ((int64_t *)(uintptr_t)p)[k*16 + i]; }
extern "C" void fg320k_put(int64_t p, int64_t k, int64_t i, uint64_t v) { ((int64_t *)(uintptr_t)p)[k*16 + i] = (int64_t)v; }
extern "C" int64_t fg320k_ld_i64(uint64_t base, int64_t byte) { return *(const int64_t *)(uintptr_t)(base + (uint64_t)byte); }
extern "C" void fg320k_st_i64(uint64_t base, int64_t byte, int64_t v) { *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v; }
extern "C" void fg320k_fail(int64_t c) { std::fprintf(stderr, "FG320K validation code %lld\n", (long long)c); std::abort(); }

static int checks, failures;
static void check(bool ok, const char * what, int trial) {
    ++checks;
    if (!ok) { ++failures; if (failures < 10) std::fprintf(stderr, "FAIL %s trial=%d\n", what, trial); }
}

static std::vector<int64_t> out(1 << 18);

static int64_t call(int op, int64_t n, int64_t ek, int64_t ev, int64_t arch, int64_t keq,
                    int64_t rdis, int64_t vq, int64_t hv) {
    for (int i = 0; i < 16; ++i) g_packet[0][i] = 0;
    std::fill(out.begin(), out.end(), -99);
    g_packet[0][0] = op;
    g_packet[0][1] = (int64_t)(uintptr_t)out.data();
    g_packet[0][2] = n; g_packet[0][3] = ek; g_packet[0][4] = ev; g_packet[0][5] = arch;
    g_packet[0][6] = keq; g_packet[0][7] = rdis; g_packet[0][8] = vq; g_packet[0][9] = hv;
    g_packet[3][0] = 0; g_packet[3][1] = 0;
    if (!fg320dh_run((int64_t)(uintptr_t)g_packet)) std::abort();
    return g_packet[3][1];
}

int main(void) {
    std::mt19937 rng(320011);

    // The native generator, transcribed: signs only, then the scale.
    for (int trial = 0; trial < 2000; ++trial) {
        const int64_t n = (int64_t)1 << (rng() % 8);
        std::vector<double> want((size_t)(n*n), 0.0);
        want[0] = 1.0 / std::sqrt((double)n);
        for (int64_t s = 1; s < n; s *= 2)
            for (int64_t i = 0; i < s; ++i)
                for (int64_t j = 0; j < s; ++j) {
                    const double val = want[i*n + j];
                    want[(i+s)*n + j] = val;
                    want[i*n + (j+s)] = val;
                    want[(i+s)*n + (j+s)] = -val;
                }

        const int64_t got = call(0, n, 0, 0, 0, 0, 0, 0, 0);
        check(got == n*n, "matrix size", trial);
        if (got != n*n) continue;
        const double scale = 1.0 / std::sqrt((double)n);
        for (int64_t i = 0; i < n*n; ++i) {
            const int64_t want_sign = (int64_t)std::llround(want[i] / scale);
            check(out[i] == want_sign, "matrix sign", trial);
        }
        // orthonormality of the reconstructed matrix: rows are orthogonal, norms equal 1
        for (int64_t a = 0; a < n; a += 7) {
            for (int64_t b = 0; b < n; b += 5) {
                double dot = 0.0;
                for (int64_t k = 0; k < n; ++k) dot += (double)out[a*n + k] * (double)out[b*n + k];
                const double want_dot = (a == b) ? (double)n : 0.0;
                check(std::fabs(dot - want_dot) < 1e-9, "orthogonality", trial);
            }
        }
    }

    for (int trial = 0; trial < 4000; ++trial) {
        const int64_t embd_k = (int64_t)(rng() % 4096);
        const int64_t embd_v = (int64_t)(rng() % 4096);
        const int64_t arch = rng() & 1;
        const int64_t keq = rng() & 1;
        const int64_t rdis = rng() & 1;
        const int64_t vq = rng() & 1;
        const int64_t hv = (int64_t)(rng() % 512);

        int64_t rot_k = (arch && keq) ? 1 : 0;
        int64_t n_rot_k = 0;
        if (rot_k && embd_k > 0) {
            n_rot_k = 64;
            while (embd_k % (2*n_rot_k) == 0) n_rot_k *= 2;
        }
        int64_t rot_v = 0, n_rot_v = 0;
        if (!rdis && embd_v > 0 && vq && hv % 64 == 0) { rot_v = 1; n_rot_v = 64; }

        call(1, 0, embd_k, embd_v, arch, keq, rdis, vq, hv);
        check(out[0] == rot_k, "rot_k", trial);
        check(out[1] == n_rot_k, "n_rot_k", trial);
        check(out[2] == rot_v, "rot_v", trial);
        check(out[3] == n_rot_v, "n_rot_v", trial);
    }

    std::printf("FG320K_FIXTURE checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
