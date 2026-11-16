# EXP-2026-10-31-124: lifetime and aliasing of large activations

Status: NOT_RUN.

EXP121 shows that the retained 32K reserve is dominated by compute/mask/graph
capacity (~1370 MiB), so the direction is relevant. The actual liveness map
(gate/up, GLU, down, cross-backend copies) and the allocator's reuse decisions
were not extracted, and no inplace change was made. Existing inplace mechanisms
were not re-claimed as new.
