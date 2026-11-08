// worker-7: llm_graph_context resource-metadata adapter.
//
// This is the only C++ glue for the constructor metadata. It does not derive
// fields: graph-context.goose writes every numeric and pointer slot. The checks
// below prove the projection is exact for the resources the constructor binds.
#include "graph-meta.h"

#include <cstddef>
#include <cstdint>

int fg153_graph_meta_fill(const llm_graph_params & params, fg153_graph_meta & meta) {
    const int64_t rc = fg153_goose_ctx(0, (int64_t) (intptr_t) &params, (int64_t) (intptr_t) &meta, 0, 0, 0);
    if (rc != 0) {
        return (int) rc;
    }
    const int64_t want[] = {
        (int64_t) params.arch,
        (int64_t) (intptr_t) params.sched,
        (int64_t) (intptr_t) params.backend_cpu,
        (int64_t) (intptr_t) params.cvec,
        (int64_t) (intptr_t) params.loras,
        (int64_t) (intptr_t) params.mctx,
        (int64_t) (intptr_t) params.cross,
        (int64_t) (intptr_t) params.moe_cache,
        (int64_t) (intptr_t) params.prec_policy,
        (int64_t) (intptr_t) params.hdmd,
        (int64_t) (intptr_t) params.res,
    };
    const int64_t got[] = {
        meta.arch, meta.sched, meta.backend_cpu, meta.cvec, meta.loras, meta.mctx,
        meta.cross, meta.moe_cache, meta.prec_policy, meta.hdmd, meta.res,
    };
    for (size_t i = 0; i < sizeof(want) / sizeof(want[0]); ++i) {
        if (want[i] != got[i]) {
            return -1;
        }
    }
    return 0;
}
