/* The rejection-acceptance door over a grid, against the native rule written out from
 * common_sampler_sample_and_accept_n_rejection: accept = q_x > 0 && (p_x >= q_x || u < p_x / q_x).
 * No model is loaded: the door is a pure function and the library exports it. */
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>

typedef int64_t (*rej_fn)(double, double, double);

int main(int argc, char ** argv) {
    const char * lib = argc > 1 ? argv[1] : "libllama.so.0";
    void * h = dlopen(lib, RTLD_LAZY | RTLD_NOW);
    if (!h) { printf("dlopen failed: %s\n", dlerror()); return 2; }
    rej_fn fn = (rej_fn) dlsym(h, "fg_sp_reject_accept");
    if (!fn) { printf("symbol missing\n"); return 2; }

    const double inf = INFINITY, nan = NAN;
    const double ps[] = { 0.0, 1e-9, 1e-3, 0.1, 0.25, 0.5, 0.75, 1.0, inf, nan };
    const double qs[] = { -1.0, 0.0, 1e-9, 1e-3, 0.1, 0.25, 0.5, 0.75, 1.0, inf, nan };
    const double us[] = { 0.0, 1e-9, 0.1, 0.25, 0.5, 0.75, 0.999999, 1.0 };

    long calls = 0, bad = 0;
    for (unsigned i = 0; i < sizeof(ps)/sizeof(ps[0]); ++i)
    for (unsigned j = 0; j < sizeof(qs)/sizeof(qs[0]); ++j)
    for (unsigned k = 0; k < sizeof(us)/sizeof(us[0]); ++k) {
        const double p = ps[i], q = qs[j], u = us[k];
        const int native = (q > 0.0) && (p >= q || u < p / q);
        const int64_t goose = fn(p, q, u);
        ++calls;
        if ((goose == 1) != native) {
            ++bad;
            if (bad <= 8) printf("DIFF goose=%lld native=%d p=%.9g q=%.9g u=%.9g\n",
                                 (long long) goose, native, p, q, u);
        }
    }
    printf("rejection door: calls=%ld disagreements=%ld\n", calls, bad);
    return bad ? 1 : 0;
}
