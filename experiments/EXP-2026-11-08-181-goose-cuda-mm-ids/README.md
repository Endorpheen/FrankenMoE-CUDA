# EXP181: actual Apex CUDA MM IDs helper in Goose

Hypothesis: replace the MM IDs routing helper, which converts the router's per-token
expert ids into the compact per-expert layout that `mul_mat_id` consumes, with actual
Goose for the width the Apex profile uses. No production or compiler-source changes.
Current state: qualified on the fixed Apex profile and preserved as a checkpoint.

## Verdict

**ACCEPTED** — byte-exact mm_ids helper; 20 checks, 15/15 byte-identical; isolated two-way median −0.46% prefill / +1.79% decode.

This is a verdict on the transfer in the experimental runtime, which is the only thing
this work decides. Installation into the production working runtime is left to Igor and
Codex, which is why the state line below says "not promoted" for several stages: it means
"not installed", never "not accepted".

## Scope transferred

`ggml_cuda_launch_mm_ids_helper` for `n_expert_used == 8`, which is the
specialisation the census shows (`mm_ids_helper<(int)8>`). Goose owns the
warp/token slot decode, the guarded id read, the used-expert compare, the padded slot
count, the 8-wide `any` reduction, the `shfl_up` scan over token positions, the
running compact counter, the shared-memory round trip through the packed
`(it, iex_used)` word, both map writes (forward and inverse) and the expert-bound
publication.

| check | result |
|---|---|
| `gpu-fixture` Goose | 20 checks / 0 failures / 5 cases |
| `gpu-fixture` native rollback | 20 / 0 / 5 |
| byte-identical outputs (dst, src1, bounds) | 15 of 15 files |
| compute-sanitizer memcheck | 0 errors |
| compute-sanitizer racecheck | 5 write-write hazards — **the same hazards are reported for the native kernel**, so this is the warp-synchronous staging pattern, not the transfer |
| export surface vs EXP180 | exactly the two diagnostics |

## The defect that took the longest

`red_any8` was written as

```
x = shfl_xor(x, 4, 8);
x = x | x;              // no-op: the original value was already overwritten
```

instead of native's `x = __shfl_xor_sync(...) || x`. The original is lost by the
assignment, so the reduce never propagated across the 8-lane group and the compact
counter advanced by one per pass instead of by the number of matching groups. The
symptom was subtle: `expert_bounds` matched native for experts 0..7 and differed by
one only at the closing entry, while `ids_dst` received 4 rows where native wrote 23.

A standalone `tests/probe.cu` was written to rule the shuffle layer in or out; it
showed the primitives themselves behave correctly, which is what pointed at the
composition rather than the intrinsics. A second helper defect was found the same
way: a bounds store was missing its `u64` cast, so the diagnostic probe wrote to the
wrong offset and reported the counter as unwritten.

Three further defects were fixed earlier: the shared window was addressed per block
instead of per kernel, the closing bound was written by block 0 instead of the last
block, and an out-of-bounds diagnostic write was removed before committing.

## Apex result

Two opposite-order pairs, only this helper toggled inside one DSO:

| Pair | Native pp/tg | Goose pp/tg | Δ prefill | Δ decode |
|---|---|---|---|---|
| forward | 2568.67 / 27.067 | 2603.83 / 27.496 | +1.368% | +1.585% |
| reverse | (same) | 2607.65 / 26.899 | +1.516% | -0.622% |
| mean | 2568.67 / 27.067 | 2605.74 / 27.197 | **+1.443%** | **+0.481%** |

All three runs return exactly the saved original C++ 128 token IDs, response text and
every returned top-5 probability (sha256 `e68df05b`). Both mean deltas are inside the
2% tolerance and in Goose's favour.

Actual execution is witnessed by `FG181M_CUDA_MM_IDS host=0x1, paths=0x1`, with
`FG180Q_CUDA_MMQ_Q8_1 host=0x23` and `FG179W_CUDA_MOE_WEIGHTED host=0x1` still
active, so six transferred operators now run together in one Apex pass.

## Re-measured in isolation, both orders

The forward/reverse pair above came from a runner that also set
`GGML_GOOSE_CUDA_SET_ROWS=0` in the native arm, so it compared two components. Re-measured
with that line removed, one pair in each order:

| pair | order | native | goose | prefill | decode |
|---|---|---|---:|---:|---:|
| iso1 | native→goose | 2610.6 / 27.20 | 2582.7 / 27.06 | −1.07% | −0.54% |
| iso2 | goose→native | 2590.5 / 26.68 | 2594.2 / 27.78 | +0.14% | +4.11% |
| **median** | | | | **−0.46%** | **+1.79%** |

Both figures are inside the 2% threshold, so the verdict holds, but the resolution here is
poor: prefill moves from −1.07% to +0.14% and decode from −0.54% to +4.11% between the two
orders. The warm-up band on this stage is wide enough that two pairs cannot separate the
effect from noise, and the +1.79% decode median should be read as "no clear difference"
rather than as a gain.

The earlier recorded +1.443% prefill / +0.481% decode came from a two-component comparison;
the prefill sign differs after isolation.

## Rollback and reproduce

`GGML_GOOSE_CUDA_MM_IDS=0` restores the private-renamed native launcher. The
immutable EXP180 runtime remains the previous checkpoint. Reproduce with
`python3 emit_sources.py`, `python3 build_runtime.py`, `python3 test_local.py`, then
`FG_STAGE=exp181 FG174_MEASURE_MODE=goose` and `native-rollback`.
`FG181M_DUMP=<dir>` writes the three output arrays per case.

## Remaining scope

Other `n_expert_used` widths and the generic template-0 variant stay native, as do
the MMQ and MUL_MAT families, dequantization, SET/PAD/ROLL and the remaining P3
subsystems.
