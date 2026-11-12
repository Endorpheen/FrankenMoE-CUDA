# EXP365: sanitizer milestone over the whole fixture matrix, four tools, and two fixture defects

EXP191 ran thirteen modules under three tools. The P3-19 criterion names four tools and a supported matrix, so
three things were open: `synccheck` was never run, the 28 fixtures added after EXP191 were never sanitized, and
the module list was hand-written, which meant a new fixture could be silently skipped.

This experiment closes all three and, in the process, finds two real defects -- both in fixtures, in the same
way that EXP191's five racecheck hazards turned out to be fixture defects rather than kernel defects.

## Method

`run_milestone.py` discovers every `runtime/exp*/build/gpu-fixture` instead of naming modules. Each fixture must
first pass on its own with zero failures; a sanitizer finding on a fixture that already fails is not a statement
about the port. Then the same binary runs under all four tools:

    memcheck   -- out-of-bounds and misaligned access
    racecheck  -- shared-memory hazards
    initcheck  -- reads of uninitialised memory
    synccheck  -- barrier divergence, the tool the criterion names and EXP191 never ran

Nothing is exempt. A missing summary line fails the milestone as loudly as a non-zero count.

## Result

    41 modules x 4 tools = 164 runs, all zero

| | memcheck | racecheck | initcheck | synccheck |
|---|---:|---:|---:|---:|
| modules with 0 errors | 41/41 | 41/41 | 41/41 | 41/41 |

Artifact: `runtime/sanitizer-milestone-full/milestone.json` plus 164 per-module per-tool logs.
Status line: `P3-19_SANITIZER_MILESTONE_PASS modules=41 tools=4`.

## Two defects found, both in fixtures

The first run failed on EXP281 (689 memcheck errors) and EXP282 (693). The reports point at
`soft_max_f32_ref<true,0,0,half>` -- the **native** reference kernel, not the Goose module -- reading past a
2400-byte allocation.

Root cause: native takes the mask strides from the mask tensor (`nb11 = src1->nb[1]`, `softmax.cu:413-415`), so a
`half` mask has half the strides of an `float` mask. Both fixtures set `nb11 = 4*ncols` once and reused it for the
F16 arm, so the *same* strides were handed to a mask allocated at `2*ncols` per row. The native kernel and the
Goose module then both read past the mask, which is why the error surfaced in the reference kernel and why both
arms agreed in a plain run -- a byte-identical comparison of two out-of-bounds reads.

Fix: set `nb11 = 2*ncols` (and the dependent strides) before the F16 launch, matching what the native launcher
does. Rebuilt as `gpu-fixture-fixed` for both modules:

| module | before | after |
|---|---|---|
| EXP281 | plain 0 failures, memcheck 689 | plain 0 failures, memcheck 0 |
| EXP282 | plain 0 failures, memcheck 693 | plain 0 failures, memcheck 0 |

Both corrected fixtures are kept under `fixtures/` with the patch in this repository, and the runner prefers a
`gpu-fixture-fixed` binary when one exists so the correction cannot be silently ignored.

Only two fixtures in the tree hand a mask to a half arm (`grep dmsh`), so the defect does not extend further.

## What this does and does not cover

Covered: every module that ships a standalone GPU fixture, under every applicable tool, with the fixture itself
verified clean first.

Not covered, and not claimed: the CUDA DSO as a whole, because a whole-library sanitizer run does not fit next to
a 35B model in 11.8 GB, and a 32-bit-shared-memory-limitations exemption, because no fixture here needs one.

## The second half of the criterion

P3-19 also asks that Apex CPU/GPU mixed paths execute Goose. In the accepted run log used by the coverage
registry (`runtime/reconstructed/build/run-ab-on1/server.log`) 94 distinct witnesses appear in the same process,
including CPU-path modules (`FG143_CPU op=40 calls=138 values=67377152`, `FG143_CPU op=100 calls=42560
values=187105280`) together with the CUDA family (`FG175FA_CUDA`, `FG185Q_CUDA_MMVQ_Q4K`, `FG174S_CUDA_GDN`,
`FG195V_CUDA_VMM_POOL`, `FG196P_CUDA_PINNED`). The mixed path is one process, not two runs stitched together.
