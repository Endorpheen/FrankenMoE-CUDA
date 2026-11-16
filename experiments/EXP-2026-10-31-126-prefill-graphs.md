# EXP-2026-10-31-126: retained prefill CUDA graph memory

Status: NOT_RUN.

EXP121 shows ~1370 MiB of compute/graph/staging capacity retained after cold32K,
but does not separate graph instances from allocator/pool memory. No graph
lifetime experiment was run and no graph was disabled globally. The EXP093
74 MiB temporary-pool result is not reused as the graph bound.
