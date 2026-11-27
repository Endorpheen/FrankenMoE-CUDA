#include "step.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

__attribute__((noinline)) static void reference(int64_t base, int64_t count, int64_t cells,
        int32_t *pos, int64_t *rows, uint16_t *mask, int32_t *ids, int64_t output_count) {
    for (int64_t row = 0; row < count; ++row) {
        pos[row] = (int32_t)(base + row);
        rows[row] = pos[row];
        for (int64_t cell = 0; cell < cells; ++cell)
            mask[row * cells + cell] = cell <= pos[row] ? 0 : UINT16_C(0xfc00);
    }
    for (int64_t i = 0; i < output_count; ++i) ids[i] = (int32_t)(count-output_count+i);
}

static unsigned long long now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

int main(void) {
    static const int64_t shapes[][4] = {{0,1,1,0},{0,31,32,3},{0,33,64,0},
        {1023,7,1030,1},{32767,1,32768,0},{28672,4096,32768,1}};
    goose_init(0, NULL);
    for (unsigned int j=0; j<sizeof shapes / sizeof *shapes; ++j) {
        int64_t base=shapes[j][0], count=shapes[j][1], cells=shapes[j][2], out=shapes[j][3];
        size_t area=(size_t)count*cells;
        uint16_t *expect=malloc(area*2), *got=malloc(area*2);
        assert(expect && got);
        int32_t pos[4096], pos2[4096], ids[4096], ids2[4096];
        int64_t rows[4096], rows2[4096];
        reference(base,count,cells,pos,rows,expect,ids,out);
        giga_fill_step_inputs(base,count,cells,(sl_i32){pos2,count},(sl_i64){rows2,count},
                              (sl_u16){got,area},(sl_i32){ids2,out});
        assert(!memcmp(expect,got,area*2) && !memcmp(pos,pos2,count*4) &&
               !memcmp(rows,rows2,count*8) && !memcmp(ids,ids2,out*4));
        if (j >= 4) {
            int repeat = count == 1 ? 8000 : 6;
            for (int pass=0; pass<4; ++pass) {
                int candidate=pass==1 || pass==2;
                unsigned long long start=now();
                for (int i=0; i<repeat; ++i) {
                    if (candidate) giga_fill_step_inputs(base,count,cells,(sl_i32){pos2,count},
                        (sl_i64){rows2,count},(sl_u16){got,area},(sl_i32){ids2,out});
                    else reference(base,count,cells,pos2,rows2,got,ids2,out);
                }
                printf("%s %lld rows %.3f ms per call\n",candidate?"Goose":"C",
                       (long long)count,(now()-start)/1000000.0/repeat);
            }
        }
        free(expect); free(got);
    }
    puts("Exact mask, position, row and output ID parity on all six shapes");
}
