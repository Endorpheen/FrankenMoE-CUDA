# Handoff — batch/output block, EXP237–244

Written at the end of a long session. The next session starts from this file, not from conversation memory.

## Where the work stands

Eight Goose routes cover the batch and output path. Five are live at 146–292 calls per request; three are
dormant, each with its reason taken from the guard in the source plus a zero witness — never inferred.

| route | prefix | calls | state |
|---|---|---:|---|
| split_equal expansion | `fg237s_` | 146 | live |
| split_seq selection | `fg238q_` | 0 | dormant: `embd_all` path not taken |
| batch entry classification | `fg239b_` | 146 | live |
| sequence-set construction (256-bit) | `fg240q_` | 146 | live |
| ubatch_add classification | `fg241u_` | 146 | live |
| out_ids + seq_idx | `fg242o_` | 292 | live |
| compat positions | `fg243c_` | 0 | dormant: legacy `llama_batch` entry point |
| output reorder | `fg244s_` | 0 | dormant: requires `n_outputs > 1` |
| SET view | `fg245v_` | — | gated candidate, deliberately NOT in the DSO |
| PAD index map | `fg246p_` | — | gated candidate, deliberately NOT in the DSO |

All gates re-verified against the shipped DSO: 89920 / 61059 / 67640 / 300008 / 160048 / 1680413 / 261197 /
325375 checks, zero failures.

## The single most important finding: division and modulo differ from C

Goose's `%` is Euclidean, C's keeps the sign of the dividend — `-5 % 10` is `5` in Goose and `-5` in C, and `/`
diverges the same way. EXP246 caught this the honest way: a direct transcription of `pad.cu`'s
`wrap_around(coord, size) = (coord + size) % size` produced **30 wrong indices in 10234 checks**, with the module
returning `+5` where the reference returned `-5`.

`docs/goose-migration/MODULO_SEMANTICS.md` holds the sweep of the eight shipped routes: seven have no `/` or `%`
at all, and the single site (EXP240 `sid/(64 as i64)`) is **verified safe** because its guard sits one line above
the division. That file also states the standing rule for new modules.

Read that file before porting anything that indexes or wraps.

## Verified performance

`exp238` vs `exp242`, four-step pair (old → new → new → old), only `libllama.so.0.7.0` swapped:

| | prefill tok/s | decode tok/s |
|---|---:|---:|
| old mean | 2493.52 | 24.547 |
| new mean | 2497.46 | 24.664 |
| delta | **+0.16%** | **+0.48%** |

**Prefill verdict is sound; decode is not.** Within-series decode spread reached 6.87% — three times the 2%
threshold. Four runs bound a decode regression at roughly ±4%, not ±2%. Do not quote a decode verdict from a
single pair. Prefill spread stayed at 0.07–0.43% and is the only trustworthy signal available here.

## How to run a measurement (learned the hard way)

- one `FG_STAGE` per campaign and a distinct `FG_SUFFIX` per run; the runner must derive **both** the results
  and measured roots from them — a hardcoded path made four runs overwrite one `summary.json`, and a fifth run
  returning figures identical to two decimals is what exposed it;
- switch DSOs by **symlink**, never by copying a `run` directory (648M, and a partial copy from a full disk is
  invisible until the runner fails minutes later with `No such file: libggml.so`);
- check `du -sh` against 648M before trusting a stage directory;
- verify the port is free rather than trusting `setsid` — without `--wait` it returns immediately;
- write link outputs **outside** the directory about to be rebuilt (I deleted one DSO that way).

## Repeated mistake classes, with the check that catches each

1. **Pristine-source rebuild drops the previous route.** Caught by `nm -u <object> | grep fg` before linking.
2. **Derived runner keeps the old path.** Caught by grepping the generated script for the new stage name.
3. **Fixture generator does not span the trap, so a real mutation passes.** Widening two generators raised the
   same mutations from 2 and 0 failures to 5344 and 30855. When a mutation passes, first suspect the generator,
   not the module.
4. **Guessing instead of instrumenting.** Three theories about a linked-but-silent route were wrong; one
   unconditional counter (`block=146 guard=146 flat=146`) settled it in a single run.

## Dequantisation block, EXP248–262

Every path in `ggml-cuda/dequantize.cuh` now has a Goose implementation, gated
against the native template compiled from the read-only source into the same
fixture:

| group | experiments | destinations |
|---|---|---|
| K-quants | EXP182 Q4_K, EXP248 Q6_K, EXP249 Q5_K, EXP250 Q2_K, EXP251 Q3_K | F32 |
| IQ lattice-free | EXP252 IQ4_NL, EXP253 IQ4_XS | F32 |
| IQ grid tables | EXP255 IQ2_XXS, EXP256 IQ2_XS, EXP257 IQ2_S, EXP258 IQ3_XXS, EXP259 IQ3_S, EXP260 IQ1_S, EXP261 IQ1_M | F32 |
| MX | EXP254 MXFP4 | F32 |
| legacy 32-element | EXP262 q1_0, q2_0, q4_0, q4_1, q5_0, q5_1, q8_0 | F32 pair |
| q4_0 8-block | EXP188 | F16 |

All are **OFF_PATH** and deliberately not linked: the fixed 32K/128 profile never
calls `ggml_get_to_fp32_cuda` for these types, because Q4_K_M executes through
MMQ. Each module carries a differential gate (native oracle in the fixture,
bitwise comparison), a host-plan gate where a row wrapper exists, and five
mutations. The shared grid/sign tables (`iq2xxs_grid`, `iq2xs_grid`, `iq2s_grid`,
`iq3xxs_grid`, `iq3s_grid`, `iq1s_grid_gpu`, `ksigns_iq2xs`) are imported as
explicit device constant primitives — data, not code; every index and arithmetic
operation is Goose.

Defect classes found in this block, all by the gate rather than by reading the
formula: block-layout conflation (Q2_K dm at the end of the block, IQ1_S stride
50 not 66), guard-index vs block-index conflation (IQ4_NL), fixture
under-allocation (MXFP4), an unaligned word read (q5_0), and a fixture whose
failure counter was invisible to `cudaMemcpyFromSymbol`, which made every
mutation report zero failures while the run looked green. The last one is the
most dangerous and is worth checking first in any new fixture.

## What is next, in order

1. `ROLL` — closed by EXP247. `SET` (EXP245), `PAD` (EXP246) and ROLL stay
   implemented and gated but not shipped: a route earns a place in the shipped
   DSO by being reachable, not by being finished.
2. Other `n_expert_used` widths for `mm_ids_helper`; only the `int)8`
   specialisation is ported and the others are dormant for this profile.
3. Dequantisation — closed by EXP248–262 for every `dequantize.cuh` path. The
   non-F32 destinations (half/bfloat16) of the same functions remain.
4. MMQ / MUL_MAT — the largest remaining native share (`mul_mat_q` Q4_K 30.3% +
   Q6_K 9.7% ≈ 40% of GPU time). Blocked on EXP180's codegen gap: 824 SASS
   instructions against native's 160, including 206 `IMAD` against ~20. The next
   idea recorded there is to precompute all address arithmetic host-side so the
   kernel only does `base + i0`. Six attempts were made; the blocker rule was
   reached.
5. Downstream phases P4–P11.

## Disk

The host runs near full (`~885G/938G`). Five stale `runtime/exp240/run*` copies were removed to recover space.
Do **not** recreate stage copies; symlink switching needs one directory per stage, not two.
