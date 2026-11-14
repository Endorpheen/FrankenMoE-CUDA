// Smoke call for the quant set (quant-types + quant-allow + rows + rows2 +
// dequant + dequant2): the block geometry of Q4_0 and a row size.
#include "quant.h"

#include <stdio.h>

void goose_init_quant(int argc, char **argv);
int64_t qz_blck(int64_t t);
int64_t qz_type_size(int64_t t);
int64_t qz_row_size(int64_t t, int64_t ne);
int64_t qz2_row_size(int64_t t, int64_t ne);
int64_t qz_dequant_block_size(int64_t t);
int64_t qd_dequant_elems(int64_t t);

int run_quant(void) {
    const int64_t T_Q4_0 = 2, T_Q8_0 = 8;
    char *argv[] = {(char *)"smoke", NULL};

    goose_init_quant(1, argv);
    int64_t blck = qz_blck(T_Q4_0);
    int64_t tsize = qz_type_size(T_Q4_0);
    int64_t row = qz_row_size(T_Q4_0, 256);
    int64_t row2 = qz2_row_size(12 /* T_Q4_K, the rows2 type set */, 256);
    int64_t dblck = qz_dequant_block_size(T_Q4_0);
    int64_t q8 = qz_row_size(T_Q8_0, 256);
    int64_t elems = qd_dequant_elems(T_Q4_0);

    printf("quant:  qz_blck(Q4_0)=%lld qz_type_size=%lld qz_row_size(256)=%lld\n",
           (long long)blck, (long long)tsize, (long long)row);
    printf("quant:  qz2_row_size(Q4_K,256)=%lld qz_dequant_block_size=%lld qz_row_size(Q8_0,256)=%lld qd_dequant_elems=%lld\n",
           (long long)row2, (long long)dblck, (long long)q8, (long long)elems);
    return (blck == 32 && tsize == 18 && row == 144 && q8 == 272 && row2 > 0) ? 0 : 1;
}
