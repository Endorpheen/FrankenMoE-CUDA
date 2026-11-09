# EXP174: actual Apex CUDA SET_ROWS KV/state path

Hypothesis:replace F32->Q4_0/F32 scatter,quantization,index/broadcast arithmetic and host validation/launch planning with actual Goose,using EXP167-qualified fast-div and preserved native rollback. No production/compiler-source changes. Current state:qualified actual Apex SET_ROWS path;preserved separately,not activated. Whole Apex and universal P3-03 remain incomplete.

Two isolated DeepSeek drafts are retained. Parent corrects the has_cap/descriptor contract,uses explicit native enum query tables,guards row/index bounds,alignment,overflows,aliases and physical row/plane overlap,handles empty native no-op and reuses optimized division rather than slow scalar64division. Indexed row uniqueness and storage capacity remain caller contracts;device guarded refusal records invalid indices and prevents invalid stores without host GPU synchronization.

Producer is the independently qualified complete selfhosted EXP303Stage3. Control is native SET_ROWS in the same experimental CUDA DSO through one component switch;all other EXP173 modules stay identical. Local host/ABI/GPU/capture/rollback gates precede a significant fixed Apex32K/128 milestone;confirmed performance regressions require sameDSO opposite-order pairs before diagnosis. Whole Apex and source scope remain incomplete.

## Verdict

**ACCEPTED** — the whole active CUDA SET_ROWS operator is Goose and byte-exact; this is the baseline every later stage was layered on.

## Verified result

Parent-corrected host fixture:22 groups/46 runs/333 checks. Public CUDA operator:13 cases and380checks in each default/native-rollback mode,two actual graph replays per mode,byte-identical output,readonly sources and guards. Separate invalid-index fixture:four actual Goose routes,132 guarded threads/no destination writes. No undefined native invalid-index comparison. Exports add only two readonly diagnostics. Fresh producer output and mechanical compiled bodies verified.

One fixed Apex32768/128 pair toggles only SET_ROWS inside the same DSO. Native SET_ROWS:2665.309pp/28.002tg;Goose:2663.005pp/27.857tg(-0.086percent/-0.517percent),RSS21310.500/21309.668MiB,VRAM3246MiBboth,swap0. Both exactly match saved original C++128IDs/text/top5;127graph reuses,sevenzeroownedheaps. Actual Goose Q4/I64 host+device path8/status0/invalid0 observed. These are preliminary component observations,not stable whole-runtime C++ parity.

## Re-measured in both orders

That single pair was the only measurement of this stage, and it is the baseline every later
stage is compared against, so it was re-taken with one pair in each order:

| pair | order | native | goose | prefill | decode |
|---|---|---|---:|---:|---:|
| iso1 | native→goose | 2628.9 / 26.93 | 2627.6 / 27.79 | −0.05% | +3.20% |
| iso2 | goose→native | 2627.2 / 27.38 | 2629.2 / 27.47 | +0.07% | +0.33% |
| **median** | | | | **+0.01%** | **+1.77%** |

Prefill is flat to within 0.07% in both pairs, which is the solid statement: SET_ROWS costs
no measurable prefill time. Decode reads +3.20% and +0.33%, a spread that says the stage is
inside the warm-up noise band rather than that it is 1.8% faster — the same limitation
recorded for EXP176 and EXP181.

The original −0.086% / −0.517% was a single-order pair taken at a time when this machine
returned 2665/28.0; the numbers today are 2627-2629/26.9-27.8, so the absolute level has
drifted and only the paired delta is comparable across sessions.

## Reproduce

Run `python3 emit_sources.py`, `python3 emit_bridge.py`, `python3 verify_generated.py`, `python3 build_runtime.py`, `python3 test_local.py` from this directory. The build replays an immutable output or keeps historical binaries;only the dedicated EXP174 run links move. Existing Apex receipts cannot be overwritten. For a new approved significant integration use distinct result labels in `run_apex_pair.py`;native mode is `FG174_MEASURE_MODE=native-rollback`,default mode Goose. Never reuse a receipt directory or compare different library hashes as causal evidence. Native SET_ROWS env0 and EXP173 remain rollback. Original allocation capacity/index uniqueness and CUDA SDK/ISA effects remain explicit caller/external contracts.

EXP303 remains the latest qualified complete selfhosted producer. EXP304 scalar cast gate appeared during this checkpoint,but no completed Stage3 bootstrap receipt was available;it was not substituted or advertised as runtime-qualified.
