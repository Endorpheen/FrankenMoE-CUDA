// seam.cpp - strong definitions of the Goose-to-Goose cells seams.
#include "bridge.h"
extern "C" int64_t fg153_goose_cells_entry(int64_t mode, int64_t a, int64_t b,
                                           int64_t c, int64_t d, int64_t e);
static int64_t fg153_cfg(int64_t h, int64_t k) { return fg153_lg(h, k, 0); }

extern "C" int64_t fg153_lc(int64_t op, int64_t a, int64_t b, int64_t c) {
    switch (op) {
        case 0:  return fg153_goose_cells_entry(0, a, fg153_cfg(b, 0), fg153_cfg(b, 1),
                                                fg153_cfg(b, 2), fg153_cfg(b, 3));
        case 10: return fg153_goose_cells_entry(10, a, b, 0, 0, 0);
        case 11: return fg153_goose_cells_entry(11, a, b, 0, 0, 0);
        case 12: return fg153_goose_cells_entry(12, a, b, 0, 0, 0);
        case 13: return fg153_goose_cells_entry(13, 0, 0, 0, 0, 0);
        default: return -1;
    }
}
extern "C" int64_t fg153_sc(int64_t op, int64_t a, int64_t b, int64_t c) {
    switch (op) {
        case 1:  return fg153_goose_cells_entry(1,  a, b, 0, 0, 0);
        case 2:  return fg153_goose_cells_entry(2,  a, b, -1, -1, 0);
        case 9:  return fg153_goose_cells_entry(9,  a, b, c, 0, 0);
        case 10: return fg153_goose_cells_entry(10, a, b, 0, 0, 0);
        case 18: return fg153_goose_cells_entry(18, a, b, c, 0, 0);
        case 19: return fg153_goose_cells_entry(25, a, b, 0, 0, 0);
        default: return -1;
    }
}
