/* worker-10: bind the real generated Goose entry ids to a stable test ABI. */
#include "apex-move-bridge.h"
#include "generated/apex-move-entry-ids.h"

#include <stdint.h>

#define FG153_AM_JOIN2(a, b) a ## b
#define FG153_AM_FN(id) FG153_AM_JOIN2(gg_function_, id)

#define main fg153_am_generated_unused_main
#include "generated/apex-move.c"
#undef main
#include "generated/apex-move-const-init.h"

int64_t fg153_am_call(int64_t kind, int64_t serial, int64_t params_ptr) {
    static int initialized;
    if (!initialized) { fg153_am_init_globals(); initialized = 1; }
    switch (kind) {
        case FG153_AM_OP_COPY_SCALAR:
            return serial ? FG153_AM_FN(FG153_AM_FN_COPY_SCALAR_SERIAL)(params_ptr, 0)
                          : FG153_AM_FN(FG153_AM_FN_COPY_SCALAR_DEVICE)(params_ptr, 0);
        case FG153_AM_OP_COPY_CONTIG:
            return serial ? FG153_AM_FN(FG153_AM_FN_COPY_CONTIG_SERIAL)(params_ptr, 0)
                          : FG153_AM_FN(FG153_AM_FN_COPY_CONTIG_DEVICE)(params_ptr, 0);
        case FG153_AM_OP_COPY_BLOCKS:
            return serial ? FG153_AM_FN(FG153_AM_FN_COPY_BLOCKS_SERIAL)(params_ptr, 0)
                          : FG153_AM_FN(FG153_AM_FN_COPY_BLOCKS_DEVICE)(params_ptr, 0);
        case FG153_AM_OP_TRANSPOSE:
            return serial ? FG153_AM_FN(FG153_AM_FN_TRANSPOSE_SERIAL)(params_ptr, 0)
                          : FG153_AM_FN(FG153_AM_FN_TRANSPOSE_DEVICE)(params_ptr, 0);
        case FG153_AM_OP_GETROWS:
            return serial ? FG153_AM_FN(FG153_AM_FN_GETROWS_SERIAL)(params_ptr, 0)
                          : FG153_AM_FN(FG153_AM_FN_GETROWS_DEVICE)(params_ptr, 0);
        case FG153_AM_OP_SETROWS:
            return serial ? FG153_AM_FN(FG153_AM_FN_SETROWS_SERIAL)(params_ptr, 0)
                          : FG153_AM_FN(FG153_AM_FN_SETROWS_DEVICE)(params_ptr, 0);
        case FG153_AM_OP_Q4_DEQUANT:
            return FG153_AM_FN(FG153_AM_FN_Q4_DEQUANT_DEVICE)(params_ptr, 0);
        default:
            return -1;
    }
}
