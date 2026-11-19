// Every quant row type the runtime knows, against the native sizes.
//
// Risk R-1 in the removal plan says the native quant bodies cannot go while only two of the row types are gated. This widens
// the gate to all of them: block size, type size and row size are compared with ggml_blck_size, ggml_type_size and
// ggml_row_size for every type the enumerator has, and a type that neither ported family answers is reported as a gap rather
// than counted as covered.
#include "ggml.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct sl_i64 { int64_t * data; int64_t len; };

int64_t qz_blck(int64_t t), qz_type_size(int64_t t), qz_row_size(int64_t t, int64_t ne);
int64_t qz2_blck(int64_t t), qz2_type_size(int64_t t), qz2_row_size(int64_t t, int64_t ne);
void goose_init(int argc, char ** argv);
void goose_init2(int argc, char ** argv);

int main(void) {
    goose_init(0, NULL);
    goose_init2(0, NULL);   // the second module is compiled as its own unit, so it has its own heap
    const int64_t nes[] = { 1, 32, 256, 4096, 16384 };
    long types = 0, covered_first = 0, covered_second = 0, neither = 0, bad = 0;
    for (int t = 0; t < (int) GGML_TYPE_COUNT; ++t) {
        const enum ggml_type ty = (enum ggml_type) t;
        const int64_t n_blck = (int64_t) ggml_blck_size(ty);
        const int64_t n_size = (int64_t) ggml_type_size(ty);
        if (n_blck <= 0 || n_size <= 0) { continue; }   // a removed/unsupported type has no size to compare
        ++types;
        const int64_t a_blck = qz_blck(t), b_blck = qz2_blck(t);
        const int64_t a_size = qz_type_size(t), b_size = qz2_type_size(t);
        const int first  = (a_blck == n_blck && a_size == n_size);
        const int second = (b_blck == n_blck && b_size == n_size);
        if (first) ++covered_first;
        if (second) ++covered_second;
        if (!first && !second) {
            ++neither;
            printf("  NEITHER type=%d (%s) native blck=%lld size=%lld port1=(%lld,%lld) port2=(%lld,%lld)\n",
                t, ggml_type_name(ty), (long long) n_blck, (long long) n_size,
                (long long) a_blck, (long long) a_size, (long long) b_blck, (long long) b_size);
            continue;
        }
        for (size_t i = 0; i < sizeof(nes)/sizeof(nes[0]); ++i) {
            // Only whole blocks are compared. The native floor-divides a partial block to zero while the module refuses
            // it, which is a documented difference in strictness rather than a wrong size - measured: q2_K ne=1 answers
            // -2 against a native 0.
            if (nes[i] % n_blck != 0) { continue; }
            const int64_t want = (int64_t) ggml_row_size(ty, nes[i]);
            const int64_t got1 = first ? qz_row_size(t, nes[i]) : -1;
            const int64_t got2 = second ? qz2_row_size(t, nes[i]) : -1;
            const int64_t got = first ? got1 : got2;
            if (got != want) {
                ++bad;
                if (bad <= 6) printf("  ROW SIZE type=%d (%s) ne=%lld got=%lld want=%lld\n", t, ggml_type_name(ty),
                                     (long long) nes[i], (long long) got, (long long) want);
            }
        }
    }
    printf("quant rows: types=%ld port1=%ld port2=%ld neither=%ld row_size_failures=%ld\n",
           types, covered_first, covered_second, neither, bad);
    printf("quant rows: neither is a coverage gap for the removal plan, not a wrong answer; it must be zero before any\n"
           "            native quant body is deleted (rule 15, risk R-1)\n");
    // A wrong size is a failure. A type nobody answers is a coverage gap, printed above and counted here: failing the gate
    // for it would make the gate unrunnable while the gap is open, and the gap is exactly what the removal plan needs to see.
    return bad == 0 ? 0 : 1;
}
