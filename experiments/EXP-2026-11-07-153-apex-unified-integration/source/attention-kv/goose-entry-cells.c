// worker-2/goose-entry-cells.c
// Binds the actual generated entry of cells-gen.c (cells.goose).
#include <stdint.h>

extern int64_t fg_w2_get(int64_t,int64_t,int64_t);
extern void    fg_w2_put(int64_t,int64_t,int64_t,int64_t);
extern int64_t fg_w2_service(int64_t,int64_t,int64_t);
extern int64_t fg_w2_set_create(void);
extern void    fg_w2_set_destroy(int64_t);
extern int64_t fg_w2_set_insert(int64_t,int64_t);
extern int64_t fg_w2_set_erase(int64_t,int64_t);
extern int64_t fg_w2_set_lower_bound(int64_t,int64_t);
extern int64_t fg_w2_set_count(int64_t,int64_t);
extern int64_t fg_w2_set_first(int64_t);
extern int64_t fg_w2_set_last(int64_t);
extern int64_t fg_w2_set_next(int64_t,int64_t);
extern int64_t fg_w2_set_prev(int64_t,int64_t);
extern int64_t fg_w2_set_size(int64_t);
extern int64_t fg_w2_set_key(int64_t,int64_t);
extern void    fg_w2_witness(int64_t);

#define main fg_goose_cells_program_main
#include "cells-gen.c"
#undef main

int64_t fg_goose_cells_entry(int64_t mode, int64_t a, int64_t b, int64_t c,
                             int64_t d, int64_t e, int64_t f) {
    int64_t r = gg_function_1(mode, a, b, c, d, e, f, 0);
    for (int i = 0; i < GG_STACK_COUNT; ++i) { gg_stacks[i].top = 0; }
    return r;
}
