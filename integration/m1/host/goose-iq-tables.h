// The IQ table contract of the merged Goose runtime (M1 seam removal, P11-01).
//
// The composed unit needs the IQ grids (ggml-common.h literals) and the
// run-time map/neighbour tables (built by iq2xs_init_impl / iq3xs_init_impl).
// Those objects are file-static inside ggml/src/ggml-quants.c, so the unit
// cannot name them and must not depend on ggml to reach them.
//
// Instead of 14 link-time entry points defined next to the tables, the unit's
// entry points now dispatch through this descriptor, which is installed once,
// at start-up, by the host that owns the tables. The descriptor is data: the
// runtime library contains no reference to any ggml symbol, and adding a table
// owner (CPU quants today, a device-side owner tomorrow) needs no relink of
// the unit and no duplicate definition.
//
// Signatures are the unit's own: (group, index) -> value, all in int64_t.
// `ctx` is owned by the provider and is passed back untouched; the native CPU
// provider leaves it NULL because its tables are process globals.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// A provider is accepted only if it records this exact ABI and this exact
// struct size, so a stale header fails at bind time instead of misreading
// function pointers.
#define FG_GOOSE_IQ_TABLES_ABI 1u

typedef int64_t (*fg_goose_iq2_grid_fn)(void * ctx, int64_t group, int64_t index);
typedef int64_t (*fg_goose_iq3_grid_fn)(void * ctx, int64_t group, int64_t index);
typedef int64_t (*fg_goose_iq2_map_fn)(void * ctx, int64_t group, int64_t unit);
typedef int64_t (*fg_goose_iq2_nb_fn)(void * ctx, int64_t group, int64_t offset);
typedef int64_t (*fg_goose_iq3_map_fn)(void * ctx, int64_t group, int64_t unit);
typedef int64_t (*fg_goose_iq3_nb_fn)(void * ctx, int64_t group, int64_t offset);

typedef struct fg_goose_iq_tables {
    uint32_t abi;   // FG_GOOSE_IQ_TABLES_ABI
    uint32_t size;  // sizeof(fg_goose_iq_tables), checked at bind time
    void *   ctx;   // provider state, may be NULL

    fg_goose_iq2_grid_fn iq2_grid;  // 4 groups: IQ2_XXS, IQ2_XS, IQ1_S/M, IQ2_S
    fg_goose_iq3_grid_fn iq3_grid;  // 2 groups: IQ3_XXS, IQ3_S
    fg_goose_iq2_map_fn  iq2_map;   // built by ggml_quantize_init()
    fg_goose_iq2_nb_fn   iq2_nb;
    fg_goose_iq3_map_fn  iq3_map;
    fg_goose_iq3_nb_fn   iq3_nb;
} fg_goose_iq_tables;

// Installs the descriptor. Returns 0 on success (including a repeated bind of
// the same pointer), and a negative value on a bad ABI/size/function table or
// on an attempt to replace an already installed provider. Binding is
// idempotent, not replaceable: two owners of the same process-global tables
// would be a second copy, which is what this seam removes.
int fg_goose_host_bind_iq_tables(const fg_goose_iq_tables * tables);

// 1 once a provider is installed, 0 before. Lets a gate measure the seam.
int fg_goose_host_iq_tables_bound(void);

// The CPU provider, implemented in ggml/src/ggml-quants.c where the tables
// live. The runtime references it weakly: a host that never links ggml can
// still use the unit, and the first IQ access then fails loudly instead of
// reading a NULL pointer table.
const fg_goose_iq_tables * fg_goose_iq_tables_native(void);

#ifdef __cplusplus
}
#endif
