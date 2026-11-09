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
| module | memcheck | racecheck | initcheck |
|---|---:|---:|---:|
| EXP175 concat | 0 | 0 | 0 |
| EXP176 get_rows | 0 | 0 | 0 |
| EXP177 cpy | 0 | 0 | 0 |
| EXP178 q8_1 quantize | 0 | 0 | 0 |
| EXP179 MoE weighted | 0 | 0 | 0 |
| EXP181 mm_ids | 0 | 0 | 0 |
| EXP183 FWHT | 0 | 0 | 0 |
| EXP184 mul_mat_vec_f | 0 | 0 | 0 |
| EXP185 mul_mat_vec_q Q4_K | 0 | 0 | 0 |
| EXP188 Q4_0 dequant | 0 | 0 | 0 |

All ten modules are clean under all three tools.

## A wrong conclusion, corrected

The first run of this milestone reported five write-write hazards in EXP181's shared
staging array. The native arm was run for comparison and showed eighteen, and that was
taken to mean the hazard was inherent to the algorithm and the transfer had merely
inherited it. The milestone was recorded as `PASS_WITH_NATIVE_PARITY` on that basis.

That reasoning was wrong, and running it down found a real defect — in the fixture, not
the kernel.

EXP181 compaction writes the packed `(token, expert)` slot at
`it_compact + inclusive_prefix`. If one token names the same expert twice, two lanes of
the same 8-wide group compute the same staging address and race. The fixture filled `ids`
with a uniform random draw over all experts, so duplicate experts within a token were
common. A router never emits that: the top-k experts of a token are distinct.

With the fixture generating a distinct expert set per token, both arms report **zero**
hazards:

| arm | before fixture fix | after |
|---|---:|---:|
| native | 18 | 0 |
| Goose | 5 | 0 |

So the hazards were an artifact of unrealistic input, the kernel was never racy, and the
"native parity" exemption has been deleted from the script — no module is exempt now, and
any non-zero sanitizer result fails the milestone.

The general lesson is recorded because it nearly became a permanent excuse: a comparison
that shows the native code is *worse* does not prove the difference is inherent. It can
equally mean both implementations are being fed input the operator does not accept.

## Scope and limits

This covers the ten modules with a standalone GPU fixture. It does not cover the CUDA
DSO as a whole, because a whole-library run needs the full model and the sanitizer's
memory overhead does not fit the 11.8 GB card next to a 35B model. The per-module
fixtures exercise the same kernels on the same compile path with representative shapes,
so the coverage is real but narrower than a whole-backend run.

Every module also passes its own differential gate; this milestone does not replace those
gates, it adds the memory and concurrency dimension they lack.
