/* fp_line against the native format, written out from the fit-params tool:
 *   printf("-c %" PRIu32 " -ngl %" PRIi32, cparams.n_ctx, mparams.n_gpu_layers);
 *   when more than one device: " -ts " then the tensor split as uint32 values separated by commas.
 * That is the whole line the tool prints for the simple case, so that is what is compared. */
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef struct { void * d; int64_t n; } sl_i64;
typedef struct { void * d; int64_t n; } sl_u8;
typedef struct { void * d; int64_t n; } sl_f64;
typedef int64_t (*fp_line_fn)(int64_t, int64_t, sl_f64, int64_t, sl_u8, sl_i64, sl_i64,
                              sl_u8, sl_i64, sl_i64, int64_t, sl_u8);

int main(int argc, char ** argv) {
    void * h = dlopen(argc > 1 ? argv[1] : "libllama.so.0", RTLD_LAZY | RTLD_NOW);
    if (!h) { printf("dlopen failed\n"); return 2; }
    fp_line_fn fp = (fp_line_fn) dlsym(h, "fp_line");
    if (!fp) { printf("fp_line missing\n"); return 2; }

    const int64_t ctxs[] = { 512, 4096, 32768, 262144 };
    const int64_t ngls[] = { 0, 8, 41, 42 };
    long calls = 0, bad = 0;
    for (unsigned a = 0; a < sizeof(ctxs)/sizeof(ctxs[0]); ++a)
    for (unsigned b = 0; b < sizeof(ngls)/sizeof(ngls[0]); ++b) {
        char native[256];
        snprintf(native, sizeof(native), "-c %u -ngl %d\n", (uint32_t) ctxs[a], (int32_t) ngls[b]);
        char out[512]; memset(out, 0, sizeof(out));
        const double splits[1] = { 1.0 };
        sl_f64 s_splits = { (void *) splits, 1 };
        char pats[8], bufts[8]; int64_t poff[1], plen[1], boff[1], blen[1];
        sl_u8  s_pats = { pats, 0 };  sl_i64 s_poff = { poff, 0 }; sl_i64 s_plen = { plen, 0 };
        sl_u8  s_bufts = { bufts, 0 }; sl_i64 s_boff = { boff, 0 }; sl_i64 s_blen = { blen, 0 };
        sl_u8  s_out = { out, (int64_t) sizeof(out) };
        (void) fp(ctxs[a], ngls[b], s_splits, 1, s_pats, s_poff, s_plen, s_bufts, s_boff, s_blen, 0, s_out);
        ++calls;
        if (strcmp(out, native) != 0) {
            ++bad;
            printf("DIFF ctx=%lld ngl=%lld native=[%s] goose=[%s]\n",
                   (long long) ctxs[a], (long long) ngls[b], native, out);
        }
    }
    /* the -ts branch: the native tool prints it when more than one device is present, and the values are
       uint32_t casts of the split floats, not their decimal text - that quirk is what has to match */
    const double split_sets[3][2] = { { 0.6, 0.4 }, { 0.5, 0.5 }, { 1.0, 0.0 } };
    for (unsigned k = 0; k < 3; ++k) {
        char native[256];
        /* the native tool trims trailing zero splits first: while (nd > 1 && split[nd-1] == 0) nd--; */
        unsigned nd = 2;
        while (nd > 1 && split_sets[k][nd - 1] == 0.0) { nd--; }
        int w = snprintf(native, sizeof(native), "-c %u -ngl %d", 4096u, 41);
        if (nd > 1) {
            w += snprintf(native + w, sizeof(native) - (size_t) w, " -ts ");
            for (unsigned id = 0; id < nd; ++id) {
                w += snprintf(native + w, sizeof(native) - (size_t) w, "%s%u", id > 0 ? "," : "",
                              (uint32_t) split_sets[k][id]);
            }
        }
        snprintf(native + w, sizeof(native) - (size_t) w, "\n");
        char out[512]; memset(out, 0, sizeof(out));
        sl_f64 s_splits = { (void *) split_sets[k], 2 };
        char pats[8], bufts[8]; int64_t poff[1], plen[1], boff[1], blen[1];
        sl_u8  s_pats = { pats, 0 };  sl_i64 s_poff = { poff, 0 }; sl_i64 s_plen = { plen, 0 };
        sl_u8  s_bufts = { bufts, 0 }; sl_i64 s_boff = { boff, 0 }; sl_i64 s_blen = { blen, 0 };
        sl_u8  s_out = { out, (int64_t) sizeof(out) };
        (void) fp(4096, 41, s_splits, 2, s_pats, s_poff, s_plen, s_bufts, s_boff, s_blen, 0, s_out);
        ++calls;
        if (strcmp(out, native) != 0) {
            ++bad;
            printf("DIFF ts k=%u native=[%s] goose=[%s]\n", k, native, out);
        }
    }

    /* the -ot branch: " -ot \"pattern=backend" per override, comma separated, closing quote at the end */
    {
        const char pats_s[] = "blk.*,attn_q*";
        const char buft_s[] = "CPU,CUDA0";
        const int64_t poff[2] = { 0, 6 };
        const int64_t plen[2] = { 5, 7 };
        const int64_t boff[2] = { 0, 4 };
        const int64_t blen[2] = { 3, 5 };
        char native[256];
        snprintf(native, sizeof(native), "-c 4096 -ngl 41 -ot \"blk.*=CPU,attn_q*=CUDA0\"\n");
        char out[512]; memset(out, 0, sizeof(out));
        const double splits[1] = { 1.0 };
        sl_f64 s_splits = { (void *) splits, 1 };
        sl_u8  s_pats = { (void *) pats_s, (int64_t) strlen(pats_s) };
        sl_i64 s_poff = { (void *) poff, 2 }; sl_i64 s_plen = { (void *) plen, 2 };
        sl_u8  s_bufts = { (void *) buft_s, (int64_t) strlen(buft_s) };
        sl_i64 s_boff = { (void *) boff, 2 }; sl_i64 s_blen = { (void *) blen, 2 };
        sl_u8  s_out = { out, (int64_t) sizeof(out) };
        (void) fp(4096, 41, s_splits, 1, s_pats, s_poff, s_plen, s_bufts, s_boff, s_blen, 2, s_out);
        ++calls;
        if (strcmp(out, native) != 0) {
            ++bad;
            printf("DIFF ot native=[%s] goose=[%s]\n", native, out);
        }
    }

    /* control: the same comparison with the expectation shifted by one must differ, or the comparison
       would be reporting zero for reasons other than agreement */
    int control_differs = 0;
    {
        char out[512]; memset(out, 0, sizeof(out));
        const double splits[1] = { 1.0 };
        sl_f64 s_splits = { (void *) splits, 1 };
        char pats[8], bufts[8]; int64_t poff[1], plen[1], boff[1], blen[1];
        sl_u8  s_pats = { pats, 0 };  sl_i64 s_poff = { poff, 0 }; sl_i64 s_plen = { plen, 0 };
        sl_u8  s_bufts = { bufts, 0 }; sl_i64 s_boff = { boff, 0 }; sl_i64 s_blen = { blen, 0 };
        sl_u8  s_out = { out, (int64_t) sizeof(out) };
        (void) fp(4096, 41, s_splits, 1, s_pats, s_poff, s_plen, s_bufts, s_boff, s_blen, 0, s_out);
        const char * expected_wrong = "-c 4097 -ngl 41\n";
        control_differs = strcmp(out, expected_wrong) != 0;
    }
    printf("control: expectation shifted by one, differs=%d\n", control_differs);
    if (!control_differs) { ++bad; }

    printf("fp_line: calls=%ld disagreements=%ld\n", calls, bad);
    return bad ? 1 : 0;
}
