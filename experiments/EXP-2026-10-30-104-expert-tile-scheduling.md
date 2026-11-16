# EXP-2026-10-30-104: expert/output-row tile scheduling

Status: ALREADY_PRESENT. The single-token decode path already distributes
expert output-row tiles; no patch.

## Finding

`ggml_compute_forward_mul_mat_id()` in `ggml/src/ggml-cpu/ggml-cpu.c`
(lines 1690-1748 of the composite base):

- `chunk_size = 16`; for decode `nr1 == 1`, so `chunk_size = 64`;
- `nchunk0 = (nr0 + 63)/64`, `nchunk1 = 1`; when `nchunk0 < nth*4` the fallback
  assigns one contiguous row range per worker (`nchunk0 = nth`, `nchunk1 = 1`);
- each worker takes one `(ith0, ith1)` tile of one expert and the next tile via
  `atomic_fetch_add` on the per-expert chunk counter.

For a single token each worker therefore processes 64 contiguous output rows of
the current expert, and work-stealing distributes whole experts across the eight
workers. This is the "expert/output-row tile" organization the hypothesis asks
for; threads (8/16) and affinity are unchanged and were not retuned.

## Verdict

ALREADY_PRESENT for the described mechanism. Changing the tile shape without a
measured scheduling/cache cost would be a blind experiment; no NUMA claim is
made (the two L3 domains are not treated as separate NUMA nodes).
