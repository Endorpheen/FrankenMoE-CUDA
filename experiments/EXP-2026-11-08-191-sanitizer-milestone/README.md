# EXP191: whole-backend sanitizer milestone (P3-19)

Runs every qualified module's differential fixture under three compute-sanitizer tools
and records the results in one machine-readable artifact.

The fixtures already prove byte-level equality against native in both arms. This adds the
checks a passing byte comparison cannot cover, and each tool catches something the others
do not:

- **memcheck** — out-of-bounds and misaligned access. In this migration it caught the
  misaligned 4-byte read in EXP186 (a 210-byte block makes `get_int_b2` only 2-byte
  aligned) and the stale-`ybase` out-of-bounds in EXP185.
- **racecheck** — shared-memory hazards between the cross-warp staging writes and the
  reads that follow them.
- **initcheck** — reads of uninitialised shared staging, which is exactly the failure mode
  of a reduction that forgets to zero a slot above the warp count (the EXP185 defect).

Run with `python3 run_milestone.py`; it writes `runtime/sanitizer-milestone/milestone.json`
plus one log per module and tool.

## Result

| module | memcheck | racecheck | initcheck |
|---|---:|---:|---:|
| EXP175 concat | 0 | 0 | 0 |
| EXP176 get_rows | 0 | 0 | 0 |
| EXP177 cpy | 0 | 0 | 0 |
| EXP178 q8_1 quantize | 0 | 0 | 0 |
| EXP179 MoE weighted | 0 | 0 | 0 |
| EXP181 mm_ids | 0 | **5** | 0 |
| EXP183 FWHT | 0 | 0 | 0 |
| EXP184 mul_mat_vec_f | 0 | 0 | 0 |
| EXP185 mul_mat_vec_q Q4_K | 0 | 0 | 0 |
| EXP188 Q4_0 dequant | 0 | 0 | 0 |

Nine of ten modules are clean under all three tools. All ten are clean under memcheck and
initcheck.

## The one non-zero result, and why it is not a regression

EXP181's `racecheck` reports five write-write hazards in the shared staging array that
packs `(token, expert)` slots during compaction.

The same array is used by the native kernel, so the native arm was run under the same
tool for comparison:

| arm | racecheck hazards |
|---|---:|
| native | **18** |
| Goose | **5** |

The pattern is inherent to the algorithm — an `__shfl`-based inclusive prefix sum where
several lanes can transiently target the same staging slot before `syncwarp()` orders the
publish — and is not introduced by the transfer. The Goose body shows *fewer* hazards than
the native one it replaces.

The script re-derives that parity on every run: for a module with a known inherent
hazard it executes the native arm under the same tool and only passes if native reports
at least as many hazards. A note in a README cannot clear a result; the comparison has to
reproduce.

It is recorded as `PASS_WITH_NATIVE_PARITY` rather than silently cleared, because the
honest statement is "the algorithm has this hazard and both implementations share it",
not "the hazard is zero". `memcheck` and `initcheck` are clean in both arms and the byte
comparison against native is exact, so nothing here changes a result.

## Scope and limits

This covers the ten modules with a standalone GPU fixture. It does not cover the CUDA
DSO as a whole, because a whole-library run needs the full model and the sanitizer's
memory overhead does not fit the 11.8 GB card next to a 35B model. The per-module
fixtures exercise the same kernels on the same compile path with representative shapes,
so the coverage is real but narrower than a whole-backend run.

Every module also passes its own differential gate; this milestone does not replace those
gates, it adds the memory and concurrency dimension they lack.
