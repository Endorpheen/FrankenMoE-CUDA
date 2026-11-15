// The merged runtime's one host seam (M1, P11-01).
#pragma once

#include <stdint.h>

#include "goose-iq-tables.h"

#ifdef __cplusplus
extern "C" {
#endif

// Allocates the composed unit's runtime exactly once per process, whichever
// bridge is the first to need it. If the CPU table provider is linked, its
// descriptor is installed first, so the tables enter the runtime at init.
void fg_goose_host_ensure_init(void);

// The explicit embedding path: install this descriptor, then initialise.
// Returns 0 on success, the bind error otherwise (the runtime is then left
// uninitialised). Use this when the caller owns the tables, or to pin the
// provider instead of relying on the weak native reference.
int fg_goose_host_init_with_tables(const fg_goose_iq_tables * tables);

// How many times goose_init() was entered; one is the only correct value.
int64_t fg_goose_host_init_runs(void);

#ifdef __cplusplus
}
#endif
