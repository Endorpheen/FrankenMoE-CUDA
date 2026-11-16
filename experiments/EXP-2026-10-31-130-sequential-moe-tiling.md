# EXP-2026-10-31-130: sequential GPU MoE prefill tiling

Status: NOT_RUN.

The most complex direction; it requires the EXP121/EXP123 lifetime map and a
concrete staging scheme before implementation. No limited-expert-group tiling,
scatter or allocator/graph integration was built. The deferred EXP107 overlap is
not included.
